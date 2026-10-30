/* P6.32 phase 0: pure parts of the full-screen map view. See the header.
 *
 * The AMD FSR 1 part (EASU and RCAS, dw3_mv_fsr_*) is a CPU port of
 * FidelityFX Super Resolution 1.0 (ffx_fsr1.h):
 *
 * Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * The code below follows the structure and constants of the AMD source (the
 * 12-tap EASU kernel with its gradient direction and length, the Lanczos
 * approximation, and RCAS with its limiter); the reciprocal and square root
 * approximations of the original are exact operations here. */
#include "map_view_core.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern int dw3_pc_title_logo_resample(const uint8_t *src, int sw, int sh,
                                      int spitch, uint8_t *dst, int dw,
                                      int dh, int dpitch, int filter);

static double now_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

/* --- scaler names --------------------------------------------------------*/

const char *dw3_mv_scaler_name(int scaler) {
    switch (scaler) {
    case MV_SCALER_NEAREST: return "NEAREST";
    case MV_SCALER_SHARP: return "SHARP BILINEAR";
    case MV_SCALER_LANCZOS2: return "LANCZOS2";
    case MV_SCALER_FSR: return "AMD FSR 1";
    case MV_SCALER_BILINEAR: return "BILINEAR";
    default: return "?";
    }
}

int dw3_mv_scaler_linear(int scaler) { return scaler != MV_SCALER_NEAREST; }

int dw3_mv_scaler_step(int scaler, int dir) {
    int n = scaler + (dir < 0 ? -1 : 1);

    if (n < 0) n = MV_SCALER_COUNT - 1;
    if (n >= MV_SCALER_COUNT) n = 0;
    return n;
}

void dw3_mv_image_free(MvImage *img) {
    if (img) {
        free(img->rgba);
        img->rgba = NULL;
        img->w = img->h = 0;
    }
}

/* --- compose -------------------------------------------------------------*/

typedef struct MvFrame {
    int count, row, abr;
    const int16_t *parts; /* count * 3: part index, x, y */
} MvFrame;

typedef struct MvPart {
    int16_t u, v, w, h, clut_x, clut_y, mode;
} MvPart;

