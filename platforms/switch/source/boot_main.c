/* Switch host bootstrap (P4.7): bring the host services up and enter the decomp
 * game loop, dw3_game_main().
 *
 * This is the analogue of platforms/pc/src/main.c and keeps the same order —
 * cdfs root and file-id map, the file-backed STR CD bridge, audio, memcard,
 * boot TIM, overlay table, then the vsync callback — with the Switch-specific
 * pieces in front: the pad source, the SD mount and the EGL surface.
 *
 * source/main.c is still the gpu_hello/audio_hello bring-up sample, and is what
 * the devkitPro Makefile builds. The CMake build — the one that links the
 * game — compiles this file instead.
 *
 * Status: this is the boot path, not a working boot. Nothing renders the game
 * yet (PSY-Z's Draw_* has no Switch implementation) — see docs/SWITCH_BACKEND.md
 * and docs/ROADMAP.md P4.7.
 */
#ifdef DW3_NXLINK_STDIO
#include <unistd.h>
#endif

#include <psyz.h>
#include <psyz/log.h>

#include "boot_tim_host.h"
#include "cdfs.h"
#include "memcard_host.h"
#include "overlay_host.h"
#include "runtime_extract.h"
#include "str_stream.h"

#include "../psyz_backend/nx.h"
#include "frame_dt.h"

#include <stdio.h>
#include <string.h>

#include <switch.h>

#ifdef DW3_NXLINK_STDIO
/* PSY-Z maps close() to psyz_close(), which has no Switch implementation. */
#undef close
#endif

int dw3_game_main(void);

extern volatile int StCdIntrFlag;
void StCdInterrupt(void);

/* The SD layout from docs/RUNTIME_LAYOUT.md: sdmc:/switch/dw3/<ver>/. */
#ifndef DW3_DEFAULT_RUNTIME
#define DW3_DEFAULT_RUNTIME "sdmc:/switch/dw3/us"
#endif

static PsyzVSyncCb g_prev_vsync_cb;

static uint64_t restore_now_us(void) {
    return armTicksToNs(armGetSystemTick()) / 1000ULL;
}

#ifdef DW3_NXLINK_STDIO
static int g_nxlink_socket_initialized;
static Result g_nxlink_socket_result;
static int g_nxlink_fd = -1;

static void dw3_nxlink_stdio_shutdown(void) {
    if (g_nxlink_fd >= 0) {
        close(g_nxlink_fd);
        g_nxlink_fd = -1;
    }
    if (g_nxlink_socket_initialized) {
        socketExit();
        g_nxlink_socket_initialized = 0;
    }
}
#endif

/* Runs once per frame from PSY-Z's vsync hook, to service the CD/STR bridge
 * (StCdIntrFlag + Psyz_CdPumpStream), exactly as the PC host does, so streaming
 * movies keep their sectors coming.
 *
 * Deliberately does NOT call appletMainLoop(): it wraps appletGetMessage, which
 * *blocks* until the applet sends something, and this runs inside the game's
 * frame — the first call hung the boot before its first frame. libnx's own docs
 * scope it to "within a main loop", which dw3_game_main() never returns to.
 * Quitting is the game's own business: its loop polls Psyz_QuitRequested.
 */
static void dw3_nx_vsync_pump(void) {
    static unsigned pumps;

    /* The pump runs once per VSync(), from inside it, *after* the frame was
     * presented — so the count is a boot progress meter for free. The game's
     * own boot path makes five calls before its main loop (four VSync(0) and a
     * VSync(3) in src/engine/system/main.c), then one per frame; a count that
     * stops at 3 says the game never reached the loop's first frame, and which
     * call it stopped on says what it was doing. Logged one by one up to 16 and
     * then at powers of two, so a healthy boot stays logarithmic. */
    pumps++;
    if (pumps <= 16 || (pumps & (pumps - 1)) == 0) {
        INFOF("vsync pump #%u", pumps);
    }

    /* One line per 60 pumps: microseconds since the previous 60-pump sample.
     * The first sample only stores the baseline, so the first line is the
     * second sample. */
    {
        static Dw3FrameDt frame_dt;
        uint64_t dt_us;

        if (dw3_frame_dt_sample(&frame_dt, pumps, restore_now_us(), &dt_us)) {
            INFOF("frame dt us=%llu", (unsigned long long)dt_us);
        }
    }

    if (StCdIntrFlag) {
        StCdInterrupt();
        StCdIntrFlag = 0;
    }
    Psyz_CdPumpStream();
    if (g_prev_vsync_cb) {
        g_prev_vsync_cb();
    }
}

