/* P6.30 page engine host test: parser, qualifier filter, tokens, wrap and
 * layout with a fixed-width metric, scrolling. No renderer. */
#include "port_pages.h"

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

static char g_logs[64][200];
static int g_nlogs;
static void on_log(const char *line) {
    if (g_nlogs < 64) {
        strncpy(g_logs[g_nlogs], line, 199);
        g_logs[g_nlogs][199] = 0;
        g_nlogs++;
    }
}
static int log_has(const char *needle) {
    int i;
    for (i = 0; i < g_nlogs; i++) {
        if (strstr(g_logs[i], needle) != NULL) {
            return 1;
        }
    }
    return 0;
}

/* Every character is 10 px wide (bold 12): words and lines are easy to count. */
static int measure(void *ud, int face, const char *s, int len) {
    int n = 0;
    int i;
    (void)ud;
    for (i = 0; i < len; i++) {
        if (((unsigned char)s[i] & 0xC0) != 0x80) {
            n++;
        }
    }
    return n * (face == DW3_PAGES_FACE_BOLD ? 12 : 10);
}

static int token(void *ud, const char *kind, const char *name, char *out,
                 int outsz) {
    (void)ud;
    if (strcmp(kind, "btn") == 0 && strcmp(name, "cross") == 0) {
        strncpy(out, "[B]", (size_t)outsz);
        return 0;
    }
    if (strcmp(kind, "path") == 0 && strcmp(name, "root") == 0) {
        strncpy(out, "sdmc:/switch/dw3/", (size_t)outsz);
        return 0;
    }
    return -1;
}

static const char *SRC =
    "# comment\n"
    "@format = 1\n"
    "@book = t\n"
    "@lang = en\n"
    "@status = draft, x\n"
    "\n"
    "t.ui.str.back = Back\n"
    "t.a.title = Alpha\n"
    "t.a.blurb = The first page.\n"
    "t.a.h.1 = Head\n"
    "t.a.p.1 = one two three four five six seven eight nine ten\n"
    "t.a.li.1 = Press {btn:cross} now.\n"
    "t.a.li.2 @us = US only line\n"
    "t.a.li.2 @eu = EU only line\n"
    "t.a.li.3 @pc = PC line\n"
    "t.a.li.4 @switch !unv = Switch line\n"
    "t.a.row.1 = Left | Right\n"
    "t.a.note.1 = braces {{x}} and a\\nbreak\n"
    "t.b.title = Beta\n"
    "t.b.blurb = Second.\n"
    "t.b.q.1 = Why?\n"
    "t.b.a.1 = Because.\n"
    "t.c.title = Gamma\n"
    "t.c.blurb = Third.\n"
    "t.c.p.1 @eu = Only on EU.\n";

static void test_parse_and_filter(void) {
    Dw3PagesText t;
    char err[160];
    const Dw3PagesEntry *e;

    CHECK(dw3_pages_parse(&t, SRC, strlen(SRC),
                          DW3_PAGES_US | DW3_PAGES_PC, err, sizeof(err)) == 0);
    CHECK(t.format == 1 && strcmp(t.lang, "en") == 0 && strcmp(t.book, "t") == 0);
    CHECK(strstr(t.status, "draft") != NULL);
    CHECK((e = dw3_pages_find(&t, "t.a.li.2")) != NULL &&
          strcmp(e->value, "US only line") == 0);
    CHECK(dw3_pages_find(&t, "t.a.li.3") != NULL);   /* @pc */
    CHECK(dw3_pages_find(&t, "t.a.li.4") == NULL);   /* @switch */
    CHECK(dw3_pages_find(&t, "t.c.p.1") == NULL);    /* @eu */
    CHECK(t.qualified == 5 && t.filtered_out == 3);
    CHECK((e = dw3_pages_find(&t, "t.a.p.1")) != NULL && e->kind == DW3_PAGES_P &&
          e->page_len == 1 && e->page[0] == 'a');
    dw3_pages_text_free(&t);

    CHECK(dw3_pages_parse(&t, SRC, strlen(SRC),
                          DW3_PAGES_EU | DW3_PAGES_SWITCH, err, sizeof(err)) == 0);
    CHECK((e = dw3_pages_find(&t, "t.a.li.2")) != NULL &&
          strcmp(e->value, "EU only line") == 0);
    CHECK((e = dw3_pages_find(&t, "t.a.li.4")) != NULL && e->unv == 1);
    CHECK(dw3_pages_find(&t, "t.a.li.3") == NULL);
    CHECK(dw3_pages_find(&t, "t.c.p.1") != NULL);
    dw3_pages_text_free(&t);
}

