#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L /* clock_gettime under -std=c11 */
#endif
/* P6.35 phase 2: the walkthrough screen (port_walkthrough_ui.h,
 * docs/P6.35_WALKTHROUGH.md "As built, phase 2").
 *
 * Two screens: a step list and a step page. The page shows the step's `goal`
 * (safe: where to go, no answers); the `help` text (the spoiler sub-area of the
 * step or of the whole area) is behind a two-action reveal: Answer, then a
 * confirmation whose cursor starts on No. A step the player has not reached
 * keeps its title hidden and needs the same reveal. Nothing is remembered: the
 * answer closes again when the player leaves the page. The game state is read
 * once, when the screen opens, through the snapshot callback; this file never
 * writes game state. */
#include "port_walkthrough_ui.h"
#include "port_atlas.h"
#include "port_options_groups.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef DW3_PORT_OPTIONS_TEST
#define WT_LOG_LINE(s) (fprintf(stderr, "%s\n", (s)))
#else
#include <psyz/log.h>
/* psyz_log: on the Switch the lines reach sdmc:/switch/dw3/dw3.log. */
#define WT_LOG_LINE(s) INFOF("%s", (s))
#endif

#include "walkthrough_text.inc"

static void wlog(const char *fmt, ...) {
    char line[320];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    WT_LOG_LINE(line);
}

static void pages_log(const char *line) {
    WT_LOG_LINE(line);
}

static double now_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

static const char *const k_lang_codes[5] = {"en", "fr", "de", "es", "it"};
static const char *g_test_text[5];

static const char *embedded_text(int lang) {
    if (g_test_text[lang] != NULL) {
        return g_test_text[lang];
    }
    switch (lang) {
    case 1: return wt_text_fr;
    case 2: return wt_text_de;
    case 3: return wt_text_es;
    case 4: return wt_text_it;
    default: return wt_text_en;
    }
}

void dw3_wt_test_set_text(int lang, const char *text) {
    if (lang >= 0 && lang < 5) {
        g_test_text[lang] = text;
    }
}

/* --- state ----------------------------------------------------------------- */

enum { IT_STEP = 0, IT_AREA, MAX_ITEMS = 24, MAX_CHAPTERS = 16 };
enum { SCR_CHAPTERS = 0, SCR_LIST, SCR_PAGE };
enum { PM_CLOSED = 0, PM_ASK, PM_SHOWN };

typedef struct Item {
    int type;
    int table_idx;       /* IT_STEP: row of the step table */
    int reached;
    int current;
    int side;            /* IT_STEP: an optional row */
    int main_no;         /* IT_STEP, main: 1-based order among the main steps */
    char id[24];         /* "ch01.s03", "area.asuka" (the log) */
    char page[24];       /* text key page: "ch01_s03", "area_asuka" */
    const char *chapter; /* chapter id of the step ("ch01"), or NULL */
    int chapter_no;      /* 1-based index of the chapter */
    char title[96];      /* the list line */
    char real_title[96]; /* the page header once the player may read it */
} Item;

/* A chapter of the step table (the rows that share a chapter id), one line of the chapter list. */
typedef struct Chapter {
    const char *id;       /* "ch02" */
    int nmain;            /* MAIN steps of the chapter that apply to this region */
    int reached;          /* the player has reached one of them */
    int current;          /* the current step is one of them */
    char name[96];        /* wt.chNN.title: the chapter's name */
    char title[96];       /* the list line: "2. Name", the arrow on the current chapter */
    char real_title[96];  /* "2. Name": the step list's header once the player may read it */
} Chapter;

static int g_open;
static Dw3WtConfig g_cfg;
static Dw3PagesText g_en, g_lang;
static int g_have_lang;
static Dw3HelpView g_view;
static Item g_items[MAX_ITEMS];
static int g_nitems;
static int g_nmain;                /* MAIN steps of the open chapter */
static Chapter g_chaps[MAX_CHAPTERS];
static int g_nchaps;
static int g_chap;                 /* the chapter whose steps the list shows */
static int g_ccursor, g_cscroll;   /* the chapter list: cursor and first visible line */
static DW3WtResult g_res;
static DW3WtState g_st;
static int g_have_state;
static int g_screen;
static int g_cursor, g_lscroll;
static int g_item;       /* the page's item */
static int g_mode;       /* PM_* */
static int g_ask_yes;    /* ASK: the cursor is on Yes */
static int g_notice;     /* the list strip shows the opening notice until the cursor moves */
static char g_notice_text[200];
static char g_strip_tmp[200];     /* a composed strip line */     /* the last line of the chapter list */
static Dw3PagesLayout g_layout;
static Dw3PagesScroll g_scroll;
static double g_t_open, g_t_page;
static int g_pages_opened, g_spoilers_shown, g_page_shown;
static int g_draft;
static unsigned g_open_serial, g_generation;
static int g_atlas_logged;
static char g_unv_tag[64];

/* --- text ------------------------------------------------------------------ */

#define MAX_FB 64
static char g_fb_seen[MAX_FB][64];
static int g_nfb;

