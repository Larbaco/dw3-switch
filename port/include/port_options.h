/* P9.2 port options shell — title-menu Config/Extra overlay (MODE_TITLE). */
#ifndef DW3_PORT_OPTIONS_H
#define DW3_PORT_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

void dw3_port_options_open(void);
/* P9.7: open it from the field menu's Options row (port/options/
 * field_options.c). Same screen and pad; the rows that cannot change safely in
 * game are locked (Widescreen) and so is the Cheats entry. Closing returns to
 * the menu that opened it. */
void dw3_port_options_open_in_game(void);
int dw3_port_options_in_game(void);
/* P9.9: the open group (a DW3_OPTGRP_* of port_options_groups.h) or -1 on the
 * group list, and the cursor's item on the list on screen. */
int dw3_port_options_group(void);
int dw3_port_options_cursor(void);
/* Host overlay: the Options screen was drawn for the first time since it
 * opened; logs the time from the open request (the stutter measure). */
void dw3_port_options_note_shown(void);
/* D4 / A18: where the Options background is in its scroll, 0..95 (game pixels).
 * It moves one pixel every second tick of dw3_port_options_tick since the screen
 * opened (down and right, back to 0 at 96), like the Items menu's background
 * (STSTATUS_drawBackground), so it follows the game's own frame count and a
 * replay sees the same phase at the same frame. The hosts draw the tile quad
 * from it; after a close it keeps the last phase (the Switch fades the screen out). */
int dw3_port_options_bg_phase(void);
/* P9.7 slice 2: the words of the in-game open / close log lines ("field menu" /
 * "the field menu" by default; the battle sets its own). NULL restores them. */
void dw3_port_options_set_origin(const char *where, const char *back);
/* 1 when `index` is a row that cannot change in game; always 0 from the title. */
int dw3_port_options_row_locked(int index);
/* Bit i: row i locked; DW3_PORT_OPTIONS_LOCK_CHEATS: the Cheats entry. */
unsigned dw3_port_options_locked_mask(void);
/* The mask the screen opens with at the title (0) or in game: lets the
 * native overlay draw that layout before the screen is opened. */
unsigned dw3_port_options_locked_mask_for(int in_game);
/* D1: registry rows ran to 28 (D5 added one; P6.7 row 29), so the Cheats entry moved from bit 16 to
 * bit 30 (bit 31 is the in-game mark of the overlay's layer key). */
enum { DW3_PORT_OPTIONS_LOCK_CHEATS = 1u << 30 };
void dw3_port_options_close(void);

/* P6.38 (combined image): the folder holding the global port_options.cfg
 * (region_default / last_region and, since D3, every other key too), or NULL when
 * options are session-only or the build has no Region at start row. */
const char *dw3_port_options_region_base(void);
int dw3_port_options_is_open(void);

/* Pad + stub registry while open. Does not set title menu choice. The screen
 * is a list of groups (Display, Sound, Gameplay, Saves, Cheats); Confirm opens
 * one (its rows, or the Cheats screen) and Back returns to the list with the
 * cursor kept; Back on the list closes the screen. */
void dw3_port_options_tick(void);

/* Draw panel over title after STDWTITL_drawMenu. */
void dw3_port_options_draw(void);

/* One-line hint while NG/Continue are shown and shell is closed. */
void dw3_port_options_draw_hint(void);

/* Registry rows read outside this file (the others are internal): P6.18
 * pacing, see port_pacing.h and docs/P6.18_PACING.md, and P7.9's Battle
 * cursor (0 Off, 1 Command, 2 Command+Tech; port_battle_menu.h). */