static int bad(const char *src, const char *want) {
    Dw3PagesText t;
    char err[160];
    int rc = dw3_pages_parse(&t, src, strlen(src), DW3_PAGES_US | DW3_PAGES_PC,
                             err, sizeof(err));
    if (rc == 0) {
        dw3_pages_text_free(&t);
        return 0;
    }
    return strstr(err, want) != NULL;
}

static void test_parse_errors(void) {
    CHECK(bad("a.b.p.1 @us\n", "line 1: missing '='"));
    CHECK(bad("# c\n\na.b.zz.1 = x\n", "line 3: unknown block kind"));
    CHECK(bad("a.b.p.1 @mac = x\n", "line 1: unknown qualifier"));
    CHECK(bad("a.b = x\n", "line 1: a key is"));
    CHECK(bad("a.b.p.1 = {btn:cross\n", "unclosed"));
    CHECK(bad("a.b.p.1 = a } b\n", "stray"));
    CHECK(bad("a.b.p.1 = {nocolon}\n", "must be {kind:name}"));
    CHECK(bad("a.b.p.1 = x\na.b.p.1 = y\n", "line 2: duplicate key"));
    CHECK(bad("@wat = 1\n", "unknown header"));
    /* The same key with different qualifiers is fine. */
    {
        Dw3PagesText t;
        char err[80];
        const char *ok = "a.b.p.1 @us = x\na.b.p.1 @eu = y\n";
        CHECK(dw3_pages_parse(&t, ok, strlen(ok), DW3_PAGES_US | DW3_PAGES_PC,
                              err, sizeof(err)) == 0);
        dw3_pages_text_free(&t);
    }
    /* CRLF and a BOM. */
    {
        Dw3PagesText t;
        char err[80];
        const char *crlf = "\xEF\xBB\xBF@lang = en\r\na.b.p.1 = x\r\n";
        CHECK(dw3_pages_parse(&t, crlf, strlen(crlf), DW3_PAGES_US, err,
                              sizeof(err)) == 0);
        CHECK(strcmp(t.lang, "en") == 0 &&
              strcmp(dw3_pages_find(&t, "a.b.p.1")->value, "x") == 0);
        dw3_pages_text_free(&t);
    }
}

static void test_tokens(void) {
    char out[128];
    int unres = 0;
    Dw3PagesResolver r;

    r.fn = token;
    r.ud = NULL;
    g_nlogs = 0;
    dw3_pages_reset_token_log();
    CHECK(dw3_pages_resolve("Press {btn:cross} or {btn:cross}.", out,
                            sizeof(out), &r, &unres) > 0);
    CHECK(strcmp(out, "Press [B] or [B].") == 0 && unres == 0);
    CHECK(log_has("token {btn:cross} -> \"[B]\""));
    {
        int i;
        int n = 0;
        for (i = 0; i < g_nlogs; i++) {
            n += strstr(g_logs[i], "{btn:cross}") != NULL;
        }
        CHECK(n == 1); /* once per token */
    }
    CHECK(dw3_pages_resolve("{btn:nope} {{lit}}\\nx", out, sizeof(out), &r,
                            &unres) > 0);
    CHECK(strcmp(out, "[?btn:nope] {lit}\nx") == 0 && unres == 1);
    CHECK(log_has("token btn:nope unresolved, drawn as [?btn:nope]"));
    CHECK(dw3_pages_resolve("{path:root}us/", out, sizeof(out), &r, &unres) > 0);
    CHECK(strcmp(out, "sdmc:/switch/dw3/us/") == 0);
    /* a small buffer is an error, not an overflow */
    CHECK(dw3_pages_resolve("0123456789", out, 5, &r, &unres) < 0);
}

