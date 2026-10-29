/* P6.20 Cheats sub-screen of the Options screen and the cheat flag store
 * (docs/PORT_OPTIONS.md "Cheats", ROADMAP P6.20). Compiled only with
 * DW3_CHEATS (CMake option, default ON); a build without it has none of this.
 *
 * The screen is reached from the Options screen with Square (Y on the
 * Switch) and goes back to it with Triangle. Every cheat is off until the
 * master switch is turned on. The toggles (master, Instant win) are kept in
 * port_options.cfg; the one-shots (Fill bag, Battle now) are armed for this
 * run and used up by the game. The hooks reuse the PC test hooks of the
 * decomp patches (DW3_FORCE_ITEMS of 0070, the forced win of 0025, the
 * encounter countdown of 0062) through patch 0077.
 *
 * PR 2 adds a second page of rows (Money, Warp, Give item) to the screen.
 * They are one-shots too, armed at the title and applied by the field at its
 * first idle frame (patches 0118, 0119, 0120); the picks of the item id and
 * count are not kept between runs. */
#include "port_button_map.h"
#include "port_cheats.h"
#include "port_options.h"
#include "port_options_groups.h"
#include "cheats_internal.h"
#include "memcard_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(DW3_PORT_OPTIONS_TEST) && defined(DW3_TITLE_NATIVE_OVERLAY)
#include "title_overlay_art.h"
#endif
#ifndef DW3_PORT_OPTIONS_TEST
#include "port_text.h"
#include <libgpu.h>
#include "engine/pad.h"
#endif

static int g_ready;         /* dw3_cheats_init ran */
static int g_master;        /* cheat_enabled */
static int g_instant_win;   /* cheat_instant_win */
static int g_fill_armed;    /* Fill bag: ready, this run only */
static int g_battle_armed;  /* Battle now: ready, this run only */
static int g_money;         /* Money: DW3_CHEAT_MONEY_*, armed, this run only */
static int g_warp;          /* Warp: 0 OFF, 1..DW3_CHEAT_WARP_AREAS armed */
static int g_item_id = DW3_CHEAT_ITEM_FIRST; /* the pick, for Give item */
static int g_item_count = 10;                /* and its count */
static int g_give_armed;    /* Give item: ready, this run only */
static int g_open;
static int g_cursor;
static int g_scroll; /* first row on screen (the sub-screen scrolls) */
static int g_marked_saves;  /* counted when the screen opens */
static int g_game_marked;   /* a cheat changed the game in progress */
static int g_battle_serial; /* battles started this run */
static int g_won_serial;    /* the battle Instant win last ended */

#ifdef DW3_PORT_OPTIONS_TEST
enum { CH_UP = 4, CH_DOWN = 6, CH_CIRCLE = 12, CH_CROSS = 13,
       CH_TRIANGLE = 14, CH_RIGHT = 5, CH_LEFT = 7, CH_L1 = 10, CH_R1 = 11 };
static int ch_pressed(int bit) {
    return dw3_port_options_test_bit(bit);
}
#define CH_REPEATED(bit) 0
#else
enum { CH_UP = PAD_UP, CH_DOWN = PAD_DOWN, CH_CIRCLE = PAD_CIRCLE,
       CH_CROSS = PAD_CROSS, CH_TRIANGLE = PAD_TRIANGLE, CH_RIGHT = PAD_RIGHT,
       CH_LEFT = PAD_LEFT, CH_L1 = PAD_L1, CH_R1 = PAD_R1 };
static int ch_pressed(int bit) {
    return PAD_PRESSED(bit);
}
#define CH_REPEATED(bit) PAD_REPEATED(bit)
#endif

/* The areas of the Warp row, by game mode (WSTAG numbers in the stage
 * select, vendor/dw3_decomp/src/debug/stage_select): Asuka City, the Digimon
 * Lab, the Digimon Arena, Central Park, the Wire Forest entrance, Tyranno
 * Valley, Chinlong City, the Bulk Swamp, Suzaku City, the Noise Desert,
 * Byakko City, Snow Mountain and Genbu City. The field leaves for the mode
 * with argument 0, the first start of the stage, as the stage select does. */
