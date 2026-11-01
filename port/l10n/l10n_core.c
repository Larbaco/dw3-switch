#define _POSIX_C_SOURCE 200809L

#include "l10n_core.h"

#include "cdfs.h"
#include "sha256.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define PACK_JSON_MAX (256u * 1024u)
#define PACK_FORMAT "dw3-l10n-pack-v1"
#define SECTOR 2048u

static Dw3L10nLogFn g_log;

void dw3_l10n_set_log(Dw3L10nLogFn fn) {
    g_log = fn;
}

void dw3_l10n_say(const char *fmt, ...) {
    char line[336];
    va_list ap;
    size_t n;

    snprintf(line, sizeof(line), "dw3-l10n: ");
    n = strlen(line);
    va_start(ap, fmt);
    vsnprintf(line + n, sizeof(line) - n, fmt, ap);
    va_end(ap);
    if (g_log != NULL) {
        g_log(line);
    } else {
        fprintf(stderr, "%s\n", line);
    }
}

static double now_ms(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

/* --- a small JSON reader, enough for pack.json (build_pack.py writes it) --- */

typedef struct Cur {
    const char *p;
    const char *end;
    int bad;
} Cur;

static void ws(Cur *c) {
    while (c->p < c->end && (*c->p == ' ' || *c->p == '\n' || *c->p == '\r' || *c->p == '\t')) {
        c->p++;
    }
}

static int expect(Cur *c, char ch) {
    ws(c);
    if (c->p < c->end && *c->p == ch) {
        c->p++;
        return 1;
    }
    c->bad = 1;
    return 0;
}

/* A string into out (cap bytes with the NUL); too long = bad. Escapes \" \\ \/ and \uXXXX (as '?'). */
static int str(Cur *c, char *out, size_t cap) {
    size_t n = 0;

    ws(c);
    if (c->p >= c->end || *c->p != '"') {
        c->bad = 1;
        return 0;
    }
    c->p++;
    while (c->p < c->end && *c->p != '"') {
        char ch = *c->p++;

        if (ch == '\\') {
            if (c->p >= c->end) {
                break;
            }
            ch = *c->p++;
            if (ch == 'u') {
                c->p += (c->end - c->p >= 4) ? 4 : (c->end - c->p);
                ch = '?';
            } else if (ch == 'n' || ch == 't' || ch == 'r') {
                ch = ' ';
            }
        }
        if (out != NULL) {
            if (n + 1 >= cap) {
                c->bad = 1;
                return 0;
            }
            out[n] = ch;
        }
        n++;
    }
    if (c->p >= c->end) {
        c->bad = 1;
        return 0;
    }
    c->p++;
    if (out != NULL) {
        out[n] = '\0';
    }
    return 1;
}

static int num(Cur *c, long *out) {
    char *e;
    long v;

    ws(c);
    if (c->p >= c->end || (!isdigit((unsigned char)*c->p) && *c->p != '-')) {
        c->bad = 1;
        return 0;
    }
    v = strtol(c->p, &e, 10);
    if (e == c->p || e > c->end) {
        c->bad = 1;
        return 0;
    }
    c->p = e;
    *out = v;
    return 1;
}

static void skip_value(Cur *c, int depth) {
    ws(c);
    if (c->p >= c->end || depth > 8) {
        c->bad = 1;
        return;
    }
    if (*c->p == '"') {
        (void)str(c, NULL, 0);
    } else if (*c->p == '{' || *c->p == '[') {
        char open = *c->p++;
        char close = open == '{' ? '}' : ']';

        ws(c);
        if (c->p < c->end && *c->p == close) {
            c->p++;
            return;
        }
        while (!c->bad) {
            if (open == '{') {
                (void)str(c, NULL, 0);
                if (!expect(c, ':')) {
                    return;
                }
            }
            skip_value(c, depth + 1);
            ws(c);
            if (c->p < c->end && *c->p == ',') {
                c->p++;
                continue;
            }
            (void)expect(c, close);
            return;
        }
    } else {
        while (c->p < c->end && *c->p != ',' && *c->p != '}' && *c->p != ']' &&
               !isspace((unsigned char)*c->p)) {
            c->p++;
        }
    }
}

static int is_hex64(const char *s) {
    int i;

    if (strlen(s) != 64) {
        return 0;
    }
    for (i = 0; i < 64; i++) {
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) {
            return 0;
        }
    }
    return 1;
}

