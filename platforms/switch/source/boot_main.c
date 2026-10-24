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
#include "port_options.h"
#include "runtime_extract.h"
#include "str_stream.h"

#include "../psyz_backend/nx.h"
#include "../psyz_backend/nx_crash.h"
#include "../psyz_backend/nx_crash_notice.h"
#include "../psyz_backend/nx_debug_overlay.h"
#include "../psyz_backend/nx_mem_budget.h"
#include "../psyz_backend/nx_mesh_debug.h"
#include "../psyz_backend/nx_movie_debug.h"
#if DW3_NX_DEBUG_OVERLAY
#include "debug_overlay_text.h"
#endif
#include "frame_dt.h"

#include <stdio.h>
#include <stdlib.h>
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
#if defined(VERSION_EU) && VERSION_EU
#define DW3_RUNTIME_VERSION_TAG "eu"
#define DW3_RUNTIME_EXECUTABLE_TAG "SLES_039.36"
#else
#define DW3_RUNTIME_VERSION_TAG "us"
#define DW3_RUNTIME_EXECUTABLE_TAG "SLUS_014.36"
#endif

static PsyzVSyncCb g_prev_vsync_cb;
static int g_applet_exit_locked;
static int g_quit_saves_flushed;

static uint64_t restore_now_us(void) {
    return armTicksToNs(armGetSystemTick()) / 1000ULL;
}

/* Current libnx: appletGetMessage does eventWait(..., 0), so appletMainLoop
 * is non-blocking. Older notes that it hung the first frame applied to an
 * older SDK where GetMessage waited forever. Drain a few messages per vsync
 * so ExitRequested (HOME) sets Psyz_QuitRequested and dirty flat saves flush
 * before the process is torn down (atexit is not reliable on HOME). */
static void dw3_nx_service_applet_quit(void) {
    int n;

    if (Psyz_QuitRequested()) {
        return;
    }
    for (n = 0; n < 4; n++) {
        if (appletMainLoop()) {
            continue;
        }
        dw3_nx_set_quit_requested();
        if (!g_quit_saves_flushed) {
            int flush_rc;

            g_quit_saves_flushed = 1;
            flush_rc = memcard_host_flush_all();
            if (flush_rc != 0) {
                ERRORF("applet exit: memcard flush_all failed");
            } else {
                INFOF("applet exit: flat saves flushed");
            }
        }
        break;
    }
}

/* P10.2 focus/sleep wiring — hardware-unverified on the OLED (the
 * "applet focus:" lines below have never been observed on a real console).
 * libnx: appletGetFocusState() returns a cached value that appletProcessMessage
 * refreshes via ICommonStateGetter::GetCurrentFocusState (cmd 9) inside our
 * appletMainLoop drain, when FocusStateChanged (15) is delivered. Out of
 * focus is AppletFocusState_Background (3, HOME menu open / console asleep)
 * or AppletFocusState_OutOfFocus (2, LibraryApplet overlay). Our NRO is
 * AppletType_LibraryApplet (hbloader), so appletSetFocusHandlingMode is
 * unavailable and AM suspends us on its default policy — the flush here is
 * therefore best effort before suspension. Exit (msg 4) stays the only
 * source of Psyz_QuitRequested; focus must not quit the game.
 * POLICY: pause the audren pull and flush dirty flats once per out-of-focus
 * episode; on return to InFocus unpause and suppress the one 60-pump
 * frame-dt window that contains the suspended gap (see the dt clamp in the
 * vsync pump). Docs: docs/P5.3_HOME_QUIT.md. */
static volatile int g_applet_focus_out;
static int g_applet_focus_gap_in_window;