enum {
    DW3_PORT_OPT_ENCOUNTER_RATE = 5,
    DW3_PORT_OPT_TEXT_SPEED = 6,
    DW3_PORT_OPT_BATTLE_MENU = 7,
    DW3_PORT_OPT_AUTOSAVE = 8, /* P6.31: 0 OFF, 1 On area change */
    DW3_PORT_OPT_BUTTON_LAYOUT = 9, /* P9.8: 0 PlayStation, 1 Nintendo */
    /* P51 item 4 (combined image only): 0 Ask, 1 US, 2 EU. Stored as
     * region_default in the TOP-LEVEL port_options.cfg, not in the region's. */
    DW3_PORT_OPT_REGION = 10,
    /* P6.12 (Controls group): two action rows with no value of their own.
     * Customize buttons opens the press-to-assign screen (title only), Reset
     * to default puts the PlayStation layout back. */
    DW3_PORT_OPT_CUSTOMIZE = 11,
    DW3_PORT_OPT_RESET_MAP = 12,
    /* P6.36: 0 = 1x (the music keeps its tempo in turbo, the default), 1 = Fast
     * (the music speeds up with the game, as before). Key turbo_music. */
    DW3_PORT_OPT_TURBO_MUSIC = 13,
    /* P10.6: 0 = the game's own button icons, 1 = Switch glyphs that
     * follow the button map. D2: the default is 1 on the Switch (where the row is)
     * and 0 on the PC (no row). A Controls row on the Switch only (the PC has no
     * row; the key switch_glyphs / DW3_SWITCH_GLYPHS turn it on for the replay
     * tests). Key switch_glyphs. */
    DW3_PORT_OPT_SWITCH_GLYPHS = 14,
    /* P6.32 phase 4: the full-screen map view, 0 = off (default), 1 = on. A
     * Display row (a switch row; Confirm toggles), read at every Square on the
     * game's Map screen, so it works at once and in game. Key map_screen, the
     * one the preview builds already used. */
    DW3_PORT_OPT_MAP_SCREEN = 15,
    /* D1 (docs/OPTIONS_REORG.md): every file-only key that changes the game
     * versus the original now has a row. The five Menus switches (default ON;
     * the patches 0188, 0190, 0192, 0193 and 0128 read them where they act), two
     * Display switches (the title's version label and the map-upscale pack), the
     * Saves row for the shared store (title only, default OFF, a Yes / No window
     * when it is turned on), the Advanced pair for the stage sweep (title only)
     * and the Controls action row that opens the Map buttons sub-screen. */
    DW3_PORT_OPT_ITEMS_EQUIP = 16,   /* items_equip_from_items */
    DW3_PORT_OPT_ITEMS_STAYS = 17,   /* items_equip_stays */
    DW3_PORT_OPT_ITEMS_BAG = 18,     /* items_bag_view */
    DW3_PORT_OPT_ITEMS_USE = 19,     /* items_use_stays */
    DW3_PORT_OPT_ITEMS_PARTNER = 20, /* items_partner_switch */
    DW3_PORT_OPT_SHOW_VERSION = 21,  /* show_version */
    DW3_PORT_OPT_MAP_MOD = 22,       /* map_mod */
    DW3_PORT_OPT_SAVE_STORE = 23,    /* save_store, read before the cards open */
    DW3_PORT_OPT_STAGE_SWEEP = 24,   /* stage_sweep: 0 OFF, 1 Run, 2 Start over */
    DW3_PORT_OPT_SWEEP_FRAMES = 25,  /* stage_sweep_frames: 30..3600 */
    DW3_PORT_OPT_MAP_BUTTONS = 26,   /* action row: opens the Map buttons screen */
    /* Menus: leaving the field menu's screens for the field skips the block
     * dissolve (patch 0200, port/menu_exit); default ON, key menu_instant_exit. */
    DW3_PORT_OPT_MENU_EXIT = 27,
    /* D5 (docs/OPTIONS_REORG.md section 6): the full-screen map's picture scaler,
     * a Display row (Nearest / Sharp bilinear / Lanczos2 / FSR 1 / Bilinear).
     * Stored as a DW3_MAPSCALER_* (the order of map_view_core.h MV_SCALER_*),
     * written as a word: map_scaler=fsr1. Default FSR 1. */
    DW3_PORT_OPT_MAP_SCALER = 28,
    /* P6.7 (docs/LOCALIZATION.md): the language of the game's text, a Display row.
     * 0 = Original (the disc's own text), 1 = Portuguese (BR) from a local text pack
     * (port_l10n.h). The file holds the words, text_language=original|ptbr;
     * DW3_LANG=ptbr pins it for one run. Row 29 is the last the lock mask has room for:
     * bit 30 is the Cheats entry and bit 31 the overlay's in-game mark. */
    DW3_PORT_OPT_TEXT_LANGUAGE = 29
};

/* D5: the values of the Map scaler row, in the order of map_view_core.h's
 * MV_SCALER_* (map_view.c asserts they agree). */
enum {
    DW3_MAPSCALER_NEAREST = 0,
    DW3_MAPSCALER_SHARP = 1,
    DW3_MAPSCALER_LANCZOS2 = 2,
    DW3_MAPSCALER_FSR1 = 3,
    DW3_MAPSCALER_BILINEAR = 4,
    DW3_MAPSCALER_COUNT = 5,
    DW3_MAPSCALER_DEFAULT = DW3_MAPSCALER_FSR1 /* owner 2026-10-31 */
};

/* D2 (docs/OPTIONS_REORG.md section 6.1): the generation of the defaults. A
 * port_options.cfg without `defaults_gen` counts as 1 (every file written before
 * D2); D2 sets 2 and migrates a file below it once (button_layout playstation ->
 * nintendo, switch_glyphs 0 -> 1). A later default change raises this number. */
enum { DW3_DEFAULTS_GEN = 2 };

/* Sweep frames: the range and the steps of the Advanced row (the sweep itself
 * clamps the same range, port/sweep/stage_sweep.c). */
enum {
    DW3_SWEEP_FRAMES_MIN = 30,
    DW3_SWEEP_FRAMES_MAX = 3600,
    DW3_SWEEP_FRAMES_DEFAULT = 90,
    DW3_SWEEP_FRAMES_STEP = 30,
    DW3_SWEEP_FRAMES_BIG_STEP = 300
};

/* Registry; unavailable features remain safe no-op stubs. Values persist in
 * <config dir>/port_options.cfg (P9.4, docs/PORT_OPTIONS.md). */
int dw3_port_options_count(void);
const char *dw3_port_options_label(int index);
/* The cfg key of a row (NULL for a row with none: Region at start, which is
 * saved as region_default by its own code, and the action rows). The key-budget test lists them. */
