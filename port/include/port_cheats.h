/* P6.20 Cheats sub-screen of the Options screen: a test aid for hardware
 * testing (docs/PORT_OPTIONS.md "Cheats", ROADMAP P6.20).
 *
 * Everything here exists only when the build defines DW3_CHEATS (CMake option
 * DW3_CHEATS, default ON; tools/build_switch_game.sh --no-cheats turns it off).
 * A build without it has no Cheats screen, no hook and no save flag, and is the
 * same game as before P6.20. With it, every cheat is off until the master
 * switch of the Cheats screen is turned on.
 *
 * Store: the master switch and Instant win are toggles kept in
 * port_options.cfg (`cheat_enabled`, `cheat_instant_win`); Fill bag and Battle
 * now are one-shot actions, armed on the screen for this run only and used up
 * by the game. No cheat writes a save by itself; a save written after a cheat
 * changed the game is marked in a sidecar file beside it (save<N>.cheats). */
#ifndef DW3_PORT_CHEATS_H
#define DW3_PORT_CHEATS_H

#ifdef __cplusplus
extern "C" {
#endif

/* The Cheats screen's rows, in screen order. */
enum {
    DW3_CHEAT_ROW_MASTER = 0,     /* OFF / ON (cheat_enabled) */
    DW3_CHEAT_ROW_FILL_BAG = 1,   /* OFF / Ready: one-shot */
    DW3_CHEAT_ROW_INSTANT_WIN = 2,/* OFF / ON (cheat_instant_win) */
    DW3_CHEAT_ROW_BATTLE_NOW = 3, /* OFF / Ready: one-shot */
    DW3_CHEAT_ROW_SAVES = 4,      /* read-only: marked saves, None or 1..6 */
    DW3_CHEAT_ROWS = 5
};

#define DW3_CHEAT_KEY_ENABLED "cheat_enabled"
#define DW3_CHEAT_KEY_INSTANT_WIN "cheat_instant_win"

/* Fill bag: the item ids patch 0070's hook fills, and how many of each. The
 * range is every usable item, weapon, armour and accessory; the key items
 * (1..42, 360, 395..402, ITEM_KIND_KEY) are left alone because they drive
 * the story. The card packs 361..394 are not in it either. */
#define DW3_CHEAT_FILL_FIRST 43
#define DW3_CHEAT_FILL_LAST 359
#define DW3_CHEAT_FILL_COUNT 99

/* Battle now: the countdown step that ends GAME.battleSteps (at most 0x4FF)
 * on the next footstep in a random-battle area. */
#define DW3_CHEAT_BATTLE_NOW_STEP 0x10000

/* Reads the toggles (port_options.cfg, loaded by the host at boot) and
 * logs the cheat state and every save marked as written with cheats.
 * `memdir` holds save0.bin / save1.bin and their save<N>.cheats sidecars;
 * NULL turns the marks off. The first call of any function below does it
 * with memcard_host_dir(), so the hosts need no call (host tests call it). */
void dw3_cheats_init(const char *memdir);

/* The Cheats screen, opened from the Options screen (Square, Y on the
 * Switch) and closed back to it (Triangle). While it is open the Options
 * screen hands its pad to dw3_cheats_tick. */
void dw3_cheats_open(void);
void dw3_cheats_close(void);
int dw3_cheats_is_open(void);
void dw3_cheats_tick(void);
int dw3_cheats_cursor(void);
/* The value a row shows: 0/1 for the switches and the armed one-shots, the
 * number of marked saves (0..6) for the last row. */
int dw3_cheats_value(int row);
/* 1 while the master switch is on. */
int dw3_cheats_enabled(void);

/* --- hooks (patch 0077 and port/pacing/pacing.c) ------------------------ */

/* getenv for the hooks that the Cheats screen can also drive. The variable
 * wins when it is set (the PC replay hooks keep working as before); else a
 * cheat that is armed answers with the value the hook expects, and is used up:
 * DW3_FORCE_ITEMS -> "43-359:99" while Fill bag is ready. Anything else is
 * plain getenv. */
const char *dw3_cheats_getenv(const char *name);

/* FIGHTSTG starts a battle (battle_mode/start.c). */
void dw3_cheats_battle_begin(void);
/* The battle menu's turn state (normal_battle/menu.c): 1 once per battle
 * while Instant win is on, and the caller ends the battle as won. */
int dw3_cheats_take_instant_win(void);

/* FIELDSTG_countBattleSteps (patch 0062, port/pacing/pacing.c): 1 while
 * Battle now is ready; the footstep then ends the countdown. */
int dw3_cheats_battle_now_armed(void);
/* The random battle is starting: Battle now is used up (if it was ready). */
void dw3_cheats_battle_now_started(void);

/* The title menu starts: a new game or a load follows, so the mark of the
 * game in progress is cleared. */
void dw3_cheats_title_started(void);

/* The memory card screen wrote a game to `slot` (0..2) of card `port`
 * (0/1): the slot's sidecar mark is set when a cheat changed this game, and
 * cleared otherwise. `offset` is the slot's byte offset in save<port>.bin and
 * `data`/`size` the bytes written; the mark holds their hash, so a slot later
 * overwritten by a build without cheats (or a copied file) is not marked. */
void dw3_cheats_save_written(int port, int slot, int offset,
                             const void *data, int size);
/* A game was loaded from that slot: a marked save keeps the game marked. */
void dw3_cheats_save_loaded(int port, int slot, const void *data, int size);

/* 1 once a cheat changed the game in progress (or it was loaded from a
 * marked save). */
int dw3_cheats_game_marked(void);

/* Text panel for the hosts without the native overlay (PC SDL3 GL). */
void dw3_cheats_draw_panel(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_CHEATS_H */