static int safe_word(const char *s, size_t max) {
    size_t i, n = strlen(s);

    if (n == 0 || n >= max) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (!isalnum((unsigned char)s[i])) {
            return 0;
        }
    }
    return 1;
}

static void parse_file(Cur *c, Dw3L10nFile *f) {
    char key[32];
    long v;

    memset(f, 0, sizeof(*f));
    f->id = -1;
    if (!expect(c, '{')) {
        return;
    }
    ws(c);
    if (c->p < c->end && *c->p == '}') {
        c->p++;
        return;
    }
    while (!c->bad) {
        if (!str(c, key, sizeof(key)) || !expect(c, ':')) {
            return;
        }
        if (strcmp(key, "id") == 0 && num(c, &v)) {
            f->id = (int)v;
        } else if (strcmp(key, "stem") == 0) {
            (void)str(c, f->stem, sizeof(f->stem));
        } else if (strcmp(key, "bytes") == 0 && num(c, &v)) {
            f->bytes = (uint32_t)v;
        } else if (strcmp(key, "sectors") == 0 && num(c, &v)) {
            f->sectors = (uint32_t)v;
        } else if (strcmp(key, "src_bytes") == 0 && num(c, &v)) {
            f->src_bytes = (uint32_t)v;
        } else if (strcmp(key, "keys_bytes") == 0 && num(c, &v)) {
            f->keys_bytes = (uint32_t)v;
        } else if (strcmp(key, "keys_sectors") == 0 && num(c, &v)) {
            f->keys_sectors = (uint32_t)v;
        } else if (strcmp(key, "sha256") == 0) {
            (void)str(c, f->sha256, sizeof(f->sha256));
        } else if (strcmp(key, "src_sha256") == 0) {
            (void)str(c, f->src_sha256, sizeof(f->src_sha256));
        } else if (strcmp(key, "strings") == 0 && num(c, &v)) {
            f->strings = (int)v;
        } else if (strcmp(key, "translated") == 0 && num(c, &v)) {
            f->translated = (int)v;
        } else if (strcmp(key, "fallback") == 0 && num(c, &v)) {
            f->fallback = (int)v;
        } else if (strcmp(key, "fallback_keys") == 0) {
            if (!expect(c, '[')) {
                return;
            }
            ws(c);
            while (!c->bad && c->p < c->end && *c->p != ']') {
                char k[DW3_L10N_KEY_LEN];

                if (!str(c, k, sizeof(k))) {
                    return;
                }
                if (f->nkeys < DW3_L10N_KEYS_MAX) {
                    memcpy(f->keys[f->nkeys++], k, strlen(k) + 1);
                }
                ws(c);
                if (c->p < c->end && *c->p == ',') {
                    c->p++;
                }
                ws(c);
            }
            (void)expect(c, ']');
        } else {
            skip_value(c, 0); /* "file", and keys a later builder adds */
        }
        ws(c);
        if (c->p < c->end && *c->p == ',') {
            c->p++;
            continue;
        }
        (void)expect(c, '}');
        return;
    }
}

