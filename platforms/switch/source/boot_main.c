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
#include <unistd.h>

#include <psyz.h>
#include <psyz/log.h>

#include "boot_tim_host.h"
#include "cdfs.h"
#include "memcard_host.h"
#include "save_convert.h"
#include "overlay_host.h"
#include "map_view.h"
#include "port_options.h"
#include "user_switch.h"
#include "runtime_extract.h"
#include "str_stream.h"
#include "dw3_region_rt.h"
#include "dw3_game_api.h" /* last of the port headers: in a `both` image it redirects the game calls */

#include "../psyz_backend/nx.h"
#include "../psyz_backend/nx_crash.h"
#include "../psyz_backend/nx_crash_notice.h"
#include "../psyz_backend/nx_exit.h"
#include "../psyz_backend/nx_title_overlay.h"
#include "../psyz_backend/nx_debug_overlay.h"
#include "../psyz_backend/nx_mem_budget.h"
#include "../psyz_backend/nx_mesh_debug.h"
#include "../psyz_backend/nx_movie_debug.h"
#if defined(DW3_BOTH) && DW3_BOTH
#include "../psyz_backend/nx_region_boot.h"
#include "dw3_region_relaunch.h"
#endif
#if DW3_NX_DEBUG_OVERLAY
#include "debug_overlay_text.h"
#endif
#include "frame_dt.h"
#include "port_version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>
#include <netinet/in.h>

/* PSY-Z maps close() to psyz_close(), which has no Switch implementation. */
#undef close

#if !(defined(DW3_BOTH) && DW3_BOTH)
int dw3_game_main(void);
#endif

extern volatile int StCdIntrFlag;
void StCdInterrupt(void);

/* The SD layout from docs/RUNTIME_LAYOUT.md: sdmc:/switch/dw3/<ver>/. */
#ifndef DW3_DEFAULT_RUNTIME
#define DW3_DEFAULT_RUNTIME "sdmc:/switch/dw3/us"
#endif
/* P6.24: the region is a run-time fact (dw3_region(), port/region/dw3_region_rt.h):
 * constant in a plain build, chosen at boot (dw3_nx_region_boot) in a `both` one. */
#define DW3_RUNTIME_VERSION_TAG (dw3_region_name(dw3_region()))
#define DW3_RUNTIME_EXECUTABLE_TAG \
    (dw3_region() == DW3_REGION_EU ? "SLES_039.36" : "SLUS_014.36")

/* The folder that holds us/ and eu/ (a `both` image looks in both). */
#ifndef DW3_NX_BASE
#define DW3_NX_BASE "sdmc:/switch/dw3"
#endif
/* The runtime root of this run: DW3_RUNTIME, else the plain build's baked
 * path, else <base>/<region> once the region is known. */
static char g_runtime_root[512];
static const char *dw3_nx_runtime_root(void);

/* P6.28: the title menu's Quit game ends the session the way HOME's exit
 * request does: Psyz_QuitRequested turns on, the game loop returns from
 * dw3_game_main() and main() returns to hbmenu. The saves were flushed by
 * port/options/confirm.c before it asked. */
void dw3_host_request_quit(void) {
    dw3_nx_set_quit_requested();
}

