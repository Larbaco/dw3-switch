/* P6.4 fast-forward policy shared by PC and Switch game hosts. The hosts own
 * the pacing (PC: frame limiter at speed x the base rate; Switch: present one
 * frame in `speed`); this only decides the speed. */
#include "port_turbo.h"

#include "port_options.h"
#include "port_internal_res.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef DW3_PORT_TURBO_TEST
#include "port_battle_options.h"
#include "port_map_view.h"
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

/* Menus opened inside the field (the field menu list, the Options / Cheats
 * screen over it, the Return to title Yes / No window) run at 1.5x, not at the
 * full setting and not at 1x (owner decision 2026-10-27: "pode acelerar os
 * menus um pouco, algo tipo 50%"). Rates are kept in half game frames per
 * vsync so that 1.5x is an integer: 3. At 1.5x the presents alternate between
 * 2 and 1 game frames. */
#define DW3_TURBO_MENU_RATE_X2 3

enum { DW3_PORT_OPT_FAST_FORWARD = 1 };

static int g_env_read;
static int g_active;
static int g_latched;
static int g_logged_speed = 1;
static unsigned g_poll_frames;   /* game frames seen by dw3_port_turbo_poll */
static unsigned g_menu_stamp;    /* g_poll_frames when the field menu last ran */
static int g_menu_seen;
static int g_menu_logged;        /* the menu rate is in force (for the log) */
static int g_credit_x2;          /* present_due: game frames owed, in halves */
static int g_frame_skip;         /* this frame will not be shown (see frame_will_skip) */
static unsigned g_menu_frames;   /* game frames at the menu rate, for the log */

#ifdef DW3_PORT_TURBO_TEST
static int g_test_r3;
static int g_test_mode_file;

void dw3_port_turbo_test_set(int r3_held, int mode_file) {
    g_test_r3 = r3_held;
    g_test_mode_file = mode_file;
}
static int turbo_r3_held(void) { return g_test_r3; }
static int turbo_mode_file(void) { return g_test_mode_file; }
#define TURBO_LOG(msg, ...) fprintf(stderr, "dw3-turbo: " msg "\n", __VA_ARGS__)
#else
static int turbo_r3_held(void) { return PAD_HELD(PAD_R3); }
static int turbo_mode_file(void) { return dw3_overlay_current_mode_file(); }
/* psyz_log, not stderr: on Switch stderr only reaches nxlink, and these
 * lines are the check in sdmc:/switch/dw3/dw3.log. */
#define TURBO_LOG(msg, ...) INFOF("dw3-turbo: " msg, __VA_ARGS__)
#endif

static int g_menu_mode; /* mode file when the menu rate started */

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
    /* P6.32 phase 6: the field hotkey's view runs at 1x, whatever the field was on. The
     * toggle itself is left as it was; the speed comes back with the field. */
    if (dw3_port_map_view_field_active()) {
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

/* Called by the field menu task and the battle menu cursor (patch 0176) on
 * each update while a menu waits for the player. A stamp, not a
 * flag: a task that is killed stops stamping and the menu rate ends one frame
 * later by itself, so no code path can leave the game stuck at the slow rate. */
void dw3_port_turbo_menu_frame(void) {
    g_menu_stamp = g_poll_frames;
    g_menu_seen = 1;
}

int dw3_port_turbo_menu_active(void) {
    int mode_file;

    if (!g_menu_seen || g_poll_frames - g_menu_stamp > 1) {
        return 0;
    }
    if (dw3_port_turbo_speed() < 2) {
        return 0;
    }
    /* The field's menus and the battle's (FIGHTSTG: the command menu and the
     * lists are sub overlays of it, WFIGHTMN, so the mode file is the same). */
    mode_file = turbo_mode_file();
    return mode_file == DW3_TURBO_FIELDSTG || mode_file == DW3_TURBO_FIGHTSTG;
}

int dw3_port_turbo_rate_x2(void) {
    int full = dw3_port_turbo_speed() * 2;

    if (full > 2 && dw3_port_turbo_menu_active() &&
        DW3_TURBO_MENU_RATE_X2 < full) {
        return DW3_TURBO_MENU_RATE_X2;
    }
    return full;
}

/* Log the edges of the menu rate; the speed line (`speed Nx`) keeps reporting
 * the field's factor, so a test can see that only the menu changed. */
static void turbo_menu_track(int on) {
    if (on == g_menu_logged) {
        return;
    }
    g_menu_logged = on;
    if (on) {
        g_menu_frames = 0;
        g_menu_mode = turbo_mode_file();
        TURBO_LOG("menu 1.5x (speed %dx) in the %s, mode file 0x%x",
                  dw3_port_turbo_speed(),
                  g_menu_mode == DW3_TURBO_FIGHTSTG ? "battle menu"
                                                    : "field menus",
                  g_menu_mode);
    } else {
        TURBO_LOG("menu off (frames=%u) after the %s", g_menu_frames,
                  g_menu_mode == DW3_TURBO_FIGHTSTG ? "battle menu"
                                                    : "field menus");
    }
}

int dw3_port_turbo_present_due(void) {
    int rate = dw3_port_turbo_rate_x2();
    int due = 1;

    if (rate > 2) {
        g_credit_x2 += 2;
        if (g_credit_x2 < rate) {
            due = 0;
        } else {
            g_credit_x2 -= rate;
            if (g_credit_x2 >= rate) {
                g_credit_x2 = 0;
            }
        }
    } else {
        g_credit_x2 = 0;
    }
    return due;
}

int dw3_port_turbo_frame_will_skip(void) {
    return g_frame_skip;
}

/* What dw3_port_turbo_present_due() will answer at the end of this frame, from
 * the credit it holds now (its arithmetic, on a copy). */
static int turbo_peek_skip(void) {
    int rate;

    if (dw3_port_turbo_speed() < 2) {
        return 0;
    }
    rate = dw3_port_turbo_rate_x2();
    return rate > 2 && g_credit_x2 + 2 < rate;
}

void dw3_port_turbo_poll(void) {
    int speed;

    turbo_read_env();
    g_poll_frames++;
#ifndef DW3_PORT_TURBO_TEST
    /* P6.32: the full-screen map swallows the pad, R3 included: a press in it
     * must not flip the fast-forward state behind the player's back. A press
     * held when the screen closes does not count as a new one. */
    if (turbo_r3_held() && (dw3_port_map_view_active() || dw3_port_map_view_field_active())) {
        g_latched = 1;
    } else
#endif
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
    /* Once per game frame, so the frame count is the menu's length. */
    if (dw3_port_turbo_rate_x2() == DW3_TURBO_MENU_RATE_X2 &&
        dw3_port_turbo_menu_active()) {
        turbo_menu_track(1);
        g_menu_frames++;
    } else {
        turbo_menu_track(0);
    }
    g_frame_skip = turbo_peek_skip();
#ifndef DW3_PORT_TURBO_TEST
    /* P6.11: the multiplier and the mesh skip of this frame, before it draws. */
    dw3_port_internal_res_poll();
#endif
}
