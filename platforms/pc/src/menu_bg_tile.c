/* P51 item 6: the game's own menu background tile for the Options screen.
 * The field menu's screens (Items, Status, ...) draw sprite 0x1D of
 * FILE_STATUS_SPRITES (STSTATCS.BIN) over the images of FILE_STATUS_SPRITES + 1
 * (STSTATTM.BIN, entry 0, two TIMs at VRAM (0x280, 0x100) and (0x2C0, 0x100));
 * the frame is a field of 48 x 48 parts that repeats every 96 pixels. This reads
 * the two files from the disc files (the same cdfs the map view reads), decodes
 * the four parts of one 96 x 96 period with the sprite rules of the game
 * (4-bit parts, palette on the page's last rows, VRAM colour 0 kept) and hands
 * back an RGBA tile. Host code: no game or map-view dependency (the game part of
 * a combined image cannot be called from here). Nothing is kept on disk. */
#include "menu_bg_tile.h"

#include "cdfs.h"
#include "dw3_region_rt.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { VW = 1024, VH = 512, RLEN_MAGIC = 0x4E454C52 };

static int rd32(const uint8_t *p) {
    return (int)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
                 ((uint32_t)p[3] << 24));
}

static int rd16(const uint8_t *p) {
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static double now_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

/* The image block of a TIM at (x0, y0) of the VRAM copy; the CLUT block is
 * skipped (the sprite's palettes are image rows of the page). */
static int place_tim(const uint8_t *tim, size_t avail, int x0, int y0, uint16_t *vram) {
    size_t pos = 8;
    int w, h, x, y;

    if (avail < 20) return -1;
    if ((rd32(tim + 4) & 8) != 0) {
        pos += (size_t)(uint32_t)rd32(tim + pos);
        if (pos + 12 > avail) return -1;
    }
    w = (uint16_t)rd16(tim + pos + 8);
    h = (uint16_t)rd16(tim + pos + 10);
    if (pos + 12 + (size_t)w * (size_t)h * 2 > avail) return -1;
    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            if (x0 + x < VW && y0 + y < VH) {
                vram[(y0 + y) * VW + x0 + x] =
                    (uint16_t)rd16(tim + pos + 12 + ((size_t)y * (size_t)w + (size_t)x) * 2);
            }
        }
    }
    return 0;
}

/* Entry 0 of the image file: a table of offsets (0 ends it) of TIMs, each one
 * 64 halfwords to the right of the one before, RLE packed when it starts with
 * "RLEN". Returns the TIMs placed, or -1. */
