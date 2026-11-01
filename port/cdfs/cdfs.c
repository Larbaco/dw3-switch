#define _POSIX_C_SOURCE 200809L

#include "cdfs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#define CDFS_ROOT_MAX 512
#define CDFS_PATH_MAX 512
#define CDFS_KEY_MAX 32
#define CDFS_ALT_MAX 48

typedef struct {
    uint32_t id;
    char path[CDFS_PATH_MAX];
    uint32_t map_bytes; /* 0 = use on-disk size */
    uint32_t lba;       /* absolute disc LBA; 0 if unknown */
    uint32_t sectors;   /* extent length in sectors; 0 if unknown */
    uint32_t form;      /* 1 = Form1/2048, 2 = Form2 pack, 0 unknown */
    char alt[CDFS_ALT_MAX]; /* P6.7: a replacement path under the root (the text
                             * pack, port/l10n); "" = none. Reads use it in place
                             * of `path`, the disc layout (lba, sectors) is kept. */
} CdfsEntry;

static char g_root[CDFS_ROOT_MAX];
static CdfsEntry *g_entries;
static size_t g_count;
static size_t g_cap;
/* One field-map file. A different id replaces it. Freed on shutdown. */
static int g_map_loaded;
static uint32_t g_map_id;
static unsigned char *g_map_buf;
static size_t g_map_len;

static uint64_t mono_us(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
}

static void entries_clear(void) {
    free(g_entries);
    g_entries = NULL;
    g_count = 0;
    g_cap = 0;
}

static void map_release(void) {
    free(g_map_buf);
    g_map_buf = NULL;
    g_map_len = 0;
    g_map_id = 0;
    g_map_loaded = 0;
}

void cdfs_shutdown(void) {
    entries_clear();
    map_release();
    g_root[0] = '\0';
}

int cdfs_set_root(const char *root) {
    size_t n;

    if (root == NULL || root[0] == '\0') {
        return -1;
    }
    n = strlen(root);
    if (n >= CDFS_ROOT_MAX) {
        return -1;
    }
    memcpy(g_root, root, n + 1);
    /* Trim trailing slashes (keep root "/" as-is). */
    while (n > 1 && (g_root[n - 1] == '/' || g_root[n - 1] == '\\')) {
        g_root[--n] = '\0';
    }
    return 0;
}

/* Map paths must be relative under the runtime root: no absolute paths,
 * no empty / "." / ".." components ( / and \\ separators). */
static int path_is_safe_relative(const char *path) {
    const char *p;
    const char *start;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    if (path[0] == '/' || path[0] == '\\') {
        return 0;
    }
    /* Windows drive / UNC-style absolute. */
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':') {
        return 0;
    }

    p = path;
    start = p;
    while (*p) {
        if (*p == '/' || *p == '\\') {
            size_t len = (size_t)(p - start);
            if (len == 0) {
                return 0; /* consecutive separators */
            }
            if ((len == 1 && start[0] == '.') ||
                (len == 2 && start[0] == '.' && start[1] == '.')) {
                return 0;
            }
            p++;
            start = p;
            continue;
        }
        p++;
    }
    {
        size_t len = (size_t)(p - start);
        if (len == 0) {
            return 0; /* trailing separator */
        }
        if ((len == 1 && start[0] == '.') ||
            (len == 2 && start[0] == '.' && start[1] == '.')) {
            return 0;
        }
    }
    return 1;
}

static int entries_push(uint32_t id, const char *path, uint32_t map_bytes,
                        uint32_t lba, uint32_t sectors, uint32_t form) {
    CdfsEntry *e;

    if (path == NULL || path[0] == '\0' || strlen(path) >= CDFS_PATH_MAX) {
        return -1;
    }
    if (!path_is_safe_relative(path)) {
        return -1;
    }
    if (g_count == g_cap) {
        size_t ncap = g_cap ? g_cap * 2 : 64;
        CdfsEntry *n = (CdfsEntry *)realloc(g_entries, ncap * sizeof(CdfsEntry));
        if (n == NULL) {
            return -1;
        }
        g_entries = n;
        g_cap = ncap;
    }
    e = &g_entries[g_count++];
    e->id = id;
    memcpy(e->path, path, strlen(path) + 1);
    e->map_bytes = map_bytes;
    e->lba = lba;
    e->sectors = sectors;
    e->form = form;
    e->alt[0] = '\0';
    return 0;
}

