/* P6.35 phase 2: the walkthrough screen, the Walkthrough group's sub-screen of
 * the Options screen (docs/P6.35_WALKTHROUGH.md). A step list and a scrolling
 * step page, on the page engine (port_pages.h) and the glyph atlas. It shows
 * through the Help screen's view (Dw3HelpView, port_help.h): the same art draws
 * both, so the PC and Switch hosts need nothing more.
 *
 * Read-only on the game: the state is read once, by the snapshot callback,
 * when the screen opens. Nothing is written to the save or the config; a
 * revealed answer is forgotten when the player leaves the step. */
#ifndef DW3_PORT_WALKTHROUGH_UI_H
#define DW3_PORT_WALKTHROUGH_UI_H

#include "port_help.h"
#include "port_walkthrough.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Reads the game: fills the state and (optionally) the field mode. 1 on
 * success. The game side is dw3_walkthrough_snapshot(). */
typedef int (*Dw3WtSnapshotFn)(DW3WtState *st, int *mode);

typedef struct Dw3WtConfig {
    int from;           /* DW3_HELP_FROM_TITLE / FIELD / BATTLE (the log line) */
    int region;         /* DW3_PAGES_US or DW3_PAGES_EU */
    int platform;       /* DW3_PAGES_SWITCH or DW3_PAGES_PC */
    int lang;           /* 0 EN, 1 FR, 2 DE, 3 ES, 4 IT (the Options lettering order) */
    int in_game;        /* 0 at the title: no game state to read, browse mode */
    Dw3WtSnapshotFn snapshot;
} Dw3WtConfig;

/* Open the screen (text loaded, items listed, the first page chosen). Returns
 * 0, or -1 when the text cannot be loaded (logged). */
int dw3_wt_open(const Dw3WtConfig *cfg);
void dw3_wt_close(void);
int dw3_wt_is_open(void);
/* One frame: the keys pressed (DW3_HELP_KEY_*). Returns a DW3_HELP_SND_*. Back
 * on the step list closes the screen (dw3_wt_is_open() then answers 0). */
int dw3_wt_tick(unsigned keys);
const Dw3HelpView *dw3_wt_view(void);

/* For the tests. */
int dw3_wt_item_count(void);
/* Test hook: replace the embedded text of a language (NULL restores it). */
void dw3_wt_test_set_text(int lang, const char *text);

#ifdef __cplusplus
}
#endif

#endif
