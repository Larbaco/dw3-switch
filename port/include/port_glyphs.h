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

/* The page-turn prompt (docs/P10.6_GLYPHS.md "Page-turn prompt"): the item shop,
 * the field menu's item list and a few other menus print `L1` / `R1` as the plain
 * letters of the string (bytes 19 05 / 1F 05) next to a wedge sprite. With the
 * glyphs on, the port hands those windows an icon string (`01 n`) for one of two
 * spare 12x12 cells it adds to the size-1 icon table (n = 115 for L1, 116 for
 * R1), drawn with the glyph of the Switch button that produces L1 / R1. */
enum {
    DW3_PAGETURN_COUNT = 2, /* L1, R1 */
    DW3_PAGETURN_N0 = 115,  /* icon number of the L1 cell; R1 is the next */
    DW3_PAGETURN_U = 232,   /* the cells: font page 0, u, v (blank in US and EU) */
    DW3_PAGETURN_V0 = 232,  /* was 224 up to p56: rows 224..231 are EU palette rows */
    DW3_PAGETURN_V_STEP = 12
};

/* The US Cross prompt (bug 4, docs/P10.6_GLYPHS.md section 10). The US memory card
 * screens (Load Game, Save) spell the confirm button as the plain letter X of the
 * string ("X Button: Execute", bytes 25 01 01 "Button"), not as a font icon: the US
 * cross icon n = 27 is the multiplication sign of item counts and stays as it is.
 * With the glyphs on, the port rewrites that letter into an icon string for a third
 * spare cell (n = 117), drawn with the glyph of the Switch button that produces
 * the Cross. EU draws the prompt with the n = 27 disc, which the cell patch covers.
 * The cell sits on font page 0 at u = 220, v = 232. The only 12 x 12 texels of the
 * page that no glyph or icon cell of either region references and that are blank in
 * both discs are in the font page's palette strip (halfwords 48..63 of the rows
 * 232..255 hold the text colour ramps, which use indices 0..6 and 15 only), so the
 * cell overwrites ramp entries 7..9 that nothing reads; the L1 / R1 cells above are in
 * the same strip (u = 232), which is why they start at row 232: rows 224..231 are the
 * EU icon palette (13 colours, indices 10..12 are real) and must not be written. */
enum {
    DW3_SPARE_COUNT = DW3_PAGETURN_COUNT + 1, /* L1, R1, US Cross */
    DW3_USCROSS_SLOT = 2,                     /* index of the US Cross cell */
    DW3_USCROSS_N = 117,                      /* its icon number */
    DW3_USCROSS_U = 220,
    DW3_USCROSS_V = 232
};
/* The glyph for the US Cross prompt under `map` (DW3_GLYPH_NONE = keep the X). */
int dw3_glyph_uscross_plan(const unsigned char map[DW3_BTN_COUNT]);
/* Rewrites every "X Button" prompt of `str` (a letter X at the start of the string
 * or after a space, a new line or an icon, then a space and the word "Button") so
 * the X is the icon string 01 n of the US Cross cell. Returns how many were
 * rewritten (0 = `out` not written, use `str`), or -1 when the result does not fit
 * in `cap` bytes. `out` ends with a NUL. */
int dw3_glyph_uscross_rewrite(const char *str, char *out, unsigned cap);

/* 0 for the string `L1`, 1 for `R1` (exactly those two letters), else -1. */
int dw3_glyph_pageturn_match(const char *str);
/* The glyph for L1 and R1 under `map` (DW3_GLYPH_NONE = keep the text). Returns
 * how many got a glyph. */
int dw3_glyph_pageturn_plan(const unsigned char map[DW3_BTN_COUNT],
                            int out[DW3_PAGETURN_COUNT]);
/* The icon string of a side: the bytes 01 n 00 00. */
const char *dw3_glyph_pageturn_string(int side);

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
/* Patch 0196, textWindowSetSubString: the string a text window of `style` (a
 * TextStyle *) shows for table string `str`. Returns `str` unless it is a size-1
 * window's exact `L1` / `R1`, or (US) a string with an "X Button" prompt, and the
 * glyph option gives that button a glyph. */
const char *dw3_port_text_override(const void *style, const char *str);
/* P6.32 phase 7: the art the map view's help line draws for a glyph (12 x 12 role
 * bytes): the owner's PNG when there is one in the glyphs folder, else the bundled
 * art. NULL for an unknown glyph. */
const unsigned char *dw3_glyphs_hud_art(int glyph);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_GLYPHS_H */