static CdfsEntry *find_entry_mut(uint32_t id) {
    size_t i;
    for (i = 0; i < g_count; i++) {
        if (g_entries[i].id == id) {
            return &g_entries[i];
        }
    }
    return NULL;
}

static const CdfsEntry *find_entry(uint32_t id) {
    return find_entry_mut(id);
}

/* The path reads open: the override when there is one, else the map's. */
static const char *entry_path(const CdfsEntry *e) {
    return e->alt[0] != '\0' ? e->alt : e->path;
}

int cdfs_override_set(uint32_t id, const char *relpath) {
    CdfsEntry *e = find_entry_mut(id);

    if (e == NULL || relpath == NULL || relpath[0] == '\0' ||
        strlen(relpath) >= CDFS_ALT_MAX || !path_is_safe_relative(relpath)) {
        return -1;
    }
    memcpy(e->alt, relpath, strlen(relpath) + 1);
    return 0;
}

void cdfs_override_clear(uint32_t id) {
    CdfsEntry *e = find_entry_mut(id);

    if (e != NULL) {
        e->alt[0] = '\0';
    }
}

int cdfs_override_active(uint32_t id) {
    const CdfsEntry *e = find_entry(id);

    return e != NULL && e->alt[0] != '\0';
}

static int join_path(char *out, size_t outlen, const char *rel) {
    int n;

    if (g_root[0] == '\0') {
        return -1;
    }
    if (!path_is_safe_relative(rel)) {
        return -1;
    }
    n = snprintf(out, outlen, "%s/%s", g_root, rel);
    if (n < 0 || (size_t)n >= outlen) {
        return -1;
    }
    return 0;
}

/* --- Minimal JSON helpers for files_<ver>.json shape ------------------- */

static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

static const char *parse_string(const char *p, char *out, size_t outlen) {
    size_t i = 0;

    p = skip_ws(p);
    if (*p != '"') {
        return NULL;
    }
    p++;
    while (*p && *p != '"') {
        char c = *p++;
        if (c == '\\' && *p) {
            c = *p++;
            switch (c) {
            case '"':
            case '\\':
            case '/':
                break;
            case 'n':
                c = '\n';
                break;
            case 't':
                c = '\t';
                break;
            case 'r':
                c = '\r';
                break;
            default:
                break;
            }
        }
        if (i + 1 >= outlen) {
            return NULL;
        }
        out[i++] = c;
    }
    if (*p != '"') {
        return NULL;
    }
    out[i] = '\0';
    return p + 1;
}

static const char *parse_u32(const char *p, uint32_t *out) {
    unsigned long v;

    p = skip_ws(p);
    if (!isdigit((unsigned char)*p)) {
        return NULL;
    }
    v = strtoul(p, (char **)&p, 10);
    *out = (uint32_t)v;
    return p;
}

static const char *skip_value(const char *p);

static const char *skip_object(const char *p) {
    p = skip_ws(p);
    if (*p != '{') {
        return NULL;
    }
    p++;
    p = skip_ws(p);
    if (*p == '}') {
        return p + 1;
    }
    for (;;) {
        char key[CDFS_KEY_MAX];
        p = parse_string(p, key, sizeof(key));
        if (p == NULL) {
            return NULL;
        }
        p = skip_ws(p);
        if (*p != ':') {
            return NULL;
        }
        p = skip_value(p + 1);
        if (p == NULL) {
            return NULL;
        }
        p = skip_ws(p);
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == '}') {
            return p + 1;
        }
        return NULL;
    }
}

static const char *skip_array(const char *p) {
    p = skip_ws(p);
    if (*p != '[') {
        return NULL;
    }
    p++;
    p = skip_ws(p);
    if (*p == ']') {
        return p + 1;
    }
    for (;;) {
        p = skip_value(p);
        if (p == NULL) {
            return NULL;
        }
        p = skip_ws(p);
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == ']') {
            return p + 1;
        }
        return NULL;
    }
}

