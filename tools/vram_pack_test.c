/* Host check for dw3_nx_pack_rgb555_rect. No GLES and no devkitPro.
 *
 * A known RGB555 rectangle, read with VRAM stride 1024, must match an
 * independent full-buffer expand of those same pixels.
 */
#include "nx_vram_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t expand5(unsigned int c5) {
    return (uint8_t)((c5 * 255u + 15u) / 31u);
}

static void expand_full(const uint16_t *src, uint8_t *dst) {
    size_t n = (size_t)DW3_NX_VRAM_W * (size_t)DW3_NX_VRAM_H;
    size_t i;

    for (i = 0; i < n; i++) {
        uint16_t v = src[i];

        dst[i * 4 + 0] = expand5(v & 0x1Fu);
        dst[i * 4 + 1] = expand5((v >> 5) & 0x1Fu);
        dst[i * 4 + 2] = expand5((v >> 10) & 0x1Fu);
        dst[i * 4 + 3] = (v & 0x8000u) ? 0xFFu : 0x00u;
    }
}

static int expect_rect(const uint16_t *vram, const uint8_t *full, uint8_t *packed,
                       int dx, int dy, int dw, int dh) {
    int ox = -1;
    int oy = -1;
    int ow = -1;
    int oh = -1;
    size_t n;
    size_t bytes;
    int row;
    int col;

    memset(packed, 0xA5, (size_t)dw * (size_t)dh * 4 + 4);
    n = dw3_nx_pack_rgb555_rect(vram, dx, dy, dw, dh, packed, &ox, &oy, &ow,
                                &oh);
    if (n != (size_t)dw * (size_t)dh || ox != dx || oy != dy || ow != dw ||
        oh != dh) {
        fprintf(stderr,
                "rect %d,%d %dx%d packed as %d,%d %dx%d (%zu pixels)\n", dx,
                dy, dw, dh, ox, oy, ow, oh, n);
        return 1;
    }
    bytes = n * 4;
    if (packed[bytes] != 0xA5) {
        fprintf(stderr, "rect %d,%d %dx%d wrote past the tight buffer\n", dx,
                dy, dw, dh);
        return 1;
    }
    for (row = 0; row < dh; row++) {
        for (col = 0; col < dw; col++) {
            size_t packed_at = ((size_t)row * (size_t)dw + (size_t)col) * 4;
            size_t full_at =
                (((size_t)dy + (size_t)row) * (size_t)DW3_NX_VRAM_W +
                 (size_t)dx + (size_t)col) *
                4;

            if (memcmp(packed + packed_at, full + full_at, 4) != 0) {
                fprintf(stderr,
                        "mismatch at vram %d,%d (rect %d,%d %dx%d): "
                        "packed %02x %02x %02x %02x full %02x %02x %02x %02x\n",
                        dx + col, dy + row, dx, dy, dw, dh, packed[packed_at],
                        packed[packed_at + 1], packed[packed_at + 2],
                        packed[packed_at + 3], full[full_at], full[full_at + 1],
                        full[full_at + 2], full[full_at + 3]);
                return 1;
            }
        }
    }
    return 0;
}

/* P6.1: gutter planes (stride 64) left and right of a 320-wide display. */
static int check_wide(const uint16_t *vram, const uint8_t *full) {
    enum { STRIDE = 64, GUT = 53, DW = 320, DH = 4, DY = 256 };
    static uint16_t left[DW3_NX_VRAM_H * STRIDE];
    static uint16_t right[DW3_NX_VRAM_H * STRIDE];
    static uint8_t wide[(DW + 2 * GUT) * DH * 4 + 4];
    size_t n;
    int row;

    /* Pure red left, pure blue right; the display comes from VRAM. */
    for (row = 0; row < DW3_NX_VRAM_H * STRIDE; row++) {
        left[row] = 0x001Fu;
        right[row] = 0x7C00u;
    }
    memset(wide, 0xA5, sizeof(wide));
    n = dw3_nx_pack_rgb555_wide(vram, left, right, STRIDE, GUT, 0, DY, DW, DH,
                                wide);
    if (n != (size_t)(DW + 2 * GUT) * DH || wide[n * 4] != 0xA5) {
        fprintf(stderr, "wide pack returned %zu pixels\n", n);
        return 1;
    }
    for (row = 0; row < DH; row++) {
        const uint8_t *line = wide + (size_t)row * (DW + 2 * GUT) * 4;
        const uint8_t *src =
            full + ((size_t)(DY + row) * DW3_NX_VRAM_W) * 4;

        if (line[0] != 0xFF || line[2] != 0 ||
            line[(GUT - 1) * 4] != 0xFF ||
            memcmp(line + GUT * 4, src, (size_t)DW * 4) != 0 ||
            line[(GUT + DW) * 4 + 2] != 0xFF ||
            line[(GUT + DW + GUT - 1) * 4 + 2] != 0xFF ||
            line[(GUT + DW) * 4] != 0) {
            fprintf(stderr, "wide pack row %d is not left|display|right\n",
                    row);
            return 1;
        }
    }
    if (dw3_nx_pack_rgb555_wide(vram, left, right, STRIDE, STRIDE + 1, 0, 0, DW,
                                DH, wide) != 0 ||
        dw3_nx_pack_rgb555_wide(vram, left, right, STRIDE, GUT, 900, 0, DW, DH,
                                wide) != 0) {
        fprintf(stderr, "wide pack accepted an invalid size\n");
        return 1;
    }
    return 0;
}

