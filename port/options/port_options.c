#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L /* clock_gettime under -std=c11 */
#endif
/* P9.2 port options shell — stays on MODE_TITLE; never sets menu choice. */
#include "port_options.h"
#include "port_audio.h"
#include "port_autosave.h"
#include "port_battle_menu.h"
#include "port_button_map.h"
#include "port_glyphs.h"
#include "port_help.h"
#include "port_walkthrough_ui.h"
#include "port_options_cfg.h"
#include "port_options_groups.h"
#include "port_pacing.h"
#include "port_turbo.h"
#if DW3_OPT_REGION_ROW
#include "dw3_region.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef DW3_PORT_OPTIONS_TEST
#define PO_INFO(...) fprintf(stderr, __VA_ARGS__)
#define PO_WARN(...) fprintf(stderr, __VA_ARGS__)
#else
#include <psyz/log.h>
/* psyz_log, not stderr: on Switch stderr only reaches nxlink, and the load
 * and save lines are the check in sdmc:/switch/dw3/dw3.log. */
#define PO_INFO(...) INFOF(__VA_ARGS__)
#define PO_WARN(...) WARNF(__VA_ARGS__)
#endif

#if !defined(DW3_PORT_OPTIONS_TEST) && defined(DW3_TITLE_NATIVE_OVERLAY)
/* The native overlay draws the options screen (platforms/pc/src). */
#include "title_overlay_art.h"
#endif

#ifndef DW3_PORT_OPTIONS_TEST
#include "memcard_host.h"
#include "port_text.h"
#include <libgpu.h>
#include "engine/pad.h"
#include "engine/sound.h"
/* Psyz_VideoAlloc* capture is PC/PSY-Z only — compile out on Switch. */
#if defined(DW3_PC_HOST)
#include <psyz/video.h>
#include <stdlib.h>
#endif
#endif

/* DW3_PORT_OPT_ENCOUNTER_RATE (5), DW3_PORT_OPT_TEXT_SPEED (6),
 * DW3_PORT_OPT_BATTLE_MENU (7), DW3_PORT_OPT_AUTOSAVE (8) and
 * DW3_PORT_OPT_BUTTON_LAYOUT (9) are in
 * port_options.h: port/pacing/pacing.c, port/battle_menu/battle_menu_memory.c
 * and port/autosave/autosave.c read them. */
enum {
    DW3_PORT_OPT_COUNT = 15,
    DW3_PORT_OPT_WIDESCREEN = 0,
    DW3_PORT_OPT_FAST_FORWARD = 1,
    DW3_PORT_OPT_RESTART = 2,
    DW3_PORT_OPT_MUSIC_VOLUME = 3,
    DW3_PORT_OPT_SFX_VOLUME = 4
};

/* port_options_groups.h repeats these registry numbers. */
_Static_assert(DW3_PORT_OPT_WIDESCREEN == 0 && DW3_PORT_OPT_FAST_FORWARD == 1 &&
               DW3_PORT_OPT_RESTART == 2 && DW3_PORT_OPT_MUSIC_VOLUME == 3 &&
               DW3_PORT_OPT_SFX_VOLUME == 4 && DW3_PORT_OPT_ENCOUNTER_RATE == 5 &&
               DW3_PORT_OPT_TEXT_SPEED == 6 && DW3_PORT_OPT_BATTLE_MENU == 7 &&
               DW3_PORT_OPT_AUTOSAVE == 8 &&
               DW3_PORT_OPT_BUTTON_LAYOUT == 9 && DW3_PORT_OPT_REGION == 10 &&
               DW3_PORT_OPT_CUSTOMIZE == 11 && DW3_PORT_OPT_RESET_MAP == 12 &&
               DW3_PORT_OPT_TURBO_MUSIC == 13 &&
               DW3_PORT_OPT_SWITCH_GLYPHS == 14 &&
               DW3_PORT_OPT_COUNT == 15,
               "registry indexes changed: update port_options_groups.h");

typedef struct Dw3PortOptStub {
    const char *label;
    const char *key; /* P9.4 option file key */
    int value; /* 0/1, 0/2..4 (Fast-forward), 0..10 (volumes), 0..4 (Encounter
                * rate) or 0..2 (Text speed, Battle cursor); Widescreen
                * defaults ON, turbo starts inactive, volumes default to 10,
                * pacing and Battle cursor to 0 (1x, Normal, Off) */
    int def;   /* value with no option file (the row's initial value) */
    const char *env; /* optional DW3_* variable that pins it for one run */
} Dw3PortOptStub;

static int g_open;
static void po_help_shutdown(void);
/* P9.7: 1 while the screen was opened from the field menu (not the title):
 * the rows that cannot change safely in game are locked (row_locked). */
static int g_in_game;
/* Where the in-game screen was opened from, for the log (P9.7: the field menu,
 * or the battle's command menu). */
static const char *g_where = "field menu";
static const char *g_back = "the field menu";
/* P9.9: the screen is a list of groups (level 0) or one group's rows (level 1).
 * Each level keeps its own cursor, so Back lands on the group that was open. */
static int g_level;
static int g_group;      /* the open group, at level 1 */
static int g_gcursor;    /* item of the group list */
static int g_gscroll;    /* first visible item of the group list */
static int g_cursor;     /* item of the open group's rows */
static int g_scroll;     /* first visible item of the rows */
static int g_open_frames;
static int g_logged_open;
static Dw3PortOptStub g_stubs[DW3_PORT_OPT_COUNT] = {
    {"Widescreen", "widescreen", 1, 1, NULL}, /* owner request: on */
    /* R3 toggles to this speed in field/battle; normal boot remains 1x. */
    {"Fast-forward", "fast_forward", DW3_TURBO_SPEED_DEFAULT,
     DW3_TURBO_SPEED_DEFAULT, NULL},
    {"Restart hotkey", "restart_hotkey", 0, 0, NULL},
    /* P6.17: 0..10, 10 = the original mix (docs/P6.17_VOLUME.md) */
    {"Music volume", "music_volume", DW3_AUDIO_VOLUME_MAX,
     DW3_AUDIO_VOLUME_MAX, "DW3_MUSIC_VOLUME"},
    {"SFX volume", "sfx_volume", DW3_AUDIO_VOLUME_MAX, DW3_AUDIO_VOLUME_MAX,
     "DW3_SFX_VOLUME"},
    /* P6.18: 0 = 1x, 1 = 0.5x, 2 = Off, 3 = 1.5x, 4 = 2x (stored values;
     * the screen steps OFF, 0.5x, 1x, 1.5x, 2x, docs/P6.18_PACING.md) */
    {"Encounter rate", "encounter_rate", DW3_ENCOUNTER_1X, DW3_ENCOUNTER_1X,
     "DW3_ENCOUNTER_RATE"},
    /* P6.18: 0 = Normal, 1 = Fast, 2 = Instant */
    {"Text speed", "text_speed", DW3_TEXT_NORMAL, DW3_TEXT_NORMAL,
     "DW3_TEXT_SPEED"},
    /* P7.9: 0 = Off (the original menu), 1 = Command, 2 = Command+Tech. The
     * key is the one that was file-only before the row existed. */
    {"Battle cursor", DW3_BATTLE_MENU_MEMORY_KEY, DW3_BATTLE_MENU_MEMORY_OFF,
     DW3_BATTLE_MENU_MEMORY_OFF, NULL},
    /* P6.31: 0 = OFF (the original game), 1 = On area change. Off by default;
     * a switch row (Confirm toggles). */
    {"Auto-save", DW3_AUTOSAVE_KEY, DW3_AUTOSAVE_OFF, DW3_AUTOSAVE_OFF, NULL},
    /* P9.8: 0 = PlayStation (the original buttons), 1 = Nintendo (A confirms,
     * B cancels). The file holds the words, button_layout=playstation|nintendo
     * (docs/CONTROLS_BUNDLE.md); DW3_BUTTON_MAP=default forces the first one
     * for a run. */
    {"Button layout", "button_layout", DW3_LAYOUT_PLAYSTATION,
     DW3_LAYOUT_PLAYSTATION, NULL},
    /* P51 item 4: 0 = Ask, 1 = US, 2 = EU; the combined image only lists it.
     * No key of this file: region_default lives in the top-level file next to
     * the region folders (po_region_load / po_region_save). */
    {"Region at start", NULL, 0, 0, NULL},
    /* P6.12: action rows, no value and no key. */
    {"Customize buttons", NULL, 0, 0, NULL},
    {"Reset to default", NULL, 0, 0, NULL},
    /* P6.36: 0 = 1x (the music keeps its tempo in turbo), 1 = Fast (today).
     * DW3_TURBO_MUSIC pins it for a run (stored values). */
    {"Turbo music", "turbo_music", 0, 0, "DW3_TURBO_MUSIC"},
    /* P10.6: 0 = the game's button icons, 1 = Switch glyphs (a switch row;
     * Confirm toggles). DW3_SWITCH_GLYPHS pins it for a run. */
    {"Switch glyphs", "switch_glyphs", 0, 0, "DW3_SWITCH_GLYPHS"},
};

/* P9.4 persistence. The registry values above are what the game reads; this
 * is the option file behind them. dw3_port_options_persist_init() fills the
 * values from the file once at boot (before any host reads a flag), and every
 * change made on the Options screen is written back at once, so a HOME exit,
 * a kill or a crash cannot lose it and no applet hook is needed. */