static const char *skip_value(const char *p) {
    char tmp[CDFS_PATH_MAX];

    p = skip_ws(p);
    if (*p == '"') {
        return parse_string(p, tmp, sizeof(tmp));
    }
    if (*p == '{') {
        return skip_object(p);
    }
    if (*p == '[') {
        return skip_array(p);
    }
    if (*p == 't' || *p == 'f' || *p == 'n') {
        /* true / false / null */
        while (isalpha((unsigned char)*p)) {
            p++;
        }
        return p;
    }
    if (*p == '-' || isdigit((unsigned char)*p)) {
        if (*p == '-') {
            p++;
        }
        while (isdigit((unsigned char)*p) || *p == '.' || *p == 'e' || *p == 'E' ||
               *p == '+' || *p == '-') {
            p++;
        }
        return p;
    }
    return NULL;
}

static const char *parse_file_object(const char *p, char *path_out, size_t path_len,
                                     uint32_t *bytes_out, uint32_t *lba_out,
                                     uint32_t *sectors_out, uint32_t *form_out) {
    path_out[0] = '\0';
    *bytes_out = 0;
    *lba_out = 0;
    *sectors_out = 0;
    *form_out = 0;

    p = skip_ws(p);
    if (*p != '{') {
        return NULL;
    }
    p++;
    p = skip_ws(p);
    if (*p == '}') {
        return p + 1;
    }
    for (;;) {
        char key[CDFS_KEY_MAX];
        p = parse_string(p, key, sizeof(key));
        if (p == NULL) {
            return NULL;
        }
        p = skip_ws(p);
        if (*p != ':') {
            return NULL;
        }
        p++;
        if (strcmp(key, "path") == 0) {
            p = parse_string(p, path_out, path_len);
            if (p == NULL) {
                return NULL;
            }
        } else if (strcmp(key, "bytes") == 0) {
            p = parse_u32(p, bytes_out);
            if (p == NULL) {
                return NULL;
            }
        } else if (strcmp(key, "lba") == 0) {
            p = parse_u32(p, lba_out);
            if (p == NULL) {
                return NULL;
            }
        } else if (strcmp(key, "sectors") == 0) {
            p = parse_u32(p, sectors_out);
            if (p == NULL) {
                return NULL;
            }
        } else if (strcmp(key, "form") == 0) {
            p = parse_u32(p, form_out);
            if (p == NULL) {
                return NULL;
            }
        } else {
            p = skip_value(p);
            if (p == NULL) {
                return NULL;
            }
        }
        p = skip_ws(p);
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == '}') {
            return p + 1;
        }
        return NULL;
    }
}

static const char *parse_files_object(const char *p) {
    p = skip_ws(p);
    if (*p != '{') {
        return NULL;
    }
    p++;
    p = skip_ws(p);
    if (*p == '}') {
        return p + 1;
    }
    for (;;) {
        char key[CDFS_KEY_MAX];
        char path[CDFS_PATH_MAX];
        uint32_t map_bytes = 0;
        uint32_t lba = 0;
        uint32_t sectors = 0;
        uint32_t form = 0;
        uint32_t id;
        char *end = NULL;

        p = parse_string(p, key, sizeof(key));
        if (p == NULL) {
            return NULL;
        }
        id = (uint32_t)strtoul(key, &end, 10);
        if (end == key || *end != '\0') {
            return NULL;
        }
        p = skip_ws(p);
        if (*p != ':') {
            return NULL;
        }
        p = parse_file_object(p + 1, path, sizeof(path), &map_bytes, &lba,
                              &sectors, &form);
        if (p == NULL) {
            return NULL;
        }
        if (path[0] != '\0') {
            if (entries_push(id, path, map_bytes, lba, sectors, form) != 0) {
                return NULL;
            }
        }
        p = skip_ws(p);
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == '}') {
            return p + 1;
        }
        return NULL;
    }
}

static int load_map_text(const char *text) {
    const char *p = text;
    char key[CDFS_KEY_MAX];

    entries_clear();

    p = skip_ws(p);
    if (*p != '{') {
        return -1;
    }
    p++;
    p = skip_ws(p);
    if (*p == '}') {
        return 0;
    }
    for (;;) {
        p = parse_string(p, key, sizeof(key));
        if (p == NULL) {
            entries_clear();
            return -1;
        }
        p = skip_ws(p);
        if (*p != ':') {
            entries_clear();
            return -1;
        }
        p++;
        if (strcmp(key, "files") == 0) {
            p = parse_files_object(p);
            if (p == NULL) {
                entries_clear();
                return -1;
            }
        } else {
            p = skip_value(p);
            if (p == NULL) {
                entries_clear();
                return -1;
            }
        }
        p = skip_ws(p);
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == '}') {
            return 0;
        }
        entries_clear();
        return -1;
    }
}