static const int g_warp_modes[DW3_CHEAT_WARP_AREAS] = {
    0x200, 0x206, 0x209, 0x21D, 0x21E, 0x22B, 0x22E,
    0x233, 0x23E, 0x248, 0x25D, 0x262, 0x26F};

int dw3_cheats_warp_mode(int index) {
    return index >= 0 && index < DW3_CHEAT_WARP_AREAS ? g_warp_modes[index] : 0;
}

/* A value of the file: 0 or 1; anything else is logged and read as 0. */
static int ch_file_bool(const char *key) {
    int v = dw3_port_options_file_int(key, 0);

    if (v != 0 && v != 1) {
        CHEAT_WARN("dw3-cheats: %s=%d is not valid; using 0", key, v);
        return 0;
    }
    return v;
}

/* PC test hook (replay scenarios, like the DW3_FORCE_* ones): arms the
 * one-shots at boot, e.g. DW3_CHEATS_ARM=fill_bag,battle_now. They still need
 * the master switch. PR 2 adds money_10k | money_100k | money_max,
 * warp=<game mode, e.g. 0x206> and item=<id>:<count>. Unset (the Switch has
 * no environment) it does nothing. */
static void ch_arm_spec(const char *arm) {
    const char *at;

    if (arm == NULL || arm[0] == '\0') {
        return;
    }
    if (strstr(arm, "fill_bag") != NULL) {
        g_fill_armed = 1;
    }
    if (strstr(arm, "battle_now") != NULL) {
        g_battle_armed = 1;
    }
    CHEAT_INFO("dw3-cheats: DW3_CHEATS_ARM=%s (fill bag %s, battle now %s)",
               arm, g_fill_armed ? "ready" : "off",
               g_battle_armed ? "ready" : "off");
    if (strstr(arm, "money_100k") != NULL) {
        g_money = DW3_CHEAT_MONEY_100K;
    } else if (strstr(arm, "money_10k") != NULL) {
        g_money = DW3_CHEAT_MONEY_10K;
    } else if (strstr(arm, "money_max") != NULL) {
        g_money = DW3_CHEAT_MONEY_MAX;
    }
    at = strstr(arm, "warp=");
    if (at != NULL) {
        int mode = (int)strtol(at + 5, NULL, 0);
        int k;

        for (k = 0; k < DW3_CHEAT_WARP_AREAS; k++) {
            if (g_warp_modes[k] == mode) {
                g_warp = k + 1;
            }
        }
    }
    at = strstr(arm, "item=");
    if (at != NULL) {
        char *end = NULL;
        long id = strtol(at + 5, &end, 0);
        long count = end != NULL && *end == ':' ? strtol(end + 1, NULL, 0) : 1;

        if (id >= DW3_CHEAT_ITEM_FIRST && id <= DW3_CHEAT_ITEM_LAST &&
            count >= 1 && count <= DW3_CHEAT_ITEM_COUNT_MAX) {
            g_item_id = (int)id;
            g_item_count = (int)count;
            g_give_armed = 1;
        }
    }
    CHEAT_INFO("dw3-cheats: DW3_CHEATS_ARM page 2: money %d, warp %d, give "
               "item %s (id %d x%d)", g_money, g_warp,
               g_give_armed ? "ready" : "off", g_item_id, g_item_count);
}

static void ch_arm_from_env(void) {
    ch_arm_spec(getenv("DW3_CHEATS_ARM"));
}

/* The Options page of the native overlay: Square/Y Cheats in its hints
 * once the master switch is on. */
static void ch_options_page(void) {
#if !defined(DW3_PORT_OPTIONS_TEST) && defined(DW3_TITLE_NATIVE_OVERLAY)
    dw3_pc_title_overlay_set_options_page(DW3_TITLE_OPTIONS_PAGE_OPTIONS,
                                          g_master);
#endif
}

