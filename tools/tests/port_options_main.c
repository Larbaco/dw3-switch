/* Headless P9.2 port-options state machine (no disc / GPU). */
#include "port_options.h"

#include <stdio.h>
#include <string.h>

void dw3_port_options_test_set_pressed(unsigned mask);

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

    if (dw3_port_options_stub_count() < 1) {
        return fail("stub count");
    }
    if (dw3_port_options_stub_label(0) == NULL ||
        strstr(dw3_port_options_stub_label(0), "Widescreen") == NULL) {
        return fail("widescreen stub label");
    }
    if (dw3_port_options_stub_value(0) != 0) {
        return fail("widescreen default off");
    }

    dw3_port_options_test_set_pressed(1u << BIT_DOWN);
    dw3_port_options_tick();
    dw3_port_options_test_set_pressed(1u << BIT_CROSS);
    dw3_port_options_tick();
    if (dw3_port_options_stub_value(1) != 1) {
        return fail("toggle stub[1]");
    }

    dw3_port_options_test_set_pressed(1u << BIT_TRIANGLE);
    dw3_port_options_tick();
    if (dw3_port_options_is_open()) {
        return fail("triangle close");
    }

    dw3_port_options_open();
    dw3_port_options_test_set_pressed(1u << BIT_SELECT);
    dw3_port_options_tick();
    if (dw3_port_options_is_open()) {
        return fail("select close");
    }

    dw3_port_options_close(); /* idempotent */
    printf("open=0 stubs=%d widescreen_default=%d\n",
           dw3_port_options_stub_count(), dw3_port_options_stub_value(0));
    printf("OK: port_options state machine\n");
    return 0;
}
