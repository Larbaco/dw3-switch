/* P6.1 widescreen: side gutters in field, battle and some menus, on by
 * default. */
#ifndef DW3_PORT_WIDESCREEN_H
#define DW3_PORT_WIDESCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Gutter per side, in 320-wide framebuffer pixels: 16:9 in field and
 * battle. The field's map streamer draws a fifth tile column for a gutter
 * over 32 (patch 0052), into 3 more VRAM tile slots. */
#define DW3_WIDESCREEN_FIELD_PX 53
#define DW3_WIDESCREEN_BATTLE_PX 53
/* Field menu, item shop, memory card, partner name, battle report, player
 * name (Digimon Online registration) and title screens. */
#define DW3_WIDESCREEN_MENU_PX 53

/* The registry toggle (Port options "Widescreen"), on by default;
 * DW3_WIDESCREEN=0 in the environment turns it off at boot. */
int dw3_port_widescreen_enabled(void);

/* The gutter the current mode draws with: 0 when off or in a mode without
 * gutters (movies, card game, lab, gym, the other menus). Game patches widen
 * their culling and camera bounds by this. */
int dw3_port_widescreen_px(void);

/* Once per frame, before drawFrame: hands the mode's gutter to the renderer. */
void dw3_port_widescreen_frame(void);

/* The field menu's 16:9 layout (patch 0076): the field menu and its STSTATUS
 * screens draw the panels that hang from the old left edge (x 0) further
 * left by this, and those that hang from the old right edge (x 320) further
 * right, so they end at the picture's edges. It is the mode's gutter, so 0
 * when widescreen is off (the PS1 layout). Drawing positions only: cursors,
 * hit tests and input keep the PS1 values. */
int dw3_port_widescreen_menu_shift(void);
#define DW3_MENU_LEFT(x) ((x) - dw3_port_widescreen_menu_shift())
#define DW3_MENU_RIGHT(x) ((x) + dw3_port_widescreen_menu_shift())

/* A full-width band of a menu sprite sheet (the help bar, the item list's
 * frame) widened by the menu shift on each side: a copy of frame `frame` of
 * `sheet` whose parts left of `split` (in the frame's own x) move left by the
 * shift and the others right, with the parts that end the left group repeated
 * across the 2 * shift gap (under the others). Draw the copy with the same
 * frame id at the same place. Returns `sheet` itself when the shift is 0 or
 * the frame does not fit; the copy is valid until the next call. */
void *dw3_port_widescreen_menu_band(void *sheet, int frame, int split);
#define DW3_MENU_BAND(sheet, frame, split) dw3_port_widescreen_menu_band((sheet), (frame), (split))

/* The title screen's backdrop calls this every frame it is drawn (patch
 * 0085): only then does the title overlay (which also plays the movies and
 * the still screen) get the menu gutter. */
void dw3_port_widescreen_title_backdrop(void);

/* A repeating pattern (a scrolling backdrop) carried into the gutters (patch
 * 0087): a copy of frame `frame` of `sheet`, to be drawn at (x, y), plus the
 * copies of its parts moved by k * (dx, dy), k = -3..3, that show in a gutter
 * and are not already in the frame, in the frame's order. (dx, dy) is the
 * pattern's period. Returns `sheet` itself when the gutter is 0 or the frame
 * does not fit; the copy is valid until the next call of this or
 * dw3_port_widescreen_wings. */
void *dw3_port_widescreen_periodic(void *sheet, int frame, int x, int y, int dx, int dy);
#define DW3_WIDE_PERIODIC(sheet, frame, x, y, dx, dy) \
    dw3_port_widescreen_periodic((sheet), (frame), (x), (y), (dx), (dy))

/* Side bands that end at the 4:3 edges carried to the 16:9 edges (patch
 * 0087): a copy of frame `frame` of `sheet`, to be drawn at x, where each part
 * that starts at x 0 or ends at x 320 gets its outer 8 columns repeated out
 * to the picture's edge (under it). For straight lines that run off the
 * screen. Same return and lifetime as dw3_port_widescreen_periodic. */
void *dw3_port_widescreen_wings(void *sheet, int frame, int x);
#define DW3_WIDE_WINGS(sheet, frame, x) dw3_port_widescreen_wings((sheet), (frame), (x))

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_WIDESCREEN_H */