static void test_utf8(void) {
    unsigned cp;
    CHECK(dw3_pages_utf8("A", 1, &cp) == 1 && cp == 'A');
    CHECK(dw3_pages_utf8("\xC3\xA9", 2, &cp) == 2 && cp == 0xE9);
    CHECK(dw3_pages_utf8("\xE2\x80\xA2", 3, &cp) == 3 && cp == 0x2022);
    CHECK(dw3_pages_utf8("\xC3", 1, &cp) == 1 && cp == 0xFFFD);
}

static Dw3PagesLayoutOpts opts(int width) {
    Dw3PagesLayoutOpts o;
    memset(&o, 0, sizeof(o));
    o.width = width;
    o.bullet_indent = 20;
    o.answer_indent = 20;
    o.bar_indent = 20;
    o.col1_w = width * 6 / 10;
    o.col2_x = width * 66 / 100;
    o.col2_w = width - o.col2_x;
    o.measure = measure;
    o.resolver.fn = token;
    o.unv_tag = "(unv)";
    return o;
}

static int line_is(const Dw3PagesLayout *l, int i, const char *text) {
    return i < l->n && (int)strlen(text) == l->lines[i].len[0] &&
           memcmp(l->lines[i].s[0], text, strlen(text)) == 0;
}

static void test_wrap(void) {
    Dw3PagesLayout l;
    int n;

    dw3_pages_layout_init(&l);
    l.lines = (Dw3PagesLine *)calloc(64, sizeof(Dw3PagesLine));
    l.cap = 64;
    l.ok = 1;
    /* width 100 = 10 chars per line */
    n = dw3_pages_wrap(&l, "one two three four", 0, DW3_PAGES_S_P, 0, 0, 0, 100,
                       measure, NULL);
    CHECK(n == 2 && line_is(&l, 0, "one two") && line_is(&l, 1, "three four"));
    l.n = 0;
    n = dw3_pages_wrap(&l, "one two three four five", 0, DW3_PAGES_S_P, 0, 0, 0,
                       100, measure, NULL);
    CHECK(n == 3 && line_is(&l, 1, "three four") && line_is(&l, 2, "five"));
    /* a word longer than a line is broken between characters */
    l.n = 0;
    n = dw3_pages_wrap(&l, "abcdefghijklmnop", 0, DW3_PAGES_S_P, 0, 0, 0, 100,
                       measure, NULL);
    CHECK(n == 2 && line_is(&l, 0, "abcdefghij") && line_is(&l, 1, "klmnop"));
    /* forced break and a blank line */
    l.n = 0;
    n = dw3_pages_wrap(&l, "a\nb\n\nc", 0, DW3_PAGES_S_P, 0, 0, 0, 100, measure,
                       NULL);
    CHECK(n == 4 && line_is(&l, 0, "a") && line_is(&l, 1, "b") &&
          l.lines[2].len[0] == 0 && line_is(&l, 3, "c"));
    /* a line that fits exactly stays on one line */
    l.n = 0;
    n = dw3_pages_wrap(&l, "0123456789", 0, DW3_PAGES_S_P, 0, 0, 0, 100, measure,
                       NULL);
    CHECK(n == 1);
    /* multibyte characters count once */
    l.n = 0;
    n = dw3_pages_wrap(&l, "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9", 0,
                       DW3_PAGES_S_P, 0, 0, 0, 100, measure, NULL);
    CHECK(n == 1);
    /* first/rest flags */
    l.n = 0;
    dw3_pages_wrap(&l, "one two three four", 0, DW3_PAGES_S_LI,
                   DW3_PAGES_LF_BULLET, 0, 20, 100, measure, NULL);
    CHECK(l.lines[0].flags == DW3_PAGES_LF_BULLET && l.lines[1].flags == 0 &&
          l.lines[1].x[0] == 20);
    free(l.lines);
}

