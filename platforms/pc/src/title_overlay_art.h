#ifndef DW3_PC_TITLE_OVERLAY_ART_H
#define DW3_PC_TITLE_OVERLAY_ART_H
#include <stdint.h>
/* RGBA8888 byte order, 960x720 transparent canvas. selected is 0..3 for the
 * menu (New Game, Options, Load Game, and since P6.28 Quit game; with the
 * Load row, P6.31, Continue / Load Game / Quit game = 2 / 3 / 4), -1 for
 * PRESS START, DW3_TITLE_ART_OPTIONS for the Port options screen. */
enum { DW3_TITLE_MENU_ROWS = 5, DW3_TITLE_ART_OPTIONS = 5 };
/* P6.31: 1 while the title menu has the extra Load row (an auto-save file
 * exists): five plaques (New Game, Options, Continue, Load Game, Quit game),
 * state 4 = Quit game. 0 = the four of P6.28 (state 4 then has no focused
 * row). A change bumps dw3_pc_title_overlay_generation. */
void dw3_pc_title_overlay_set_load_row(int on);
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

/* P6.27 / P6.28 Yes / No confirmation window (port/options/confirm.c): the
 * quit question of the title menu and the return-to-title question of the
 * field menu. It is drawn into its own DW3_DIALOG_W x DW3_DIALOG_H RGBA
 * texture (not part of the title canvas, so it also shows over the field) and
 * placed at DW3_DIALOG_X, DW3_DIALOG_Y of the 960x720 canvas, which each host
 * fits to the 4:3 middle of its picture. focus: 0 = No (the default), 1 =
 * Yes. The words are in the language of the Options lettering
 * (dw3_pc_title_overlay_set_options_language). */
enum { DW3_DIALOG_QUIT = 0, DW3_DIALOG_RETURN = 1 };
enum { DW3_DIALOG_W = 768, DW3_DIALOG_H = 348, DW3_DIALOG_X = 96, DW3_DIALOG_Y = 186 };
void dw3_pc_dialog_art(uint8_t *rgba, int pitch, int kind, int focus);
/* Host hook (title_menu_overlay.c, nx_title_overlay.c): show (active) or hide
 * the window with that kind and focus. Cheap to call every frame. */
void dw3_pc_dialog_set(int active, int kind, int focus);
/* Bumped when the language of the lettering changes: a shown window must be
 * drawn again. */
unsigned dw3_pc_dialog_language_generation(void);

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
 * Command+Tech, Auto-save 0/1 = OFF / On area change). */
enum { DW3_TITLE_OPTION_ROWS = 9 };
void dw3_pc_title_overlay_set_options_view(int cursor, const int *values,
                                           int count);
/* P9.7: the Options screen opened from the field menu greys the rows that
 * cannot change in game: bit i = row i (label and value in grey, and the
 * message line says it is changed at the title), DW3_TITLE_LOCK_CHEATS = the
 * Square / Y "Cheats" hint. 0 on the title. A change rebuilds the retained
 * static layer (dw3_pc_title_overlay_options_layout). */
enum { DW3_TITLE_LOCK_CHEATS = 1u << 16 };
void dw3_pc_title_overlay_set_options_locked(unsigned mask);
unsigned dw3_pc_title_overlay_options_locked(void);
/* Bumped when the page, the cheats switch or the locked rows change: the
 * static layer (labels, title, hints) must be drawn again
 * (title_options_cache.c). */
unsigned dw3_pc_title_overlay_options_layout(void);
/* The static layer of an arbitrary layout, rows y..y+h-1 of a 960x720
 * canvas (cleared first), without changing the live layout. Lets the cache
 * draw the layout a menu will open with before it is open. */
void dw3_pc_title_overlay_options_static_band(uint8_t *rgba, int pitch,
        unsigned locked, int page, int cheats_on, int y, int h);
/* Live page / cheats switch (0 without DW3_CHEATS). */
int dw3_pc_title_overlay_options_cheats_on(void);
int dw3_pc_title_overlay_options_page(void);
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
#endif
#endif