int dw3_l10n_pack_parse(const char *text, size_t len, Dw3L10nPack *out, char *err, size_t errcap) {
    Cur c;
    char key[32];
    char format[40] = "";
    int i, j;

    memset(out, 0, sizeof(*out));
    c.p = text;
    c.end = text + len;
    c.bad = 0;
#define FAIL(msg) do { if (err != NULL && errcap > 0) { snprintf(err, errcap, "%s", msg); } return -1; } while (0)
    if (!expect(&c, '{')) {
        FAIL("not a JSON object");
    }
    while (!c.bad) {
        if (!str(&c, key, sizeof(key)) || !expect(&c, ':')) {
            FAIL("bad key");
        }
        if (strcmp(key, "format") == 0) {
            (void)str(&c, format, sizeof(format));
        } else if (strcmp(key, "region") == 0) {
            (void)str(&c, out->region, sizeof(out->region));
        } else if (strcmp(key, "language") == 0) {
            (void)str(&c, out->language, sizeof(out->language));
        } else if (strcmp(key, "synthetic") == 0) {
            ws(&c);
            if (c.p < c.end && *c.p == '"') {
                (void)str(&c, out->synthetic, sizeof(out->synthetic));
            } else {
                skip_value(&c, 0);
            }
        } else if (strcmp(key, "proof_chars") == 0) {
            long v;

            if (num(&c, &v)) {
                out->proof_chars = (int)v;
            }
        } else if (strcmp(key, "files") == 0) {
            if (!expect(&c, '[')) {
                FAIL("files is not an array");
            }
            ws(&c);
            while (!c.bad && c.p < c.end && *c.p != ']') {
                if (out->count >= DW3_L10N_MAX_FILES) {
                    FAIL("too many files");
                }
                parse_file(&c, &out->files[out->count++]);
                ws(&c);
                if (c.p < c.end && *c.p == ',') {
                    c.p++;
                }
                ws(&c);
            }
            (void)expect(&c, ']');
        } else {
            skip_value(&c, 0); /* totals, and anything a later builder adds */
        }
        ws(&c);
        if (c.p < c.end && *c.p == ',') {
            c.p++;
            continue;
        }
        break;
    }
    (void)expect(&c, '}');
    if (c.bad) {
        FAIL("malformed JSON");
    }
    if (strcmp(format, PACK_FORMAT) != 0) {
        FAIL("format is not " PACK_FORMAT);
    }
    if (strcmp(out->region, "eu") != 0 && strcmp(out->region, "us") != 0) {
        FAIL("region is not eu or us");
    }
    if (!safe_word(out->language, sizeof(out->language))) {
        FAIL("bad language code");
    }
    if (out->count < 1) {
        FAIL("no files");
    }
    for (i = 0; i < out->count; i++) {
        const Dw3L10nFile *f = &out->files[i];

        if (f->id <= 0 || f->bytes == 0 || f->src_bytes == 0 ||
            f->sectors != (f->bytes + SECTOR - 1) / SECTOR ||
            (f->keys_bytes != 0 && f->keys_sectors != (f->keys_bytes + SECTOR - 1) / SECTOR) ||
            !is_hex64(f->sha256) || !is_hex64(f->src_sha256) || !safe_word(f->stem, sizeof(f->stem))) {
            FAIL("a file entry is incomplete or inconsistent");
        }
        for (j = 0; j < i; j++) {
            if (out->files[j].id == f->id) {
                FAIL("duplicate file id");
            }
        }
        out->strings += f->strings;
        out->translated += f->translated;
        out->fallback += f->fallback;
    }
#undef FAIL
    return 0;
}

/* --- the loader ---------------------------------------------------------- */

void dw3_l10n_core_bind(Dw3L10n *l, int *sectors, unsigned short *counts, int file_count,
                        const char *lang, const char *region) {
    memset(l, 0, sizeof(*l));
    l->sectors = sectors;
    l->counts = counts;
    l->file_count = file_count;
    snprintf(l->lang, sizeof(l->lang), "%s", lang);
    snprintf(l->region, sizeof(l->region), "%s", region);
    l->ready = 1;
}

static void off(Dw3L10n *l, const char *reason) {
    l->pack_ok = 0;
    snprintf(l->reason, sizeof(l->reason), "%s", reason);
    dw3_l10n_say("off reason=%s", reason);
}

static char *read_all(const char *path, size_t max, size_t *len_out) {
    FILE *fp = fopen(path, "rb");
    char *buf;
    size_t n;

    if (fp == NULL) {
        return NULL;
    }
    buf = (char *)malloc(max + 1);
    if (buf == NULL) {
        fclose(fp);
        return NULL;
    }
    n = fread(buf, 1, max + 1, fp);
    fclose(fp);
    if (n > max) {
        free(buf);
        return NULL;
    }
    buf[n] = '\0';
    *len_out = n;
    return buf;
}

static int pack_file_path(const Dw3L10n *l, int id, int keys, char *out, size_t cap) {
    return snprintf(out, cap, "l10n/%s/%d.%sbin", l->lang, id, keys ? "k." : "") < (int)cap ? 0 : -1;
}

void dw3_l10n_core_set_keys(Dw3L10n *l, int on) {
    l->want_keys = on != 0;
}

