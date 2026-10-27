/*
 * PC host bootstrap (P2.8): init cdfs + memcard + overlays, load boot TIM,
 * then enter the decomp game loop (dw3_game_main).
 *
 * P3.7 Task 3–4: file-backed STR CD bridge, StCdIntrFlag pump, XA evidence
 * drain when the SDL audio device is missing/headless.
 */
#include <SDL3/SDL.h>

#include <psyz.h>
#include <psyz/audio.h>
#include <psyz/spu.h>

#include "boot_tim_host.h"
#include "cdfs.h"
#include "memcard_host.h"
#include "save_convert.h"
#include "overlay_host.h"
#include "port_options.h"
#include "port_turbo.h"
#include "runtime_extract.h"
#include "str_stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

int dw3_game_main(void);
#if defined(DW3_PC_SDL3_GPU)
void dw3_pc_title_menu_overlay_register(void);
#endif

void dw3_pc_mem_sampler_tick(void); /* P5.1, mem_sampler.c */

extern volatile int StCdIntrFlag;
void StCdInterrupt(void);
extern char **environ;

static int dw3_pc_capture_window(const char *display, const char *path) {
    pid_t child;
    int rc;
    int status;
    char *argv[] = {"import", "-display", (char *)display, "-window", "root",
                    (char *)path, NULL};

    rc = posix_spawnp(&child, "import", NULL, NULL, argv, environ);
    if (rc != 0) {
        fprintf(stderr, "dw3-replay: could not start import: %s\n", strerror(rc));
        return -1;
    }
    do {
        rc = waitpid(child, &status, 0);
    } while (rc < 0 && errno == EINTR);
    if (rc < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "dw3-replay: import failed for %s\n", path);
        return -1;
    }
    return 0;
}

static int dw3_pc_safe_replay_name(const char *name) {
    const unsigned char *p = (const unsigned char *)name;

    if (p == NULL || *p == '\0') {
        return 0;
    }
    for (; *p != '\0'; ++p) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '_' || *p == '-')) {
            return 0;
        }
    }
    return 1;
}

/* P6.15: checkpoint capture for DW3_REPLAY runs (hook lives in
 * port/psyz_missing/psyz_pad_host.c). Writes the presented display frame as
 * a binary PPM to $DW3_REPLAY_OUT/<name>.ppm. */
extern void (*dw3_pad_host_shot_cb)(const char *name, unsigned frame);

static void dw3_pc_replay_shot(const char *name, unsigned frame) {
    const char *dir = getenv("DW3_REPLAY_OUT");
    char path[512];
    unsigned char *rgb;
    int w = 0, h = 0;
    FILE *fp;

    if (!dw3_pc_safe_replay_name(name)) {
        fprintf(stderr, "dw3-replay: rejected unsafe checkpoint name\n");
        return;
    }
    if (dir == NULL || dir[0] == '\0') {
        dir = ".";
    }
    snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    rgb = Psyz_VideoAllocCapturedFrame(&w, &h);
    if (rgb == NULL || w <= 0 || h <= 0) {
        fprintf(stderr, "dw3-replay: shot %s failed (no frame)\n", name);
        return;
    }
    fp = fopen(path, "wb");
    if (fp != NULL) {
        fprintf(fp, "P6\n%d %d\n255\n", w, h);
        fwrite(rgb, 1, (size_t)w * (size_t)h * 3u, fp);
        fclose(fp);
    }
    fprintf(stderr, "dw3-replay: shot %s frame=%u %dx%d -> %s\n", name, frame,
            w, h, path);
    free(rgb);
    /* The PSY-Z frame above is the PS1 display only. The title logo and menu
     * are host overlays drawn at present time, so DW3_REPLAY_XSHOT=1 also
     * grabs the composited window from the (Xvfb) X server. The game thread
     * is blocked here, so the last presented frame stays on screen. */
    if (getenv("DW3_REPLAY_XSHOT") != NULL && getenv("DISPLAY") != NULL) {
        char window_path[512];

        SDL_Delay(250);
        snprintf(window_path, sizeof(window_path), "%s/%s.png", dir, name);
        if (dw3_pc_capture_window(getenv("DISPLAY"), window_path) != 0) {
            fprintf(stderr, "dw3-replay: window grab failed (%s)\n", window_path);
        }
    }
}

static PsyzVSyncCb g_prev_vsync_cb;
static int g_audio_ok;
static int g_xa_mix_logged;
static unsigned g_xa_drain_frames;

