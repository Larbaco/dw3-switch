/* Title entry rendered through the original game's font and ordering table. */
#include "port_title_options.h"
#include "engine/game.h"

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

/* Modulation (128 is neutral) for the dim, unselected state. The original
 * sprites use a dark-blue CLUT row for the same purpose. */
#define FILL_DIM 128
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
    int fill = selected ? PALETTE_BLUE : PALETTE_DARK_BLUE;
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
