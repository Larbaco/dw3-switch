/* Title-menu presentation: GPU overlay when available, PS1 primitives on GL. */
#include "port_title_options.h"
#include "engine/game.h"

/* PC SDL3 GPU and Switch GLES draw the native overlay; the PC SDL3 GL path
 * draws PS1-primitive plaques; anything else keeps the original-style row. */
#if defined(DW3_TITLE_NATIVE_OVERLAY)

extern void dw3_pc_title_menu_set_state(int active, int selected);
extern void dw3_pc_title_menu_set_fade(int level);

void dw3_port_title_overlay_fade(int level) {
    dw3_pc_title_menu_set_fade(level);
}

void dw3_port_title_options_draw(int layer_id, int x, int y, int selected) {
    (void)layer_id;
    (void)x;
    (void)y;
    if (GAME.funcs.getMode() != MODE_TITLE) {
        dw3_pc_title_menu_set_state(0, -2);
        return;
    }
    dw3_pc_title_menu_set_state(selected != -2, selected);
}

#elif defined(DW3_PC_HOST)

#include <libgpu.h>

enum { TITLE_MENU_W = 140, TITLE_MENU_H = 20 };

typedef struct TitleColor {
    int r;
    int g;
    int b;
} TitleColor;

static int title_glyph_code(char c) {
    if (c >= 'A' && c <= 'Z') {
        return 14 + c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return 40 + c - 'a';
    }
    return 0;
}

static int title_label_width(TextStyle *style, const char *label) {
    int width = 0;
    while (*label) {
        int code = title_glyph_code(*label++);
        if (code) {
            Glyph *glyph = &style->glyphs[code - 4];
            width += glyph->advance + glyph->dx;
        } else {
            width += 5;
        }
    }
    return width;
}

static void title_draw_glyph_pass(PrimPtr *prim, u_long *ot, TextStyle *style,
                                  const char *label, int x, int y, int palette,
                                  TitleColor tint) {
    int width = title_label_width(style, label);
    x += (TITLE_MENU_W - width) / 2;
    while (*label) {
        int code = title_glyph_code(*label++);
        Glyph *glyph;
        if (!code) {
            x += 5;
            continue;
        }
        glyph = &style->glyphs[code - 4];
        if (glyph->page != 0xFF) {
            setPolyFT4(prim->ft4);
            setRGB0(prim->ft4, tint.r, tint.g, tint.b);
            setXYWH(prim->ft4, x + glyph->dx, y + glyph->dy,
                    glyph->w, glyph->h);
            setUVWH(prim->ft4, glyph->u, glyph->v,
                    glyph->w - 1, glyph->h - 1);
            prim->ft4->tpage =
                getTPage(0, 0, 0x140 + (glyph->page << 6), 0);
            prim->ft4->clut = getClut(
                0x140 + glyph->clutX, glyph->clutY + palette);
            addPrim(ot, prim->any);
            prim->ft4++;
        }
        x += glyph->advance + glyph->dx;
    }
}

static TILE *title_add_rect(TILE *tile, u_long *ot, int x, int y, int w, int h,
                            TitleColor color) {
    setTile(tile);
    setRGB0(tile, color.r, color.g, color.b);
    setXY0(tile, x, y);
    setWH(tile, w, h);
    addPrim(ot, tile);
    return tile + 1;
}

