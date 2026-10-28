/* P6.30 technical help: the Help group's sub-screen of the Options screen
 * (docs/P6.30_HELP.md). A list of pages and a scrolling text page, on the
 * page engine (port_pages.h) and the glyph atlas (port_atlas.h). It only
 * reads text and a few facts about the running build; it writes nothing. */
#ifndef DW3_PORT_HELP_H
#define DW3_PORT_HELP_H

#include "port_pages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Where the help was opened from (the log line). */
enum { DW3_HELP_FROM_TITLE = 0, DW3_HELP_FROM_FIELD = 1, DW3_HELP_FROM_BATTLE = 2 };

/* Keys of one tick: edges, with the hold repeat on the four directions. */
enum {
    DW3_HELP_KEY_UP = 1,
    DW3_HELP_KEY_DOWN = 2,
    DW3_HELP_KEY_LEFT = 4,
    DW3_HELP_KEY_RIGHT = 8,
    DW3_HELP_KEY_PREV = 16,    /* L1 / L */
    DW3_HELP_KEY_NEXT = 32,    /* R1 / R */
    DW3_HELP_KEY_BACK = 64,    /* Triangle / X */
    DW3_HELP_KEY_CONFIRM = 128 /* Cross / Circle */
};

/* Sounds a tick asks the caller to play (the Options screen's three). */
enum { DW3_HELP_SND_NONE = 0, DW3_HELP_SND_CURSOR = 1, DW3_HELP_SND_SELECT = 2,
       DW3_HELP_SND_CANCEL = 3 };

enum { DW3_HELP_SCREEN_LIST = 0, DW3_HELP_SCREEN_PAGE = 1 };

/* Geometry shared with the art (platforms/pc/src/title_overlay_art.c), in
 * canvas pixels of the 960x720 title canvas. */
enum {
    DW3_HELP_PANEL_X = 100, DW3_HELP_PANEL_Y = 104,
    DW3_HELP_PANEL_W = 760, DW3_HELP_PANEL_H = 410,
    DW3_HELP_PAD_X = 24, DW3_HELP_PAD_Y = 10,
    DW3_HELP_LINE_PITCH = 30,
    DW3_HELP_PANEL_LINES = 13,
    DW3_HELP_BODY_W = DW3_HELP_PANEL_W - 2 * DW3_HELP_PAD_X,
    DW3_HELP_MAX_HINTS = 3
};

typedef struct Dw3HelpConfig {
    int from;           /* DW3_HELP_FROM_* */
    int region;         /* DW3_PAGES_US or DW3_PAGES_EU */
    int platform;       /* DW3_PAGES_SWITCH or DW3_PAGES_PC */
    int lang;           /* 0 EN, 1 FR, 2 DE, 3 ES, 4 IT (the Options lettering order) */
    int in_game;
    const char *root;       /* {path:root} */
    const char *memcard;    /* {path:memcard} */
    const char *version;    /* {build:version} */
} Dw3HelpConfig;

/* What the art draws. Pointers stay valid until the next tick / close. */
typedef struct Dw3HelpView {
    int screen;
    unsigned generation;      /* changes whenever the picture changes */
    unsigned open_serial;     /* changes with every open */
    double open_ms;           /* CLOCK_MONOTONIC ms of the open (the first-frame timing) */
    int in_game;
    int lang;
    char header[160];         /* page title (+ draft tag); "" on the list */
    char strip[200];          /* message strip: blurb, or "Page 3 / 10" */
    char hint[DW3_HELP_MAX_HINTS][72];
    int hint_cap[DW3_HELP_MAX_HINTS]; /* 0 none, 1 confirm cap, 2 back cap */
    int nhints;
    /* list */
    int npages, cursor, scroll;
    const char *titles[DW3_PAGES_MAX_PAGES];
    /* page */
    const Dw3PagesLine *lines;
    int nlines, top, panel;
    int arrow_up, arrow_down;
} Dw3HelpView;

/* Open the help (text loaded, page list built). Returns 0, or -1 when the
 * text cannot be loaded (logged); the caller then stays where it was. */
int dw3_help_open(const Dw3HelpConfig *cfg);
void dw3_help_close(void);
int dw3_help_is_open(void);
/* One frame: the keys pressed. Returns a DW3_HELP_SND_*. Back on the list
 * closes the help (dw3_help_is_open() then answers 0). */
int dw3_help_tick(unsigned keys);
const Dw3HelpView *dw3_help_view(void);
/* Pages the build lists, for the tests and the validator. */
int dw3_help_page_count(void);

/* Test hooks: replace the embedded text of a language (NULL restores it). */
void dw3_help_test_set_text(int lang, const char *text);

#ifdef __cplusplus
}
#endif

#endif
