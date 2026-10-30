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

static int g_progress, g_flag, g_snap_calls, g_unknown, g_asked_eu;
static unsigned g_codes[8];   /* more flags / items that are set */
static int g_ncodes;

static int fake_check(void *ctx, unsigned code, unsigned value) {
    int i;

    (void)ctx;
    if (code == WT_FLAG(0x40, 0xCD) || code == WT_SPECIAL(0x54) || code == WT_FLAG(0x10, 0x0C)) {
        g_asked_eu++;                                 /* the EU extra chapter's codes */
    }
    if (code == WT_FLAG(0x40, 0x0D)) {
        return value == (unsigned)g_flag;
    }
    for (i = 0; i < g_ncodes; i++) {
        if (g_codes[i] == code) {
            return value == 1;
        }
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
    g_ncodes = 0;
}

/* The same with up to two more flags / items set. */
static void state_codes(int progress, unsigned c0, unsigned c1) {
    state(progress, 0);
    if (c0) {
        g_codes[g_ncodes++] = c0;
    }
    if (c1) {
        g_codes[g_ncodes++] = c1;
    }
}

static int starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static void test_current_step_and_reveal(void) {
    Dw3WtConfig c = cfg(DW3_PAGES_US, 0, 1);
    const Dw3HelpView *v;

    state(2, 0);
    CHECK(dw3_wt_open(&c) == 0 && dw3_wt_is_open());
    CHECK(g_snap_calls == 1); /* one read of the game */
    v = dw3_wt_view();
    CHECK(v->screen == DW3_HELP_SCREEN_PAGE && dw3_wt_level() == DW3_WT_LEVEL_PAGE);
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
    /* Back goes up one level (the chapter's steps, the cursor on the step), and the answer is
     * closed again */
    keys(DW3_HELP_KEY_BACK);
    v = dw3_wt_view();
    CHECK(v->screen == DW3_HELP_SCREEN_LIST && dw3_wt_level() == DW3_WT_LEVEL_STEPS &&
          v->cursor == 2);
    CHECK(strcmp(v->header, "1. Getting started") == 0);
    keys(DW3_HELP_KEY_CONFIRM);
    CHECK(!page_has("three starter packs"));
    /* L1 / R1 browse the steps */
    keys(DW3_HELP_KEY_NEXT);
    CHECK(strcmp(dw3_wt_view()->header, "Meet Tamer Genji") == 0 ||
          strcmp(dw3_wt_view()->header, "Not reached yet") == 0);
    keys(DW3_HELP_KEY_PREV);
    keys(DW3_HELP_KEY_PREV);
    CHECK(strcmp(dw3_wt_view()->header, "Log in to Asuka City") == 0);
    keys(DW3_HELP_KEY_BACK);                          /* the steps */
    keys(DW3_HELP_KEY_BACK);                          /* the chapters */
    CHECK(dw3_wt_level() == DW3_WT_LEVEL_CHAPTERS && dw3_wt_is_open());
    keys(DW3_HELP_KEY_BACK);                          /* the chapter list closes the screen */
    CHECK(!dw3_wt_is_open() && dw3_wt_level() == DW3_WT_LEVEL_CLOSED);
}

static void test_never_ahead(void) {
    Dw3WtConfig c = cfg(DW3_PAGES_US, 0, 1);
    const Dw3HelpView *v;
    int i;

    state(2, 0);
    CHECK(dw3_wt_open(&c) == 0);
    keys(DW3_HELP_KEY_BACK); /* to the steps of chapter 1 */
    v = dw3_wt_view();
    CHECK(v->screen == DW3_HELP_SCREEN_LIST);
    CHECK(list_has("Get your first partner"));
    CHECK(!list_has("Seiryu") && !list_has("Genji") && !list_has("New faces"));
    CHECK(v->npages == 8); /* 6 steps and 2 areas; the optional row is not met yet */
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
    keys(DW3_HELP_KEY_BACK);
    CHECK(!dw3_wt_is_open());
    /* a chapter ahead: its title and its steps stay hidden, the page needs the same reveal */
    state(2, 0);
    CHECK(dw3_wt_open(&c) == 0);
    keys(DW3_HELP_KEY_BACK);
    keys(DW3_HELP_KEY_BACK);                          /* the chapters */
    v = dw3_wt_view();
    CHECK(v->npages == 11 && v->cursor == 0);
    CHECK(starts_with(v->titles[0], "\xE2\x86\x92 1. Getting started"));
    CHECK(strcmp(v->titles[1], "2. Not reached yet") == 0);
    CHECK(strcmp(v->titles[9], "10. Not reached yet") == 0);
    CHECK(strcmp(v->titles[10], "11. Not reached yet") == 0);
    CHECK(!list_has("southern road") && !list_has("Spy Satellite"));
    keys(DW3_HELP_KEY_DOWN);
    CHECK(strstr(dw3_wt_view()->strip, "Not reached yet") != NULL);
    keys(DW3_HELP_KEY_CONFIRM);                       /* chapter 2: nothing reached */
    v = dw3_wt_view();
    CHECK(v->screen == DW3_HELP_SCREEN_LIST && strcmp(v->header, "Walkthrough") == 0);
    CHECK(!list_has("East Station") && !list_has("Zanbamon") && !list_has("Area: South") &&
          list_has("Area: Asuka City")); /* Asuka City was seen in chapter 1 */
    keys(DW3_HELP_KEY_CONFIRM);
    CHECK(strcmp(dw3_wt_view()->header, "Not reached yet") == 0 && !page_has("East Station"));
    keys(DW3_HELP_KEY_CONFIRM);
    keys(DW3_HELP_KEY_DOWN);
    keys(DW3_HELP_KEY_CONFIRM);
    CHECK(strcmp(dw3_wt_view()->header, "East Station") == 0 && page_has("Seiryu City"));
    dw3_wt_close();
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
    CHECK(list_has("New faces around Asuka") && dw3_wt_view()->npages == 9);
    dw3_wt_close();
    /* progress 0x14: the third step of chapter 5, the chapter list marks the chapter */
    state(0x14, 0);
    CHECK(dw3_wt_open(&c) == 0);
    v = dw3_wt_view();
    CHECK(strcmp(v->header, "Suzaku UG Lake") == 0);
    CHECK(strstr(v->strip, "Step 3 / 8") != NULL && strstr(v->strip, "Current step") != NULL);
    CHECK(page_has("Secret Room") && !page_has("flag"));
    keys(DW3_HELP_KEY_BACK);
    v = dw3_wt_view();
    CHECK(v->screen == DW3_HELP_SCREEN_LIST && v->cursor == 2 &&
          strcmp(v->header, "5. West Sector rebels") == 0);
    CHECK(starts_with(v->titles[2], "\xE2\x86\x92 ") && list_has("Secret Room again") == 0);
    CHECK(v->npages == 10); /* 8 steps and the 2 areas of the chapter: West Sector, Asuka City */
    keys(DW3_HELP_KEY_BACK);
    v = dw3_wt_view();
    CHECK(v->cursor == 4 && starts_with(v->titles[4], "\xE2\x86\x92 5. ") &&
          strcmp(v->titles[5], "6. Not reached yet") == 0);
    CHECK(strstr(v->strip, "Current chapter") != NULL);
    CHECK(strcmp(v->titles[3], "4. Byakko City") == 0); /* reached, past */
    dw3_wt_close();
    /* the same point with the flag set (the lake scene is done): the next step */
    state_codes(0x14, WT_FLAG(0x40, 0x03), 0);
    CHECK(dw3_wt_open(&c) == 0);
    CHECK(strcmp(dw3_wt_view()->header, "Secret Room again") == 0);
    dw3_wt_close();
    /* an item as the done condition: progress 9 */
    state_codes(9, WT_FLAG(0x1A, 0x1B), 0);
    CHECK(dw3_wt_open(&c) == 0);
    CHECK(strcmp(dw3_wt_view()->header, "Item from the Sewers") == 0);
    dw3_wt_close();
    state_codes(9, WT_FLAG(0x1A, 0x1B), WT_ITEM(0, 0x15));
    CHECK(dw3_wt_open(&c) == 0);
    CHECK(strcmp(dw3_wt_view()->header, "Shaman House again") == 0);
    dw3_wt_close();
    /* the last step of the first half */
    state_codes(0x29, WT_FLAG(0x40, 0x74), 0);
    CHECK(dw3_wt_open(&c) == 0);
    CHECK(strcmp(dw3_wt_view()->header, "Snatchmon") == 0);
    CHECK(strstr(dw3_wt_view()->strip, "Step 7 / 7") != NULL);
    dw3_wt_close();
    /* 0x2A is written by no script: a gap, so "not recognised" with the chapter list, and no
     * "more chapters" page any more */
    state(0x2A, 0);
    CHECK(dw3_wt_open(&c) == 0);
    v = dw3_wt_view();
    CHECK(v->screen == DW3_HELP_SCREEN_LIST && dw3_wt_level() == DW3_WT_LEVEL_CHAPTERS &&
          strstr(v->strip, "not recognised") != NULL && v->npages == 11);
    CHECK(!list_has("More chapters") && strcmp(v->titles[1], "2. Not reached yet") == 0);
    dw3_wt_close();
    /* a value past the end of the table (0x2D on US): everything is behind the player and the
     * notice says the step is not recognised */
    state(0x2D, 0);
    CHECK(dw3_wt_open(&c) == 0);
    v = dw3_wt_view();
    CHECK(dw3_wt_level() == DW3_WT_LEVEL_CHAPTERS && v->npages == 11 &&
          strstr(v->strip, "not recognised") != NULL);
    CHECK(list_has("10. Spy Satellite") && list_has("11. The last battle") && !list_has("Not reached"));
    keys(DW3_HELP_KEY_DOWN);
    keys(DW3_HELP_KEY_CONFIRM);                       /* the whole table is readable */
    keys(DW3_HELP_KEY_CONFIRM);
    CHECK(strcmp(dw3_wt_view()->header, "A.o.A HQ scenes") == 0 || dw3_wt_level() == DW3_WT_LEVEL_PAGE);
    dw3_wt_close();
    /* the last battle (0x2B, both discs): the page of the only step of chapter 11 */
    state(0x2B, 0);
    CHECK(dw3_wt_open(&c) == 0);
    v = dw3_wt_view();
    CHECK(dw3_wt_level() == DW3_WT_LEVEL_PAGE && strcmp(v->header, "The final battle") == 0);
    CHECK(strstr(v->strip, "Step 1 / 1") != NULL && strstr(v->strip, "Current step") != NULL);
    CHECK(page_has("opponents") && !page_has("Kusanagi"));        /* the ending is the answer */
    keys(DW3_HELP_KEY_CONFIRM);
    keys(DW3_HELP_KEY_DOWN);
    keys(DW3_HELP_KEY_CONFIRM);
    CHECK(page_has("Kusanagi City") && page_has("European disc"));
    keys(DW3_HELP_KEY_BACK);
    v = dw3_wt_view();
    CHECK(v->screen == DW3_HELP_SCREEN_LIST && strcmp(v->header, "11. The last battle") == 0);
    keys(DW3_HELP_KEY_BACK);
    v = dw3_wt_view();
    CHECK(v->cursor == 10 && starts_with(v->titles[10], "\xE2\x86\x92 11. The last battle") &&
          strstr(v->strip, "Current chapter") != NULL);
    CHECK(!list_has("Extra chapter"));                                  /* US has no extra chapter */
    dw3_wt_close();
    /* the debug value: not recognised, the chapter list with the notice, the first chapter only */
    state(0, 0);
    g_unknown = 1;
    CHECK(dw3_wt_open(&c) == 0);
    v = dw3_wt_view();
    CHECK(v->screen == DW3_HELP_SCREEN_LIST && strstr(v->strip, "not recognised") != NULL);
    CHECK(strcmp(v->titles[0], "1. Getting started") == 0 &&
          strcmp(v->titles[1], "2. Not reached yet") == 0);
    keys(DW3_HELP_KEY_CONFIRM);
    CHECK(list_has("Visit the Online Center") && !list_has("Log in to Asuka City"));
    keys(DW3_HELP_KEY_BACK);
    keys(DW3_HELP_KEY_DOWN);
    CHECK(strstr(dw3_wt_view()->strip, "not recognised") == NULL);
    dw3_wt_close();
    g_unknown = 0;
    /* the title: no game state is read at all */
    c = cfg(DW3_PAGES_US, 0, 0);
    state(0x14, 0);
    CHECK(dw3_wt_open(&c) == 0);
    CHECK(g_snap_calls == 0);
    v = dw3_wt_view();
    CHECK(v->screen == DW3_HELP_SCREEN_LIST && strstr(v->strip, "No game loaded") != NULL);
    CHECK(strcmp(v->titles[0], "1. Getting started") == 0 &&
          strcmp(v->titles[4], "5. Not reached yet") == 0);
    keys(DW3_HELP_KEY_CONFIRM);
    CHECK(list_has("Visit the Online Center") && !list_has("Seiryu"));
    CHECK(v->in_game == 0);
    dw3_wt_close();
}

/* L1 / R1 cross from one chapter to the next; there is nothing after the last chapter. */
static void test_neighbours(void) {
    Dw3WtConfig c = cfg(DW3_PAGES_US, 0, 1);
    int i;

    state(0xB, 0);
    CHECK(dw3_wt_open(&c) == 0);
    CHECK(strcmp(dw3_wt_view()->header, "Back to Main Lobby") == 0);
    keys(DW3_HELP_KEY_PREV);                          /* the last line of chapter 2 */
    CHECK(starts_with(dw3_wt_view()->header, "Area: "));
    keys(DW3_HELP_KEY_NEXT);
    CHECK(strcmp(dw3_wt_view()->header, "Back to Main Lobby") == 0);
    dw3_wt_close();
    state_codes(0x29, WT_FLAG(0x40, 0x74), 0);
    CHECK(dw3_wt_open(&c) == 0);
    for (i = 0; i < 12 && strcmp(dw3_wt_view()->header, "Not reached yet") != 0; i++) {
        keys(DW3_HELP_KEY_NEXT);                      /* across the edge into chapter 11 */
    }
    CHECK(strcmp(dw3_wt_view()->header, "Not reached yet") == 0);
    keys(DW3_HELP_KEY_PREV);
    CHECK(starts_with(dw3_wt_view()->header, "Area: "));
    dw3_wt_close();
    /* US: chapter 11 is the last one; Next stops at its last line */
    state(0x2B, 0);
    CHECK(dw3_wt_open(&c) == 0);
    CHECK(strcmp(dw3_wt_view()->header, "The final battle") == 0);
    for (i = 0; i < 4; i++) {
        keys(DW3_HELP_KEY_NEXT);
    }
    CHECK(starts_with(dw3_wt_view()->header, "Area: ") && dw3_wt_level() == DW3_WT_LEVEL_PAGE);
    keys(DW3_HELP_KEY_PREV);
    CHECK(strcmp(dw3_wt_view()->header, "The final battle") == 0);
    dw3_wt_close();
}


/* Moves the cursor of the open list to the entry whose title is `title`; 1 when it got there. */
static int goto_entry(const char *title) {
    int i;
    int n = dw3_wt_view()->npages;

    for (i = 0; i < n; i++) {
        keys(DW3_HELP_KEY_UP);
    }
    for (i = 0; i < n; i++) {
        if (strcmp(dw3_wt_view()->titles[dw3_wt_view()->cursor], title) == 0) {
            return 1;
        }
        keys(DW3_HELP_KEY_DOWN);
    }
    return 0;
}

/* From the open page of the current step to the step list of the next chapter (the player has
 * not reached it): the entries of an unreached chapter. Returns the entry count. */
static int next_chapter_count(void) {
    keys(DW3_HELP_KEY_BACK);                          /* the chapter's steps */
    keys(DW3_HELP_KEY_BACK);                          /* the chapters */
    keys(DW3_HELP_KEY_DOWN);
    keys(DW3_HELP_KEY_CONFIRM);
    return dw3_wt_view()->npages;
}

/* The two-action reveal on the page that is open: the help text is absent until Yes. */
static void reveal(const char *help_word, const char *goal_word) {
    CHECK(page_has(goal_word) && !page_has(help_word));
    keys(DW3_HELP_KEY_CONFIRM);
    CHECK(strcmp(dw3_wt_view()->header, "Show the answer?") == 0 && !page_has(help_word));
    keys(DW3_HELP_KEY_DOWN);
    keys(DW3_HELP_KEY_CONFIRM);
    CHECK(page_has(help_word));
}

/* Slice 4, the side content: a chapter's optional rows are listed once the chapter is reached and
 * not a progress value before; the goal is visible, the help sits behind the two actions; the
 * extra chapter's rows exist on EU only. */
static void test_side_content(void) {
    static const struct {
        int region;
        int before;                                   /* progress one value before the chapter */
        int at;                                       /* the chapter's first value */
        const char *rows[2];
        const char *goal_word[2];
        const char *help_word[2];
    } cases[] = {
        {DW3_PAGES_US, 0xE, 0xF, {"The Seabed", "The Seabed forge"}, {"UG Lake", "weapon forge"},
         {"Divermon", "Seehomon"}},
        {DW3_PAGES_EU, 0xE, 0xF, {"The Seabed", "The Seabed forge"}, {"UG Lake", "weapon forge"},
         {"Divermon", "Seehomon"}},
        {DW3_PAGES_US, 0x1D, 0x1E, {"The Circuit Board", "The Black Kingz' den"}, {"several sectors", "card duels"},
         {"Numemon", "Eight Black"}},
        {DW3_PAGES_EU, 0x1D, 0x1E, {"The Circuit Board", "The Black Kingz' den"}, {"several sectors", "card duels"},
         {"Numemon", "Eight Black"}},
        {DW3_PAGES_EU, 0x2B, 0x2D, {"A set of five weapons", "Partners to win back"}, {"North Sector", "recruited"},
         {"Kulon Weapons", "level 45"}},
    };
    int k;

    for (k = 0; k < (int)(sizeof(cases) / sizeof(cases[0])); k++) {
        Dw3WtConfig c = cfg(cases[k].region, 0, 1);
        const Dw3HelpView *v;
        int hidden;
        int shown;
        int r;

        /* one value before the chapter: its step list has no optional row, and says so */
        state(cases[k].before, 0);
        CHECK(dw3_wt_open(&c) == 0);
        hidden = next_chapter_count();
        CHECK(!list_has(cases[k].rows[0]) && !list_has(cases[k].rows[1]));
        dw3_wt_close();
        /* the chapter's first value: both rows are listed, as Optional, after the main steps */
        state(cases[k].at, 0);
        CHECK(dw3_wt_open(&c) == 0);
        keys(DW3_HELP_KEY_BACK);
        v = dw3_wt_view();
        shown = v->npages;
        CHECK(shown == hidden + 2);
        for (r = 0; r < 2; r++) {
            CHECK(goto_entry(cases[k].rows[r]));
            CHECK(strstr(dw3_wt_view()->strip, "Optional") != NULL);
            keys(DW3_HELP_KEY_CONFIRM);
            CHECK(dw3_wt_view()->screen == DW3_HELP_SCREEN_PAGE &&
                  strcmp(dw3_wt_view()->header, cases[k].rows[r]) == 0);
            CHECK(strstr(dw3_wt_view()->strip, "Optional") != NULL);
            reveal(cases[k].help_word[r], cases[k].goal_word[r]);
            keys(DW3_HELP_KEY_BACK);                  /* the steps; the answer closes again */
            CHECK(dw3_wt_level() == DW3_WT_LEVEL_STEPS);
            keys(DW3_HELP_KEY_CONFIRM);
            CHECK(!page_has(cases[k].help_word[r]));
            keys(DW3_HELP_KEY_BACK);
        }
        dw3_wt_close();
    }
    /* the extra chapter's rows are not on US: the value 0x2D is past its table, so the chapter
     * list opens with all 11 chapters reached, and chapters 4 and 8 list their optional rows */
    {
        Dw3WtConfig us = cfg(DW3_PAGES_US, 0, 1);
        int i;

        state(0x2D, 0);
        g_asked_eu = 0;
        CHECK(dw3_wt_open(&us) == 0);
        CHECK(g_asked_eu == 0 && dw3_wt_level() == DW3_WT_LEVEL_CHAPTERS && dw3_wt_view()->npages == 11);
        for (i = 0; i < 11; i++) {
            CHECK(strstr(dw3_wt_view()->titles[i], "Not reached") == NULL);
        }
        for (i = 0; i < 3; i++) {
            keys(DW3_HELP_KEY_DOWN);                  /* chapter 4 */
        }
        keys(DW3_HELP_KEY_CONFIRM);
        CHECK(list_has("The Seabed") && !list_has("Circuit Board") && !list_has("weapons"));
        keys(DW3_HELP_KEY_BACK);
        for (i = 0; i < 4; i++) {
            keys(DW3_HELP_KEY_DOWN);                  /* chapter 8 */
        }
        keys(DW3_HELP_KEY_CONFIRM);
        CHECK(list_has("The Circuit Board") && list_has("Black Kingz") && !list_has("Seabed"));
        dw3_wt_close();
    }
    /* French on EU: the rows are translated, no English fallback line */
    {
        Dw3WtConfig fr = cfg(DW3_PAGES_EU, 1, 1);

        state(0x2D, 0);
        CHECK(dw3_wt_open(&fr) == 0);
        keys(DW3_HELP_KEY_BACK);
        CHECK(list_has("Cinq armes") && list_has("Partenaires"));
        dw3_wt_close();
    }
}

/* The EU extra chapter: region EU only, three steps that go by flags. */
static void test_extra_chapter(void) {
    Dw3WtConfig eu = cfg(DW3_PAGES_EU, 0, 1);
    Dw3WtConfig us = cfg(DW3_PAGES_US, 0, 1);
    const Dw3HelpView *v;

    g_asked_eu = 0;
    state(0x2D, 0);
    CHECK(dw3_wt_open(&eu) == 0);
    CHECK(g_asked_eu > 0);                            /* EU reads the extra chapter's codes */
    v = dw3_wt_view();
    CHECK(dw3_wt_level() == DW3_WT_LEVEL_PAGE && strcmp(v->header, "Login Room tour") == 0);
    CHECK(strstr(v->strip, "Step 1 / 3") != NULL && strstr(v->strip, "Current step") != NULL);
    keys(DW3_HELP_KEY_BACK);
    keys(DW3_HELP_KEY_BACK);
    v = dw3_wt_view();
    CHECK(v->npages == 12 && v->cursor == 11 && starts_with(v->titles[11], "\xE2\x86\x92 12. Extra chapter"));
    CHECK(strcmp(v->titles[10], "11. The last battle") == 0);
    dw3_wt_close();
    state_codes(0x2D, WT_FLAG(0x40, 0xCD), 0);
    CHECK(dw3_wt_open(&eu) == 0);
    CHECK(strcmp(dw3_wt_view()->header, "Four challengers") == 0);
    dw3_wt_close();
    state_codes(0x2D, WT_FLAG(0x40, 0xCD), WT_SPECIAL(0x54));
    CHECK(dw3_wt_open(&eu) == 0);
    CHECK(strcmp(dw3_wt_view()->header, "Kurt at the Arena") == 0);
    CHECK(strstr(dw3_wt_view()->strip, "Step 3 / 3") != NULL);
    keys(DW3_HELP_KEY_NEXT);                          /* the last chapter: nothing after its area */
    keys(DW3_HELP_KEY_NEXT);
    keys(DW3_HELP_KEY_NEXT);
    keys(DW3_HELP_KEY_NEXT);
    CHECK(dw3_wt_is_open());
    dw3_wt_close();
    /* the same value on US: the extra chapter does not exist */
    state_codes(0x2D, WT_FLAG(0x40, 0xCD), WT_SPECIAL(0x54));
    g_asked_eu = 0;
    CHECK(dw3_wt_open(&us) == 0);
    CHECK(g_asked_eu == 0);                           /* US never asks the game about EU-only flags */
    v = dw3_wt_view();
    CHECK(dw3_wt_level() == DW3_WT_LEVEL_CHAPTERS && v->npages == 11 && !list_has("Extra chapter"));
    dw3_wt_close();
    /* EU at the last battle: chapter 12 is still ahead of the player */
    state(0x2B, 0);
    CHECK(dw3_wt_open(&eu) == 0);
    keys(DW3_HELP_KEY_BACK);
    keys(DW3_HELP_KEY_BACK);
    v = dw3_wt_view();
    CHECK(v->npages == 12 && strcmp(v->titles[11], "12. Not reached yet") == 0);
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
    state(0x14, 0);
    CHECK(dw3_wt_open(&c) == 0);
    CHECK(strstr(dw3_wt_view()->strip, "\xC3\x89tape 3 / 8") != NULL);
    keys(DW3_HELP_KEY_BACK);
    CHECK(strncmp(dw3_wt_view()->header, "5. ", 3) == 0);
    keys(DW3_HELP_KEY_BACK);
    CHECK(strncmp(dw3_wt_view()->titles[5], "6. ", 3) == 0);
    dw3_wt_close();
    /* a language whose file is missing keys falls back to English, once */
    {
        static const char part[] =
            "@format = 1\n@book = walkthrough\n@lang = de\n@status = draft\n"
            "wt.ch01_s03.title = Nur der Titel\n";

        dw3_wt_test_set_text(2, part);
        c = cfg(DW3_PAGES_EU, 2, 1);
        state(2, 0);
        CHECK(dw3_wt_open(&c) == 0);
        CHECK(strcmp(dw3_wt_view()->header, "Nur der Titel") == 0);
        CHECK(page_has("Main Lobby")); /* the goal came from English */
        dw3_wt_close();
        dw3_wt_test_set_text(2, NULL);
    }
}

/* One view fits: header and each line of a list in the title face, the strip and the hint row. */
static void fit_view(void) {
    const Dw3HelpView *v = dw3_wt_view();
    int hint_w = 0;
    int h;
    int k;

    CHECK(fits(DW3_ATLAS_TITLE, v->header, 410));
    CHECK(fits(DW3_ATLAS_BODY, v->strip, 664));
    if (v->screen == DW3_HELP_SCREEN_LIST) {
        for (k = 0; k < v->npages; k++) {
            CHECK(fits(DW3_ATLAS_TITLE, v->titles[k], 410));
        }
    }
    for (h = 0; h < v->nhints; h++) {
        hint_w += dw3_atlas_measure(DW3_ATLAS_BODY, v->hint[h], (int)strlen(v->hint[h])) +
                  (v->hint_cap[h] ? 36 : 0) + 44;
    }
    CHECK(hint_w <= 740);
}

/* The three modes of the page on screen: closed, the question, the answer. */
static void fit_page_modes(void) {
    int s;

    for (s = 0; s < 3; s++) {
        fit_view();
        if (s == 0) {
            keys(DW3_HELP_KEY_CONFIRM); /* ask */
        } else if (s == 1) {
            keys(DW3_HELP_KEY_DOWN);
            keys(DW3_HELP_KEY_CONFIRM); /* shown */
        }
    }
}

/* Everything of one state: the chapter list, every chapter's list and each page in its three
 * modes. The screen is walked with the keys, Back from a page returns to the list on the item. */
static void fit_state(int lang, int in_game, int progress, int unknown) {
    Dw3WtConfig c = cfg(DW3_PAGES_EU, lang, in_game);
    int nch;
    int ch;

    if (progress == 0x2D) {                           /* the extra chapter, up to its last step */
        state_codes(progress, WT_FLAG(0x40, 0xCD), WT_SPECIAL(0x54));
    } else {
        state(progress, progress == 5);
    }
    g_unknown = unknown;
    CHECK(dw3_wt_open(&c) == 0);
    while (dw3_wt_level() == DW3_WT_LEVEL_PAGE || dw3_wt_level() == DW3_WT_LEVEL_STEPS) {
        keys(DW3_HELP_KEY_BACK);
    }
    CHECK(dw3_wt_level() == DW3_WT_LEVEL_CHAPTERS);
    nch = dw3_wt_view()->npages;
    CHECK(nch == 12);                                 /* EU: the story and the extra chapter */
    for (ch = 0; ch < nch; ch++) {
        keys(DW3_HELP_KEY_UP);
    }
    fit_view();
    for (ch = 0; ch < nch; ch++) {
        int i;
        int n;

        keys(DW3_HELP_KEY_CONFIRM);
        CHECK(dw3_wt_level() == DW3_WT_LEVEL_STEPS);
        n = dw3_wt_view()->npages;
        fit_view();
        for (i = 0; i < n; i++) {
            keys(DW3_HELP_KEY_UP);
        }
        for (i = 0; i < n; i++) {
            keys(DW3_HELP_KEY_CONFIRM);
            CHECK(dw3_wt_level() == DW3_WT_LEVEL_PAGE);
            fit_page_modes();
            keys(DW3_HELP_KEY_BACK);
            CHECK(dw3_wt_level() == DW3_WT_LEVEL_STEPS);
            keys(DW3_HELP_KEY_DOWN);
        }
        keys(DW3_HELP_KEY_BACK);
        CHECK(dw3_wt_level() == DW3_WT_LEVEL_CHAPTERS);
        keys(DW3_HELP_KEY_DOWN);
    }
    dw3_wt_close();
    g_unknown = 0;
}

/* Everything that must fit does fit, in every language and state. */
static void test_fit(void) {
    static const int points[] = {0, 2, 4, 5, 0x10, 0x14, 0x20, 0x29, 0x2A, 0x2B, 0x2D};
    int lang;
    int p;

    for (lang = 0; lang < 5; lang++) {
        for (p = 0; p < (int)(sizeof(points) / sizeof(points[0])); p++) {
            fit_state(lang, 1, points[p], 0);
        }
        fit_state(lang, 1, 0, 1);   /* not recognised */
        fit_state(lang, 0, 0, 0);   /* the title: browse mode */
    }
}

int main(void) {
    test_current_step_and_reveal();
    test_never_ahead();
    test_other_points();
    test_neighbours();
    test_extra_chapter();
    test_side_content();
    test_region_and_language();
    test_fit();
    if (g_fail != 0) {
        fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }
    printf("OK: walkthrough screen\n");
    return 0;
}