static TILE *title_draw_plaque(TILE *tile, u_long *ot, int x, int y,
                               int selected) {
    const TitleColor shadow = {0, 1, 7};
    const TitleColor steel_dark = {18, 39, 79};
    const TitleColor steel = {31, 76, 143};
    const TitleColor steel_mid = {52, 107, 180};
    const TitleColor steel_light = {92, 148, 213};
    const TitleColor steel_glint = {143, 190, 239};
    const TitleColor face_edge = {8, 21, 52};
    const TitleColor face = {5, 17, 43};
    const TitleColor face_inset = {9, 25, 59};
    const TitleColor rail = selected ? (TitleColor){255, 166, 35}
                                     : (TitleColor){118, 64, 29};
    const TitleColor fastener = selected ? (TitleColor){255, 184, 45}
                                         : (TitleColor){104, 74, 43};
    const TitleColor fastener_shadow = selected ? (TitleColor){141, 70, 18}
                                                : (TitleColor){40, 37, 38};
    const TitleColor glow = selected ? (TitleColor){255, 139, 28}
                                     : (TitleColor){47, 95, 158};
    int w = TITLE_MENU_W;
    int h = TITLE_MENU_H;

    /* The OT is traversed newest-first, so earlier submissions sit in front.
     * Draw backplates first, then the inset face and finally small details. */
    tile = title_add_rect(tile, ot, x - 1, y + 7, 1, 6, glow);
    tile = title_add_rect(tile, ot, x + w, y + 7, 1, 6, glow);

    /* Single-pixel rails sit clear of the resident font's ink box. */
    tile = title_add_rect(tile, ot, x + 14, y + 3, w - 28, 1, rail);
    tile = title_add_rect(tile, ot, x + 14, y + h - 2, w - 28, 1, rail);

    /* Submit each bolt before its rim and corner plate. Since older OT entries
     * render in front, the amber core remains visible instead of being buried
     * under the plate fill. */
    tile = title_add_rect(tile, ot, x + 6, y + 4, 3, 3, fastener);
    tile = title_add_rect(tile, ot, x + w - 9, y + 4, 3, 3, fastener);
    tile = title_add_rect(tile, ot, x + 6, y + h - 7, 3, 3, fastener);
    tile = title_add_rect(tile, ot, x + w - 9, y + h - 7, 3, 3, fastener);
    tile = title_add_rect(tile, ot, x + 5, y + 3, 5, 5, fastener_shadow);
    tile = title_add_rect(tile, ot, x + w - 10, y + 3, 5, 5,
                          fastener_shadow);
    tile = title_add_rect(tile, ot, x + 5, y + h - 8, 5, 5,
                          fastener_shadow);
    tile = title_add_rect(tile, ot, x + w - 10, y + h - 8, 5, 5,
                          fastener_shadow);

    /* Five stepped rows form clipped corner clamps around the square bolts. */
    tile = title_add_rect(tile, ot, x + 4, y + 2, 5, 1, steel_mid);
    tile = title_add_rect(tile, ot, x + 2, y + 3, 8, 1, steel);
    tile = title_add_rect(tile, ot, x + 2, y + 4, 9, 1, steel_mid);
    tile = title_add_rect(tile, ot, x + 3, y + 5, 8, 1, steel);
    tile = title_add_rect(tile, ot, x + 4, y + 6, 6, 1, steel_dark);
    tile = title_add_rect(tile, ot, x + w - 9, y + 2, 5, 1, steel_mid);
    tile = title_add_rect(tile, ot, x + w - 10, y + 3, 8, 1, steel);
    tile = title_add_rect(tile, ot, x + w - 11, y + 4, 9, 1, steel_mid);
    tile = title_add_rect(tile, ot, x + w - 11, y + 5, 8, 1, steel);
    tile = title_add_rect(tile, ot, x + w - 10, y + 6, 6, 1, steel_dark);
    tile = title_add_rect(tile, ot, x + 4, y + h - 3, 5, 1, steel_mid);
    tile = title_add_rect(tile, ot, x + 2, y + h - 4, 8, 1, steel);
    tile = title_add_rect(tile, ot, x + 2, y + h - 5, 9, 1, steel_mid);
    tile = title_add_rect(tile, ot, x + 3, y + h - 6, 8, 1, steel);
    tile = title_add_rect(tile, ot, x + 4, y + h - 7, 6, 1, steel_dark);
    tile = title_add_rect(tile, ot, x + w - 9, y + h - 3, 5, 1, steel_mid);
    tile = title_add_rect(tile, ot, x + w - 10, y + h - 4, 8, 1, steel);
    tile = title_add_rect(tile, ot, x + w - 11, y + h - 5, 9, 1, steel_mid);
    tile = title_add_rect(tile, ot, x + w - 11, y + h - 6, 8, 1, steel);
    tile = title_add_rect(tile, ot, x + w - 10, y + h - 7, 6, 1, steel_dark);

    /* Short side ticks echo the Digivice data marks without crowding text. */
    tile = title_add_rect(tile, ot, x + 12, y + 8, 2, 1, rail);
    tile = title_add_rect(tile, ot, x + 12, y + 10, 2, 1, steel_light);
    tile = title_add_rect(tile, ot, x + 12, y + 12, 2, 1, rail);
    tile = title_add_rect(tile, ot, x + w - 14, y + 8, 2, 1, rail);
    tile = title_add_rect(tile, ot, x + w - 14, y + 10, 2, 1, steel_light);
    tile = title_add_rect(tile, ot, x + w - 14, y + 12, 2, 1, rail);

    /* Deep indigo face, above the outer body and below the hardware details. */
    tile = title_add_rect(tile, ot, x + 16, y + 7, w - 32, h - 14,
                          face_inset);
    tile = title_add_rect(tile, ot, x + 13, y + 5, w - 26, h - 10, face);
    tile = title_add_rect(tile, ot, x + 11, y + 4, w - 22, h - 8,
                          face_edge);

    /* Three clipped steel-blue bevel bands and dark side returns. */
    tile = title_add_rect(tile, ot, x + 11, y + 1, w - 22, 1, steel_glint);
    tile = title_add_rect(tile, ot, x + 8, y + 2, w - 16, 1, steel_light);
    tile = title_add_rect(tile, ot, x + 11, y + h - 2, w - 22, 1, steel);
    tile = title_add_rect(tile, ot, x + 8, y + h - 3, w - 16, 1,
                          steel_dark);
    tile = title_add_rect(tile, ot, x + 2, y + 5, 1, h - 10, steel_light);
    tile = title_add_rect(tile, ot, x + w - 3, y + 5, 1, h - 10, steel_dark);

    /* Stepped outer silhouette and shadow are the backmost layers. */
    tile = title_add_rect(tile, ot, x + 10, y, w - 20, 1, steel_dark);
    tile = title_add_rect(tile, ot, x + 5, y + 1, w - 10, 2, steel);
    tile = title_add_rect(tile, ot, x + 2, y + 5, 2, h - 10, steel_dark);
    tile = title_add_rect(tile, ot, x + w - 4, y + 5, 2, h - 10, steel_dark);
    tile = title_add_rect(tile, ot, x + 5, y + h - 3, w - 10, 2, steel);
    tile = title_add_rect(tile, ot, x + 10, y + h - 1, w - 20, 1,
                          steel_dark);
    tile = title_add_rect(tile, ot, x + 9, y + 1, w - 18, h - 2, shadow);
    return tile;
}