static void dw3_pc_xa_evidence_drain(void) {
    short buf[1024 * 2];
    int i;
    long peak = 0;
    long abs_sum = 0;

    /* SDL callback pulls when AudioInit succeeded; otherwise drain here so
     * file-bridge XA → SPU mix can still be proven from logs. */
    if (g_audio_ok) {
        return;
    }
    Psyz_SpuPullSamples(buf, 1024);
    g_xa_drain_frames += 1024;
    if (g_xa_mix_logged) {
        return;
    }
    for (i = 0; i < 1024 * 2; i++) {
        long v = buf[i];
        if (v < 0) {
            v = -v;
        }
        abs_sum += v;
        if (v > peak) {
            peak = v;
        }
    }
    if (peak > 64 || g_xa_drain_frames > 44100u * 3u) {
        fprintf(stderr,
                "dw3-xa: SPU mix drain frames=%u peak=%ld mean_abs=%ld "
                "(no SDL playback device; owner listen still required)\n",
                g_xa_drain_frames, peak, abs_sum / (1024 * 2));
        g_xa_mix_logged = 1;
    }
}

/* P6.4 fast-forward. PSY-Z paces one frame per vsync with driver VSync or
 * its own limiter; while fast-forward runs, switch that off and pace here at
 * `speed` times the base rate (50 or 59.94 Hz, from the PSY-Z stats). */
static void dw3_pc_turbo_pace(void) {
    static int engaged;
    static PsyzVsyncMode saved_mode;
    static Uint64 next_ns;
    PsyzVideoStats stats;
    double base_us = 1000000.0 / 59.94;
    Uint64 period_ns;
    Uint64 now_ns;
    int speed = dw3_port_turbo_speed();
    int rate_x2 = dw3_port_turbo_rate_x2();

    if (speed <= 1) {
        if (engaged) {
            engaged = 0;
            Psyz_VideoSetVsyncMode(saved_mode);
        }
        return;
    }
    now_ns = SDL_GetTicksNS();
    if (!engaged) {
        engaged = 1;
        saved_mode = Psyz_VideoGetVsyncMode();
        Psyz_VideoSetVsyncMode(PSYZ_VSYNC_LIMITLESS);
        next_ns = now_ns;
    }
    if (Psyz_VideoStats(&stats) == 0 && stats.target_frame_time_us > 1000.0) {
        base_us = stats.target_frame_time_us;
    }
    period_ns = (Uint64)(base_us * 2000.0 / (double)rate_x2);
    next_ns += period_ns;
    if (next_ns > now_ns) {
        SDL_DelayPrecise(next_ns - now_ns);
    } else if (now_ns - next_ns > 4 * period_ns) {
        /* Slower than the target (loading, a slow GPU): do not sprint to
         * catch up afterwards. */
        next_ns = now_ns;
    }
}

static void dw3_pc_vsync_pump(void) {
    if (StCdIntrFlag) {
        StCdInterrupt();
        StCdIntrFlag = 0;
    }
    Psyz_CdPumpStream();
    dw3_pc_xa_evidence_drain();
    dw3_pc_turbo_pace();
    dw3_pc_mem_sampler_tick();
    if (g_prev_vsync_cb) {
        g_prev_vsync_cb();
    }
}

static const char *env_or(const char *key, const char *fallback) {
    const char *v = getenv(key);
    return (v != NULL && v[0] != '\0') ? v : fallback;
}

#ifndef DW3_DEFAULT_RUNTIME
#define DW3_DEFAULT_RUNTIME "runtime/us"
#endif

static int init_paths(int argc, char **argv) {
    const char *root = env_or("DW3_RUNTIME", DW3_DEFAULT_RUNTIME);
    const char *memdir = env_or("DW3_MEMCARD_DIR", NULL);
    const char *cfgdir = env_or("DW3_CONFIG_DIR", NULL);
    char memcard_buf[512];
    int memdir_explicit;
    char boot_path[512];
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--runtime") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if (strcmp(argv[i], "--memcard-dir") == 0 && i + 1 < argc) {
            memdir = argv[++i];
        } else if (strcmp(argv[i], "--config-dir") == 0 && i + 1 < argc) {
            cfgdir = argv[++i];
        }
    }

#if defined(VERSION_US) && VERSION_US
    if (dw3_runtime_ensure(root) != 0) {
        fprintf(stderr, "dw3-pc: runtime extract failed\n");
        return -1;
    }