static int g_persist_on;
static char g_cfg_dir[DW3_OPTCFG_PATH_MAX];
static Dw3OptCfg g_cfg;          /* the loaded file; keeps unknown keys */
static int g_saved[DW3_PORT_OPT_COUNT]; /* what the file holds */
static unsigned g_env_pinned;    /* values forced by DW3_WIDESCREEN/DW3_TURBO */
static int g_dirty;              /* a write failed; retried by _flush */
/* P6.12: the map of button_layout=custom as read from the file. */
static unsigned char g_custom_map[DW3_BTN_COUNT] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
/* P6.12: the PS button the Customize screen is waiting for a press for (-1 =
 * none) and the frames the prompt has been up (5 s at 60 fps). */
static int g_assign_btn = -1;
static int g_assign_frames;
enum { PO_ASSIGN_TIMEOUT = 300 };

#ifdef DW3_PORT_OPTIONS_TEST
/* Button bits match engine/pad.h. */
enum {
    PO_BIT_SELECT = 0,
    PO_BIT_UP = 4,
    PO_BIT_RIGHT = 5,
    PO_BIT_DOWN = 6,
    PO_BIT_LEFT = 7,
    PO_BIT_CIRCLE = 12,
    PO_BIT_CROSS = 13,
    PO_BIT_TRIANGLE = 14
};
static unsigned g_test_pressed;
void dw3_port_options_test_set_pressed(unsigned mask) {
    g_test_pressed = mask;
}
static int po_pressed(int bit) {
    return (int)((g_test_pressed >> bit) & 1u);
}
/* Host tests: put the cursor on registry row `reg` (its group opened) without
 * the keys, the sounds or the log lines. */
void dw3_port_options_test_goto_row(int reg) {
    int g, k;

    for (g = 0; g < DW3_OPTGRP_COUNT; g++) {
        for (k = 0; k < dw3_optgroups[g].nrows; k++) {
            if (g != DW3_OPTGRP_CHEATS && dw3_optgroups[g].rows[k] == reg) {
                g_level = 1;
                g_group = g;
                g_cursor = k;
                g_scroll = 0;
                return;
            }
        }
    }
}
#else
static int po_pressed(int bit) {
    return PAD_PRESSED(bit);
}
#endif

/* The groups' words, for the log. */
static const char *const g_group_names[DW3_OPTGRP_COUNT] = {
    "display", "sound", "gameplay", "saves", "cheats", "controls", "help",
    "walkthrough", "customize"};

static int po_cheats_on(void) {
#ifdef DW3_CHEATS
    return dw3_cheats_enabled();
#else
    return 0;
#endif
}

/* The group at item `item` of the group list. */
static int po_group_at(int item) {
    int ids[DW3_OPTGRP_COUNT];
    int n = dw3_optgroups_visible(DW3_OPTGRP_CHEATS_LISTED, ids);

    return item >= 0 && item < n ? ids[item] : -1;
}

/* Keep the cursors inside their lists (the Cheats group can come and go with
 * its master switch) and the cursor's item on screen. */
static void po_fit_cursors(void) {
    int n = dw3_optlist_count(-1, DW3_OPTGRP_CHEATS_LISTED);

    if (g_gcursor >= n) {
        g_gcursor = n - 1;
    }
    if (g_gcursor < 0) {
        g_gcursor = 0;
    }
    g_gscroll = dw3_optlist_scroll_for(g_gscroll, g_gcursor, n);
    if (g_level) {
        n = dw3_optlist_count(g_group, DW3_OPTGRP_CHEATS_LISTED);
        if (g_cursor >= n) {
            g_cursor = n - 1;
        }
        if (g_cursor < 0) {
            g_cursor = 0;
        }
        g_scroll = dw3_optlist_scroll_for(g_scroll, g_cursor, n);
    }
}

/* Registry row under the cursor of the open group (level 1). */
static int po_cursor_row(void) {
    return dw3_optgroups[g_group].rows[g_cursor];
}

/* Hand the page, cursor and values to the native overlay, which regenerates
 * its options screen only when they change. */
static void po_push_view(void) {
    po_fit_cursors();
#if !defined(DW3_PORT_OPTIONS_TEST) && defined(DW3_TITLE_NATIVE_OVERLAY)
    int values[DW3_TITLE_OPTION_ROWS];
    int i;

    for (i = 0; i < DW3_TITLE_OPTION_ROWS; i++) {
        values[i] = 0;
    }
    if (g_level && g_group == DW3_OPTGRP_CUSTOM) {
        /* P6.12: a row's value is the source that makes the PS button; 10 =
         * the row is waiting for a press. */
        for (i = 0; i < DW3_BTN_COUNT && i < DW3_TITLE_OPTION_ROWS; i++) {
            values[i] = dw3_button_map_current()[i];
        }
        if (g_assign_btn >= 0) {
            values[g_assign_btn] = 10;
        }
    } else if (g_level) {
        for (i = 0; i < dw3_optgroups[g_group].nrows; i++) {
            values[i] = g_stubs[dw3_optgroups[g_group].rows[i]].value;
        }
    }
    dw3_pc_title_overlay_set_options_page(
        dw3_optlist_page(g_level ? g_group : -1,
                         g_level ? g_scroll : g_gscroll), po_cheats_on());
    dw3_pc_title_overlay_set_options_view(g_level ? g_cursor : g_gcursor,
                                          values, DW3_TITLE_OPTION_ROWS);
    dw3_pc_title_overlay_set_options_locked(dw3_port_options_locked_mask());
#endif
}

/* P9.7: the rows that change nothing in game. Widescreen: the gutter, the
 * field's map streamer layout (15 slots at 16:9) and the battle camera are
 * set up when a mode loads, so switching in the middle of a map would leave
 * them half old, half new; it is changed at the title. Everything else reads
 * its value where it is used (docs/PORT_OPTIONS.md "Options in game"). */
int dw3_port_options_row_locked(int index) {
    return g_in_game && (index == DW3_PORT_OPT_WIDESCREEN ||
                         index == DW3_PORT_OPT_CUSTOMIZE);
}

/* Bit i = row i is locked; DW3_PORT_OPTIONS_LOCK_CHEATS = the Cheats entry
 * (Square / Y), which is title-only: cheats are armed at the title. */
unsigned dw3_port_options_locked_mask_for(int in_game) {
    if (!in_game) {
        return 0;
    }
    /* The same rows dw3_port_options_row_locked() reports in game. */
    return (1u << DW3_PORT_OPT_WIDESCREEN) | (1u << DW3_PORT_OPT_CUSTOMIZE) |
           DW3_PORT_OPTIONS_LOCK_CHEATS;
}

unsigned dw3_port_options_locked_mask(void) {
    return dw3_port_options_locked_mask_for(g_in_game);
}

int dw3_port_options_in_game(void) {
    return g_in_game != 0;
}

int dw3_port_options_group(void) {
    return g_level ? g_group : -1;
}

int dw3_port_options_cursor(void) {
    return g_level ? g_cursor : g_gcursor;
}

/* Open-to-screen timing (the owner felt a stutter opening Options in game):
 * the open request, the first frame the host drew it, and the longest gap
 * between two ticks in the first PO_WATCH_FRAMES frames. */
enum { PO_WATCH_FRAMES = 90 };
static double g_open_ms, g_last_tick_ms, g_max_gap_ms;
static int g_max_gap_frame, g_shown_logged, g_watch_logged;

static double po_now_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

static void po_watch_report(const char *why) {
    if (g_watch_logged) {
        return;
    }
    g_watch_logged = 1;
    fprintf(stderr, "dw3-port-options: longest frame gap %.1f ms (frame %d of "
            "the first %d after open; %s)\n", g_max_gap_ms, g_max_gap_frame,
            g_open_frames, why);
}

void dw3_port_options_note_shown(void) {
    if (!g_open || g_shown_logged) {
        return;
    }
    g_shown_logged = 1;
    fprintf(stderr, "dw3-port-options: first frame shown %.1f ms after the open "
            "request (%s, tick %d)\n", po_now_ms() - g_open_ms,
            g_in_game ? "in game" : "title", g_open_frames);
}

static void po_open(int in_game) {
    if (g_open) {
        return;
    }
    g_open = 1;
    g_open_ms = g_last_tick_ms = po_now_ms();
    g_max_gap_ms = 0.0;
    g_max_gap_frame = 0;
    g_shown_logged = g_watch_logged = 0;
    g_in_game = in_game;
    g_level = 0;
    g_group = 0;
    g_gcursor = g_gscroll = g_cursor = g_scroll = 0;
    g_assign_btn = -1;
    g_open_frames = 0;
    g_logged_open = 0;
    po_push_view();
    if (in_game) {
        fprintf(stderr, "dw3-port-options: open in game (%s; "
                        "widescreen and cheats locked)\n", g_where);
    } else {
        fprintf(stderr, "dw3-port-options: open (stay MODE_TITLE)\n");
    }
    {
        /* P51 item 5: the Cheats group is always on the list; say what the
         * owner should see, once per open. */
        int ids[DW3_OPTGRP_COUNT];
        int n = dw3_optgroups_visible(DW3_OPTGRP_CHEATS_LISTED, ids);

        fprintf(stderr, "dw3-port-options: group list n=%d (cheats %s, master %s, "
                        "locked in game %d)\n", n,
                DW3_OPTGRP_CHEATS_LISTED ? "listed" : "not built",
                po_cheats_on() ? "on" : "off", in_game ? 1 : 0);
    }
    fflush(stderr);
}

