/* PS1 VRAM is RGB555. GLES2's GL_UNSIGNED_SHORT_5_5_5_1 stores R in bits
 * 15–11, and this port has no GL_UNSIGNED_SHORT_1_5_5_5_REV. Uploading the
 * halfword raw scrambles every channel — that was the speckled title.
 * Expand on the CPU the way PSY-Z's ConvertRgb5551ToRgba8888 does. Alpha is
 * forced opaque: bit 15 is the PS1 mask, not coverage, and present does not
 * blend. Rows are packed tight because GLES2 has no core GL_UNPACK_ROW_LENGTH.
 */
#include "nx_vram_pack.h"

#include <stdint.h>

static uint8_t color_5to8(unsigned int c5) {
    return (uint8_t)((c5 * 255u + 15u) / 31u);
}

/* Shrink one axis into [0, limit). Uses a wide accumulator so a huge size
 * cannot wrap the far edge. */
static int clamp_axis(int origin, int size, int limit, int *out_origin,
                      int *out_size) {
    int64_t o = origin;
    int64_t s = size;

    if (s <= 0 || o >= limit) {
        *out_origin = origin;
        *out_size = 0;
        return 0;
    }
    if (o < 0) {
        s += o;
        o = 0;
        if (s <= 0) {
            *out_origin = 0;
            *out_size = 0;
            return 0;
        }
    }
    if (s > (int64_t)limit - o) {
        s = (int64_t)limit - o;
    }
    *out_origin = (int)o;
    *out_size = (int)s;
    return 1;
}

size_t dw3_nx_pack_rgb555_rect(const uint16_t *src, int dx, int dy, int dw,
                               int dh, uint8_t *dst, int *out_x, int *out_y,
                               int *out_w, int *out_h) {
    int x;
    int y;
    int w;
    int h;
    int row;

    if (!out_x || !out_y || !out_w || !out_h) {
        return 0;
    }
    *out_x = dx;
    *out_y = dy;
    *out_w = 0;
    *out_h = 0;
    if (!src || !dst || dw <= 0 || dh <= 0) {
        return 0;
    }
    if (!clamp_axis(dx, dw, DW3_NX_VRAM_W, &x, &w) ||
        !clamp_axis(dy, dh, DW3_NX_VRAM_H, &y, &h)) {
        return 0;
    }

    *out_x = x;
    *out_y = y;
    *out_w = w;
    *out_h = h;

    for (row = 0; row < h; row++) {
        const uint16_t *srow =
            src + ((size_t)(y + row) * (size_t)DW3_NX_VRAM_W) + (size_t)x;
        int col;

        for (col = 0; col < w; col++) {
            uint16_t v = srow[col];

            dst[0] = color_5to8(v & 0x1Fu);
            dst[1] = color_5to8((v >> 5) & 0x1Fu);
            dst[2] = color_5to8((v >> 10) & 0x1Fu);
            dst[3] = 0xFFu;
            dst += 4;
        }
    }
    return (size_t)w * (size_t)h;
}
