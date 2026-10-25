/* P6.4 fast-forward policy shared by PC and Switch game hosts. The hosts own
 * the pacing (PC: frame limiter at speed x the base rate; Switch: present one
 * frame in `speed`); this only decides the speed. */
#include "port_turbo.h"

#include "port_options.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef DW3_PORT_TURBO_TEST
#include "port_battle_options.h"
#include "engine/pad.h"
#include "overlay_host.h"

#include <psyz/log.h>
#endif

/* The mode files fast-forward runs in: the field, the battle and the battle
 * report (STFGTREP, the results screen between a won battle and the field;
 * docs/P6.4_FAST_FORWARD.md). Everything else runs at normal speed. */
#if VERSION_EU
#define DW3_TURBO_FIELDSTG 0x166
#define DW3_TURBO_FIGHTSTG 0x167
#define DW3_TURBO_STFGTREP 0x1D1
#else
#define DW3_TURBO_FIELDSTG 0x158
#define DW3_TURBO_FIGHTSTG 0x159
#define DW3_TURBO_STFGTREP 0x1C2
#endif

enum { DW3_PORT_OPT_FAST_FORWARD = 1 };

static int g_env_read;
static int g_active;
static int g_latched;
static int g_logged_speed = 1;

#ifdef DW3_PORT_TURBO_TEST
static int g_test_r3;
static int g_test_mode_file;

void dw3_port_turbo_test_set(int r3_held, int mode_file) {
    g_test_r3 = r3_held;
    g_test_mode_file = mode_file;
}
static int turbo_r3_held(void) { return g_test_r3; }
static int turbo_mode_file(void) { return g_test_mode_file; }
#define TURBO_LOG(msg, arg) fprintf(stderr, "dw3-turbo: " msg "\n", arg)
#else
static int turbo_r3_held(void) { return PAD_HELD(PAD_R3); }
static int turbo_mode_file(void) { return dw3_overlay_current_mode_file(); }
/* psyz_log, not stderr: on Switch stderr only reaches nxlink, and these
 * lines are the check in sdmc:/switch/dw3/dw3.log. */
#define TURBO_LOG(msg, arg) INFOF("dw3-turbo: " msg, arg)
#endif

static void turbo_read_env(void) {
    const char *env;
    int speed;

    if (g_env_read) {
        return;
    }
    g_env_read = 1;
    env = getenv("DW3_TURBO");
    if (env == NULL || env[0] < '0' || env[0] > '9' || env[1] != '\0') {
        return;
    }
    speed = env[0] - '0';
    if (speed == 0) {
        dw3_port_options_apply_env(DW3_PORT_OPT_FAST_FORWARD, 0);
        return;
    }
    if (speed < DW3_TURBO_SPEED_MIN || speed > DW3_TURBO_SPEED_MAX) {
        return;
    }
    dw3_port_options_apply_env(DW3_PORT_OPT_FAST_FORWARD, speed);
    g_active = 1;
}

int dw3_port_turbo_speed(void) {
    int speed;
    int mode_file;

    turbo_read_env();
    speed = dw3_port_options_level(DW3_PORT_OPT_FAST_FORWARD);
    if (!g_active || speed < DW3_TURBO_SPEED_MIN) {
        return 1;
    }
#ifndef DW3_PORT_TURBO_TEST
    /* P9.7 slice 2: the Options screen over a held battle runs at 1x (a held
     * direction would repeat 4x as fast); the battle's speed comes back on
     * resume. */
    if (dw3_port_battle_options_holding()) {
        return 1;
    }
#endif
    mode_file = turbo_mode_file();
    if (mode_file != DW3_TURBO_FIELDSTG && mode_file != DW3_TURBO_FIGHTSTG &&
        mode_file != DW3_TURBO_STFGTREP) {
        return 1;
    }
    return speed > DW3_TURBO_SPEED_MAX ? DW3_TURBO_SPEED_MAX : speed;
}

void dw3_port_turbo_poll(void) {
    int speed;

    turbo_read_env();
    if (!turbo_r3_held()) {
        g_latched = 0;
    } else if (!g_latched) {
        /* One toggle per press. */
        g_latched = 1;
        if (dw3_port_options_level(DW3_PORT_OPT_FAST_FORWARD) >=
            DW3_TURBO_SPEED_MIN) {
            g_active = !g_active;
            TURBO_LOG("%s", g_active ? "on" : "off");
        }
    }
    if (dw3_port_options_level(DW3_PORT_OPT_FAST_FORWARD) <
        DW3_TURBO_SPEED_MIN) {
        g_active = 0;
    }
    speed = dw3_port_turbo_speed();
    if (speed != g_logged_speed) {
        g_logged_speed = speed;
        TURBO_LOG("speed %dx", speed);
    }
}
