/* Title entry rendered through the original game's font and ordering table. */
#include "port_title_options.h"
#include "engine/game.h"

#if defined(DW3_PC_HOST)

#include <libgpu.h>

enum { TITLE_MENU_X = 78, TITLE_MENU_W = 164, TITLE_MENU_H = 14 };

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
                                  const char *label, int x, int y, int palette) {
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
            setRGB0(prim->ft4, 128, 128, 128);
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
    const TitleColor shadow = {1, 3, 13};
    const TitleColor edge = selected ? (TitleColor){72, 126, 203}
                                     : (TitleColor){32, 65, 119};
    const TitleColor face = {5, 14, 40};
    const TitleColor top = selected ? (TitleColor){123, 172, 231}
                                    : (TitleColor){74, 119, 177};
    const TitleColor bottom = {13, 29, 57};
    const TitleColor rail = selected ? (TitleColor){255, 151, 33}
                                     : (TitleColor){164, 83, 28};
    const TitleColor fastener = selected ? (TitleColor){255, 194, 70}
                                         : (TitleColor){183, 113, 39};
    int w = TITLE_MENU_W;
    int h = TITLE_MENU_H;

    if (selected) {
        tile = title_add_rect(tile, ot, x - 1, y - 1, w + 2, 1,
                              (TitleColor){232, 128, 28});
        tile = title_add_rect(tile, ot, x - 1, y + h, w + 2, 1,
                              (TitleColor){232, 128, 28});
    }
    tile = title_add_rect(tile, ot, x + 1, y + 1, w, h, shadow);
    tile = title_add_rect(tile, ot, x, y + 1, w, h - 2, edge);
    tile = title_add_rect(tile, ot, x + 2, y, w - 4, h, edge);
    tile = title_add_rect(tile, ot, x + 4, y + 2, w - 8, h - 4, face);

    /* Bevels, amber rails and square corner fasteners stay pixel aligned. */
    tile = title_add_rect(tile, ot, x + 7, y + 1, w - 14, 1, top);
    tile = title_add_rect(tile, ot, x + 7, y + h - 2, w - 14, 1, bottom);
    tile = title_add_rect(tile, ot, x + 14, y + 3, w - 28, 1, rail);
    tile = title_add_rect(tile, ot, x + 14, y + h - 4, w - 28, 1, rail);
    tile = title_add_rect(tile, ot, x + 3, y + 3, 2, 2, fastener);
    tile = title_add_rect(tile, ot, x + w - 5, y + 3, 2, 2, fastener);
    tile = title_add_rect(tile, ot, x + 3, y + h - 5, 2, 2, fastener);
    tile = title_add_rect(tile, ot, x + w - 5, y + h - 5, 2, 2,
                          fastener);
    tile = title_add_rect(tile, ot, x + 8, y + 5, 2, 1, rail);
    tile = title_add_rect(tile, ot, x + 8, y + 7, 2, 1, rail);
    tile = title_add_rect(tile, ot, x + w - 10, y + 5, 2, 1, rail);
    tile = title_add_rect(tile, ot, x + w - 10, y + 7, 2, 1, rail);
    return tile;
}

void dw3_port_title_options_draw(int layer_id, int x, int y, int selected) {
    static const char *const labels[] = {"New Game", "Options", "Load Game"};
    TextStyle *style = &FONT.styles[1];
    Layer *layer = GFX.funcs.getLayer(layer_id);
    u_long *ot = layer->getOtEntry(layer, 0);
    PrimPtr prim;
    TILE *tiles;
    int row;

    (void)x;
    prim.any = GFX.funcs.getPrim();
    for (row = 0; row < 3; row++) {
        int row_y = y + (row - 1) * 16;
        title_draw_glyph_pass(&prim, ot, style, labels[row],
                              TITLE_MENU_X, row_y + 3, PALETTE_WHITE);
    }
    for (row = 0; row < 3; row++) {
        int row_y = y + (row - 1) * 16;
        title_draw_glyph_pass(&prim, ot, style, labels[row],
                              TITLE_MENU_X + 1, row_y + 4,
                              PALETTE_DARK_BLUE);
    }

    /* Added last in OT order so each opaque plaque sits under text/sprites. */
    tiles = (TILE *)prim.any;
    for (row = 0; row < 3; row++) {
        int row_y = y + (row - 1) * 16;
        tiles = title_draw_plaque(tiles, ot, TITLE_MENU_X, row_y,
                                  selected == row);
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
