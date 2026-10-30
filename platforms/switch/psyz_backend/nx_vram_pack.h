/* RGB555 → tight RGBA8888 pack for the GLES2 VRAM upload.
 * No GL types: the host test compiles this without devkitPro or GLES. */
#ifndef DW3_NX_VRAM_PACK_H
#define DW3_NX_VRAM_PACK_H

#include <stddef.h>
#include <stdint.h>

#define DW3_NX_VRAM_W 1024
#define DW3_NX_VRAM_H 512

/* Expand the VRAM rectangle (dx, dy, dw, dh) into dst.
 *
 * src is RGB555 with a row stride of DW3_NX_VRAM_W pixels: R in bits 0–4,
 * G in 5–9, B in 10–14. Each color channel expands with
 * (c5 * 255 + 15) / 31. Alpha carries the PS1 mask (bit 15): 0xFF when
 * set, 0x00 otherwise. This metadata is not coverage; present shaders write
 * opaque alpha, while the battle UI composite uses it for drawn black pixels.
 *
 * The rectangle is clamped to 1024×512. dst receives out_w pixels per row
 * and out_h rows, tightly packed (GLES2 has no core GL_UNPACK_ROW_LENGTH).
 * Returns the packed pixel count (out_w * out_h), or 0 when the clamped
 * size is empty. out_x/out_y/out_w/out_h receive the clamped origin and size.
 */
size_t dw3_nx_pack_rgb555_rect(const uint16_t *src, int dx, int dy, int dw,
                               int dh, uint8_t *dst, int *out_x, int *out_y,
                               int *out_w, int *out_h);

/* Clamp the rectangle to 1024x512 as the pack above does. Returns 1 and the
 * clamped origin and size, or 0 (out_w = out_h = 0) when nothing is left. The
 * 16-bit present (P6.22 pass 4) uses it to upload the same pixels straight from
 * VRAM. */
int dw3_nx_clamp_rect(int dx, int dy, int dw, int dh, int *out_x, int *out_y,
                      int *out_w, int *out_h);

/* 1 when (dx, dy, dw, dh) lies inside VRAM and 0 <= gutter <= stride, the
 * conditions dw3_nx_pack_rgb555_wide needs for the display with gutters. */
int dw3_nx_wide_rect_ok(int stride, int gutter, int dx, int dy, int dw,
                        int dh);

/* P6.1: the display rectangle with its side gutters, (dw + 2 * gutter) × dh
 * pixels tightly packed: `gutter` columns of `left`, the VRAM rectangle,
 * `gutter` columns of `right`. The planes hold one row per VRAM row with a
 * stride of `stride` pixels. The rectangle must lie inside VRAM; returns the
 * packed pixel count, or 0 when it does not or the sizes are invalid. */
size_t dw3_nx_pack_rgb555_wide(const uint16_t *src, const uint16_t *left,
                               const uint16_t *right, int stride, int gutter,
                               int dx, int dy, int dw, int dh, uint8_t *dst);

#endif
