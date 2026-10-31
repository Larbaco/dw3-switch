/* Capture the original title-logo sprites from PSY-Z VRAM, then resample them
 * into a private GPU texture (PC SDL3 GPU, Switch GLES). US uses its single
 * bank; the EU disc uses the bank of the current LANGUAGE, and also lends the
 * overlay plaques its own words for PRESS START / New Game / Continue.
 * No game art is embedded. */
#include "engine/gfx.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(DW3_TITLE_NATIVE_OVERLAY) && \
    ((defined(VERSION_US) && VERSION_US) || (defined(VERSION_EU) && VERSION_EU))
#include "engine/game_state.h"
#include "title_logo_resample.h"
#include "title_overlay_art.h"
#if defined(VERSION_EU) && VERSION_EU
#include "engine/file.h"
#define TITLE_EU 1
/* STDWTITL_TEXTURE_Y: the EU title bank sits at VRAM (0x280, 0x100). */
#define TITLE_TEXTURE_Y 0x100
#else
#define TITLE_EU 0
#define TITLE_TEXTURE_Y 0
#endif
enum {
    VRAM_WIDTH = 1024,
    VRAM_HEIGHT = 512,
    CANVAS_W = 320,
    CANVAS_H = 240,
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

/* One sprite draw of the stable title: frame, position, CLUT row, and whether
 * the draw set the alternate CLUT (setAltClut(0, 0x1f0)) for 8-bit parts. */
typedef struct LogoPiece {
    s16 frame;
    s16 x;
    s16 y;
    s16 clut_row;
    s16 alt;
    s16 first_part; /* parts before it are skipped (the EU label plates) */
} LogoPiece;

/* US, and EU languages other than 0: the terminal shadow row of sprite 3
 * behind title parts 0/1 and the colored numeral 2. The glint task finishes
 * with frame=-1, so no sparkle belongs in the stable title state. US keeps the
 * alternate-CLUT flag it was tuned with on sprite 3. */
static const LogoPiece k_layout_us[] = {
    {3, 8, 28, 11, 1}, {0, 19, 24, 0, 1}, {1, 19, 111, 0, 1},
    {2, 253, 100, 0, 1},
};
#if TITLE_EU
static const LogoPiece k_layout_eu[] = {
    {3, 8, 28, 11, 0}, {0, 19, 24, 0, 1}, {1, 19, 111, 0, 1},
    {2, 253, 100, 0, 1},
};
/* EU language 0 (the Japanese logo): Title0Alt/Title1Alt end at (17,61) and
 * (207,34), and GlintAlt keeps sprite 2 lit at (49,121). Its last glint frame
 * is an additive flash, which an opaque texture cannot carry; it is left out. */
static const LogoPiece k_layout_eu_alt[] = {
    {3, 8, 28, 11, 0}, {0, 17, 61, 0, 1}, {1, 207, 34, 0, 1},
    {2, 49, 121, 0, 1},
};
/* STDWTITL_menuSprites[LANGUAGE]: PRESS START, New Game, Continue. */
static const u8 k_menu_sprites[7][3] = {
    {8, 9, 10}, {8, 9, 10}, {8, 9, 10}, {17, 18, 19},
    {11, 12, 13}, {20, 21, 22}, {14, 15, 16},
};
#endif

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

/* Mirror spriteDrawerDraw for an untransformed draw with
 * setTexture(0x280, TITLE_TEXTURE_Y) and, when alt is set,
 * setAltClut(0, 0x1f0): 4-bit parts take the CLUT at (0x280 + clutX,
 * TITLE_TEXTURE_Y + clutY), 8-bit parts at (clutX, 240 + clutY) with the alt
 * CLUT or (clutX, clutY) without it, each plus the frame and drawer rows. */
static int draw_frame(const u16 *vram, const s32 *sheet,
                      const LogoPiece *piece, LogoPixel *canvas) {
    const SpritePart *parts;
    LogoFrame frame;
    int i;

    if (!read_frame(sheet, piece->frame, &frame)) return 0;
    parts = (const SpritePart *)((const u8 *)sheet + sheet[0]);
#if TITLE_EU
    /* spriteDrawerDraw adds the last part first; addPrim prepends, so part 0
     * is drawn first and later parts land on top (the EU menu labels are a
     * dark plate, part 0, under their word). */
    for (i = piece->first_part; i < frame.count; ++i) {
#else
    /* The US logo was tuned compositing the parts last to first. */
    for (i = frame.count - 1; i >= 0; --i) {
#endif
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
        /* setAltClut(0, 0x1f0) stores Y=240 internally; the US 8-bit parts
         * add clutY=256, yielding the actual CLUT row 496. */
        palette_y = (mode ? (piece->alt ? 240 : 0) : TITLE_TEXTURE_Y) +
                    part->clutY + frame.row + piece->clut_row;
        if (palette_x < 0 || palette_x >= VRAM_WIDTH ||
            palette_y < 0 || palette_y >= VRAM_HEIGHT) continue;

        for (y = 0; y < part->h; ++y) {
            int dest_y = piece->y + part_y + y;
            if (dest_y < 0 || dest_y >= CANVAS_H) continue;
            for (x = 0; x < part->w; ++x) {
                int dest_x = piece->x + part_x + x;
                int source_x = source_u + x;
                int word_x, word_y, subpixel, index;
                u16 packed, color;

                if (dest_x < 0 || dest_x >= CANVAS_W) continue;
                word_x = (page_x + (source_x >> shift)) % VRAM_WIDTH;
                word_y = (TITLE_TEXTURE_Y + part->v + y) % VRAM_HEIGHT;
                packed = vram[word_y * VRAM_WIDTH + word_x];
                subpixel = source_x & mask;
                {
                    const unsigned bits_per_texel = 16u >> shift;
                    index = (packed >> (subpixel * bits_per_texel)) &
                            ((1u << bits_per_texel) - 1u);
                }
                color = vram[palette_y * VRAM_WIDTH +
                             ((palette_x + index) % VRAM_WIDTH)];
                composite_pixel(&canvas[dest_y * CANVAS_W + dest_x], color,
                                frame.abr);
            }
        }
    }
    return 1;
}

static int draw_layout(const u16 *vram, const s32 *sheet,
                       const LogoPiece *pieces, int count,
                       LogoPixel *canvas) {
    int i;

    for (i = 0; i < count; ++i) {
        if (!draw_frame(vram, sheet, &pieces[i], canvas)) return 0;
    }
    return 1;
}

#if TITLE_EU
/* Opaque bounding box of a composite; 0 when it is empty. */
static int opaque_bounds(const LogoPixel *canvas, int *x0, int *y0, int *x1,
                         int *y1) {
    int x, y;

    *x0 = CANVAS_W;
    *y0 = CANVAS_H;
    *x1 = -1;
    *y1 = -1;
    for (y = 0; y < CANVAS_H; ++y) {
        for (x = 0; x < CANVAS_W; ++x) {
            if (!canvas[y * CANVAS_W + x].rgba[3]) continue;
            if (x < *x0) *x0 = x;
            if (y < *y0) *y0 = y;
            if (x > *x1) *x1 = x;
            if (y > *y1) *y1 = y;
        }
    }
    return *x1 >= *x0;
}

/* The EU logo's opaque box, scaled like the US crop into the 720x326 logo
 * texture at the place its scaled sprites have; the rest stays transparent. */
static int fit_logo(const LogoPixel *canvas, uint8_t *output) {
    int x0, y0, x1, y1, w, h, ow, oh, ox, oy, y, rc;
    uint8_t *cropped, *scaled;

    if (!opaque_bounds(canvas, &x0, &y0, &x1, &y1)) return -1;
    w = x1 - x0 + 1;
    h = y1 - y0 + 1;
    /* Same pixels-per-sprite-pixel as the US crop (LOGO_OUTPUT_W / SOURCE_W)
     * and at the position the scaled game sprites have (patch 0129), so the
     * overlay logo has the size and place of the sprite logo it replaces. */
    ow = (w * LOGO_OUTPUT_W + LOGO_SOURCE_W / 2) / LOGO_SOURCE_W;
    oh = (h * LOGO_OUTPUT_W + LOGO_SOURCE_W / 2) / LOGO_SOURCE_W;
    ox = ((x0 - LOGO_CROP_X) * LOGO_OUTPUT_W + LOGO_SOURCE_W / 2) / LOGO_SOURCE_W;
    oy = ((y0 - LOGO_CROP_Y) * LOGO_OUTPUT_W + LOGO_SOURCE_W / 2) / LOGO_SOURCE_W;
    if (x0 < LOGO_CROP_X || y0 < LOGO_CROP_Y ||
        ox + ow > LOGO_OUTPUT_W || oy + oh > LOGO_OUTPUT_H) {
        /* Outside the US crop (a bank with another layout): fitted and
         * centered, as before. */
        if (w * LOGO_OUTPUT_H >= h * LOGO_OUTPUT_W) {
            ow = LOGO_OUTPUT_W;
            oh = (h * LOGO_OUTPUT_W + w / 2) / w;
        } else {
            oh = LOGO_OUTPUT_H;
            ow = (w * LOGO_OUTPUT_H + h / 2) / h;
        }
        ox = (LOGO_OUTPUT_W - ow) / 2;
        oy = (LOGO_OUTPUT_H - oh) / 2;
    }
    if (ow < 1 || oh < 1) return -1;
    cropped = (uint8_t *)malloc((size_t)w * h * 4);
    scaled = (uint8_t *)malloc((size_t)ow * oh * 4);
    if (!cropped || !scaled) {
        free(cropped);
        free(scaled);
        return -1;
    }
    for (y = 0; y < h; ++y) {
        memcpy(cropped + (size_t)y * w * 4,
               canvas[(y0 + y) * CANVAS_W + x0].rgba, (size_t)w * 4);
    }
    rc = dw3_pc_title_logo_resample(cropped, w, h, w * 4, scaled, ow, oh,
                                    ow * 4, DW3_PC_LOGO_LANCZOS2_FINE);
    if (rc == 0) {
        memset(output, 0, (size_t)LOGO_OUTPUT_W * LOGO_OUTPUT_H * 4);
        for (y = 0; y < oh; ++y) {
            memcpy(output + ((size_t)(oy + y) * LOGO_OUTPUT_W + ox) * 4,
                   scaled + (size_t)y * ow * 4, (size_t)ow * 4);
        }
    }
    fprintf(stderr, "dw3-title-logo: EU logo box %dx%d at %d,%d -> %dx%d at %d,%d\n",
            w, h, x0, y0, ow, oh, ox, oy);
    free(cropped);
    free(scaled);
    return rc;
}

/* One menu sprite as an 8-bit coverage mask at the overlay's lettering size:
 * the word only (part 0 is its dark plate), luminance of the opaque texels
 * with the dark outline ramped out, cropped to the opaque box and scaled
 * bilinearly to at most max_w x 30 canvas pixels. */
static int capture_label(const u16 *vram, const s32 *sheet, int frame_id,
                         int max_w, int slot) {
    LogoPiece piece = {0, 60, 100, 0, 0, 1};
    LogoPixel *canvas;
    uint8_t *lum = NULL, *mask = NULL;
    int x0, y0, x1, y1, w, h, ow, oh, x, y, peak = 1, rc = -1;
    float scale;

    piece.frame = (s16)frame_id;
    canvas = (LogoPixel *)calloc((size_t)CANVAS_W * CANVAS_H,
                                 sizeof(LogoPixel));
    if (!canvas) return -1;
    if (!draw_frame(vram, sheet, &piece, canvas) ||
        !opaque_bounds(canvas, &x0, &y0, &x1, &y1)) goto done;
    w = x1 - x0 + 1;
    h = y1 - y0 + 1;
    lum = (uint8_t *)malloc((size_t)w * h);
    if (!lum) goto done;
    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            const uint8_t *p = canvas[(y0 + y) * CANVAS_W + x0 + x].rgba;
            int v = p[3] ? (p[0] * 77 + p[1] * 150 + p[2] * 29) >> 8 : 0;
            lum[y * w + x] = (uint8_t)v;
            if (v > peak) peak = v;
        }
    }
    scale = 30.0f / (float)h;
    if (scale * (float)w > (float)max_w) scale = (float)max_w / (float)w;
    ow = (int)(w * scale + 0.5f);
    oh = (int)(h * scale + 0.5f);
    if (ow < 1 || oh < 1) goto done;
    mask = (uint8_t *)malloc((size_t)ow * oh);
    if (!mask) goto done;
    for (y = 0; y < oh; ++y) {
        float sy = ((float)y + 0.5f) / scale - 0.5f;
        int iy = (int)(sy < 0 ? -1 : sy);
        float fy = sy - (float)iy;
        for (x = 0; x < ow; ++x) {
            float sx = ((float)x + 0.5f) / scale - 0.5f;
            int ix = (int)(sx < 0 ? -1 : sx);
            float fx = sx - (float)ix, v = 0.0f;
            int dy, dx;

            for (dy = 0; dy < 2; ++dy) {
                for (dx = 0; dx < 2; ++dx) {
                    int px = ix + dx, py = iy + dy;
                    float wgt = (dx ? fx : 1.0f - fx) * (dy ? fy : 1.0f - fy);
                    if (px < 0 || py < 0 || px >= w || py >= h) continue;
                    v += wgt * (float)lum[py * w + px];
                }
            }
            v = (v / (float)peak - 0.2f) * 2.0f;
            v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
            mask[y * ow + x] = (uint8_t)(v * 255.0f + 0.5f);
        }
    }
    rc = dw3_pc_title_overlay_set_label(slot, mask, ow, oh);
    fprintf(stderr, "dw3-title-logo: EU label %d from sprite %d %dx%d -> %dx%d\n",
            slot, frame_id, w, h, ow, oh);
done:
    free(canvas);
    free(lum);
    free(mask);
    return rc;
}

