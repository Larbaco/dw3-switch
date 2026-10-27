/* Whole-picture fade to black, drawn by the native overlay on top of the
 * game's picture (PC SDL3 GPU and Switch GLES). Used by the return to title
 * (port/options/restart.c): the field fades out, the reset runs on a black
 * screen, and the title or the language select fades in, so the 4:3 frame the
 * discarded field left on screen is never shown. */
#ifndef DW3_PORT_SCREEN_FADE_H
#define DW3_PORT_SCREEN_FADE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 0 = no fade, 255 = black. */
void dw3_port_screen_fade_set(int level);
int dw3_port_screen_fade_level(void);

#ifdef __cplusplus
}
#endif

#endif
