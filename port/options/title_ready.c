/* PRESS START only when the game is ready to answer it (patch 0199).
 *
 * The original title menu (src/menus/title/menu.c, runPressStart) starts the
 * prompt in substate 1 and draws it on every tick, but it reads Start only in
 * substate 2, after seven ticks. A press made while the prompt was already on
 * screen but still in substate 1 was dropped: the owner had to press again
 * (2026-10-31, OLED). The patch asks this module on every tick whether the
 * pad is being read; if not, it skips the draw, so the prompt appears on the
 * tick that already polls Start (no press is buffered and nothing the player
 * can press changes).
 *
 * The module also keeps the proof: the frame where the prompt was first drawn,
 * the frame where Start started to be read, and the first accepted press, one
 * line per visit to the title (a visit is a title menu task: the first boot,
 * Return to title, Quit to title, the region restart each start a new one).
 * Frames are PadChkVsync counts, the same clock as the replay recordings.
 *
 * DW3_TITLE_EARLY_PROMPT=1 (PC test aid, not an option) draws the prompt in the
 * dead window as the original does, to show that the replay scenario sees it. */
#include "port_title_options.h"

#include <stdio.h>
#include <time.h>

extern char *getenv(const char *);
extern unsigned dw3_pad_host_vsync_count(void);
extern int dw3_port_title_logo_pending(void);

/* A frame this slow (ms) while the title waits for Start is reported: a press
 * made during it can fall between two pad reads. */
#define SLOW_FRAME_MS 100.0
#define SLOW_FRAME_LOGS 4
/* The EU logo is composed on the first frame its animations are done (a frame
 * of some hundred ms). Once the pad is read the prompt waits for it, at most
 * this many frames: it must never wait for good if the logo is not drawn. */
#define LOGO_WAIT_MAX 30

static unsigned g_shown, g_ready, g_accepted, g_hidden, g_logo_wait, g_slow_logs;
static int g_have_shown, g_have_ready, g_have_accepted, g_printed;
static double g_last_ms;

static double now_ms(void) {
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

/* Once both frames are known: the proof line. N is when the prompt was drawn,
 * M when the pad started to be read. M <= N means a press is never lost. */
static void print_proof(void) {
    if (g_printed || !g_have_shown || !g_have_ready) {
        return;
    }
    g_printed = 1;
    if (g_ready <= g_shown) {
        if (g_hidden > 0) {
            fprintf(stderr,
                    "dw3-title: press start shown at frame %u, input ready at frame %u "
                    "(cause none: the prompt was held back %u frame(s) until Start was read",
                    g_shown, g_ready, g_hidden);
            if (g_logo_wait > 0) {
                fprintf(stderr, " and the logo was composed (%u frame(s) of that)", g_logo_wait);
            }
            fprintf(stderr, ")\n");
        } else {
            fprintf(stderr,
                    "dw3-title: press start shown at frame %u, input ready at frame %u "
                    "(cause none: Start was already being read)\n",
                    g_shown, g_ready);
        }
    } else {
        fprintf(stderr,
                "dw3-title: press start shown at frame %u, input ready at frame %u "
                "(cause dead window: the original menu draws the prompt %u frame(s) before it reads Start)\n",
                g_shown, g_ready, g_ready - g_shown);
    }
    fflush(stderr);
}

/* The title menu task starts (the first boot, Return to title, a restart). */
void dw3_port_title_prompt_begin(void) {
    g_have_shown = g_have_ready = g_have_accepted = g_printed = 0;
    g_hidden = g_logo_wait = 0;
    g_slow_logs = 0;
    g_last_ms = 0.0;
}

/* One tick of the menu task on PRESS START, before the original reads the pad.
 * Returns 1 when this tick reads Start (so the prompt may be drawn). */
int dw3_port_title_prompt_frame(int substate, int start_pressed) {
    unsigned frame = dw3_pad_host_vsync_count();
    double ms = now_ms();
    int open = substate >= 2;

    if (g_last_ms > 0.0 && ms - g_last_ms >= SLOW_FRAME_MS && !g_have_accepted &&
        g_slow_logs < SLOW_FRAME_LOGS) {
        g_slow_logs++;
        fprintf(stderr,
                "dw3-title: slow frame of %.0f ms before frame %u (title substate %d); "
                "a press made during it can fall between two pad reads\n",
                ms - g_last_ms, frame, substate);
        fflush(stderr);
    }
    g_last_ms = ms;
    if (open && !g_have_ready) {
        g_have_ready = 1;
        g_ready = frame;
        print_proof();
    }
    if (substate == 2 && start_pressed && !g_have_accepted) {
        g_have_accepted = 1;
        g_accepted = frame;
        fprintf(stderr, "dw3-title: first Start accepted at frame %u (+%u after input opened)\n",
                g_accepted, g_accepted - g_ready);
        fflush(stderr);
    }
    {
        static int early = -1;
        int draw = open;

        if (early < 0) {
            early = getenv("DW3_TITLE_EARLY_PROMPT") != NULL;
        }
        if (early) {
            return 1;
        }
        if (open && substate < 3 && dw3_port_title_logo_pending() && g_logo_wait < LOGO_WAIT_MAX) {
            g_logo_wait++;
            draw = 0;
        }
        if (!draw) {
            g_hidden++;
        }
        return draw;
    }
}

/* The menu is about to draw PRESS START. */
void dw3_port_title_prompt_drawn(void) {
    if (!g_have_shown) {
        g_have_shown = 1;
        g_shown = dw3_pad_host_vsync_count();
        print_proof();
    }
}