/* eglSwapBuffers waits for vsync. Byte ticks arrive every 256 KiB; painting
 * each one would cap the copy near one chunk per refresh. Present on the
 * log lines (a file starting, the 8 MiB marks, a file finishing) and, between
 * those, about every 300 ms. The "restoring N STR" line holds a sweep for
 * a few refreshes before fread blocks. */
#define STR_RESTORE_PAINT_US 300000ull
#define STR_RESTORE_INTRO_US 400000ull

static int sweep_origin(int origin, int span, int sweep_w, uint64_t now_us,
                        uint64_t period_us) {
    int travel;
    uint64_t t;

    if (sweep_w > span) {
        sweep_w = span;
    }
    travel = span - sweep_w;
    if (travel < 0) {
        travel = 0;
    }
    if (period_us == 0) {
        return origin;
    }
    t = now_us % period_us;
    return origin + (int)(((uint64_t)travel * t) / period_us);
}

/* Dark blue field plus a bar. done == 0 draws a sweep on the whole track
 * so the first frame is not a flat clear. After that, the filled width is
 * done/total and a slower sweep crosses whatever is left. */
static void paint_str_restore(unsigned long long done, unsigned long long total) {
    Dw3NxGpuInfo info;
    int margin;
    int bar_w;
    int bar_h;
    int bar_x;
    int bar_y;
    uint64_t now_us;

    dw3_nx_gpu_get_info(&info);
    dw3_nx_gpu_clear(0.02f, 0.06f, 0.12f, 1.0f);
    if (info.width < 32 || info.height < 32) {
        return;
    }

    margin = (int)info.width / 10;
    bar_w = (int)info.width - margin * 2;
    bar_h = (int)info.height / 16;
    if (bar_h < 24) {
        bar_h = 24;
    }
    if (bar_h > 72) {
        bar_h = 72;
    }
    if (bar_w < 8) {
        return;
    }
    bar_x = margin;
    bar_y = ((int)info.height - bar_h) / 2;
    now_us = restore_now_us();

    dw3_nx_gpu_fill_rect(bar_x, bar_y, bar_w, bar_h, 0.10f, 0.18f, 0.28f, 1.0f);

    if (total == 0 || done == 0) {
        int sweep_w = bar_w / 5;
        if (sweep_w < 16) {
            sweep_w = 16;
        }
        if (sweep_w > bar_w) {
            sweep_w = bar_w;
        }
        dw3_nx_gpu_fill_rect(sweep_origin(bar_x, bar_w, sweep_w, now_us, 1200000ull),
                             bar_y, sweep_w, bar_h, 0.45f, 0.82f, 0.95f, 1.0f);
        return;
    }

    {
        double frac = (double)done / (double)total;
        int fill_w;
        int rest;
        int sweep_w;

        if (frac > 1.0) {
            frac = 1.0;
        }
        fill_w = (int)(frac * (double)bar_w + 0.5);
        if (fill_w > bar_w) {
            fill_w = bar_w;
        }
        if (fill_w > 0) {
            dw3_nx_gpu_fill_rect(bar_x, bar_y, fill_w, bar_h, 0.28f, 0.62f, 0.95f,
                                 1.0f);
        }
        rest = bar_w - fill_w;
        if (rest <= 0) {
            return;
        }
        sweep_w = rest / 3;
        if (sweep_w < 8) {
            sweep_w = 8;
        }
        if (sweep_w > rest) {
            sweep_w = rest;
        }
        dw3_nx_gpu_fill_rect(
            sweep_origin(bar_x + fill_w, rest, sweep_w, now_us, 1600000ull), bar_y,
            sweep_w, bar_h, 0.16f, 0.32f, 0.48f, 1.0f);
    }
}

