/*
 * PC host bootstrap (P2.8): init cdfs + memcard + overlays, load boot TIM,
 * then enter the decomp game loop (dw3_game_main).
 *
 * P3.7 Task 3–4: file-backed STR CD bridge, StCdIntrFlag pump, XA evidence
 * drain when the SDL audio device is missing/headless.
 */
#include <psyz.h>
#include <psyz/audio.h>
#include <psyz/spu.h>

#include "boot_tim_host.h"
#include "cdfs.h"
#include "memcard_host.h"
#include "overlay_host.h"
#include "runtime_extract.h"
#include "str_stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int dw3_game_main(void);

extern volatile int StCdIntrFlag;
void StCdInterrupt(void);

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

static void dw3_pc_vsync_pump(void) {
    if (StCdIntrFlag) {
        StCdInterrupt();
        StCdIntrFlag = 0;
    }
    Psyz_CdPumpStream();
    dw3_pc_xa_evidence_drain();
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
    char memcard_buf[512];
    char boot_path[512];
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--runtime") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if (strcmp(argv[i], "--memcard-dir") == 0 && i + 1 < argc) {
            memdir = argv[++i];
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

    if (memdir == NULL) {
        snprintf(memcard_buf, sizeof(memcard_buf), "%s/memcard", root);
        memdir = memcard_buf;
    }
    if (memcard_host_set_dir(memdir) != 0) {
        fprintf(stderr, "dw3-pc: memcard_host_set_dir(%s) failed\n", memdir);
        return -1;
    }
    if (memcard_host_ensure(0) != 0 || memcard_host_ensure(1) != 0) {
        fprintf(stderr, "dw3-pc: memcard ensure failed under %s\n", memdir);
        return -1;
    }
    /* Dirty flat saves flush here on clean process exit (P5.3). HOME /
     * abrupt kill still cannot run atexit — see docs/P5.3_FLAT_SAVE.md. */
    atexit(memcard_host_shutdown);
    fprintf(stderr, "dw3-pc: memcard dir=%s\n", memdir);

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

int main(int argc, char **argv) {
    if (init_paths(argc, argv) != 0) {
        return 1;
    }
    g_prev_vsync_cb = Psyz_SetVSyncCb(dw3_pc_vsync_pump);
    fprintf(stderr, "dw3-pc: entering dw3_game_main()\n");
    {
        int rc = dw3_game_main();
        memcard_host_shutdown();
        return rc;
    }
}
