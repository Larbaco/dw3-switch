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
