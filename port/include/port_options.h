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
enum { DW3_PORT_OPTIONS_LOCK_CHEATS = 1u << 16 };
void dw3_port_options_close(void);
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
    DW3_PORT_OPT_AUTOSAVE = 8 /* P6.31: 0 OFF, 1 On area change */
};

/* Registry; unavailable features remain safe no-op stubs. Values persist in
 * <config dir>/port_options.cfg (P9.4, docs/PORT_OPTIONS.md). */
int dw3_port_options_count(void);
const char *dw3_port_options_label(int index);
int dw3_port_options_value(int index);
void dw3_port_options_set(int index, int value);
/* Raw row value: 0/1, 0 (OFF) / 2..4 (speed) for Fast-forward, 0..10 for the
 * volumes, 0..4 for Encounter rate, 0..2 for Text speed and Battle cursor,
 * 0/1 for Auto-save. */
int dw3_port_options_level(int index);
void dw3_port_options_set_level(int index, int level);

/* Restart is opt-in (OFF by default); the chord is polled by restart.c. */
int dw3_port_options_restart_enabled(void);

/* P9.4. Call once at boot, after the runtime/config directory is known and
 * before anything reads a flag (widescreen, fast-forward, restart hotkey).
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

/* A file-only integer key of port_options.cfg (a setting with no row on the
 * Options screen; there is none today, P7.9's battle_menu_memory became the
 * Battle cursor row): its value as loaded at boot, or `def` when the key is
 * absent, not a number, or no file was loaded. The key is kept when the
 * Options screen rewrites the file. */
int dw3_port_options_file_int(const char *key, int def);

/* The directory port_options.cfg lives in (the dir given to _persist_init),
 * or "" when the options are session-only. Small files of the port's own
 * (the P5.5 stage sweep's progress) are kept beside it. */
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
