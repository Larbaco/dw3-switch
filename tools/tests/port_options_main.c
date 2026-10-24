/* Headless P9.2 port-options state machine (no disc / GPU). */
#include "port_audio.h"
#include "port_options.h"

#include <stdio.h>
#include <string.h>

void dw3_port_options_test_set_pressed(unsigned mask);

void dw3_port_audio_apply(int music_step, int sfx_step) {
    (void)music_step;
    (void)sfx_step;
}

enum {
    BIT_SELECT = 0,
    BIT_UP = 4,
    BIT_DOWN = 6,
    BIT_CROSS = 13,
    BIT_TRIANGLE = 14
};

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

int main(void) {
    if (dw3_port_options_is_open()) {
        return fail("starts closed");
    }
    dw3_port_options_open();
    if (!dw3_port_options_is_open()) {
        return fail("open");
    }
    dw3_port_options_open(); /* idempotent */
    if (!dw3_port_options_is_open()) {
        return fail("re-open");
    }

    if (dw3_port_options_count() != 8) {
        return fail("option count");
    }
    if (dw3_port_options_level(DW3_PORT_OPT_BATTLE_MENU) != 0 ||
        dw3_port_options_label(DW3_PORT_OPT_BATTLE_MENU) == NULL ||
        strstr(dw3_port_options_label(DW3_PORT_OPT_BATTLE_MENU),
               "Battle cursor") == NULL) {
        return fail("Battle cursor row, default Off");
    }
    if (dw3_port_options_level(DW3_PORT_OPT_ENCOUNTER_RATE) != 0 ||
        dw3_port_options_level(DW3_PORT_OPT_TEXT_SPEED) != 0) {
        return fail("pacing rows default to 1x / Normal");
    }
    if (dw3_port_options_label(5) == NULL ||
        strstr(dw3_port_options_label(5), "Encounter") == NULL ||
        dw3_port_options_label(6) == NULL ||
        strstr(dw3_port_options_label(6), "Text speed") == NULL) {
        return fail("pacing labels");
    }
    if (dw3_port_options_label(0) == NULL ||
        strstr(dw3_port_options_label(0), "Widescreen") == NULL) {
        return fail("widescreen stub label");
    }
    if (dw3_port_options_value(0) != 1) {
        return fail("widescreen default on");
    }
    if (dw3_port_options_restart_enabled() != 0 ||
        dw3_port_options_value(2) != 0) {
        return fail("restart hotkey default off");
    }

    if (dw3_port_options_label(1) == NULL ||
        strstr(dw3_port_options_label(1), "Fast-forward") == NULL) {
        return fail("fast-forward label");
    }
    if (dw3_port_options_level(1) != 2) {
        return fail("fast-forward default 2x");
    }

    /* Fast-forward cycles 2x -> 3x -> 4x -> OFF -> 2x. */
    dw3_port_options_test_set_pressed(1u << BIT_DOWN);
    dw3_port_options_tick();
    dw3_port_options_test_set_pressed(1u << BIT_CROSS);
    dw3_port_options_tick();
    if (dw3_port_options_level(1) != 3) {
        return fail("fast-forward 3x");
    }
    dw3_port_options_tick();
    if (dw3_port_options_level(1) != 4) {
        return fail("fast-forward 4x");
    }
    dw3_port_options_tick();
    if (dw3_port_options_level(1) != 0 || dw3_port_options_value(1) != 0) {
        return fail("fast-forward off");
    }
    dw3_port_options_tick();
    if (dw3_port_options_level(1) != 2) {
        return fail("fast-forward wraps to 2x");
    }

    dw3_port_options_test_set_pressed(1u << BIT_DOWN);
    dw3_port_options_tick();
    dw3_port_options_test_set_pressed(1u << BIT_CROSS);
    dw3_port_options_tick();
    if (dw3_port_options_restart_enabled() != 1) {
        return fail("toggle restart hotkey");
    }

    dw3_port_options_test_set_pressed(1u << BIT_TRIANGLE);
    dw3_port_options_tick();
    if (dw3_port_options_is_open()) {
        return fail("triangle close");
    }

    dw3_port_options_open();
    /* Select must not close — it is the title open chord. */
    dw3_port_options_test_set_pressed(1u << BIT_SELECT);
    dw3_port_options_tick();
    if (!dw3_port_options_is_open()) {
        return fail("select must not close");
    }

    dw3_port_options_close(); /* idempotent */
    printf("open=0 options=%d widescreen_default=%d restart=%d\n",
           dw3_port_options_count(), dw3_port_options_value(0),
           dw3_port_options_restart_enabled());
    printf("OK: port_options state machine\n");
    return 0;
}