static void test_book_and_layout(void) {
    Dw3PagesText en, fr;
    Dw3PagesBook b;
    Dw3PagesLayout l;
    Dw3PagesLayoutOpts o = opts(200);
    char err[160];
    int n;
    const char *frsrc = "@lang = fr\nt.a.title = Alpha FR\nt.a.blurb = Premiere.\n"
                        "t.a.p.1 = un deux\nt.b.title = Beta\nt.b.blurb = b\n"
                        "t.b.q.1 = Pourquoi ?\nt.b.a.1 = Parce que.\n"
                        "t.c.title = G\nt.c.blurb = g\n";

    CHECK(dw3_pages_parse(&en, SRC, strlen(SRC), DW3_PAGES_US | DW3_PAGES_PC,
                          err, sizeof(err)) == 0);
    /* page c has no block for US: hidden */
    g_nlogs = 0;
    CHECK(dw3_pages_book_build(&b, &en, NULL, "en") == 2);
    CHECK(strcmp(b.pages[0].id, "a") == 0 && strcmp(b.pages[1].id, "b") == 0);
    CHECK(strcmp(dw3_pages_book_str(&b, "back"), "Back") == 0);
    CHECK(dw3_pages_book_str(&b, "nothing") == NULL);
    CHECK(b.fallbacks == 0 && b.unverified == 0);

    dw3_pages_layout_init(&l);
    dw3_pages_reset_token_log();
    n = dw3_pages_layout_page(&l, &b, 0, &o);
    CHECK(n > 0 && l.ok && l.unresolved == 0);
    /* h, p wraps at 20 chars, li with the token, li 2, li 3 (PC), row, note */
    CHECK(l.lines[0].style == DW3_PAGES_S_H && l.lines[0].face[0] == DW3_PAGES_FACE_BOLD);
    CHECK(l.lines[1].style == DW3_PAGES_S_P); /* no blank after a heading */
    {
        int i;
        int bullets = 0;
        int rows = 0;
        int bars = 0;
        int found_tok = 0;
        int blank = 0;
        for (i = 0; i < l.n; i++) {
            bullets += (l.lines[i].flags & DW3_PAGES_LF_BULLET) != 0;
            bars += (l.lines[i].flags & DW3_PAGES_LF_BAR) != 0;
            rows += l.lines[i].style == DW3_PAGES_S_ROW;
            blank += l.lines[i].style == DW3_PAGES_S_BLANK;
            if (l.lines[i].len[0] >= 3 && l.lines[i].style == DW3_PAGES_S_LI &&
                memcmp(l.lines[i].s[0], "Press [B]", 9) == 0) {
                found_tok = 1;
            }
        }
        CHECK(bullets == 3);          /* li 1, 2 and 3; li 4 is @switch */
        CHECK(found_tok);
        CHECK(rows == 1);
        CHECK(bars >= 2);             /* the note wraps over its \n */
        CHECK(blank == 3);            /* p|li, li|row, row|note; li li tight */
        for (i = 0; i < l.n; i++) {
            if (l.lines[i].style == DW3_PAGES_S_ROW) {
                CHECK(l.lines[i].len[0] == 4 && l.lines[i].len[1] == 5);
                CHECK(l.lines[i].x[1] == o.col2_x);
            }
        }
    }
    /* Q/A: tight pair */
    n = dw3_pages_layout_page(&l, &b, 1, &o);
    CHECK(n == 2 && l.lines[0].style == DW3_PAGES_S_Q && l.lines[1].style == DW3_PAGES_S_A &&
          l.lines[1].x[0] == 20);

    /* !unv adds the tag */
    {
        Dw3PagesText sw;
        Dw3PagesBook b2;
        CHECK(dw3_pages_parse(&sw, SRC, strlen(SRC), DW3_PAGES_EU | DW3_PAGES_SWITCH,
                              err, sizeof(err)) == 0);
        CHECK(dw3_pages_book_build(&b2, &sw, NULL, "en") == 3);
        CHECK(b2.unverified == 1 && b2.pages[0].unverified == 1);
        n = dw3_pages_layout_page(&l, &b2, 0, &o);
        {
            int i;
            int tag = 0;
            for (i = 0; i < l.n; i++) {
                tag += l.lines[i].len[0] >= 5 &&
                       memcmp(l.lines[i].s[0] + l.lines[i].len[0] - 5, "(unv)", 5) == 0;
            }
            CHECK(tag == 1);
        }
        dw3_pages_text_free(&sw);
    }

    /* a language over English: its titles, English for the keys it lacks, with
     * one fallback log line per key */
    CHECK(dw3_pages_parse(&fr, frsrc, strlen(frsrc), DW3_PAGES_US | DW3_PAGES_PC,
                          err, sizeof(err)) == 0);
    g_nlogs = 0;
    CHECK(dw3_pages_book_build(&b, &en, &fr, "fr") == 2);
    CHECK(strcmp(b.pages[0].title, "Alpha FR") == 0 &&
          strcmp(b.pages[1].title, "Beta") == 0);
    CHECK(b.fallbacks > 0);
    CHECK(log_has("dw3-pages: text fallback t t.a.h.1 (fr)"));
    CHECK(!log_has("t.a.p.1 (fr)"));
    CHECK(b.blocks[b.pages[0].first].fallback == 1 &&
          b.blocks[b.pages[0].first + 1].fallback == 0);
    dw3_pages_layout_free(&l);
    dw3_pages_text_free(&fr);
    dw3_pages_text_free(&en);
}