static void dw3_nx_service_applet_focus(void) {
    AppletFocusState focus;

    if (Psyz_QuitRequested()) {
        return;
    }
    focus = appletGetFocusState();
    if (focus != AppletFocusState_InFocus) {
        g_applet_focus_gap_in_window = 1;
        if (!g_applet_focus_out) {
            g_applet_focus_out = 1;
            INFOF("applet focus: lost (state=%d, flushing)", (int)focus);
            Psyz_AudioPause();
            if (memcard_host_flush_all() != 0) {
                ERRORF("applet focus: memcard flush_all failed");
            } else {
                INFOF("applet focus: flat saves flushed");
            }
        }
        return;
    }
    if (g_applet_focus_out) {
        g_applet_focus_out = 0;
        Psyz_AudioUnpause();
        INFOF("applet focus: restored (state=%d)", (int)focus);
    }
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
 * Also drains applet messages (HOME / ExitRequested) via non-blocking
 * appletMainLoop — see dw3_nx_service_applet_quit. The game loop then sees
 * Psyz_QuitRequested and returns from dw3_game_main.
 */
static void dw3_nx_vsync_pump(void) {
    static unsigned pumps;
    static uint64_t last_dt_us;

    /* P5.4: note frame-loop progress so a crash report can tell a boot fault
     * from a mid-game fault. Two-word store, before anything that could
     * fault in this pump. */
    pumps++;
    dw3_nx_crash_note_vsync(pumps, last_dt_us);

    dw3_nx_service_applet_quit();
    dw3_nx_service_applet_focus();

    /* The pump runs once per VSync(), from inside it, *after* the frame was
     * presented — so the count is a boot progress meter for free. The game's
     * own boot path makes five calls before its main loop (four VSync(0) and a
     * VSync(3) in src/engine/system/main.c), then one per frame; a count that
     * stops at 3 says the game never reached the loop's first frame, and which
     * call it stopped on says what it was doing. Logged one by one up to 16 and
     * then at powers of two, so a healthy boot stays logarithmic. The count
     * was already advanced once at the top of this function (before the P5.4
     * crash note); advancing it again here made every "60-pump" window 30
     * frames long and doubled the HUD fps (preview.29). */
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
            if (g_applet_focus_gap_in_window) {
                /* P10.2 (hardware-unverified): the window spanned a focus
                 * loss/sleep and its wall time includes the suspended gap —
                 * only the logging/overlay stats read this, never game
                 * pacing, so clamp to the clean 1 s 60-pump baseline. */
                g_applet_focus_gap_in_window = 0;
                INFOF("frame dt us=%llu includes an applet suspend gap; "
                      "clamped to 1000000",
                      (unsigned long long)dt_us);
                dt_us = 1000000;
            }
            INFOF("frame dt us=%llu", (unsigned long long)dt_us);
            last_dt_us = dt_us; /* report field: see dw3_nx_crash_note_vsync */
#if DW3_NX_DEBUG_MOVIE
            /* Movie producer/consumer health for this 60-pump window: how many
             * STR frames the game actually took, how many the producer never
             * published (fc_skipped), and how often the two bridge readers
             * (video on the main thread, XA on the audio thread) overlapped on
             * the one shared FILE*. `reads`/`read_overlap` are window deltas of
             * the cumulative bridge counters. Logged only when the window had
             * movie activity, so a non-movie window stays silent. */
            {
                static unsigned long prev_reads;
                static unsigned long prev_overlaps;
                Dw3NxMovieDebugWindow movie;
                unsigned long reads = 0;
                unsigned long overlaps = 0;

                dw3_nx_movie_debug_take_window(&movie);
                dw3_str_stream_get_debug(&reads, &overlaps);
                if (movie.handed != 0 || reads != prev_reads) {
                    INFOF("movie dbg: handed=%u fc_skipped=%llu restarts=%u "
                          "reads=%lu read_overlap=%lu",
                          (unsigned)movie.handed,
                          (unsigned long long)movie.fc_skipped,
                          (unsigned)movie.restarts,
                          reads - prev_reads, overlaps - prev_overlaps);
                }
                prev_reads = reads;
                prev_overlaps = overlaps;
            }
#endif
#if DW3_NX_DEBUG_MEM
            {
                Dw3NxMemBudgetSample mem;

                dw3_nx_mem_budget_sample(&mem);
                dw3_nx_mem_budget_log(&mem);
#if DW3_NX_DEBUG_OVERLAY
                dw3_nx_debug_overlay_note_mem(&mem);
#endif
            }
#endif
#if DW3_NX_DEBUG_MESH
            {
                Dw3NxMeshDebugWindow mesh;

                dw3_nx_mesh_debug_take_window(&mesh);
                INFOF("mesh dbg: avail=%d tris_enq=%llu tris_flush=%llu "
                      "frames_mesh=%u frames_blit=%u soft=%u "
                      "flush_us=%llu upload_us=%llu uploads=%u "
                      "upload_px=%llu present_us=%llu",
                      mesh.mesh_available,
                      (unsigned long long)mesh.tris_enqueued,
                      (unsigned long long)mesh.tris_flushed, mesh.frames_mesh,
                      mesh.frames_blit, mesh.soft_fallback,
                      (unsigned long long)mesh.flush_us,
                      (unsigned long long)mesh.upload_us, mesh.uploads,
                      (unsigned long long)mesh.upload_px,
                      (unsigned long long)mesh.present_us);
#if DW3_NX_DEBUG_OVERLAY
                {
                    Dw3DebugOverlayMeshSnap overlay_mesh;

                    overlay_mesh.valid = 1;
                    overlay_mesh.mesh_available = mesh.mesh_available;
                    overlay_mesh.tris_enqueued = mesh.tris_enqueued;
                    overlay_mesh.tris_flushed = mesh.tris_flushed;
                    overlay_mesh.flush_us = mesh.flush_us;
                    overlay_mesh.upload_us = mesh.upload_us;
                    overlay_mesh.present_us = mesh.present_us;
                    overlay_mesh.frames_mesh = mesh.frames_mesh;
                    overlay_mesh.frames_blit = mesh.frames_blit;
                    overlay_mesh.soft_fallback = mesh.soft_fallback;
                    dw3_nx_debug_overlay_note_sample(dt_us, &overlay_mesh);
                }
#endif
            }
#elif DW3_NX_DEBUG_OVERLAY
            {
                Dw3DebugOverlayMeshSnap overlay_mesh;

                memset(&overlay_mesh, 0, sizeof(overlay_mesh));
                dw3_nx_debug_overlay_note_sample(dt_us, &overlay_mesh);
            }
#endif
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
    const char *cfgdir = env_or("DW3_CONFIG_DIR", NULL);
    char memcard_buf[512];
    char boot_path[512];

    /* A card with a matching BIN and no complete versioned tree is filled in
     * here. The bar is the same callback the STR rewrite uses, so register it
     * first. */
    dw3_str_restore_set_progress(str_restore_progress);
#if (defined(VERSION_US) && VERSION_US) || (defined(VERSION_EU) && VERSION_EU)
    if (dw3_runtime_ensure(root) != 0) {
        ERRORF("runtime extract failed");
        return -1;
    }
    INFOF("runtime version=%s root=%s executable=%s",
          DW3_RUNTIME_VERSION_TAG, root, DW3_RUNTIME_EXECUTABLE_TAG);
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
    /* Dirty flat saves also flush on HOME via vsync appletMainLoop (P5.3).
     * atexit covers clean return from dw3_game_main / process exit. */
    atexit(memcard_host_shutdown);
    INFOF("memcard dir=%s", memdir);

    /* P9.4: sdmc:/switch/dw3/<ver>/port_options.cfg, loaded before anything
     * reads widescreen / fast-forward / restart. Each change is written when
     * it is made (nothing to flush on HOME); atexit only retries a failure. */
    (void)dw3_port_options_persist_init(cfgdir != NULL ? cfgdir : root);
    atexit(dw3_port_options_flush);

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

#ifndef DW3_NRO_VERSION_STR
#define DW3_NRO_VERSION_STR "unknown"
#endif

/* Development only: CMake -DDW3_NX_TEST_ENV="K=V,K=V" bakes evidence-aid
 * variables (DW3_START_MODE, DW3_FORCE_ENCOUNTER, DW3_MESH_SOFT, ...) into the
 * NRO. Ryubing cannot pass an environment to a homebrew NRO, so this is how a
 * headless emulator run reaches FIGHTSTG. Empty (the default) is a no-op. */
#ifndef DW3_NX_TEST_ENV_STR
#define DW3_NX_TEST_ENV_STR ""
#endif

static int apply_test_env(void) {
    static const char spec[] = DW3_NX_TEST_ENV_STR;
    char buf[sizeof(spec)];
    char *save = NULL;
    char *item;
    int n = 0;

    if (spec[0] == '\0') {
        return 0;
    }
    memcpy(buf, spec, sizeof(spec));
    for (item = strtok_r(buf, ",", &save); item != NULL;
         item = strtok_r(NULL, ",", &save)) {
        char *eq = strchr(item, '=');

        if (eq == NULL || eq == item) {
            continue;
        }
        *eq = '\0';
        if (setenv(item, eq + 1, 1) == 0) {
            n++;
        }
    }
    return n;
}

int main(int argc, char **argv) {
    int test_env_count;

    (void)argc;
    (void)argv;
    test_env_count = apply_test_env();

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
    /* hbmenu only shows NACP metadata for installed SD copies; nxlink loads
     * still get the same label on the host log. */
    fprintf(stderr, "dw3: version=%s\n", DW3_NRO_VERSION_STR);
#endif

    /* Order matters: the pad and the SD mount have to be up before anything
     * opens a runtime path, and the log file is opened as early as it can be so
     * a boot that dies does so with its reasons on disk — Ryubing has no nxlink
     * and guest stderr goes nowhere there. */
    dw3_nx_input_init();
#ifdef DW3_NX_RUMBLE_SELFTEST
    /* Build-time diagnostic: one short HD rumble pulse at boot, before the
     * game runs (no SHOCKTST / in-game vibration needed). See
     * docs/P10.4_RUMBLE.md for the nxlink smoke plan. */
    dw3_nx_rumble_boot_selftest();
#endif

    if (!dw3_nx_fs_mount_sd()) {
        /* Not fatal: the log falls back to stderr and cdfs will report what it
         * cannot find. */
        ERRORF("sdmc: mount failed; runtime paths will not resolve");
    }
    dw3_nx_log_open();
#ifdef DW3_NX_REPLAY
    /* P6.15 replay build: game code writes markers (overlay restores, replay
     * shot/end, ...) with fprintf(stderr), which is invisible under an
     * emulator. Route it to svcOutputDebugString unless nxlink already owns
     * stderr, so Ryubing's log carries them. */
#ifdef DW3_NXLINK_STDIO
    if (g_nxlink_fd < 0)
#endif
    {
        consoleDebugInit(debugDevice_SVC);
    }
    INFOF("dw3: REPLAY BUILD (DW3_NX_REPLAY): input from sdmc:/switch/dw3/replay.rec");
    /* A failed overlay restore aborts the replay run instead of logging on. */
    dw3_overlay_set_strict(1);
#endif
    INFOF("dw3: version=%s", DW3_NRO_VERSION_STR);
    if (test_env_count > 0) {
        INFOF("dw3: DW3_NX_TEST_ENV applied %d vars: %s", test_env_count,
              DW3_NX_TEST_ENV_STR);
    }
#ifdef DW3_NXLINK_STDIO
    INFOF("nxlink socket init=0x%08x stdio fd=%d",
          (unsigned)g_nxlink_socket_result, g_nxlink_fd);
#endif
    /* P5.4: SD crash report sink. Does not touch memcard atexit / quit-flush. */
    dw3_nx_crash_install();

    /* HOME exit request: give the process ~15s to flush and leave main()
     * instead of an immediate force-kill (see appletLockExit). */
    if (R_SUCCEEDED(appletLockExit())) {
        g_applet_exit_locked = 1;
        INFOF("appletLockExit OK");
    } else {
        ERRORF("appletLockExit failed; HOME may force-kill before flush");
    }

    if (!dw3_nx_gpu_init()) {
        ERRORF("dw3_nx_gpu_init failed");
        /* Logged setup failure, not a crash: exit(1) stays a plain exit. */
        dw3_nx_crash_allow_exit();
        if (g_applet_exit_locked) {
            appletUnlockExit();
            g_applet_exit_locked = 0;
        }
#ifdef DW3_NXLINK_STDIO
        dw3_nxlink_stdio_shutdown();
#endif
        return 1;
    }
    INFOF("EGL/GLES2 surface up");

    /* P5.4: "the last run crashed" panel for a few seconds (or until A),
     * only when the install latch above rotated a report; otherwise this
     * returns at once and draws nothing. */
    dw3_nx_crash_notice_show();

    if (init_paths() != 0) {
        /* Logged setup failure (runtime, audio, memcard, boot TIM), not a
         * crash: exit(1) stays a plain exit. */
        dw3_nx_crash_allow_exit();
        if (g_applet_exit_locked) {
            appletUnlockExit();
            g_applet_exit_locked = 0;
        }
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
#if DW3_NX_FORCE_CRASH
    /* Debug builds only — see docs/P5.4_CRASH.md. */
    dw3_nx_crash_force_fault();
#endif
    {
        int game_result = dw3_game_main();
        memcard_host_shutdown();
#ifdef DW3_NXLINK_STDIO
        dw3_nxlink_stdio_shutdown();
#endif
        if (g_applet_exit_locked) {
            appletUnlockExit();
            g_applet_exit_locked = 0;
        }
        return game_result;
    }
}