static const char *txt(const char *key) {
    const Dw3PagesEntry *e = NULL;

    if (g_have_lang) {
        e = dw3_pages_find(&g_lang, key);
    }
    if (e != NULL) {
        return e->value;
    }
    e = dw3_pages_find(&g_en, key);
    if (e == NULL) {
        return NULL;
    }
    if (g_have_lang) {
        int i;

        for (i = 0; i < g_nfb; i++) {
            if (strcmp(g_fb_seen[i], key) == 0) {
                return e->value;
            }
        }
        if (g_nfb < MAX_FB) {
            snprintf(g_fb_seen[g_nfb++], sizeof(g_fb_seen[0]), "%s", key);
        }
        wlog("dw3-walkthrough: text fallback %s (%s)", key,
             k_lang_codes[g_cfg.lang]);
    }
    return e->value;
}

static const char *ui(const char *name, const char *dflt) {
    char key[64];
    const char *s;

    snprintf(key, sizeof(key), "wt.ui.str.%s", name);
    s = txt(key);
    return s != NULL ? s : dflt;
}

static const char *txt2(const char *page, const char *layer) {
    char key[96];

    snprintf(key, sizeof(key), "wt.%s.%s", page, layer);
    return txt(key);
}

/* The text of a value with a "TODO(verify)" note cut off; *unv says that it was. */
static const char *cook(const char *src, char *dst, size_t n, int *unv) {
    const char *m;
    size_t len;

    *unv = 0;
    if (src == NULL) {
        dst[0] = '\0';
        return dst;
    }
    m = strstr(src, "TODO(verify)");
    len = m != NULL ? (size_t)(m - src) : strlen(src);
    if (len >= n) {
        len = n - 1;
    }
    memcpy(dst, src, len);
    while (len > 0 && (dst[len - 1] == ' ' || dst[len - 1] == ':')) {
        len--;
    }
    dst[len] = '\0';
    *unv = m != NULL;
    return dst;
}

static int measure_fn(void *ud, int face, const char *s, int len) {
    (void)ud;
    return dw3_atlas_measure(face, s, len);
}

/* A title that is wider than its plaque is cut with an ellipsis (the text
 * budgets keep that from happening; this is the safety). */
enum { TITLE_FIT_PX = 410 };
static void fit_title(char *s) {
    size_t n = strlen(s);

    if (dw3_atlas_measure(DW3_ATLAS_TITLE, s, (int)n) <= TITLE_FIT_PX) {
        return;
    }
    wlog("dw3-walkthrough: title too wide for its plaque, cut: %s", s);
    while (n > 0) {
        n--;
        while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) {
            n--;
        }
        memcpy(s + n, "\xE2\x80\xA6", 4);
        if (dw3_atlas_measure(DW3_ATLAS_TITLE, s, (int)n + 3) <= TITLE_FIT_PX) {
            return;
        }
    }
}

static void layout_opts(Dw3PagesLayoutOpts *o) {
    memset(o, 0, sizeof(*o));
    o->width = DW3_HELP_BODY_W;
    o->bullet_indent = 28;
    o->answer_indent = 26;
    o->bar_indent = 22;
    o->col1_w = 380;
    o->col2_x = 410;
    o->col2_w = DW3_HELP_BODY_W - 410;
    o->measure = measure_fn;
    o->unv_tag = g_unv_tag;
}

/* --- items ------------------------------------------------------------------- */

static int wt_region(void) {
    return g_cfg.region == DW3_PAGES_EU ? WT_REGION_EU : WT_REGION_US;
}

static int row_applies(const DW3WtStep *s) {
    return s->region == WT_REGION_ALL || s->region == wt_region();
}

static void id_to_page(const char *id, char *page, size_t n) {
    size_t i;

    for (i = 0; i + 1 < n && id[i] != '\0'; i++) {
        page[i] = id[i] == '.' ? '_' : id[i];
    }
    page[i] = '\0';
}

static int first_main_index(const DW3WtStep *t, int n) {
    int i;

    for (i = 0; i < n; i++) {
        if (t[i].kind == WT_MAIN && row_applies(&t[i])) {
            return i;
        }
    }
    return -1;
}

static int step_reached(const DW3WtStep *t, int n, int idx) {
    const DW3WtStep *s = &t[idx];

    if (g_have_state && g_res.status == WT_OK) {
        if (s->kind == WT_MAIN) {
            return idx <= g_res.index;
        }
        return g_st.progress >= s->from_progress;
    }
    if (g_have_state && g_res.status == WT_ABOVE) {
        return 1; /* past the end of the table: everything in it is behind the player */
    }
    return s->kind == WT_MAIN && idx == first_main_index(t, n);
}

static void set_real_title(Item *it) {
    const char *s = txt2(it->page, "title");

    snprintf(it->real_title, sizeof(it->real_title), "%s", s != NULL ? s : it->id);
}

/* The chapters of the table, in order: the rows that share a chapter id and apply to this
 * region. A chapter is reached when one of its MAIN steps is (step_reached). */
