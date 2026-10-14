/* Capture only the original US title-logo sprites from PSY-Z VRAM, then
 * resample them into a private PC GPU texture. No game art is embedded. */
#include "engine/gfx.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(DW3_PC_SDL3_GPU) && defined(VERSION_US) && VERSION_US
#include "engine/game_state.h"
#include "title_logo_resample.h"
enum {
    VRAM_WIDTH = 1024,
    VRAM_HEIGHT = 512,
    LOGO_CROP_X = 17,
    LOGO_CROP_Y = 22,
    LOGO_SOURCE_W = 300,
    LOGO_SOURCE_H = 136,
    LOGO_OUTPUT_W = 720,
    LOGO_OUTPUT_H = 326,
};

/* PSY-Z GPU backend's VRAM readback helper is private to its renderer TU. */
extern void Draw_StoreImage(RECT *rect, u_long *pixels);

typedef struct LogoPixel {
    uint8_t rgba[4];
} LogoPixel;

typedef struct LogoFrame {
    int count;
    int row;
    int abr;
    const s16 *parts;
} LogoFrame;

extern int dw3_pc_title_menu_set_logo_rgba(const uint8_t *rgba, int width,
                                            int height, int pitch);
extern void dw3_pc_title_menu_logo_set_active(int active);

static int g_logo_ready;
static int g_logo_active;

static int read_frame(const s32 *sheet, int id, LogoFrame *frame) {
    const u8 *bytes = (const u8 *)sheet;
    const u8 *ids;
    int index;
    s32 offset;
    const s16 *header;

    if (!sheet || !frame || sheet[0] <= 0 || sheet[1] <= 0) return 0;
    ids = bytes + sheet[1];
    for (index = 0; index < 64 && ids[index] != 0xff; ++index) {
        if (ids[index] == id) break;
    }
    if (index >= 64 || ids[index] == 0xff) return 0;
    offset = sheet[index + 2];
    if (offset <= 0) return 0;
    header = (const s16 *)(bytes + offset);
    frame->count = header[0];
    frame->row = header[1];
    frame->abr = header[2];
    frame->parts = header + 3;
    return frame->count > 0 && frame->count <= 64;
}

static uint8_t expand5(unsigned value) {
    return (uint8_t)((value * 255u + 15u) / 31u);
}

static void composite_pixel(LogoPixel *dst, u16 color, int abr) {
    unsigned r, g, b;
    int c;

    if (color == 0) return;
    r = expand5(color & 31u);
    g = expand5((color >> 5) & 31u);
    b = expand5((color >> 10) & 31u);
    if (abr == 2 && (color & 0x8000u)) {
        if (dst->rgba[3]) {
            dst->rgba[0] = (uint8_t)(dst->rgba[0] > r ? dst->rgba[0] - r : 0);
            dst->rgba[1] = (uint8_t)(dst->rgba[1] > g ? dst->rgba[1] - g : 0);
            dst->rgba[2] = (uint8_t)(dst->rgba[2] > b ? dst->rgba[2] - b : 0);
        } else {
            /* A PS1 subtractive shadow over the unknown animated backdrop
             * is represented as a soft black edge in the RGBA overlay. */
            dst->rgba[0] = 0;
            dst->rgba[1] = 0;
            dst->rgba[2] = 0;
            dst->rgba[3] = 112;
        }
        return;
    }
    for (c = 0; c < 3; ++c) dst->rgba[c] = 0;
    dst->rgba[0] = (uint8_t)r;
    dst->rgba[1] = (uint8_t)g;
    dst->rgba[2] = (uint8_t)b;
    dst->rgba[3] = 255;
}

static int draw_frame(const u16 *vram, const s32 *sheet, int frame_id,
                      int origin_x, int origin_y, int clut_row,
                      LogoPixel *canvas) {
    const SpritePart *parts;
    LogoFrame frame;
    int i;

    if (!read_frame(sheet, frame_id, &frame)) return 0;
    parts = (const SpritePart *)((const u8 *)sheet + sheet[0]);
    for (i = frame.count - 1; i >= 0; --i) {
        int part_index = frame.parts[i * 3];
        const SpritePart *part;
        int part_x = frame.parts[i * 3 + 1];
        int part_y = frame.parts[i * 3 + 2];
        int mode, shift, mask, source_u, page_x, palette_x, palette_y;
        int y, x;

        if (part_index < 0 || part_index > 4096) continue;
        part = &parts[part_index];
        mode = part->mode ? 1 : 0;
        shift = mode ? 1 : 2;
        mask = (1 << shift) - 1;
        source_u = mode ? (part->u & 0x7f) : (part->u & 0xff);
        page_x = 640 + part->u / (1 << shift);
        page_x = (page_x / 64) * 64;
        palette_x = (mode ? 0 : 640) + part->clutX;
        palette_x = (palette_x / 16) * 16;
        /* setAltClut(0, 0x1f0) stores Y=240 internally; these US 8-bit
         * parts add clutY=256, yielding the actual CLUT row 496. */
        palette_y = (mode ? 240 : 0) + part->clutY + frame.row + clut_row;
        if (palette_x < 0 || palette_x >= VRAM_WIDTH ||
            palette_y < 0 || palette_y >= VRAM_HEIGHT) continue;

        for (y = 0; y < part->h; ++y) {
            int dest_y = origin_y + part_y + y;
            if (dest_y < 0 || dest_y >= 240) continue;
            for (x = 0; x < part->w; ++x) {
                int dest_x = origin_x + part_x + x;
                int source_x = source_u + x;
                int word_x, word_y, subpixel, index;
                u16 packed, color;

                if (dest_x < 0 || dest_x >= 320) continue;
                word_x = (page_x + (source_x >> shift)) % VRAM_WIDTH;
                word_y = (part->v + y) % VRAM_HEIGHT;
                packed = vram[word_y * VRAM_WIDTH + word_x];
                subpixel = source_x & mask;
                {
                    const unsigned bits_per_texel = 16u >> shift;
                    index = (packed >> (subpixel * bits_per_texel)) &
                            ((1u << bits_per_texel) - 1u);
                }
                color = vram[palette_y * VRAM_WIDTH +
                             ((palette_x + index) % VRAM_WIDTH)];
                composite_pixel(&canvas[dest_y * 320 + dest_x], color,
                                frame.abr);
            }
        }
    }
    return 1;
}