/* LANGUAGE to the authored Options lettering, as the EU banks read: 0 is the
 * Japanese logo, 1 and 2 English, then French, Italian, German, Spanish. */
static int options_language(int language) {
    switch (language) {
    case 3: return DW3_TITLE_LANG_FR;
    case 4: return DW3_TITLE_LANG_IT;
    case 5: return DW3_TITLE_LANG_DE;
    case 6: return DW3_TITLE_LANG_ES;
    default: return DW3_TITLE_LANG_EN;
    }
}

static void capture_menu_labels(const u16 *vram, const s32 *sheet,
                                int language) {
    const u8 *sprites = k_menu_sprites[(language >= 0 && language < 7)
                                           ? language : 2];

    if (capture_label(vram, sheet, sprites[0], 270, DW3_TITLE_LABEL_START) ||
        capture_label(vram, sheet, sprites[1], 400, DW3_TITLE_LABEL_NEW_GAME) ||
        capture_label(vram, sheet, sprites[2], 400,
                      DW3_TITLE_LABEL_LOAD_GAME)) {
        /* Partial captures fall back to the built-in English labels. */
        dw3_pc_title_overlay_set_label(DW3_TITLE_LABEL_START, NULL, 0, 0);
        dw3_pc_title_overlay_set_label(DW3_TITLE_LABEL_NEW_GAME, NULL, 0, 0);
        dw3_pc_title_overlay_set_label(DW3_TITLE_LABEL_LOAD_GAME, NULL, 0, 0);
        fprintf(stderr, "dw3-title-logo: EU menu labels unavailable\n");
    }
    dw3_pc_title_overlay_set_options_language(options_language(language));
    /* The EU banks carry the same three credit lines as the US disc (checked
     * sprite by sprite, docs/TITLE_MENU_DESIGN.md), so the overlay draws its
     * native-resolution lines on every bank and the game's own credit
     * sprites (logo sprites 5/6) are hidden by patch 0073. */
    dw3_pc_title_overlay_set_footer(1);
}
#endif

