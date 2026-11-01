/* Runtime-only title background compositor for PC widescreen side panels.
 * The center remains the original renderer output. No disc artwork is embedded. */
#include "engine/gfx.h"
#include "port_internal_res.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#if defined(DW3_PC_SDL3_GPU) && defined(VERSION_US) && VERSION_US
#include "engine/game_state.h"
enum { BG_W = 320, BG_H = 240, VRAM_W = 1024, VRAM_H = 512 };
extern int dw3_pc_title_background_rgba(const uint8_t *rgba, int w, int h, int pitch);
static uint16_t g_vram[VRAM_W * VRAM_H];
static uint8_t g_base[BG_W * BG_H * 4];
static uint8_t g_frame[BG_W * BG_H * 4];
static uint8_t g_faded[BG_W * BG_H * 4];
static int g_ready, g_level;

static unsigned expand5(unsigned v) { return (v * 255u + 15u) / 31u; }

/* getTPage/getClut quantize origins; SpriteDrawer's alternate CLUT is used
 * by eight-bit parts. Frame order follows the final ordering-table traversal. */
static void draw_sheet(const s32 *sheet, int id, int page, int alt_y,
                       int origin_x, int origin_y, uint8_t *canvas) {
    const uint8_t *bytes = (const uint8_t *)sheet;
    const uint8_t *ids;
    const SpritePart *parts;
    const s16 *header, *entries;
    int index, count, row, abr, k;
    if (!sheet || id == 300 || sheet[0] <= 0 || sheet[1] <= 0) return;
    ids = bytes + sheet[1];
    for (index = 0; index < 256 && ids[index] != 0xff && ids[index] != id; ++index) {}
    if (index >= 256 || ids[index] == 0xff || sheet[index + 2] <= 0) return;
    header = (const s16 *)(bytes + sheet[index + 2]);
    count = header[0]; row = header[1]; abr = header[2]; entries = header + 3;
    if (count <= 0 || count > 4096) return;
    parts = (const SpritePart *)(bytes + sheet[0]);
    for (k = 0; k < count; ++k) {
        const int part_id = entries[k * 3];
        const SpritePart *part;
        int mode, shift, mask, page_x, palette_x, palette_y, source_u;
        int px, py, x, y;
        if (part_id < 0 || part_id > 4096) continue;
        part = &parts[part_id]; mode = part->mode ? 1 : 0;
        shift = mode ? 1 : 2; mask = (1 << shift) - 1;
        source_u = mode ? (part->u & 127) : (part->u & 255);
        page_x = ((page + part->u / (1 << shift)) / 64) * 64;
        palette_x = (((mode ? 0 : page) + part->clutX) / 16) * 16;
        palette_y = (mode ? alt_y : 0) + part->clutY + row;
        if (palette_y < 0 || palette_y >= VRAM_H) continue;
        px = origin_x + entries[k * 3 + 1];
        py = origin_y + entries[k * 3 + 2];
        for (y = 0; y < part->h; ++y) {
            int dy = py + y;
            if (dy < 0 || dy >= BG_H) continue;
            for (x = 0; x < part->w; ++x) {
                int dx = px + x, sx = source_u + x, c;
                unsigned word, ci, color;
                uint8_t *dst;
                unsigned rgb[3];
                if (dx < 0 || dx >= BG_W) continue;
                word = g_vram[((part->v + y) & 511) * VRAM_W +
                              ((page_x + (sx >> shift)) & 1023)];
                ci = (word >> ((sx & mask) * (16 >> shift))) &
                     ((1u << (16 >> shift)) - 1u);
                color = g_vram[palette_y * VRAM_W + ((palette_x + ci) & 1023)];
                if (color == 0) continue;
                dst = canvas + (dy * BG_W + dx) * 4;
                rgb[0] = expand5(color & 31u);
                rgb[1] = expand5((color >> 5) & 31u);
                rgb[2] = expand5((color >> 10) & 31u);
                for (c = 0; c < 3; ++c) {
                    unsigned v = rgb[c];
                    if (abr >= 0 && (color & 0x8000)) {
                        switch (abr) {
                        case 0: v = (dst[c] + v) / 2u; break;
                        case 1: v += dst[c]; break;
                        case 2: v = dst[c] > v ? dst[c] - v : 0; break;
                        case 3: v = dst[c] + v / 4u; break;
                        }
                    }
                    dst[c] = (uint8_t)(v > 255u ? 255u : v);
                }
                dst[3] = 255;
            }
        }
    }
}

static void upload_frame(void) {
    int i, c, edge = g_level * 2;
    if (!g_ready) return;
    if (edge > 255) edge = 255;
    for (i = 0; i < BG_W * BG_H; ++i) {
        for (c = 0; c < 3; ++c) {
            int v = g_frame[i * 4 + c] - edge;
            g_faded[i * 4 + c] = (uint8_t)(v > 0 ? v : 0);
        }
        g_faded[i * 4 + 3] = 255;
    }
    dw3_pc_title_background_rgba(g_faded, BG_W, BG_H, BG_W * 4);
}

void dw3_port_title_background_reset(void) {
    g_ready = 0; g_level = 0;
    dw3_pc_title_background_rgba(NULL, 0, 0, 0);
}

void dw3_port_title_background_fade(int level) {
    int next_level = level < 0 ? 0 : (level > 255 ? 255 : level);
    if (next_level == g_level) return;
    g_level = next_level;
    upload_frame();
}

void dw3_port_title_background_frame(const s32 *s0, const s32 *s1,
    const s32 *s2, const s32 *s3, const s32 *s8, const int frames[8],
    int pos6x, int pos6y, int pos7x, int pos7y) {
    int i;
    /* P6.11: only at the Internal resolution row's 3x (it was DW3_PC_INTERNAL_RES=3). */
    if (dw3_port_internal_res_target() != 3) return;
    if (GAME.funcs.getMode() != MODE_TITLE) {
        dw3_port_title_background_reset(); return;
    }
    if (!g_ready) {
        RECT rect = {0, 0, VRAM_W, VRAM_H};
        /* PSY-Z's StoreImage is asynchronous: the pixels are in g_vram after DrawSync */
        DrawSync(0);
        StoreImage(&rect, (u_long *)g_vram);
        DrawSync(0);
        memset(g_base, 0, sizeof(g_base));
        for (i = 0; i < BG_W * BG_H; ++i) g_base[i * 4 + 3] = 255;
        /* Static sprites are queued 0,1,2, so rendered 2,1,0. */
        draw_sheet(s2, 0, 0x380, 243, 0, 0, g_base);
        draw_sheet(s1, 0, 0x340, 243, 0, 0, g_base);
        draw_sheet(s0, 0, 0x300, 243, 0, 0, g_base);
        g_ready = 1;
    }
    memcpy(g_frame, g_base, sizeof(g_frame));
    /* Eight animation draws are queued 0..7 and rendered in reverse. */
    for (i = 7; i >= 0; --i) {
        int x = i == 6 ? pos6x : (i == 7 ? pos7x : 0);
        int y = i == 6 ? pos6y : (i == 7 ? pos7y : 0);
        draw_sheet(i < 4 ? s3 : s8, frames[i], 0x3c0, 0, x, y, g_frame);
    }
    upload_frame();
}
#endif
