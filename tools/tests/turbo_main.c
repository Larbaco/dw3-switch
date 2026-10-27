/* Headless P6.4 fast-forward policy (no disc / GPU / pad). */
#include "port_options.h"
#include "port_turbo.h"

#include <stdio.h>

#if VERSION_EU
enum { FIELD = 0x166, BATTLE = 0x167, REPORT = 0x1D1, STATUS = 0x207, TITLE = 0x1DD, FF_ROW = 1 };
#else
enum { FIELD = 0x158, BATTLE = 0x159, REPORT = 0x1C2, STATUS = 0x1F9, TITLE = 0x1C1, FF_ROW = 1 };
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
    /* The battle report (results screen) after a won battle runs at the
     * same speed as the battle before it. */
    frame(0, REPORT);
    if (dw3_port_turbo_speed() != 2) {
        return fail("stays on in the battle report");
    }
    frame(0, STATUS);
    if (dw3_port_turbo_speed() != 1) {
        return fail("normal speed in the field menu screens");
    }
    frame(0, TITLE);
    if (dw3_port_turbo_speed() != 1) {
        return fail("normal speed outside field, battle and report");
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

    /* P6.4 menus at 1.5x: the field menu task stamps each frame it runs. */
    dw3_port_options_set_level(FF_ROW, 4);
    frame(1, FIELD);
    frame(0, FIELD);
    if (dw3_port_turbo_speed() != 4 || dw3_port_turbo_rate_x2() != 8 ||
        dw3_port_turbo_menu_active()) {
        return fail("field at 4x before the menu");
    }
    {
        int presents = 0, run = 0, i;
        int gaps[12], n = 0;

        for (i = 0; i < 12; i++) {
            frame(0, FIELD);
            presents += dw3_port_turbo_present_due();
        }
        if (presents != 3) {
            return fail("4x presents one frame in four");
        }
        presents = 0;
        for (i = 0; i < 12; i++) {
            frame(0, FIELD);
            dw3_port_turbo_menu_frame();
            run++;
            if (dw3_port_turbo_present_due()) {
                presents++;
                gaps[n++] = run;
                run = 0;
            }
        }
        if (!dw3_port_turbo_menu_active() || dw3_port_turbo_rate_x2() != 3) {
            return fail("menu open in the field is 1.5x");
        }
        if (dw3_port_turbo_speed() != 4) {
            return fail("the field factor does not change in a menu");
        }
        if (presents != 8) {
            return fail("1.5x presents 8 frames in 12");
        }
        for (i = 1; i < n; i++) {
            if (gaps[i] + gaps[i - 1] != 3) {
                return fail("1.5x alternates 2 and 1 game frames per present");
            }
        }
    }
    /* A battle menu waiting for the player (the cursor stamps) is 1.5x too;
     * without the stamp the battle is at the full speed. */
    frame(0, BATTLE);
    frame(0, BATTLE);
    if (dw3_port_turbo_rate_x2() != 8) {
        return fail("battle animation stays at the full speed");
    }
    dw3_port_turbo_menu_frame();
    if (!dw3_port_turbo_menu_active() || dw3_port_turbo_rate_x2() != 3) {
        return fail("battle menu is 1.5x");
    }
    /* The battle report is not a menu mode: the stamp is ignored. */
    frame(0, REPORT);
    dw3_port_turbo_menu_frame();
    if (dw3_port_turbo_menu_active() || dw3_port_turbo_rate_x2() != 8) {
        return fail("the report keeps the full speed");
    }
    frame(0, STATUS);
    dw3_port_turbo_menu_frame();
    if (dw3_port_turbo_rate_x2() != 2) {
        return fail("STSTATUS stays at 1x");
    }
    /* Back in the field with no menu update: the rate returns after a frame. */
    frame(0, FIELD);
    dw3_port_turbo_menu_frame();
    if (dw3_port_turbo_rate_x2() != 3) {
        return fail("menu again");
    }
    frame(0, FIELD);
    frame(0, FIELD);
    if (dw3_port_turbo_menu_active() || dw3_port_turbo_rate_x2() != 8) {
        return fail("a killed menu task releases the rate");
    }
    /* 2x (rate 4) also drops to 1.5x in a menu; a lower rate never rises. */
    dw3_port_options_set_level(FF_ROW, 2);
    frame(0, FIELD);
    dw3_port_turbo_menu_frame();
    if (dw3_port_turbo_rate_x2() != 3) {
        return fail("2x drops to 1.5x in a menu");
    }

    printf("OK: turbo policy\n");
    return 0;
}
