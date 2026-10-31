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
    /* P9.9: the label of one idle row, on the sub-screens whose labels change
     * with a value (Cheats: greyed while the master switch is off). */
    DW3_TITLE_OPTIONS_LABEL = 5,
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
/* The language the Options lettering is in (DW3_TITLE_LANG_*). */
int dw3_pc_title_overlay_get_options_language(void);
/* Authored lettering for the port-only Options row. */
void dw3_pc_title_overlay_set_options_language(int lang);
/* 0 drops the three-line credits drawn at the foot of the title canvas (the
 * game then draws its own sprites); on by default, US and EU alike. */
void dw3_pc_title_overlay_set_footer(int show);
/* P6.37: 0 drops the port version from the corner of the title canvas
 * (show_version=0 in port_options.cfg); on by default. The Options footer
 * does not follow it. */
void dw3_pc_title_overlay_set_version(int show);
/* 1 while the credits are part of the canvas (the setting above). */
int dw3_pc_title_overlay_footer_shown(void);
/* The Port options screen: focused row and the rows' values in registry order
 * (Widescreen 0/1, Fast-forward 0 or its 2..4 speed, Restart hotkey 0/1, the
 * two volumes 0..10, Encounter rate 0..4 = 1x / 0.5x / OFF / 1.5x / 2x, Text
 * speed 0..2 = Normal / Fast / Instant, Battle cursor 0..2 = OFF / Command /
 * Command+Tech, Auto-save 0/1 = OFF / On area change). */
enum { DW3_TITLE_OPTION_ROWS = 10 };
void dw3_pc_title_overlay_set_options_view(int cursor, const int *values,
                                           int count);
/* P9.7: the Options screen opened from the field menu greys the rows that
 * cannot change in game: bit i = row i (label and value in grey, and the
 * message line says it is changed at the title), DW3_TITLE_LOCK_CHEATS = the
 * Square / Y "Cheats" hint. 0 on the title. A change rebuilds the retained
 * static layer (dw3_pc_title_overlay_options_layout). */
enum { DW3_TITLE_LOCK_CHEATS = 1u << 16 };
/* P10.6 fix-now: the source positions (0..9 of port_button_map.h: 0 B, 1 A,
 * 2 X, 3 Y, 4 L, 5 R, 6 ZL, 7 ZR, 8 L3, 9 R3) of the buttons that produce the
 * PS Cross (confirm) and Triangle (back) under the button map. The Switch hint
 * caps show their letters; a change rebuilds the retained layer. The PC keeps
 * its drawn PS marks. */
void dw3_pc_title_overlay_set_hint_caps(int confirm_src, int back_src);
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
/* The same layer in two parts (page SUB_BASE: the part shared by every group,
 * page = a group page: part 1 shared, part 2 the group's own, over a canvas that
 * holds the shared part; clear = 0 keeps the canvas). A group's layer is then a
 * copy of the shared layer plus a header, a few plaques and labels, not a raster
 * of the whole screen. */
void dw3_pc_title_overlay_options_static_part(uint8_t *rgba, int pitch,
        unsigned locked, int page, int cheats_on, int part, int y, int h, int clear);
/* 1 when `page` is a group's sub-screen (not the group list). */
int dw3_pc_title_overlay_options_is_group_page(int page);
/* Live page / cheats switch. */
int dw3_pc_title_overlay_options_cheats_on(void);
int dw3_pc_title_overlay_options_page(void);
/* Bumped by setters that change the menu states; hosts re-upload on change. */
unsigned dw3_pc_title_overlay_generation(void);
/* Bumped only when the Port options screen (state 4) changes. */
unsigned dw3_pc_title_overlay_options_generation(void);
int dw3_pc_title_overlay_options_cursor(void);
/* Value last handed to set_options_view for a row (0 for a bad row). */
int dw3_pc_title_overlay_options_value(int row);
/* Which page the Options screen shows (port_options_groups.h dw3_optlist_page):
 * PAGE_OPTIONS = the list of groups, a larger code = a group's sub-screen (its
 * rows, scrolled). The Cheats group (P6.20) is such a sub-screen since P9.9:
 * ten rows, six on screen, its view values are the cheat rows (port_cheats.h:
 * master, Fill bag, Instant win, Battle now, marked saves 0..6, Money (0..3),
 * Warp (0 OFF, 1..13), the item id, the item count and Give item (0/1)).
 * cheats_on = the cheats master switch: the Cheats group is listed while it is
 * on, and on its own sub-screen it greys rows 1..9 while it is off. On a list
 * page the view's cursor is the item's index in the whole list (not its place
 * on screen) and the values are those of the group's rows in screen order (a
 * group list ignores them). A change rebuilds the retained static layer, except
 * the master switch on the Cheats sub-screen, which is a dirty-row update. */
enum { DW3_TITLE_OPTIONS_PAGE_OPTIONS = 0,
       /* Never shown: the cache's layer shared by every group sub-screen. */
       DW3_TITLE_OPTIONS_PAGE_SUB_BASE = 1 };
enum { DW3_TITLE_CHEAT_WARPS = 13 };
void dw3_pc_title_overlay_set_options_page(int page, int cheats_on);
/* P51 item 6: reads the game's menu background tile once (frame thread), before
 * any Options layer is composed; logs `dw3-options: menu background tile ...`. */
void dw3_pc_title_overlay_menu_tile_prepare(void);
/* Layout of a page for the retained cache. kind: the rectangle of a plaque
 * (with its halo), of its value cell, or of the message strip; `slot` is the
 * plaque's place on screen (the strip ignores it). */
enum { DW3_OPTRECT_ROW = 0, DW3_OPTRECT_VALUE = 1, DW3_OPTRECT_DESC = 2,
       DW3_OPTRECT_ARROW_UP = 3, DW3_OPTRECT_ARROW_DOWN = 4 };
void dw3_pc_title_overlay_options_rect(int page, int kind, int slot, int *x,
        int *y, int *w, int *h);
/* scroll = first visible item, items = items in the list, slots = plaques that
 * fit on screen. */
void dw3_pc_title_overlay_options_page_info(int page, int cheats_on,
        int *scroll, int *items, int *slots);
/* The bits of `locked` that change the static layer of `page`. */
unsigned dw3_pc_title_overlay_options_lock_key(int page, unsigned locked);
/* 1 when the labels of `page` are not in its static layer (drawn per row by
 * DW3_TITLE_OPTIONS_LABEL), and the cheats switch is not part of its layer key. */
int dw3_pc_title_overlay_options_dynamic_labels(int page);
/* 1 when the message line of the focused row depends on its value (P6.12: the
 * Customize screen says "press a button" while a row waits for one). */
int dw3_pc_title_overlay_options_desc_follows_value(int page);
/* The page whose static layer `page` uses (the Cheats sub-screen: its offset 0). */
int dw3_pc_title_overlay_options_static_page(int page);
/* P6.30 technical help (port/help/help.c, docs/P6.30_HELP.md): the Help
 * sub-screen is its own full 960x720 RGBA layer, composed on the CPU from the
 * view the help keeps (the page list or a page of text) and drawn by the host
 * instead of the Options layer while it is active. set_view(NULL) hides it and
 * frees the canvas; a view is composed again when its generation changes.
 * render returns the canvas (pitch 960 * 4) and a version that changes when
 * the host must upload it again, or NULL while the help is not open. */
struct Dw3HelpView;
void dw3_pc_help_set_view(const struct Dw3HelpView *view);
int dw3_pc_help_active(void);
const uint8_t *dw3_pc_help_render(unsigned *version);
#endif