static int load_map_file(const char *path) {
    FILE *fp;
    long sz;
    char *buf;
    size_t n;
    int rc;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    buf = (char *)malloc((size_t)sz + 1);
    if (buf == NULL) {
        fclose(fp);
        return -1;
    }
    n = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    buf[n] = '\0';
    rc = load_map_text(buf);
    free(buf);
    return rc;
}

int cdfs_load_map(const char *json_path) {
    char path[CDFS_ROOT_MAX + 32];

    if (json_path != NULL && json_path[0] != '\0') {
        return load_map_file(json_path);
    }
    if (g_root[0] == '\0') {
        return -1;
    }
    snprintf(path, sizeof(path), "%s/files_us.json", g_root);
    if (load_map_file(path) == 0) {
        return 0;
    }
    snprintf(path, sizeof(path), "%s/files_eu.json", g_root);
    return load_map_file(path);
}

/* One stat() per size query instead of fopen + fseek + ftell + fclose (PR
 * #267 audit). A regular file only; a missing file, a directory or a size over
 * 4 GiB is -1, as before. libnx/newlib implement stat on both the sdmc: and
 * romfs: devoptabs (fsdev_stat / romfs_stat), the same ones fopen uses. */
static int host_file_size(const char *path, uint32_t *out) {
    struct stat st;

    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return -1;
    }
    if (st.st_size < 0 || (uint64_t)st.st_size > 0xFFFFFFFFull) {
        return -1;
    }
    *out = (uint32_t)st.st_size;
    return 0;
}

uint32_t cdfs_file_size(uint32_t id) {
    const CdfsEntry *e;
    char path[CDFS_ROOT_MAX + CDFS_PATH_MAX];
    uint32_t sz;

    e = find_entry(id);
    if (e == NULL) {
        return (uint32_t)-1;
    }
    if (join_path(path, sizeof(path), entry_path(e)) != 0) {
        return (uint32_t)-1;
    }
    if (host_file_size(path, &sz) != 0) {
        return (uint32_t)-1;
    }
    return sz;
}