void dw3_port_options_open(void) {
    po_open(0);
}

void dw3_port_options_set_origin(const char *where, const char *back) {
    g_where = where != NULL ? where : "field menu";
    g_back = back != NULL ? back : "the field menu";
}

/* P9.7: from the field menu's Options row. */
void dw3_port_options_open_in_game(void) {
    po_open(1);
}

void dw3_port_options_close(void) {
    if (!g_open) {
        return;
    }
    po_watch_report("closed");
    po_help_shutdown();
    fprintf(stderr, "dw3-port-options: open for %.0f ms\n",
            po_now_ms() - g_open_ms);
    g_open = 0;
    g_open_frames = 0;
    if (g_in_game) {
        g_in_game = 0;
        fprintf(stderr, "dw3-port-options: close in game (back to %s)\n", g_back);
    } else {
        fprintf(stderr, "dw3-port-options: close (resume runOptions)\n");
    }
    fflush(stderr);
    dw3_port_options_flush();
}

int dw3_port_options_is_open(void) {
    return g_open != 0;
}

int dw3_port_options_count(void) {
    return DW3_PORT_OPT_COUNT;
}

const char *dw3_port_options_label(int index) {
    if (index < 0 || index >= DW3_PORT_OPT_COUNT) {
        return NULL;
    }
    return g_stubs[index].label;
}

int dw3_port_options_value(int index) {
    if (index < 0 || index >= DW3_PORT_OPT_COUNT) {
        return 0;
    }
    return g_stubs[index].value != 0;
}

void dw3_port_options_set(int index, int value) {
    if (index < 0 || index >= DW3_PORT_OPT_COUNT) {
        return;
    }
    g_stubs[index].value = value != 0;
}

static int po_is_volume(int index);
static void po_audio_apply(void);
static int po_valid_layout(int v);
static void po_layout_boot(int from_cfg);
static void po_glyphs_boot(void);

static int po_valid_layout(int v) {
    return v >= 0 && v < DW3_LAYOUT_COUNT;
}

int dw3_port_options_level(int index) {
    if (index < 0 || index >= DW3_PORT_OPT_COUNT) {
        return 0;
    }
    return g_stubs[index].value;
}

void dw3_port_options_set_level(int index, int level) {
    if (index < 0 || index >= DW3_PORT_OPT_COUNT) {
        return;
    }
    g_stubs[index].value = level;
    if (po_is_volume(index)) {
        po_audio_apply();
    }
    if (index == DW3_PORT_OPT_BUTTON_LAYOUT && po_valid_layout(level)) {
        dw3_button_map_change(level, "api");
    }
}


static int po_is_volume(int index) {
    return index == DW3_PORT_OPT_MUSIC_VOLUME ||
           index == DW3_PORT_OPT_SFX_VOLUME;
}

/* The P6.18 rows, the Battle cursor and the Button layout hold a mode, 0 = the
 * original behaviour. */
static int po_is_mode(int index) {
    return index == DW3_PORT_OPT_ENCOUNTER_RATE ||
           index == DW3_PORT_OPT_TEXT_SPEED ||
           index == DW3_PORT_OPT_BATTLE_MENU ||
           index == DW3_PORT_OPT_BUTTON_LAYOUT ||
           index == DW3_PORT_OPT_REGION ||
           index == DW3_PORT_OPT_TURBO_MUSIC;
}

/* Rows that Left / Right step and Confirm cycles: the volumes and the modes. */
static int po_is_stepped(int index) {
    return po_is_volume(index) || po_is_mode(index);
}

/* Highest stored value of a stepped row. */
static int po_value_max(int index) {
    return po_is_volume(index) ? DW3_AUDIO_VOLUME_MAX
           : index == DW3_PORT_OPT_ENCOUNTER_RATE ? DW3_ENCOUNTER_MODE_MAX
           : index == DW3_PORT_OPT_BATTLE_MENU ? DW3_BATTLE_MENU_MEMORY_TECH
           : index == DW3_PORT_OPT_BUTTON_LAYOUT ? DW3_LAYOUT_COUNT - 1
           : index == DW3_PORT_OPT_REGION ? 2
           : index == DW3_PORT_OPT_TURBO_MUSIC ? 1
                                                   : DW3_PACING_MODE_MAX;
}

/* The order Left / Right and Confirm walk a stepped row in, by position. The
 * stored values never change (a file written by an older build keeps its
 * meaning), so Encounter rate, stored 0 1x / 1 0.5x / 2 Off / 3 1.5x / 2x 4,
 * is stepped OFF, 0.5x, 1x, 1.5x, 2x. Every other row steps in value order. */
static const int po_encounter_order[DW3_ENCOUNTER_MODE_MAX + 1] = {
    DW3_ENCOUNTER_OFF, DW3_ENCOUNTER_HALF, DW3_ENCOUNTER_1X,
    DW3_ENCOUNTER_1_5X, DW3_ENCOUNTER_2X
};

static int po_value_at(int index, int pos) {
    return index == DW3_PORT_OPT_ENCOUNTER_RATE ? po_encounter_order[pos] : pos;
}

static int po_pos_of(int index, int value) {
    int pos;

    if (index != DW3_PORT_OPT_ENCOUNTER_RATE) {
        return value;
    }
    for (pos = 0; pos <= DW3_ENCOUNTER_MODE_MAX; pos++) {
        if (po_encounter_order[pos] == value) {
            return pos;
        }
    }
    return 0;
}

/* Last position of a stepped row. */
static int po_step_max(int index) {
    return po_value_max(index);
}

/* A value the row can hold: ON/OFF, OFF / 2x..4x for Fast-forward, a volume
 * step 0..10, an Encounter rate 0..4, a Text speed 0..2 or a Battle cursor
 * 0..2. */
static int po_valid(int index, int v) {
    if (index == DW3_PORT_OPT_FAST_FORWARD) {
        return v == 0 || (v >= DW3_TURBO_SPEED_MIN && v <= DW3_TURBO_SPEED_MAX);
    }
    if (po_is_stepped(index)) {
        return v >= 0 && v <= po_value_max(index);
    }
    return v == 0 || v == 1;
}

#ifdef DW3_PORT_OPTIONS_TEST
/* Host tests link no mixer; a test that wants the values defines its own. */
__attribute__((weak)) void dw3_port_audio_apply(int music_step, int sfx_step) {
    (void)music_step;
    (void)sfx_step;
}
#endif

/* Hand the two volume steps to the mixer (port/audio/audio_bus.c). */
static void po_audio_apply(void) {
    dw3_port_audio_apply(g_stubs[DW3_PORT_OPT_MUSIC_VOLUME].value,
                         g_stubs[DW3_PORT_OPT_SFX_VOLUME].value);
}

/* DW3_MUSIC_VOLUME / DW3_SFX_VOLUME (0..10), DW3_ENCOUNTER_RATE (0..4, the
 * stored values) and DW3_TEXT_SPEED (0..2) pin a value for this run: it is used like any other
 * value but never written by an unrelated change (the same rule as
 * DW3_WIDESCREEN and DW3_TURBO). An invalid value is ignored. */
static void po_stepped_env(void) {
    int row;

    for (row = 0; row < DW3_PORT_OPT_COUNT; row++) {
        const char *env;
        int n;

        if (g_stubs[row].env == NULL) {
            continue;
        }
        env = getenv(g_stubs[row].env);
        if (env == NULL || dw3_optcfg_parse_int(env, &n) != 0 ||
            !po_valid(row, n)) {
            continue;
        }
        g_stubs[row].value = n;
        g_env_pinned |= 1u << row;
    }
}

/* Boot: the layout in force becomes the map the pad host reads, and the log
 * says so (`dw3-controls: map loaded ...`). DW3_BUTTON_MAP=default forces the
 * PlayStation layout for this run only, like DW3_TURBO: it is never written
 * (the row stays pinned until it is changed on the screen). */
static void po_layout_boot(int from_cfg) {
    const char *env = getenv("DW3_BUTTON_MAP");

    if (env != NULL && strcmp(env, "default") == 0) {
        g_stubs[DW3_PORT_OPT_BUTTON_LAYOUT].value = DW3_LAYOUT_PLAYSTATION;
        g_env_pinned |= 1u << DW3_PORT_OPT_BUTTON_LAYOUT;
        dw3_button_map_load(DW3_LAYOUT_PLAYSTATION, "env");
        return;
    }
    if (from_cfg && g_stubs[DW3_PORT_OPT_BUTTON_LAYOUT].value == DW3_LAYOUT_CUSTOM) {
        dw3_button_map_load_custom(g_custom_map, "cfg");
        return;
    }
    dw3_button_map_load(g_stubs[DW3_PORT_OPT_BUTTON_LAYOUT].value,
                        from_cfg ? "cfg" : "default");
}

/* P10.6: wrap FONT.load now that the option is known, and say which way this
 * run goes (the font itself loads a little later, dw3-glyphs: lines). */
static void po_glyphs_boot(void) {
    PO_INFO("dw3-options: switch_glyphs=%s%s (docs/P10.6_GLYPHS.md)",
            g_stubs[DW3_PORT_OPT_SWITCH_GLYPHS].value ? "on" : "off",
            (g_env_pinned >> DW3_PORT_OPT_SWITCH_GLYPHS) & 1u ? " [env]" : "");
    dw3_glyphs_init();
}

void dw3_port_options_apply_env(int index, int level) {
    if (index < 0 || index >= DW3_PORT_OPT_COUNT) {
        return;
    }
    g_stubs[index].value = level;
    g_env_pinned |= 1u << index;
    if (po_is_volume(index)) {
        po_audio_apply();
    }
}

