/* P9.4 option file store; see port_options_cfg.h. */
#include "port_options_cfg.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int is_key_char(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
}

static int key_ok(const char *key) {
    size_t i;
    size_t n = key != NULL ? strlen(key) : 0;

    if (n == 0 || n >= DW3_OPTCFG_KEY_MAX) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (!is_key_char((unsigned char)key[i])) {
            return 0;
        }
    }
    return 1;
}

static int val_ok(const char *val) {
    size_t i;
    size_t n = val != NULL ? strlen(val) : 0;

    if (n >= DW3_OPTCFG_VAL_MAX) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)val[i];

        if (c < 0x20 || c == 0x7f) { /* no newline injection */
            return 0;
        }
    }
    return 1;
}

void dw3_optcfg_init(Dw3OptCfg *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->version = DW3_OPTCFG_VERSION;
}

int dw3_optcfg_parse_int(const char *val, int *out) {
    long v = 0;
    int neg = 0;
    size_t i = 0;

    if (val == NULL) {
        return -1;
    }
    if (val[0] == '-') {
        neg = 1;
        i = 1;
    }
    if (val[i] == '\0') {
        return -1;
    }
    for (; val[i] != '\0'; i++) {
        if (val[i] < '0' || val[i] > '9') {
            return -1;
        }
        v = v * 10 + (val[i] - '0');
        if (v > INT_MAX) {
            return -1;
        }
    }
    *out = (int)(neg ? -v : v);
    return 0;
}

const char *dw3_optcfg_get(const Dw3OptCfg *cfg, const char *key) {
    int i;

    for (i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->entry[i].key, key) == 0) {
            return cfg->entry[i].val;
        }
    }
    return NULL;
}

int dw3_optcfg_set(Dw3OptCfg *cfg, const char *key, const char *val) {
    int i;

    if (!key_ok(key) || !val_ok(val)) {
        return -1;
    }
    for (i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->entry[i].key, key) == 0) {
            strcpy(cfg->entry[i].val, val);
            return 0;
        }
    }
    if (cfg->count >= DW3_OPTCFG_MAX_ENTRIES) {
        return -1;
    }
    strcpy(cfg->entry[cfg->count].key, key);
    strcpy(cfg->entry[cfg->count].val, val);
    cfg->count++;
    return 0;
}

int dw3_optcfg_remove(Dw3OptCfg *cfg, const char *key) {
    int i;

    for (i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->entry[i].key, key) == 0) {
            for (; i + 1 < cfg->count; i++) {
                cfg->entry[i] = cfg->entry[i + 1];
            }
            cfg->count--;
            return 1;
        }
    }
    return 0;
}

static void parse_line(Dw3OptCfg *cfg, const char *s, size_t n) {
    char key[DW3_OPTCFG_KEY_MAX];
    char val[DW3_OPTCFG_VAL_MAX];
    size_t eq;
    size_t kend;
    size_t vstart;

    while (n > 0 && (s[0] == ' ' || s[0] == '\t')) {
        s++;
        n--;
    }
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                     s[n - 1] == '\r')) {
        n--;
    }
    if (n == 0 || s[0] == '#' || s[0] == ';') {
        return;
    }
    for (eq = 0; eq < n && s[eq] != '='; eq++) {
    }
    if (eq == n) {
        cfg->skipped++;
        return;
    }
    kend = eq;
    while (kend > 0 && (s[kend - 1] == ' ' || s[kend - 1] == '\t')) {
        kend--;
    }
    vstart = eq + 1;
    while (vstart < n && (s[vstart] == ' ' || s[vstart] == '\t')) {
        vstart++;
    }
    if (kend == 0 || kend >= sizeof(key) || n - vstart >= sizeof(val)) {
        cfg->skipped++;
        return;
    }
    memcpy(key, s, kend);
    key[kend] = '\0';
    memcpy(val, s + vstart, n - vstart);
    val[n - vstart] = '\0';

    if (strcmp(key, "end") == 0) {
        cfg->complete = strcmp(val, "1") == 0;
        return;
    }
    if (strcmp(key, "version") == 0) {
        int v;

        if (dw3_optcfg_parse_int(val, &v) != 0 || v < 1) {
            cfg->skipped++; /* keep DW3_OPTCFG_VERSION */
        } else {
            cfg->version = v;
        }
        return;
    }
    if (dw3_optcfg_set(cfg, key, val) != 0) {
        cfg->skipped++;
    }
}