void dw3_cheats_init(const char *memdir) {
    g_ready = 1;
    g_master = ch_file_bool(DW3_CHEAT_KEY_ENABLED);
    g_instant_win = ch_file_bool(DW3_CHEAT_KEY_INSTANT_WIN);
    g_fill_armed = 0;
    g_battle_armed = 0;
    g_money = 0;
    g_warp = 0;
    g_give_armed = 0;
    g_item_id = DW3_CHEAT_ITEM_FIRST;
    g_item_count = 10;
    g_game_marked = 0;
    dw3_cheat_saves_set_dir(memdir);
    CHEAT_INFO("dw3-cheats: cheats %s (instant win %s); Options, Square/Y "
               "opens the Cheats screen",
               g_master ? "ON" : "off", g_instant_win ? "on" : "off");
    ch_arm_from_env();
    g_marked_saves = dw3_cheat_saves_count(1);
    ch_options_page();
}

/* The first use initialises the store: the hosts load port_options.cfg and
 * set the memcard dir before the game runs, and the first caller is the
 * title menu (or a hook, when a PC test boots straight into a field). No
 * host code is needed, so a build without DW3_CHEATS has none. */
static void ch_ready(void) {
    if (!g_ready) {
        dw3_cheats_init(memcard_host_dir());
    }
}

/* P5.1 soak driver (platforms/pc/src/mem_sampler.c): arms one-shots again in
 * the middle of a run, with the same spec as DW3_CHEATS_ARM. */
void dw3_cheats_soak_arm(const char *spec) {
    ch_ready();
    ch_arm_spec(spec);
}

int dw3_cheats_enabled(void) {
    ch_ready();
    return g_master;
}

int dw3_cheats_game_marked(void) {
    return g_game_marked;
}

/* A cheat changed the game in progress: the next save of it is marked. */
static void ch_mark_game(const char *what) {
    if (!g_game_marked) {
        CHEAT_INFO("dw3-cheats: %s changed the game; a save of it will be "
                   "marked as written with cheats", what);
    }
    g_game_marked = 1;
}

/* --- the screen -------------------------------------------------------- */

int dw3_cheats_value(int row) {
    switch (row) {
    case DW3_CHEAT_ROW_MASTER:
        return g_master;
    case DW3_CHEAT_ROW_FILL_BAG:
        return g_fill_armed;
    case DW3_CHEAT_ROW_INSTANT_WIN:
        return g_instant_win;
    case DW3_CHEAT_ROW_BATTLE_NOW:
        return g_battle_armed;
    case DW3_CHEAT_ROW_SAVES:
        return g_marked_saves;
    case DW3_CHEAT_ROW_MONEY:
        return g_money;
    case DW3_CHEAT_ROW_WARP:
        return g_warp;
    case DW3_CHEAT_ROW_ITEM_ID:
        return g_item_id;
    case DW3_CHEAT_ROW_ITEM_COUNT:
        return g_item_count;
    case DW3_CHEAT_ROW_GIVE_ITEM:
        return g_give_armed;
    default:
        return 0;
    }
}

int dw3_cheats_cursor(void) {
    return g_cursor;
}

int dw3_cheats_is_open(void) {
    return g_open;
}

/* P9.9: the Cheats group is a scrolling sub-screen of the Options list (ten
 * rows, six on screen); the view holds every row, the cursor is the row. */
static void ch_push_view(void) {
#if !defined(DW3_PORT_OPTIONS_TEST) && defined(DW3_TITLE_NATIVE_OVERLAY)
    int values[DW3_TITLE_OPTION_ROWS];
    int i;

    g_scroll = dw3_optlist_scroll_for(g_scroll, g_cursor, DW3_CHEAT_ROWS);
    for (i = 0; i < DW3_TITLE_OPTION_ROWS; i++) {
        values[i] = i < DW3_CHEAT_ROWS ? dw3_cheats_value(i) : 0;
    }
    dw3_pc_title_overlay_set_options_page(
        dw3_optlist_page(DW3_OPTGRP_CHEATS, g_scroll), g_master);
    dw3_pc_title_overlay_set_options_view(g_cursor, values,
                                          DW3_TITLE_OPTION_ROWS);
#endif
}