/* P51 item 4: the Region at start row (combined image). The key is
 * region_default of the TOP-LEVEL port_options.cfg (`ask`, `us`, `eu`), the
 * file the region boot reads (port/region/dw3_region.h); the region folder's
 * own file is never touched by this row. The folder is the parent of the
 * region's config dir (sdmc:/switch/dw3 beside us/ and eu/), or DW3_CONFIG_DIR
 * when that is set (the PC replays keep both files in one folder). */
#if DW3_OPT_REGION_ROW
static char g_region_base[DW3_OPTCFG_PATH_MAX];

static const char *po_region_base(void) {
    const char *env = getenv("DW3_CONFIG_DIR");
    char *slash;
    size_t n;

    if (env != NULL && env[0] != '\0' && strlen(env) < sizeof(g_region_base)) {
        strcpy(g_region_base, env);
        return g_region_base;
    }
    if (!g_persist_on || g_cfg_dir[0] == '\0') {
        return NULL;
    }
    strcpy(g_region_base, g_cfg_dir);
    n = strlen(g_region_base);
    while (n > 1 && g_region_base[n - 1] == '/') {
        g_region_base[--n] = '\0';
    }
    slash = strrchr(g_region_base, '/');
    if (slash == NULL || slash == g_region_base) {
        return NULL;
    }
    *slash = '\0';
    return g_region_base;
}

static void po_region_load(void) {
    const char *base = po_region_base();
    Dw3Region pref = DW3_REGION_NONE;
    Dw3Region last = DW3_REGION_NONE;

    g_stubs[DW3_PORT_OPT_REGION].value = 0;
    if (base == NULL) {
        return;
    }
    dw3_region_cfg_read(base, &pref, &last);
    g_stubs[DW3_PORT_OPT_REGION].value =
        pref == DW3_REGION_US ? 1 : pref == DW3_REGION_EU ? 2 : 0;
    PO_INFO("dw3-options: region_default %s read from %s/%s",
            pref == DW3_REGION_NONE ? "ask" : dw3_region_name(pref), base,
            DW3_OPTCFG_FILE);
}

static void po_region_save(int v) {
    const char *base = po_region_base();
    Dw3Region pref = v == 1 ? DW3_REGION_US : v == 2 ? DW3_REGION_EU
                                                      : DW3_REGION_NONE;

    if (base == NULL) {
        PO_WARN("dw3-options: region_default not saved: no top-level folder "
                "(options are session-only)");
        return;
    }
    if (dw3_region_cfg_write(base, DW3_REGION_NONE, 1, pref) != 0) {
        PO_WARN("dw3-options: region_default %s NOT saved to %s/%s "
                "(cannot read or write it)",
                pref == DW3_REGION_NONE ? "ask" : dw3_region_name(pref), base,
                DW3_OPTCFG_FILE);
        return;
    }
    PO_INFO("dw3-options: region_default %s saved to %s/%s (takes effect at "
            "the next start)",
            pref == DW3_REGION_NONE ? "ask" : dw3_region_name(pref), base,
            DW3_OPTCFG_FILE);
}
#else
static void po_region_load(void) {}
static void po_region_save(int v) { (void)v; }
#endif

/* Write every option to the file. An option still forced by the environment
 * keeps the value the file had, so `DW3_WIDESCREEN=0 dw3-pc` for one run is
 * not saved as the user's choice by an unrelated toggle. */
static int po_save(void) {
    int out[DW3_PORT_OPT_COUNT];
    char num[16];
    int i;

    if (!g_persist_on) {
        return 0;
    }
    for (i = 0; i < DW3_PORT_OPT_COUNT; i++) {
        out[i] = (g_env_pinned >> i) & 1u ? g_saved[i] : g_stubs[i].value;
        if (g_stubs[i].key == NULL) {
            continue; /* the Region at start row: the top-level file */
        }
        if (i == DW3_PORT_OPT_BUTTON_LAYOUT) {
            snprintf(num, sizeof(num), "%s", dw3_button_layout_name(out[i]));
        } else {
            snprintf(num, sizeof(num), "%d", out[i]);
        }
        if (dw3_optcfg_set(&g_cfg, g_stubs[i].key, num) != 0) {
            PO_WARN("dw3-options: cannot stage %s", g_stubs[i].key);
        }
    }
    /* P6.12: button_map only exists while the layout is custom. A layout the
     * environment pinned leaves the file's map alone. */
    if (!((g_env_pinned >> DW3_PORT_OPT_BUTTON_LAYOUT) & 1u)) {
        if (out[DW3_PORT_OPT_BUTTON_LAYOUT] == DW3_LAYOUT_CUSTOM) {
            char digits[DW3_BTN_COUNT + 1];

            dw3_button_map_format(dw3_button_map_current(), digits);
            if (dw3_optcfg_set(&g_cfg, "button_map", digits) != 0) {
                PO_WARN("dw3-options: cannot stage button_map");
            }
        } else {
            (void)dw3_optcfg_remove(&g_cfg, "button_map");
        }
    }
    if (dw3_optcfg_save(&g_cfg, g_cfg_dir) != 0) {
        g_dirty = 1;
        PO_WARN("dw3-options: save to %s failed (will retry on close/exit)",
                g_cfg_dir);
        return -1;
    }
    g_dirty = 0;
    for (i = 0; i < DW3_PORT_OPT_COUNT; i++) {
        g_saved[i] = out[i];
    }
    PO_INFO("dw3-options: saved %s/%s (widescreen=%d fast_forward=%d "
            "restart_hotkey=%d music_volume=%d sfx_volume=%d "
            "encounter_rate=%d text_speed=%d battle_menu_memory=%d)",
            g_cfg_dir, DW3_OPTCFG_FILE, g_saved[DW3_PORT_OPT_WIDESCREEN],
            g_saved[DW3_PORT_OPT_FAST_FORWARD], g_saved[DW3_PORT_OPT_RESTART],
            g_saved[DW3_PORT_OPT_MUSIC_VOLUME],
            g_saved[DW3_PORT_OPT_SFX_VOLUME],
            g_saved[DW3_PORT_OPT_ENCOUNTER_RATE],
            g_saved[DW3_PORT_OPT_TEXT_SPEED],
            g_saved[DW3_PORT_OPT_BATTLE_MENU]);
    return 0;
}

int dw3_port_options_file_int(const char *key, int def) {
    const char *v = g_persist_on ? dw3_optcfg_get(&g_cfg, key) : NULL;
    int n;

    if (v == NULL || dw3_optcfg_parse_int(v, &n) != 0) {
        return def;
    }
    return n;
}

void dw3_port_options_flush(void) {
    if (g_persist_on && g_dirty) {
        (void)po_save();
    }
}

const char *dw3_port_options_cfg_dir(void) {
    return g_persist_on ? g_cfg_dir : "";
}

