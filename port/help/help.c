#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L /* clock_gettime under -std=c11 */
#endif
/* P6.30 technical help (port_help.h, docs/P6.30_HELP.md). */
#include "port_help.h"
#include "port_atlas.h"
#include "port_options_groups.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef DW3_PORT_OPTIONS_TEST
#define HELP_LOG_LINE(s) (fprintf(stderr, "%s\n", (s)))
#else
#include <psyz/log.h>
/* psyz_log: on the Switch the lines reach sdmc:/switch/dw3/dw3.log. */
#define HELP_LOG_LINE(s) INFOF("%s", (s))
#endif

#include "help_text.inc"

static void hlog(const char *fmt, ...) {
    char line[320];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    HELP_LOG_LINE(line);
}

static void pages_log(const char *line) {
    HELP_LOG_LINE(line);
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
    case 1: return help_text_fr;
    case 2: return help_text_de;
    case 3: return help_text_es;
    case 4: return help_text_it;
    default: return help_text_en;
    }
}

void dw3_help_test_set_text(int lang, const char *text) {
    if (lang >= 0 && lang < 5) {
        g_test_text[lang] = text;
    }
}

/* --- state ----------------------------------------------------------------- */

static int g_open;
static Dw3HelpConfig g_cfg;
static Dw3PagesText g_en, g_lang;
static int g_have_lang;
static Dw3PagesBook g_book;
static Dw3PagesLayout g_layout;
static Dw3PagesScroll g_scroll;
static Dw3HelpView g_view;
static int g_screen;
static int g_cursor, g_lscroll;
static int g_page;
static double g_t_open, g_t_page;
static int g_pages_opened;
static int g_end_logged;
static unsigned g_open_serial;
static int g_atlas_logged;
static char g_unv_tag[64];
static char g_draft_tag[32];
static int g_draft;
static unsigned g_generation;

static const char *region_name(void) {
    return g_cfg.region == DW3_PAGES_EU ? "EU" : "US";
}

static const char *platform_name(void) {
    return g_cfg.platform == DW3_PAGES_SWITCH ? "switch" : "pc";
}

/* --- tokens ---------------------------------------------------------------- */

typedef struct BtnName {
    const char *name;
    const char *ps;
    const char *nx;
    const char *pc;
} BtnName;

static const BtnName k_btn[] = {
    {"cross", "Cross", "[B]", "[X]"},
    {"circle", "Circle", "[A]", "[D]"},
    {"triangle", "Triangle", "[X]", "[S]"},
    {"square", "Square", "[Y]", "[Z]"},
    {"start", "Start", "[+]", "[Enter]"},
    {"select", "Select", "[-]", "[Backspace]"},
    {"l1", "L1", "[L]", "[Q]"},
    {"r1", "R1", "[R]", "[R]"},
    {"l2", "L2", "[ZL]", "[W]"},
    {"r2", "R2", "[ZR]", "[E]"},
    {"l3", "L3", "[L3]", "[1]"},
    {"r3", "R3", "[R3]", "[2]"},
    {"dpad", "D-pad", "[D-pad]", "[Arrows]"},
};

static int token_fn(void *ud, const char *kind, const char *name, char *out,
                    int outsz) {
    unsigned i;
    const char *s = NULL;

    (void)ud;
    if (strcmp(kind, "btn") == 0 || strcmp(kind, "ps") == 0) {
        for (i = 0; i < sizeof(k_btn) / sizeof(k_btn[0]); i++) {
            if (strcmp(k_btn[i].name, name) == 0) {
                s = kind[0] == 'p'
                        ? k_btn[i].ps
                        : (g_cfg.platform == DW3_PAGES_SWITCH ? k_btn[i].nx
                                                              : k_btn[i].pc);
                break;
            }
        }
    } else if (strcmp(kind, "chord") == 0 && strcmp(name, "restart") == 0) {
        s = g_cfg.platform == DW3_PAGES_SWITCH
                ? "[L] + [R] + [+] + [-]"
                : "[Q] + [R] + [Enter] + [Backspace]";
    } else if (strcmp(kind, "path") == 0) {
        if (strcmp(name, "root") == 0) {
            s = g_cfg.root;
        } else if (strcmp(name, "memcard") == 0) {
            s = g_cfg.memcard;
        }
        if (s != NULL && s[0] == '\0') {
            s = NULL;
        }
    } else if (strcmp(kind, "build") == 0) {
        if (strcmp(name, "version") == 0) {
            s = g_cfg.version != NULL && g_cfg.version[0] != '\0' ? g_cfg.version
                                                                  : "dev";
        } else if (strcmp(name, "region") == 0) {
            s = region_name();
        }
    }
    if (s == NULL || (int)strlen(s) >= outsz) {
        return -1;
    }
    strcpy(out, s);
    return 0;
}