#endif
    if (cdfs_set_root(root) != 0) {
        fprintf(stderr, "dw3-pc: cdfs_set_root(%s) failed\n", root);
        return -1;
    }
    if (cdfs_load_map(NULL) != 0) {
        fprintf(stderr, "dw3-pc: cdfs_load_map under %s failed\n", root);
        return -1;
    }
    fprintf(stderr, "dw3-pc: cdfs root=%s\n", root);

    if (dw3_str_restore_from_disc() != 0) {
        fprintf(stderr, "dw3-pc: STR restore failed; movie XA may stay slow\n");
    }

    dw3_str_stream_install_cd_bridge();

    /* Eager audio init so CdlModeRT XA has a device (or we log failure and
     * fall back to the vsync SPU drain for decode/mix evidence). */
    if (Psyz_AudioInit() == 0) {
        g_audio_ok = 1;
        fprintf(stderr, "dw3-pc: Psyz_AudioInit OK\n");
    } else {
        g_audio_ok = 0;
        fprintf(stderr,
                "dw3-pc: Psyz_AudioInit failed — XA decode/mix will be "
                "drained on vsync for logs; audible listen needs a device\n");
    }

    memdir_explicit = (memdir != NULL);
    if (memdir == NULL) {
        snprintf(memcard_buf, sizeof(memcard_buf), "%s/memcard", root);
        memdir = memcard_buf;
    }
    if (memcard_host_set_dir(memdir) != 0) {
        fprintf(stderr, "dw3-pc: memcard_host_set_dir(%s) failed\n", memdir);
        return -1;
    }
    /* P6.25: a port with no save of its own takes the other region's (converted
     * copy; the original is never touched). */
    (void)memcard_host_setup_cross_import(root,
#if defined(VERSION_EU) && VERSION_EU
                                          SAVE_REGION_EU,
#else
                                          SAVE_REGION_US,
#endif
                                          memdir_explicit);
    if (memcard_host_ensure(0) != 0 || memcard_host_ensure(1) != 0) {
        fprintf(stderr, "dw3-pc: memcard ensure failed under %s\n", memdir);
        return -1;
    }
    /* Dirty flat saves flush here on clean process exit (P5.3). HOME /
     * abrupt kill still cannot run atexit — see docs/P5.3_FLAT_SAVE.md. */
    atexit(memcard_host_shutdown);
    fprintf(stderr, "dw3-pc: memcard dir=%s\n", memdir);

    /* P9.4: port_options.cfg lives in the runtime root unless redirected, and
     * is loaded before anything reads widescreen / fast-forward / restart. */
    (void)dw3_port_options_persist_init(cfgdir != NULL ? cfgdir : root);
    atexit(dw3_port_options_flush);

    snprintf(boot_path, sizeof(boot_path), "%s/boot_tim.bin", root);
    if (dw3_host_load_boot_tim(boot_path) != 0) {
        fprintf(stderr, "dw3-pc: boot TIM load failed (%s)\n", boot_path);
        return -1;
    }
    fprintf(stderr, "dw3-pc: boot TIM loaded from %s\n", boot_path);

#if defined(VERSION_EU) && VERSION_EU
    dw3_overlay_register_eu_title();
#else
    dw3_overlay_register_us_title();
#endif
    return 0;
}

/* P6.28: the title menu's Quit game closes the window the way its close button
 * does: the same SDL quit event, so PSY-Z sets Psyz_QuitRequested, the game
 * loop returns from dw3_game_main() and the saves are flushed once more on
 * the way out (port/options/confirm.c flushed them already). */
void dw3_host_request_quit(void) {
    SDL_Event event;

    SDL_zero(event);
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
}

int main(int argc, char **argv) {
    const char *internal_res = getenv("DW3_PC_INTERNAL_RES");

    if (init_paths(argc, argv) != 0) {
        return 1;
    }
    if (internal_res != NULL && internal_res[0] != '\0') {
        char *end = NULL;
        unsigned long multiplier = strtoul(internal_res, &end, 10);
        if (end == internal_res || *end != '\0' || multiplier != 3 ||
            Psyz_VideoSetInternalResolution((unsigned)multiplier) != 0) {
            fprintf(stderr,
                    "dw3-pc: DW3_PC_INTERNAL_RES must be 3 (experimental 720p)\n");
            return 1;
        }
        fprintf(stderr,
                "dw3-pc: experimental internal resolution %lux "
                "(960x720 for 320x240 content)\n",
                multiplier);
    }
    dw3_pad_host_shot_cb = dw3_pc_replay_shot;
    g_prev_vsync_cb = Psyz_SetVSyncCb(dw3_pc_vsync_pump);
    if (getenv("DW3_PC_FRAME_LIMIT") != NULL) {
        /* P5.1 soak: under Xvfb the driver VSync does not block, so a run
         * would go at 2x or more; the PSY-Z frame limiter paces it at 1x. */
        (void)Psyz_VideoSetVsyncMode(PSYZ_VSYNC_OFF);
    }
#if defined(DW3_PC_SDL3_GPU)
    dw3_pc_title_menu_overlay_register();
#endif
    fprintf(stderr, "dw3-pc: entering dw3_game_main()\n");
    {
        int rc = dw3_game_main();
        memcard_host_shutdown();
        return rc;
    }
}