int dw3_port_options_persist_init(const char *dir) {
    char path[DW3_OPTCFG_PATH_MAX];
    int rc;
    int i;
    int bad = 0;
    int layout_from_cfg = 0;

    g_persist_on = 0;
    g_dirty = 0;
    g_env_pinned = 0;
    if (dir == NULL || dir[0] == '\0' ||
        strlen(dir) >= sizeof(g_cfg_dir) ||
        dw3_optcfg_path(dir, "", path, sizeof(path)) != 0) {
        PO_WARN("dw3-options: no usable config dir; options are session-only");
        po_stepped_env();
        po_audio_apply();
        po_layout_boot(0);
        po_glyphs_boot();
        return -1;
    }
    strcpy(g_cfg_dir, dir);
    rc = dw3_optcfg_load(&g_cfg, dir);
    g_persist_on = 1;
    for (i = 0; i < DW3_PORT_OPT_COUNT; i++) {
        g_stubs[i].value = g_stubs[i].def;
    }
    if (rc == DW3_OPTCFG_LOAD_ERROR) {
        PO_WARN("dw3-options: cannot read %s; using defaults, the next "
                "change will replace it", path);
    } else if (rc == DW3_OPTCFG_LOAD_MISSING) {
        PO_INFO("dw3-options: no %s; defaults", path);
    } else {
        const char *v;
        int n;

        if (rc == DW3_OPTCFG_LOAD_RECOVERED) {
            PO_WARN("dw3-options: %s was missing; recovered %s.tmp", path,
                    path);
        }
        if (g_cfg.version > DW3_OPTCFG_VERSION) {
            PO_WARN("dw3-options: %s is version %d (newer than %d); reading "
                    "known keys, keeping the rest", path, g_cfg.version,
                    DW3_OPTCFG_VERSION);
        }
        if (g_cfg.skipped > 0) {
            PO_WARN("dw3-options: %s: ignored %d malformed line(s)", path,
                    g_cfg.skipped);
        }
        for (i = 0; i < DW3_PORT_OPT_COUNT; i++) {
            if (g_stubs[i].key == NULL) {
                continue;
            }
            v = dw3_optcfg_get(&g_cfg, g_stubs[i].key);
            if (v == NULL) {
                continue;
            }
            if (i == DW3_PORT_OPT_BUTTON_LAYOUT) {
                n = dw3_button_layout_parse(v);
                if (n < 0) {
                    PO_WARN("dw3-options: %s=%s is not valid; using default %s",
                            g_stubs[i].key, v,
                            dw3_button_layout_name(g_stubs[i].def));
                    dw3_button_map_log_invalid("button_layout is not "
                                               "playstation, nintendo or "
                                               "custom");
                    bad++;
                    continue;
                }
                if (n == DW3_LAYOUT_CUSTOM) {
                    const char *mv = dw3_optcfg_get(&g_cfg, "button_map");

                    if (mv == NULL) {
                        PO_WARN("dw3-options: button_layout=custom has no "
                                "button_map; using default");
                        dw3_button_map_log_invalid("button_layout is custom "
                                                   "but button_map is missing");
                        bad++;
                        continue;
                    }
                    if (dw3_button_map_parse(mv, g_custom_map) != 0) {
                        PO_WARN("dw3-options: button_map=%s is not valid; "
                                "using default", mv);
                        dw3_button_map_log_invalid("button_map is not a "
                                                   "permutation of 0-9");
                        bad++;
                        continue;
                    }
                }
                g_stubs[i].value = n;
                layout_from_cfg = 1;
                continue;
            }
            if (dw3_optcfg_parse_int(v, &n) != 0 || !po_valid(i, n)) {
                PO_WARN("dw3-options: %s=%s is not valid; using default %d",
                        g_stubs[i].key, v, g_stubs[i].def);
                bad++;
                continue;
            }
            g_stubs[i].value = n;
        }
    }
    po_region_load();
    for (i = 0; i < DW3_PORT_OPT_COUNT; i++) {
        g_saved[i] = g_stubs[i].value;
    }
    PO_INFO("dw3-options: %s (widescreen=%d fast_forward=%d "
            "restart_hotkey=%d music_volume=%d sfx_volume=%d "
            "encounter_rate=%d text_speed=%d battle_menu_memory=%d)%s", path,
            g_saved[DW3_PORT_OPT_WIDESCREEN],
            g_saved[DW3_PORT_OPT_FAST_FORWARD], g_saved[DW3_PORT_OPT_RESTART],
            g_saved[DW3_PORT_OPT_MUSIC_VOLUME],
            g_saved[DW3_PORT_OPT_SFX_VOLUME],
            g_saved[DW3_PORT_OPT_ENCOUNTER_RATE],
            g_saved[DW3_PORT_OPT_TEXT_SPEED],
            g_saved[DW3_PORT_OPT_BATTLE_MENU],
            bad ? " [some values reset]" : "");
    if (g_stubs[DW3_PORT_OPT_AUTOSAVE].value != DW3_AUTOSAVE_OFF) {
        PO_INFO("dw3-options: autosave=%d (on area change; docs/P6.31_AUTOSAVE.md)",
                g_stubs[DW3_PORT_OPT_AUTOSAVE].value);
    }
    /* File-only keys (not rows): their values at boot, so a log says which
     * way the test ran. */
    PO_INFO("dw3-options: file keys items_equip_from_items=%d items_equip_stays=%d items_bag_view=%d items_use_stays=%d items_partner_switch=%d "
            "map_screen=%d battle_menu_memory=%d",
            dw3_port_options_file_int("items_equip_from_items", 1),
            dw3_port_options_file_int("items_equip_stays", 1),
            dw3_port_options_file_int("items_bag_view", 1),
            dw3_port_options_file_int("items_use_stays", 1),
            dw3_port_options_file_int("items_partner_switch", 1),
            dw3_port_options_file_int("map_screen", 0), /* off by default (P6.32 phase 1) */
            g_saved[DW3_PORT_OPT_BATTLE_MENU]);
    /* DW3_MUSIC_VOLUME / DW3_SFX_VOLUME force a value for this run only. */
    po_stepped_env();
    po_audio_apply();
    PO_INFO("dw3-options: turbo_music=%s%s (docs/P6.36_TURBO_MUSIC.md)",
            g_stubs[DW3_PORT_OPT_TURBO_MUSIC].value ? "fast" : "1x",
            (g_env_pinned >> DW3_PORT_OPT_TURBO_MUSIC) & 1u ? " [env]" : "");
    po_layout_boot(layout_from_cfg);
    po_glyphs_boot();
    return rc == DW3_OPTCFG_LOAD_ERROR ? -1 : 0;
}

/* The words of a mode row, for the log (the overlay has its own lettering). */
static const char *po_mode_name(int index, int v) {
    static const char *const encounter[DW3_ENCOUNTER_MODE_MAX + 1] = {
        "1x", "0.5x", "off", "1.5x", "2x"};
    static const char *const text[3] = {"normal", "fast", "instant"};
    static const char *const battle[3] = {"off", "command",
                                          "command+technique"};
    static const char *const layout[DW3_LAYOUT_COUNT] = {"playstation",
                                                         "nintendo", "custom"};
    static const char *const region[3] = {"ask", "us", "eu"};
    static const char *const tmusic[2] = {"1x", "fast"};

    if (v < 0 || v > po_value_max(index)) {
        return "?";
    }
    return index == DW3_PORT_OPT_ENCOUNTER_RATE ? encounter[v]
           : index == DW3_PORT_OPT_BUTTON_LAYOUT ? layout[v]
           : index == DW3_PORT_OPT_REGION ? region[v]
           : index == DW3_PORT_OPT_TURBO_MUSIC ? tmusic[v]
           : index == DW3_PORT_OPT_BATTLE_MENU ? battle[v] : text[v];
}

/* Options screen menu sound feedback (owner request after the preview.30-rc4 OLED test): the game's own title-menu sounds (cursor tick,
 * choice, and the menu cancel of its other menus). All three are key-ons of
 * the system bank (bank 1, SYSTEM00..02), so they go through the SFX bus
 * (P6.17) and follow the SFX volume row. A host test counts them instead. */
enum { PO_SND_CURSOR, PO_SND_SELECT, PO_SND_CANCEL };

#ifdef DW3_PORT_OPTIONS_TEST
static int g_test_sounds[3];
int dw3_port_options_test_sound_count(int kind) {
    return kind >= 0 && kind < 3 ? g_test_sounds[kind] : 0;
}
#endif

static void po_sound(int kind) {
    static const char *const name[3] = {"cursor", "select", "cancel"};
#ifdef DW3_PORT_OPTIONS_TEST
    g_test_sounds[kind]++;
    fprintf(stderr, "dw3-port-options: sound %s (sfx volume %d)\n", name[kind],
            g_stubs[DW3_PORT_OPT_SFX_VOLUME].value);
    fflush(stderr);
#else
    static const s32 id[3] = {SOUND_CURSOR, SOUND_SELECT, SOUND_MENU_CANCEL};
    short voice;

    if (SOUND.playSound == NULL || findSoundBank(SOUND_BANK(id[kind])) < 0) {
        PO_WARN("dw3-port-options: sound %s not played: bank %d is not "
                "loaded", name[kind], (int)SOUND_BANK(id[kind]));
        return;
    }
    voice = SOUND.playSound(id[kind]);
    PO_INFO("dw3-port-options: sound %s (sfx volume %d, voice %d)",
            name[kind], g_stubs[DW3_PORT_OPT_SFX_VOLUME].value, (int)voice);
#endif
}

/* P9.7: a press on a row that is locked in game changes nothing; the screen
 * shows why (its message line). 1 = refused. */
static int po_refuse_locked(int index) {
    if (!dw3_port_options_row_locked(index)) {
        return 0;
    }
    fprintf(stderr, "dw3-port-options: %s is locked in game "
                    "(change it at the title)\n", g_stubs[index].label);
    fflush(stderr);
    return 1;
}

/* Move a stepped row (a volume 0..10, a pacing mode) by `delta` steps in
 * the row's order (po_value_at), clamped, and save. Plays the choice sound
 * once the new value (and, for a volume, its gain) is in effect. */
static void po_step(int index, int delta) {
    int pos = po_pos_of(index, g_stubs[index].value) + delta;
    int v;

    if (po_refuse_locked(index)) {
        return;
    }

    if (pos < 0) {
        pos = 0;
    }
    if (pos > po_step_max(index)) {
        pos = po_step_max(index);
    }
    v = po_value_at(index, pos);
    if (v == g_stubs[index].value && !((g_env_pinned >> index) & 1u)) {
        return;
    }
    g_stubs[index].value = v;
    g_env_pinned &= ~(1u << index);
    if (po_is_volume(index)) {
        fprintf(stderr, "dw3-port-options: %s %d\n",
                index == DW3_PORT_OPT_MUSIC_VOLUME ? "music volume"
                                                    : "sfx volume", v);
    } else {
        fprintf(stderr, "dw3-port-options: %s %s\n",
                index == DW3_PORT_OPT_ENCOUNTER_RATE ? "encounter rate"
                : index == DW3_PORT_OPT_BATTLE_MENU ? "battle cursor"
                : index == DW3_PORT_OPT_BUTTON_LAYOUT ? "button layout"
                : index == DW3_PORT_OPT_REGION ? "region at start"
                : index == DW3_PORT_OPT_TURBO_MUSIC ? "turbo music"
                                                    : "text speed",
                po_mode_name(index, v));
    }
    fflush(stderr);
    if (po_is_volume(index)) {
        po_audio_apply();
    }
    if (index == DW3_PORT_OPT_BUTTON_LAYOUT) {
        /* In force from the next pad poll (the pad host reads the map every
         * frame): the choice sound below is already played with it. */
        dw3_button_map_change(v, "screen");
    }
    if (index == DW3_PORT_OPT_REGION) {
        po_region_save(v);
    } else {
        (void)po_save();
    }
    po_sound(PO_SND_SELECT);
}

