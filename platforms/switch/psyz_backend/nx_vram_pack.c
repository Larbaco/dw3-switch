/* PS1 VRAM is RGB555. GLES2's GL_UNSIGNED_SHORT_5_5_5_1 stores R in bits
 * 15–11, and this port has no GL_UNSIGNED_SHORT_1_5_5_5_REV. Uploading the
 * halfword raw scrambles every channel — that was the speckled title.
 * Expand on the CPU the way PSY-Z's ConvertRgb5551ToRgba8888 does. Alpha
 * carries bit 15 (0xFF set, 0x00 clear). It is not coverage: the present
 * shaders write alpha 1 and do not blend. The battle UI composite reads it to
 * tell a drawn black pixel from cleared VRAM (0x0000) inside the partner
 * view. switch_gpu.c sets the GPU mask bit while the partner model draws, so
 * every pixel it writes has bit 15, even a dark one that would round to
 * 0x0000. Rows are packed tight because GLES2
 * has no core GL_UNPACK_ROW_LENGTH.
 */
#include "nx_vram_pack.h"

#include <stdint.h>
#include <string.h>

static uint8_t color_5to8(unsigned int c5) {
    return (uint8_t)((c5 * 255u + 15u) / 31u);
}

/* P6.22 pass 2: the Cortex-A57 spent most of a present (about 2.8 ms of the
 * rc7 log's present_us) expanding 102000 pixels with three multiplies and
 * shifts each. One 256 KiB table from the halfword to its four output bytes
 * (the same bytes, built with color_5to8) makes it a load and a store per
 * pixel. Built on first use; the present runs on the game thread only. */
static uint32_t s_lut[65536];
static int s_lut_ready;

static void lut_init(void) {
    unsigned int v;

    for (v = 0; v < 65536u; v++) {
        uint8_t px[4];

        px[0] = color_5to8(v & 0x1Fu);
        px[1] = color_5to8((v >> 5) & 0x1Fu);
        px[2] = color_5to8((v >> 10) & 0x1Fu);
        px[3] = (v & 0x8000u) ? 0xFFu : 0x00u;
        memcpy(&s_lut[v], px, 4);
    }
    s_lut_ready = 1;
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

int dw3_nx_clamp_rect(int dx, int dy, int dw, int dh, int *out_x, int *out_y,
                      int *out_w, int *out_h) {
    if (!clamp_axis(dx, dw, DW3_NX_VRAM_W, out_x, out_w) ||
        !clamp_axis(dy, dh, DW3_NX_VRAM_H, out_y, out_h)) {
        *out_w = 0;
        *out_h = 0;
        return 0;
    }
    return 1;
}

int dw3_nx_wide_rect_ok(int stride, int gutter, int dx, int dy, int dw,
                        int dh) {
    return gutter >= 0 && gutter <= stride && dw > 0 && dh > 0 && dx >= 0 &&
           dy >= 0 && (int64_t)dx + dw <= DW3_NX_VRAM_W &&
           (int64_t)dy + dh <= DW3_NX_VRAM_H;
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
    if (!s_lut_ready) {
        lut_init();
    }
    if (!dw3_nx_clamp_rect(dx, dy, dw, dh, &x, &y, &w, &h)) {
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
            memcpy(dst, &s_lut[srow[col]], 4);
            dst += 4;
        }
    }
    return (size_t)w * (size_t)h;
}

static uint8_t *expand_row(const uint16_t *s, int n, uint8_t *dst) {
    int col;

    for (col = 0; col < n; col++) {
        memcpy(dst, &s_lut[s[col]], 4);
        dst += 4;
    }
    return dst;
}

size_t dw3_nx_pack_rgb555_wide(const uint16_t *src, const uint16_t *left,
                               const uint16_t *right, int stride, int gutter,
                               int dx, int dy, int dw, int dh, uint8_t *dst) {
    int row;

    if (!src || !left || !right || !dst ||
        !dw3_nx_wide_rect_ok(stride, gutter, dx, dy, dw, dh)) {
        return 0;
    }
    if (!s_lut_ready) {
        lut_init();
    }
    for (row = 0; row < dh; row++) {
        size_t y = (size_t)(dy + row);

        dst = expand_row(left + y * (size_t)stride, gutter, dst);
        dst = expand_row(src + y * DW3_NX_VRAM_W + (size_t)dx, dw, dst);
        dst = expand_row(right + y * (size_t)stride, gutter, dst);
    }
    return (size_t)(dw + 2 * gutter) * (size_t)dh;
}

size_t dw3_nx_copy16_rect(const uint16_t *src, int x, int y, int w, int h,
                          uint16_t *dst) {
    int row;

    if (!src || !dst || w <= 0 || h <= 0 || x < 0 || y < 0 ||
        (int64_t)x + w > DW3_NX_VRAM_W || (int64_t)y + h > DW3_NX_VRAM_H) {
        return 0;
    }
    for (row = 0; row < h; row++) {
        memcpy(dst + (size_t)row * (size_t)w,
               src + (size_t)(y + row) * DW3_NX_VRAM_W + (size_t)x,
               (size_t)w * sizeof(uint16_t));
    }
    return (size_t)w * (size_t)h;
}

size_t dw3_nx_copy16_wide(const uint16_t *src, const uint16_t *left,
                          const uint16_t *right, int stride, int gutter,
                          int dx, int dy, int dw, int dh, uint16_t *dst) {
    int row;

    if (!src || !left || !right || !dst ||
        !dw3_nx_wide_rect_ok(stride, gutter, dx, dy, dw, dh)) {
        return 0;
    }
    for (row = 0; row < dh; row++) {
        size_t y = (size_t)(dy + row);

        memcpy(dst, left + y * (size_t)stride, (size_t)gutter * 2u);
        dst += gutter;
        memcpy(dst, src + y * DW3_NX_VRAM_W + (size_t)dx, (size_t)dw * 2u);
        dst += dw;
        memcpy(dst, right + y * (size_t)stride, (size_t)gutter * 2u);
        dst += gutter;
    }
    return (size_t)(dw + 2 * gutter) * (size_t)dh;
}
