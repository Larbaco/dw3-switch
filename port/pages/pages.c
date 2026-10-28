/* P6.30 page engine (see port_pages.h). Pure C, no host calls. */
#include "port_pages.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- log ------------------------------------------------------------------ */

static Dw3PagesLogFn g_log;

void dw3_pages_set_log(Dw3PagesLogFn fn) {
    g_log = fn;
}

static void plog(const char *fmt, ...) {
    char line[320];
    va_list ap;

    if (g_log == NULL) {
        return;
    }
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    g_log(line);
}

/* --- kinds ---------------------------------------------------------------- */

static const char *const k_kind_names[DW3_PAGES_KINDS] = {
    "title", "blurb", "h", "p", "li", "row", "note", "warn", "q", "a", "str"};

int dw3_pages_kind_of(const char *name, int len) {
    int k;

    for (k = 0; k < DW3_PAGES_KINDS; k++) {
        if ((int)strlen(k_kind_names[k]) == len &&
            memcmp(k_kind_names[k], name, (size_t)len) == 0) {
            return k;
        }
    }
    return -1;
}

const char *dw3_pages_kind_name(int kind) {
    return kind >= 0 && kind < DW3_PAGES_KINDS ? k_kind_names[kind] : "?";
}

/* --- UTF-8 ---------------------------------------------------------------- */

int dw3_pages_utf8(const char *s, int n, unsigned *cp) {
    const unsigned char *u = (const unsigned char *)s;

    if (n <= 0) {
        *cp = 0;
        return 0;
    }
    if (u[0] < 0x80) {
        *cp = u[0];
        return 1;
    }
    if (u[0] >= 0xC2 && u[0] < 0xE0 && n >= 2 && (u[1] & 0xC0) == 0x80) {
        *cp = ((unsigned)(u[0] & 0x1F) << 6) | (u[1] & 0x3F);
        return 2;
    }
    if (u[0] >= 0xE0 && u[0] < 0xF0 && n >= 3 && (u[1] & 0xC0) == 0x80 &&
        (u[2] & 0xC0) == 0x80) {
        *cp = ((unsigned)(u[0] & 0x0F) << 12) | ((unsigned)(u[1] & 0x3F) << 6) |
              (u[2] & 0x3F);
        return 3;
    }
    if (u[0] >= 0xF0 && u[0] < 0xF5 && n >= 4 && (u[1] & 0xC0) == 0x80 &&
        (u[2] & 0xC0) == 0x80 && (u[3] & 0xC0) == 0x80) {
        *cp = ((unsigned)(u[0] & 0x07) << 18) | ((unsigned)(u[1] & 0x3F) << 12) |
              ((unsigned)(u[2] & 0x3F) << 6) | (u[3] & 0x3F);
        return 4;
    }
    *cp = 0xFFFD;
    return 1;
}

/* --- parser --------------------------------------------------------------- */

static void set_err(char *err, size_t errsz, int line, const char *msg) {
    if (err != NULL && errsz > 0) {
        snprintf(err, errsz, "line %d: %s", line, msg);
    }
}

/* Check the braces of a value: {{ }} literal, {kind:name} a token. */
static const char *check_value(const char *v) {
    const char *p = v;

    while (*p != '\0') {
        if (p[0] == '{' && p[1] == '{') {
            p += 2;
        } else if (p[0] == '}' && p[1] == '}') {
            p += 2;
        } else if (*p == '{') {
            const char *colon = NULL;
            const char *q = p + 1;

            while (*q != '\0' && *q != '}' && *q != '{') {
                if (*q == ':' && colon == NULL) {
                    colon = q;
                }
                q++;
            }
            if (*q != '}') {
                return "unclosed '{' (write {{ for a literal brace)";
            }
            if (colon == NULL || colon == p + 1 || colon + 1 == q) {
                return "a token must be {kind:name}";
            }
            p = q + 1;
        } else if (*p == '}') {
            return "stray '}' (write }} for a literal brace)";
        } else {
            p++;
        }
    }
    return NULL;
}

