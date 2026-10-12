/* Host check: HUD font bit order is LSB-left (font8x8), not MSB-left. */
#include "debug_overlay_glyph.h"

#include <stdio.h>

int main(void) {
    /* Wrong convention (MSB left) would put the 'b' stem on the right. */
    printf("b_row0_ltr=%d\n", dw3_dbg_glyph_b_row0_ltr() ? 1 : 0);
    printf("bit0_left=%d\n", dw3_dbg_glyph_pixel(0x01u, 0) ? 1 : 0);
    printf("bit7_right=%d\n", dw3_dbg_glyph_pixel(0x80u, 7) ? 1 : 0);
    printf("bit7_not_left=%d\n", dw3_dbg_glyph_pixel(0x80u, 0) ? 0 : 1);
    return dw3_dbg_glyph_b_row0_ltr() ? 0 : 1;
}