static void chapters_build(void) {
    const DW3WtStep *t;
    int n;
    int i;

    t = dw3_wt_steps(&n);
    g_nchaps = 0;
    for (i = 0; i < n; i++) {
        Chapter *c;

        if (t[i].kind != WT_MAIN || !row_applies(&t[i])) {
            continue;
        }
        c = g_nchaps > 0 ? &g_chaps[g_nchaps - 1] : NULL;
        if (c == NULL || strcmp(c->id, t[i].chapter) != 0) {
            if (g_nchaps >= MAX_CHAPTERS) {
                break;
            }
            c = &g_chaps[g_nchaps++];
            memset(c, 0, sizeof(*c));
            c->id = t[i].chapter;
        }
        c->nmain++;
        if (step_reached(t, n, i)) {
            c->reached = 1;
        }
        if (g_have_state && g_res.status == WT_OK && g_res.index == i) {
            c->current = 1;
        }
    }
    for (i = 0; i < g_nchaps; i++) {
        Chapter *c = &g_chaps[i];
        const char *s = txt2(c->id, "title");
        char name[64];

        snprintf(name, sizeof(name), "%s", s != NULL ? s : c->id);
        snprintf(c->name, sizeof(c->name), "%s", name);
        snprintf(c->real_title, sizeof(c->real_title), "%d. %s", i + 1, name);
        if (c->reached) {
            char shown[80];

            snprintf(shown, sizeof(shown), "%d. %s", i + 1, name);
            snprintf(c->title, sizeof(c->title), "%s%s", c->current ? "\xE2\x86\x92 " : "", shown);
        } else {
            snprintf(c->title, sizeof(c->title), "%d. %s", i + 1, ui("notreached", "Not reached yet"));
        }
        fit_title(c->title);
        fit_title(c->real_title);
    }
}