static int capture_logo(const s32 *sheet) {
    RECT rect = {0, 0, VRAM_WIDTH, VRAM_HEIGHT};
    const LogoPiece *layout = k_layout_us;
    int layout_count = (int)(sizeof(k_layout_us) / sizeof(k_layout_us[0]));
    u16 *vram;
    LogoPixel *source;
    uint8_t *cropped;
    uint8_t *output;
    int x, y, rc, opaque_pixels = 0, colored_pixels = 0;
#if TITLE_EU
    const int language = LANGUAGE;
#endif

    g_logo_ready = 0;
    if (!sheet) return 0;
#if TITLE_EU
    if (language == 0) {
        layout = k_layout_eu_alt;
        layout_count =
            (int)(sizeof(k_layout_eu_alt) / sizeof(k_layout_eu_alt[0]));
    } else {
        layout = k_layout_eu;
        layout_count = (int)(sizeof(k_layout_eu) / sizeof(k_layout_eu[0]));
    }
#endif
    vram = (u16 *)calloc((size_t)VRAM_WIDTH * VRAM_HEIGHT, sizeof(u16));
    source = (LogoPixel *)calloc((size_t)CANVAS_W * CANVAS_H,
                                 sizeof(LogoPixel));
    cropped = (uint8_t *)malloc((size_t)LOGO_SOURCE_W * LOGO_SOURCE_H * 4);
    output = (uint8_t *)malloc((size_t)LOGO_OUTPUT_W * LOGO_OUTPUT_H * 4);
    if (!vram || !source || !cropped || !output) goto fail;

    Draw_StoreImage(&rect, (u_long *)vram);
    if (!draw_layout(vram, sheet, layout, layout_count, source)) goto fail;
    for (y = 0; y < CANVAS_H; ++y) {
        for (x = 0; x < CANVAS_W; ++x) {
            const LogoPixel *pixel = &source[y * CANVAS_W + x];
            if (pixel->rgba[3] != 0) {
                ++opaque_pixels;
                if (pixel->rgba[0] || pixel->rgba[1] || pixel->rgba[2])
                    ++colored_pixels;
            }
        }
    }
    if (opaque_pixels < 1000 || colored_pixels < 500) goto fail;
#if TITLE_EU
    rc = fit_logo(source, output);
#else
    for (y = 0; y < LOGO_SOURCE_H; ++y) {
        for (x = 0; x < LOGO_SOURCE_W; ++x) {
            memcpy(cropped + ((size_t)y * LOGO_SOURCE_W + x) * 4,
                   source[(y + LOGO_CROP_Y) * CANVAS_W + x + LOGO_CROP_X].rgba,
                   4);
        }
    }
    rc = dw3_pc_title_logo_resample(
        cropped, LOGO_SOURCE_W, LOGO_SOURCE_H, LOGO_SOURCE_W * 4,
        output, LOGO_OUTPUT_W, LOGO_OUTPUT_H, LOGO_OUTPUT_W * 4,
        DW3_PC_LOGO_LANCZOS2_FINE);
#endif
    if (rc != 0 || dw3_pc_title_menu_set_logo_rgba(
                       output, LOGO_OUTPUT_W, LOGO_OUTPUT_H,
                       LOGO_OUTPUT_W * 4) != 0) goto fail;
    g_logo_ready = 1;
#if TITLE_EU
    fprintf(stderr,
            "dw3-title-logo: runtime EU language %d sprites (%d opaque, "
            "%d colored) resampled Lanczos2+fine into %dx%d; original files "
            "remain external\n",
            language, opaque_pixels, colored_pixels, LOGO_OUTPUT_W,
            LOGO_OUTPUT_H);
#else
    fprintf(stderr,
            "dw3-title-logo: runtime US sprites 0-3 (%d opaque, %d colored) "
            "resampled Lanczos2+fine "
            "%dx%d -> %dx%d; original files remain external\n",
            opaque_pixels, colored_pixels,
            LOGO_SOURCE_W, LOGO_SOURCE_H, LOGO_OUTPUT_W, LOGO_OUTPUT_H);
#endif
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

#if TITLE_EU
/* The EU bank's colored parts can take CLUT rows the title fills in after
 * its sprites, so the logo is composed once the title stands still (the
 * first readiness check); the words of the menu are needed sooner. */
static const s32 *g_pending_sheet;

int dw3_port_title_logo_capture(const s32 *sheet) {
    RECT rect = {0, 0, VRAM_WIDTH, VRAM_HEIGHT};
    u16 *vram;

    g_logo_ready = 0;
    g_pending_sheet = sheet;
    if (!sheet) return 0;
    vram = (u16 *)calloc((size_t)VRAM_WIDTH * VRAM_HEIGHT, sizeof(u16));
    if (!vram) return 0;
    Draw_StoreImage(&rect, (u_long *)vram);
    capture_menu_labels(vram, sheet, LANGUAGE);
    free(vram);
    return 1;
}

int dw3_port_title_logo_ready(void) {
    if (g_pending_sheet) {
        const s32 *sheet = g_pending_sheet;
        g_pending_sheet = NULL;
        capture_logo(sheet);
    }
    return g_logo_ready;
}

/* 1 while the logo has still to be composed: that first readiness check costs a
 * frame of some hundred ms (0.24 s measured on PC), which the title prompt must
 * not stand on (title_ready.c). */
int dw3_port_title_logo_pending(void) {
    return g_pending_sheet != NULL;
}
#else
int dw3_port_title_logo_capture(const s32 *sheet) {
    return capture_logo(sheet);
}

int dw3_port_title_logo_ready(void) { return g_logo_ready; }
int dw3_port_title_logo_pending(void) { return 0; }
#endif
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
int dw3_port_title_logo_pending(void) { return 0; }
int dw3_port_title_logo_active(void) { return 0; }
void dw3_port_title_logo_set_active(int active) { (void)active; }
#endif
