/* P6.1 widescreen: side gutters in field and battle, on by default. */
#ifndef DW3_PORT_WIDESCREEN_H
#define DW3_PORT_WIDESCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Gutter per side, in 320-wide framebuffer pixels. Battle fills 16:9. The
 * field stops at 16:10: the map streamer keeps 4×3 tiles, which always
 * cover 32 columns past each edge but not more. */
#define DW3_WIDESCREEN_FIELD_PX 32
#define DW3_WIDESCREEN_BATTLE_PX 53

/* The registry toggle (Port options "Widescreen"), on by default;
 * DW3_WIDESCREEN=0 in the environment turns it off at boot. */
int dw3_port_widescreen_enabled(void);

/* The gutter the current mode draws with: 0 when off or outside field and
 * battle. Game patches widen their culling and camera bounds by this. */
int dw3_port_widescreen_px(void);

/* Once per frame, before drawFrame: hands the mode's gutter to the renderer. */
void dw3_port_widescreen_frame(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_WIDESCREEN_H */
