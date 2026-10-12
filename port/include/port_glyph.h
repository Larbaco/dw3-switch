/* Shared 8×8 glyph bit convention for port UI text (PC + Switch).
 *
 * Matches public-domain font8x8_basic: within each row byte, bit 0 (LSB) is
 * the leftmost pixel and bit 7 is the rightmost. Interpreting MSB-as-left
 * mirrors Latin glyphs.
 */
#ifndef DW3_PORT_GLYPH_H
#define DW3_PORT_GLYPH_H

#include <stdint.h>

static inline int dw3_port_glyph_pixel(uint8_t row_bits, int gx) {
    if (gx < 0 || gx > 7) {
        return 0;
    }
    return (row_bits & (uint8_t)(1u << gx)) != 0;
}

/* 'b' row 0 (0x07): left stem occupies the three leftmost pixels. */
static inline int dw3_port_glyph_b_row0_ltr(void) {
    const uint8_t row = 0x07u;
    return dw3_port_glyph_pixel(row, 0) && dw3_port_glyph_pixel(row, 1) &&
           dw3_port_glyph_pixel(row, 2) && !dw3_port_glyph_pixel(row, 3) &&
           !dw3_port_glyph_pixel(row, 4) && !dw3_port_glyph_pixel(row, 5) &&
           !dw3_port_glyph_pixel(row, 6) && !dw3_port_glyph_pixel(row, 7);
}

#endif /* DW3_PORT_GLYPH_H */