void dw3_cheats_open(void) {
    ch_ready();
    if (g_open) {
        return;
    }
    g_open = 1;
    g_cursor = 0;
    g_scroll = 0;
    g_marked_saves = dw3_cheat_saves_count(0);
    CHEAT_INFO("dw3-cheats: open (master %s, fill bag %s, instant win %s, "
               "battle now %s, %d marked save(s))",
               g_master ? "on" : "off", g_fill_armed ? "ready" : "off",
               g_instant_win ? "on" : "off", g_battle_armed ? "ready" : "off",
               g_marked_saves);
    CHEAT_INFO("dw3-cheats: page 2: money %d, warp %d, give item %s (id %d "
               "x%d)", g_money, g_warp, g_give_armed ? "ready" : "off",
               g_item_id, g_item_count);
    ch_push_view();
}

void dw3_cheats_close(void) {
    if (!g_open) {
        return;
    }
    g_open = 0;
    ch_options_page();
    CHEAT_INFO("dw3-cheats: close (back to Options)");
}

/* Page 2 rows (the master must be on). `dir` is +1 or -1; `big` is 1 for
 * a single step, 10 for L1 / R1 on the two numbers. Cross and Right step
 * forward, Left backward; the choices wrap around, the numbers stop at their
 * ends. Returns 1 when something changed. */
static int ch_step(int row, int dir, int big) {
    int v;

    if (!g_master) {
        return 0;
    }
    switch (row) {
    case DW3_CHEAT_ROW_MONEY:
        g_money = (g_money + dir + DW3_CHEAT_MONEY_MODES) %
                  DW3_CHEAT_MONEY_MODES;
        CHEAT_INFO("dw3-cheats: money %s", g_money == 0 ? "cancelled"
                   : g_money == DW3_CHEAT_MONEY_10K
                   ? "+10000 (the next idle moment in the field)"
                   : g_money == DW3_CHEAT_MONEY_100K
                   ? "+100000 (the next idle moment in the field)"
                   : "MAX (the next idle moment in the field)");
        return 1;
    case DW3_CHEAT_ROW_WARP:
        g_warp = (g_warp + dir + DW3_CHEAT_WARP_AREAS + 1) %
                 (DW3_CHEAT_WARP_AREAS + 1);
        if (g_warp == 0) {
            CHEAT_INFO("dw3-cheats: warp cancelled");
        } else {
            CHEAT_INFO("dw3-cheats: warp to mode 0x%x (area %d), at the next "
                       "idle moment in a field", g_warp_modes[g_warp - 1],
                       g_warp);
        }
        return 1;
    case DW3_CHEAT_ROW_ITEM_ID:
        v = g_item_id + dir * big;
        if (v < DW3_CHEAT_ITEM_FIRST) {
            v = DW3_CHEAT_ITEM_FIRST;
        } else if (v > DW3_CHEAT_ITEM_LAST) {
            v = DW3_CHEAT_ITEM_LAST;
        }
        if (v == g_item_id) {
            return 0;
        }
        g_item_id = v;
        return 1;
    case DW3_CHEAT_ROW_ITEM_COUNT:
        v = g_item_count + dir * big;
        if (v < 1) {
            v = 1;
        } else if (v > DW3_CHEAT_ITEM_COUNT_MAX) {
            v = DW3_CHEAT_ITEM_COUNT_MAX;
        }
        if (v == g_item_count) {
            return 0;
        }
        g_item_count = v;
        return 1;
    case DW3_CHEAT_ROW_GIVE_ITEM:
        g_give_armed = !g_give_armed;
        if (g_give_armed) {
            CHEAT_INFO("dw3-cheats: give item ready: id %d x%d (at the next "
                       "idle moment in a field)", g_item_id, g_item_count);
        } else {
            CHEAT_INFO("dw3-cheats: give item cancelled");
        }
        return 1;
    default:
        return 0;
    }
}