const char *dw3_port_options_key(int index);
int dw3_port_options_value(int index);
void dw3_port_options_set(int index, int value);
/* Raw row value: 0/1, 0 (OFF) / 2..4 (speed) for Fast-forward, 0..10 for the
 * volumes, 0..4 for Encounter rate, 0..2 for Text speed and Battle cursor,
 * 0/1 for Auto-save and 0 PlayStation / 1 Nintendo / 2 Custom for Button
 * layout (port_button_map.h). */
int dw3_port_options_level(int index);
void dw3_port_options_set_level(int index, int level);

/* Restart is opt-in (OFF by default); the chord is polled by restart.c. */
int dw3_port_options_restart_enabled(void);

/* P9.4. Call once at boot, after the runtime/config directory is known and
 * before anything reads a flag (widescreen, fast-forward, restart hotkey).
 * `dir` is the folder of the global file (dw3_optcfg_global_dir derives it from
 * the region's runtime folder). D3: first folds the old <dir>/us/ and <dir>/eu/
 * port_options.cfg into it, once (port_options_cfg.h).
 * Loads <dir>/port_options.cfg over the defaults: a missing file keeps the
 * defaults, an invalid value falls back to its default with a log line,
 * unknown keys are kept for the next write. Returns 0, or -1 if `dir` is
 * unusable or the file unreadable (the defaults apply; a later change
 * replaces the file). With no call at all the options are session-only,
 * which is what the host tests rely on. */
int dw3_port_options_persist_init(const char *dir);

/* A value forced for this run by the environment (DW3_WIDESCREEN,
 * DW3_TURBO). It is used like any other, but never written to the file by
 * an unrelated change; toggling that row on the Options screen releases it. */
void dw3_port_options_apply_env(int index, int level);

/* An integer key of port_options.cfg: its value as loaded at boot, or `def`
 * when the key is absent, not a number, or no file was loaded. A key that has
 * a registry row (all of the port's behaviour keys since D1) answers with the
 * row's value, so a change on the Options screen is seen at once, and also in a
 * session-only run. Other keys are kept when the Options screen rewrites the
 * file. */
int dw3_port_options_file_int(const char *key, int def);
/* A file-only text key (P6.32 phase 4: the map view's button bindings): its
 * value as loaded at boot, or NULL when absent or no file was loaded. The
 * pointer is valid until the next call that rewrites the file; copy it. */
const char *dw3_port_options_file_str(const char *key);

/* D1: set a text key of port_options.cfg and write the file now (the Map
 * buttons screen's seven map_view_* keys). Returns 0, or -1 when the key could
 * not be staged or written (retried by dw3_port_options_flush). A session-only
 * run keeps the value in memory and returns -1. */
int dw3_port_options_store_str(const char *key, const char *value);
/* Bumped by every change of a key behind the Map buttons screen (and by a
 * successful store_str): lets the full-screen map read its bindings again at the
 * next open instead of once per run. */
unsigned dw3_port_options_keys_generation(void);

/* D1: the sweep has started a "Start over" (stage_sweep=2): the file goes back
 * to 1 ("Run") so that the next start continues the sweep instead of clearing
 * its progress again. A no-op unless the row holds 2. Logs the rewrite. */
void dw3_port_options_sweep_started(void);

/* D1: the answer of a Yes / No window the Options screen asked for
 * (port/options/confirm.c). `what` is a DW3_PORT_ASK_* of port_confirm.h. */
void dw3_port_options_ask_answer(int what, int yes);

/* The folder of the ONE global port_options.cfg (D3: the dir given to
 * _persist_init; sdmc:/switch/dw3 on the Switch), or "" when the options are
 * session-only. Small files of the port's own (the P5.5 stage sweep's progress,
 * one per region) are kept beside it. */
const char *dw3_port_options_cfg_dir(void);

/* Retry a write that failed earlier (the SD was busy); a no-op otherwise.
 * Hosts register it with atexit. Changes are written when they are made. */
void dw3_port_options_flush(void);

#ifdef DW3_CHEATS
#include "port_cheats.h"
/* P6.20 (port/options/cheats.c, docs/PORT_OPTIONS.md "Cheats"). Square (Y on
 * the Switch) on the Options screen opens the Cheats screen. */

/* Set a file-only integer key (a cheat_* toggle) and write the file now,
 * like a change on the Options screen. Returns 0, or -1 when the file could
 * not be written (retried by dw3_port_options_flush). */
int dw3_port_options_store_int(const char *key, int value);

/* The Options screen's sounds, for the Cheats screen. */
enum {
    DW3_PORT_OPTIONS_SOUND_CURSOR = 0,
    DW3_PORT_OPTIONS_SOUND_SELECT = 1,
    DW3_PORT_OPTIONS_SOUND_CANCEL = 2
};
void dw3_port_options_play_sound(int kind);

#ifdef DW3_PORT_OPTIONS_TEST
/* The pad bit given by dw3_port_options_test_set_pressed (host tests). */
int dw3_port_options_test_bit(int bit);
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_OPTIONS_H */