void dw3_optcfg_parse(Dw3OptCfg *cfg, const char *text, size_t len) {
    size_t start = 0;
    size_t i;

    dw3_optcfg_init(cfg);
    for (i = 0; i <= len; i++) {
        if (i == len || text[i] == '\n') {
            if (i > start) {
                parse_line(cfg, text + start, i - start);
            }
            start = i + 1;
        }
    }
}

size_t dw3_optcfg_serialize(const Dw3OptCfg *cfg, char *buf, size_t cap) {
    size_t len = 0;
    int i;
    int n;

#define OPTCFG_PUT(...)                                                      \
    do {                                                                     \
        n = snprintf(buf + len, cap - len, __VA_ARGS__);                     \
        if (n < 0 || (size_t)n >= cap - len) {                               \
            return 0;                                                        \
        }                                                                    \
        len += (size_t)n;                                                    \
    } while (0)

    if (cap == 0) {
        return 0;
    }
    OPTCFG_PUT("# dw3-switch port options (docs/PORT_OPTIONS.md)\n");
    OPTCFG_PUT("version=%d\n", cfg->version < 1 ? DW3_OPTCFG_VERSION
                                                : cfg->version);
    for (i = 0; i < cfg->count; i++) {
        OPTCFG_PUT("%s=%s\n", cfg->entry[i].key, cfg->entry[i].val);
    }
    OPTCFG_PUT("end=1\n");
#undef OPTCFG_PUT
    return len;
}

int dw3_optcfg_path(const char *dir, const char *suffix, char *out,
                    size_t cap) {
    int n;
    size_t dl;

    if (dir == NULL || dir[0] == '\0') {
        return -1;
    }
    dl = strlen(dir);
    n = snprintf(out, cap, "%s%s%s%s", dir,
                 (dir[dl - 1] == '/' || dir[dl - 1] == '\\') ? "" : "/",
                 DW3_OPTCFG_FILE, suffix != NULL ? suffix : "");
    return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}

/* Returns 1 read, 0 absent, -1 error/too big. */
static int read_text(const char *path, char *buf, size_t cap, size_t *len) {
    FILE *fp = fopen(path, "rb");
    size_t n;

    if (fp == NULL) {
        return errno == ENOENT ? 0 : -1;
    }
    n = fread(buf, 1, cap, fp);
    if (ferror(fp) || n >= cap) { /* cap bytes read means "at least cap" */
        fclose(fp);
        return -1;
    }
    fclose(fp);
    *len = n;
    return 1;
}

int dw3_optcfg_load(Dw3OptCfg *cfg, const char *dir) {
    char path[DW3_OPTCFG_PATH_MAX];
    char text[DW3_OPTCFG_TEXT_MAX];
    size_t len = 0;
    int rc;

    dw3_optcfg_init(cfg);
    if (dw3_optcfg_path(dir, "", path, sizeof(path)) != 0) {
        return DW3_OPTCFG_LOAD_ERROR;
    }
    rc = read_text(path, text, sizeof(text), &len);
    if (rc > 0) {
        dw3_optcfg_parse(cfg, text, len);
        return DW3_OPTCFG_LOAD_OK;
    }
    if (rc < 0) {
        return DW3_OPTCFG_LOAD_ERROR;
    }
    /* No file. A save interrupted between the unlink and the rename leaves a
     * complete .tmp; a torn one lacks `end=1` and is not trusted. */
    if (dw3_optcfg_path(dir, ".tmp", path, sizeof(path)) == 0 &&
        read_text(path, text, sizeof(text), &len) > 0) {
        dw3_optcfg_parse(cfg, text, len);
        if (cfg->complete) {
            return DW3_OPTCFG_LOAD_RECOVERED;
        }
        dw3_optcfg_init(cfg);
    }
    return DW3_OPTCFG_LOAD_MISSING;
}