void dw3_port_title_options_draw(int layer_id, int x, int y, int selected) {
    static const char *const labels[] = {"New Game", "Options", "Load Game"};
    if (selected < 0) {
        return;
    }
    TextStyle *style = &FONT.styles[1];
    Layer *layer = GFX.funcs.getLayer(layer_id);
    u_long *ot = layer->getOtEntry(layer, 0);
    PrimPtr prim;
    TILE *tiles;
    int panel_x = x + (164 - TITLE_MENU_W) / 2;
    int row;

    prim.any = GFX.funcs.getPrim();
    for (row = 0; row < 3; row++) {
        int row_y = y + (row - 1) * 16;
        title_draw_glyph_pass(&prim, ot, style, labels[row], panel_x + 1,
                              row_y + 6, PALETTE_WHITE,
                              (TitleColor){22, 22, 28});
        title_draw_glyph_pass(&prim, ot, style, labels[row],
                              panel_x, row_y + 5, PALETTE_WHITE,
                              (TitleColor){128, 128, 110});
    }

    /* The OT draws newest entries first; adding plaques last keeps the
     * labels in front of their opaque faces. */
    tiles = (TILE *)prim.any;
    for (row = 0; row < 3; row++) {
        int row_y = y + (row - 1) * 16;
        tiles = title_draw_plaque(tiles, ot, panel_x, row_y, selected == row);
    }
    GFX.funcs.setPrim(tiles);
}

#else

static int title_glyph_code(char c) {
    if (c >= 'A' && c <= 'Z') {
        return 14 + c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return 40 + c - 'a';
    }
    return 0;
}

/* Metrics of the three title rows, shared with the original sprites: each
 * button background is 164x16 and the selection frame is drawn one pixel
 * outside it. Stacking the rows at a 16-pixel pitch leaves no gap between
 * the backgrounds, so the added row reads as a third original button. */
#define ROW_W 164
#define ROW_H 16
/* The original lettering is about 11 pixels tall, the resident font is
 * about 8. Scale the glyph quads by 5/4 so the label has the same weight. */
#define SCALE_NUM 5
#define SCALE_DEN 4
#define SPACE_ADV 6

/* Modulation (128 is neutral) for the dim, unselected state. The font's
 * dark-blue CLUT row loses the letter counters at this size, so the dim
 * state is the same blue row at reduced brightness. */
#define FILL_DIM 80
#define FILL_LIT 128

static int scaled(int v) {
    return (v * SCALE_NUM + (SCALE_DEN / 2)) / SCALE_DEN;
}

