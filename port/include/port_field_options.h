/* P9.7 (slice 1): the port Options screen opened from the field menu's
 * Options row. Policy lives in port/options/field_options.c; the screen is
 * port/options/port_options.c, drawn by the native overlay
 * (platforms/pc/src/title_overlay_art.c). Patch 0160 is the game side. */
#ifndef DW3_PORT_FIELD_OPTIONS_H
#define DW3_PORT_FIELD_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

/* The field menu's Options row is offered where the native overlay that
 * draws the screen exists (PC SDL3 GPU and the Switch), the same condition as
 * Return to title (dw3_port_confirm_available). */
int dw3_port_field_options_enabled(void);

/* The row's text in the game's own font encoding (capitals, as the other
 * rows), for LANGUAGE `language` of the EU disc (1 and 2 English, 3 French,
 * 4 Italian, 5 German, 6 Spanish; the US disc passes 2). Valid until the
 * next call. */
const char *dw3_port_field_options_label(int language);

/* Cross on the row: open the screen (cursor on the first row) in `language`'s
 * lettering. Rows that cannot change safely in game are locked
 * (dw3_port_options_row_locked). */
void dw3_port_field_options_open(int language);
int dw3_port_field_options_is_open(void);

/* One frame while open: the screen has the pad (Up / Down / Left / Right /
 * Cross change rows, Triangle closes). The field menu calls it instead of its
 * own input, so no pad frame leaks into the game or the menu. */
void dw3_port_field_options_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_FIELD_OPTIONS_H */