static void test_book_errors(void) {
    Dw3PagesText t;
    Dw3PagesBook b;
    char err[80];
    /* page keys apart: error */
    const char *split = "t.a.title = A\nt.a.blurb = a\nt.a.p.1 = x\n"
                        "t.b.title = B\nt.b.blurb = b\nt.b.p.1 = y\nt.a.p.2 = z\n";
    /* a page with a title but no block is hidden; a block without a title too */
    const char *hid = "t.a.title = A\nt.a.blurb = a\nt.b.p.1 = y\n";
    CHECK(dw3_pages_parse(&t, split, strlen(split), DW3_PAGES_US, err,
                          sizeof(err)) == 0);
    CHECK(dw3_pages_book_build(&b, &t, NULL, "en") == -1);
    dw3_pages_text_free(&t);
    CHECK(dw3_pages_parse(&t, hid, strlen(hid), DW3_PAGES_US, err, sizeof(err)) == 0);
    CHECK(dw3_pages_book_build(&b, &t, NULL, "en") == 0);
    dw3_pages_text_free(&t);
}

static void test_scroll(void) {
    Dw3PagesScroll s;

    dw3_pages_scroll_init(&s, 30, 10);
    CHECK(dw3_pages_scroll_needed(&s) && !dw3_pages_scroll_at_end(&s));
    CHECK(dw3_pages_scroll_by(&s, -1) == 0 && s.top == 0);      /* clamp at top */
    CHECK(dw3_pages_scroll_by(&s, 1) == 1 && s.top == 1);
    CHECK(dw3_pages_scroll_page(&s, 1) == 1 && s.top == 10);    /* panel - 1 */
    CHECK(dw3_pages_scroll_page(&s, 1) == 1 && s.top == 19);
    CHECK(dw3_pages_scroll_page(&s, 1) == 1 && s.top == 20);    /* clamped to max */
    CHECK(dw3_pages_scroll_at_end(&s));
    CHECK(dw3_pages_scroll_by(&s, 1) == 0 && s.top == 20);      /* no move, no sound */
    CHECK(dw3_pages_scroll_page(&s, -1) == 1 && s.top == 11);
    dw3_pages_scroll_init(&s, 7, 10);                           /* fits */
    CHECK(!dw3_pages_scroll_needed(&s) && dw3_pages_scroll_at_end(&s));
    CHECK(dw3_pages_scroll_by(&s, 1) == 0 && dw3_pages_scroll_page(&s, 1) == 0);
    dw3_pages_scroll_init(&s, 10, 10);                          /* exactly */
    CHECK(!dw3_pages_scroll_needed(&s));
}

int main(void) {
    dw3_pages_set_log(on_log);
    test_parse_and_filter();
    test_parse_errors();
    test_tokens();
    test_utf8();
    test_wrap();
    test_book_and_layout();
    test_book_errors();
    test_scroll();
    if (g_fail) {
        printf("FAILED: %d check(s)\n", g_fail);
        return 1;
    }
    printf("OK: page engine\n");
    return 0;
}