static int measure_fn(void *ud, int face, const char *s, int len) {
    (void)ud;
    return dw3_atlas_measure(face, s, len);
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
    o->resolver.fn = token_fn;
    o->unv_tag = g_unv_tag;
}

/* --- view ------------------------------------------------------------------ */

static const char *ui(const char *name, const char *dflt) {
    const char *s = dw3_pages_book_str(&g_book, name);

    return s != NULL ? s : dflt;
}

/* A hint: the token's name for a button, a space, the word. */
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
        strncpy(name, colon + 1, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        if (token_fn(NULL, kind, name, tok, (int)sizeof(tok)) != 0) {
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

static void view_list(void) {
    int i;
    const Dw3PagesPage *pg;

    g_view.screen = DW3_HELP_SCREEN_LIST;
    g_view.header[0] = '\0';
    g_view.npages = g_book.npages;
    g_view.cursor = g_cursor;
    g_lscroll = dw3_optlist_scroll_for(g_lscroll, g_cursor, g_book.npages);
    g_view.scroll = g_lscroll;
    for (i = 0; i < g_book.npages; i++) {
        g_view.titles[i] = g_book.pages[i].title;
    }
    pg = &g_book.pages[g_cursor];
    snprintf(g_view.strip, sizeof(g_view.strip), "%s",
             pg->blurb != NULL ? pg->blurb : "");
    g_view.nhints = 2;
    set_hint(0, "", ui("open", "Open"), 1);
    set_hint(1, "", ui("back", "Back"), 2);
    g_view.lines = NULL;
    g_view.nlines = g_view.top = g_view.panel = 0;
    g_view.arrow_up = g_view.arrow_down = 0;
    view_common();
}

static void view_page(void) {
    const Dw3PagesPage *pg = &g_book.pages[g_page];

    g_view.screen = DW3_HELP_SCREEN_PAGE;
    snprintf(g_view.header, sizeof(g_view.header), "%s%s%s", pg->title,
             g_draft ? " " : "", g_draft ? g_draft_tag : "");
    snprintf(g_view.strip, sizeof(g_view.strip), "%s %d / %d", ui("page", "Page"),
             g_page + 1, g_book.npages);
    g_view.nhints = 3;
    if (dw3_pages_scroll_needed(&g_scroll)) {
        set_hint(0, "btn:dpad", ui("scroll", "Scroll"), 0);
    } else {
        g_view.hint[0][0] = '\0';
        g_view.hint_cap[0] = 0;
    }
    {
        char l[48];
        char r[48];
        char both[110];

        token_fn(NULL, "btn", "l1", l, (int)sizeof(l));
        token_fn(NULL, "btn", "r1", r, (int)sizeof(r));
        snprintf(both, sizeof(both), "%s%s %s", l, r, ui("pages", "Pages"));
        strncpy(g_view.hint[1], both, sizeof(g_view.hint[1]) - 1);
        g_view.hint[1][sizeof(g_view.hint[1]) - 1] = '\0';
        g_view.hint_cap[1] = 0;
    }
    set_hint(2, "", ui("back", "Back"), 2);
    g_view.lines = g_layout.lines;
    g_view.nlines = g_layout.n;
    g_view.top = g_scroll.top;
    g_view.panel = g_scroll.panel;
    g_view.arrow_up = g_scroll.top > 0;
    g_view.arrow_down = !dw3_pages_scroll_at_end(&g_scroll);
    view_common();
}

/* --- pages ----------------------------------------------------------------- */

static void page_close_log(void) {
    int line = g_scroll.top + g_scroll.panel;

    if (line > g_scroll.nlines) {
        line = g_scroll.nlines;
    }
    hlog("dw3-help: page %s closed after %.0f ms (read to line %d of %d)",
         g_book.pages[g_page].id, now_ms() - g_t_page, line, g_scroll.nlines);
}

static void end_check(void) {
    if (!g_end_logged && dw3_pages_scroll_at_end(&g_scroll)) {
        int line = g_scroll.top + g_scroll.panel;

        g_end_logged = 1;
        if (line > g_scroll.nlines) {
            line = g_scroll.nlines;
        }
        hlog("dw3-help: page %s end reached (line %d of %d)",
             g_book.pages[g_page].id, line, g_scroll.nlines);
    }
}

static int page_open(int page, const char *how) {
    Dw3PagesLayoutOpts o;
    double t0 = now_ms();
    int n;
    double us;

    dw3_pages_reset_token_log();
    layout_opts(&o);
    n = dw3_pages_layout_page(&g_layout, &g_book, page, &o);
    if (n < 0) {
        hlog("dw3-help: page %s cannot be laid out (text too long)",
             g_book.pages[page].id);
        return -1;
    }
    us = (now_ms() - t0) * 1000.0;
    g_page = page;
    dw3_pages_scroll_init(&g_scroll, n, DW3_HELP_PANEL_LINES);
    g_screen = DW3_HELP_SCREEN_PAGE;
    g_end_logged = 0;
    g_pages_opened++;
    g_t_page = now_ms();
    hlog("dw3-help: layout %s %d lines in %.0f us", g_book.pages[page].id, n, us);
    hlog("dw3-help: page %s (from %s, %d lines, panel %d lines, unverified "
         "blocks %d)", g_book.pages[page].id, how, n, DW3_HELP_PANEL_LINES,
         g_book.pages[page].unverified);
    if (g_layout.unresolved > 0) {
        hlog("dw3-help: page %s has %d unresolved token(s)",
             g_book.pages[page].id, g_layout.unresolved);
    }
    end_check();
    view_page();
    return 0;
}

/* --- open / close ----------------------------------------------------------- */

int dw3_help_page_count(void) {
    return g_open ? g_book.npages : 0;
}

int dw3_help_is_open(void) {
    return g_open;
}

const Dw3HelpView *dw3_help_view(void) {
    return &g_view;
}

static void atlas_report(void) {
    double t0;
    double us;
    int f;

    if (g_atlas_logged) {
        return;
    }
    g_atlas_logged = 1;
    t0 = now_ms();
    for (f = 0; f < DW3_ATLAS_FACES; f++) {
        (void)dw3_atlas_measure(f, "A", 1);
    }
    us = (now_ms() - t0) * 1000.0;
    hlog("dw3-pages: atlas %d glyphs in %d faces, %u bytes of coverage in the "
         "image, index built in %.0f us", dw3_atlas_glyph_count(),
         DW3_ATLAS_FACES, dw3_atlas_bytes(), us);
}

int dw3_help_open(const Dw3HelpConfig *cfg) {
    char err[160];
    const char *src;
    int env;
    int lang;
    int n;

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
        hlog("dw3-help: text en cannot be loaded (%s)", err);
        return -1;
    }
    hlog("dw3-pages: text help en loaded (%d keys, %d qualified, %d filtered "
         "out for %s/%s, %u bytes)", g_en.keys_total, g_en.qualified,
         g_en.filtered_out, region_name(), platform_name(), (unsigned)g_en.bytes);
    g_have_lang = 0;
    if (lang != 0) {
        src = embedded_text(lang);
        if (dw3_pages_parse(&g_lang, src, strlen(src), env, err, sizeof(err)) !=
            0) {
            hlog("dw3-help: text %s cannot be loaded (%s), English shown",
                 k_lang_codes[lang], err);
        } else if (g_lang.n == 0) {
            hlog("dw3-pages: text fallback help * (%s): the language file has "
                 "no keys yet (stub), English shown", k_lang_codes[lang]);
            dw3_pages_text_free(&g_lang);
        } else {
            g_have_lang = 1;
            hlog("dw3-pages: text help %s loaded (%d keys, %d qualified, %d "
                 "filtered out, %u bytes)", k_lang_codes[lang], g_lang.keys_total,
                 g_lang.qualified, g_lang.filtered_out, (unsigned)g_lang.bytes);
        }
    }
    n = dw3_pages_book_build(&g_book, &g_en, g_have_lang ? &g_lang : NULL,
                             k_lang_codes[lang]);
    if (n <= 0) {
        hlog("dw3-help: no pages (book build returned %d)", n);
        dw3_pages_text_free(&g_en);
        if (g_have_lang) {
            dw3_pages_text_free(&g_lang);
        }
        return -1;
    }
    g_draft = strstr(g_en.status, "draft") != NULL;
    snprintf(g_unv_tag, sizeof(g_unv_tag), "%s", ui("unverified", "(unverified)"));
    snprintf(g_draft_tag, sizeof(g_draft_tag), "%s", ui("draft", "(draft)"));
    dw3_pages_layout_init(&g_layout);
    g_open = 1;
    g_screen = DW3_HELP_SCREEN_LIST;
    g_cursor = g_lscroll = 0;
    g_pages_opened = 0;
    g_open_serial++;
    memset(&g_view, 0, sizeof(g_view));
    g_view.open_serial = g_open_serial;
    g_view.open_ms = g_t_open;
    hlog("dw3-help: open (from %s, region %s, platform %s, language %s, %d "
         "pages)", g_cfg.from == DW3_HELP_FROM_BATTLE ? "battle"
                   : g_cfg.from == DW3_HELP_FROM_FIELD ? "field" : "title",
         region_name(), platform_name(), k_lang_codes[lang], g_book.npages);
    hlog("dw3-help: text %s status \"%s\", %d unverified block(s), %d key(s) "
         "from English", k_lang_codes[lang], g_en.status, g_book.unverified,
         g_book.fallbacks);
    view_list();
    return 0;
}

void dw3_help_close(void) {
    if (!g_open) {
        return;
    }
    if (g_screen == DW3_HELP_SCREEN_PAGE) {
        page_close_log();
    }
    hlog("dw3-help: closed after %.0f ms (pages opened %d)",
         now_ms() - g_t_open, g_pages_opened);
    g_open = 0;
    dw3_pages_layout_free(&g_layout);
    dw3_pages_text_free(&g_en);
    if (g_have_lang) {
        dw3_pages_text_free(&g_lang);
        g_have_lang = 0;
    }
}

/* --- tick ------------------------------------------------------------------ */

int dw3_help_tick(unsigned keys) {
    int snd = DW3_HELP_SND_NONE;

    if (!g_open) {
        return snd;
    }
    if (g_screen == DW3_HELP_SCREEN_LIST) {
        if (keys & DW3_HELP_KEY_BACK) {
            dw3_help_close();
            return DW3_HELP_SND_CANCEL;
        }
        if ((keys & DW3_HELP_KEY_UP) && g_cursor > 0) {
            g_cursor--;
            snd = DW3_HELP_SND_CURSOR;
        }
        if ((keys & DW3_HELP_KEY_DOWN) && g_cursor < g_book.npages - 1) {
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
    if (keys & DW3_HELP_KEY_BACK) {
        page_close_log();
        g_screen = DW3_HELP_SCREEN_LIST;
        g_cursor = g_page;
        view_list();
        return DW3_HELP_SND_CANCEL;
    }
    if ((keys & DW3_HELP_KEY_PREV) && g_page > 0) {
        page_close_log();
        if (page_open(g_page - 1, "prev") == 0) {
            return DW3_HELP_SND_SELECT;
        }
        return snd;
    }
    if ((keys & DW3_HELP_KEY_NEXT) && g_page < g_book.npages - 1) {
        page_close_log();
        if (page_open(g_page + 1, "next") == 0) {
            return DW3_HELP_SND_SELECT;
        }
        return snd;
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
            end_check();
            view_page();
        }
    }
    return snd;
}
