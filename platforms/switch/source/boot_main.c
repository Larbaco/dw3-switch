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
#include <psyz.h>
#include <psyz/log.h>

#include "boot_tim_host.h"
#include "cdfs.h"
#include "memcard_host.h"
#include "overlay_host.h"
#include "str_stream.h"

#include "../psyz_backend/nx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>

int dw3_game_main(void);

extern volatile int StCdIntrFlag;
void StCdInterrupt(void);

/* The SD layout from docs/RUNTIME_LAYOUT.md: sdmc:/switch/dw3/<ver>/. */
#ifndef DW3_DEFAULT_RUNTIME
#define DW3_DEFAULT_RUNTIME "sdmc:/switch/dw3/us"
#endif

static PsyzVSyncCb g_prev_vsync_cb;

/* Runs once per frame from PSY-Z's vsync hook. Two jobs:
 *
 *  - service the CD/STR bridge (StCdIntrFlag + Psyz_CdPumpStream), exactly as
 *    the PC host does, so streaming movies keep their sectors coming;
 *  - keep the console's applet loop alive. dw3_game_main() never returns to
 *    main(), so this callback is the only place the home menu can be polled. If
 *    the user asks to quit there is no way to unwind the game's loop, so the
 *    process exits — abrupt, but the alternative is a homebrew that ignores the
 *    home button.
 */
static void dw3_nx_vsync_pump(void) {
    static int first_frame_logged;

    if (!appletMainLoop()) {
        INFOF("quit requested; leaving the game loop");
        exit(0);
    }
    if (!first_frame_logged) {
        first_frame_logged = 1;
        INFOF("first vsync callback reached");
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

static const char *env_or(const char *key, const char *fallback) {
    const char *v = getenv(key);
    return (v != NULL && v[0] != '\0') ? v : fallback;
}

static int init_paths(void) {
    const char *root = env_or("DW3_RUNTIME", DW3_DEFAULT_RUNTIME);
    const char *memdir = env_or("DW3_MEMCARD_DIR", NULL);
    char memcard_buf[512];
    char boot_path[512];

    if (cdfs_set_root(root) != 0) {
        ERRORF("cdfs_set_root(%s) failed", root);
        return -1;
    }
    if (cdfs_load_map(NULL) != 0) {
        ERRORF("cdfs_load_map under %s failed", root);
        return -1;
    }
    INFOF("cdfs root=%s", root);

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

    if (!dw3_nx_gpu_init()) {
        ERRORF("dw3_nx_gpu_init failed");
        return 1;
    }
    INFOF("EGL/GLES2 surface up");

    if (init_paths() != 0) {
        return 1;
    }

    g_prev_vsync_cb = Psyz_SetVSyncCb(dw3_nx_vsync_pump);
    INFOF("entering dw3_game_main()");
    return dw3_game_main();
}