static PrimPtr title_glyph(PrimPtr prim, u_long *ot, Glyph *glyph, int x,
                           int y, int clutRow, int rgb) {
    int gw = scaled(glyph->w);
    int gh = scaled(glyph->h);

    setPolyFT4(prim.ft4);
    setRGB0(prim.ft4, rgb, rgb, rgb);
    setXYWH(prim.ft4, x + scaled(glyph->dx), y + scaled(glyph->dy), gw, gh);
    setUVWH(prim.ft4, glyph->u, glyph->v, glyph->w - 1, glyph->h - 1);
    prim.ft4->tpage = getTPage(0, 0, 0x140 + (glyph->page << 6), 0);
    prim.ft4->clut = getClut(0x140 + glyph->clutX, glyph->clutY + clutRow);
    addPrim(ot, prim.any);
    prim.ft4++;
    return prim;
}

void dw3_port_title_options_draw(int layer_id, int x, int y, int selected) {
    static const char label[] = "Port options";
    /* Drop shadow only: a full outline fills the counters of o and p. */
    static const signed char outline[2][2] = {{1, 1}, {2, 1}};
    TextStyle *style = &FONT.styles[1];
    Layer *layer = GFX.funcs.getLayer(layer_id);
    u_long *ot = layer->getOtEntry(layer, 0);
    PrimPtr prim;
    Glyph *glyph;
    TILE *tiles;
    int width = 0;
    int i;
    int j;
    int code;
    int panel_x = x;
    int text_x;
    int text_y;
    int fill = PALETTE_BLUE;
    int fill_rgb = selected ? FILL_LIT : FILL_DIM;

    /* Title has already loaded FONT. Reuse its proportional glyph advances
     * and blue CLUTs, without allocating a task or loading a second font. */
    for (i = 0; label[i] != 0; i++) {
        code = title_glyph_code(label[i]);
        width += code ? scaled(style->glyphs[code - 4].advance +
                               style->glyphs[code - 4].dx) : SPACE_ADV;
    }
    text_x = panel_x + (ROW_W - width) / 2;
    /* Centre the capital P (no descender) vertically in the button. */
    glyph = &style->glyphs[title_glyph_code('P') - 4];
    text_y = y + (ROW_H - scaled(glyph->h)) / 2 - scaled(glyph->dy);

    prim.any = GFX.funcs.getPrim();
    /* Painted in reverse submission order: fill first on top, then the
     * bold pass, then the dark shadow, then the stripes underneath. */
    x = text_x;
    for (i = 0; label[i] != 0; i++) {
        code = title_glyph_code(label[i]);
        if (!code) {
            x += SPACE_ADV;
            continue;
        }
        glyph = &style->glyphs[code - 4];
        if (glyph->page != 0xFF) {
            prim = title_glyph(prim, ot, glyph, x, text_y, fill, fill_rgb);
        }
        x += scaled(glyph->advance + glyph->dx);
    }
    x = text_x;
    for (i = 0; label[i] != 0; i++) {
        code = title_glyph_code(label[i]);
        if (!code) {
            x += SPACE_ADV;
            continue;
        }
        glyph = &style->glyphs[code - 4];
        if (glyph->page != 0xFF) {
            prim = title_glyph(prim, ot, glyph, x + 1, text_y, fill, fill_rgb);
        }
        x += scaled(glyph->advance + glyph->dx);
    }
    for (j = 0; j < 2; j++) {
        x = text_x;
        for (i = 0; label[i] != 0; i++) {
            code = title_glyph_code(label[i]);
            if (!code) {
                x += SPACE_ADV;
                continue;
            }
            glyph = &style->glyphs[code - 4];
            if (glyph->page != 0xFF) {
                prim = title_glyph(prim, ot, glyph, x + outline[j][0],
                                   text_y + outline[j][1], PALETTE_DARK_BLUE, 0);
            }
            x += scaled(glyph->advance + glyph->dx);
        }
    }
    /* Same one-pixel black/grey stripe rhythm as the title button sprites,
     * across the full 16-pixel button height. The original selection
     * cursor is drawn separately, only when selected. */
    tiles = (TILE *)prim.any;
    for (i = 0; i < ROW_H; i++) {
        int shade = (i & 1) ? 8 : 32;
        setTile(tiles);
        setRGB0(tiles, shade, shade, shade);
        setXY0(tiles, panel_x, y + i);
        setWH(tiles, ROW_W, 1);
        addPrim(ot, tiles);
        tiles++;
    }
    GFX.funcs.setPrim(tiles);
}

#endif
