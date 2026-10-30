/* P10.6 Switch glyphs (docs/P10.6_GLYPHS.md): the game draws its button icons
 * as cells of the font (FONT_ICONS_1, size 1 only). With the Switch glyphs
 * option on, the port replaces the four cells of the PS buttons in VRAM, after
 * the font is loaded, with the glyph of the physical button that produces that
 * PS button under the current map (docs/CONTROLS_BUNDLE.md). No decomp edit
 * for the patch itself: the port wraps FONT.load (port/options/switch_glyphs.c).
 *
 * Pure part first (art, role-to-palette maps, packing, the plan per map): host
 * tested by tools/tests/switch_glyphs_main.c. Then the host part. */
#ifndef DW3_PORT_GLYPHS_H
#define DW3_PORT_GLYPHS_H

#include "port_button_map.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The glyphs, in the order of tools/generate_switch_glyphs.py (NAMES). */
enum {
    DW3_GLYPH_A = 0,
    DW3_GLYPH_B = 1,
    DW3_GLYPH_X = 2,
    DW3_GLYPH_Y = 3,
    DW3_GLYPH_L = 4,
    DW3_GLYPH_R = 5,
    DW3_GLYPH_ZL = 6,
    DW3_GLYPH_ZR = 7,
    DW3_GLYPH_COUNT = 8,
    DW3_GLYPH_NONE = -1
};

/* One glyph is 12 x 12 pixels of five roles (the art is stored by role, not by
 * colour; each region maps the roles to palette indices of its font CLUT). */
enum {
    DW3_GLYPH_W = 12,
    DW3_GLYPH_H = 12,
    DW3_GLYPH_PIXELS = 144,
    DW3_GLYPH_PACKED = 72, /* 4 bpp, low nibble first: 6 bytes x 12 rows */
    DW3_ROLE_CLEAR = 0,
    DW3_ROLE_OUTLINE = 1,
    DW3_ROLE_BODY = 2,
    DW3_ROLE_LETTER = 3,
    DW3_ROLE_SHADE = 4,
    DW3_ROLE_COUNT = 5
};

/* The four PS buttons the font has an icon for, in this order, and the 1-based
 * icon number n of each (the drawn cell is icons[n - 1]; sizes 2 and 3 have
 * none, the tables are page 0xFF there). */
enum { DW3_GLYPH_CELLS = 4 };

typedef struct Dw3GlyphCell {
    int ps_button; /* DW3_BTN_CROSS, CIRCLE, SQUARE or TRIANGLE */
    int n;         /* the icon number in FONT_ICONS_1 */
    int glyph;     /* DW3_GLYPH_*, or DW3_GLYPH_NONE: keep the original icon */
} Dw3GlyphCell;

/* "A", "B", "X", "Y", "L", "R", "ZL", "ZR" (what dw3_button_cap_label gives)
 * -> the glyph; DW3_GLYPH_NONE for anything else ("L3", "R3", "?"). */
int dw3_glyph_from_cap(const char *cap);
const char *dw3_glyph_name(int glyph);

/* The compiled-in art of a glyph: DW3_GLYPH_PIXELS role bytes, row by row, or
 * NULL for an unknown glyph. */
const unsigned char *dw3_glyph_art(int glyph);

/* The role -> font palette index map of a region (US: the icon takes the text
 * colour, so three of the original's indices; EU: greys of the icon palette). */
const unsigned char *dw3_glyph_index_map(int eu);

/* Pack role art into the 4 bpp bytes of a cell: out[DW3_GLYPH_PACKED], the low
 * nibble of a byte is the left pixel. Returns 0, or -1 on a bad role. */
int dw3_glyph_pack(const unsigned char roles[DW3_GLYPH_PIXELS],
                   const unsigned char map[DW3_ROLE_COUNT],
                   unsigned char out[DW3_GLYPH_PACKED]);

/* Decode an RGBA image (what the PNG loader gives) of the five role colours of
 * the art into role bytes. Returns 0, or -1 with the reason in `why`. */
int dw3_glyph_roles_from_rgba(const unsigned char *rgba, int w, int h,
                              unsigned char out[DW3_GLYPH_PIXELS], char *why,
                              unsigned why_cap);

/* The art of one glyph from a PNG file (the owner's override, or the bundled
 * file): 12x12 in the role colours of assets/glyphs. Returns 0, or -1 with the
 * reason in `why` (a missing file reads "cannot open ..."). */
int dw3_glyph_art_from_png(const char *path,
                           unsigned char out[DW3_GLYPH_PIXELS], char *why,
                           unsigned why_cap);

/* The cells to draw for `map` (dw3_button_map_current): the glyph of each of
 * the four icons. `eu` selects the region's cells. Returns how many cells get a
 * glyph (the rest keep the original icon). */
int dw3_glyph_plan(const unsigned char map[DW3_BTN_COUNT], int eu,
                   Dw3GlyphCell out[DW3_GLYPH_CELLS]);

/* The `patched N cells (...)` list of a plan: "n=47:A n=50:Y ..." (a cell that
 * keeps the original icon is "n=27:orig"). */
void dw3_glyph_plan_text(const Dw3GlyphCell cells[DW3_GLYPH_CELLS], char *out,
                         unsigned cap);

/* --- host part --------------------------------------------------------- */

/* The unit tests of the options and the button map link no font. */
#if defined(DW3_PORT_OPTIONS_TEST) || defined(DW3_BUTTON_MAP_TEST)
static inline void dw3_glyphs_init(void) {}
static inline void dw3_glyphs_refresh(const char *why) { (void)why; }
#else
/* Wrap FONT.load: after the font is in VRAM, patch the cells when the option is
 * on. Called once at boot (after the option file is read). */
void dw3_glyphs_init(void);
/* The option, the layout or the map changed (or the font was just loaded):
 * bring the cells in line, log the result once. Safe before the font exists. */
void dw3_glyphs_refresh(const char *why);
/* DW3_ICON_CENSUS=1: log once per distinct (icon n, text style, string) with the
 * scene. Called from processTextChar (patch 0195); costs a flag test when off. */
void dw3_port_icon_census(int n, int style, const unsigned char *data, int pos,
                          int sjis);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_GLYPHS_H */
