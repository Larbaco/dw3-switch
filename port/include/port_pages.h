/* P6.30 page engine: the text file parser, the qualifier filter, the token
 * resolver, the wrapper and the scroll state shared by the technical help and
 * (P6.35) the walkthrough. Pure C: no renderer, no pad, no globals except the
 * log hook. A width callback tells it how wide a string is, so the host tests
 * run it with a fixed-width metric. docs/P6.30_HELP.md sections 2 to 6.
 *
 * Text format (one file per language): `key [qualifier ...] = value`, '#'
 * comments, `@name = value` header lines. Qualifiers: @us @eu @switch @pc
 * (the entry exists only on that region / host) and !unv (not yet checked on
 * an OLED; the block is drawn with an "(unverified)" tag). Values: `\n` is a
 * forced line break, `{{` and `}}` are literal braces, `{kind:name}` is a token
 * (btn, ps, chord, path, build) resolved when a page is laid out. */
#ifndef DW3_PORT_PAGES_H
#define DW3_PORT_PAGES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Build environment bits of the qualifier filter. */
enum {
    DW3_PAGES_US = 1,
    DW3_PAGES_EU = 2,
    DW3_PAGES_SWITCH = 4,
    DW3_PAGES_PC = 8
};

/* Block kinds (the third part of a key). KIND_STR is a lookup-only string
 * (the screen's own words), never a block. */
enum {
    DW3_PAGES_TITLE = 0,
    DW3_PAGES_BLURB,
    DW3_PAGES_H,
    DW3_PAGES_P,
    DW3_PAGES_LI,
    DW3_PAGES_ROW,
    DW3_PAGES_NOTE,
    DW3_PAGES_WARN,
    DW3_PAGES_Q,
    DW3_PAGES_A,
    DW3_PAGES_STR,
    DW3_PAGES_KINDS
};

/* Log hook: one line without a newline. NULL (the default) logs nothing. */
typedef void (*Dw3PagesLogFn)(const char *line);
void dw3_pages_set_log(Dw3PagesLogFn fn);

/* --- text files ----------------------------------------------------------- */

typedef struct Dw3PagesEntry {
    const char *key;     /* NUL-terminated, inside the owned buffer */
    const char *value;   /* escapes and tokens still in */
    const char *page;    /* second part of the key (not terminated) */
    int page_len;
    int kind;            /* DW3_PAGES_* */
    int unv;             /* !unv */
} Dw3PagesEntry;

typedef struct Dw3PagesText {
    char *buf;
    Dw3PagesEntry *e;
    int n, cap;
    char lang[16];       /* @lang */
    char book[16];       /* @book */
    char status[96];     /* @status */
    int format;          /* @format */
    /* load statistics (the "text loaded" log line) */
    int keys_total;      /* entries in the file */
    int qualified;       /* of them with a qualifier */
    int filtered_out;    /* removed for this region / host */
    size_t bytes;
} Dw3PagesText;

/* Parse `src` (UTF-8, not necessarily terminated). env: DW3_PAGES_* bits, one
 * region and one host. Returns 0, or -1 with "line N: message" in err. The
 * text is not changed. */
int dw3_pages_parse(Dw3PagesText *t, const char *src, size_t len, int env,
                    char *err, size_t errsz);
void dw3_pages_text_free(Dw3PagesText *t);
const Dw3PagesEntry *dw3_pages_find(const Dw3PagesText *t, const char *key);
/* Kind of a key's third part ("title", "p", ...), -1 when unknown. */
int dw3_pages_kind_of(const char *name, int len);
const char *dw3_pages_kind_name(int kind);

/* --- tokens ----------------------------------------------------------------- */

/* Resolve {kind:name}: write the text into out and return 0, or return -1
 * (the engine draws [?name] and logs once per name). */
typedef int (*Dw3PagesTokenFn)(void *ud, const char *kind, const char *name,
                               char *out, int outsz);
typedef struct Dw3PagesResolver {
    Dw3PagesTokenFn fn;
    void *ud;
} Dw3PagesResolver;

/* Forget which tokens were logged (call when a book is opened). */
void dw3_pages_reset_token_log(void);
/* Expand escapes and tokens of `in` into out. Returns the length, or -1 when
 * out is too small. `unresolved` counts the tokens that could not be resolved. */
int dw3_pages_resolve(const char *in, char *out, int outsz,
                      const Dw3PagesResolver *r, int *unresolved);

/* --- a book: the English order, one language on top --------------------------- */

enum { DW3_PAGES_MAX_PAGES = 32, DW3_PAGES_MAX_BLOCKS = 512 };

typedef struct Dw3PagesBlock {
    int kind;
    const char *key;
    const char *value;   /* the language's, else English's */
    int unv;
    int fallback;        /* the value came from English */
} Dw3PagesBlock;

typedef struct Dw3PagesPage {
    char id[24];
    const char *title;
    const char *blurb;
    int first, nblocks;  /* range in Dw3PagesBook.blocks */
    int unverified;      /* blocks with !unv */
} Dw3PagesPage;