static int add_entry(Dw3PagesText *t) {
    if (t->n == t->cap) {
        int cap = t->cap != 0 ? t->cap * 2 : 128;
        Dw3PagesEntry *ne =
            (Dw3PagesEntry *)realloc(t->e, (size_t)cap * sizeof(*ne));

        if (ne == NULL) {
            return -1;
        }
        t->e = ne;
        t->cap = cap;
    }
    return t->n++;
}

static void copy_str(char *dst, size_t dstsz, const char *src) {
    size_t n = strlen(src);

    if (n >= dstsz) {
        n = dstsz - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

int dw3_pages_parse(Dw3PagesText *t, const char *src, size_t len, int env,
                    char *err, size_t errsz) {
    char *p;
    char *end;
    int lineno = 0;

    memset(t, 0, sizeof(*t));
    t->buf = (char *)malloc(len + 1);
    if (t->buf == NULL) {
        set_err(err, errsz, 0, "out of memory");
        return -1;
    }
    memcpy(t->buf, src, len);
    t->buf[len] = '\0';
    t->bytes = len;
    p = t->buf;
    end = t->buf + len;
    while (p < end) {
        char *nl = (char *)memchr(p, '\n', (size_t)(end - p));
        char *line = p;
        char *key;
        char *q;
        char *val;
        int quals = 0; /* bits: 1 us, 2 eu, 4 switch, 8 pc, 16 unv */
        int unv = 0;
        int kind;
        const char *msg;

        if (nl == NULL) {
            nl = end;
        }
        p = nl + 1;
        *nl = '\0';
        lineno++;
        if (nl > line && nl[-1] == '\r') {
            nl[-1] = '\0';
        }
        while (*line == ' ' || *line == '\t') {
            line++;
        }
        if (*line == '\0' || *line == '#') {
            continue;
        }
        /* Strip a UTF-8 BOM on the first line. */
        if (lineno == 1 && (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) {
            line += 3;
            if (*line == '\0' || *line == '#') {
                continue;
            }
        }
        key = line;
        q = line;
        while (*q != '\0' && *q != ' ' && *q != '\t' && *q != '=') {
            q++;
        }
        if (q == key || (*key == '@' && q == key + 1)) {
            set_err(err, errsz, lineno, "empty key");
            goto fail;
        }
        /* qualifiers up to '=' */
        {
            char *k_end = q;

            while (*q == ' ' || *q == '\t') {
                q++;
            }
            while (*q != '=' && *q != '\0') {
                char *tok = q;
                size_t tl;

                while (*q != '\0' && *q != ' ' && *q != '\t' && *q != '=') {
                    q++;
                }
                tl = (size_t)(q - tok);
                if (tl == 3 && memcmp(tok, "@us", 3) == 0) {
                    quals |= 1;
                } else if (tl == 3 && memcmp(tok, "@eu", 3) == 0) {
                    quals |= 2;
                } else if (tl == 7 && memcmp(tok, "@switch", 7) == 0) {
                    quals |= 4;
                } else if (tl == 3 && memcmp(tok, "@pc", 3) == 0) {
                    quals |= 8;
                } else if (tl == 4 && memcmp(tok, "!unv", 4) == 0) {
                    quals |= 16;
                } else {
                    set_err(err, errsz, lineno, "unknown qualifier");
                    goto fail;
                }
                while (*q == ' ' || *q == '\t') {
                    q++;
                }
            }
            if (*q != '=') {
                set_err(err, errsz, lineno, "missing '='");
                goto fail;
            }
            val = q + 1;
            *k_end = '\0';
        }
        while (*val == ' ' || *val == '\t') {
            val++;
        }
        {
            size_t vl = strlen(val);

            while (vl > 0 && (val[vl - 1] == ' ' || val[vl - 1] == '\t')) {
                val[--vl] = '\0';
            }
        }
        msg = check_value(val);
        if (msg != NULL) {
            set_err(err, errsz, lineno, msg);
            goto fail;
        }
        if (*key == '@') {
            const char *name = key + 1;

            if (strcmp(name, "format") == 0) {
                t->format = atoi(val);
            } else if (strcmp(name, "book") == 0) {
                copy_str(t->book, sizeof(t->book), val);
            } else if (strcmp(name, "lang") == 0) {
                copy_str(t->lang, sizeof(t->lang), val);
            } else if (strcmp(name, "status") == 0) {
                copy_str(t->status, sizeof(t->status), val);
            } else {
                set_err(err, errsz, lineno, "unknown header");
                goto fail;
            }
            continue;
        }
        /* key: book.page.kind[.n] */
        {
            const char *d1 = strchr(key, '.');
            const char *d2 = d1 != NULL ? strchr(d1 + 1, '.') : NULL;
            const char *d3;
            int kl;

            if (d1 == NULL || d2 == NULL || d1 == key || d2 == d1 + 1) {
                set_err(err, errsz, lineno,
                        "a key is <book>.<page>.<kind>[.<n>]");
                goto fail;
            }
            d3 = strchr(d2 + 1, '.');
            kl = (int)((d3 != NULL ? d3 : key + strlen(key)) - (d2 + 1));
            kind = dw3_pages_kind_of(d2 + 1, kl);
            if (kind < 0) {
                set_err(err, errsz, lineno, "unknown block kind");
                goto fail;
            }
            unv = (quals & 16) != 0;
            t->keys_total++;
            if (quals != 0) {
                t->qualified++;
            }
            {
                int rmask = quals & 3;
                int pmask = quals & 12;
                int env_r = env & 3;
                int env_p = env & 12;

                if ((rmask != 0 && (rmask & env_r) == 0) ||
                    (pmask != 0 && (pmask & env_p) == 0)) {
                    t->filtered_out++;
                    continue;
                }
            }
            if (dw3_pages_find(t, key) != NULL) {
                set_err(err, errsz, lineno, "duplicate key for this build");
                goto fail;
            }
            {
                int i = add_entry(t);

                if (i < 0) {
                    set_err(err, errsz, lineno, "out of memory");
                    goto fail;
                }
                t->e[i].key = key;
                t->e[i].value = val;
                t->e[i].page = d1 + 1;
                t->e[i].page_len = (int)(d2 - (d1 + 1));
                t->e[i].kind = kind;
                t->e[i].unv = unv;
            }
        }
    }
    return 0;
fail:
    dw3_pages_text_free(t);
    return -1;
}

void dw3_pages_text_free(Dw3PagesText *t) {
    free(t->buf);
    free(t->e);
    memset(t, 0, sizeof(*t));
}

const Dw3PagesEntry *dw3_pages_find(const Dw3PagesText *t, const char *key) {
    int i;

    for (i = 0; i < t->n; i++) {
        if (strcmp(t->e[i].key, key) == 0) {
            return &t->e[i];
        }
    }
    return NULL;
}

/* --- tokens --------------------------------------------------------------- */

enum { SEEN_MAX = 96, SEEN_LEN = 72 };
static char g_seen[SEEN_MAX][SEEN_LEN];
static int g_nseen;

void dw3_pages_reset_token_log(void) {
    g_nseen = 0;
}

/* 1 the first time `name` is seen since the reset. */
static int first_seen(const char *name) {
    int i;

    for (i = 0; i < g_nseen; i++) {
        if (strcmp(g_seen[i], name) == 0) {
            return 0;
        }
    }
    if (g_nseen < SEEN_MAX) {
        copy_str(g_seen[g_nseen++], SEEN_LEN, name);
    }
    return 1;
}

int dw3_pages_resolve(const char *in, char *out, int outsz,
                      const Dw3PagesResolver *r, int *unresolved) {
    int n = 0;
    const char *p = in;

#define PUT(c)                                                                 \
    do {                                                                       \
        if (n + 1 >= outsz) {                                                  \
            return -1;                                                         \
        }                                                                      \
        out[n++] = (char)(c);                                                  \
    } while (0)

    while (*p != '\0') {
        if (p[0] == '\\' && p[1] == 'n') {
            PUT('\n');
            p += 2;
        } else if (p[0] == '{' && p[1] == '{') {
            PUT('{');
            p += 2;
        } else if (p[0] == '}' && p[1] == '}') {
            PUT('}');
            p += 2;
        } else if (p[0] == '{') {
            char kind[16];
            char name[48];
            char tmp[160];
            char full[SEEN_LEN];
            const char *colon = strchr(p, ':');
            const char *close = strchr(p, '}');
            int ok = -1;
            size_t kl;
            size_t nl;
            const char *s;

            if (colon == NULL || close == NULL || colon > close) {
                PUT(*p);
                p++;
                continue;
            }
            kl = (size_t)(colon - (p + 1));
            nl = (size_t)(close - (colon + 1));
            if (kl >= sizeof(kind)) {
                kl = sizeof(kind) - 1;
            }
            if (nl >= sizeof(name)) {
                nl = sizeof(name) - 1;
            }
            memcpy(kind, p + 1, kl);
            kind[kl] = '\0';
            memcpy(name, colon + 1, nl);
            name[nl] = '\0';
            snprintf(full, sizeof(full), "%s:%s", kind, name);
            tmp[0] = '\0';
            if (r != NULL && r->fn != NULL) {
                ok = r->fn(r->ud, kind, name, tmp, (int)sizeof(tmp));
            }
            if (ok == 0) {
                if (first_seen(full)) {
                    plog("dw3-pages: token {%s} -> \"%s\"", full, tmp);
                }
                s = tmp;
            } else {
                if (unresolved != NULL) {
                    (*unresolved)++;
                }
                if (first_seen(full)) {
                    plog("dw3-pages: token %s unresolved, drawn as [?%s]", full,
                         full);
                }
                snprintf(tmp, sizeof(tmp), "[?%s]", full);
                s = tmp;
            }
            while (*s != '\0') {
                PUT(*s);
                s++;
            }
            p = close + 1;
        } else {
            PUT(*p);
            p++;
        }
    }
    out[n] = '\0';
    return n;
#undef PUT
}

/* --- book ----------------------------------------------------------------- */

int dw3_pages_book_build(Dw3PagesBook *b, const Dw3PagesText *en,
                         const Dw3PagesText *lang, const char *lang_code) {
    int i;
    int cur = -1;
    int w;
    const char *book = en->book[0] != '\0' ? en->book : "book";

    memset(b, 0, sizeof(*b));
    b->en = en;
    b->lang = lang != en ? lang : NULL;
    copy_str(b->lang_code, sizeof(b->lang_code),
             lang_code != NULL ? lang_code : "en");
    for (i = 0; i < en->n; i++) {
        const Dw3PagesEntry *e = &en->e[i];
        const char *value = e->value;
        int unv = e->unv;
        int fb = 0;

        if (e->kind == DW3_PAGES_STR) {
            continue;
        }
        if (b->lang != NULL) {
            const Dw3PagesEntry *le = dw3_pages_find(b->lang, e->key);

            if (le != NULL) {
                value = le->value;
                unv = unv || le->unv;
            } else {
                fb = 1;
                b->fallbacks++;
                plog("dw3-pages: text fallback %s %s (%s)", book, e->key,
                     b->lang_code);
            }
        }
        if (cur < 0 || (int)strlen(b->pages[cur].id) != e->page_len ||
            memcmp(b->pages[cur].id, e->page, (size_t)e->page_len) != 0) {
            int k;

            for (k = 0; k < b->npages; k++) {
                if ((int)strlen(b->pages[k].id) == e->page_len &&
                    memcmp(b->pages[k].id, e->page, (size_t)e->page_len) == 0) {
                    return -1; /* the keys of a page must be together */
                }
            }
            if (b->npages >= DW3_PAGES_MAX_PAGES || e->page_len >= 24) {
                return -1;
            }
            cur = b->npages++;
            memcpy(b->pages[cur].id, e->page, (size_t)e->page_len);
            b->pages[cur].id[e->page_len] = '\0';
            b->pages[cur].first = b->nblocks;
        }
        if (e->kind == DW3_PAGES_TITLE) {
            b->pages[cur].title = value;
        } else if (e->kind == DW3_PAGES_BLURB) {
            b->pages[cur].blurb = value;
        } else {
            Dw3PagesBlock *blk;

            if (b->nblocks >= DW3_PAGES_MAX_BLOCKS) {
                return -1;
            }
            blk = &b->blocks[b->nblocks++];
            blk->kind = e->kind;
            blk->key = e->key;
            blk->value = value;
            blk->unv = unv;
            blk->fallback = fb;
            b->pages[cur].nblocks++;
            b->pages[cur].unverified += unv;
        }
    }
    /* Hide pages with no title or no block (all filtered out). */
    for (i = 0, w = 0; i < b->npages; i++) {
        if (b->pages[i].title == NULL || b->pages[i].nblocks == 0) {
            continue;
        }
        if (w != i) {
            b->pages[w] = b->pages[i];
        }
        b->unverified += b->pages[w].unverified;
        w++;
    }
    b->npages = w;
    return w;
}

const char *dw3_pages_book_str(const Dw3PagesBook *b, const char *name) {
    char key[96];
    const Dw3PagesEntry *e = NULL;
    const char *book = b->en->book[0] != '\0' ? b->en->book : "book";

    snprintf(key, sizeof(key), "%s.ui.str.%s", book, name);
    if (b->lang != NULL) {
        e = dw3_pages_find(b->lang, key);
    }
    if (e == NULL) {
        e = dw3_pages_find(b->en, key);
    }
    return e != NULL ? e->value : NULL;
}

/* --- layout --------------------------------------------------------------- */

enum { ARENA_CAP = 96 * 1024, LINES_CAP = 1536 };

void dw3_pages_layout_init(Dw3PagesLayout *l) {
    memset(l, 0, sizeof(*l));
}

void dw3_pages_layout_free(Dw3PagesLayout *l) {
    free(l->lines);
    free(l->arena);
    memset(l, 0, sizeof(*l));
}

static Dw3PagesLine *new_line(Dw3PagesLayout *l) {
    Dw3PagesLine *ln;

    if (l->n >= l->cap) {
        l->ok = 0;
        return NULL;
    }
    ln = &l->lines[l->n++];
    memset(ln, 0, sizeof(*ln));
    return ln;
}

static int text_w(Dw3PagesMeasureFn m, void *ud, int face, const char *s,
                  int len) {
    return len <= 0 ? 0 : m(ud, face, s, len);
}

static int wrap_para(Dw3PagesLayout *l, const char *s, int len, int face,
                     int style, int first_flags, int rest_flags, int x,
                     int width, Dw3PagesMeasureFn m, void *ud, int *first) {
    int pos = 0;
    int added = 0;

    if (len == 0) {
        Dw3PagesLine *ln = new_line(l);

        if (ln == NULL) {
            return added;
        }
        ln->style = (unsigned char)style;
        ln->flags = (unsigned char)(*first ? first_flags : rest_flags);
        *first = 0;
        return 1;
    }
    while (pos < len) {
        int last_ok = -1;
        int i;
        Dw3PagesLine *ln;

        while (pos < len && s[pos] == ' ') {
            pos++;
        }
        if (pos >= len) {
            break;
        }
        i = pos;
        while (i < len) {
            int we = i;

            while (we < len && s[we] != ' ') {
                we++;
            }
            if (text_w(m, ud, face, s + pos, we - pos) <= width) {
                last_ok = we;
                i = we;
                while (i < len && s[i] == ' ') {
                    i++;
                }
            } else {
                break;
            }
        }
        if (last_ok < 0) {
            /* One word wider than the line: break it between characters. */
            int k = pos;

            while (k < len) {
                unsigned cp;
                int u = dw3_pages_utf8(s + k, len - k, &cp);

                if (u <= 0) {
                    break;
                }
                if (k > pos && text_w(m, ud, face, s + pos, k + u - pos) > width) {
                    break;
                }
                k += u;
            }
            last_ok = k > pos ? k : pos + 1;
        }
        ln = new_line(l);
        if (ln == NULL) {
            return added;
        }
        ln->s[0] = s + pos;
        ln->len[0] = (short)(last_ok - pos);
        ln->x[0] = (short)x;
        ln->face[0] = (unsigned char)face;
        ln->style = (unsigned char)style;
        ln->flags = (unsigned char)(*first ? first_flags : rest_flags);
        *first = 0;
        added++;
        pos = last_ok;
    }
    return added;
}

int dw3_pages_wrap(Dw3PagesLayout *l, const char *text, int face, int style,
                   int first_flags, int rest_flags, int x, int width,
                   Dw3PagesMeasureFn measure, void *ud) {
    const char *p = text;
    int added = 0;
    int first = 1;

    for (;;) {
        const char *nl = strchr(p, '\n');
        int len = nl != NULL ? (int)(nl - p) : (int)strlen(p);

        added += wrap_para(l, p, len, face, style, first_flags, rest_flags, x,
                           width, measure, ud, &first);
        if (nl == NULL) {
            break;
        }
        p = nl + 1;
    }
    return added;
}

/* Resolved text of a block into the arena; NULL when it does not fit. */
static char *arena_text(Dw3PagesLayout *l, const Dw3PagesBlock *blk,
                        const Dw3PagesLayoutOpts *o) {
    char *dst = l->arena + l->arena_len;
    int room = (int)(l->arena_cap - l->arena_len);
    int n;

    n = dw3_pages_resolve(blk->value, dst, room, &o->resolver, &l->unresolved);
    if (n < 0) {
        l->ok = 0;
        return NULL;
    }
    if (blk->unv && o->unv_tag != NULL && o->unv_tag[0] != '\0') {
        int tl = (int)strlen(o->unv_tag);

        if (n + 1 + tl + 1 > room) {
            l->ok = 0;
            return NULL;
        }
        dst[n++] = ' ';
        memcpy(dst + n, o->unv_tag, (size_t)tl + 1);
        n += tl;
    }
    l->arena_len += (size_t)n + 1;
    return dst;
}

static int tight(int prev, int cur) {
    if (prev == DW3_PAGES_H) {
        return 1;
    }
    if (cur == DW3_PAGES_H) {
        return 0;
    }
    return (prev == DW3_PAGES_LI && cur == DW3_PAGES_LI) ||
           (prev == DW3_PAGES_ROW && cur == DW3_PAGES_ROW) ||
           (prev == DW3_PAGES_Q && cur == DW3_PAGES_A);
}

int dw3_pages_layout_page(Dw3PagesLayout *l, const Dw3PagesBook *b, int page,
                          const Dw3PagesLayoutOpts *o) {
    const Dw3PagesPage *pg;
    int j;
    int prev = -1;

    if (page < 0 || page >= b->npages || o->measure == NULL) {
        return -1;
    }
    if (l->lines == NULL) {
        l->lines = (Dw3PagesLine *)malloc(LINES_CAP * sizeof(Dw3PagesLine));
        l->arena = (char *)malloc(ARENA_CAP);
        if (l->lines == NULL || l->arena == NULL) {
            dw3_pages_layout_free(l);
            return -1;
        }
        l->cap = LINES_CAP;
        l->arena_cap = ARENA_CAP;
    }
    l->n = 0;
    l->arena_len = 0;
    l->unresolved = 0;
    l->ok = 1;
    pg = &b->pages[page];
    for (j = 0; j < pg->nblocks; j++) {
        const Dw3PagesBlock *blk = &b->blocks[pg->first + j];
        char *txt;
        Dw3PagesLine *ln;

        txt = arena_text(l, blk, o);
        if (txt == NULL) {
            return -1;
        }
        if (prev >= 0 && !tight(prev, blk->kind)) {
            ln = new_line(l);
            if (ln == NULL) {
                return -1;
            }
            ln->style = DW3_PAGES_S_BLANK;
        }
        prev = blk->kind;
        switch (blk->kind) {
        case DW3_PAGES_H:
            dw3_pages_wrap(l, txt, DW3_PAGES_FACE_BOLD, DW3_PAGES_S_H, 0, 0, 0,
                           o->width, o->measure, o->measure_ud);
            break;
        case DW3_PAGES_LI:
            dw3_pages_wrap(l, txt, DW3_PAGES_FACE_REGULAR, DW3_PAGES_S_LI,
                           DW3_PAGES_LF_BULLET, 0, o->bullet_indent,
                           o->width - o->bullet_indent, o->measure,
                           o->measure_ud);
            break;
        case DW3_PAGES_NOTE:
        case DW3_PAGES_WARN:
            dw3_pages_wrap(l, txt, DW3_PAGES_FACE_REGULAR,
                           blk->kind == DW3_PAGES_NOTE ? DW3_PAGES_S_NOTE
                                                       : DW3_PAGES_S_WARN,
                           DW3_PAGES_LF_BAR, DW3_PAGES_LF_BAR, o->bar_indent,
                           o->width - o->bar_indent, o->measure, o->measure_ud);
            break;
        case DW3_PAGES_Q:
            dw3_pages_wrap(l, txt, DW3_PAGES_FACE_BOLD, DW3_PAGES_S_Q, 0, 0, 0,
                           o->width, o->measure, o->measure_ud);
            break;
        case DW3_PAGES_A:
            dw3_pages_wrap(l, txt, DW3_PAGES_FACE_REGULAR, DW3_PAGES_S_A, 0, 0,
                           o->answer_indent, o->width - o->answer_indent,
                           o->measure, o->measure_ud);
            break;
        case DW3_PAGES_ROW: {
            char *sep = strstr(txt, " | ");
            const char *right = "";
            int n0 = l->n;
            int n1;
            int n2;
            int nl;
            int nr;
            int k;

            if (sep != NULL) {
                *sep = '\0';
                right = sep + 3;
            }
            dw3_pages_wrap(l, txt, DW3_PAGES_FACE_REGULAR, DW3_PAGES_S_ROW, 0, 0,
                           0, o->col1_w, o->measure, o->measure_ud);
            n1 = l->n;
            dw3_pages_wrap(l, right, DW3_PAGES_FACE_BOLD, DW3_PAGES_S_ROW, 0, 0,
                           o->col2_x, o->col2_w, o->measure, o->measure_ud);
            n2 = l->n;
            nl = n1 - n0;
            nr = n2 - n1;
            for (k = 0; k < nr; k++) {
                Dw3PagesLine rl = l->lines[n1 + k];
                Dw3PagesLine *dst = &l->lines[n0 + k];

                if (k >= nl) {
                    memset(dst, 0, sizeof(*dst));
                    dst->style = DW3_PAGES_S_ROW;
                }
                dst->s[1] = rl.s[0];
                dst->len[1] = rl.len[0];
                dst->x[1] = rl.x[0];
                dst->face[1] = rl.face[0];
            }
            l->n = n0 + (nl > nr ? nl : nr);
            break;
        }
        default:
            dw3_pages_wrap(l, txt, DW3_PAGES_FACE_REGULAR, DW3_PAGES_S_P, 0, 0, 0,
                           o->width, o->measure, o->measure_ud);
            break;
        }
        if (!l->ok) {
            return -1;
        }
    }
    return l->n;
}

/* --- scrolling ------------------------------------------------------------ */

void dw3_pages_scroll_init(Dw3PagesScroll *s, int nlines, int panel) {
    s->nlines = nlines;
    s->panel = panel > 0 ? panel : 1;
    s->top = 0;
}

int dw3_pages_scroll_max(const Dw3PagesScroll *s) {
    return s->nlines > s->panel ? s->nlines - s->panel : 0;
}

int dw3_pages_scroll_by(Dw3PagesScroll *s, int delta) {
    int top = s->top + delta;
    int mx = dw3_pages_scroll_max(s);

    if (top > mx) {
        top = mx;
    }
    if (top < 0) {
        top = 0;
    }
    if (top == s->top) {
        return 0;
    }
    s->top = top;
    return 1;
}

int dw3_pages_scroll_page(Dw3PagesScroll *s, int dir) {
    int step = s->panel > 1 ? s->panel - 1 : 1;

    return dw3_pages_scroll_by(s, dir < 0 ? -step : step);
}

int dw3_pages_scroll_at_end(const Dw3PagesScroll *s) {
    return s->top + s->panel >= s->nlines;
}

int dw3_pages_scroll_needed(const Dw3PagesScroll *s) {
    return s->nlines > s->panel;
}