static int chapter_of(const char *id) {
    int i;

    for (i = 0; i < g_nchaps; i++) {
        if (strcmp(g_chaps[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

/* The optional rows of a chapter that apply to this disc, and how many of them are listed
 * (those the player's progress has reached): the log line that shows a side block appearing. */
static void optional_log(int chapter) {
    const DW3WtStep *t;
    int n;
    int i;
    int total = 0;
    int listed = 0;

    t = dw3_wt_steps(&n);
    for (i = 0; i < n; i++) {
        if (t[i].kind == WT_SIDE && row_applies(&t[i]) && strcmp(t[i].chapter, g_chaps[chapter].id) == 0) {
            total++;
        }
    }
    for (i = 0; i < g_nitems; i++) {
        listed += g_items[i].side;
    }
    if (total > 0) {
        wlog("dw3-walkthrough: chapter %s optional rows listed %d of %d", g_chaps[chapter].id, listed, total);
    }
}

/* The items of one chapter: its MAIN steps, its optional rows, one spoiler entry per area
 */
static void items_build(int chapter) {
    const DW3WtStep *t;
    const char *cid = g_chaps[chapter].id;
    int n;
    int i;
    int a;

    t = dw3_wt_steps(&n);
    g_chap = chapter;
    g_nitems = 0;
    g_nmain = 0;
    /* main steps, in table order */
    for (i = 0; i < n && g_nitems < MAX_ITEMS - 3; i++) {
        Item *it;

        if (t[i].kind != WT_MAIN || !row_applies(&t[i]) || strcmp(t[i].chapter, cid) != 0) {
            continue;
        }
        it = &g_items[g_nitems++];
        memset(it, 0, sizeof(*it));
        it->type = IT_STEP;
        it->table_idx = i;
        it->chapter = t[i].chapter;
        it->chapter_no = chapter + 1;
        it->main_no = ++g_nmain;
        snprintf(it->id, sizeof(it->id), "%s", t[i].id);
        id_to_page(t[i].id, it->page, sizeof(it->page));
        it->reached = step_reached(t, n, i);
        it->current = g_have_state && g_res.status == WT_OK && g_res.index == i;
    }
    /* optional rows: listed only once the player can have met them */
    for (i = 0; i < n && g_nitems < MAX_ITEMS - 3; i++) {
        Item *it;

        if (t[i].kind != WT_SIDE || !row_applies(&t[i]) || strcmp(t[i].chapter, cid) != 0 ||
            !step_reached(t, n, i)) {
            continue;
        }
        it = &g_items[g_nitems++];
        memset(it, 0, sizeof(*it));
        it->type = IT_STEP;
        it->table_idx = i;
        it->side = 1;
        it->reached = 1;
        it->chapter = t[i].chapter;
        snprintf(it->id, sizeof(it->id), "%s", t[i].id);
        id_to_page(t[i].id, it->page, sizeof(it->page));
    }
    /* the spoiler sub-area of each area of the chapter */
    for (i = 0; i < n && g_nitems < MAX_ITEMS - 1; i++) {
        Item *it;
        int seen = 0;
        int j;

        if (t[i].kind != WT_MAIN || !row_applies(&t[i]) || strcmp(t[i].chapter, cid) != 0) {
            continue;
        }
        for (j = 0; j < g_nitems; j++) {
            if (g_items[j].type == IT_AREA &&
                strcmp(g_items[j].id + 5, t[i].area) == 0) {
                seen = 1;
            }
        }
        if (seen) {
            continue;
        }
        it = &g_items[g_nitems++];
        memset(it, 0, sizeof(*it));
        it->type = IT_AREA;
        it->table_idx = i;
        it->chapter = t[i].chapter;
        snprintf(it->id, sizeof(it->id), "area.%s", t[i].area);
        snprintf(it->page, sizeof(it->page), "area_%s", t[i].area);
        it->reached = 0;
        for (j = 0; j < n; j++) {
            if (t[j].kind == WT_MAIN && row_applies(&t[j]) &&
                strcmp(t[j].area, t[i].area) == 0 && step_reached(t, n, j)) {
                it->reached = 1;
            }
        }
    }
    /* the list lines */
    for (a = 0; a < g_nitems; a++) {
        Item *it = &g_items[a];

        set_real_title(it);
        if (it->type == IT_AREA) {
            char shown[96];

            if (it->reached) {
                snprintf(shown, sizeof(shown), "%s: %s", ui("area", "Area"), it->real_title);
            } else {
                snprintf(shown, sizeof(shown), "%s", ui("notreached", "Not reached yet"));
            }
            snprintf(it->title, sizeof(it->title), "%s", shown);
            snprintf(shown, sizeof(shown), "%s: %s", ui("area", "Area"), it->real_title);
            snprintf(it->real_title, sizeof(it->real_title), "%s", shown);
        } else {
            snprintf(it->title, sizeof(it->title), "%s%s",
                     it->current ? "\xE2\x86\x92 " : "",
                     it->reached ? it->real_title : ui("notreached", "Not reached yet"));
        }
        fit_title(it->title);
        fit_title(it->real_title);
    }
    optional_log(chapter);
}

/* --- view ------------------------------------------------------------------ */

static void set_hint(int i, const char *token, const char *word, int cap) {
    char tok[64];
    char kind[16];
    char name[32];
    const char *colon = strchr(token, ':');

    tok[0] = '\0';
    if (colon != NULL) {
        size_t kl = (size_t)(colon - token);

        if (kl >= sizeof(kind)) {
            kl = sizeof(kind) - 1;
        }
        memcpy(kind, token, kl);
        kind[kl] = '\0';
        snprintf(name, sizeof(name), "%s", colon + 1);
        if (dw3_help_button_name(g_cfg.platform, kind, name, tok, (int)sizeof(tok)) != 0) {
            tok[0] = '\0';
        }
    }
    if (tok[0] != '\0') {
        snprintf(g_view.hint[i], sizeof(g_view.hint[i]), "%s %s", tok, word);
    } else {
        snprintf(g_view.hint[i], sizeof(g_view.hint[i]), "%s", word);
    }
    g_view.hint_cap[i] = cap;
}

static void view_common(void) {
    g_view.in_game = g_cfg.in_game;
    g_view.lang = g_cfg.lang;
    g_view.generation = ++g_generation;
}

static void strip_set(const char *a, const char *b) {
    if (b != NULL && b[0] != '\0') {
        snprintf(g_view.strip, sizeof(g_view.strip), "%s - %s", a, b);
    } else {
        snprintf(g_view.strip, sizeof(g_view.strip), "%s", a);
    }
}

static const char *item_strip_status(const Item *it) {
    if (!it->reached) {
        return ui("ahead_strip", "Not reached yet.");
    }
    if (it->type == IT_AREA) {
        return ui("area_strip", "Spoiler: the whole area");
    }
    if (it->side) {
        return ui("optional", "Optional");
    }
    if (it->current) {
        return ui("current", "Current step");
    }
    snprintf(g_strip_tmp, sizeof(g_strip_tmp), "%s %d - %s", ui("chapter", "Chapter"),
             it->chapter_no, txt2(it->chapter != NULL ? it->chapter : "ch01", "title"));
    return g_strip_tmp;
}

static void view_chapters(void) {
    int i;
    const char *strip;

    g_view.screen = DW3_HELP_SCREEN_LIST;
    snprintf(g_view.header, sizeof(g_view.header), "%s", ui("title", "Walkthrough"));
    fit_title(g_view.header);
    g_view.npages = g_nchaps;
    g_view.cursor = g_ccursor;
    g_cscroll = dw3_optlist_scroll_for(g_cscroll, g_ccursor, g_nchaps);
    g_view.scroll = g_cscroll;
    for (i = 0; i < g_nchaps; i++) {
        g_view.titles[i] = g_chaps[i].title;
    }
    if (g_notice) {
        strip = g_notice_text;
    } else if (!g_chaps[g_ccursor].reached) {
        strip = ui("ahead_strip", "Not reached yet.");
    } else if (g_chaps[g_ccursor].current) {
        strip = ui("curchap", "Current chapter");
    } else {
        strip = ui("pick", "Pick a chapter to read.");
    }
    snprintf(g_view.strip, sizeof(g_view.strip), "%s", strip);
    g_view.nhints = 2;
    set_hint(0, "", ui("open", "Open"), 1);
    set_hint(1, "", ui("back", "Back"), 2);
    g_view.lines = NULL;
    g_view.nlines = g_view.top = g_view.panel = 0;
    g_view.arrow_up = g_view.arrow_down = 0;
    view_common();
}

static void view_list(void) {
    int i;
    const Item *cur = &g_items[g_cursor];

    g_view.screen = DW3_HELP_SCREEN_LIST;
    snprintf(g_view.header, sizeof(g_view.header), "%s",
             g_chaps[g_chap].reached ? g_chaps[g_chap].real_title : ui("title", "Walkthrough"));
    fit_title(g_view.header);
    g_view.npages = g_nitems;
    g_view.cursor = g_cursor;
    g_lscroll = dw3_optlist_scroll_for(g_lscroll, g_cursor, g_nitems);
    g_view.scroll = g_lscroll;
    for (i = 0; i < g_nitems; i++) {
        g_view.titles[i] = g_items[i].title;
    }
    snprintf(g_view.strip, sizeof(g_view.strip), "%s", item_strip_status(cur));
    g_view.nhints = 2;
    set_hint(0, "", ui("open", "Open"), 1);
    set_hint(1, "", ui("back", "Back"), 2);
    g_view.lines = NULL;
    g_view.nlines = g_view.top = g_view.panel = 0;
    g_view.arrow_up = g_view.arrow_down = 0;
    view_common();
}

static void view_page(void) {
    const Item *it = &g_items[g_item];
    const int hidden = !it->reached && g_mode != PM_SHOWN;
    char l[24];
    char r[24];
    int nh = 0;

    g_view.screen = DW3_HELP_SCREEN_PAGE;
    snprintf(g_view.header, sizeof(g_view.header), "%s",
             g_mode == PM_ASK ? ui("ask_hdr", "Show the answer?")
             : hidden        ? ui("notreached", "Not reached yet")
                             : it->real_title);
    fit_title(g_view.header);
    if (it->type == IT_STEP && !it->side && !hidden) {
        char step[64];

        snprintf(step, sizeof(step), "%s %d / %d", ui("step", "Step"), it->main_no, g_nmain);
        strip_set(step, it->current ? ui("current", "Current step") : NULL);
    } else if (it->type == IT_STEP && it->side) {
        strip_set(ui("optional", "Optional"), NULL);
    } else if (it->type == IT_AREA && !hidden) {
        strip_set(ui("area_strip", "Spoiler: the whole area"), NULL);
    } else {
        strip_set(ui("ahead_strip", "Not reached yet."), NULL);
    }
    if (g_draft && it->reached) {
        char tmp[101];

        snprintf(tmp, sizeof(tmp), "%.100s", g_view.strip);
        snprintf(g_view.strip, sizeof(g_view.strip), "%s %.40s", tmp, ui("draft", "(draft)"));
    }
    if (g_mode == PM_ASK) {
        set_hint(nh++, "", ui("choose", "Choose"), 1);
        set_hint(nh++, "", ui("back", "Back"), 2);
    } else {
        if (dw3_pages_scroll_needed(&g_scroll)) {
            set_hint(nh++, "btn:dpad", ui("scroll", "Scroll"), 0);
        }
        dw3_help_button_name(g_cfg.platform, "btn", "l1", l, (int)sizeof(l));
        dw3_help_button_name(g_cfg.platform, "btn", "r1", r, (int)sizeof(r));
        snprintf(g_view.hint[nh], sizeof(g_view.hint[nh]), "%s%s %s", l, r,
                 ui("steps", "Steps"));
        g_view.hint_cap[nh++] = 0;
        if (g_mode == PM_CLOSED) {
            set_hint(nh++, "", ui("answer", "Answer"), 1);
        }
        set_hint(nh++, "", ui("back", "Back"), 2);
    }
    g_view.nhints = nh;
    g_view.lines = g_layout.lines;
    g_view.nlines = g_layout.n;
    g_view.top = g_scroll.top;
    g_view.panel = g_scroll.panel;
    g_view.arrow_up = g_scroll.top > 0;
    g_view.arrow_down = !dw3_pages_scroll_at_end(&g_scroll);
    view_common();
}

/* --- pages ----------------------------------------------------------------- */

static Dw3PagesBook g_book;
static char g_cook[4][700];

static void blk_add(int kind, const char *value, int unv) {
    Dw3PagesBlock *b;

    if (g_book.nblocks >= DW3_PAGES_MAX_BLOCKS) {
        return;
    }
    b = &g_book.blocks[g_book.nblocks++];
    b->kind = kind;
    b->key = "wt";
    b->value = value;
    b->unv = unv;
    g_book.pages[0].nblocks++;
}

/* Lay the page out for the item and mode. Returns the line count, -1 on failure. */
static int page_layout(void) {
    const Item *it = &g_items[g_item];
    Dw3PagesLayoutOpts o;
    int unv_g = 0;
    int unv_h = 0;
    const char *goal = "";
    const char *help = "";
    char ask_a[120];
    char ask_b[120];

    memset(&g_book, 0, sizeof(g_book));
    g_book.npages = 1;
    if (it->type == IT_STEP) {
        goal = cook(txt2(it->page, "goal"), g_cook[0], sizeof(g_cook[0]), &unv_g);
    }
    help = cook(txt2(it->page, "help"), g_cook[1], sizeof(g_cook[1]), &unv_h);
    if (g_mode == PM_ASK) {
        blk_add(DW3_PAGES_P, it->reached ? ui("ask", "Show it?") : ui("ask_ahead", "Show it anyway?"), 0);
        snprintf(ask_a, sizeof(ask_a), "%s%s", g_ask_yes ? "\xE2\x86\x92 " : "", ui("yes", "Yes"));
        snprintf(ask_b, sizeof(ask_b), "%s%s", g_ask_yes ? "" : "\xE2\x86\x92 ", ui("no", "No"));
        blk_add(g_ask_yes ? DW3_PAGES_H : DW3_PAGES_P, ask_a, 0);
        blk_add(g_ask_yes ? DW3_PAGES_P : DW3_PAGES_H, ask_b, 0);
    } else if (g_mode == PM_SHOWN) {
        if (goal[0] != '\0') {
            blk_add(DW3_PAGES_P, goal, unv_g);
        }
        blk_add(DW3_PAGES_H, ui("answer_hdr", "The answer"), 0);
        blk_add(DW3_PAGES_P, help, unv_h);
    } else if (!it->reached) {
        blk_add(DW3_PAGES_NOTE, ui("ahead", "Not reached yet."), 0);
    } else {
        if (goal[0] != '\0') {
            blk_add(DW3_PAGES_P, goal, unv_g);
        }
        blk_add(DW3_PAGES_NOTE, ui("closed", "The answer is hidden."), 0);
    }
    layout_opts(&o);
    dw3_pages_reset_token_log();
    return dw3_pages_layout_page(&g_layout, &g_book, 0, &o);
}

static void page_close_log(void) {
    wlog("dw3-walkthrough: page %s closed after %.0f ms (spoiler shown %s)",
         g_items[g_item].id, now_ms() - g_t_page, g_page_shown ? "y" : "n");
}

static const char *kind_name(const Item *it) {
    return it->type == IT_AREA ? "AREA" : it->side ? "SIDE" : "MAIN";
}

static int page_open(int item, const char *how) {
    const Item *it = &g_items[item];
    double t0 = now_ms();
    int n;

    g_item = item;
    g_mode = PM_CLOSED;
    g_ask_yes = 0;
    g_page_shown = 0;
    n = page_layout();
    if (n < 0) {
        wlog("dw3-walkthrough: page %s cannot be laid out (text too long)", it->id);
        return -1;
    }
    dw3_pages_scroll_init(&g_scroll, n, DW3_HELP_PANEL_LINES);
    g_screen = SCR_PAGE;
    g_pages_opened++;
    g_t_page = now_ms();
    wlog("dw3-walkthrough: page %s (kind %s, from %s, reached %s, current %s, %d lines, "
         "%.0f us)", it->id, kind_name(it), how, it->reached ? "y" : "n",
         it->current ? "y" : "n", n, (g_t_page - t0) * 1000.0);
    wlog("dw3-walkthrough: spoiler %s closed (answer available)", it->id);
    view_page();
    return 0;
}

/* Re-lay the current page after the mode changed. */
static void page_relayout(void) {
    int n = page_layout();

    if (n < 0) {
        wlog("dw3-walkthrough: page %s cannot be laid out (text too long)",
             g_items[g_item].id);
        n = 0;
    }
    dw3_pages_scroll_init(&g_scroll, n, DW3_HELP_PANEL_LINES);
    view_page();
}

/* --- open / close ----------------------------------------------------------- */

int dw3_wt_is_open(void) {
    return g_open;
}

const Dw3HelpView *dw3_wt_view(void) {
    return &g_view;
}

int dw3_wt_level(void) {
    return g_open ? g_screen : DW3_WT_LEVEL_CLOSED;
}

int dw3_wt_item_count(void) {
    if (!g_open) {
        return 0;
    }
    return g_screen == SCR_CHAPTERS ? g_nchaps : g_nitems;
}

static void log_flags_read(void) {
    const DW3WtStep *t;
    int n;
    int i;
    int j;
    unsigned seen[32];
    int nseen = 0;
    char line[512];
    size_t len;

    t = dw3_wt_steps(&n);
    snprintf(line, sizeof(line), "dw3-walkthrough: flags read");
    len = strlen(line);
    for (i = 0; i < n; i++) {
        if (!row_applies(&t[i])) {
            continue; /* the other disc's rows (the EU extra chapter): its flags may not exist here */
        }
        for (j = 0; j < t[i].nunlock + t[i].ndone; j++) {
            const DW3WtCond *c = j < t[i].nunlock ? &t[i].unlock[j]
                                                  : &t[i].done[j - t[i].nunlock];
            int k;
            int dup = 0;

            if ((c->code & 0xF000) == 0x6000) {
                continue; /* a progress test: the progress value is logged already */
            }
            for (k = 0; k < nseen; k++) {
                dup |= seen[k] == c->code;
            }
            if (dup || nseen >= 32) {
                continue;
            }
            seen[nseen++] = c->code;
            if (g_st.check != NULL && len < sizeof(line) - 24) {
                len += (size_t)snprintf(line + len, sizeof(line) - len, " 0x%04x=%d",
                                        c->code, g_st.check(g_st.ctx, c->code, 1) ? 1 : 0);
            }
        }
    }
    if (nseen == 0) {
        snprintf(line + len, sizeof(line) - len, " none");
    }
    WT_LOG_LINE(line);
}

static void atlas_report(void) {
    if (g_atlas_logged) {
        return;
    }
    g_atlas_logged = 1;
    wlog("dw3-pages: atlas %d glyphs in %d faces, %u bytes of coverage in the image",
         dw3_atlas_glyph_count(), DW3_ATLAS_FACES, dw3_atlas_bytes());
}

int dw3_wt_open(const Dw3WtConfig *cfg) {
    char err[160];
    const char *src;
    int env;
    int lang;
    int mode = -1;

    if (g_open) {
        return 0;
    }
    g_cfg = *cfg;
    if (g_cfg.region != DW3_PAGES_EU) {
        g_cfg.region = DW3_PAGES_US;
    }
    if (g_cfg.platform != DW3_PAGES_SWITCH) {
        g_cfg.platform = DW3_PAGES_PC;
    }
    lang = g_cfg.lang >= 0 && g_cfg.lang < 5 ? g_cfg.lang : 0;
    g_cfg.lang = lang;
    env = g_cfg.region | g_cfg.platform;
    g_t_open = now_ms();
    dw3_pages_set_log(pages_log);
    atlas_report();

    src = embedded_text(0);
    if (dw3_pages_parse(&g_en, src, strlen(src), env, err, sizeof(err)) != 0) {
        wlog("dw3-walkthrough: text en cannot be loaded (%s)", err);
        return -1;
    }
    g_have_lang = 0;
    g_nfb = 0;
    if (lang != 0) {
        src = embedded_text(lang);
        if (dw3_pages_parse(&g_lang, src, strlen(src), env, err, sizeof(err)) != 0) {
            wlog("dw3-walkthrough: text %s cannot be loaded (%s), English shown",
                 k_lang_codes[lang], err);
        } else if (g_lang.n == 0) {
            wlog("dw3-walkthrough: text fallback * (%s): the language file has no keys "
                 "yet, English shown", k_lang_codes[lang]);
            dw3_pages_text_free(&g_lang);
        } else {
            g_have_lang = 1;
        }
    }
    g_draft = strstr(g_en.status, "draft") != NULL;
    snprintf(g_unv_tag, sizeof(g_unv_tag), "%s", ui("unverified", "(unverified)"));

    /* the one read of the game */
    memset(&g_st, 0, sizeof(g_st));
    g_have_state = 0;
    memset(&g_res, 0, sizeof(g_res));
    g_res.index = -1;
    if (g_cfg.in_game && g_cfg.snapshot != NULL &&
        g_cfg.snapshot(&g_st, &mode) != 0) {
        g_have_state = 1;
        g_st.region = wt_region();
        dw3_wt_resolve(&g_st, &g_res);
    }
    g_open = 1;
    g_open_serial++;
    memset(&g_view, 0, sizeof(g_view));
    snprintf(g_view.log_tag, sizeof(g_view.log_tag), "dw3-walkthrough");
    g_view.open_serial = g_open_serial;
    g_view.open_ms = g_t_open;
    g_pages_opened = g_spoilers_shown = 0;
    dw3_pages_layout_init(&g_layout);
    chapters_build();
    {
        int nsteps = 0;
        int c;

        for (c = 0; c < g_nchaps; c++) {
            nsteps += g_chaps[c].nmain;
        }
        wlog("dw3-walkthrough: open (from %s, region %s, platform %s, language %s, %d chapters, "
             "%d steps)",
             g_cfg.from == DW3_HELP_FROM_BATTLE ? "battle"
             : g_cfg.from == DW3_HELP_FROM_FIELD ? "field" : "title",
             g_cfg.region == DW3_PAGES_EU ? "EU" : "US",
             g_cfg.platform == DW3_PAGES_SWITCH ? "switch" : "pc", k_lang_codes[lang],
             g_nchaps, nsteps);
    }
    wlog("dw3-walkthrough: text %s status \"%s\", %d key(s) loaded, draft tag %s",
         k_lang_codes[lang], g_en.status, g_en.n, g_draft ? "on" : "off");
    g_notice = 0;
    if (g_have_state) {
        char line[192];

        dw3_wt_format_line(line, sizeof(line), &g_st, &g_res);
        wlog("%s [open mode 0x%x]", line, (unsigned)(mode < 0 ? 0 : mode));
        log_flags_read();
        if (g_res.status != WT_OK) {
            wlog("dw3-walkthrough: step not recognised (progress %d)", g_st.progress);
            snprintf(g_notice_text, sizeof(g_notice_text), "%s",
                     ui("unrecognised", "Step not recognised."));
            g_notice = 1;
        }
    } else {
        wlog("dw3-walkthrough: browse mode (%s)",
             g_cfg.in_game ? "game state unavailable" : "no game state");
        snprintf(g_notice_text, sizeof(g_notice_text), "%s", ui("browse", "Pick a chapter to read."));
        g_notice = 1;
    }
    g_screen = SCR_CHAPTERS;
    g_ccursor = g_cscroll = 0;
    g_cursor = g_lscroll = 0;
    g_chap = 0;
    items_build(0);
    if (g_have_state && g_res.status == WT_OK) {
        int c = chapter_of(g_res.step->chapter);
        int i;

        if (c >= 0) {
            items_build(c);
            g_ccursor = c;
            for (i = 0; i < g_nitems; i++) {
                if (g_items[i].current) {
                    g_cursor = i;
                }
            }
            g_item = g_cursor;
            /* in game: the page of the step comes first, the lists are Back away */
            if (page_open(g_cursor, "open") != 0) {
                g_screen = SCR_LIST;
            }
        }
    }
    if (g_screen == SCR_CHAPTERS) {
        view_chapters();
    } else if (g_screen == SCR_LIST) {
        view_list();
    }
    return 0;
}

void dw3_wt_close(void) {
    if (!g_open) {
        return;
    }
    if (g_screen == SCR_PAGE) {
        page_close_log();
    }
    wlog("dw3-walkthrough: closed after %.0f ms (pages opened %d, spoilers shown %d)",
         now_ms() - g_t_open, g_pages_opened, g_spoilers_shown);
    g_open = 0;
    dw3_pages_layout_free(&g_layout);
    dw3_pages_text_free(&g_en);
    if (g_have_lang) {
        dw3_pages_text_free(&g_lang);
        g_have_lang = 0;
    }
}

/* --- tick ------------------------------------------------------------------ */

/* Opens the chapter's step list; the cursor goes on its current step, else the first line. */
static void chapter_open(int chapter) {
    int i;

    items_build(chapter);
    g_cursor = g_lscroll = 0;
    for (i = 0; i < g_nitems; i++) {
        if (g_items[i].current) {
            g_cursor = i;
        }
    }
    g_screen = SCR_LIST;
    wlog("dw3-walkthrough: chapter %s opened (reached %s, current %s, %d steps)",
         g_chaps[chapter].id, g_chaps[chapter].reached ? "y" : "n",
         g_chaps[chapter].current ? "y" : "n", g_chaps[chapter].nmain);
    view_list();
}

/* L1 / R1 from a page: the neighbour entry of the chapter's list, and across the chapter's
 * edge the last line of the previous chapter, the first of the next one. */
static int page_step(int dir) {
    int chap = g_chap;
    int item = g_item;

    if (dir < 0) {
        if (g_item > 0) {
            item = g_item - 1;
        } else if (g_chap > 0) {
            chap = g_chap - 1;
            item = -1;
        } else {
            return 0;
        }
    } else if (g_item < g_nitems - 1) {
        item = g_item + 1;
    } else if (g_chap < g_nchaps - 1) {
        chap = g_chap + 1;
        item = 0;
    } else {
        return 0;
    }
    page_close_log();
    if (chap != g_chap) {
        items_build(chap);
        if (item < 0) {
            item = g_nitems - 1;
        }
    }
    if (page_open(item, dir < 0 ? "prev" : "next") != 0) {
        return 0;
    }
    return 1;
}

int dw3_wt_tick(unsigned keys) {
    int snd = DW3_HELP_SND_NONE;

    if (!g_open) {
        return snd;
    }
    if (g_screen == SCR_CHAPTERS) {
        if (keys & DW3_HELP_KEY_BACK) {
            dw3_wt_close();
            return DW3_HELP_SND_CANCEL;
        }
        if ((keys & DW3_HELP_KEY_UP) && g_ccursor > 0) {
            g_ccursor--;
            snd = DW3_HELP_SND_CURSOR;
        }
        if ((keys & DW3_HELP_KEY_DOWN) && g_ccursor < g_nchaps - 1) {
            g_ccursor++;
            snd = DW3_HELP_SND_CURSOR;
        }
        if (keys & DW3_HELP_KEY_CONFIRM) {
            g_notice = 0;
            chapter_open(g_ccursor);
            return DW3_HELP_SND_SELECT;
        }
        if (snd != DW3_HELP_SND_NONE) {
            g_notice = 0;
            view_chapters();
        }
        return snd;
    }
    if (g_screen == SCR_LIST) {
        if (keys & DW3_HELP_KEY_BACK) {
            g_screen = SCR_CHAPTERS;
            g_ccursor = g_chap;
            view_chapters();
            return DW3_HELP_SND_CANCEL;
        }
        if ((keys & DW3_HELP_KEY_UP) && g_cursor > 0) {
            g_cursor--;
            snd = DW3_HELP_SND_CURSOR;
        }
        if ((keys & DW3_HELP_KEY_DOWN) && g_cursor < g_nitems - 1) {
            g_cursor++;
            snd = DW3_HELP_SND_CURSOR;
        }
        if (keys & DW3_HELP_KEY_CONFIRM) {
            if (page_open(g_cursor, "list") == 0) {
                return DW3_HELP_SND_SELECT;
            }
        }
        if (snd != DW3_HELP_SND_NONE) {
            view_list();
        }
        return snd;
    }
    /* a page */
    if (g_mode == PM_ASK) {
        if (keys & DW3_HELP_KEY_BACK) {
            wlog("dw3-walkthrough: spoiler %s cancelled", g_items[g_item].id);
            g_mode = PM_CLOSED;
            page_relayout();
            return DW3_HELP_SND_CANCEL;
        }
        if (keys & (DW3_HELP_KEY_UP | DW3_HELP_KEY_DOWN | DW3_HELP_KEY_LEFT |
                    DW3_HELP_KEY_RIGHT)) {
            g_ask_yes = !g_ask_yes;
            page_relayout();
            snd = DW3_HELP_SND_CURSOR;
        }
        if (keys & DW3_HELP_KEY_CONFIRM) {
            if (g_ask_yes) {
                g_mode = PM_SHOWN;
                g_page_shown = 1;
                g_spoilers_shown++;
                wlog("dw3-walkthrough: spoiler %s shown", g_items[g_item].id);
            } else {
                wlog("dw3-walkthrough: spoiler %s cancelled", g_items[g_item].id);
                g_mode = PM_CLOSED;
            }
            page_relayout();
            return DW3_HELP_SND_SELECT;
        }
        return snd;
    }
    if (keys & DW3_HELP_KEY_BACK) {
        page_close_log();
        g_screen = SCR_LIST;
        g_cursor = g_item;
        view_list();
        return DW3_HELP_SND_CANCEL;
    }
    if (keys & (DW3_HELP_KEY_PREV | DW3_HELP_KEY_NEXT)) {
        return page_step((keys & DW3_HELP_KEY_PREV) ? -1 : 1) ? DW3_HELP_SND_SELECT : snd;
    }
    if ((keys & DW3_HELP_KEY_CONFIRM) && g_mode == PM_CLOSED) {
        g_mode = PM_ASK;
        g_ask_yes = 0; /* the cursor starts on No */
        wlog("dw3-walkthrough: spoiler %s asked", g_items[g_item].id);
        page_relayout();
        return DW3_HELP_SND_SELECT;
    }
    {
        int moved = 0;

        if (keys & DW3_HELP_KEY_UP) {
            moved |= dw3_pages_scroll_by(&g_scroll, -1);
        }
        if (keys & DW3_HELP_KEY_DOWN) {
            moved |= dw3_pages_scroll_by(&g_scroll, 1);
        }
        if (moved) {
            snd = DW3_HELP_SND_CURSOR;
        }
        if (keys & DW3_HELP_KEY_LEFT) {
            moved |= dw3_pages_scroll_page(&g_scroll, -1);
        }
        if (keys & DW3_HELP_KEY_RIGHT) {
            moved |= dw3_pages_scroll_page(&g_scroll, 1);
        }
        if (moved) {
            view_page();
        }
    }
    return snd;
}