typedef struct Dw3PagesBook {
    const Dw3PagesText *en;
    const Dw3PagesText *lang;     /* NULL or the same as en: English */
    char lang_code[16];
    Dw3PagesPage pages[DW3_PAGES_MAX_PAGES];
    int npages;
    Dw3PagesBlock blocks[DW3_PAGES_MAX_BLOCKS];
    int nblocks;
    int fallbacks;                /* keys taken from English */
    int unverified;               /* blocks with !unv, all pages */
} Dw3PagesBook;

/* Build the page list from the English order. A key the language lacks falls
 * back to English with one log line per key. Pages without a title or without
 * a block left after the filter are hidden. Returns the page count, -1 on
 * overflow. */
int dw3_pages_book_build(Dw3PagesBook *b, const Dw3PagesText *en,
                         const Dw3PagesText *lang, const char *lang_code);
/* A screen string (`<book>.ui.str.<name>`), the language's else English's;
 * NULL when neither has it. */
const char *dw3_pages_book_str(const Dw3PagesBook *b, const char *name);

/* --- layout ------------------------------------------------------------------ */

enum {
    DW3_PAGES_FACE_REGULAR = 0,
    DW3_PAGES_FACE_BOLD = 1
};
/* Line styles (colour / role), set by the block kind. */
enum {
    DW3_PAGES_S_BLANK = 0,
    DW3_PAGES_S_P,
    DW3_PAGES_S_H,
    DW3_PAGES_S_LI,
    DW3_PAGES_S_ROW,
    DW3_PAGES_S_NOTE,
    DW3_PAGES_S_WARN,
    DW3_PAGES_S_Q,
    DW3_PAGES_S_A
};
enum {
    DW3_PAGES_LF_BULLET = 1,    /* draw a bullet at the left of the first line of an item */
    DW3_PAGES_LF_BAR = 2        /* the bar of a note / warn at the left */
};

typedef struct Dw3PagesLine {
    const char *s[2];   /* run text (UTF-8, not terminated); a row has two */
    short len[2];
    short x[2];         /* pixels from the body's left edge */
    unsigned char face[2];
    unsigned char style;
    unsigned char flags;
} Dw3PagesLine;

typedef int (*Dw3PagesMeasureFn)(void *ud, int face, const char *s, int len);

typedef struct Dw3PagesLayoutOpts {
    int width;            /* body width in pixels */
    int bullet_indent;    /* li text indent */
    int answer_indent;    /* a indent */
    int bar_indent;       /* note / warn text indent */
    int col2_x;           /* row: where the right column starts */
    int col1_w;           /* row: width of the left column */
    int col2_w;           /* row: width of the right column */
    Dw3PagesMeasureFn measure;
    void *measure_ud;
    Dw3PagesResolver resolver;
    const char *unv_tag;  /* appended to a !unv block, e.g. "(unverified)" */
} Dw3PagesLayoutOpts;

typedef struct Dw3PagesLayout {
    Dw3PagesLine *lines;
    int n, cap;
    char *arena;          /* resolved text the lines point into */
    size_t arena_len, arena_cap;
    int unresolved;       /* tokens drawn as [?name] */
    int ok;               /* 0 when the arena or the line table overflowed */
} Dw3PagesLayout;

void dw3_pages_layout_init(Dw3PagesLayout *l);
void dw3_pages_layout_free(Dw3PagesLayout *l);
/* Lay page `page` of the book out. Returns the line count (the layout is
 * replaced), -1 on a bad page or an overflow. */
int dw3_pages_layout_page(Dw3PagesLayout *l, const Dw3PagesBook *b, int page,
                          const Dw3PagesLayoutOpts *o);
/* Wrap one string into lines of at most `width` pixels (UTF-8; words are
 * broken only when one is wider than a line). Appends to l with the given
 * x, face and style (first_flags on the first line, rest_flags after); returns the lines added. Exposed for the tests. */
int dw3_pages_wrap(Dw3PagesLayout *l, const char *text, int face, int style,
                   int first_flags, int rest_flags, int x, int width,
                   Dw3PagesMeasureFn measure, void *ud);

/* --- scrolling ---------------------------------------------------------------- */

typedef struct Dw3PagesScroll {
    int nlines;
    int panel;   /* lines on screen */
    int top;     /* first visible line */
} Dw3PagesScroll;

void dw3_pages_scroll_init(Dw3PagesScroll *s, int nlines, int panel);
/* Up (-1) / Down (+1) by lines; clamps. Returns 1 when the view moved. */
int dw3_pages_scroll_by(Dw3PagesScroll *s, int delta);
/* A screenful (panel - 1 lines, one line of context) up (-1) or down (+1). */
int dw3_pages_scroll_page(Dw3PagesScroll *s, int dir);
int dw3_pages_scroll_max(const Dw3PagesScroll *s);
/* 1 when the last line is on screen. */
int dw3_pages_scroll_at_end(const Dw3PagesScroll *s);
/* 1 when the page is longer than the panel. */
int dw3_pages_scroll_needed(const Dw3PagesScroll *s);

/* --- UTF-8 -------------------------------------------------------------------- */

/* Decode one code point at s (n bytes available); returns the bytes used
 * (1 on an invalid byte, which yields U+FFFD). */
int dw3_pages_utf8(const char *s, int n, unsigned *cp);

#ifdef __cplusplus
}
#endif

#endif