/* Confirm on a row. Returns 1 when something changed. */
static int ch_toggle(int row) {
    if (row == DW3_CHEAT_ROW_MASTER) {
        g_master = !g_master;
        if (!g_master) {
            /* Off means off: nothing stays ready behind the switch. */
            g_fill_armed = 0;
            g_battle_armed = 0;
            g_money = 0;
            g_warp = 0;
            g_give_armed = 0;
        }
        (void)dw3_port_options_store_int(DW3_CHEAT_KEY_ENABLED, g_master);
        CHEAT_INFO("dw3-cheats: master %s", g_master ? "on" : "off");
        return 1;
    }
    if (!g_master || row == DW3_CHEAT_ROW_SAVES) {
        return 0; /* greyed rows, and the read-only count */
    }
    if (row == DW3_CHEAT_ROW_INSTANT_WIN) {
        g_instant_win = !g_instant_win;
        (void)dw3_port_options_store_int(DW3_CHEAT_KEY_INSTANT_WIN,
                                         g_instant_win);
        CHEAT_INFO("dw3-cheats: instant win %s", g_instant_win ? "on" : "off");
        return 1;
    }
    if (row == DW3_CHEAT_ROW_FILL_BAG) {
        g_fill_armed = !g_fill_armed;
        CHEAT_INFO("dw3-cheats: fill bag %s", g_fill_armed
                   ? "ready (the next field menu fills the bag)" : "cancelled");
        return 1;
    }
    if (row == DW3_CHEAT_ROW_BATTLE_NOW) {
        g_battle_armed = !g_battle_armed;
        CHEAT_INFO("dw3-cheats: battle now %s", g_battle_armed
                   ? "ready (the next footstep in a battle area starts one)"
                   : "cancelled");
        return 1;
    }
    return ch_step(row, 1, 1);
}

void dw3_cheats_tick(void) {
    if (!g_open) {
        return;
    }
    if (ch_pressed(CH_TRIANGLE)) {
        dw3_port_options_play_sound(DW3_PORT_OPTIONS_SOUND_CANCEL);
        dw3_cheats_close();
        return;
    }
    if (ch_pressed(CH_UP) || CH_REPEATED(CH_UP)) {
        if (g_cursor > 0) {
            g_cursor--;
            dw3_port_options_play_sound(DW3_PORT_OPTIONS_SOUND_CURSOR);
        }
    }
    if (ch_pressed(CH_DOWN) || CH_REPEATED(CH_DOWN)) {
        if (g_cursor < DW3_CHEAT_ROWS - 1) {
            g_cursor++;
            dw3_port_options_play_sound(DW3_PORT_OPTIONS_SOUND_CURSOR);
        }
    }
    if (ch_pressed(CH_CROSS) || ch_pressed(CH_CIRCLE)) {
        if (ch_toggle(g_cursor)) {
            dw3_port_options_play_sound(DW3_PORT_OPTIONS_SOUND_SELECT);
        }
    }
    /* Left / Right step the page 2 rows (auto-repeat), L1 / R1 by ten on
     * the item id and the count. */
    if (g_cursor >= DW3_CHEAT_PAGE_ROWS) {
        int dir = 0;
        int big = 1;

        if (ch_pressed(CH_LEFT) || CH_REPEATED(CH_LEFT)) {
            dir = -1;
        } else if (ch_pressed(CH_RIGHT) || CH_REPEATED(CH_RIGHT)) {
            dir = 1;
        } else if (ch_pressed(CH_L1) || CH_REPEATED(CH_L1)) {
            dir = -1;
            big = 10;
        } else if (ch_pressed(CH_R1) || CH_REPEATED(CH_R1)) {
            dir = 1;
            big = 10;
        }
        if (dir != 0 && (big == 1 || g_cursor == DW3_CHEAT_ROW_ITEM_ID ||
                         g_cursor == DW3_CHEAT_ROW_ITEM_COUNT) &&
            ch_step(g_cursor, dir, big)) {
            dw3_port_options_play_sound(DW3_PORT_OPTIONS_SOUND_CURSOR);
        }
    }
    ch_push_view();
}