static PsyzVSyncCb g_prev_vsync_cb;
static int g_applet_exit_locked;
static int g_quit_saves_flushed;
static int g_exit_pending;
static int g_exit_deferred_logged;
static uint64_t g_exit_since_us;

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
    if (!g_exit_pending) {
        for (n = 0; n < 4; n++) {
            if (appletMainLoop()) {
                continue;
            }
            g_exit_pending = 1;
            g_exit_since_us = restore_now_us();
            break;
        }
        if (!g_exit_pending) {
            return;
        }
    }
    /* P10.2: an exit request that arrives while the game is half way through
     * a save (MemCardWriteFile burst) waits for the section to end, so the
     * card file gets the whole new save, not a half written one. Bounded:
     * HOME force-kills 15 s after the request; past 8 s the section is
     * dropped and the card keeps its previous complete content. */
    if (!memcard_host_exit_ready(restore_now_us() - g_exit_since_us)) {
        if (!g_exit_deferred_logged) {
            g_exit_deferred_logged = 1;
            INFOF("applet exit: a save is being written; waiting for it");
        }
        return;
    }
    dw3_nx_set_quit_requested();
    if (!g_quit_saves_flushed) {
        g_quit_saves_flushed = 1;
        INFOF("applet exit: flat saves flushed");
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
            /* Not a save that is half written (P10.2): that one finishes
             * through the game's own next MemCard call. */
            if (memcard_host_flush_settled() != 0) {
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

/* nxlink is a run-time fact, not a build. libnx fills __nxlink_host (a
 * struct in_addr) at startup when the NetLoader started this NRO and handed it
 * the host address in the final argv token; it stays 0.0.0.0 when the NRO was
 * started from hbmenu, Sphaira or a forwarder. Only then connect stdout and
 * stderr to the host. Nothing here touches the network otherwise. */
static int g_nxlink_socket_initialized;
static Result g_nxlink_socket_result;
static int g_nxlink_fd = -1;
static int g_nxlink_host_set;
static unsigned g_nxlink_host_be; /* network byte order */
static char g_nxlink_launcher[256];

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

static void dw3_nxlink_connect(int argc, char **argv) {
    g_nxlink_launcher[0] = '\0';
    if (argc > 0 && argv && argv[0]) {
        snprintf(g_nxlink_launcher, sizeof(g_nxlink_launcher), "%s", argv[0]);
    }
    g_nxlink_host_be = (unsigned)__nxlink_host.s_addr;
    g_nxlink_host_set = g_nxlink_host_be != 0;
    if (!g_nxlink_host_set) {
        return;
    }
    /* nxlinkConnectToHost uses the default socket service. */
    g_nxlink_socket_result = socketInitializeDefault();
    if (R_FAILED(g_nxlink_socket_result)) {
        fprintf(stderr, "nxlink socketInitializeDefault failed (0x%08x)\n",
                (unsigned)g_nxlink_socket_result);
        return;
    }
    g_nxlink_socket_initialized = 1;
    g_nxlink_fd = nxlinkStdio();
    if (g_nxlink_fd < 0) {
        fprintf(stderr, "nxlinkStdio failed (%d); continuing\n", g_nxlink_fd);
    }
}

/* After dw3_nx_log_open, so the line lands in dw3.log as well as on the host. */
static void dw3_nxlink_log_state(void) {
    const unsigned char *b = (const unsigned char *)&g_nxlink_host_be;

    if (g_nxlink_host_set && g_nxlink_fd >= 0) {
        INFOF("dw3: nxlink host=%u.%u.%u.%u connected (fd=%d)", b[0], b[1],
              b[2], b[3], g_nxlink_fd);
    } else if (g_nxlink_host_set) {
        INFOF("dw3: nxlink host=%u.%u.%u.%u NOT connected (socket init=0x%08x, "
              "fd=%d); continuing with dw3.log only", b[0], b[1], b[2], b[3],
              (unsigned)g_nxlink_socket_result, g_nxlink_fd);
    } else {
        INFOF("dw3: no nxlink host (launched from %s)",
              g_nxlink_launcher[0] ? g_nxlink_launcher : "unknown");
    }
}

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
            dw3_nx_switch_gpu_defer_log();
            dw3_nx_gpu_pace_log(dt_us);
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

#if defined(DW3_BOTH) && DW3_BOTH
/* P6.38: restart into the chosen region (docs/PORT_OPTIONS.md). The Options
 * screen asks (port/options/region_relaunch.c); on Yes the NRO queues itself
 * with libnx envSetNextLoad, argument `--relaunch-region <r>`, and the game
 * loop is asked to end like the title menu's Quit game: main() returns through
 * the normal shutdown (audio, GPU, saves, log) and hbloader then starts the
 * queued NRO, which boots the asked region whatever region_default says. The
 * NRO path is argv[0], as hbmenu passes it. No relaunch when envHasNextLoad()
 * is false (the NRO was not started by hbloader: a forwarder, a title launcher):
 * the player then gets "Restart the game to apply." and the choice takes
 * effect at the next start, as before. */
static char g_nro_path[512];
static Dw3Region g_relaunch_target = DW3_REGION_NONE;

int dw3_host_relaunch_check(Dw3Region target, char *why, size_t cap) {
    const char *base = env_or("DW3_RUNTIME_BASE", DW3_NX_BASE);
    char args[640];

    if (!dw3_relaunch_target_present(base, target)) {
        snprintf(why, cap, "no %s game data under %s", dw3_region_name(target), base);
        return DW3_RELAUNCH_NO_DATA;
    }
    if (!envHasNextLoad()) {
        snprintf(why, cap, "envHasNextLoad is false (not started through hbloader)");
        return DW3_RELAUNCH_NO_HOST;
    }
    if (dw3_relaunch_nx_args(g_nro_path, target, args, sizeof(args)) < 0) {
        snprintf(why, cap, "no usable NRO path in argv[0] ('%s')", g_nro_path);
        return DW3_RELAUNCH_NO_HOST;
    }
    snprintf(why, cap, "envSetNextLoad %s", args);
    return DW3_RELAUNCH_OK;
}

int dw3_host_relaunch_request(Dw3Region target, char *why, size_t cap) {
    char args[640];
    Result rc;

    if (dw3_host_relaunch_check(target, why, cap) != DW3_RELAUNCH_OK) {
        return -1;
    }
    (void)dw3_relaunch_nx_args(g_nro_path, target, args, sizeof(args));
    rc = envSetNextLoad(g_nro_path, args);
    if (R_FAILED(rc)) {
        snprintf(why, cap, "envSetNextLoad failed (0x%08x)", (unsigned)rc);
        return -1;
    }
    g_relaunch_target = target;
    dw3_nx_set_quit_requested();
    return 0;
}
#endif

/* P6.32: the map view's cache is prepared from the runtime tree on a
 * low-priority thread on core 2 (the game and audio threads run on the default
 * core) while the title plays; it frees what it allocated when it is done
 * (port/map_view/map_view.c). */
static Thread s_map_thread;
static int s_map_thread_live;
/* BUG-1: set once the exit path has joined the workers; no thread starts after. */
static int s_workers_closed;

static void map_view_precompute_entry(void *arg) {
    (void)arg;
    INFOF("dw3-map: precompute thread running");
    dw3_map_view_precompute_run();
    INFOF("dw3-map: precompute thread finished");
}

/* The map view's CPU scalers (Lanczos2 took 6.4 s, FSR 1 2.1 s on the Switch)
 * build on a worker thread on core 2 so the game keeps taking input
 * (map_view.h). Two slots: the screen may be closed and opened again while the
 * last build still runs; a slot is closed and reused once its thread has
 * finished. When both are busy the caller builds on its own thread. */
typedef struct MapViewSlot {
    Thread thread;
    void (*fn)(void *);
    void *arg;
    volatile int state; /* 0 free, 1 running, 2 finished (thread to close) */
} MapViewSlot;

static MapViewSlot s_map_slots[2];

static void map_view_job_entry(void *p) {
    MapViewSlot *slot = (MapViewSlot *)p;

    slot->fn(slot->arg);
    __atomic_store_n(&slot->state, 2, __ATOMIC_RELEASE);
}

int dw3_map_view_async_start(void (*fn)(void *), void *arg) {
    int i;

    if (s_workers_closed) {
        return -1; /* leaving: the caller builds on its own thread */
    }
    for (i = 0; i < 2; ++i) {
        MapViewSlot *slot = &s_map_slots[i];
        int st = __atomic_load_n(&slot->state, __ATOMIC_ACQUIRE);

        if (st == 1) continue;
        if (st == 2) {
            threadWaitForExit(&slot->thread);
            threadClose(&slot->thread);
            slot->state = 0;
        }
        slot->fn = fn;
        slot->arg = arg;
        slot->state = 1;
        if (R_FAILED(threadCreate(&slot->thread, map_view_job_entry, slot, NULL,
                                  0x40000, 0x3B, 2)) ||
            R_FAILED(threadStart(&slot->thread))) {
            INFOF("dw3-map: scaler worker thread not started (slot %d)", i);
            slot->state = 0;
            return -1;
        }
        INFOF("dw3-map: scaler worker thread started (slot %d, core 2)", i);
        return 0;
    }
    return -1;
}

static void map_view_precompute_start(const char *config_dir) {
    dw3_map_view_init(config_dir);
    if (R_FAILED(threadCreate(&s_map_thread, map_view_precompute_entry, NULL,
                              NULL, 0x40000, 0x3B, 2)) ||
        R_FAILED(threadStart(&s_map_thread))) {
        INFOF("map view precompute thread not started");
    } else {
        s_map_thread_live = 1;
    }
}

/* BUG-1 (docs/SPHAIRA_EXIT.md): every worker thread this file and the
 * title overlay started is joined and threadClose()d here, before main()
 * returns. A libnx thread that finished but was never closed keeps its stack
 * svcMapMemory-borrowed (Perm_None) in the heap; Sphaira's hbl then fails the
 * read of the next NRO over that heap with 0xD401 and svcBreaks. The forwarder
 * path never reaches that read (hbl exits the process), which is why only the
 * Sphaira launch crashed. One `dw3-exit:` line per thread. */
static void dw3_nx_workers_shutdown(void) {
    int i, still = 0;

    if (s_workers_closed) {
        return;
    }
    s_workers_closed = 1;
    dw3_map_view_precompute_cancel();
    if (s_map_thread_live) {
        still += dw3_nx_exit_join(&s_map_thread, "map precompute");
        s_map_thread_live = 0;
    } else {
        dw3_nx_exit_log("thread map precompute: not started (nothing to close)");
    }
    for (i = 0; i < 2; ++i) {
        MapViewSlot *slot = &s_map_slots[i];
        int st = __atomic_load_n(&slot->state, __ATOMIC_ACQUIRE);
        char name[32];

        if (st == 0) {
            continue; /* never used, or closed already */
        }
        snprintf(name, sizeof(name), "map scaler slot %d", i);
        /* st 1 is a build still running: join waits for it (a Lanczos2 build
         * is about 6 s) within the exit budget. st 2 is finished, not closed. */
        if (dw3_nx_exit_join(&slot->thread, name) == 0) {
            slot->state = 0;
        } else {
            ++still;
        }
    }
    still += dw3_nx_title_overlay_shutdown();
    still += dw3_nx_os_threads_shutdown();
    dw3_nx_exit_log("worker threads closed in %u ms, %d still running",
                    (unsigned)dw3_nx_exit_elapsed_ms(), still);
}

/* atexit() takes an address, which the `both` redirect (a macro) cannot give. */
static void dw3_nx_flush_options(void) { dw3_port_options_flush(); }

static const char *dw3_nx_runtime_root(void) {
    return g_runtime_root[0] != '\0' ? g_runtime_root : env_or("DW3_RUNTIME", DW3_DEFAULT_RUNTIME);
}

static int init_paths(void) {
    const char *root = dw3_nx_runtime_root();
    const char *memdir = env_or("DW3_MEMCARD_DIR", NULL);
    const char *cfgdir = env_or("DW3_CONFIG_DIR", NULL);
    char memcard_buf[512];
    int memdir_explicit;
    char boot_path[512];

    /* P6.26: the chosen user's folder (user_switch.c); NULL when skipped. */
    if (memdir == NULL) {
        memdir = dw3_switch_user_memdir();
    }

    /* A card with a matching BIN and no complete versioned tree is filled in
     * here. The bar is the same callback the STR rewrite uses, so register it
     * first. */
    dw3_str_restore_set_progress(str_restore_progress);
    if (dw3_runtime_ensure(root) != 0) {
        ERRORF("runtime extract failed");
        return -1;
    }
    INFOF("runtime version=%s root=%s executable=%s",
          DW3_RUNTIME_VERSION_TAG, root, DW3_RUNTIME_EXECUTABLE_TAG);
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

    memdir_explicit = (memdir != NULL);
    if (memdir == NULL) {
        snprintf(memcard_buf, sizeof(memcard_buf), "%s/memcard", root);
        memdir = memcard_buf;
    }
    if (memcard_host_set_dir(memdir) != 0) {
        ERRORF("memcard_host_set_dir(%s) failed", memdir);
        return -1;
    }
    /* P6.25: a port with no save of its own takes the other region's (converted
     * copy; the original is never touched). */
    (void)memcard_host_setup_cross_import(root,
                                          dw3_region() == DW3_REGION_EU
                                              ? SAVE_REGION_EU
                                              : SAVE_REGION_US,
                                          memdir_explicit);
    /* P6.25 slice 2: opt-in shared store (save_store=1 in port_options.cfg),
     * inside the profile folder; off for the legacy folder and replays. */
    (void)memcard_host_setup_store(cfgdir != NULL ? cfgdir : root,
                                   dw3_switch_user_profile_dir(),
                                   dw3_region() == DW3_REGION_EU
                                       ? SAVE_REGION_EU
                                       : SAVE_REGION_US);
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
    atexit(dw3_nx_flush_options);
    map_view_precompute_start(cfgdir != NULL ? cfgdir : root);

    snprintf(boot_path, sizeof(boot_path), "%s/boot_tim.bin", root);
    if (dw3_host_load_boot_tim(boot_path) != 0) {
        ERRORF("boot TIM load failed (%s)", boot_path);
        return -1;
    }
    INFOF("boot TIM loaded from %s", boot_path);

#if defined(DW3_BOTH) && DW3_BOTH
    DW3_API(overlay_register_title)();
#elif defined(VERSION_EU) && VERSION_EU
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

/* BUG-1: set when the game loop has ended and the exit path is running. */
static int g_exit_trace;
static int g_exit_result;

/* Registered first, so it runs last: after the memcard, option and audio
 * handlers, the last point at which dw3.log can still be written. The final
 * heap probe is what the loader's next read sees; the next run's log and the
 * absence of an Atmosphère report answer the rest. */
static void dw3_nx_exit_final(void) {
    if (g_exit_trace) {
        dw3_nx_exit_heap_probe("final, after the exit handlers");
        dw3_nx_exit_log("leaving to the loader: return %d, envHasNextLoad=%d, "
                        "%u ms since the game loop ended",
                        g_exit_result, envHasNextLoad() ? 1 : 0,
                        (unsigned)dw3_nx_exit_elapsed_ms());
    }
    dw3_nx_log_close();
}

int main(int argc, char **argv) {
    int test_env_count;

    atexit(dw3_nx_exit_final);
    test_env_count = apply_test_env();

    dw3_nxlink_connect(argc, argv);
#if defined(DW3_BOTH) && DW3_BOTH
    if (argc > 0 && argv && argv[0]) {
        snprintf(g_nro_path, sizeof(g_nro_path), "%s", argv[0]);
    }
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
#ifndef DW3_NX_REPLAY
    if (g_nxlink_fd < 0)
    {
        /* Game-side fprintf(stderr) lines (dw3-port-options, memcard, ...)
         * otherwise reach no sink on an ordinary build. */
        dw3_nx_log_capture_stderr();
    }
#endif
#ifdef DW3_NX_REPLAY
    /* P6.15 replay build: game code writes markers (overlay restores, replay
     * shot/end, ...) with fprintf(stderr), which is invisible under an
     * emulator. Route it to svcOutputDebugString unless nxlink already owns
     * stderr, so Ryubing's log carries them. */
    if (g_nxlink_fd < 0)
    {
        consoleDebugInit(debugDevice_SVC);
    }
    INFOF("dw3: REPLAY BUILD (DW3_NX_REPLAY): input from sdmc:/switch/dw3/replay.rec");
    /* A failed overlay restore aborts the replay run instead of logging on. */
    dw3_overlay_set_strict(1);
#endif
    /* P6.37: the first line of the log names the build (region: US+EU until a
     * combined image has picked one; it is announced again then). */
    dw3_version_announce();
    INFOF("dw3: version=%s", DW3_NRO_VERSION_STR);
    dw3_nx_log_run_header();
    if (test_env_count > 0) {
        INFOF("dw3: DW3_NX_TEST_ENV applied %d vars: %s", test_env_count,
              DW3_NX_TEST_ENV_STR);
    }
    dw3_nxlink_log_state();
    dw3_nx_exit_loader_report(); /* BUG-1: who launched us, what the loader gave */
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

#if defined(DW3_BOTH) && DW3_BOTH
    /* P6.24: which game copy runs. Before the user step, which needs the
     * region's save folder. Forced (nothing probed, no picker, nothing written):
     * DW3_REGION (the test environment of a replay NRO). Otherwise the data on
     * the card decides, and the picker asks when both regions are there. */
    {
        Dw3RegionBootOut rb;
        Dw3Region forced = dw3_region_parse(getenv("DW3_REGION"));
        const char *base = env_or("DW3_RUNTIME_BASE", DW3_NX_BASE);
        const char *explicit_root = env_or("DW3_RUNTIME", NULL);
        const char *forced_by = "DW3_REGION";
        /* P6.38: started by a relaunch: the asked region is not "forced" (the
         * data on the card still decides, the log says so) but it does beat a
         * DW3_REGION / DW3_RUNTIME of the test environment, which named the
         * region that was running. */
        const Dw3Region relaunch = dw3_relaunch_arg_region(argc, argv);

        if (relaunch != DW3_REGION_NONE) {
            forced = DW3_REGION_NONE;
            explicit_root = NULL;
        }

        if (forced == DW3_REGION_NONE && explicit_root != NULL) {
            /* a runtime folder given by hand: judged by what it holds */
            Dw3RegionRequest req;
            Dw3RegionChoice choice;

            memset(&req, 0, sizeof(req));
            req.root = explicit_root;
            req.base = base;
            if (dw3_region_resolve(&req, &choice) == 0) {
                forced = choice.region;
                forced_by = "DW3_RUNTIME";
            }
        }
        dw3_nx_region_boot(base, forced, forced_by, relaunch, &rb);
        if (rb.result != DW3_RB_RUN || dw3_region_select(rb.region, rb.source) != 0) {
            INFOF("region: %s: leaving without starting the game",
                  rb.result == DW3_RB_NO_DATA ? "no game data" :
                  rb.result == DW3_RB_QUIT ? "left the picker" : "no region");
            dw3_nx_crash_allow_exit();
            if (g_applet_exit_locked) {
                appletUnlockExit();
                g_applet_exit_locked = 0;
            }
            dw3_nxlink_stdio_shutdown();
            if (rb.result == DW3_RB_RUN) {
                return 1; /* the copy itself was refused (ABI mismatch): a real failure */
            }
            dw3_nx_gpu_shutdown();
            dw3_nx_log_close();
            return 0;
        }
        if (explicit_root != NULL) {
            snprintf(g_runtime_root, sizeof(g_runtime_root), "%s", explicit_root);
        } else {
            snprintf(g_runtime_root, sizeof(g_runtime_root), "%s/%s", base,
                     dw3_region_name(rb.region));
        }
        dw3_version_announce(); /* now with the region */
    }
#endif

    /* P6.26: whose saves. Cancelling the system profile selector means the
     * game does not start: leave cleanly, nothing is written. */
    if (dw3_switch_user_boot(dw3_nx_runtime_root(),
                             dw3_region_name(dw3_region())) == DW3_USER_BOOT_STOP) {
        INFOF("no user chosen: leaving without starting the game");
        dw3_nx_crash_allow_exit();
        if (g_applet_exit_locked) {
            appletUnlockExit();
            g_applet_exit_locked = 0;
        }
        dw3_nxlink_stdio_shutdown();
        return 0;
    }

    if (!dw3_nx_gpu_init()) {
        ERRORF("dw3_nx_gpu_init failed");
        /* Logged setup failure, not a crash: exit(1) stays a plain exit. */
        dw3_nx_crash_allow_exit();
        dw3_switch_user_exit();
        if (g_applet_exit_locked) {
            appletUnlockExit();
            g_applet_exit_locked = 0;
        }
        dw3_nxlink_stdio_shutdown();
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
        /* BUG-1: the threads and services init_paths opened before it failed
         * are closed too, so the loader gets a clean heap back. */
        dw3_nx_exit_begin();
        g_exit_trace = 1;
        g_exit_result = 1;
        dw3_nx_exit_log("setup failed; closing what init_paths opened");
        dw3_nx_workers_shutdown();
        dw3_nx_audio_shutdown();
        dw3_nx_gpu_shutdown();
        dw3_switch_user_exit();
        if (g_applet_exit_locked) {
            appletUnlockExit();
            g_applet_exit_locked = 0;
        }
        dw3_nxlink_stdio_shutdown();
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
        INFOF("dw3_game_main returned %d; shutting down (quit path: %s)",
              game_result,
              g_exit_pending ? "HOME / applet exit"
#if defined(DW3_BOTH) && DW3_BOTH
              : g_relaunch_target != DW3_REGION_NONE ? "relaunch into the other region"
#endif
              : "title Quit game");
        dw3_nx_exit_begin();
        g_exit_trace = 1;
        dw3_nx_exit_log("game loop ended (return %d); closing in reverse order",
                        game_result);
        dw3_nx_exit_heap_probe("before cleanup");
        memcard_host_shutdown();
        dw3_nx_exit_log("memcard flushed");
        /* BUG-1: every worker thread is joined and closed first. */
        dw3_nx_workers_shutdown();
        /* P38: leave the way the P4 sample does, services closed in reverse
         * order of their opening, before libnx tears the rest down. Quit game
         * (P6.28) was the first return from main that was not a HOME kill, and
         * it left the audren worker thread running and the EGL surface and
         * nwindow open while exit() ran: Horizon reported a fatal error. */
        dw3_nx_audio_shutdown();
        INFOF("audio stopped");
        dw3_nx_gpu_shutdown();
        INFOF("gpu closed; leaving main");
        dw3_nx_exit_log("gpu closed (EGL terminated), user service closed next");
        dw3_switch_user_exit();
        {
            const int had_socket = g_nxlink_socket_initialized;

            dw3_nxlink_stdio_shutdown();
            dw3_nx_exit_log("nxlink: %s",
                            had_socket ? "socket service closed"
                                       : "socket service never opened (no nxlink host)");
        }
        if (g_applet_exit_locked) {
            appletUnlockExit();
            g_applet_exit_locked = 0;
            dw3_nx_exit_log("appletUnlockExit done");
        }
        dw3_nx_exit_heap_probe("after cleanup");
        g_exit_result = game_result;
        /* dw3.log is closed by dw3_nx_exit_final, the last atexit handler. */
        return game_result;
    }
}
