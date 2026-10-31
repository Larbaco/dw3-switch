/* Instant menu exit (docs/P6.34_MENU_FLOW.md section 16, patch 0200).
 *
 * Leaving the field menu's screens (Items, Sort, Map, Techniques, Status, Card
 * folders) for the walking field plays the field menu's block dissolve: six
 * 64-pixel block sprites stepped through darker CLUT rows, two vsyncs a row,
 * about 96 frames. With the key on, the port runs that fade's frames in one go:
 * the fade's state machine is stepped to its end, and every frame of it still
 * advances what the main loop advances per frame (the vsync, frame and play
 * time counters, the play time clock and one RANDOM.next()), so the game state
 * when the field starts is the one the original reaches after the animation.
 * Only the drawing and the wait are skipped. Policy and the frame step are in
 * port/menu_exit/menu_exit.c. */
#ifndef DW3_PORT_MENU_EXIT_H
#define DW3_PORT_MENU_EXIT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Key of port_options.cfg and its Options row (Menus > Instant menu exit):
 * 0 or 1, default 1 (owner request, 2026-10-31). DW3_MENU_INSTANT_EXIT
 * overrides it for one run. 0 is the original game's fade. */
#define DW3_MENU_INSTANT_EXIT_KEY "menu_instant_exit"
int dw3_port_menu_instant_exit_enabled(void);

/* One frame of the main loop, minus the drawing and the waiting: what
 * vsyncCallback, drawFrame and main add per frame (counters, play time, one
 * RANDOM.next()). Called once for each frame of the skipped fade. */
void dw3_port_menu_exit_frame(void);

/* The name of the screen the field menu was reached from: FIELD_MENU_CHOICE's
 * option and extra (the list had the Card folders row): "items", "sort", "map",
 * "techniques", "status", "cards" or "other". */
const char *dw3_port_menu_exit_screen_name(int option, int extra);

/* Logs the exit once per event (psyz_log, so it reaches dw3.log on the
 * Switch): instant ("exit to field instant (skipped N transition frames,
 * from <screen>)") or the original fade ("exit to field with the original
 * transition (instant exit off, from <screen>)"). */
void dw3_port_menu_exit_report(int instant, int skipped, int option, int extra);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_MENU_EXIT_H */