void dw3_cheats_draw_panel(void) {
#ifndef DW3_PORT_OPTIONS_TEST
    static const char *const label[DW3_CHEAT_ROWS] = {
        "Cheats", "Fill bag", "Instant win", "Battle now", "Cheat saves",
        "Money", "Warp", "Item id", "Item count", "Give item"};
    static const char *const money[DW3_CHEAT_MONEY_MODES] = {
        "OFF", "+10000", "+100000", "MAX"};
    char buf[DW3_CHEAT_PAGE_ROWS][40];
    char back_buf[40];
    const char *lines[DW3_CHEAT_PAGE_ROWS + 3];
    DISPENV env;
    int page = g_cursor / DW3_CHEAT_PAGE_ROWS;
    int n = 0;
    int i;

    if (!g_open) {
        return;
    }
    GetDispEnv(&env);
    lines[n++] = page ? "Cheats 2/2 (test aid)" : "Cheats 1/2 (test aid)";
    lines[n++] = "-----------------";
    for (i = 0; i < DW3_CHEAT_PAGE_ROWS; i++) {
        int row = page * DW3_CHEAT_PAGE_ROWS + i;
        int v = dw3_cheats_value(row);
        char mark = row == g_cursor ? '>' : ' ';

        if (row == DW3_CHEAT_ROW_SAVES || row == DW3_CHEAT_ROW_WARP ||
            row == DW3_CHEAT_ROW_ITEM_ID || row == DW3_CHEAT_ROW_ITEM_COUNT) {
            snprintf(buf[i], sizeof(buf[i]), "%c %s: %d", mark, label[row], v);
        } else if (row == DW3_CHEAT_ROW_MONEY) {
            snprintf(buf[i], sizeof(buf[i]), "%c %s: %s", mark, label[row],
                     money[v]);
        } else {
            snprintf(buf[i], sizeof(buf[i]), "%c %s: %s", mark, label[row],
                     v ? (row == DW3_CHEAT_ROW_FILL_BAG ||
                          row == DW3_CHEAT_ROW_BATTLE_NOW ||
                          row == DW3_CHEAT_ROW_GIVE_ITEM ? "READY" : "ON")
                       : "OFF");
        }
        lines[n++] = buf[i];
    }
    {
        char name[24];

        dw3_button_hint_name(DW3_BTN_TRIANGLE, "Triangle (S)", name, sizeof(name));
        snprintf(back_buf, sizeof(back_buf), "%s: Back", name);
        lines[n++] = back_buf;
    }
    dw3_port_text_draw_panel((int)env.disp.x + 32, (int)env.disp.y + 32, 256,
                             128, lines, n);
#endif
}

/* --- hooks --------------------------------------------------------------- */

const char *dw3_cheats_getenv(const char *name) {
    static char items[24];
    const char *env = getenv(name);

    ch_ready();
    if (env != NULL || !g_master) {
        return env;
    }
    if (g_fill_armed && strcmp(name, "DW3_FORCE_ITEMS") == 0) {
        g_fill_armed = 0;
        snprintf(items, sizeof(items), "%d-%d:%d", DW3_CHEAT_FILL_FIRST,
                 DW3_CHEAT_FILL_LAST, DW3_CHEAT_FILL_COUNT);
        CHEAT_INFO("dw3-cheats: fill bag: items %d..%d x%d (used up)",
                   DW3_CHEAT_FILL_FIRST, DW3_CHEAT_FILL_LAST,
                   DW3_CHEAT_FILL_COUNT);
        ch_mark_game("fill bag");
        return items;
    }
    return NULL;
}

void dw3_cheats_battle_begin(void) {
    ch_ready();
    g_battle_serial++;
}