/* --- P6.12 Customize buttons --------------------------------------------
 * The Controls group's Customize row opens a list of the ten remappable PS
 * buttons, each showing the physical button that produces it. Confirm on a row
 * waits for a press (the source word before the map, button_map.c), Left /
 * Right pick a source without one. The map stays a permutation: the PS button
 * that had the new source gets the old one. Start / Select cancel the prompt
 * and the d-pad is ignored; the prompt also gives up after five seconds. */
static void po_custom_open(void) {
    g_level = 1;
    g_group = DW3_OPTGRP_CUSTOM;
    g_cursor = g_scroll = 0;
    g_assign_btn = -1;
    fprintf(stderr, "dw3-port-options: group open customize (%d row(s), %s)\n",
            dw3_optgroups[DW3_OPTGRP_CUSTOM].nrows,
            g_in_game ? "in game" : "title");
    fflush(stderr);
    po_sound(PO_SND_SELECT);
}

static void po_custom_back(void) {
    fprintf(stderr, "dw3-port-options: group close customize (back to "
                    "controls)\n");
    fflush(stderr);
    g_assign_btn = -1;
    g_group = DW3_OPTGRP_CONTROLS;
    g_cursor = 1; /* the Customize row */
    g_scroll = 0;
}

/* The map changed from this screen: the Button layout row reads Custom (or
 * PlayStation after a reset) and the file is written. */
static void po_map_changed(int layout) {
    g_stubs[DW3_PORT_OPT_BUTTON_LAYOUT].value = layout;
    g_env_pinned &= ~(1u << DW3_PORT_OPT_BUTTON_LAYOUT);
    (void)po_save();
}

static void po_assign(int button, int source) {
    const int before = dw3_button_map_current()[button];

    if (dw3_button_map_assign(button, source) < 0 || before == source) {
        return;
    }
    po_map_changed(DW3_LAYOUT_CUSTOM);
    po_sound(PO_SND_SELECT);
}

static void po_assign_begin(int button) {
    g_assign_btn = button;
    g_assign_frames = 0;
    dw3_button_map_capture_begin();
    fprintf(stderr, "dw3-port-options: customize waiting for a button for %s\n",
            dw3_button_ps_label(button));
    fflush(stderr);
    po_sound(PO_SND_SELECT);
}

/* One frame of the prompt: a press assigns, Start / Select or the timeout
 * cancel. */
static void po_assign_tick(void) {
    const int btn = g_assign_btn;
    const int r = dw3_button_map_capture_poll();

    g_assign_frames++;
    if (r >= 0) {
        g_assign_btn = -1;
        po_assign(btn, r);
    } else if (r == DW3_CAPTURE_CANCEL || g_assign_frames > PO_ASSIGN_TIMEOUT) {
        g_assign_btn = -1;
        fprintf(stderr, "dw3-port-options: customize %s cancelled (%s)\n",
                dw3_button_ps_label(btn),
                r == DW3_CAPTURE_CANCEL ? "Start / Select" : "timeout");
        fflush(stderr);
        po_sound(PO_SND_CANCEL);
    }
}

static void po_reset_map(void) {
    dw3_button_map_reset("screen");
    po_map_changed(DW3_LAYOUT_PLAYSTATION);
    po_sound(PO_SND_SELECT);
}

/* Confirm on a row: ON/OFF, OFF -> 2x -> 3x -> 4x -> OFF for Fast-forward, or
 * the next step of a volume / pacing row (wraps to the first after the last:
 * Encounter rate 2x -> OFF). */
static void po_toggle(int index) {
    int v;

    if (po_refuse_locked(index)) {
        return;
    }
    if (index == DW3_PORT_OPT_CUSTOMIZE) {
        po_custom_open();
        return;
    }
    if (index == DW3_PORT_OPT_RESET_MAP) {
        po_reset_map();
        return;
    }

    if (index == DW3_PORT_OPT_FAST_FORWARD) {
        v = g_stubs[index].value;
        v = v < DW3_TURBO_SPEED_MIN ? DW3_TURBO_SPEED_MIN : v + 1;
        if (v > DW3_TURBO_SPEED_MAX) {
            v = 0;
        }
        g_stubs[index].value = v;
        g_env_pinned &= ~(1u << index);
        if (v) {
            fprintf(stderr, "dw3-port-options: fast-forward %dx (R3)\n", v);
        } else {
            fprintf(stderr, "dw3-port-options: fast-forward off\n");
        }
        fflush(stderr);
        (void)po_save();
        po_sound(PO_SND_SELECT);
        return;
    }
    if (po_is_stepped(index)) {
        /* Confirm steps up and wraps; Left/Right step either way. */
        po_step(index, po_pos_of(index, g_stubs[index].value) >=
                               po_step_max(index)
                           ? -po_step_max(index) : 1);
        return;
    }
    v = g_stubs[index].value ? 0 : 1;
    g_stubs[index].value = v;
    g_env_pinned &= ~(1u << index);
    fprintf(stderr, "dw3-port-options: %s %s\n",
            index == DW3_PORT_OPT_RESTART ? "restart hotkey"
            : index == DW3_PORT_OPT_AUTOSAVE ? "auto-save"
            : index == DW3_PORT_OPT_SWITCH_GLYPHS ? "switch glyphs"
                                                   : "widescreen",
            v ? "enabled" : "disabled");
    fflush(stderr);
    (void)po_save();
    if (index == DW3_PORT_OPT_SWITCH_GLYPHS) {
        dw3_glyphs_refresh("option");
    }
    po_sound(PO_SND_SELECT);
}

int dw3_port_options_restart_enabled(void) {
    return g_stubs[DW3_PORT_OPT_RESTART].value != 0;
}

#ifdef DW3_PORT_OPTIONS_TEST
#define po_key_back() po_pressed(PO_BIT_TRIANGLE)
#define po_key_up() po_pressed(PO_BIT_UP)
#define po_key_down() po_pressed(PO_BIT_DOWN)
#define po_key_left() po_pressed(PO_BIT_LEFT)
#define po_key_right() po_pressed(PO_BIT_RIGHT)
#define po_key_confirm() (po_pressed(PO_BIT_CROSS) || po_pressed(PO_BIT_CIRCLE))
#define po_key_square() po_pressed(15) /* PAD_SQUARE */
#define po_key_prev() po_pressed(10)   /* PAD_L1 */
#define po_key_next() po_pressed(11)   /* PAD_R1 */
#else
#define po_key_back() po_pressed(PAD_TRIANGLE)
#define po_key_up() (po_pressed(PAD_UP) || PAD_REPEATED(PAD_UP))
#define po_key_down() (po_pressed(PAD_DOWN) || PAD_REPEATED(PAD_DOWN))
#define po_key_left() (po_pressed(PAD_LEFT) || PAD_REPEATED(PAD_LEFT))
#define po_key_right() (po_pressed(PAD_RIGHT) || PAD_REPEATED(PAD_RIGHT))
#define po_key_confirm() (po_pressed(PAD_CROSS) || po_pressed(PAD_CIRCLE))
#define po_key_square() po_pressed(PAD_SQUARE)
#define po_key_prev() po_pressed(PAD_L1)
#define po_key_next() po_pressed(PAD_R1)
#endif

static void po_custom_tick(void) {
    const int btn = g_cursor;

    if (po_key_left() || po_key_right()) {
        const int src = dw3_button_map_current()[btn];

        po_assign(btn, (src + (po_key_right() ? 1 : DW3_BTN_COUNT - 1)) %
                           DW3_BTN_COUNT);
    }
    if (po_key_confirm()) {
        po_assign_begin(btn);
    }
}

/* --- P6.30 technical help ------------------------------------------------
 * The Help group is a screen of its own (port/help/help.c), like Cheats: while
 * it is open it has the pad, and the native overlay shows its layer instead of
 * the Options layer. A host test build only tracks that it is open (the real
 * flow is tools/tests/help_main.c). */
#ifdef DW3_PORT_OPTIONS_TEST
static int g_test_help_open;
#endif

#ifdef DW3_PORT_OPTIONS_TEST
static int g_test_wt_open;
#endif

/* P6.35: the Walkthrough group's screen. It shows through the help's view and
 * layer, so it has the pad the same way the Help does. */
#ifndef DW3_PORT_OPTIONS_TEST
static int po_wt_open_p(void) {
    return dw3_wt_is_open();
}
#endif

static int po_help_open_p(void) {
#ifdef DW3_PORT_OPTIONS_TEST
    return g_test_help_open || g_test_wt_open;
#else
    return dw3_help_is_open() || dw3_wt_is_open();
#endif
}

#ifndef DW3_PORT_OPTIONS_TEST
/* {path:root}: the runtime folder above the region's (sdmc:/switch/dw3/). */
static char g_help_root[DW3_OPTCFG_PATH_MAX];

static const char *po_help_root(void) {
    const char *dir = dw3_port_options_cfg_dir();
    size_t n = strlen(dir);

    if (n == 0 || n >= sizeof(g_help_root) - 2) {
        return "";
    }
    memcpy(g_help_root, dir, n + 1);
    while (n > 0 && (g_help_root[n - 1] == '/' || g_help_root[n - 1] == '\\')) {
        g_help_root[--n] = '\0';
    }
    if (n >= 3 && (strcmp(g_help_root + n - 3, "/us") == 0 ||
                   strcmp(g_help_root + n - 3, "/eu") == 0)) {
        g_help_root[n - 2] = '\0';
    } else {
        g_help_root[n] = '/';
        g_help_root[n + 1] = '\0';
    }
    return g_help_root;
}
#endif