int cdfs_read(uint32_t id, void *buf, size_t buflen) {
    const CdfsEntry *e;
    char path[CDFS_ROOT_MAX + CDFS_PATH_MAX];
    FILE *fp;
    uint32_t sz;
    size_t n;

    if (buf == NULL) {
        return -1;
    }
    e = find_entry(id);
    if (e == NULL) {
        return -1;
    }
    if (join_path(path, sizeof(path), entry_path(e)) != 0) {
        return -1;
    }
    if (host_file_size(path, &sz) != 0) {
        return -1;
    }
    if (buflen < (size_t)sz) {
        return -1;
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    n = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    if (n != (size_t)sz) {
        return -1;
    }
    (void)e->map_bytes; /* map bytes is advisory; host file is source of truth */
    return (int)sz;
}

int cdfs_read_at(uint32_t id, void *buf, size_t offset, size_t len) {
    const CdfsEntry *e;
    char path[CDFS_ROOT_MAX + CDFS_PATH_MAX];
    FILE *fp;
    uint32_t sz;
    size_t n;
    size_t want;

    if (buf == NULL) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    e = find_entry(id);
    if (e == NULL) {
        return -1;
    }
    if (join_path(path, sizeof(path), entry_path(e)) != 0) {
        return -1;
    }
    if (host_file_size(path, &sz) != 0) {
        return -1;
    }
    memset(buf, 0, len);
    if (offset >= (size_t)sz) {
        return 0;
    }
    want = len;
    if (offset + want > (size_t)sz) {
        want = (size_t)sz - offset;
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    n = fread(buf, 1, want, fp);
    fclose(fp);
    if (n != want) {
        return -1;
    }
    (void)e->map_bytes;
    return (int)n;
}

/* One fopen of this id. Later tiles of the same id copy from g_map_buf. */
static int map_load(uint32_t id) {
    const CdfsEntry *e;
    char path[CDFS_ROOT_MAX + CDFS_PATH_MAX];
    FILE *fp;
    long sz;
    unsigned char *buf;
    size_t n;
    uint64_t t0;
    uint64_t dt;

    e = find_entry(id);
    if (e == NULL) {
        return -1;
    }
    if (join_path(path, sizeof(path), entry_path(e)) != 0) {
        return -1;
    }
    t0 = mono_us();
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    sz = ftell(fp);
    if (sz < 0 || (unsigned long)sz > 0xFFFFFFFFul) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    buf = (unsigned char *)malloc((size_t)sz == 0 ? 1 : (size_t)sz);
    if (buf == NULL) {
        fclose(fp);
        return -1;
    }
    n = 0;
    if (sz > 0) {
        n = fread(buf, 1, (size_t)sz, fp);
    }
    fclose(fp);
    if (n != (size_t)sz) {
        free(buf);
        return -1;
    }
    map_release();
    g_map_buf = buf;
    g_map_len = (size_t)sz;
    g_map_id = id;
    g_map_loaded = 1;
    dt = mono_us() - t0;
    fprintf(stderr, "dw3: map_preload id=%u bytes=%zu us=%llu\n", (unsigned)id,
            g_map_len, (unsigned long long)dt);
    return 0;
}

static int map_hold(uint32_t id) {
    if (g_map_loaded && g_map_id == id) {
        return 0;
    }
    return map_load(id);
}

int cdfs_read_field_tile(uint32_t id, void *buf, size_t offset, size_t len) {
    size_t want;

    if (buf == NULL) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    if (map_hold(id) != 0) {
        return -1;
    }
    memset(buf, 0, len);
    if (offset >= g_map_len) {
        return 0;
    }
    want = len;
    if (offset + want > g_map_len) {
        want = g_map_len - offset;
    }
    memcpy(buf, g_map_buf + offset, want);
    return (int)want;
}

int cdfs_join_root(const char *rel, char *out, size_t outlen) {
    return join_path(out, outlen, rel);
}

const char *cdfs_root(void) { return g_root; }

int cdfs_visit(int (*fn)(const CdfsMapInfo *info, void *user), void *user) {
    size_t i;

    if (fn == NULL) {
        return -1;
    }
    for (i = 0; i < g_count; i++) {
        const CdfsEntry *e = &g_entries[i];
        CdfsMapInfo info;
        int rc;

        info.id = e->id;
        info.path = e->path;
        info.lba = e->lba;
        info.sectors = e->sectors;
        info.form = e->form;
        info.bytes = e->map_bytes;
        rc = fn(&info, user);
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}

int cdfs_lookup_lba(uint32_t lba, uint32_t *id_out, char *relpath_out,
                    size_t relpath_len, uint32_t *base_lba_out,
                    uint32_t *sectors_out, uint32_t *form_out) {
    size_t i;

    for (i = 0; i < g_count; i++) {
        const CdfsEntry *e = &g_entries[i];
        uint32_t nsec;

        if (e->lba == 0 || e->path[0] == '\0') {
            continue;
        }
        nsec = e->sectors;
        if (nsec == 0) {
            /* Fall back to on-disk size / 2048 when map omits sectors. */
            char path[CDFS_ROOT_MAX + CDFS_PATH_MAX];
            uint32_t sz;
            if (join_path(path, sizeof(path), e->path) != 0 ||
                host_file_size(path, &sz) != 0) {
                continue;
            }
            nsec = (sz + 2047u) / 2048u;
        }
        if (lba < e->lba || lba >= e->lba + nsec) {
            continue;
        }
        if (id_out) {
            *id_out = e->id;
        }
        if (relpath_out && relpath_len > 0) {
            size_t n = strlen(e->path);
            if (n + 1 > relpath_len) {
                return -1;
            }
            memcpy(relpath_out, e->path, n + 1);
        }
        if (base_lba_out) {
            *base_lba_out = e->lba;
        }
        if (sectors_out) {
            *sectors_out = nsec;
        }
        if (form_out) {
            *form_out = e->form;
        }
        return 0;
    }
    return -1;
}

void cdfs_poll(void) {
    /* The one-tile hold is gone. Kept so older callers still link. */
}