int dw3_optcfg_save(const Dw3OptCfg *cfg, const char *dir) {
    char path[DW3_OPTCFG_PATH_MAX];
    char tmp[DW3_OPTCFG_PATH_MAX];
    char text[DW3_OPTCFG_TEXT_MAX];
    size_t len;
    FILE *fp;
    int err;

    if (dw3_optcfg_path(dir, "", path, sizeof(path)) != 0 ||
        dw3_optcfg_path(dir, ".tmp", tmp, sizeof(tmp)) != 0) {
        errno = ENAMETOOLONG;
        return -1;
    }
    len = dw3_optcfg_serialize(cfg, text, sizeof(text));
    if (len == 0) {
        errno = EFBIG;
        return -1;
    }
    fp = fopen(tmp, "wb");
    if (fp == NULL) {
        return -1;
    }
    if (fwrite(text, 1, len, fp) != len || fflush(fp) != 0) {
        err = errno;
        fclose(fp);
        remove(tmp);
        errno = err;
        return -1;
    }
    if (fclose(fp) != 0) {
        err = errno;
        remove(tmp);
        errno = err;
        return -1;
    }
    /* Same rule as the flat saves (port/memcard/memcard_host.c): rename()
     * over an existing file fails with EEXIST on FAT/exFAT, so remove the
     * old file first. If the rename then fails the complete .tmp stays and
     * dw3_optcfg_load recovers from it. */
    if (remove(path) != 0 && errno != ENOENT) {
        err = errno;
        remove(tmp);
        errno = err;
        return -1;
    }
    if (rename(tmp, path) != 0) {
        return -1; /* keep the .tmp, the destination is gone */
    }
    return 0;
}

/* ---- D3 / P6.26: one global options file (port_options_cfg.h) ---------------- */

#include <stdarg.h>

int dw3_optcfg_global_dir(const char *cfgdir_env, const char *root, char *out,
                          size_t cap) {
    size_t n;
    const char *src;

    if (cfgdir_env != NULL && cfgdir_env[0] != '\0') {
        src = cfgdir_env;
        n = strlen(src);
    } else {
        src = root != NULL ? root : "";
        n = strlen(src);
        while (n > 1 && (src[n - 1] == '/' || src[n - 1] == '\\')) {
            n--;
        }
        if (n >= 3 && (src[n - 3] == '/' || src[n - 3] == '\\') &&
            (src[n - 2] == 'u' || src[n - 2] == 'e') &&
            (src[n - 1] == 's' || src[n - 1] == 'u')) {
            const int us = src[n - 2] == 'u' && src[n - 1] == 's';
            const int eu = src[n - 2] == 'e' && src[n - 1] == 'u';

            if (us || eu) {
                n -= 3;
                while (n > 1 && (src[n - 1] == '/' || src[n - 1] == '\\')) {
                    n--;
                }
                if (n == 0) {
                    n = 1; /* "/us" -> "/" */
                    src = "/";
                }
            }
        }
    }
    if (n >= cap) {
        return -1;
    }
    memcpy(out, src, n);
    out[n] = '\0';
    return 0;
}

static void gl_log(Dw3OptCfgLogFn log, void *ud, int warn, const char *fmt, ...) {
    char line[640];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (log != NULL) {
        log(ud, warn, line);
    } else {
        fprintf(stderr, "%s\n", line);
    }
}

static const char *const g_gl_region[2] = {"us", "eu"};

typedef struct GlOld {
    Dw3OptCfg cfg;
    int present;         /* a file was read */
    char dir[DW3_OPTCFG_PATH_MAX];
    char path[DW3_OPTCFG_PATH_MAX]; /* the file that was read (port_options.cfg or .tmp) */
} GlOld;

static int gl_is_region_key(const char *key) {
    return strcmp(key, "region_default") == 0 || strcmp(key, "last_region") == 0;
}

static int gl_gen(const Dw3OptCfg *cfg) {
    const char *v = dw3_optcfg_get(cfg, "defaults_gen");
    int n;

    if (v == NULL || dw3_optcfg_parse_int(v, &n) != 0 || n < 1) {
        return 1; /* a missing key counts as generation 1 (D2) */
    }
    return n;
}

static int gl_same(const Dw3OptCfg *a, const Dw3OptCfg *b) {
    int i;
    const char *v;

    if (a->count != b->count) {
        return 0;
    }
    for (i = 0; i < a->count; i++) {
        v = dw3_optcfg_get(b, a->entry[i].key);
        if (v == NULL || strcmp(v, a->entry[i].val) != 0) {
            return 0;
        }
    }
    return 1;
}

/* Copies `text` to `dst` and reads it back. 0 = the file holds exactly `text`. */
static int gl_copy_verified(const char *dst, const char *text, size_t len) {
    char back[DW3_OPTCFG_TEXT_MAX];
    size_t n = 0;
    FILE *fp = fopen(dst, "wb");
    int err;

    if (fp == NULL) {
        return -1;
    }
    if ((len > 0 && fwrite(text, 1, len, fp) != len) || fflush(fp) != 0) {
        err = errno;
        fclose(fp);
        remove(dst);
        errno = err;
        return -1;
    }
    if (fclose(fp) != 0) {
        err = errno;
        remove(dst);
        errno = err;
        return -1;
    }
    if (read_text(dst, back, sizeof(back), &n) != 1 || n != len ||
        (len > 0 && memcmp(back, text, len) != 0)) {
        errno = EIO;
        return -1;
    }
    return 0;
}