#ifndef DW3_PORT_OPTIONS_TEST
/* Hand the help's view to the native overlay (or take it away). */
static void po_help_present(void) {
#if defined(DW3_TITLE_NATIVE_OVERLAY)
    dw3_pc_help_set_view(dw3_help_is_open() ? dw3_help_view()
                         : dw3_wt_is_open() ? dw3_wt_view() : NULL);
#endif
}
#endif

static void po_open_help(void) {
#ifdef DW3_PORT_OPTIONS_TEST
    g_test_help_open = 1;
    fprintf(stderr, "dw3-port-options: group open help (test)\n");
    po_sound(PO_SND_SELECT);
#else
    Dw3HelpConfig c;

    memset(&c, 0, sizeof(c));
    c.from = !g_in_game ? DW3_HELP_FROM_TITLE
             : strcmp(g_where, "field menu") == 0 ? DW3_HELP_FROM_FIELD
                                                  : DW3_HELP_FROM_BATTLE;
    c.in_game = g_in_game;
#if VERSION_EU
    c.region = DW3_PAGES_EU;
#else
    c.region = DW3_PAGES_US;
#endif
#ifdef __SWITCH__
    c.platform = DW3_PAGES_SWITCH;
#else
    c.platform = DW3_PAGES_PC;
#endif
#if defined(DW3_TITLE_NATIVE_OVERLAY)
    c.lang = dw3_pc_title_overlay_get_options_language();
#endif
    c.root = po_help_root();
    c.memcard = memcard_host_dir();
#ifdef DW3_NRO_VERSION_STR
    c.version = DW3_NRO_VERSION_STR;
#endif
    if (dw3_help_open(&c) != 0) {
        PO_WARN("dw3-port-options: help cannot be opened (see dw3-help)");
        return;
    }
    PO_INFO("dw3-port-options: group open help (%s)",
            g_in_game ? "in game" : "title");
    po_sound(PO_SND_SELECT);
    po_help_present();
#endif
}

static void po_open_walkthrough(void) {
#ifdef DW3_PORT_OPTIONS_TEST
    g_test_wt_open = 1;
    fprintf(stderr, "dw3-port-options: group open walkthrough (test)\n");
    po_sound(PO_SND_SELECT);
#else
    Dw3WtConfig c;

    memset(&c, 0, sizeof(c));
    c.from = !g_in_game ? DW3_HELP_FROM_TITLE
             : strcmp(g_where, "field menu") == 0 ? DW3_HELP_FROM_FIELD
                                                  : DW3_HELP_FROM_BATTLE;
    c.in_game = g_in_game;
#if VERSION_EU
    c.region = DW3_PAGES_EU;
#else
    c.region = DW3_PAGES_US;
#endif
#ifdef __SWITCH__
    c.platform = DW3_PAGES_SWITCH;
#else
    c.platform = DW3_PAGES_PC;
#endif
#if defined(DW3_TITLE_NATIVE_OVERLAY)
    c.lang = dw3_pc_title_overlay_get_options_language();
#endif
    c.snapshot = dw3_walkthrough_snapshot;
    if (dw3_wt_open(&c) != 0) {
        PO_WARN("dw3-port-options: walkthrough cannot be opened (see dw3-walkthrough)");
        return;
    }
    PO_INFO("dw3-port-options: group open walkthrough (%s)",
            g_in_game ? "in game" : "title");
    po_sound(PO_SND_SELECT);
    po_help_present();
#endif
}

static void po_help_shutdown(void) {
#ifndef DW3_PORT_OPTIONS_TEST
    dw3_help_close();
    dw3_wt_close();
    po_help_present();
#else
    g_test_help_open = 0;
    g_test_wt_open = 0;
#endif
}

/* The help's tick: the keys of this frame, the sound it asks for. */
static void po_help_tick(void) {
#ifdef DW3_PORT_OPTIONS_TEST
    if (po_key_back()) {
        fprintf(stderr, "dw3-port-options: group close %s (test)\n",
                g_test_wt_open ? "walkthrough" : "help");
        g_test_help_open = 0;
        g_test_wt_open = 0;
        po_sound(PO_SND_CANCEL);
    }
#else
    unsigned k = 0;
    int snd;
    const int po_was_wt = po_wt_open_p();

    if (po_key_up()) k |= DW3_HELP_KEY_UP;
    if (po_key_down()) k |= DW3_HELP_KEY_DOWN;
    if (po_key_left()) k |= DW3_HELP_KEY_LEFT;
    if (po_key_right()) k |= DW3_HELP_KEY_RIGHT;
    if (po_key_prev()) k |= DW3_HELP_KEY_PREV;
    if (po_key_next()) k |= DW3_HELP_KEY_NEXT;
    if (po_key_back()) k |= DW3_HELP_KEY_BACK;
    if (po_key_confirm()) k |= DW3_HELP_KEY_CONFIRM;
    snd = po_wt_open_p() ? dw3_wt_tick(k) : dw3_help_tick(k);
    if (snd == DW3_HELP_SND_CURSOR) {
        po_sound(PO_SND_CURSOR);
    } else if (snd == DW3_HELP_SND_SELECT) {
        po_sound(PO_SND_SELECT);
    } else if (snd == DW3_HELP_SND_CANCEL) {
        po_sound(PO_SND_CANCEL);
    }
    po_help_present();
    if (!dw3_help_is_open() && !dw3_wt_is_open()) {
        PO_INFO("dw3-port-options: group close %s (back to the group list)",
                po_was_wt ? "walkthrough" : "help");
    }
#endif
}

#ifdef DW3_CHEATS
/* Open the Cheats screen (title only: cheats are armed at the title). */
static void po_open_cheats(const char *how) {
    if (g_in_game) {
        /* P9.7: the entry is greyed in game. */
        fprintf(stderr, "dw3-port-options: cheats are locked in game "
                        "(change them at the title)\n");
        fflush(stderr);
        return;
    }
    fprintf(stderr, "dw3-port-options: group open cheats (%s)\n", how);
    fflush(stderr);
    po_sound(PO_SND_SELECT);
    dw3_cheats_open();
}
#endif

/* Open a group from the list: its rows, or the Cheats screen. */
static void po_group_open(int group) {
    if (group < 0 || group >= DW3_OPTGRP_COUNT) {
        return;
    }
#ifdef DW3_CHEATS
    if (group == DW3_OPTGRP_CHEATS) {
        po_open_cheats("group");
        return;
    }
#endif
    if (group == DW3_OPTGRP_HELP) {
        po_open_help();
        return;
    }
    if (group == DW3_OPTGRP_WALKTHROUGH) {
        po_open_walkthrough();
        return;
    }
    g_level = 1;
    g_group = group;
    g_cursor = g_scroll = 0;
    po_sound(PO_SND_SELECT);
    fprintf(stderr, "dw3-port-options: group open %s (%d row(s), %s)\n",
            g_group_names[group], dw3_optgroups[group].nrows,
            g_in_game ? "in game" : "title");
    fflush(stderr);
}

/* Back from a group to the list, the cursor on the group that was open. */
static void po_group_back(void) {
    fprintf(stderr, "dw3-port-options: group close %s (back to the group "
                    "list)\n", g_group_names[g_group]);
    fflush(stderr);
    g_level = 0;
}

void dw3_port_options_tick(void) {
    if (!g_open) {
        return;
    }
    g_open_frames++;
    if (g_open_frames <= PO_WATCH_FRAMES) {
        double now = po_now_ms();

        if (now - g_last_tick_ms > g_max_gap_ms) {
            g_max_gap_ms = now - g_last_tick_ms;
            g_max_gap_frame = g_open_frames;
        }
        g_last_tick_ms = now;
        if (g_open_frames == PO_WATCH_FRAMES) {
            po_watch_report("watch window over");
        }
    }
    if (!g_logged_open && g_open_frames == 1) {
        g_logged_open = 1;
        fprintf(stderr, "dw3-port-options: shell tick (Select opened)\n");
        fflush(stderr);
    }
    /* P6.30: the Help screen has the pad while it is open. */
    if (po_help_open_p()) {
        po_help_tick();
        if (!po_help_open_p()) {
            po_push_view(); /* back on the group list */
        }
        return;
    }
#ifdef DW3_CHEATS
    /* P6.20: the Cheats screen has the pad while it is open; Square (Y on
     * the Switch) opens it from any list, the Cheats group does too. */
    if (dw3_cheats_is_open()) {
        dw3_cheats_tick();
        if (!dw3_cheats_is_open()) {
            po_push_view(); /* back on the list it was opened from */
        }
        return;
    }
    if (po_key_square()) {
        po_open_cheats("Square / Y shortcut");
        return;
    }
#endif
    /* P6.12: the prompt of the Customize screen takes every press. */
    if (g_assign_btn >= 0) {
        po_assign_tick();
        po_push_view();
        return;
    }
    if (po_key_back()) {
        po_sound(PO_SND_CANCEL);
        if (g_level && g_group == DW3_OPTGRP_CUSTOM) {
            po_custom_back();
        } else if (g_level) {
            po_group_back();
        } else {
            dw3_port_options_close();
        }
        return;
    }
    if (po_key_up() && (g_level ? g_cursor : g_gcursor) > 0) {
        if (g_level) {
            g_cursor--;
        } else {
            g_gcursor--;
        }
        po_sound(PO_SND_CURSOR);
    }
    if (po_key_down() &&
        (g_level ? g_cursor : g_gcursor) <
            dw3_optlist_count(g_level ? g_group : -1, DW3_OPTGRP_CHEATS_LISTED) - 1) {
        if (g_level) {
            g_cursor++;
        } else {
            g_gcursor++;
        }
        po_sound(PO_SND_CURSOR);
    }
    if (!g_level) {
        if (po_key_confirm()) {
            po_group_open(po_group_at(g_gcursor));
        }
    } else if (g_group == DW3_OPTGRP_CUSTOM) {
        po_custom_tick();
    } else {
        const int row = po_cursor_row();

        if (po_is_stepped(row)) {
            if (po_key_left()) {
                po_step(row, -1);
            }
            if (po_key_right()) {
                po_step(row, 1);
            }
        }
        if (po_key_confirm()) {
            po_toggle(row);
        }
    }
    po_push_view();
}

