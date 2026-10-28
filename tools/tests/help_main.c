/* P6.30 help host test: the real embedded en.txt on the real atlas metrics,
 * the page list, scrolling, page changes, region / host filters, language
 * fallback. Log lines go to stderr; test_help.py checks them. */
#include "port_atlas.h"
#include "port_help.h"

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

static Dw3HelpConfig cfg(int region, int platform, int lang, int from) {
    Dw3HelpConfig c;

    memset(&c, 0, sizeof(c));
    c.from = from;
    c.region = region;
    c.platform = platform;
    c.lang = lang;
    c.in_game = from != DW3_HELP_FROM_TITLE;
    c.root = platform == DW3_PAGES_SWITCH ? "sdmc:/switch/dw3/" : "/home/u/dw3/";
    c.memcard = platform == DW3_PAGES_SWITCH ? "sdmc:/switch/dw3/us/memcard/u1/"
                                             : "/home/u/dw3/us/memcard/";
    c.version = "0.6.5-test";
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

static int page_has(const char *needle, int *lines) {
    /* the page on screen; every line, not only the visible ones */
    const Dw3HelpView *v = dw3_help_view();
    int i;

    if (lines != NULL) {
        *lines = v->nlines;
    }
    for (i = 0; i < v->nlines; i++) {
        if (line_has(&v->lines[i], needle)) {
            return 1;
        }
    }
    return 0;
}

static void test_flow(void) {
    Dw3HelpConfig c = cfg(DW3_PAGES_US, DW3_PAGES_PC, 0, DW3_HELP_FROM_TITLE);
    const Dw3HelpView *v;
    int n;
    int i;

    CHECK(dw3_help_open(&c) == 0 && dw3_help_is_open());
    v = dw3_help_view();
    CHECK(v->screen == DW3_HELP_SCREEN_LIST && v->npages == 10);
    CHECK(strcmp(v->titles[0], "Setup") == 0 && strcmp(v->titles[1], "Controls") == 0);
    CHECK(strstr(v->strip, "first launch") != NULL);
    /* down, down, up: the cursor sound only when it moves */
    CHECK(dw3_help_tick(DW3_HELP_KEY_UP) == DW3_HELP_SND_NONE);
    CHECK(dw3_help_tick(DW3_HELP_KEY_DOWN) == DW3_HELP_SND_CURSOR && v->cursor == 1);
    CHECK(strstr(v->strip, "Which button") != NULL);
    /* open Controls */
    CHECK(dw3_help_tick(DW3_HELP_KEY_CONFIRM) == DW3_HELP_SND_SELECT);
    CHECK(v->screen == DW3_HELP_SCREEN_PAGE && v->nlines > v->panel);
    CHECK(strncmp(v->header, "Controls", 8) == 0 && strstr(v->header, "(draft)") != NULL);
    CHECK(strcmp(v->strip, "Page 2 / 10") == 0);
    CHECK(v->top == 0 && !v->arrow_up && v->arrow_down);
    CHECK(page_has("[X]", NULL) && !page_has("[?", NULL)); /* PC key names */
    CHECK(page_has("F4 toggles fullscreen", NULL));         /* @pc line */
    /* line scroll */
    CHECK(dw3_help_tick(DW3_HELP_KEY_DOWN) == DW3_HELP_SND_CURSOR && v->top == 1);
    CHECK(v->arrow_up);
    CHECK(dw3_help_tick(DW3_HELP_KEY_UP) == DW3_HELP_SND_CURSOR && v->top == 0);
    CHECK(dw3_help_tick(DW3_HELP_KEY_UP) == DW3_HELP_SND_NONE && v->top == 0);
    /* screenfuls to the end */
    n = v->nlines;
    for (i = 0; i < 20 && v->top + v->panel < n; i++) {
        dw3_help_tick(DW3_HELP_KEY_RIGHT);
    }
    CHECK(v->top + v->panel >= n && !v->arrow_down && v->arrow_up);
    CHECK(dw3_help_tick(DW3_HELP_KEY_DOWN) == DW3_HELP_SND_NONE);
    dw3_help_tick(DW3_HELP_KEY_LEFT);
    CHECK(v->top < n - v->panel);
    /* Cross does nothing on a page */
    {
        int top = v->top;
        CHECK(dw3_help_tick(DW3_HELP_KEY_CONFIRM) == DW3_HELP_SND_NONE && v->top == top);
    }
    /* L / R: previous and next page */
    CHECK(dw3_help_tick(DW3_HELP_KEY_NEXT) == DW3_HELP_SND_SELECT);
    CHECK(strcmp(v->strip, "Page 3 / 10") == 0 && v->top == 0);
    CHECK(strncmp(v->header, "Port hotkeys", 12) == 0);
    CHECK(dw3_help_tick(DW3_HELP_KEY_PREV) == DW3_HELP_SND_SELECT);
    CHECK(dw3_help_tick(DW3_HELP_KEY_PREV) == DW3_HELP_SND_SELECT);
    CHECK(strcmp(v->strip, "Page 1 / 10") == 0);
    CHECK(dw3_help_tick(DW3_HELP_KEY_PREV) == DW3_HELP_SND_NONE);   /* first page */
    /* Back: to the list on the page that was open, then out */
    CHECK(dw3_help_tick(DW3_HELP_KEY_BACK) == DW3_HELP_SND_CANCEL);
    CHECK(v->screen == DW3_HELP_SCREEN_LIST && v->cursor == 0 && dw3_help_is_open());
    /* the last page, and no next */
    for (i = 0; i < 12; i++) {
        dw3_help_tick(DW3_HELP_KEY_DOWN);
    }
    CHECK(v->cursor == 9);
    dw3_help_tick(DW3_HELP_KEY_CONFIRM);
    CHECK(dw3_help_tick(DW3_HELP_KEY_NEXT) == DW3_HELP_SND_NONE);
    CHECK(strcmp(v->strip, "Page 10 / 10") == 0);
    dw3_help_tick(DW3_HELP_KEY_BACK);
    CHECK(dw3_help_tick(DW3_HELP_KEY_BACK) == DW3_HELP_SND_CANCEL && !dw3_help_is_open());
}

static void test_regions(void) {
    Dw3HelpConfig us = cfg(DW3_PAGES_US, DW3_PAGES_SWITCH, 0, DW3_HELP_FROM_FIELD);
    Dw3HelpConfig eu = cfg(DW3_PAGES_EU, DW3_PAGES_SWITCH, 0, DW3_HELP_FROM_BATTLE);
    const Dw3HelpView *v;

    CHECK(dw3_help_open(&us) == 0);
    v = dw3_help_view();
    CHECK(v->in_game == 1);
    dw3_help_tick(DW3_HELP_KEY_CONFIRM);   /* Setup */
    CHECK(page_has("SLUS-01436", NULL) && !page_has("SLES-03936", NULL));
    CHECK(page_has("dw3-switch.nro", NULL) && page_has("sdmc:/switch/dw3/us/", NULL));
    CHECK(!page_has("[?", NULL));
    dw3_help_close();
    CHECK(!dw3_help_is_open());
    CHECK(dw3_help_open(&eu) == 0);
    dw3_help_tick(DW3_HELP_KEY_CONFIRM);
    CHECK(page_has("SLES-03936", NULL) && !page_has("SLUS-01436", NULL));
    /* Switch names on the Controls page; the unverified blocks are tagged */
    dw3_help_tick(DW3_HELP_KEY_NEXT);
    CHECK(page_has("[B]", NULL) && page_has("[ZR]", NULL) && !page_has("F4 toggles", NULL));
    dw3_help_tick(DW3_HELP_KEY_BACK);
    dw3_help_tick(DW3_HELP_KEY_DOWN);
    dw3_help_tick(DW3_HELP_KEY_DOWN);
    dw3_help_tick(DW3_HELP_KEY_DOWN);
    dw3_help_tick(DW3_HELP_KEY_DOWN);   /* Controls -> Saves */
    dw3_help_tick(DW3_HELP_KEY_CONFIRM);
    CHECK(page_has("(unverified)", NULL));
    CHECK(page_has("Switch user profile", NULL));
    dw3_help_close();
    /* on PC the @switch lines and their tags are gone */
    {
        Dw3HelpConfig pc = cfg(DW3_PAGES_EU, DW3_PAGES_PC, 0, DW3_HELP_FROM_TITLE);
        CHECK(dw3_help_open(&pc) == 0);
        for (int i = 0; i < 5; i++) {
            dw3_help_tick(DW3_HELP_KEY_DOWN);
        }
        dw3_help_tick(DW3_HELP_KEY_CONFIRM);
        CHECK(!page_has("Switch user profile", NULL));
        dw3_help_close();
    }
}

static void test_language(void) {
    Dw3HelpConfig fr = cfg(DW3_PAGES_EU, DW3_PAGES_SWITCH, 1, DW3_HELP_FROM_TITLE);
    const Dw3HelpView *v;
    static const char partial[] =
        "@format = 1\n@book = help\n@lang = fr\n@status = test\n"
        "help.controls.title = Commandes\n"
        "help.controls.blurb = Quel bouton fait quoi.\n"
        "help.controls.p.1 = Le jeu pense en boutons PlayStation.\n"
        "help.ui.str.back = Retour\n";

    /* the stub: English, one log line */
    CHECK(dw3_help_open(&fr) == 0);
    v = dw3_help_view();
    CHECK(v->npages == 10 && strcmp(v->titles[1], "Controls") == 0);
    dw3_help_close();
    /* a partial pack: its titles, English for the rest */
    dw3_help_test_set_text(1, partial);
    CHECK(dw3_help_open(&fr) == 0);
    v = dw3_help_view();
    CHECK(strcmp(v->titles[1], "Commandes") == 0 && strcmp(v->titles[0], "Setup") == 0);
    CHECK(strstr(v->hint[1], "Retour") != NULL);
    dw3_help_tick(DW3_HELP_KEY_DOWN);
    dw3_help_tick(DW3_HELP_KEY_CONFIRM);
    CHECK(page_has("Le jeu pense", NULL));
    CHECK(page_has("Cross, confirms", NULL));   /* a key it lacks: English */
    dw3_help_close();
    dw3_help_test_set_text(1, NULL);
}

/* "dump": every page of the real text on the real metrics, both regions and
 * hosts (test_help.py reads the counts). */
static int dump(void) {
    static const int envs[4][2] = {{DW3_PAGES_US, DW3_PAGES_PC},
                                   {DW3_PAGES_EU, DW3_PAGES_PC},
                                   {DW3_PAGES_US, DW3_PAGES_SWITCH},
                                   {DW3_PAGES_EU, DW3_PAGES_SWITCH}};
    int e;

    for (e = 0; e < 4; e++) {
        Dw3HelpConfig c = cfg(envs[e][0], envs[e][1], 0, DW3_HELP_FROM_TITLE);
        const Dw3HelpView *v;
        int p;

        if (dw3_help_open(&c) != 0) {
            return 1;
        }
        v = dw3_help_view();
        for (p = 0; p < v->npages; p++) {
            int lines = 0;

            dw3_help_tick(DW3_HELP_KEY_CONFIRM);
            if (!page_has("zzzzqq", &lines)) {
                (void)0;
            }
            printf("PAGE env=%d/%d %d %s lines=%d unresolved=%d\n", envs[e][0],
                   envs[e][1], p, v->screen == DW3_HELP_SCREEN_PAGE ? "ok" : "?",
                   lines, page_has("[?", NULL));
            dw3_help_tick(DW3_HELP_KEY_BACK);
            dw3_help_tick(DW3_HELP_KEY_DOWN);
        }
        dw3_help_close();
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "dump") == 0) {
        return dump();
    }
    test_flow();
    test_regions();
    test_language();
    CHECK(dw3_atlas_has(0, 0xE9) && dw3_atlas_has(1, 0x2022) && !dw3_atlas_has(0, 0x4E2D));
    CHECK(dw3_atlas_measure(0, "iiii", 4) < dw3_atlas_measure(0, "mmmm", 4));
    if (g_fail) {
        printf("FAILED: %d check(s)\n", g_fail);
        return 1;
    }
    printf("OK: help\n");
    return 0;
}
