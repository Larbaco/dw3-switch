/* P6.35 phase 2 host test: the walkthrough screen on the real embedded text
 * and the real atlas metrics, with a fake game state. Log lines go to stderr;
 * test_walkthrough.py checks them. Plain C, no game data. */
#include "port_atlas.h"
#include "port_button_map.h"
#include "port_walkthrough_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
#define CHECK(c)                                                               \
    do {                                                                       \
        if (!(c)) {                                                            \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);       \
            g_fail++;                                                          \
        }                                                                      \
    } while (0)

static int g_progress, g_flag, g_snap_calls, g_unknown;

static int fake_check(void *ctx, unsigned code, unsigned value) {
    (void)ctx;
    if (code == WT_FLAG(0x40, 0x0D)) {
        return value == (unsigned)g_flag;
    }
    return value == 0;
}

static int fake_snapshot(DW3WtState *st, int *mode) {
    g_snap_calls++;
    st->progress = g_unknown ? 0x8000 : g_progress;
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

static Dw3WtConfig cfg(int region, int lang, int in_game) {
    Dw3WtConfig c;

    memset(&c, 0, sizeof(c));
    c.from = in_game ? DW3_HELP_FROM_FIELD : DW3_HELP_FROM_TITLE;
    c.region = region;
    c.platform = DW3_PAGES_PC;
    c.lang = lang;
    c.in_game = in_game;
    c.snapshot = fake_snapshot;
    return c;
}

static int line_has(const Dw3PagesLine *l, const char *needle) {
    int r;

    for (r = 0; r < 2; r++) {
        if (l->s[r] != NULL && l->len[r] > 0) {
            char buf[512];
            int n = l->len[r] < 511 ? l->len[r] : 511;

            memcpy(buf, l->s[r], (size_t)n);
            buf[n] = '\0';
            if (strstr(buf, needle) != NULL) {
                return 1;
            }
        }
    }
    return 0;
}

/* The page on screen has the text (every line, not only the visible ones). */
static int page_has(const char *needle) {
    const Dw3HelpView *v = dw3_wt_view();
    int i;

    if (v->screen != DW3_HELP_SCREEN_PAGE) {
        return 0;
    }
    for (i = 0; i < v->nlines; i++) {
        if (line_has(&v->lines[i], needle)) {
            return 1;
        }
    }
    return 0;
}

static int list_has(const char *needle) {
    const Dw3HelpView *v = dw3_wt_view();
    int i;

    for (i = 0; i < v->npages; i++) {
        if (strstr(v->titles[i], needle) != NULL) {
            return 1;
        }
    }
    return 0;
}

static int fits(int face, const char *s, int limit) {
    int w = dw3_atlas_measure(face, s, (int)strlen(s));

    if (w > limit) {
        fprintf(stderr, "WIDTH %d > %d: %s\n", w, limit, s);
    }
    return w <= limit;
}

static void keys(unsigned k) {
    (void)dw3_wt_tick(k);
}

static void state(int progress, int flag) {
    g_progress = progress;
    g_flag = flag;
    g_unknown = 0;
    g_snap_calls = 0;
}

static void test_current_step_and_reveal(void) {
    Dw3WtConfig c = cfg(DW3_PAGES_US, 0, 1);
    const Dw3HelpView *v;

    state(2, 0);
    CHECK(dw3_wt_open(&c) == 0 && dw3_wt_is_open());
    CHECK(g_snap_calls == 1); /* one read of the game */
    v = dw3_wt_view();
    CHECK(v->screen == DW3_HELP_SCREEN_PAGE);
    CHECK(strcmp(v->header, "Get your first partner") == 0);
    CHECK(strcmp(v->log_tag, "dw3-walkthrough") == 0);
    CHECK(strstr(v->strip, "Step 3 / 6") != NULL && strstr(v->strip, "Current step") != NULL);
    CHECK(page_has("Main Lobby"));                    /* the goal */
    CHECK(!page_has("three starter packs"));          /* the answer is closed */
    CHECK(page_has("The answer is hidden"));
    /* reveal: two actions, the cursor starts on No */
    keys(DW3_HELP_KEY_CONFIRM);
    CHECK(strcmp(dw3_wt_view()->header, "Show the answer?") == 0);
    CHECK(page_has("\xE2\x86\x92 No") && !page_has("\xE2\x86\x92 Yes"));
    CHECK(!page_has("three starter packs"));
    keys(DW3_HELP_KEY_CONFIRM);                       /* Confirm on No: cancelled */
    CHECK(!page_has("three starter packs") && page_has("The answer is hidden"));
    keys(DW3_HELP_KEY_CONFIRM);                       /* ask again */
    keys(DW3_HELP_KEY_BACK);                          /* Back cancels, not the screen */
    CHECK(dw3_wt_is_open() && dw3_wt_view()->screen == DW3_HELP_SCREEN_PAGE);
    CHECK(!page_has("three starter packs"));
    keys(DW3_HELP_KEY_CONFIRM);
    keys(DW3_HELP_KEY_DOWN);                          /* the cursor moves to Yes */
    CHECK(page_has("\xE2\x86\x92 Yes"));
    keys(DW3_HELP_KEY_CONFIRM);                       /* shown */
    CHECK(page_has("three starter packs") && page_has("The answer"));
    CHECK(strcmp(dw3_wt_view()->header, "Get your first partner") == 0);
    /* Back goes up one level (the list, the cursor on the step), and the answer is closed again */
    keys(DW3_HELP_KEY_BACK);
    v = dw3_wt_view();
    CHECK(v->screen == DW3_HELP_SCREEN_LIST && v->cursor == 2);
    keys(DW3_HELP_KEY_CONFIRM);
    CHECK(!page_has("three starter packs"));
    /* L1 / R1 browse the steps */
    keys(DW3_HELP_KEY_NEXT);
    CHECK(strcmp(dw3_wt_view()->header, "Meet Tamer Genji") == 0 ||
          strcmp(dw3_wt_view()->header, "Not reached yet") == 0);
    keys(DW3_HELP_KEY_PREV);
    keys(DW3_HELP_KEY_PREV);
    CHECK(strcmp(dw3_wt_view()->header, "Log in to Asuka City") == 0);
    keys(DW3_HELP_KEY_BACK);
    keys(DW3_HELP_KEY_BACK);                          /* the list closes the screen */
    CHECK(!dw3_wt_is_open());
}

static void test_never_ahead(void) {
    Dw3WtConfig c = cfg(DW3_PAGES_US, 0, 1);
    const Dw3HelpView *v;
    int i;

    state(2, 0);
    CHECK(dw3_wt_open(&c) == 0);
    keys(DW3_HELP_KEY_BACK); /* to the list */
    v = dw3_wt_view();
    CHECK(v->screen == DW3_HELP_SCREEN_LIST);
    CHECK(list_has("Get your first partner"));
    CHECK(!list_has("Seiryu") && !list_has("Genji") && !list_has("New faces"));
    CHECK(v->npages == 9); /* 6 steps, 2 areas, more; the optional row is not met yet */
    for (i = 0; i < v->npages; i++) {
        CHECK(strstr(v->titles[i], "Seiryu") == NULL);
    }
    /* a step ahead: the title stays hidden, the page needs the reveal */
    keys(DW3_HELP_KEY_DOWN); /* the cursor was on s03; s04 is the next one */
    keys(DW3_HELP_KEY_CONFIRM);
    v = dw3_wt_view();
    CHECK(v->screen == DW3_HELP_SCREEN_PAGE && strcmp(v->header, "Not reached yet") == 0);
    CHECK(!page_has("Genji") && page_has("spoil"));
    keys(DW3_HELP_KEY_CONFIRM);
    CHECK(page_has("further on") && !page_has("Genji"));
    keys(DW3_HELP_KEY_DOWN);
    keys(DW3_HELP_KEY_CONFIRM);
    CHECK(page_has("Genji") && strcmp(dw3_wt_view()->header, "Meet Tamer Genji") == 0);
    keys(DW3_HELP_KEY_BACK);
    keys(DW3_HELP_KEY_BACK);
    CHECK(!dw3_wt_is_open());
}

static void test_other_points(void) {
    Dw3WtConfig c = cfg(DW3_PAGES_US, 0, 1);
    const Dw3HelpView *v;

    /* progress 3 with Genji's flag set: s05 */
    state(3, 1);
    CHECK(dw3_wt_open(&c) == 0);
    CHECK(strcmp(dw3_wt_view()->header, "Finish up with Genji") == 0);
    dw3_wt_close();
    /* progress 4: s06 and the optional row is on the list */
    state(4, 0);
    CHECK(dw3_wt_open(&c) == 0);
    CHECK(strcmp(dw3_wt_view()->header, "Travel to Seiryu City") == 0);
    keys(DW3_HELP_KEY_BACK);
    CHECK(list_has("New faces around Asuka") && dw3_wt_view()->npages == 10);
    dw3_wt_close();
    /* progress 7: past the table, the "more chapters" page */
    state(7, 0);
    CHECK(dw3_wt_open(&c) == 0);
    v = dw3_wt_view();
    CHECK(v->screen == DW3_HELP_SCREEN_PAGE && strcmp(v->header, "More chapters coming") == 0);
    CHECK(page_has("first chapter"));
    keys(DW3_HELP_KEY_CONFIRM); /* nothing to reveal here */
    CHECK(strcmp(dw3_wt_view()->header, "More chapters coming") == 0);
    keys(DW3_HELP_KEY_BACK);
    CHECK(dw3_wt_view()->screen == DW3_HELP_SCREEN_LIST && list_has("Seiryu"));
    dw3_wt_close();
    /* the debug value: not recognised, a list with the notice, the first step only */
    state(0, 0);
    g_unknown = 1;
    CHECK(dw3_wt_open(&c) == 0);
    v = dw3_wt_view();
    CHECK(v->screen == DW3_HELP_SCREEN_LIST && strstr(v->strip, "not recognised") != NULL);
    CHECK(list_has("Visit the Online Center") && !list_has("Log in to Asuka City"));
    keys(DW3_HELP_KEY_DOWN);
    CHECK(strstr(dw3_wt_view()->strip, "not recognised") == NULL);
    dw3_wt_close();
    g_unknown = 0;
    /* the title: no game state is read at all */
    c = cfg(DW3_PAGES_US, 0, 0);
    state(7, 0);
    CHECK(dw3_wt_open(&c) == 0);
    CHECK(g_snap_calls == 0);
    v = dw3_wt_view();
    CHECK(v->screen == DW3_HELP_SCREEN_LIST && strstr(v->strip, "No game loaded") != NULL);
    CHECK(list_has("Visit the Online Center") && !list_has("Seiryu"));
    CHECK(v->in_game == 0);
    dw3_wt_close();
}

static void test_region_and_language(void) {
    Dw3WtConfig c = cfg(DW3_PAGES_EU, 1, 1);

    /* EU, French: the translated text, no English fallback */
    state(2, 0);
    CHECK(dw3_wt_open(&c) == 0);
    CHECK(strcmp(dw3_wt_view()->header, "Votre premier partenaire") == 0);
    CHECK(strstr(dw3_wt_view()->strip, "\xC3\x89tape 3 / 6") != NULL);
    dw3_wt_close();
    /* a language whose file is missing keys falls back to English, once */
    {
        static const char part[] =
            "@format = 1\n@book = walkthrough\n@lang = de\n@status = draft\n"
            "wt.ch01_s03.title = Nur der Titel\n";

        dw3_wt_test_set_text(2, part);
        c = cfg(DW3_PAGES_EU, 2, 1);
        CHECK(dw3_wt_open(&c) == 0);
        CHECK(strcmp(dw3_wt_view()->header, "Nur der Titel") == 0);
        CHECK(page_has("Main Lobby")); /* the goal came from English */
        dw3_wt_close();
        dw3_wt_test_set_text(2, NULL);
    }
}

/* Everything that must fit does fit, in every language and state. */
static void test_fit(void) {
    int lang;
    int p;

    for (lang = 0; lang < 5; lang++) {
        for (p = 0; p < 9; p++) {
            Dw3WtConfig c = cfg(DW3_PAGES_EU, lang, p != 8);
            const Dw3HelpView *v;
            int i;
            int hint_w;

            state(p == 8 ? 0 : p, p == 5);
            CHECK(dw3_wt_open(&c) == 0);
            for (i = 0; i < dw3_wt_item_count(); i++) {
                int s;

                /* the list, then each item's page in its three modes */
                dw3_wt_close();
                CHECK(dw3_wt_open(&c) == 0);
                if (dw3_wt_view()->screen == DW3_HELP_SCREEN_PAGE) {
                    keys(DW3_HELP_KEY_BACK);
                }
                v = dw3_wt_view();
                CHECK(fits(DW3_ATLAS_TITLE, v->header, 410));
                CHECK(fits(DW3_ATLAS_BODY, v->strip, 664));
                {
                    int k;

                    for (k = 0; k < v->npages; k++) {
                        CHECK(fits(DW3_ATLAS_TITLE, v->titles[k], 410));
                    }
                }
                for (s = 0; s < i; s++) {
                    keys(DW3_HELP_KEY_DOWN);
                }
                keys(DW3_HELP_KEY_CONFIRM);
                for (s = 0; s < 3; s++) {
                    v = dw3_wt_view();
                    CHECK(fits(DW3_ATLAS_TITLE, v->header, 410));
                    CHECK(fits(DW3_ATLAS_BODY, v->strip, 664));
                    hint_w = 0;
                    {
                        int h;

                        for (h = 0; h < v->nhints; h++) {
                            hint_w += dw3_atlas_measure(DW3_ATLAS_BODY, v->hint[h],
                                                        (int)strlen(v->hint[h])) +
                                      (v->hint_cap[h] ? 36 : 0) + 44;
                        }
                    }
                    CHECK(hint_w <= 740);
                    if (s == 0) {
                        keys(DW3_HELP_KEY_CONFIRM); /* ask */
                    } else if (s == 1) {
                        keys(DW3_HELP_KEY_DOWN);
                        keys(DW3_HELP_KEY_CONFIRM); /* shown */
                    }
                }
            }
            dw3_wt_close();
        }
    }
}

int main(void) {
    test_current_step_and_reveal();
    test_never_ahead();
    test_other_points();
    test_region_and_language();
    test_fit();
    if (g_fail != 0) {
        fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }
    printf("OK: walkthrough screen\n");
    return 0;
}