#if !defined(DW3_PORT_OPTIONS_TEST) && defined(DW3_PC_HOST) && \
    !defined(DW3_TITLE_NATIVE_OVERLAY)
static void dw3_port_options_maybe_fb_dump(void) {
    const char *path;
    unsigned char *rgb;
    int w = 0;
    int h = 0;
    FILE *fp;
    size_t nbytes;
    static int dumped;

    if (dumped || g_open_frames < 45) {
        return;
    }
    path = getenv("DW3_PORT_OPTIONS_FB_DUMP");
    if (path == NULL || path[0] == '\0') {
        return;
    }
    rgb = Psyz_VideoAllocCapturedFrame(&w, &h);
    if (rgb == NULL || w <= 0 || h <= 0) {
        rgb = Psyz_VideoAllocVramDump(&w, &h);
    }
    if (rgb == NULL || w <= 0 || h <= 0) {
        fprintf(stderr, "dw3-port-options: FB dump failed\n");
        fflush(stderr);
        return;
    }
    nbytes = (size_t)w * (size_t)h * 3u;
    fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "dw3-port-options: cannot write %s\n", path);
        fflush(stderr);
        free(rgb);
        return;
    }
    fprintf(fp, "P6\n%d %d\n255\n", w, h);
    fwrite(rgb, 1, nbytes, fp);
    fclose(fp);
    free(rgb);
    dumped = 1;
    fprintf(stderr, "dw3-port-options: TTB FB dump %dx%d -> %s\n", w, h,
            path);
    fflush(stderr);
    if (getenv("DW3_PORT_OPTIONS_QUIT_AFTER_DUMP") != NULL) {
        exit(0);
    }
}
#endif

#if !defined(DW3_PORT_OPTIONS_TEST) && !defined(DW3_TITLE_NATIVE_OVERLAY)
/* P6.30: the help on a host without the native overlay (plain text panel). */
static void po_help_draw_plain(short ox, short oy) {
    const Dw3HelpView *v = dw3_wt_is_open() ? dw3_wt_view() : dw3_help_view();
    char buf[14][60];
    const char *lines[14];
    int n = 0;
    int i;

    if (v->screen == DW3_HELP_SCREEN_LIST) {
        for (i = v->scroll; i < v->npages && n < 13; i++) {
            snprintf(buf[n], sizeof(buf[n]), "%c %s",
                     i == v->cursor ? '>' : ' ', v->titles[i]);
            lines[n] = buf[n];
            n++;
        }
    } else {
        for (i = 0; i < v->panel && v->top + i < v->nlines && n < 13; i++) {
            const Dw3PagesLine *ln = &v->lines[v->top + i];
            int len = ln->len[0] < 58 ? ln->len[0] : 58;

            memcpy(buf[n], ln->s[0] != NULL ? ln->s[0] : "", (size_t)len);
            buf[n][len] = '\0';
            lines[n] = buf[n];
            n++;
        }
    }
    snprintf(buf[n], sizeof(buf[n]), "%s", v->hint[v->nhints - 1]);
    lines[n] = buf[n];
    n++;
    dw3_port_text_draw_panel((int)ox + 8, (int)oy + 8, 304, 224, lines, n);
}
#endif

void dw3_port_options_draw(void) {
#ifdef DW3_PORT_OPTIONS_TEST
    (void)g_cursor;
#elif defined(DW3_TITLE_NATIVE_OVERLAY)
    /* The native overlay draws the options screen (po_push_view). */
#else
    short ox = 0;
    short oy = 0;
    DISPENV env;
    char lines_buf[DW3_PORT_OPT_COUNT][48];
    char back_line[48];
    const char *lines[DW3_PORT_OPT_COUNT + 4];
    int n = 0;
    int i;

    if (!g_open) {
        return;
    }
#ifdef DW3_CHEATS
    if (dw3_cheats_is_open()) {
        dw3_cheats_draw_panel();
        return;
    }
#endif
    GetDispEnv(&env);
    ox = env.disp.x;
    oy = env.disp.y;
    if (po_help_open_p()) {
        po_help_draw_plain(ox, oy);
        return;
    }

    if (!g_level) {
        /* P9.9: the group list (PS1-primitive fallback, plain text). */
        int ids[DW3_OPTGRP_COUNT];
        int count = dw3_optgroups_visible(DW3_OPTGRP_CHEATS_LISTED, ids);

        lines[n++] = "Port options";
        lines[n++] = "------------";
        for (i = 0; i < count && n < DW3_PORT_OPT_COUNT + 2; i++) {
            snprintf(lines_buf[i], sizeof(lines_buf[i]), "%c %s",
                     (i == g_gcursor) ? '>' : ' ', g_group_names[ids[i]]);
            lines[n++] = lines_buf[i];
        }
    } else {
        lines[n++] = g_group_names[g_group];
        lines[n++] = "------------";
        for (int k = 0; k < dw3_optgroups[g_group].nrows &&
                        n < DW3_PORT_OPT_COUNT + 2; k++) {
            char *buf = lines_buf[k];

            i = dw3_optgroups[g_group].rows[k];
            if (i == DW3_PORT_OPT_CUSTOMIZE || i == DW3_PORT_OPT_RESET_MAP) {
                snprintf(buf, sizeof(lines_buf[k]), "%c %s",
                         (k == g_cursor) ? '>' : ' ', g_stubs[i].label);
            } else if (po_is_mode(i)) {
                snprintf(buf, sizeof(lines_buf[k]), "%c %s: %s",
                         (k == g_cursor) ? '>' : ' ', g_stubs[i].label,
                         po_mode_name(i, g_stubs[i].value));
            } else if (po_is_volume(i)) {
                snprintf(buf, sizeof(lines_buf[k]), "%c %s: %d",
                         (k == g_cursor) ? '>' : ' ', g_stubs[i].label,
                         g_stubs[i].value);
            } else if (i == DW3_PORT_OPT_FAST_FORWARD && g_stubs[i].value) {
                snprintf(buf, sizeof(lines_buf[k]), "%c %s: %dx (R3)",
                         (k == g_cursor) ? '>' : ' ', g_stubs[i].label,
                         g_stubs[i].value);
            } else {
                snprintf(buf, sizeof(lines_buf[k]), "%c %s: %s",
                         (k == g_cursor) ? '>' : ' ', g_stubs[i].label,
                         g_stubs[i].value ? "ON" : "OFF");
            }
            lines[n++] = buf;
        }
    }
    {
        char name[24];

        dw3_button_hint_name(DW3_BTN_TRIANGLE, "Triangle (S)", name, sizeof(name));
        snprintf(back_line, sizeof(back_line), "%s: Back", name);
    }
    lines[n++] = back_line;

    dw3_port_text_draw_panel((int)ox + 32, (int)oy + 32, 256, 128, lines, n);
#if defined(DW3_PC_HOST)
    dw3_port_options_maybe_fb_dump();
#endif
#endif
}

void dw3_port_options_draw_hint(void) {
#ifdef DW3_PORT_OPTIONS_TEST
    return;
#else
    short ox = 0;
    short oy = 0;
    DISPENV env;
    const char *lines[1];

    if (g_open) {
        return;
    }
    GetDispEnv(&env);
    ox = env.disp.x;
    oy = env.disp.y;
    lines[0] = "Select/Backspace: Port options";
    dw3_port_text_draw_panel((int)ox + 40, (int)oy + 210, 240, 20, lines, 1);
#endif
}

/* P6.20 (port/options/cheats.c). At the end of the file, so that a build
 * without DW3_CHEATS keeps every log line above at its line number (psyz_log
 * passes __LINE__) and compiles to the same code. */
#ifdef DW3_CHEATS
int dw3_port_options_store_int(const char *key, int value) {
    char num[16];

    if (!g_persist_on) {
        return -1; /* session-only options: the value stays live */
    }
    snprintf(num, sizeof(num), "%d", value);
    if (dw3_optcfg_set(&g_cfg, key, num) != 0) {
        PO_WARN("dw3-options: cannot stage %s", key);
        return -1;
    }
    if (po_save() != 0) {
        return -1;
    }
    PO_INFO("dw3-options: saved %s=%d", key, value);
    return 0;
}
#endif

#ifdef DW3_CHEATS
void dw3_port_options_play_sound(int kind) {
    if (kind >= PO_SND_CURSOR && kind <= PO_SND_CANCEL) {
        po_sound(kind);
    }
}
#endif

#if defined(DW3_CHEATS) && defined(DW3_PORT_OPTIONS_TEST)
int dw3_port_options_test_bit(int bit) {
    return po_pressed(bit);
}
#endif
