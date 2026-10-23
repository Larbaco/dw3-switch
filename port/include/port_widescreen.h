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
/* Field menu, item shop, memory card, partner name and battle report screens. */
#define DW3_WIDESCREEN_MENU_PX 53

/* The registry toggle (Port options "Widescreen"), on by default;
 * DW3_WIDESCREEN=0 in the environment turns it off at boot. */
int dw3_port_widescreen_enabled(void);

/* The gutter the current mode draws with: 0 when off or in a mode without
 * gutters (title, movies, card game, the other menus). Game patches widen
 * their culling and camera bounds by this. */
int dw3_port_widescreen_px(void);

/* Once per frame, before drawFrame: hands the mode's gutter to the renderer. */
void dw3_port_widescreen_frame(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_WIDESCREEN_H */