static void str_restore_progress(const char *msg, unsigned long long done,
                                 unsigned long long total) {
    static uint64_t last_paint_us;
    static unsigned long long last_done;
    static int painted;
    uint64_t now;
    int first_bytes;

    if (msg != NULL) {
        INFOF("%s", msg);
    }
    now = restore_now_us();
    first_bytes = painted && last_done == 0 && done > 0;
    if (painted && msg == NULL && !first_bytes &&
        (now - last_paint_us) < STR_RESTORE_PAINT_US) {
        return;
    }
    /* The first log line is "restoring N STR ..." or "extracting runtime ...".
     * Hold the sweep there. Other notes paint a single frame. */
    if (!painted && done == 0 && msg != NULL &&
        (strncmp(msg, "restoring ", sizeof("restoring ") - 1) == 0 ||
         strncmp(msg, "extracting ", sizeof("extracting ") - 1) == 0)) {
        uint64_t intro_start = now;
        int frames = 0;
        /* Cap the refreshes so a swap that does not block, or a clock that
         * does not advance, cannot sit in this loop. */
        do {
            paint_str_restore(done, total);
            if (!dw3_nx_gpu_swap()) {
                break;
            }
            frames++;
        } while (frames < 40 &&
                 (restore_now_us() - intro_start) < STR_RESTORE_INTRO_US);
    } else {
        paint_str_restore(done, total);
        dw3_nx_gpu_swap();
    }
    last_paint_us = restore_now_us();
    last_done = done;
    painted = 1;
}

static const char *env_or(const char *key, const char *fallback) {
    const char *v = getenv(key);
    return (v != NULL && v[0] != '\0') ? v : fallback;
}

