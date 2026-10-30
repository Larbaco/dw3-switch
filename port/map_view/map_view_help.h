/* P6.32 phase 7: the help line of the full-screen map view (the green footer of
 * the bars), pure part. The line names the buttons the way the player sees them:
 *   - the button of each action is the one the game reads (the phase 4 bindings,
 *     map_view_*) seen through the button map in force (button_layout, the custom
 *     map): the PS button i is made by the position map[i];
 *   - on the Switch the position is a Switch button: its cap text ("A", "ZL",
 *     "MINUS"), or with switch_glyphs=1 the Switch glyph of assets/glyphs/ when the
 *     button has one;
 *   - on the PC it is the PS word of the position ("SQUARE", "TRI", "L1"), which is
 *     the PS button's own word under the PlayStation layout;
 *   - the words around the names are in the language of the game's text (EN, and
 *     on the EU disc FR, DE, IT, ES), in capitals and ASCII only because the port's
 *     8x8 font has nothing else (German umlauts and sharp s are written out, the
 *     other languages drop the accents).
 * The translations are machine drafts that need native review, like the help
 * pages (docs/P6.32_MAP_ZOOM.md section 17).
 *
 * No game headers: stdio/string only, so tools/tests builds it with a plain
 * compiler (tools/tests/map_view_help_main.c). A glyph in a line is one byte,
 * MV_HELP_GLYPH0 + the DW3_GLYPH_* number; the renderer draws it 12 x 12. */
#ifndef DW3_MAP_VIEW_HELP_H
#define DW3_MAP_VIEW_HELP_H

#include <stddef.h>

#include "map_view_core.h"
#include "port_button_map.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The languages of the help line, in the order of the tables. */
enum {
    MV_HELP_EN = 0,
    MV_HELP_FR = 1,
    MV_HELP_DE = 2,
    MV_HELP_IT = 3,
    MV_HELP_ES = 4,
    MV_HELP_LANGS = 5
};

/* The names a line shows, one per button the line mentions. */
enum {
    MV_HS_ZOOM_OUT = 0,
    MV_HS_ZOOM_IN = 1,
    MV_HS_SCALER_PREV = 2,
    MV_HS_SCALER_NEXT = 3,
    MV_HS_CENTRE = 4,
    MV_HS_HUD = 5,   /* Circle: the bars on / off */
    MV_HS_BACK = 6,  /* Triangle: the game's cancel, closes the view */
    MV_HS_COUNT = 7
};

enum {
    MV_HELP_LINES = 2,
    MV_HELP_NAME_CAP = 12,
    MV_HELP_LINE_CAP = 200,
    MV_HELP_GLYPH0 = 0x80,  /* the byte of glyph 0 (A); glyph g is 0x80 + g */
    MV_HELP_CHAR_W = 8,     /* the 8x8 font's advance */
    MV_HELP_GLYPH_W = 12,   /* the glyph's cell */
    MV_HELP_GLYPH_ADV = 13, /* its advance: the cell and a 1 px gap */
    MV_HELP_MAX_W = 628     /* the bar is 640 wide with a 6 px margin each side */
};

/* The language index of the game's LANGUAGE value (patch 0147: 1 the US text, 2
 * English, 3 French, 4 Italian, 5 German, 6 Spanish); anything else is English. */
int dw3_mv_help_lang(int game_language);
/* "en" "fr" "de" "it" "es" */
const char *dw3_mv_help_lang_name(int lang);

/* How the names are produced. `switch_names`: the Switch's (cap text, glyphs
 * allowed), else the PC's (PS words of the position). `glyphs`: draw the glyph
 * where the button has one (only meaningful with switch_names). */
typedef struct MvHelpMode {
    int switch_names;
    int glyphs;
} MvHelpMode;

/* The name of PS button `ps` (DW3_BTN_*) or, with ps < 0, of Select, under `map`. */
void dw3_mv_help_name(const unsigned char map[DW3_BTN_COUNT], int ps,
                      const MvHelpMode *mode, char out[MV_HELP_NAME_CAP]);
/* The seven names of a line: the five bound actions (`b`), Circle and Triangle. */
void dw3_mv_help_names(const unsigned char map[DW3_BTN_COUNT], const MvBindings *b,
                       const MvHelpMode *mode,
                       char names[MV_HS_COUNT][MV_HELP_NAME_CAP]);

/* Line `line` (0 or 1) in `lang` with the names put in. Returns the length, or
 * -1 for a bad argument or a line that does not fit `cap`. */
int dw3_mv_help_line(int lang, int line,
                     const char names[MV_HS_COUNT][MV_HELP_NAME_CAP], char *out,
                     size_t cap);
/* The width in pixels the renderer needs for `text` (8 per character, 13 per
 * glyph byte). */
int dw3_mv_help_width(const char *text);
/* `text` for the log: a glyph byte as "[ZL]". Returns `out`. */
const char *dw3_mv_help_log_text(const char *text, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* DW3_MAP_VIEW_HELP_H */