static int load_entry0(const uint8_t *file, size_t len, uint16_t *vram) {
    const uint8_t *arch;
    uint8_t *buf;
    int placed = 0, i, x = 0x280;

    if (len < 16 || rd32(file) <= 0 || (size_t)rd32(file) >= len) return -1;
    arch = file + rd32(file);
    buf = (uint8_t *)malloc(0x10000);
    if (!buf) return -1;
    for (i = 0; i < 16; ++i) {
        int off = rd32(arch + 4 * i);
        const uint8_t *tim;
        size_t avail;

        if (off == 0) break;
        if (off < 0 || (size_t)(arch - file) + (size_t)off + 16 > len) {
            placed = -1;
            break;
        }
        tim = arch + off;
        avail = len - (size_t)(tim - file);
        if (rd32(tim) == RLEN_MAGIC) {
            const uint8_t *src = tim + 8, *end = tim + avail;
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
        if (place_tim(tim, avail, x, 0x100, vram) != 0) {
            placed = -1;
            break;
        }
        x += 0x40;
        ++placed;
    }
    free(buf);
    return placed;
}

static uint8_t expand5(unsigned v) { return (uint8_t)((v * 255u + 15u) / 31u); }

int dw3_menu_bg_tile_load(uint8_t *rgba, double *ms, char *why, size_t why_cap) {
    const int us = dw3_region() == DW3_REGION_US;
    const uint32_t sheet_id = us ? 0x3F4u : 0x404u;
    uint32_t sn = cdfs_file_size(sheet_id), in = cdfs_file_size(sheet_id + 1);
    uint8_t *sheet = NULL, *imgs = NULL;
    uint16_t *vram = NULL;
    const uint8_t *b;
    size_t slen, foff = 0, poff;
    int rc = -1, i, count, found = 0, drawn = 0;
    double t0 = now_ms();
    const char *msg = "cannot read the sprite files";

    if (sn == (uint32_t)-1 || in == (uint32_t)-1 || sn > 0x20000 || in > 0x80000) goto out;
    sheet = (uint8_t *)malloc(sn);
    imgs = (uint8_t *)malloc(in);
    vram = (uint16_t *)calloc((size_t)VW * VH, 2);
    if (!sheet || !imgs || !vram || cdfs_read(sheet_id, sheet, sn) != (int)sn ||
        cdfs_read(sheet_id + 1, imgs, in) != (int)in) {
        goto out;
    }
    msg = "unexpected sprite file layout";
    if (sn < 32 || rd32(sheet) <= 0 || (uint32_t)rd32(sheet) >= sn) goto out;
    b = sheet + rd32(sheet);
    slen = sn - (size_t)rd32(sheet);
    if (load_entry0(imgs, in, vram) < 2) goto out;
    poff = (size_t)rd32(b);
    {
        size_t ids = (size_t)rd32(b + 4);

        if (poff >= slen || ids >= slen) goto out;
        for (i = 0; i < 255 && ids + (size_t)i < slen; ++i) {
            if (b[ids + (size_t)i] == 0xff) break;
            if (b[ids + (size_t)i] == 0x1D) {
                int off = rd32(b + 8 + 4 * i);

                if (off > 0 && (size_t)off + 6 <= slen) {
                    foff = (size_t)off;
                    found = 1;
                }
                break;
            }
        }
    }
    if (!found) goto out;
    count = rd16(b + foff);
    if (count <= 0 || count > 64 || foff + 6 + (size_t)count * 6 > slen) goto out;
    memset(rgba, 0, (size_t)MENU_BG_TILE * MENU_BG_TILE * 4);
    for (i = 0; i < count; ++i) {
        const uint8_t *p = b + foff + 6 + (size_t)i * 6;
        int pi = rd16(p), dx = rd16(p + 2), dy = rd16(p + 4);
        const uint8_t *part;
        int pu, pv, pw, ph, pcx, pcy, row, pal_x, pal_y, page_x, x, y;

        if (dx < 0 || dy < 0 || dx >= MENU_BG_TILE || dy >= MENU_BG_TILE) continue;
        if (pi < 0 || poff + (size_t)(pi + 1) * 14 > slen) goto out;
        part = b + poff + (size_t)pi * 14;
        if (rd16(part + 12) != 0) continue; /* only the 4-bit parts form this tile */
        pu = rd16(part);
        pv = rd16(part + 2);
        pw = rd16(part + 4);
        ph = rd16(part + 6);
        pcx = rd16(part + 8);
        pcy = rd16(part + 10);
        row = rd16(b + foff + 2);
        page_x = ((0x280 + (pu >> 2)) / 64) * 64;
        pal_x = (0x280 + pcx) / 16 * 16;
        pal_y = 0x100 + pcy + row;
        if (pal_x < 0 || pal_x + 16 > VW || pal_y < 0 || pal_y >= VH) continue;
        for (y = 0; y < ph; ++y) {
            for (x = 0; x < pw; ++x) {
                int sx = (pu & 0xff) + x, ox = dx + x, oy = dy + y;
                unsigned packed, idx, color;
                uint8_t *d;

                if (ox >= MENU_BG_TILE || oy >= MENU_BG_TILE) continue;
                packed = vram[((0x100 + pv + y) % VH) * VW + (page_x + (sx >> 2)) % VW];
                idx = (packed >> ((sx & 3) * 4)) & 15u;
                color = vram[pal_y * VW + pal_x + (int)idx];
                d = rgba + ((size_t)oy * MENU_BG_TILE + (size_t)ox) * 4;
                d[0] = expand5(color & 31u);
                d[1] = expand5((color >> 5) & 31u);
                d[2] = expand5((color >> 10) & 31u);
                d[3] = 255;
                ++drawn;
            }
        }
    }
    msg = "the tile has holes";
    if (drawn < MENU_BG_TILE * MENU_BG_TILE) goto out;
    rc = 0;
out:
    if (ms) *ms = now_ms() - t0;
    if (rc != 0 && why && why_cap) snprintf(why, why_cap, "%s", msg);
    free(sheet);
    free(imgs);
    free(vram);
    return rc;
}