int dw3_l10n_core_load(Dw3L10n *l, const char *root) {
    char path[640];
    char err[96];
    char *text;
    size_t len = 0;
    double t0 = now_ms();
    Dw3Sha256 all;
    int i;

    l->loaded = 1;
    l->pack_ok = 0;
    l->reason[0] = '\0';
    snprintf(l->root, sizeof(l->root), "%s", root);
    if (snprintf(path, sizeof(path), "%s/l10n/%s/pack.json", root, l->lang) >= (int)sizeof(path)) {
        off(l, "no_pack");
        return 0;
    }
    text = read_all(path, PACK_JSON_MAX, &len);
    if (text == NULL) {
        struct stat st;

        off(l, stat(path, &st) != 0 ? "no_pack" : "bad_pack");
        return 0;
    }
    if (dw3_l10n_pack_parse(text, len, &l->pack, err, sizeof(err)) != 0) {
        dw3_l10n_say("pack.json: %s", err);
        free(text);
        off(l, "bad_pack");
        return 0;
    }
    free(text);
    if (strcmp(l->pack.region, l->region) != 0 || strcmp(l->pack.language, l->lang) != 0) {
        dw3_l10n_say("pack is for region=%s language=%s, this is region=%s language=%s", l->pack.region,
            l->pack.language, l->region, l->lang);
        off(l, "region_mismatch");
        return 0;
    }
    dw3_sha256_init(&all);
    for (i = 0; i < l->pack.count; i++) {
        const Dw3L10nFile *f = &l->pack.files[i];
        char rel[64];
        char full[640];
        struct stat st;
        unsigned char *src;
        uint32_t sz;
        Dw3Sha256 h;
        char hex[65];

        if (f->id >= l->file_count || l->sectors[f->id] == 0 || cdfs_file_size((uint32_t)f->id) == (uint32_t)-1) {
            dw3_l10n_say("file id %d (%s) is not on this disc", f->id, f->stem);
            off(l, "bad_id");
            return 0;
        }
        if (pack_file_path(l, f->id, 0, rel, sizeof(rel)) != 0 ||
            snprintf(full, sizeof(full), "%s/%s", root, rel) >= (int)sizeof(full)) {
            off(l, "bad_pack");
            return 0;
        }
        if (stat(full, &st) != 0 || !S_ISREG(st.st_mode) || (uint64_t)st.st_size < f->bytes) {
            dw3_l10n_say("%s is missing or shorter than the %u bytes pack.json says", rel, (unsigned)f->bytes);
            off(l, "short_file");
            return 0;
        }
        if ((uint64_t)st.st_size != f->bytes) {
            dw3_l10n_say("%s is %llu bytes, pack.json says %u", rel, (unsigned long long)st.st_size, (unsigned)f->bytes);
            off(l, "bad_pack");
            return 0;
        }
        sz = cdfs_file_size((uint32_t)f->id);
        src = (sz == f->src_bytes) ? (unsigned char *)malloc(sz) : NULL;
        if (src == NULL || cdfs_read((uint32_t)f->id, src, sz) != (int)sz) {
            free(src);
            dw3_l10n_say("the English file %d (%s) has %u bytes, the pack was built from %u", f->id, f->stem,
                (unsigned)sz, (unsigned)f->src_bytes);
            off(l, "hash_mismatch");
            return 0;
        }
        dw3_sha256_init(&h);
        dw3_sha256_update(&h, src, sz);
        dw3_sha256_hex(&h, hex);
        free(src);
        if (strcmp(hex, f->src_sha256) != 0) {
            dw3_l10n_say("the English file %d (%s) is not the one the pack was built from", f->id, f->stem);
            off(l, "hash_mismatch");
            return 0;
        }
        dw3_sha256_update(&all, hex, 64);
    }
    dw3_sha256_hex(&all, l->src_hash);
    l->keys_ok = 1;
    for (i = 0; i < l->pack.count; i++) {
        const Dw3L10nFile *f = &l->pack.files[i];
        char rel[64];
        char full[640];
        struct stat st;

        if (f->keys_bytes == 0 || pack_file_path(l, f->id, 1, rel, sizeof(rel)) != 0 ||
            snprintf(full, sizeof(full), "%s/%s", root, rel) >= (int)sizeof(full) || stat(full, &st) != 0 ||
            (uint64_t)st.st_size != f->keys_bytes) {
            l->keys_ok = 0;
            break;
        }
    }
    l->pack_ok = 1;
    dw3_l10n_say("pack ok region=%s files=%d strings=%d translated=%d fallback=%d src=sha256:%.16s%s%s",
        l->pack.region, l->pack.count, l->pack.strings, l->pack.translated, l->pack.fallback,
        l->src_hash, l->pack.synthetic[0] ? " synthetic=" : "", l->pack.synthetic);
    dw3_l10n_say("pack checked in %.1f ms", now_ms() - t0);
    return 1;
}

