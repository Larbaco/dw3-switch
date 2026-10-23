/* P9.2 port options shell — title-menu Config/Extra overlay (MODE_TITLE). */
#ifndef DW3_PORT_OPTIONS_H
#define DW3_PORT_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

void dw3_port_options_open(void);
void dw3_port_options_close(void);
int dw3_port_options_is_open(void);

/* Pad + stub registry while open. Does not set title menu choice. */
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
    DW3_PORT_OPT_BATTLE_MENU = 7
};

/* Registry; unavailable features remain safe no-op stubs. Values persist in
 * <config dir>/port_options.cfg (P9.4, docs/PORT_OPTIONS.md). */
int dw3_port_options_count(void);
const char *dw3_port_options_label(int index);
int dw3_port_options_value(int index);
void dw3_port_options_set(int index, int value);
/* Raw row value: 0/1, 0 (OFF) / 2..4 (speed) for Fast-forward, 0..10 for the
 * volumes, 0..4 for Encounter rate, 0..2 for Text speed and Battle cursor. */
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

/* Retry a write that failed earlier (the SD was busy); a no-op otherwise.
 * Hosts register it with atexit. Changes are written when they are made. */
void dw3_port_options_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_OPTIONS_H */
