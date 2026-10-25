/* In-process restart policy shared by PC and Switch game hosts. */
#include "port_restart.h"

#include "memcard_host.h"
#include "port_confirm.h"
#include "port_options.h"
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

/* P6.27: the field menu's Return to title, after its Yes / No window. The
 * reset runs on the next poll, right after the pad update. */
static int g_restart_requested;

void dw3_port_restart_request(void) {
    g_restart_requested = 1;
}

int dw3_port_restart_poll(void) {
    int held = restart_chord_held();

    if (g_restart_requested) {
        g_restart_requested = 0;
        fprintf(stderr, "dw3-port-restart: requested from the field menu\n");
        fflush(stderr);
        return restart_perform();
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
    return restart_perform();
}