static int rd32(const uint8_t *p) {
    return (int)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                 ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static int16_t rd16(const uint8_t *p) {
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* The sheet is little-endian data in memory; read it bytewise so a host with
 * other alignment rules is safe. frame_hdr is the offset of the frame list. */
static int find_frame(const uint8_t *sheet, size_t len, int frame,
                      size_t *frame_off, size_t *parts_off) {
    size_t ids_off, i;

    if (!sheet || len < 16) return -1;
    *parts_off = (size_t)rd32(sheet);
    ids_off = (size_t)rd32(sheet + 4);
    if (*parts_off >= len || ids_off >= len) return -1;
    for (i = 0; i < 64 && ids_off + i < len; ++i) {
        int id = sheet[ids_off + i];

        if (id == 0xff) return -1;
        if (id == frame) {
            int off = rd32(sheet + 8 + 4 * i);

            if (off <= 0 || (size_t)off + 6 > len) return -1;
            *frame_off = (size_t)off;
            return 0;
        }
    }
    return -1;
}

int dw3_mv_sheet_frame_box(const int32_t *sheet, size_t sheet_len, int frame,
                           MvBox *box) {
    const uint8_t *b = (const uint8_t *)sheet;
    size_t foff, poff;
    int count, i;

    if (find_frame(b, sheet_len, frame, &foff, &poff) != 0) return -1;
    count = rd16(b + foff);
    if (count <= 0 || count > 64 || foff + 6 + (size_t)count * 6 > sheet_len)
        return -1;
    box->x0 = box->y0 = 1 << 20;
    box->x1 = box->y1 = -(1 << 20);
    for (i = 0; i < count; ++i) {
        const uint8_t *p = b + foff + 6 + (size_t)i * 6;
        int pi = rd16(p), px = rd16(p + 2), py = rd16(p + 4);
        const uint8_t *part;

        if (pi < 0 || poff + (size_t)(pi + 1) * 14 > sheet_len) return -1;
        part = b + poff + (size_t)pi * 14;
        if (px < box->x0) box->x0 = px;
        if (py < box->y0) box->y0 = py;
        if (px + rd16(part + 4) > box->x1) box->x1 = px + rd16(part + 4);
        if (py + rd16(part + 6) > box->y1) box->y1 = py + rd16(part + 6);
    }
    return 0;
}

static uint8_t expand5(unsigned v) { return (uint8_t)((v * 255u + 15u) / 31u); }

/* Draws every part of the frame at foff into px (w x h), the frame's origin
 * landing on pixel (org_x, org_y). transparent: VRAM colour 0 is skipped (the
 * PS1 sprite rule), as for the town and area cells over the map; 0 writes it
 * (the map's own frame, which has no holes). The parts go in list order: the
 * game submits them last to first, and a later primitive of one OT entry is
 * drawn first, so the first part is the bottom one. */
static void draw_frame(const uint16_t *vram, const uint8_t *b, size_t foff,
                       size_t poff, int tex_x, int tex_y, int alt_x, int alt_y,
                       int org_x, int org_y, uint8_t *px, int w, int h,
                       int transparent) {
    int count = rd16(b + foff), row = rd16(b + foff + 2), i;

    for (i = 0; i < count; ++i) {
        const uint8_t *p = b + foff + 6 + (size_t)i * 6;
        int pi = rd16(p), dx = rd16(p + 2), dy = rd16(p + 4);
        const uint8_t *part = b + poff + (size_t)pi * 14;
        int pu = rd16(part), pv = rd16(part + 2), pw = rd16(part + 4);
        int ph = rd16(part + 6), pcx = rd16(part + 8), pcy = rd16(part + 10);
        int mode = rd16(part + 12) ? 1 : 0;
        int shift = mode ? 1 : 2;
        int mask = (1 << shift) - 1;
        int bits = 16 >> shift;
        int source_u = mode ? (pu & 0x7f) : (pu & 0xff);
        int page_x = ((tex_x + (pu >> shift)) / 64) * 64;
        int pal_x = ((mode ? alt_x : tex_x) + pcx) / 16 * 16;
        int pal_y = (mode ? alt_y - 0x100 : tex_y) + pcy + row;
        int y, x;

        if (pal_x < 0 || pal_x >= MV_VRAM_W || pal_y < 0 || pal_y >= MV_VRAM_H)
            continue;
        for (y = 0; y < ph; ++y) {
            int oy = org_y + dy + y;

            if (oy < 0 || oy >= h) continue;
            for (x = 0; x < pw; ++x) {
                int ox = org_x + dx + x;
                int sx = source_u + x;
                int wx, wy, idx;
                unsigned packed, color;
                uint8_t *d;

                if (ox < 0 || ox >= w) continue;
                wx = (page_x + (sx >> shift)) % MV_VRAM_W;
                wy = (tex_y + pv + y) % MV_VRAM_H;
                packed = vram[wy * MV_VRAM_W + wx];
                idx = (int)((packed >> ((sx & mask) * bits)) &
                            ((1u << bits) - 1u));
                color = vram[pal_y * MV_VRAM_W + ((pal_x + idx) % MV_VRAM_W)];
                if (transparent && (color & 0xffffu) == 0) continue;
                d = px + ((size_t)oy * (size_t)w + (size_t)ox) * 4;
                d[0] = expand5(color & 31u);
                d[1] = expand5((color >> 5) & 31u);
                d[2] = expand5((color >> 10) & 31u);
                d[3] = 255;
            }
        }
    }
}

int dw3_mv_compose(const uint16_t *vram, const int32_t *sheet,
                   size_t sheet_len, int frame, int tex_x, int tex_y,
                   int alt_x, int alt_y, MvImage *out, MvBox *box_out) {
    const uint8_t *b = (const uint8_t *)sheet;
    size_t foff, poff;
    MvBox box;
    int i, w, h;
    uint8_t *px;

    out->rgba = NULL;
    out->w = out->h = 0;
    out->ox = out->oy = 0;
    out->reveal = 0;
    if (!vram || dw3_mv_sheet_frame_box(sheet, sheet_len, frame, &box) != 0)
        return -1;
    if (find_frame(b, sheet_len, frame, &foff, &poff) != 0) return -1;
    w = box.x1 - box.x0;
    h = box.y1 - box.y0;
    if (w <= 0 || h <= 0 || w > 2048 || h > 2048) return -1;
    px = (uint8_t *)calloc((size_t)w * (size_t)h, 4);
    if (!px) return -1;
    draw_frame(vram, b, foff, poff, tex_x, tex_y, alt_x, alt_y, -box.x0,
               -box.y0, px, w, h, 0);
    /* Texels no part covers stay black but opaque. */
    for (i = 0; i < w * h; ++i) px[(size_t)i * 4 + 3] = 255;
    out->w = w;
    out->h = h;
    out->ox = box.x0;
    out->oy = box.y0;
    out->rgba = px;
    if (box_out) *box_out = box;
    return 0;
}

int dw3_mv_blit_frame(const uint16_t *vram, const int32_t *sheet,
                      size_t sheet_len, int frame, int tex_x, int tex_y,
                      int alt_x, int alt_y, MvImage *dst, int org_x,
                      int org_y) {
    const uint8_t *b = (const uint8_t *)sheet;
    size_t foff, poff;
    int count;

    if (!vram || !dst || !dst->rgba) return -1;
    if (find_frame(b, sheet_len, frame, &foff, &poff) != 0) return -1;
    count = rd16(b + foff);
    if (count <= 0 || count > 64 || foff + 6 + (size_t)count * 6 > sheet_len)
        return -1;
    draw_frame(vram, b, foff, poff, tex_x, tex_y, alt_x, alt_y, org_x, org_y,
               dst->rgba, dst->w, dst->h, 1);
    return 0;
}

uint32_t dw3_mv_hash(const void *data, size_t len, uint32_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t h = seed ? seed : 2166136261u;
    size_t i;

    for (i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

/* --- disc ----------------------------------------------------------------*/

#define MV_RLEN_MAGIC 0x4E454C52u

static int place_tim(const uint8_t *tim, size_t avail, int image_x, int image_y,
                     int clut_x, int clut_y, uint16_t *vram) {
    uint32_t flag;
    size_t pos = 8;
    int mode, has_clut;

    if (avail < 20) return -1;
    flag = (uint32_t)rd32(tim + 4);
    mode = (int)(flag & 7u);
    has_clut = (flag & 8u) != 0;
    if (has_clut) {
        uint32_t size = (uint32_t)rd32(tim + pos);
        int w = (int)(uint16_t)rd16(tim + pos + 8);
        int h = (int)(uint16_t)rd16(tim + pos + 10);
        int x, y;

        if (pos + 12 + (size_t)w * h * 2 > avail) return -1;
        if (mode == 0 || mode == 1) {
            for (y = 0; y < h; ++y) {
                for (x = 0; x < w; ++x) {
                    int vx = clut_x + x, vy = clut_y + y;

                    if (vx < MV_VRAM_W && vy < MV_VRAM_H) {
                        vram[vy * MV_VRAM_W + vx] = (uint16_t)rd16(
                            tim + pos + 12 + ((size_t)y * w + x) * 2);
                    }
                }
            }
        }
        pos += size;
    }
    if (pos + 12 > avail) return -1;
    {
        int w = (int)(uint16_t)rd16(tim + pos + 8);
        int h = (int)(uint16_t)rd16(tim + pos + 10);
        int x, y;

        if (pos + 12 + (size_t)w * h * 2 > avail) return -1;
        for (y = 0; y < h; ++y) {
            for (x = 0; x < w; ++x) {
                int vx = image_x + x, vy = image_y + y;

                if (vx < MV_VRAM_W && vy < MV_VRAM_H) {
                    vram[vy * MV_VRAM_W + vx] = (uint16_t)rd16(
                        tim + pos + 12 + ((size_t)y * w + x) * 2);
                }
            }
        }
    }
    return 0;
}

int dw3_mv_disc_load_entry(const uint8_t *file, size_t len, int entry,
                           int image_x, int image_y, uint16_t *vram) {
    const uint8_t *arch;
    uint8_t *buf;
    int placed = 0, i;

    if (!file || !vram || len < 16 || entry < 0 || entry > 2) return -1;
    if ((size_t)rd32(file + 4 * entry) <= 0 ||
        (size_t)rd32(file + 4 * entry) >= len) return -1;
    arch = file + rd32(file + 4 * entry); /* the entry's TIM archive */
    buf = (uint8_t *)malloc(0x10000);
    if (!buf) return -1;
    for (i = 0; i < 16; ++i) {
        int off = rd32(arch + 4 * i);
        const uint8_t *data;
        size_t avail;
        const uint8_t *tim;

        if (off == 0) break; /* the table ends with 0: the archive itself */
        if (off < 0 || (size_t)(arch - file) + (size_t)off + 16 > len) {
            placed = -1;
            break;
        }
        data = arch + off;
        avail = len - (size_t)(data - file);
        tim = data;
        if ((uint32_t)rd32(data) == MV_RLEN_MAGIC) {
            const uint8_t *src = data + 8;
            const uint8_t *end = data + avail;
            size_t n = 0;

            while (src < end && *src != 0) {
                int c = *src;

                if (c & 0x80) {
                    int run = c & 0x7f, k;

                    if (src + 1 >= end || n + (size_t)run > 0x10000) break;
                    for (k = 0; k < run; ++k) buf[n++] = src[1];
                    src += 2;
                } else {
                    if (src + 1 + c > end || n + (size_t)c > 0x10000) break;
                    memcpy(buf + n, src + 1, (size_t)c);
                    n += (size_t)c;
                    src += 1 + c;
                }
            }
            tim = buf;
            avail = n;
        }
        if (place_tim(tim, avail, image_x, image_y, 0x140, 0x100, vram) != 0) {
            placed = -1;
            break;
        }
        image_x += 0x40;
        ++placed;
    }
    free(buf);
    return placed;
}

int dw3_mv_disc_load_map_vram(const uint8_t *file, size_t len,
                              uint16_t *vram) {
    return dw3_mv_disc_load_entry(file, len, 0, 0x280, 0, vram);
}

/* --- scalers -------------------------------------------------------------*/

static int alloc_image(MvImage *img, int w, int h) {
    img->rgba = (uint8_t *)malloc((size_t)w * (size_t)h * 4);
    img->w = w;
    img->h = h;
    img->ox = img->oy = 0;
    img->reveal = 0;
    if (!img->rgba) {
        img->w = img->h = 0;
        return -1;
    }
    return 0;
}

static int scale_sharp(const MvImage *src, MvImage *dst) {
    int x, y, k;

    if (alloc_image(dst, src->w * MV_SCALE, src->h * MV_SCALE) != 0) return -1;
    for (y = 0; y < dst->h; ++y) {
        const uint8_t *srow = src->rgba + (size_t)(y / MV_SCALE) * src->w * 4;
        uint8_t *drow = dst->rgba + (size_t)y * dst->w * 4;

        for (x = 0; x < dst->w; ++x) {
            for (k = 0; k < 4; ++k) drow[x * 4 + k] = srow[(x / MV_SCALE) * 4 + k];
        }
    }
    return 0;
}

static int scale_lanczos(const MvImage *src, MvImage *dst) {
    if (alloc_image(dst, src->w * MV_SCALE, src->h * MV_SCALE) != 0) return -1;
    if (dw3_pc_title_logo_resample(src->rgba, src->w, src->h, src->w * 4,
                                   dst->rgba, dst->w, dst->h, dst->w * 4,
                                   1 /* DW3_PC_LOGO_LANCZOS2 */) != 0) {
        dw3_mv_image_free(dst);
        return -1;
    }
    return 0;
}

static float fclampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static float luma2(const float *c) { return c[2] * 0.5f + (c[0] * 0.5f + c[1]); }

static void texel(const MvImage *img, int x, int y, float *c) {
    const uint8_t *p;

    if (x < 0) x = 0;
    if (x >= img->w) x = img->w - 1;
    if (y < 0) y = 0;
    if (y >= img->h) y = img->h - 1;
    p = img->rgba + ((size_t)y * (size_t)img->w + (size_t)x) * 4;
    c[0] = (float)p[0] * (1.0f / 255.0f);
    c[1] = (float)p[1] * (1.0f / 255.0f);
    c[2] = (float)p[2] * (1.0f / 255.0f);
}

/* One bilinear-weighted '+' of the EASU gradient (FsrEasuSetF). */
static void easu_set(float *dir, float *len, float ppx, float ppy, int which,
                     float la, float lb, float lc, float ld, float le) {
    float w = 0.0f, dc, cb, len_x, dir_x, ec, ca, len_y, dir_y;

    switch (which) {
    case 0: w = (1.0f - ppx) * (1.0f - ppy); break;
    case 1: w = ppx * (1.0f - ppy); break;
    case 2: w = (1.0f - ppx) * ppy; break;
    default: w = ppx * ppy; break;
    }
    dc = ld - lc;
    cb = lc - lb;
    len_x = fabsf(dc) > fabsf(cb) ? fabsf(dc) : fabsf(cb);
    len_x = len_x > 0.0f ? 1.0f / len_x : 0.0f;
    dir_x = ld - lb;
    dir[0] += dir_x * w;
    len_x = fclampf(fabsf(dir_x) * len_x, 0.0f, 1.0f);
    len_x *= len_x;
    *len += len_x * w;
    ec = le - lc;
    ca = lc - la;
    len_y = fabsf(ec) > fabsf(ca) ? fabsf(ec) : fabsf(ca);
    len_y = len_y > 0.0f ? 1.0f / len_y : 0.0f;
    dir_y = le - la;
    dir[1] += dir_y * w;
    len_y = fclampf(fabsf(dir_y) * len_y, 0.0f, 1.0f);
    len_y *= len_y;
    *len += len_y * w;
}

/* One tap of the EASU kernel (FsrEasuTapF). */
static void easu_tap(float *ac, float *aw, float ox, float oy, const float *dir,
                     float lenx, float leny, float lob, float clp,
                     const float *c) {
    float vx = ox * dir[0] + oy * dir[1];
    float vy = ox * -dir[1] + oy * dir[0];
    float d2, wb, wa, w;

    vx *= lenx;
    vy *= leny;
    d2 = vx * vx + vy * vy;
    if (d2 > clp) d2 = clp;
    wb = 2.0f / 5.0f * d2 - 1.0f;
    wa = lob * d2 - 1.0f;
    wb *= wb;
    wa *= wa;
    wb = 25.0f / 16.0f * wb - (25.0f / 16.0f - 1.0f);
    w = wb * wa;
    ac[0] += c[0] * w;
    ac[1] += c[1] * w;
    ac[2] += c[2] * w;
    *aw += w;
}

void dw3_mv_fsr_easu(const MvImage *src, MvImage *dst) {
    const float sx = (float)src->w / (float)dst->w;
    const float sy = (float)src->h / (float)dst->h;
    int ox, oy;

    for (oy = 0; oy < dst->h; ++oy) {
        for (ox = 0; ox < dst->w; ++ox) {
            float ipx = ((float)ox + 0.5f) * sx - 0.5f;
            float ipy = ((float)oy + 0.5f) * sy - 0.5f;
            int px = (int)floorf(ipx), py = (int)floorf(ipy);
            float fx = ipx - (float)px, fy = ipy - (float)py;
            float b[3], c[3], e[3], f[3], g[3], h[3], i[3], j[3], k[3], l[3];
            float n[3], o[3];
            float lb, lc, le, lf, lg, lh, li, lj, lk, ll, ln, lo;
            float dir[2], len, dirr, lenx, leny, stretch, lob, clp, lenh;
            float ac[3], aw, min4[3], max4[3];
            uint8_t *d;
            int ch;

            texel(src, px, py - 1, b);     texel(src, px + 1, py - 1, c);
            texel(src, px - 1, py, e);     texel(src, px, py, f);
            texel(src, px + 1, py, g);     texel(src, px + 2, py, h);
            texel(src, px - 1, py + 1, i); texel(src, px, py + 1, j);
            texel(src, px + 1, py + 1, k); texel(src, px + 2, py + 1, l);
            texel(src, px, py + 2, n);     texel(src, px + 1, py + 2, o);
            lb = luma2(b); lc = luma2(c); le = luma2(e); lf = luma2(f);
            lg = luma2(g); lh = luma2(h); li = luma2(i); lj = luma2(j);
            lk = luma2(k); ll = luma2(l); ln = luma2(n); lo = luma2(o);

            dir[0] = dir[1] = 0.0f;
            len = 0.0f;
            easu_set(dir, &len, fx, fy, 0, lb, le, lf, lg, lj);
            easu_set(dir, &len, fx, fy, 1, lc, lf, lg, lh, lk);
            easu_set(dir, &len, fx, fy, 2, lf, li, lj, lk, ln);
            easu_set(dir, &len, fx, fy, 3, lg, lj, lk, ll, lo);
            dirr = dir[0] * dir[0] + dir[1] * dir[1];
            if (dirr < 1.0f / 32768.0f) {
                dirr = 1.0f;
                dir[0] = 1.0f;
            } else {
                dirr = 1.0f / sqrtf(dirr);
            }
            dir[0] *= dirr;
            dir[1] *= dirr;
            len = len * 0.5f;
            len *= len;
            lenh = fabsf(dir[0]) > fabsf(dir[1]) ? fabsf(dir[0]) : fabsf(dir[1]);
            stretch = (dir[0] * dir[0] + dir[1] * dir[1]) *
                      (lenh > 0.0f ? 1.0f / lenh : 0.0f);
            lenx = 1.0f + (stretch - 1.0f) * len;
            leny = 1.0f - 0.5f * len;
            lob = 0.5f + ((1.0f / 4.0f - 0.04f) - 0.5f) * len;
            clp = 1.0f / lob;
            for (ch = 0; ch < 3; ++ch) {
                float a = f[ch] < g[ch] ? f[ch] : g[ch];
                float m = j[ch] < k[ch] ? j[ch] : k[ch];
                float A = f[ch] > g[ch] ? f[ch] : g[ch];
                float M = j[ch] > k[ch] ? j[ch] : k[ch];

                min4[ch] = a < m ? a : m;
                max4[ch] = A > M ? A : M;
            }
            ac[0] = ac[1] = ac[2] = 0.0f;
            aw = 0.0f;
            easu_tap(ac, &aw, 0.0f - fx, -1.0f - fy, dir, lenx, leny, lob, clp, b);
            easu_tap(ac, &aw, 1.0f - fx, -1.0f - fy, dir, lenx, leny, lob, clp, c);
            easu_tap(ac, &aw, -1.0f - fx, 1.0f - fy, dir, lenx, leny, lob, clp, i);
            easu_tap(ac, &aw, 0.0f - fx, 1.0f - fy, dir, lenx, leny, lob, clp, j);
            easu_tap(ac, &aw, 0.0f - fx, 0.0f - fy, dir, lenx, leny, lob, clp, f);
            easu_tap(ac, &aw, -1.0f - fx, 0.0f - fy, dir, lenx, leny, lob, clp, e);
            easu_tap(ac, &aw, 1.0f - fx, 1.0f - fy, dir, lenx, leny, lob, clp, k);
            easu_tap(ac, &aw, 2.0f - fx, 1.0f - fy, dir, lenx, leny, lob, clp, l);
            easu_tap(ac, &aw, 2.0f - fx, 0.0f - fy, dir, lenx, leny, lob, clp, h);
            easu_tap(ac, &aw, 1.0f - fx, 0.0f - fy, dir, lenx, leny, lob, clp, g);
            easu_tap(ac, &aw, 1.0f - fx, 2.0f - fy, dir, lenx, leny, lob, clp, o);
            easu_tap(ac, &aw, 0.0f - fx, 2.0f - fy, dir, lenx, leny, lob, clp, n);
            d = dst->rgba + ((size_t)oy * (size_t)dst->w + (size_t)ox) * 4;
            for (ch = 0; ch < 3; ++ch) {
                float v = aw != 0.0f ? ac[ch] / aw : f[ch];
                float lo_ = v > min4[ch] ? v : min4[ch];

                v = lo_ < max4[ch] ? lo_ : max4[ch];
                d[ch] = (uint8_t)(fclampf(v, 0.0f, 1.0f) * 255.0f + 0.5f);
            }
            d[3] = 255;
        }
    }
}

void dw3_mv_fsr_rcas(MvImage *img, float attenuation) {
    const float con = exp2f(-attenuation);
    const float limit = 0.25f - 1.0f / 16.0f; /* FSR_RCAS_LIMIT */
    const size_t pitch = (size_t)img->w * 4;
    uint8_t *orig = (uint8_t *)malloc(pitch * (size_t)img->h);
    int x, y;

    if (!orig) return;
    memcpy(orig, img->rgba, pitch * (size_t)img->h);
    for (y = 0; y < img->h; ++y) {
        int yu = y > 0 ? y - 1 : 0, yd = y < img->h - 1 ? y + 1 : y;
        uint8_t *out = img->rgba + (size_t)y * pitch;

        for (x = 0; x < img->w; ++x) {
            int xl = x > 0 ? x - 1 : 0, xr = x < img->w - 1 ? x + 1 : x;
            const uint8_t *taps[4];
            const uint8_t *pe = orig + (size_t)y * pitch + (size_t)x * 4;
            float t[4][3], ec[3], lobe, nz, nzv;
            float lb, ld, le, lf, lh, mx, mn, lobes[3], rcp_l;
            float mn4[3], mx4[3];
            int ch, q;

            taps[0] = orig + (size_t)yu * pitch + (size_t)x * 4;  /* b */
            taps[1] = orig + (size_t)y * pitch + (size_t)xl * 4;  /* d */
            taps[2] = orig + (size_t)y * pitch + (size_t)xr * 4;  /* f */
            taps[3] = orig + (size_t)yd * pitch + (size_t)x * 4;  /* h */
            for (q = 0; q < 4; ++q) {
                for (ch = 0; ch < 3; ++ch)
                    t[q][ch] = (float)taps[q][ch] * (1.0f / 255.0f);
            }
            for (ch = 0; ch < 3; ++ch) ec[ch] = (float)pe[ch] * (1.0f / 255.0f);
            lb = luma2(t[0]); ld = luma2(t[1]); lf = luma2(t[2]);
            lh = luma2(t[3]); le = luma2(ec);
            nz = 0.25f * lb + 0.25f * ld + 0.25f * lf + 0.25f * lh - le;
            mx = fmaxf(fmaxf(fmaxf(lb, ld), fmaxf(le, lf)), lh);
            mn = fminf(fminf(fminf(lb, ld), fminf(le, lf)), lh);
            nzv = (mx - mn) > 1.0e-6f ? fabsf(nz) / (mx - mn) : 0.0f;
            nzv = 1.0f - 0.5f * fclampf(nzv, 0.0f, 1.0f);
            for (ch = 0; ch < 3; ++ch) {
                float hit_min, hit_max;

                mn4[ch] = fminf(fminf(t[0][ch], t[1][ch]), fminf(t[2][ch], t[3][ch]));
                mx4[ch] = fmaxf(fmaxf(t[0][ch], t[1][ch]), fmaxf(t[2][ch], t[3][ch]));
                hit_min = mn4[ch] / (4.0f * mx4[ch] + 1.0e-6f);
                hit_max = (1.0f - mx4[ch]) / (4.0f * mn4[ch] - 4.0f - 1.0e-6f);
                lobes[ch] = fmaxf(-hit_min, hit_max);
            }
            lobe = fmaxf(lobes[0], fmaxf(lobes[1], lobes[2]));
            lobe = fmaxf(-limit, fminf(lobe, 0.0f)) * con * nzv;
            rcp_l = 1.0f / (4.0f * lobe + 1.0f);
            for (ch = 0; ch < 3; ++ch) {
                float v = (lobe * t[0][ch] + lobe * t[1][ch] +
                           lobe * t[3][ch] + lobe * t[2][ch] + ec[ch]) * rcp_l;

                out[x * 4 + ch] =
                    (uint8_t)(fclampf(v, 0.0f, 1.0f) * 255.0f + 0.5f);
            }
        }
    }
    free(orig);
}

static int scale_fsr(const MvImage *src, MvImage *dst) {
    if (alloc_image(dst, src->w * MV_SCALE, src->h * MV_SCALE) != 0) return -1;
    dw3_mv_fsr_easu(src, dst);
    dw3_mv_fsr_rcas(dst, 0.2f);
    return 0;
}

int dw3_mv_scale(int scaler, const MvImage *src, MvImage *dst, double *ms) {
    double t0 = now_ms();
    int rc;

    if (!src || !src->rgba || !dst) return -1;
    dst->rgba = NULL;
    dst->w = dst->h = 0;
    dst->ox = dst->oy = 0;
    dst->reveal = 0;
    switch (scaler) {
    case MV_SCALER_SHARP: rc = scale_sharp(src, dst); break;
    case MV_SCALER_LANCZOS2: rc = scale_lanczos(src, dst); break;
    case MV_SCALER_FSR: rc = scale_fsr(src, dst); break;
    default:
        rc = alloc_image(dst, src->w, src->h);
        if (rc == 0) memcpy(dst->rgba, src->rgba, (size_t)src->w * src->h * 4);
        break;
    }
    if (rc == 0) {
        dst->ox = src->ox * (dst->w / src->w);
        dst->oy = src->oy * (dst->h / src->h);
        dst->reveal = src->reveal;
    }
    if (ms) *ms = now_ms() - t0;
    return rc;
}

/* --- view maths ----------------------------------------------------------*/

const float MV_ZOOM_TABLE[MV_ZOOM_STEPS] = {0.0f, 1.0f, 1.25f, 1.5f, 2.0f, 3.0f, 4.0f};

/* The highest step the view may use: 3.0 (the source cannot show more), 4.0 when a
 * higher-resolution picture (a mod pack) is loaded (P6.32 section 4.4). */
static int view_top(const MvView *v) {
    return v->top == MV_ZOOM_STEPS - 1 ? MV_ZOOM_STEPS - 1 : MV_ZOOM_TOP_PLAIN;
}

float dw3_mv_view_top_zoom(const MvView *v) { return MV_ZOOM_TABLE[view_top(v)]; }

float dw3_mv_view_scale(int step, int map_w, int map_h, int vw, int vh) {
    float sw, sh;

    if (step < 0) step = 0;
    if (step >= MV_ZOOM_STEPS) step = MV_ZOOM_STEPS - 1;
    if (step > 0) return MV_ZOOM_TABLE[step] * (float)vh / 240.0f;
    sw = (float)vw / (float)map_w;
    sh = (float)vh / (float)map_h;
    return sw < sh ? sw : sh;
}

float dw3_mv_view_cur_scale(const MvView *v, int map_w, int map_h, int vw,
                            int vh) {
    if (v->zoom > 0.0f) return v->zoom * (float)vh / 240.0f;
    return dw3_mv_view_scale(v->step, map_w, map_h, vw, vh);
}

float dw3_mv_view_cur_zoom(const MvView *v, int map_w, int map_h, int vw,
                           int vh) {
    return dw3_mv_view_cur_scale(v, map_w, map_h, vw, vh) * 240.0f / (float)vh;
}

int dw3_mv_view_clamp_ex(MvView *v, int map_w, int map_h, int vw, int vh,
                         float *want_cx, float *want_cy) {
    float s = dw3_mv_view_cur_scale(v, map_w, map_h, vw, vh);
    float hx = (float)vw * 0.5f / s, hy = (float)vh * 0.5f / s;
    float wx = v->cx, wy = v->cy;
    int flags = 0;

    if ((float)map_w <= 2.0f * hx) {
        v->cx = (float)map_w * 0.5f;
        flags |= MV_PAN_CENTRED_X;
    } else {
        v->cx = fclampf(v->cx, hx, (float)map_w - hx);
        if (v->cx - hx <= 0.001f) flags |= MV_PAN_LEFT;
        if (v->cx + hx >= (float)map_w - 0.001f) flags |= MV_PAN_RIGHT;
    }
    if ((float)map_h <= 2.0f * hy) {
        v->cy = (float)map_h * 0.5f;
        flags |= MV_PAN_CENTRED_Y;
    } else {
        v->cy = fclampf(v->cy, hy, (float)map_h - hy);
        if (v->cy - hy <= 0.001f) flags |= MV_PAN_TOP;
        if (v->cy + hy >= (float)map_h - 0.001f) flags |= MV_PAN_BOTTOM;
    }
    if (want_cx) *want_cx = wx;
    if (want_cy) *want_cy = wy;
    return flags;
}

void dw3_mv_view_clamp(MvView *v, int map_w, int map_h, int vw, int vh) {
    dw3_mv_view_clamp_ex(v, map_w, map_h, vw, vh, NULL, NULL);
}

void dw3_mv_view_zoom(MvView *v, int dir, int map_w, int map_h, int vw, int vh) {
    int step, top = view_top(v);

    if (v->zoom > 0.0f) {
        /* off the table (the stick): the next table zoom in that direction */
        float cur = dw3_mv_view_cur_zoom(v, map_w, map_h, vw, vh);
        float fit = dw3_mv_view_scale(0, map_w, map_h, vw, vh) * 240.0f /
                    (float)vh;
        int i;

        step = dir < 0 ? 0 : top;
        if (dir < 0) {
            for (i = top; i >= 1; --i)
                if (MV_ZOOM_TABLE[i] < cur - 0.001f &&
                    MV_ZOOM_TABLE[i] > fit + 0.001f) { step = i; break; }
        } else {
            for (i = 1; i <= top; ++i)
                if (MV_ZOOM_TABLE[i] > cur + 0.001f) { step = i; break; }
        }
        v->zoom = 0.0f;
    } else {
        step = v->step + (dir < 0 ? -1 : 1);
    }
    if (step < 0) step = 0;
    if (step > top) step = top;
    v->step = step;
    dw3_mv_view_clamp(v, map_w, map_h, vw, vh);
}

void dw3_mv_view_zoom_by(MvView *v, float factor, int map_w, int map_h, int vw,
                         int vh) {
    float fit = dw3_mv_view_scale(0, map_w, map_h, vw, vh) * 240.0f / (float)vh;
    float z = dw3_mv_view_cur_zoom(v, map_w, map_h, vw, vh) * factor;
    float top = MV_ZOOM_TABLE[view_top(v)];
    int i, best = 1;

    if (z > top) z = top;
    if (z <= fit + 0.0005f) {
        v->step = 0;
        v->zoom = 0.0f;
    } else {
        v->zoom = z;
        for (i = 1; i <= view_top(v); ++i)
            if (fabsf(MV_ZOOM_TABLE[i] - z) < fabsf(MV_ZOOM_TABLE[best] - z))
                best = i;
        v->step = best;
    }
    dw3_mv_view_clamp(v, map_w, map_h, vw, vh);
}

void dw3_mv_view_rect(const MvView *v, int map_w, int map_h, int vw, int vh,
                      float *x0, float *y0, float *x1, float *y1) {
    float s = dw3_mv_view_cur_scale(v, map_w, map_h, vw, vh);

    *x0 = (float)vw * 0.5f - v->cx * s;
    *y0 = (float)vh * 0.5f - v->cy * s;
    *x1 = *x0 + (float)map_w * s;
    *y1 = *y0 + (float)map_h * s;
}

void dw3_mv_view_point(const MvView *v, int map_w, int map_h, int vw, int vh,
                       float mx, float my, float *px, float *py) {
    float s = dw3_mv_view_cur_scale(v, map_w, map_h, vw, vh);

    *px = (float)vw * 0.5f + (mx - v->cx) * s;
    *py = (float)vh * 0.5f + (my - v->cy) * s;
}

int dw3_mv_view_anchor(MvView *v, float mx, float my, float px, float py,
                       int map_w, int map_h, int vw, int vh, float *want_cx,
                       float *want_cy) {
    float s = dw3_mv_view_cur_scale(v, map_w, map_h, vw, vh);

    v->cx = mx - (px - (float)vw * 0.5f) / s;
    v->cy = my - (py - (float)vh * 0.5f) / s;
    return dw3_mv_view_clamp_ex(v, map_w, map_h, vw, vh, want_cx, want_cy);
}

int dw3_mv_view_follow(MvView *v, float mx, float my, float margin_x,
                       float margin_y, int map_w, int map_h, int vw, int vh,
                       float *want_cx, float *want_cy) {
    float px, py, s = dw3_mv_view_cur_scale(v, map_w, map_h, vw, vh);

    dw3_mv_view_point(v, map_w, map_h, vw, vh, mx, my, &px, &py);
    if (px < margin_x) v->cx -= (margin_x - px) / s;
    if (px > (float)vw - margin_x) v->cx += (px - ((float)vw - margin_x)) / s;
    if (py < margin_y) v->cy -= (margin_y - py) / s;
    if (py > (float)vh - margin_y) v->cy += (py - ((float)vh - margin_y)) / s;
    return dw3_mv_view_clamp_ex(v, map_w, map_h, vw, vh, want_cx, want_cy);
}

float dw3_mv_pan_speed(int hold, float zoom) {
    float cap;

    if (hold <= 0) return 0.0f;
    if (hold <= 10) return 2.0f;
    if (hold <= 20) return 3.0f;
    if (hold <= MV_PAN_ACCEL_AFTER) return 4.0f;
    cap = 10.0f / (zoom > 0.1f ? zoom : 0.1f);
    if (cap < 4.0f) cap = 4.0f;
    {
        float v = 4.0f + (float)(hold - MV_PAN_ACCEL_AFTER) * (1.0f / 15.0f);

        return v > cap ? cap : v;
    }
}

float dw3_mv_stick_unit(int raw) {
    int a = raw < 0 ? -raw : raw;
    float d, f;

    if (a > 127) a = 127;
    if (a <= MV_STICK_MOVE_DEADZONE) return 0.0f;
    d = (float)(a - MV_STICK_MOVE_DEADZONE) / (float)(127 - MV_STICK_MOVE_DEADZONE);
    f = d * d * (3.0f - 2.0f * d); /* smoothstep: fine control near the centre */
    if (f < 0.02f) f = 0.02f;
    return raw < 0 ? -f : f;
}

int dw3_mv_backdrop(const MvImage *src, uint8_t *rgba, int dim_percent) {
    int cx, cy;

    if (!src || !src->rgba || src->w <= 0 || src->h <= 0 || !rgba) return -1;
    if (dim_percent < 0) dim_percent = 0;
    if (dim_percent > 100) dim_percent = 100;
    for (cy = 0; cy < MV_BACKDROP_H; ++cy) {
        for (cx = 0; cx < MV_BACKDROP_W; ++cx) {
            int x0 = cx * src->w / MV_BACKDROP_W, x1 = (cx + 1) * src->w / MV_BACKDROP_W;
            int y0 = cy * src->h / MV_BACKDROP_H, y1 = (cy + 1) * src->h / MV_BACKDROP_H;
            unsigned long sum[3] = {0, 0, 0};
            unsigned long n = 0;
            int x, y, k;
            uint8_t *o = rgba + ((size_t)cy * MV_BACKDROP_W + (size_t)cx) * 4;

            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;
            for (y = y0; y < y1 && y < src->h; ++y) {
                for (x = x0; x < x1 && x < src->w; ++x) {
                    const uint8_t *p = src->rgba + ((size_t)y * src->w + (size_t)x) * 4;

                    sum[0] += p[0];
                    sum[1] += p[1];
                    sum[2] += p[2];
                    ++n;
                }
            }
            for (k = 0; k < 3; ++k)
                o[k] = (uint8_t)(n ? sum[k] / n * (unsigned long)dim_percent / 100u : 0);
            o[3] = 255;
        }
    }
    return 0;
}

float dw3_mv_sprite_factor(const MvView *v, int map_w, int map_h, int vw, int vh) {
    /* the size at zoom 1.0 (step 1): the view the screen opens in and the size
     * the game draws them at on the PS1 */
    float ref = dw3_mv_view_scale(1, map_w, map_h, vw, vh);
    float cur = dw3_mv_view_cur_scale(v, map_w, map_h, vw, vh);

    if (cur <= 0.0f) return 1.0f;
    return ref / cur;
}

void dw3_mv_sprite_rect(const MvView *v, int map_w, int map_h, int vw, int vh,
                        float ax, float ay, int ox, int oy, int w, int h,
                        int fixed, float *x0, float *y0, float *x1, float *y1) {
    float rx0, ry0, rx1, ry1;
    float s = dw3_mv_view_cur_scale(v, map_w, map_h, vw, vh);
    float k = fixed ? dw3_mv_sprite_factor(v, map_w, map_h, vw, vh) : 1.0f;
    float px, py;

    dw3_mv_view_rect(v, map_w, map_h, vw, vh, &rx0, &ry0, &rx1, &ry1);
    px = rx0 + s * ax;
    py = ry0 + s * ay;
    *x0 = px + s * k * (float)ox;
    *y0 = py + s * k * (float)oy;
    *x1 = px + s * k * (float)(ox + w);
    *y1 = py + s * k * (float)(oy + h);
}

/* --- cache ---------------------------------------------------------------*/

#define MV_CACHE_MAGIC 0x4D335744u /* "DW3M" little-endian */
#define MV_CACHE_VERSION 3u

int dw3_mv_cache_write(const char *path, const MvImage *img) {
    FILE *f;
    uint32_t hdr[7];
    char tmp[600];
    size_t bytes;
    int ok;

    if (!path || !img || !img->rgba) return -1;
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) return -1;
    f = fopen(tmp, "wb");
    if (!f) return -1;
    hdr[0] = MV_CACHE_MAGIC;
    hdr[1] = MV_CACHE_VERSION;
    hdr[2] = (uint32_t)img->w;
    hdr[3] = (uint32_t)img->h;
    hdr[4] = (uint32_t)img->ox;
    hdr[5] = (uint32_t)img->oy;
    hdr[6] = img->reveal;
    bytes = (size_t)img->w * (size_t)img->h * 4;
    ok = fwrite(hdr, sizeof(hdr), 1, f) == 1 &&
         fwrite(img->rgba, 1, bytes, f) == bytes;
    ok = (fclose(f) == 0) && ok;
    if (!ok || rename(tmp, path) != 0) {
        remove(tmp);
        return -1;
    }
    return 0;
}

int dw3_mv_cache_read(const char *path, MvImage *img) {
    FILE *f = fopen(path, "rb");
    uint32_t hdr[7];
    size_t bytes;

    img->rgba = NULL;
    img->w = img->h = 0;
    img->ox = img->oy = 0;
    img->reveal = 0;
    if (!f) return -1;
    if (fread(hdr, sizeof(hdr), 1, f) != 1 || hdr[0] != MV_CACHE_MAGIC ||
        hdr[1] != MV_CACHE_VERSION || hdr[2] == 0 || hdr[3] == 0 ||
        hdr[2] > 4096 || hdr[3] > 4096) {
        fclose(f);
        return -1;
    }
    bytes = (size_t)hdr[2] * hdr[3] * 4;
    img->rgba = (uint8_t *)malloc(bytes);
    if (!img->rgba || fread(img->rgba, 1, bytes, f) != bytes) {
        free(img->rgba);
        img->rgba = NULL;
        fclose(f);
        return -1;
    }
    fclose(f);
    img->w = (int)hdr[2];
    img->h = (int)hdr[3];
    img->ox = (int)hdr[4];
    img->oy = (int)hdr[5];
    img->reveal = hdr[6];
    return 0;
}

void dw3_mv_cache_name(char *out, size_t cap, const char *dir, const char *region,
                       int half, int scaler) {
    snprintf(out, cap, "%s/map_%s_h%d_s%d.rgba", dir ? dir : ".", region, half,
             scaler);
}

int dw3_mv_namebox_bottom(MvNameBox *nb, int hover, float scr_y) {
    if (hover > 0 && nb->hover <= 0) nb->bottom = scr_y < 120.0f;
    nb->hover = hover > 0 ? hover : 0;
    return nb->bottom;
}

int dw3_mv_hitbox_reach(int sx, int sy) {
    int x0 = sx + 7, x1 = sx + 0x12, y0 = sy + 7, y1 = sy + 0x12;
    int inside = x0 >= MV_PS1_X0 && x1 <= MV_PS1_X1 && y0 >= MV_PS1_Y0 &&
                 y1 <= MV_PS1_Y1;
    int apart = x1 < MV_PS1_X0 || x0 > MV_PS1_X1 || y1 < MV_PS1_Y0 ||
                y0 > MV_PS1_Y1;

    return inside ? 0 : apart ? 2 : 1;
}

int dw3_mv_point_strip(int x, int y) {
    return (x < MV_PS1_X0 ? 1 : 0) | (x > MV_PS1_X1 ? 2 : 0) |
           (y < MV_PS1_Y0 ? 4 : 0) | (y > MV_PS1_Y1 ? 8 : 0);
}

/* The font's icons (kind 1 codes) that the area names use, as text for the log.
 * Found by reading the five EU languages' names: 1 is the space, 5 the full stop
 * of the abbreviations ("Pos. Bos."), 19 the apostrophe, 26 the hyphen of the
 * compound names, the rest the accented letters (UTF-8). Any other icon is left
 * as <iN>. */
static const char *icon_text(unsigned n) {
    switch (n) {
    case 1: return " ";
    case 5: return ".";
    case 19: return "'";
    case 26: return "-";
    case 65: return "\xC3\x84";   /* A diaeresis */
    case 71: return "\xC3\x96";   /* O diaeresis */
    case 75: return "\xC3\xA0";   /* a grave */
    case 78: return "\xC3\xA4";   /* a diaeresis */
    case 81: return "\xC3\xA9";   /* e acute */
    case 82: return "\xC3\xAA";   /* e circumflex */
    case 87: return "\xC3\xB1";   /* n tilde */
    case 91: return "\xC3\xB6";   /* o diaeresis */
    case 94: return "\xC3\xBC";   /* u diaeresis */
    case 95: return "\xC3\xA1";   /* a acute */
    case 96: return "\xC3\xAD";   /* i acute */
    case 97: return "\xC3\xB3";   /* o acute */
    case 105: return "\xC3\x89";  /* E acute */
    default: return NULL;
    }
}

int dw3_mv_name_text(const unsigned short *chars, int n, char *out, size_t cap) {
    size_t o = 0;
    int i, odd = 0;

    if (cap == 0) return 0;
    for (i = 0; i < n; ++i) {
        unsigned c = chars[i];
        char tmp[16];
        const char *s = tmp;

        tmp[1] = 0;
        if (c == 0x0A) {
            s = " / ";
        } else if (c >= 0x824F && c <= 0x8258) {
            tmp[0] = (char)('0' + (c - 0x824F));
        } else if (c >= 0x8260 && c <= 0x8279) {
            tmp[0] = (char)('A' + (c - 0x8260));
        } else if (c >= 0x8281 && c <= 0x829A) {
            tmp[0] = (char)('a' + (c - 0x8281));
        } else if (c == 0x8140) {
            tmp[0] = ' ';
        } else if (c == 0x8143) {
            tmp[0] = ',';
        } else if (c == 0x8144) {
            tmp[0] = '.';
        } else if (c == 0x8148) {
            tmp[0] = '?';
        } else if (c == 0x8149) {
            tmp[0] = '!';
        } else if (c == 0x815D || c == 0x817C) {
            tmp[0] = '-';
        } else if (c == 0x8166) {
            tmp[0] = '\'';
        } else if (c & 0x8000 && c < 0x8100 && icon_text(c & 0xFF)) {
            s = icon_text(c & 0xFF);
        } else if (c & 0x8000 && c < 0x8100) {
            snprintf(tmp, sizeof(tmp), "<i%u>", c & 0xFF);
            ++odd;
        } else {
            snprintf(tmp, sizeof(tmp), "<%04X>", c);
            ++odd;
        }
        while (*s && o + 1 < cap) out[o++] = *s++;
    }
    out[o] = 0;
    return odd;
}