int dw3_port_title_logo_capture(const s32 *sheet) {
    RECT rect = {0, 0, VRAM_WIDTH, VRAM_HEIGHT};
    u16 *vram;
    LogoPixel *source;
    uint8_t *cropped;
    uint8_t *output;
    int x, y, rc, opaque_pixels = 0, colored_pixels = 0;

    g_logo_ready = 0;
    if (!sheet) return 0;
    vram = (u16 *)calloc((size_t)VRAM_WIDTH * VRAM_HEIGHT, sizeof(u16));
    source = (LogoPixel *)calloc((size_t)320 * 240, sizeof(LogoPixel));
    cropped = (uint8_t *)malloc((size_t)LOGO_SOURCE_W * LOGO_SOURCE_H * 4);
    output = (uint8_t *)malloc((size_t)LOGO_OUTPUT_W * LOGO_OUTPUT_H * 4);
    if (!vram || !source || !cropped || !output) goto fail;

    Draw_StoreImage(&rect, (u_long *)vram);
    /* Compose the terminal shadow row behind the original top/lower logo and
     * colored numeral. The US glint task finishes with frame=-1, so no sparkle
     * belongs in the stable title state. */
    if (!draw_frame(vram, sheet, 3, 8, 28, 11, source) ||
        !draw_frame(vram, sheet, 0, 19, 24, 0, source) ||
        !draw_frame(vram, sheet, 1, 19, 111, 0, source) ||
        !draw_frame(vram, sheet, 2, 253, 100, 0, source)) goto fail;
    for (y = 0; y < 240; ++y) {
        for (x = 0; x < 320; ++x) {
            const LogoPixel *pixel = &source[y * 320 + x];
            if (pixel->rgba[3] != 0) {
                ++opaque_pixels;
                if (pixel->rgba[0] || pixel->rgba[1] || pixel->rgba[2])
                    ++colored_pixels;
            }
        }
    }
    if (opaque_pixels < 1000 || colored_pixels < 500) goto fail;
    for (y = 0; y < LOGO_SOURCE_H; ++y) {
        for (x = 0; x < LOGO_SOURCE_W; ++x) {
            memcpy(cropped + ((size_t)y * LOGO_SOURCE_W + x) * 4,
                   source[(y + LOGO_CROP_Y) * 320 + x + LOGO_CROP_X].rgba,
                   4);
        }
    }
    rc = dw3_pc_title_logo_resample(
        cropped, LOGO_SOURCE_W, LOGO_SOURCE_H, LOGO_SOURCE_W * 4,
        output, LOGO_OUTPUT_W, LOGO_OUTPUT_H, LOGO_OUTPUT_W * 4,
        DW3_PC_LOGO_LANCZOS2_FINE);
    if (rc != 0 || dw3_pc_title_menu_set_logo_rgba(
                       output, LOGO_OUTPUT_W, LOGO_OUTPUT_H,
                       LOGO_OUTPUT_W * 4) != 0) goto fail;
    g_logo_ready = 1;
    fprintf(stderr,
            "dw3-title-logo: runtime US sprites 0-3 (%d opaque, %d colored) "
            "resampled Lanczos2+fine "
            "%dx%d -> %dx%d; original files remain external\n",
            opaque_pixels, colored_pixels,
            LOGO_SOURCE_W, LOGO_SOURCE_H, LOGO_OUTPUT_W, LOGO_OUTPUT_H);
    free(vram);
    free(source);
    free(cropped);
    free(output);
    return 1;

fail:
    fprintf(stderr, "dw3-title-logo: capture/resample unavailable; "
                    "using original sprites\n");
    free(vram);
    free(source);
    free(cropped);
    free(output);
    return 0;
}

int dw3_port_title_logo_ready(void) { return g_logo_ready; }
int dw3_port_title_logo_active(void) { return g_logo_active; }

void dw3_port_title_logo_set_active(int active) {
    const int in_title = GAME.funcs.getMode() == MODE_TITLE;
    const int next_active = active && g_logo_ready && in_title;
    if (g_logo_active != next_active) {
        fprintf(stderr, "dw3-title-logo: active=%d ready=%d\n",
                next_active, g_logo_ready);
    }
    g_logo_active = next_active;
    dw3_pc_title_menu_logo_set_active(g_logo_active);
}
#else
int dw3_port_title_logo_capture(const s32 *sheet) { (void)sheet; return 0; }
int dw3_port_title_logo_ready(void) { return 0; }
int dw3_port_title_logo_active(void) { return 0; }
void dw3_port_title_logo_set_active(int active) { (void)active; }
#endif
