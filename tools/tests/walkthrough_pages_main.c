/* P6.35 condensed pages: the line count of every page in every language, on the real embedded text
 * and the real atlas metrics, with the player at the end of a region's table (everything reached,
 * so every step of a condensed page is shown). Prints one line per page and language:
 *   PAGE <region> <lang> <page id> steps <n> closed <lines> answer <lines>
 * `closed` is the page the player sees first (goals and the "answer is hidden" note), `answer` the
 * page after the reveal (goals, the answer header and the helps). test_walkthrough.py holds
 * `closed` to one panel (DW3_HELP_PANEL_LINES) in every language. Plain C, no game data. */
#include "port_atlas.h"
#include "port_button_map.h"
#include "port_walkthrough_ui.h"

#include <stdio.h>
#include <string.h>

static int g_progress;

static int fake_check(void *ctx, unsigned code, unsigned value) {
    (void)ctx;
    (void)code;
    return value == 1; /* every flag and item is set: the player is past all of it */
}

static int fake_snapshot(DW3WtState *st, int *mode) {
    st->progress = g_progress;
    st->half = 0;
    st->area = 0x13;
    st->region = WT_REGION_US;
    st->check = fake_check;
    st->ctx = NULL;
    if (mode != NULL) {
        *mode = 0x200;
    }
    return 1;
}

static void keys(unsigned k) {
    (void)dw3_wt_tick(k);
}

int main(void) {
    static const char *const lang_names[] = {"en", "fr", "de", "es", "it"};
    int region;
    int lang;

    for (region = 0; region < 2; region++) {
        for (lang = 0; lang < 5; lang++) {
            Dw3WtConfig c;
            int nch;
            int ch;

            memset(&c, 0, sizeof(c));
            c.from = DW3_HELP_FROM_FIELD;
            c.region = region ? DW3_PAGES_EU : DW3_PAGES_US;
            c.platform = DW3_PAGES_PC;
            c.lang = lang;
            c.in_game = 1;
            c.snapshot = fake_snapshot;
            g_progress = region ? 0x2D : 0x2B; /* past the end of the US table; the EU table's last value */
            if (dw3_wt_open(&c) != 0) {
                fprintf(stderr, "open failed\n");
                return 1;
            }
            while (dw3_wt_level() == DW3_WT_LEVEL_PAGE || dw3_wt_level() == DW3_WT_LEVEL_STEPS) {
                keys(DW3_HELP_KEY_BACK);
            }
            nch = dw3_wt_view()->npages;
            for (ch = 0; ch < nch; ch++) {
                keys(DW3_HELP_KEY_UP);
            }
            for (ch = 0; ch < nch; ch++) {
                int i;
                int n;

                keys(DW3_HELP_KEY_CONFIRM);
                n = dw3_wt_view()->npages;
                for (i = 0; i < n; i++) {
                    keys(DW3_HELP_KEY_UP);
                }
                for (i = 0; i < n; i++) {
                    int closed;
                    int answer;

                    keys(DW3_HELP_KEY_CONFIRM);
                    closed = dw3_wt_view()->nlines;
                    keys(DW3_HELP_KEY_CONFIRM);
                    keys(DW3_HELP_KEY_DOWN);
                    keys(DW3_HELP_KEY_CONFIRM);
                    answer = dw3_wt_view()->nlines;
                    keys(DW3_HELP_KEY_BACK);
                    printf("PAGE %s %s ch%d.%d closed %d answer %d\n", region ? "EU" : "US",
                           lang_names[lang], ch + 1, i + 1, closed, answer);
                    keys(DW3_HELP_KEY_DOWN);
                }
                keys(DW3_HELP_KEY_BACK);
                keys(DW3_HELP_KEY_DOWN);
            }
            dw3_wt_close();
        }
    }
    return 0;
}