static int init_paths(void) {
    const char *root = env_or("DW3_RUNTIME", DW3_DEFAULT_RUNTIME);
    const char *memdir = env_or("DW3_MEMCARD_DIR", NULL);
    char memcard_buf[512];
    char boot_path[512];

    /* US: a card with the BIN and no us/ tree is filled in here. A tree that
     * is already complete only stats. The bar is the same callback the STR
     * rewrite uses, so register it first. EU does not extract. */
    dw3_str_restore_set_progress(str_restore_progress);
#if defined(VERSION_US) && VERSION_US
    if (dw3_runtime_ensure(root) != 0) {
        ERRORF("runtime extract failed");
        return -1;
    }
#endif
    if (cdfs_set_root(root) != 0) {
        ERRORF("cdfs_set_root(%s) failed", root);
        return -1;
    }
    if (cdfs_load_map(NULL) != 0) {
        ERRORF("cdfs_load_map under %s failed", root);
        return -1;
    }
    INFOF("cdfs root=%s", root);

    /* Packed 2048-byte STR files still need a raw rewrite. An extract that
     * just wrote sectors*2352 only stats here. */
    if (dw3_str_restore_from_disc() != 0) {
        ERRORF("STR restore failed; opening-movie audio may stay slow");
    }

    dw3_str_stream_install_cd_bridge();

    /* The PC host tolerates a missing audio device and drains the SPU on vsync
     * for evidence. Here audren either comes up or the run is not worth
     * continuing: without it there is no mixer to pull and the game blocks. */
    if (Psyz_AudioInit() != 0) {
        ERRORF("Psyz_AudioInit failed");
        return -1;
    }
    INFOF("Psyz_AudioInit OK");

    if (memdir == NULL) {
        snprintf(memcard_buf, sizeof(memcard_buf), "%s/memcard", root);
        memdir = memcard_buf;
    }
    if (memcard_host_set_dir(memdir) != 0) {
        ERRORF("memcard_host_set_dir(%s) failed", memdir);
        return -1;
    }
    if (memcard_host_ensure(0) != 0 || memcard_host_ensure(1) != 0) {
        ERRORF("memcard ensure failed under %s", memdir);
        return -1;
    }
    INFOF("memcard dir=%s", memdir);

    snprintf(boot_path, sizeof(boot_path), "%s/boot_tim.bin", root);
    if (dw3_host_load_boot_tim(boot_path) != 0) {
        ERRORF("boot TIM load failed (%s)", boot_path);
        return -1;
    }
    INFOF("boot TIM loaded from %s", boot_path);

#if defined(VERSION_EU) && VERSION_EU
    dw3_overlay_register_eu_title();
#else
    dw3_overlay_register_us_title();
#endif
    INFOF("overlay table registered");
    return 0;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

#ifdef DW3_NXLINK_STDIO
    /* libnx nxlinkConnectToHost uses the default socket service. Initialize it
     * before asking nxlink to open the callback connection to the host. */
    g_nxlink_socket_result = socketInitializeDefault();
    if (R_FAILED(g_nxlink_socket_result)) {
        fprintf(stderr, "nxlink socketInitializeDefault failed (0x%08x)\n",
                (unsigned)g_nxlink_socket_result);
    } else {
        g_nxlink_socket_initialized = 1;
        g_nxlink_fd = nxlinkStdio();
        if (g_nxlink_fd < 0) {
            fprintf(stderr, "nxlinkStdio failed (%d); continuing\n",
                    g_nxlink_fd);
        } else {
            fprintf(stderr, "nxlink stdio connected (fd=%d)\n", g_nxlink_fd);
        }
    }
#endif

    /* Order matters: the pad and the SD mount have to be up before anything
     * opens a runtime path, and the log file is opened as early as it can be so
     * a boot that dies does so with its reasons on disk — Ryubing has no nxlink
     * and guest stderr goes nowhere there. */
    dw3_nx_input_init();

    if (!dw3_nx_fs_mount_sd()) {
        /* Not fatal: the log falls back to stderr and cdfs will report what it
         * cannot find. */
        ERRORF("sdmc: mount failed; runtime paths will not resolve");
    }
    dw3_nx_log_open();
#ifdef DW3_NXLINK_STDIO
    INFOF("nxlink socket init=0x%08x stdio fd=%d",
          (unsigned)g_nxlink_socket_result, g_nxlink_fd);
#endif

    if (!dw3_nx_gpu_init()) {
        ERRORF("dw3_nx_gpu_init failed");
#ifdef DW3_NXLINK_STDIO
        dw3_nxlink_stdio_shutdown();
#endif
        return 1;
    }
    INFOF("EGL/GLES2 surface up");

    if (init_paths() != 0) {
#ifdef DW3_NXLINK_STDIO
        dw3_nxlink_stdio_shutdown();
#endif
        return 1;
    }

    g_prev_vsync_cb = Psyz_SetVSyncCb(dw3_nx_vsync_pump);
    {
        /* Self-check, because the callback marker never appeared: registering
         * again must hand back our own callback, which proves the global took.
         * If got == want and the callback still never runs, the fault is
         * elsewhere and the marker itself is lying. */
        PsyzVSyncCb got = Psyz_SetVSyncCb(dw3_nx_vsync_pump);
        INFOF("vsync cb: prev=%p got=%p want=%p",
              (void *)g_prev_vsync_cb, (void *)got,
              (void *)dw3_nx_vsync_pump);
    }
    INFOF("entering dw3_game_main()");
    {
        int game_result = dw3_game_main();
#ifdef DW3_NXLINK_STDIO
        dw3_nxlink_stdio_shutdown();
#endif
        return game_result;
    }
}
