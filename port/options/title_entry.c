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

void dw3_port_title_options_draw(int layer_id, int x, int y, int selected) {
    static const char label[] = "Port options";
    TextStyle *style = &FONT.styles[1];
    Layer *layer = GFX.funcs.getLayer(layer_id);
    u_long *ot = layer->getOtEntry(layer, 0);
    PrimPtr prim;
    Glyph *glyph;
    TILE *tiles;
    int width = 0;
    int i;
    int code;
    int panel_x = x;

    /* Title has already loaded FONT. Reuse its proportional glyph advances
     * and blue CLUTs, without allocating a task or loading a second font. */
    for (i = 0; label[i] != 0; i++) {
        code = title_glyph_code(label[i]);
        width += code ? style->glyphs[code - 4].advance +
                            style->glyphs[code - 4].dx : 5;
    }
    x += (164 - width) / 2;
    prim.any = GFX.funcs.getPrim();
    for (i = 0; label[i] != 0; i++) {
        code = title_glyph_code(label[i]);
        if (!code) {
            x += 5;
            continue;
        }
        glyph = &style->glyphs[code - 4];
        if (glyph->page != 0xFF) {
            setPolyFT4(prim.ft4);
            setRGB0(prim.ft4, 128, 128, 128);
            setXYWH(prim.ft4, x + glyph->dx, y + glyph->dy,
                    glyph->w, glyph->h);
            setUVWH(prim.ft4, glyph->u, glyph->v,
                    glyph->w - 1, glyph->h - 1);
            prim.ft4->tpage = getTPage(0, 0, 0x140 + (glyph->page << 6), 0);
            prim.ft4->clut = getClut(0x140 + glyph->clutX,
                glyph->clutY + (selected ? PALETTE_BLUE : PALETTE_DARK_BLUE));
            addPrim(ot, prim.any);
            prim.ft4++;
        }
        x += glyph->advance + glyph->dx;
    }
    /* Same one-pixel black/grey stripe rhythm as the title button sprites.
     * Submitted after the letters so the reversed OT paints it underneath.
     * The original selection cursor is drawn separately, only when selected. */
    tiles = (TILE *)prim.any;
    for (i = 0; i < 14; i++) {
        int shade = (i & 1) ? 8 : 32;
        setTile(tiles);
        setRGB0(tiles, shade, shade, shade);
        setXY0(tiles, panel_x, y + i);
        setWH(tiles, 164, 1);
        addPrim(ot, tiles);
        tiles++;
    }
    GFX.funcs.setPrim(tiles);
}