int dw3_l10n_core_apply(Dw3L10n *l) {
    int i, n = 0, shown = 0;
    uint32_t lba = DW3_L10N_VLBA_BASE;

    if (!l->ready || !l->pack_ok) {
        return -1;
    }
    if (l->applied) {
        return l->pack.count;
    }
    l->keys_on = l->want_keys && l->keys_ok;
    if (l->want_keys && !l->keys_ok) {
        dw3_l10n_say("show keys: this pack has no key variants (build_pack.py --keys); its normal text is used");
    }
    for (i = 0; i < l->pack.count; i++) {
        const Dw3L10nFile *f = &l->pack.files[i];
        const uint32_t sectors = l->keys_on ? f->keys_sectors : f->sectors;
        char rel[64];

        l->saved_sector[i] = l->sectors[f->id];
        l->saved_count[i] = l->counts[f->id];
        l->vlba[i] = lba;
        lba += sectors;
        if (pack_file_path(l, f->id, l->keys_on, rel, sizeof(rel)) != 0 || cdfs_override_set((uint32_t)f->id, rel) != 0) {
            int j;

            for (j = 0; j < i; j++) {
                cdfs_override_clear((uint32_t)l->pack.files[j].id);
                l->sectors[l->pack.files[j].id] = l->saved_sector[j];
                l->counts[l->pack.files[j].id] = l->saved_count[j];
            }
            dw3_l10n_say("cannot override file id %d; staying with the original text", f->id);
            off(l, "bad_id");
            return -1;
        }
        l->sectors[f->id] = (int)l->vlba[i];
        l->counts[f->id] = (unsigned short)sectors;
        n++;
        if (shown < 8) {
            dw3_l10n_say("override id=%d stem=%s bytes=%u lba=0x%x sectors=%u (was %u)%s", f->id, f->stem,
                (unsigned)(l->keys_on ? f->keys_bytes : f->bytes), (unsigned)l->vlba[i], (unsigned)sectors,
                (unsigned)l->saved_count[i], l->keys_on ? " keys" : "");
            shown++;
        }
    }
    if (n > shown) {
        dw3_l10n_say("override: %d more file(s)", n - shown);
    }
    shown = 0;
    for (i = 0; i < l->pack.count && shown < 32; i++) {
        int k;

        for (k = 0; k < l->pack.files[i].nkeys && shown < 32; k++, shown++) {
            dw3_l10n_say("fallback key=%s (empty pt)", l->pack.files[i].keys[k]);
        }
    }
    dw3_l10n_say("fallback total=%d of %d strings", l->pack.fallback, l->pack.strings);
    if (l->pack.proof_chars > 0) {
        dw3_l10n_say("glyph proof n=%d (the pack's card messages carry them: Title, Load Game)", l->pack.proof_chars);
    }
    l->applied = 1;
    return n;
}

int dw3_l10n_core_restore(Dw3L10n *l) {
    int i;

    if (!l->ready || !l->applied) {
        return 0;
    }
    for (i = 0; i < l->pack.count; i++) {
        int id = l->pack.files[i].id;

        cdfs_override_clear((uint32_t)id);
        l->sectors[id] = l->saved_sector[i];
        l->counts[id] = l->saved_count[i];
    }
    l->applied = 0;
    return l->pack.count;
}

int dw3_l10n_core_has_id(const Dw3L10n *l, int id) {
    int i;

    for (i = 0; l->pack_ok && i < l->pack.count; i++) {
        if (l->pack.files[i].id == id) {
            return 1;
        }
    }
    return 0;
}

int dw3_l10n_core_original_sectors(const Dw3L10n *l, int id) {
    int i;

    for (i = 0; l->pack_ok && i < l->pack.count; i++) {
        if (l->pack.files[i].id == id) {
            return l->applied ? (int)l->saved_count[i] : (id < l->file_count ? (int)l->counts[id] : 0);
        }
    }
    return 0;
}
