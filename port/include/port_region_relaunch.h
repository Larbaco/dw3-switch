/* P6.38: restart into the chosen region (combined image only).
 *
 * The Options screen's Region at start row (port/options/port_options.c) saves
 * the choice as it always did. When the player leaves the row after changing it
 * to the OTHER region, the Yes / No window of port/options/confirm.c asks
 * "Restart in EU now?"; Yes relaunches the binary into that region
 * (port/region/dw3_region_relaunch.h, the host hooks), No keeps "takes effect
 * at the next start". Ask, and the region that is already running, never ask.
 * Policy and log lines live in port/options/region_relaunch.c; see
 * docs/PORT_OPTIONS.md, "Restart into the chosen region". Plain US / EU images
 * have no Region row, so every call here is then a no-op. */
#ifndef DW3_PORT_REGION_RELAUNCH_H
#define DW3_PORT_REGION_RELAUNCH_H

#ifdef __cplusplus
extern "C" {
#endif

/* The Options screen opened: forget a change from an earlier visit. */
void dw3_port_region_relaunch_reset(void);

/* The player stepped the Region row (any value, also back to where it was). */
void dw3_port_region_relaunch_note_step(void);

/* The cursor left the Region row. `value` is the row's value now (0 Ask, 1 US,
 * 2 EU), `in_game` whether Options was opened from the field menu or a battle.
 * Opens the window when the change asks for a restart. Returns 1 when a window
 * was opened (the Options screen then waits for it). */
int dw3_port_region_relaunch_leave(int value, int in_game);

/* The window's answers (port/options/confirm.c). `target` is 1 US / 2 EU. */
void dw3_port_region_relaunch_yes(int target);
void dw3_port_region_relaunch_no(int target, int cancelled);
/* The "Restart the game to apply." notice was dismissed. */
void dw3_port_region_relaunch_notice_done(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_REGION_RELAUNCH_H */
