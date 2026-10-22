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
/* 0 drops the built-in US credit lines (the game draws its own). */
void dw3_pc_title_overlay_set_footer(int show);
/* The Port options screen: focused row and the rows' values in registry order
 * (Widescreen 0/1, Fast-forward 0 or its 2..4 speed, Restart hotkey 0/1, the
 * two volumes 0..10, Encounter rate 0..2 = 1x / 0.5x / OFF, Text speed 0..2 =
 * Normal / Fast / Instant). */
enum { DW3_TITLE_OPTION_ROWS = 7 };
void dw3_pc_title_overlay_set_options_view(int cursor, const int *values,
                                           int count);
/* Bumped by setters that change the menu states; hosts re-upload on change. */
unsigned dw3_pc_title_overlay_generation(void);
/* Bumped only when the Port options screen (state 4) changes. */
unsigned dw3_pc_title_overlay_options_generation(void);
int dw3_pc_title_overlay_options_cursor(void);
/* Value last handed to set_options_view for a row (0 for a bad row). */
int dw3_pc_title_overlay_options_value(int row);
#endif
