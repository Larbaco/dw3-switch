#define _POSIX_C_SOURCE 200809L
/* P6.11: host test for port/options/internal_res.c (the policy behind the
 * Internal resolution and Native res while turbo rows). The platform and the
 * turbo state are stubs that record what the policy asks of them. */
#include "port_internal_res.h"
#include "port_options.h"
#include "port_turbo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_max = 3;
static int g_speed = 1;
static int g_will_skip;
static int g_applies;
static int g_active;
static int g_reserve;
static int g_skip;
static const char *g_mode = "handheld";

int dw3_platform_max_internal_res(void) { return g_max; }
void dw3_platform_apply_internal_res(int active, int reserve) {
    g_applies++;
    g_active = active;
    g_reserve = reserve;
    fprintf(stderr, "stub: apply active=%d reserve=%d\n", active, reserve);
}
void dw3_platform_set_mesh_skip(int skip) { g_skip = skip; }
const char *dw3_platform_res_mode(void) { return g_mode; }
int dw3_port_turbo_speed(void) { return g_speed; }
int dw3_port_turbo_frame_will_skip(void) { return g_will_skip; }

static int fail(const char *what) {
    fprintf(stderr, "FAIL: %s\n", what);
    return 1;
}

int main(void) {
    int applies;

    /* defaults: 1x, the turbo row ON */
    if (dw3_port_options_level(DW3_PORT_OPT_INTERNAL_RES) != DW3_INTERNAL_RES_1X ||
        dw3_port_options_level(DW3_PORT_OPT_INTERNAL_RES_TURBO) != 1 ||
        strcmp(dw3_port_options_key(DW3_PORT_OPT_INTERNAL_RES), "internal_res") != 0 ||
        strcmp(dw3_port_options_key(DW3_PORT_OPT_INTERNAL_RES_TURBO),
               "internal_res_turbo") != 0) {
        return fail("defaults: internal_res 1x, internal_res_turbo ON, keys");
    }
    dw3_port_internal_res_boot();
    if (g_active != 1 || g_reserve != 1 || dw3_port_internal_res_target() != 1) {
        return fail("1x boot: active 1, reserve 1");
    }

    /* a row change is pushed by sync, once */
    dw3_port_options_set_level(DW3_PORT_OPT_INTERNAL_RES, DW3_INTERNAL_RES_2X);
    dw3_port_internal_res_sync();
    if (g_active != 2 || g_reserve != 2) {
        return fail("2x: active 2, reserve 2 (the turbo switch is pre-allocated)");
    }
    applies = g_applies;
    dw3_port_internal_res_sync();
    dw3_port_internal_res_poll();
    if (g_applies != applies) {
        return fail("nothing changed: no second apply");
    }

    /* turbo on: 1x with the 2x targets kept; mesh skip follows the frame */
    g_speed = 4;
    g_will_skip = 1;
    dw3_port_internal_res_poll();
    if (g_active != 1 || g_reserve != 2 || g_skip != 1 ||
        dw3_port_internal_res_active() != 1 ||
        dw3_port_internal_res_target() != 2) {
        return fail("turbo on: active 1, reserve 2, skip 1, target stays 2");
    }
    g_will_skip = 0;
    dw3_port_internal_res_poll();
    if (g_skip != 0 || g_active != 1) {
        return fail("a frame turbo shows is not skipped");
    }
    g_speed = 1;
    dw3_port_internal_res_poll();
    if (g_active != 2 || g_reserve != 2 || g_skip != 0) {
        return fail("turbo off: back to 2x");
    }

    /* the row OFF: turbo keeps 2x, nothing is reserved, nothing is skipped */
    dw3_port_options_set_level(DW3_PORT_OPT_INTERNAL_RES_TURBO, 0);
    dw3_port_internal_res_sync();
    if (g_active != 2 || g_reserve != 1) {
        return fail("native-while-turbo OFF: reserve 1");
    }
    g_speed = 4;
    g_will_skip = 1;
    dw3_port_internal_res_poll();
    if (g_active != 2 || g_skip != 0) {
        return fail("native-while-turbo OFF: turbo stays 2x and skips nothing");
    }
    g_speed = 1;
    g_will_skip = 0;
    dw3_port_internal_res_poll();
    dw3_port_options_set_level(DW3_PORT_OPT_INTERNAL_RES_TURBO, 1);

    /* the clamp: a request above the machine's maximum is lowered and logged */
    dw3_port_options_set_level(DW3_PORT_OPT_INTERNAL_RES, DW3_INTERNAL_RES_3X);
    g_max = 2;
    dw3_port_internal_res_poll();
    if (g_active != 2 || dw3_port_internal_res_target() != 2) {
        return fail("clamp: 3x requested, 2x allowed");
    }
    dw3_port_internal_res_poll(); /* no second clamp line */
    g_mode = "docked";
    g_max = 3; /* a dock change raises the limit: the request is honoured */
    dw3_port_internal_res_poll();
    if (g_active != 3 || g_reserve != 3) {
        return fail("dock change: 3x allowed again");
    }
    g_max = 1;
    dw3_port_internal_res_poll();
    if (g_active != 1 || g_reserve != 1) {
        return fail("limit 1x: everything at 1x");
    }

    /* the environment pins a value for the run */
    g_max = 3;
    setenv("DW3_INTERNAL_RES", "2", 1);
    dw3_port_internal_res_boot();
    if (dw3_port_options_level(DW3_PORT_OPT_INTERNAL_RES) != DW3_INTERNAL_RES_2X) {
        return fail("DW3_INTERNAL_RES=2 pins the row to 2x");
    }
    printf("OK: internal res policy\n");
    return 0;
}
