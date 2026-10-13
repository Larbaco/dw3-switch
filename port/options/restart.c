/* In-process restart policy shared by PC and Switch game hosts. */
#include "port_restart.h"

#include "memcard_host.h"
#include "port_options.h"

#include <stdio.h>

#include "engine/pad.h"

static int g_restart_chord_latched;

static int restart_chord_held(void) {
    return PAD_HELD(PAD_L1) && PAD_HELD(PAD_R1) &&
           PAD_HELD(PAD_START) && PAD_HELD(PAD_SELECT);
}

int dw3_port_restart_poll(void) {
    int held = restart_chord_held();

    if (!held) {
        g_restart_chord_latched = 0;
        return 0;
    }
    if (g_restart_chord_latched || !dw3_port_options_restart_enabled()) {
        return 0;
    }

    /* One activation per press. A failed flush leaves the current game intact. */
    g_restart_chord_latched = 1;
    if (memcard_host_flush_all() != 0) {
        fprintf(stderr, "dw3-port-restart: save flush failed; restart cancelled\n");
        fflush(stderr);
        return 0;
    }
    dw3_port_options_close();
    fprintf(stderr, "dw3-port-restart: saves flushed; returning to title in-process\n");
    fflush(stderr);
    return 1;
}
