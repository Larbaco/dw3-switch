/* In-process restart policy shared by PC and Switch game hosts. */
#include "port_restart.h"

#include "memcard_host.h"
#include "port_confirm.h"
#include "port_options.h"
#include "port_screen_fade.h"
#include "port_widescreen.h"
#include "port_title_options.h"

#include <stdio.h>

#include "engine/pad.h"

static int g_restart_chord_latched;

static int restart_chord_held(void) {
    return PAD_HELD(PAD_L1) && PAD_HELD(PAD_R1) &&
           PAD_HELD(PAD_START) && PAD_HELD(PAD_SELECT);
}

/* The reset itself, shared by the chord and the field menu's Return to title
 * (P6.27): flush the dirty save slots, cancel on a failure, then hide what the
 * discarded tasks cannot. 1 = the caller resets the game. */
static int restart_perform(void) {
    if (memcard_host_section_open_any()) {
        /* A game save is half written: the reset would discard its tasks and
         * the card file would keep the old content. Try again when it ends. */
        fprintf(stderr, "dw3-port-restart: a save is being written; restart cancelled\n");
        fflush(stderr);
        return 0;
    }
    if (memcard_host_flush_all() != 0) {
        fprintf(stderr, "dw3-port-restart: save flush failed; restart cancelled\n");
        fflush(stderr);
        return 0;
    }
    dw3_port_options_close();
    dw3_port_confirm_close();
    /* The discarded title task cannot hide its overlay; EU restarts into
       CNTY_SEL, not the title, so it would otherwise draw over it. */
    dw3_port_title_overlay_reset();
    fprintf(stderr, "dw3-port-restart: saves flushed; returning to the boot screen in-process (title on US, language select on EU)\n");
    fflush(stderr);
    return 1;
}

/* Return to title through black. The reset drops the mode task without a
 * final draw, so the picture it leaves on screen (the field, 16:9) stays up
 * while the title loads, and the gutters switching off squeezed it into a 4:3
 * frame for a few frames before the title came (owner report, preview.41).
 * Now: the whole picture fades to black first (the overlay draws it, level
 * from port_screen_fade.h), the reset runs on the black screen, and the black
 * stays until the new screen is drawing (the title backdrop; on EU the
 * language select), then fades in. Every step is logged with its frame count
 * (OLED test plan E1). */
enum { RF_IDLE, RF_OUT, RF_BLACK, RF_IN };
enum {
    FADE_OUT_FRAMES = 20,
    BLACK_MIN_FRAMES = 4,
    BLACK_MAX_FRAMES = 300,   /* never stay black if the new screen is not seen */
    FADE_IN_FRAMES = 15,
};
static int g_rf_state = RF_IDLE;
static int g_rf_frames;
static unsigned g_rf_total;
static const char *g_rf_why = "";

/* P6.27: the field menu's Return to title, after its Yes / No window. */
static int g_restart_requested;

void dw3_port_restart_request(void) {
    g_restart_requested = 1;
}

static int restart_begin(const char *why) {
    if (memcard_host_section_open_any()) {
        /* Same answer as the reset itself, without fading first. */
        return restart_perform();
    }
    g_rf_state = RF_OUT;
    g_rf_frames = 0;
    g_rf_total = 0;
    g_rf_why = why;
    fprintf(stderr, "dw3-port-restart: fading to black before the reset "
            "(%s, %d frames)\n", why, (int)FADE_OUT_FRAMES);
    fflush(stderr);
    return 0;
}

/* One frame of the fade; 1 = the caller resets the game now (the screen is
 * black). */
static int restart_fade_step(void) {
    g_rf_total++;
    g_rf_frames++;
    switch (g_rf_state) {
    case RF_OUT:
        dw3_port_screen_fade_set(255 * g_rf_frames / FADE_OUT_FRAMES);
        if (g_rf_frames >= FADE_OUT_FRAMES) {
            dw3_port_screen_fade_set(255);
            fprintf(stderr, "dw3-port-restart: screen is black after %u "
                    "frames; resetting now\n", g_rf_total);
            fflush(stderr);
            g_rf_frames = 0;
            if (restart_perform()) {
                g_rf_state = RF_BLACK;
                return 1;
            }
            g_rf_state = RF_IN;   /* cancelled: back to the game */
        }
        return 0;
    case RF_BLACK:
        if (g_rf_frames >= BLACK_MIN_FRAMES &&
            (dw3_port_widescreen_new_screen_ready() ||
             g_rf_frames >= BLACK_MAX_FRAMES)) {
            fprintf(stderr, "dw3-port-restart: new screen %s after %d black "
                    "frames; fading in (%d frames)\n",
                    g_rf_frames >= BLACK_MAX_FRAMES ? "not seen" : "ready",
                    g_rf_frames, (int)FADE_IN_FRAMES);
            fflush(stderr);
            g_rf_state = RF_IN;
            g_rf_frames = 0;
        }
        return 0;
    default:
        dw3_port_screen_fade_set(255 - 255 * g_rf_frames / FADE_IN_FRAMES);
        if (g_rf_frames >= FADE_IN_FRAMES) {
            dw3_port_screen_fade_set(0);
            g_rf_state = RF_IDLE;
            fprintf(stderr, "dw3-port-restart: fade finished, %u frames from "
                    "the request (%s)\n", g_rf_total, g_rf_why);
            fflush(stderr);
        }
        return 0;
    }
}

int dw3_port_restart_poll(void) {
    int held = restart_chord_held();

    if (g_rf_state != RF_IDLE) {
        return restart_fade_step();
    }
    if (g_restart_requested) {
        g_restart_requested = 0;
        fprintf(stderr, "dw3-port-restart: requested from the field menu\n");
        fflush(stderr);
        return restart_begin("field menu");
    }
    if (!held) {
        g_restart_chord_latched = 0;
        return 0;
    }
    if (g_restart_chord_latched || !dw3_port_options_restart_enabled()) {
        return 0;
    }

    /* One activation per press. A failed flush leaves the current game intact. */
    g_restart_chord_latched = 1;
    return restart_begin("restart chord");
}