int main(void) {
    size_t pixels = (size_t)DW3_NX_VRAM_W * (size_t)DW3_NX_VRAM_H;
    uint16_t *vram = calloc(pixels, sizeof(uint16_t));
    uint8_t *full = malloc(pixels * 4);
    /* Title width is 320; leave room past a clamped edge plus a canary. */
    uint8_t *packed = malloc((size_t)320 * 32 * 4 + 4);
    int x;
    int y;
    int ox, oy, ow, oh;
    size_t n;
    int failed = 0;

    if (!vram || !full || !packed) {
        fprintf(stderr, "alloc failed\n");
        free(vram);
        free(full);
        free(packed);
        return 1;
    }

    for (y = 0; y < DW3_NX_VRAM_H; y++) {
        for (x = 0; x < DW3_NX_VRAM_W; x++) {
            unsigned int r = (unsigned int)x & 0x1Fu;
            unsigned int g = (unsigned int)y & 0x1Fu;
            unsigned int b =
                ((unsigned int)(x >> 5) ^ (unsigned int)(y >> 5)) & 0x1Fu;

            /* Bit 15 on alternate columns: it becomes alpha 0xFF / 0x00. */
            vram[(size_t)y * (size_t)DW3_NX_VRAM_W + (size_t)x] =
                (uint16_t)(r | (g << 5) | (b << 10) |
                           ((x & 1) ? 0u : 0x8000u));
        }
    }
    /* Known mid-grey at a non-zero origin: c5=16 → 132, alpha 0xFF. */
    vram[(size_t)36 * (size_t)DW3_NX_VRAM_W + 32] =
        (uint16_t)(16u | (16u << 5) | (16u << 10) | 0x8000u);

    expand_full(vram, full);

    failed |= expect_rect(vram, full, packed, 32, 36, 320, 16);
    /* First pixel of that rect is the known mid-grey halfword. */
    if (packed[0] != 132 || packed[1] != 132 || packed[2] != 132 ||
        packed[3] != 0xFF) {
        fprintf(stderr, "known pixel at 32,36 expanded to %02x %02x %02x %02x\n",
                packed[0], packed[1], packed[2], packed[3]);
        failed = 1;
    }
    failed |= expect_rect(vram, full, packed, 0, 0, 320, 8);
    failed |= expect_rect(vram, full, packed, 0, 256, 320, 8);

    memset(packed, 0xA5, 64);
    n = dw3_nx_pack_rgb555_rect(vram, 1000, 500, 50, 40, packed, &ox, &oy, &ow,
                                &oh);
    if (n != 24u * 12u || ox != 1000 || oy != 500 || ow != 24 || oh != 12) {
        fprintf(stderr, "clamp packed as %d,%d %dx%d (%zu pixels)\n", ox, oy,
                ow, oh, n);
        failed = 1;
    } else if (expect_rect(vram, full, packed, ox, oy, ow, oh) != 0) {
        failed = 1;
    }

    n = dw3_nx_pack_rgb555_rect(vram, 0, 0, 0, 16, packed, &ox, &oy, &ow, &oh);
    if (n != 0 || ow != 0) {
        fprintf(stderr, "zero width packed %zu pixels\n", n);
        failed = 1;
    }
    n = dw3_nx_pack_rgb555_rect(vram, 0, 0, 16, -4, packed, &ox, &oy, &ow,
                                &oh);
    if (n != 0 || oh != 0) {
        fprintf(stderr, "negative height packed %zu pixels\n", n);
        failed = 1;
    }

    /* Every halfword, in one row: the pack is a table lookup now. */
    for (x = 0; x < 1024; x++) {
        unsigned int v;
        int rep;

        for (rep = 0; rep < 64; rep++) {
            v = (unsigned int)(rep * 1024 + x);
            vram[(size_t)(100 + rep) * DW3_NX_VRAM_W + (size_t)x] = (uint16_t)v;
        }
    }
    expand_full(vram, full);
    for (y = 100; y < 164; y += 32) {
        failed |= expect_rect(vram, full, packed, 0, y, 320, 32);
        failed |= expect_rect(vram, full, packed, 320, y, 320, 32);
        failed |= expect_rect(vram, full, packed, 640, y, 320, 32);
        failed |= expect_rect(vram, full, packed, 704, y, 320, 32);
    }

    failed |= check_wide(vram, full);

    free(vram);
    free(full);
    free(packed);
    if (failed) {
        return 1;
    }
    printf("OK: rgb555 rect pack matches full-buffer expand\n");
    printf("OK: rgb555 wide pack puts the gutters beside the display\n");
    return 0;
}