/* The path for the backup of region `r`: port_options.cfg.<r>.bak, or .bak.1 ...
 * when an earlier, different backup is there. Returns 0, 1 when an identical
 * backup is already there (nothing to write), -1 when no name is free. */
static int gl_bak_path(const char *gdir, const char *r, const char *text, size_t len,
                       char *out, size_t cap) {
    char cur[DW3_OPTCFG_TEXT_MAX];
    int i;

    for (i = 0; i < 10; i++) {
        size_t n = 0;
        int rc;

        if (i == 0) {
            snprintf(out, cap, "%s/%s.%s.bak", gdir, DW3_OPTCFG_FILE, r);
        } else {
            snprintf(out, cap, "%s/%s.%s.bak.%d", gdir, DW3_OPTCFG_FILE, r, i);
        }
        rc = read_text(out, cur, sizeof(cur), &n);
        if (rc == 0) {
            return 0;
        }
        if (rc > 0 && n == len && (len == 0 || memcmp(cur, text, len) == 0)) {
            return 1;
        }
    }
    return -1;
}

/* Appends `key` to the comma list of conflicting keys (cut when full). */
static void gl_note(char *list, size_t cap, const char *key) {
    size_t n = strlen(list);

    if (n + strlen(key) + 3 > cap) {
        return;
    }
    snprintf(list + n, cap - n, "%s%s", n > 0 ? ", " : "", key);
}

static void gl_failed(Dw3OptCfgLogFn log, void *ud, const char *fmt, ...) {
    char why[400];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(why, sizeof(why), fmt, ap);
    va_end(ap);
    gl_log(log, ud, 1, "dw3-options: migration NOT done (%s), old files kept", why);
}

