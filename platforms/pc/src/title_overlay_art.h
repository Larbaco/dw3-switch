#ifndef DW3_PC_TITLE_OVERLAY_ART_H
#define DW3_PC_TITLE_OVERLAY_ART_H
#include <stdint.h>
/* RGBA8888 byte order, 960x720 transparent canvas. selected is 0..2 for the
 * menu, -1 for PRESS START, DW3_TITLE_ART_OPTIONS for the Port options screen. */
enum { DW3_TITLE_ART_OPTIONS = 3 };
void dw3_pc_title_overlay_art(uint8_t *rgba, int pitch, int selected);
enum {
    DW3_TITLE_OPTIONS_STATIC = 1,
    DW3_TITLE_OPTIONS_FOCUS = 2,
    DW3_TITLE_OPTIONS_VALUE = 3,
    DW3_TITLE_OPTIONS_DESCRIPTION = 4,
};
/* Draw a single Options component into its clipped bounds on the retained
 * 960x720 canvas. The selected row/value are used only by their layer. */
void dw3_pc_title_overlay_options_component(uint8_t *rgba, int pitch,
        int layer, int row, int value, int x, int y, int w, int h,
        int clear_region);

/* Plaque labels. The defaults are the built-in English lettering; the EU disc
 * replaces them with its own words, captured from the title bank at runtime. */
enum {
    DW3_TITLE_LABEL_START = 0,
    DW3_TITLE_LABEL_NEW_GAME = 1,
    DW3_TITLE_LABEL_OPTIONS = 2,
    DW3_TITLE_LABEL_LOAD_GAME = 3,
    DW3_TITLE_LABEL_COUNT = 4,
};
enum {
    DW3_TITLE_LANG_EN = 0,
    DW3_TITLE_LANG_FR = 1,
    DW3_TITLE_LANG_DE = 2,
    DW3_TITLE_LANG_ES = 3,
    DW3_TITLE_LANG_IT = 4,
};
/* 8-bit coverage mask (copied; at most 440x48). NULL restores the default.
 * Returns 0, or -1 when the mask is rejected. */
int dw3_pc_title_overlay_set_label(int slot, const uint8_t *coverage, int w,
                                   int h);
/* Authored lettering for the port-only Options row. */
void dw3_pc_title_overlay_set_options_language(int lang);
/* 0 drops the three-line credits drawn at the foot of the title canvas (the
 * game then draws its own sprites); on by default, US and EU alike. */
void dw3_pc_title_overlay_set_footer(int show);
/* 1 while the credits are part of the canvas (the setting above). */
int dw3_pc_title_overlay_footer_shown(void);
/* The Port options screen: focused row and the rows' values in registry order
 * (Widescreen 0/1, Fast-forward 0 or its 2..4 speed, Restart hotkey 0/1, the
 * two volumes 0..10, Encounter rate 0..4 = 1x / 0.5x / OFF / 1.5x / 2x, Text
 * speed 0..2 = Normal / Fast / Instant, Battle cursor 0..2 = OFF / Command /
 * Command+Tech). */
enum { DW3_TITLE_OPTION_ROWS = 8 };
void dw3_pc_title_overlay_set_options_view(int cursor, const int *values,
                                           int count);
/* Bumped by setters that change the menu states; hosts re-upload on change. */
unsigned dw3_pc_title_overlay_generation(void);
/* Bumped only when the Port options screen (state 4) changes. */
unsigned dw3_pc_title_overlay_options_generation(void);
int dw3_pc_title_overlay_options_cursor(void);
/* Value last handed to set_options_view for a row (0 for a bad row). */
int dw3_pc_title_overlay_options_value(int row);
#ifdef DW3_CHEATS
/* P6.20: the Options window shows the Options page or the Cheats page
 * (port/options/cheats.c). On the Cheats page the view's values are the
 * cheat rows (port_cheats.h: master, Fill bag, Instant win, Battle now 0/1,
 * marked saves 0..6) and cheats_on dims rows 1..4 while the master switch is
 * off; on the Options page cheats_on adds the Square/Y "Cheats" hint.
 * PAGE_CHEATS2 is the second page (PR 2): the view's values are Money (0..3),
 * Warp (0 OFF, 1..13), the item id, the item count and Give item (0/1). */
enum { DW3_TITLE_OPTIONS_PAGE_OPTIONS = 0, DW3_TITLE_OPTIONS_PAGE_CHEATS = 1,
       DW3_TITLE_OPTIONS_PAGE_CHEATS2 = 2 };
enum { DW3_TITLE_CHEAT_ROWS = 5, DW3_TITLE_CHEAT_PAGES = 2, DW3_TITLE_CHEAT_WARPS = 13 };
void dw3_pc_title_overlay_set_options_page(int page, int cheats_on);
/* Bumped when the page or cheats_on changes: the static layer (labels,
 * title, hints) must be drawn again (title_options_cache.c). */
unsigned dw3_pc_title_overlay_options_layout(void);
#endif
#endif