int dw3_cheats_take_instant_win(void) {
    const char *fw = getenv("DW3_FORCE_WIN");

    /* The variable wins, as in dw3_cheats_getenv: its hook (patch 0025)
     * ends the battle itself, once per run. */
    if (fw != NULL && fw[0] != '\0' && fw[0] != '0') {
        return 0;
    }
    if (!g_master || !g_instant_win || g_won_serial == g_battle_serial) {
        return 0;
    }
    g_won_serial = g_battle_serial;
    CHEAT_INFO("dw3-cheats: instant win (battle %d of this run)",
               g_battle_serial);
    ch_mark_game("instant win");
    return 1;
}

int dw3_cheats_battle_now_armed(void) {
    ch_ready();
    return g_master && g_battle_armed;
}

void dw3_cheats_battle_now_started(void) {
    if (!dw3_cheats_battle_now_armed()) {
        return;
    }
    g_battle_armed = 0;
    CHEAT_INFO("dw3-cheats: battle now: a random battle starts on this "
               "footstep (used up)");
    ch_mark_game("battle now");
}

int dw3_cheats_take_money(int current, int max, int *out) {
    static const int add[DW3_CHEAT_MONEY_MODES] = {0, 10000, 100000, 0};
    int v;

    ch_ready();
    if (!g_master || g_money == DW3_CHEAT_MONEY_OFF) {
        return 0;
    }
    if (g_money == DW3_CHEAT_MONEY_MAX) {
        v = max;
    } else {
        v = current + add[g_money];
        if (v > max || v < current) {
            v = max;
        }
    }
    CHEAT_INFO("dw3-cheats: money %s: %d -> %d (used up)",
               g_money == DW3_CHEAT_MONEY_MAX ? "MAX"
               : g_money == DW3_CHEAT_MONEY_100K ? "+100000" : "+10000",
               current, v);
    g_money = DW3_CHEAT_MONEY_OFF;
    ch_mark_game("money");
    *out = v;
    return 1;
}

int dw3_cheats_take_item(int *id, int *count) {
    ch_ready();
    if (!g_master || !g_give_armed) {
        return 0;
    }
    g_give_armed = 0;
    *id = g_item_id;
    *count = g_item_count;
    CHEAT_INFO("dw3-cheats: give item: id %d x%d (used up)", g_item_id,
               g_item_count);
    ch_mark_game("give item");
    return 1;
}

int dw3_cheats_take_warp(int *mode) {
    ch_ready();
    if (!g_master || g_warp == 0) {
        return 0;
    }
    *mode = g_warp_modes[g_warp - 1];
    CHEAT_INFO("dw3-cheats: warp to mode 0x%x (used up)", *mode);
    g_warp = 0;
    ch_mark_game("warp");
    return 1;
}

void dw3_cheats_title_started(void) {
    ch_ready();
    if (g_open) {
        /* The Options screen went away under it (P6.9 restart). */
        g_open = 0;
        ch_options_page();
    }
    if (g_game_marked) {
        CHEAT_INFO("dw3-cheats: title: the next game starts unmarked");
    }
    g_game_marked = 0;
}

void dw3_cheats_save_written(int port, int slot, int offset,
                             const void *data, int size) {
    ch_ready();
    (void)dw3_cheat_saves_mark(port, slot, offset, data, size, g_game_marked);
}

void dw3_cheats_save_loaded(int port, int slot, const void *data, int size) {
    ch_ready();
    g_game_marked = dw3_cheat_saves_is_marked(port, slot, data, size);
    if (g_game_marked) {
        char name[16];

        if (port == 2) { /* MEMCARD_HOST_AUTO */
            snprintf(name, sizeof(name), "autosave.bin");
        } else {
            snprintf(name, sizeof(name), "save%d.bin", port);
        }
        CHEAT_INFO("dw3-cheats: loaded %s slot %d, which was written "
                   "with cheats: the game stays marked", name, slot);
    }
}