int dw3_optcfg_migrate_global(const char *gdir, Dw3OptCfgLogFn log, void *ud) {
    static Dw3OptCfg g_old;   /* the global file as it was (to put back) */
    static Dw3OptCfg g_mrg;   /* the two region files merged */
    static Dw3OptCfg g_fin;   /* the new global file */
    static Dw3OptCfg g_chk;   /* the read-back */
    static GlOld old[2];
    char gpath[DW3_OPTCFG_PATH_MAX];
    char conflicts[256];
    const char *last;
    int rg;
    int i;
    int e;
    int win;
    int nconf = 0;
    int nsrc = 0;
    int nkeys;
    int rc;
    int saved_errno;

    if (gdir == NULL || gdir[0] == '\0' ||
        dw3_optcfg_path(gdir, "", gpath, sizeof(gpath)) != 0) {
        return DW3_OPTGLOBAL_NONE;
    }
    rg = dw3_optcfg_load(&g_old, gdir);
    if (rg > 0 && dw3_optcfg_get(&g_old, DW3_OPTCFG_MARKER_KEY) != NULL) {
        return DW3_OPTGLOBAL_NONE; /* migrated before */
    }
    for (i = 0; i < 2; i++) {
        char tmp[DW3_OPTCFG_PATH_MAX];

        memset(&old[i], 0, sizeof(old[i]));
        if (snprintf(old[i].dir, sizeof(old[i].dir), "%s/%s", gdir,
                     g_gl_region[i]) >= (int)sizeof(old[i].dir)) {
            continue;
        }
        rc = dw3_optcfg_load(&old[i].cfg, old[i].dir);
        if (rc == DW3_OPTCFG_LOAD_MISSING) {
            continue;
        }
        if (rc == DW3_OPTCFG_LOAD_ERROR) {
            gl_failed(log, ud, "cannot read %s/%s", g_gl_region[i], DW3_OPTCFG_FILE);
            return DW3_OPTGLOBAL_FAILED;
        }
        old[i].present = 1;
        dw3_optcfg_path(old[i].dir, "", old[i].path, sizeof(old[i].path));
        if (rc == DW3_OPTCFG_LOAD_RECOVERED) {
            dw3_optcfg_path(old[i].dir, ".tmp", tmp, sizeof(tmp));
            strcpy(old[i].path, tmp);
        }
        nsrc++;
    }
    if (nsrc == 0) {
        if (rg != DW3_OPTCFG_LOAD_ERROR) {
            gl_log(log, ud, 0,
                   "dw3-options: global file %s (no us/eu port_options.cfg to migrate)",
                   gpath);
        }
        return DW3_OPTGLOBAL_NONE;
    }
    if (rg == DW3_OPTCFG_LOAD_ERROR) {
        gl_failed(log, ud, "cannot read the global file %s", gpath);
        return DW3_OPTGLOBAL_FAILED;
    }
    if (rg == DW3_OPTCFG_LOAD_MISSING) {
        dw3_optcfg_init(&g_old);
    }

    /* The winner of a conflict: the last_region region's file; US when the key
     * is missing or invalid, or when that region has no file. */
    last = dw3_optcfg_get(&g_old, "last_region");
    win = (last != NULL && (last[0] == 'e' || last[0] == 'E') &&
           (last[1] == 'u' || last[1] == 'U') && last[2] == '\0')
              ? 1 : 0;
    if (!old[win].present) {
        win = !win;
    }
    if (nsrc == 2) {
        gl_log(log, ud, 0, "dw3-options: migrate winner=%s (last_region=%s)",
               g_gl_region[win], last != NULL ? last : "missing");
    }

    /* Merge the two region files: the loser's keys first, then the winner's, so
     * a key in both ends with the winner's value; a key in one is taken as is. */
    dw3_optcfg_init(&g_mrg);
    {
        const int order[2] = {!win, win};
        int o;

        for (o = 0; o < 2; o++) {
            const GlOld *src = &old[order[o]];
            const GlOld *other = &old[!order[o]];

            if (!src->present) {
                continue;
            }
            if (src->cfg.version > g_mrg.version) {
                g_mrg.version = src->cfg.version;
            }
            for (e = 0; e < src->cfg.count; e++) {
                const char *key = src->cfg.entry[e].key;
                const char *val = src->cfg.entry[e].val;
                const char *ov = other->present ? dw3_optcfg_get(&other->cfg, key) : NULL;

                if (strcmp(key, "defaults_gen") == 0 && nsrc == 2) {
                    continue; /* settled below */
                }
                if (o == 1 && ov != NULL && strcmp(ov, val) != 0) {
                    nconf++;
                    gl_log(log, ud, 0,
                           "dw3-options: migrate key=%s us=%s eu=%s kept=%s", key,
                           win == 0 ? val : ov, win == 1 ? val : ov, val);
                    gl_note(conflicts, sizeof(conflicts), key);
                }
                if (dw3_optcfg_set(&g_mrg, key, val) != 0) {
                    gl_failed(log, ud, "too many keys, the limit is %d entries",
                              DW3_OPTCFG_MAX_ENTRIES);
                    return DW3_OPTGLOBAL_FAILED;
                }
            }
        }
    }
    if (nsrc == 2) {
        /* D2: the older defaults generation, so the migration of the Controls
         * defaults still runs on the merged file. A missing key counts as 1. */
        const int gu = gl_gen(&old[0].cfg);
        const int ge = gl_gen(&old[1].cfg);
        const int g = gu < ge ? gu : ge;
        const char *vu = dw3_optcfg_get(&old[0].cfg, "defaults_gen");
        const char *ve = dw3_optcfg_get(&old[1].cfg, "defaults_gen");

        if (vu != NULL || ve != NULL) {
            char num[16];

            if (gu != ge || (vu == NULL) != (ve == NULL)) {
                nconf++;
                gl_log(log, ud, 0,
                       "dw3-options: migrate key=defaults_gen us=%s eu=%s kept=%d "
                       "(the older generation, so the D2 migration still runs)",
                       vu != NULL ? vu : "(none)", ve != NULL ? ve : "(none)", g);
                gl_note(conflicts, sizeof(conflicts), "defaults_gen");
            }
            if (g > 1) {
                snprintf(num, sizeof(num), "%d", g);
                (void)dw3_optcfg_set(&g_mrg, "defaults_gen", num);
            }
        }
    }
    /* Then the global file: it keeps region_default / last_region and its other
     * keys; a key the regions also hold takes the regions' value (a stray in the
     * old top-level file was never read by the game, so it never was a choice). */
    g_fin = g_old;
    if (g_mrg.version > g_fin.version) {
        g_fin.version = g_mrg.version;
    }
    for (e = 0; e < g_mrg.count; e++) {
        const char *key = g_mrg.entry[e].key;
        const char *val = g_mrg.entry[e].val;
        const char *top = dw3_optcfg_get(&g_old, key);

        if (top != NULL && strcmp(top, val) != 0) {
            if (gl_is_region_key(key)) {
                continue; /* the global file's own value stays */
            }
            nconf++;
            gl_log(log, ud, 0, "dw3-options: migrate key=%s top=%s regions=%s kept=%s",
                   key, top, val, val);
            gl_note(conflicts, sizeof(conflicts), key);
        }
        if (dw3_optcfg_set(&g_fin, key, val) != 0) {
            gl_failed(log, ud, "too many keys, the limit is %d entries",
                      DW3_OPTCFG_MAX_ENTRIES);
            return DW3_OPTGLOBAL_FAILED;
        }
    }
    if (dw3_optcfg_set(&g_fin, DW3_OPTCFG_MARKER_KEY, "1") != 0) {
        gl_failed(log, ud, "too many keys, the limit is %d entries", DW3_OPTCFG_MAX_ENTRIES);
        return DW3_OPTGLOBAL_FAILED;
    }
    nkeys = g_fin.count;

    /* Write, then read back. Nothing is deleted before both are good. */
    if (dw3_optcfg_save(&g_fin, gdir) != 0) {
        saved_errno = errno;
        gl_failed(log, ud, "cannot write %s: %s", gpath, strerror(saved_errno));
        if (rg > 0) {
            (void)dw3_optcfg_save(&g_old, gdir);
        }
        return DW3_OPTGLOBAL_FAILED;
    }
    rc = dw3_optcfg_load(&g_chk, gdir);
    if (rc != DW3_OPTCFG_LOAD_OK || !g_chk.complete || !gl_same(&g_fin, &g_chk) ||
        dw3_optcfg_get(&g_chk, DW3_OPTCFG_MARKER_KEY) == NULL) {
        gl_failed(log, ud, "read-back of %s does not match what was written", gpath);
        if (rg > 0) {
            (void)dw3_optcfg_save(&g_old, gdir); /* the previous global file */
        } else {
            (void)remove(gpath);
        }
        return DW3_OPTGLOBAL_FAILED;
    }
    gl_log(log, ud, 0,
           "dw3-options: global file %s (migrated from %s%s%s, %d keys, %d conflicts%s%s)",
           gpath, old[0].present ? "us" : "", nsrc == 2 ? "+" : "",
           old[1].present ? "eu" : "", nkeys, nconf, nconf > 0 ? ": " : "", conflicts);

    /* The old files: a verified copy first, then the delete. */
    for (i = 0; i < 2; i++) {
        char text[DW3_OPTCFG_TEXT_MAX];
        char bak[DW3_OPTCFG_PATH_MAX];
        size_t len = 0;
        int b;

        if (!old[i].present) {
            continue;
        }
        if (read_text(old[i].path, text, sizeof(text), &len) != 1) {
            gl_log(log, ud, 1,
                   "dw3-options: cannot re-read %s/%s for its backup; the file is kept",
                   g_gl_region[i], DW3_OPTCFG_FILE);
            continue;
        }
        b = gl_bak_path(gdir, g_gl_region[i], text, len, bak, sizeof(bak));
        if (b < 0) {
            gl_log(log, ud, 1,
                   "dw3-options: no free backup name for %s/%s; the file is kept",
                   g_gl_region[i], DW3_OPTCFG_FILE);
            continue;
        }
        if (b == 0 && gl_copy_verified(bak, text, len) != 0) {
            gl_log(log, ud, 1,
                   "dw3-options: backup of %s/%s to %s failed (%s); the file is kept",
                   g_gl_region[i], DW3_OPTCFG_FILE, bak, strerror(errno));
            continue;
        }
        gl_log(log, ud, 0, "dw3-options: backup %s/%s -> %s (%d bytes, verified)",
               g_gl_region[i], DW3_OPTCFG_FILE, bak, (int)len);
        if (remove(old[i].path) != 0) {
            gl_log(log, ud, 1, "dw3-options: could not delete %s/%s (%s); it stays, unused",
                   g_gl_region[i], DW3_OPTCFG_FILE, strerror(errno));
            continue;
        }
        gl_log(log, ud, 0, "dw3-options: deleted %s/%s after read-back ok",
               g_gl_region[i], DW3_OPTCFG_FILE);
    }
    return DW3_OPTGLOBAL_DONE;
}
