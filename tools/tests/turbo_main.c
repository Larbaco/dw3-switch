/* Headless P6.4 fast-forward policy (no disc / GPU / pad). */
#include "port_options.h"
#include "port_turbo.h"

#include <stdio.h>

#if VERSION_EU
enum { FIELD = 0x166, BATTLE = 0x167, TITLE = 0x1DD, FF_ROW = 1 };
#else
enum { FIELD = 0x158, BATTLE = 0x159, TITLE = 0x1C1, FF_ROW = 1 };
#endif

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

/* One frame with R3 held or released. */
static void frame(int r3, int mode_file) {
    dw3_port_turbo_test_set(r3, mode_file);
    dw3_port_turbo_poll();
}

int main(void) {
    frame(0, FIELD);
    if (dw3_port_turbo_speed() != 1) {
        return fail("starts at normal speed");
    }

    /* A press toggles once, however long it is held. */
    frame(1, FIELD);
    if (dw3_port_turbo_speed() != 2) {
        return fail("R3 turns 2x on in the field");
    }
    frame(1, FIELD);
    frame(1, FIELD);
    if (dw3_port_turbo_speed() != 2) {
        return fail("holding R3 does not toggle again");
    }

    frame(0, BATTLE);
    if (dw3_port_turbo_speed() != 2) {
        return fail("stays on in battle");
    }
    frame(0, TITLE);
    if (dw3_port_turbo_speed() != 1) {
        return fail("normal speed outside field and battle");
    }
    frame(0, FIELD);
    if (dw3_port_turbo_speed() != 2) {
        return fail("back on when the field returns");
    }

    dw3_port_options_set_level(FF_ROW, 4);
    if (dw3_port_turbo_speed() != 4) {
        return fail("speed follows the option");
    }

    frame(1, FIELD);
    frame(0, FIELD);
    if (dw3_port_turbo_speed() != 1) {
        return fail("second press turns it off");
    }

    /* Option OFF: R3 does nothing. */
    dw3_port_options_set_level(FF_ROW, 0);
    frame(1, FIELD);
    frame(0, FIELD);
    if (dw3_port_turbo_speed() != 1) {
        return fail("option OFF disables R3");
    }

    /* Turning the option OFF while running stops it. */
    dw3_port_options_set_level(FF_ROW, 3);
    frame(1, FIELD);
    frame(0, FIELD);
    if (dw3_port_turbo_speed() != 3) {
        return fail("3x on");
    }
    dw3_port_options_set_level(FF_ROW, 0);
    frame(0, FIELD);
    dw3_port_options_set_level(FF_ROW, 3);
    frame(0, FIELD);
    if (dw3_port_turbo_speed() != 1) {
        return fail("option OFF stops a running fast-forward");
    }

    printf("OK: turbo policy\n");
    return 0;
}
