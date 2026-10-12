/* Minimal host-agnostic 8×8 text blit into PS1 VRAM (P9.2 port options). */
#ifndef DW3_PORT_TEXT_H
#define DW3_PORT_TEXT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RGB555 panel + white glyphs via LoadImage. Returns 0 on success. */
int dw3_port_text_draw_panel(int vram_x, int vram_y, int width, int height,
                             const char *const *lines, int line_count);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_TEXT_H */
