/* Build a version-matched runtime tree from a Mode2/2352 BIN.
 *
 * Form-1 files are the 2048-byte user payload at offset 24 of each sector.
 * Form-2 .STR files are raw sectors (FILE_SECTOR_COUNTS * 2352). The file
 * list is the linked FILE_SECTORS table (file_table.c); paths come from the
 * ISO9660 path table, the same join build_file_map.py uses. boot_tim.bin is
 * the PS-X EXE tail at the selected version's STAGE_VRAM.
 */
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64

#include "runtime_extract.h"

#include "cdfs.h"
#include "str_disc.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>

#define SECTOR_BYTES 2352u
#define USER_OFF 24u
#define LOGICAL 2048u
#define COPY_CHUNK (256u * 1024u)
#define PROGRESS_MARK (8ull * 1024ull * 1024ull)
#define EXE_HEADER 0x800u
#define EXE_LOAD 0x80010000u
#if defined(VERSION_EU) && VERSION_EU
#define RUNTIME_VERSION "eu"
#define RUNTIME_EXECUTABLE "SLES_039.36"
#define RUNTIME_OTHER_EXECUTABLE "SLUS_014.36"
#define RUNTIME_MAP "files_eu.json"
#define STAGE_VRAM 0x800A5DE0u
#else
#define RUNTIME_VERSION "us"
#define RUNTIME_EXECUTABLE "SLUS_014.36"
#define RUNTIME_OTHER_EXECUTABLE "SLES_039.36"
#define RUNTIME_MAP "files_us.json"
#define STAGE_VRAM 0x800A4CA4u
#endif
#define MAX_FILES 8192
#define MAX_DIRS 1024
#define MAX_DIR_BYTES (4u * 1024u * 1024u)
#define MAX_EXE_BYTES (16ull * 1024ull * 1024ull)
#define PATH_MAX_REL 480

/* file_table.c owns these. s32 is signed int, u16 is unsigned short. */
extern int FILE_SECTORS[];
extern unsigned short FILE_SECTOR_COUNTS[];
extern int FILE_COUNT;

static unsigned long long g_done;
static unsigned long long g_total;
static unsigned long long g_since;
static unsigned long long g_mark;

static void pump(int force_note) {
    unsigned long long mark;
    char buf[80];

    if (!force_note && g_since < COPY_CHUNK && g_done < g_total) {
        return;
    }
    g_since = 0;
    mark = g_done / PROGRESS_MARK;
    if (g_done > 0 && mark != g_mark) {
        g_mark = mark;
        snprintf(buf, sizeof(buf), "extract %llu/%llu", g_done, g_total);
        dw3_str_restore_emit(buf, g_done, g_total);
        return;
    }
    dw3_str_restore_emit(NULL, g_done, g_total);
}

static void account(unsigned long long n) {
    g_done += n;
    g_since += n;
    pump(0);
}

static int file_size(const char *path, unsigned long long *out) {
    struct stat st;

    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return -1;
    }
    *out = (unsigned long long)st.st_size;
    return 0;
}

static int dir_exists(const char *path) {
    struct stat st;
    return path != NULL && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int join_root(const char *root, const char *rel, char *out, size_t n) {
    size_t lr = strlen(root);
    int slash = lr > 0 && root[lr - 1] != '/' && root[lr - 1] != '\\';
    int k = snprintf(out, n, "%s%s%s", root, slash ? "/" : "", rel);
    if (k < 0 || (size_t)k >= n) {
        return -1;
    }
    return 0;
}

static int is_device_prefix(const char *path) {
    return strchr(path, ':') != NULL && strchr(path, '/') == NULL &&
           strchr(path, '\\') == NULL;
}

static int mkdir_p(const char *path) {
    char buf[1024];
    size_t i;
    size_t n;
    int k;

    k = snprintf(buf, sizeof(buf), "%s", path);
    if (k < 0 || (size_t)k >= sizeof(buf)) {
        return -1;
    }
    n = strlen(buf);
    for (i = 1; i < n; i++) {
        if (buf[i] != '/' && buf[i] != '\\') {
            continue;
        }
        buf[i] = '\0';
        if (!is_device_prefix(buf) && mkdir(buf, 0777) != 0 && errno != EEXIST) {
            if (!dir_exists(buf)) {
                return -1;
            }
        }
        buf[i] = '/';
    }
    if (mkdir(buf, 0777) != 0 && errno != EEXIST && !dir_exists(buf)) {
        return -1;
    }
    return 0;
}

static int mkdir_parent(const char *file) {
    char dir[1024];
    char *slash;
    int k;

    k = snprintf(dir, sizeof(dir), "%s", file);
    if (k < 0 || (size_t)k >= sizeof(dir)) {
        return -1;
    }
    slash = strrchr(dir, '/');
    if (slash == NULL) {
        slash = strrchr(dir, '\\');
    }
    if (slash == NULL || slash == dir) {
        return 0;
    }
    *slash = '\0';
    return mkdir_p(dir);
}

static int ends_with_ci(const char *path, const char *suf) {
    size_t n = strlen(path);
    size_t e = strlen(suf);
    size_t i;

    if (n < e) {
        return 0;
    }
    for (i = 0; i < e; i++) {
        unsigned char a = (unsigned char)path[n - e + i];
        unsigned char b = (unsigned char)suf[i];
        if (tolower(a) != tolower(b)) {
            return 0;
        }
    }
    return 1;
}

static int path_safe(const char *path) {
    const char *p;
    const char *start;

    if (path == NULL || path[0] == '\0' || path[0] == '/' || path[0] == '\\') {
        return 0;
    }
    if (strlen(path) >= PATH_MAX_REL) {
        return 0;
    }
    p = path;
    start = p;
    while (*p) {
        unsigned char c = (unsigned char)*p;
        size_t len;
        if (c < 0x20 || c == '"' || c == '\\') {
            return 0;
        }
        if (*p == '/') {
            len = (size_t)(p - start);
            if (len == 0) {
                return 0;
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
            return 0;
        }
        if ((len == 1 && start[0] == '.') ||
            (len == 2 && start[0] == '.' && start[1] == '.')) {
            return 0;
        }
    }
    return 1;
}

static int is_mode2_sync(const unsigned char *s) {
    int i;

    if (s[0] != 0x00 || s[11] != 0x00 || s[15] != 0x02) {
        return 0;
    }
    for (i = 1; i < 11; i++) {
        if (s[i] != 0xFF) {
            return 0;
        }
    }
    return 1;
}

static uint32_t ru32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint16_t ru16(const unsigned char *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int seek_read(FILE *fp, unsigned long long off, void *dst, size_t n) {
    if (fseeko(fp, (off_t)off, SEEK_SET) != 0) {
        return -1;
    }
    if (fread(dst, 1, n, fp) != n) {
        return -1;
    }
    return 0;
}

static int read_user(FILE *fp, uint32_t lba, unsigned char *dst) {
    unsigned char sec[SECTOR_BYTES];
    unsigned long long off = (unsigned long long)lba * SECTOR_BYTES;

    if (seek_read(fp, off, sec, SECTOR_BYTES) != 0) {
        return -1;
    }
    if (!is_mode2_sync(sec)) {
        return -1;
    }
    memcpy(dst, sec + USER_OFF, LOGICAL);
    return 0;
}

static int read_prefix(const char *path, unsigned char *buf, size_t n) {
    FILE *fp;
    size_t got;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    got = fread(buf, 1, n, fp);
    fclose(fp);
    return got == n ? 0 : -1;
}

static int exe_ok(const char *root) {
    char path[1024];
    unsigned char mag[8];
    unsigned long long sz = 0;

    if (join_root(root, RUNTIME_EXECUTABLE, path, sizeof(path)) != 0) {
        return 0;
    }
    if (file_size(path, &sz) != 0 || sz < EXE_HEADER) {
        return 0;
    }
    if (read_prefix(path, mag, sizeof(mag)) != 0) {
        return 0;
    }
    return memcmp(mag, "PS-X EXE", 8) == 0;
}

static int map_identity_ok(const char *path) {
    char head[256];
    FILE *fp = fopen(path, "rb");
    size_t n;
    if (fp == NULL) {
        return 0;
    }
    n = fread(head, 1, sizeof(head) - 1, fp);
    fclose(fp);
    head[n] = '\0';
    return strstr(head, "\"version\": \"" RUNTIME_VERSION "\"") != NULL &&
           strstr(head, "\"executable\": \"" RUNTIME_EXECUTABLE "\"") != NULL;
}

static int boot_tim_ok(const char *root) {
    char path[1024];
    unsigned char mag[4];
    unsigned long long sz = 0;

    if (join_root(root, "boot_tim.bin", path, sizeof(path)) != 0) {
        return 0;
    }
    if (file_size(path, &sz) != 0 || sz < 12) {
        return 0;
    }
    if (read_prefix(path, mag, sizeof(mag)) != 0) {
        return 0;
    }
    return mag[0] == 0x10 && mag[1] == 0x00 && mag[2] == 0x00 && mag[3] == 0x00;
}

/* FAT/exFAT rename does not replace an existing file. Move the old runtime
 * file aside only after its replacement is complete, then restore it if the
 * install fails. These paths contain extracted runtime data, never saves. */
static int replace_runtime_file(const char *tmp, const char *dest) {
    char backup[1200];
    struct stat st;
    int dest_exists;
    int backup_exists;
    int moved_old = 0;
    int err;
    int n = snprintf(backup, sizeof(backup), "%s.dw3-replace-backup", dest);
    if (n < 0 || (size_t)n >= sizeof(backup)) {
        return -1;
    }
    dest_exists = stat(dest, &st) == 0;
    if (!dest_exists && errno != ENOENT) {
        return -1;
    }
    backup_exists = stat(backup, &st) == 0;
    if (!backup_exists && errno != ENOENT) {
        return -1;
    }
    if (dest_exists && backup_exists) {
        /* A prior install committed the destination but missed cleanup. */
        if (remove(backup) != 0) {
            return -1;
        }
        backup_exists = 0;
    } else if (!dest_exists && backup_exists) {
        /* Recover the previous file before attempting this replacement. */
        if (rename(backup, dest) != 0) {
            return -1;
        }
        dest_exists = 1;
    }
    if (dest_exists) {
        if (rename(dest, backup) != 0) {
            return -1;
        }
        moved_old = 1;
    }
    if (rename(tmp, dest) != 0) {
        err = errno;
        if (moved_old) {
            (void)rename(backup, dest);
        }
        errno = err;
        return -1;
    }
    if (moved_old) {
        (void)remove(backup);
    }
    return 0;
}

struct Ready {
    int count;
    int bad;
};

static int visit_ready(const CdfsMapInfo *info, void *user) {
    struct Ready *ready = (struct Ready *)user;
    char abs[1024];
    unsigned long long sz = 0;
    int str;

    ready->count++;
    if (info->path == NULL || !path_safe(info->path) ||
        cdfs_join_root(info->path, abs, sizeof(abs)) != 0 ||
        file_size(abs, &sz) != 0) {
        ready->bad = 1;
        return 1;
    }
    str = info->form == 2 && ends_with_ci(info->path, ".str");
    if (str && info->sectors > 0) {
        unsigned long long raw = (unsigned long long)info->sectors * SECTOR_BYTES;
        unsigned long long pack = (unsigned long long)info->sectors * LOGICAL;
        if (sz != raw && sz != pack &&
            (info->bytes == 0 || sz != (unsigned long long)info->bytes)) {
            ready->bad = 1;
            return 1;
        }
        return 0;
    }
    if (info->bytes > 0 && sz != (unsigned long long)info->bytes) {
        ready->bad = 1;
        return 1;
    }
    return 0;
}

static int tree_ready(const char *root) {
    char map[1024];
    struct Ready ready;

    if (!exe_ok(root) || !boot_tim_ok(root)) {
        return 0;
    }
    if (cdfs_set_root(root) != 0) {
        return 0;
    }
    if (join_root(root, RUNTIME_MAP, map, sizeof(map)) != 0) {
        return 0;
    }
    if (!map_identity_ok(map)) {
        return 0;
    }
    if (cdfs_load_map(map) != 0) {
        return 0;
    }
    ready.count = 0;
    ready.bad = 0;
    if (cdfs_visit(visit_ready, &ready) != 0) {
        return 0;
    }
    return ready.count > 0 && !ready.bad;
}

static int ensure_memcard(const char *root) {
    char path[1024];
    struct stat st;

    if (!dir_exists(root)) {
        return 0;
    }
    if (join_root(root, "memcard", path, sizeof(path)) != 0) {
        return -1;
    }
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    return mkdir_p(path);
}

/* TIM at the PS-X EXE tail. Same offsets as tools/pack_runtime.py. */
static int write_boot_tim(const char *exe_path, const char *tim_path) {
    unsigned char *data = NULL;
    FILE *fp = NULL;
    FILE *out = NULL;
    unsigned long long sz = 0;
    size_t off;
    size_t pos;
    size_t end;
    uint32_t flag;
    uint32_t block;
    char tmp[1100];
    int rc = -1;

    if (file_size(exe_path, &sz) != 0 || sz < EXE_HEADER || sz > MAX_EXE_BYTES) {
        return -1;
    }
    data = (unsigned char *)malloc((size_t)sz);
    if (data == NULL) {
        return -1;
    }
    fp = fopen(exe_path, "rb");
    if (fp == NULL || fread(data, 1, (size_t)sz, fp) != (size_t)sz) {
        goto out;
    }
    fclose(fp);
    fp = NULL;
    if (memcmp(data, "PS-X EXE", 8) != 0) {
        goto out;
    }
    off = (size_t)EXE_HEADER + ((size_t)STAGE_VRAM - (size_t)EXE_LOAD);
    if (off + 8 > (size_t)sz || data[off] != 0x10 || data[off + 1] != 0 ||
        data[off + 2] != 0 || data[off + 3] != 0) {
        goto out;
    }
    flag = ru32(data + off + 4);
    pos = off + 8;
    if (flag & 8u) {
        if (pos + 4 > (size_t)sz) {
            goto out;
        }
        block = ru32(data + pos);
        if (block < 4 || block > (size_t)sz - pos) {
            goto out;
        }
        pos += block;
    }
    if (pos + 4 > (size_t)sz) {
        goto out;
    }
    block = ru32(data + pos);
    if (block < 12 || block > (size_t)sz - pos) {
        goto out;
    }
    end = pos + block;
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", tim_path) >= (int)sizeof(tmp)) {
        goto out;
    }
    if (mkdir_parent(tim_path) != 0) {
        goto out;
    }
    out = fopen(tmp, "wb");
    if (out == NULL || fwrite(data + off, 1, end - off, out) != end - off) {
        if (out != NULL) {
            fclose(out);
        }
        out = NULL;
        remove(tmp);
        goto out;
    }
    if (fclose(out) != 0) {
        out = NULL;
        remove(tmp);
        goto out;
    }
    out = NULL;
    if (replace_runtime_file(tmp, tim_path) != 0) {
        remove(tmp);
        goto out;
    }
    rc = 0;
out:
    if (fp != NULL) {
        fclose(fp);
    }
    free(data);
    return rc;
}

static int try_local_boot_tim(const char *root) {
    char exe[1024];
    char tim[1024];

    if (boot_tim_ok(root) || !exe_ok(root)) {
        return -1;
    }
    if (join_root(root, RUNTIME_EXECUTABLE, exe, sizeof(exe)) != 0 ||
        join_root(root, "boot_tim.bin", tim, sizeof(tim)) != 0) {
        return -1;
    }
    return write_boot_tim(exe, tim);
}

struct IsoDir {
    char name[128];
    uint32_t extent;
    uint16_t parent;
};

struct IsoFile {
    uint32_t lba;
    uint32_t bytes;
    char path[PATH_MAX_REL];
};

struct Job {
    uint32_t id;
    uint32_t lba;
    uint32_t sectors;
    uint32_t iso_bytes;
    int form;
    char path[PATH_MAX_REL];
};

static int copy_name(char *dst, size_t n, const unsigned char *src, unsigned len) {
    unsigned i;

    if (len == 0 || len >= n) {
        return -1;
    }
    for (i = 0; i < len; i++) {
        unsigned char c = src[i];
        if (c < 0x20 || c > 0x7e) {
            return -1;
        }
        dst[i] = (char)c;
    }
    dst[len] = '\0';
    return 0;
}

static int dir_path(const struct IsoDir *dirs, int ndirs, int k, char *out, size_t n,
                    int depth) {
    char parent[PATH_MAX_REL];
    int p;

    if (k < 0 || k >= ndirs || depth > 32) {
        return -1;
    }
    if (k == 0) {
        out[0] = '\0';
        return 0;
    }
    p = (int)dirs[k].parent - 1;
    if (p < 0 || p == k) {
        return -1;
    }
    if (dir_path(dirs, ndirs, p, parent, sizeof(parent), depth + 1) != 0) {
        return -1;
    }
    if (dirs[k].name[0] == '\0') {
        return -1;
    }
    if (parent[0] == '\0') {
        if (snprintf(out, n, "%s", dirs[k].name) >= (int)n) {
            return -1;
        }
    } else if (snprintf(out, n, "%s/%s", parent, dirs[k].name) >= (int)n) {
        return -1;
    }
    return 0;
}

static int files_push(struct IsoFile **files, size_t *nfiles, size_t *cap, uint32_t lba,
                      uint32_t bytes, const char *path) {
    struct IsoFile *grown;

    if (!path_safe(path)) {
        return 0;
    }
    if (*nfiles >= MAX_FILES) {
        return -1;
    }
    if (*nfiles == *cap) {
        size_t ncap = *cap == 0 ? 64 : *cap * 2;
        if (ncap > MAX_FILES) {
            ncap = MAX_FILES;
        }
        grown = (struct IsoFile *)realloc(*files, ncap * sizeof(**files));
        if (grown == NULL) {
            return -1;
        }
        *files = grown;
        *cap = ncap;
    }
    (*files)[*nfiles].lba = lba;
    (*files)[*nfiles].bytes = bytes;
    snprintf((*files)[*nfiles].path, sizeof((*files)[*nfiles].path), "%s", path);
    (*nfiles)++;
    return 0;
}

static int walk_dir(FILE *fp, const struct IsoDir *dirs, int ndirs, int k,
                    struct IsoFile **files, size_t *nfiles, size_t *cap) {
    unsigned char first[LOGICAL];
    unsigned char *data = NULL;
    char base[PATH_MAX_REL];
    uint32_t dir_size;
    uint32_t alloc_n;
    uint32_t sec_n;
    uint32_t s;
    uint32_t j;
    int rc = -1;

    if (read_user(fp, dirs[k].extent, first) != 0) {
        return -1;
    }
    dir_size = ru32(first + 10);
    if (dir_size == 0 || dir_size > MAX_DIR_BYTES) {
        return -1;
    }
    sec_n = (dir_size + LOGICAL - 1u) / LOGICAL;
    alloc_n = sec_n * LOGICAL;
    data = (unsigned char *)malloc(alloc_n);
    if (data == NULL) {
        return -1;
    }
    for (s = 0; s < sec_n; s++) {
        if (read_user(fp, dirs[k].extent + s, data + s * LOGICAL) != 0) {
            goto out;
        }
    }
    if (dir_path(dirs, ndirs, k, base, sizeof(base), 0) != 0) {
        goto out;
    }
    j = 0;
    while (j < dir_size) {
        unsigned rec_len = data[j];
        unsigned name_len;
        unsigned flags;
        char name[128];
        char full[PATH_MAX_REL];
        uint32_t lba;
        uint32_t nbytes;
        char *semi;

        if (rec_len == 0) {
            j = (j / LOGICAL + 1u) * LOGICAL;
            continue;
        }
        if (rec_len < 34 || j + rec_len > alloc_n) {
            break;
        }
        flags = data[j + 25];
        name_len = data[j + 32];
        if (33u + name_len > rec_len) {
            break;
        }
        lba = ru32(data + j + 2);
        nbytes = ru32(data + j + 10);
        j += rec_len;
        if ((flags & 2u) != 0 ||
            (name_len == 1 && data[j - rec_len + 33] <= 1)) {
            continue;
        }
        if (copy_name(name, sizeof(name), data + (j - rec_len) + 33, name_len) != 0) {
            continue;
        }
        semi = strchr(name, ';');
        if (semi != NULL) {
            *semi = '\0';
        }
        if (name[0] == '\0') {
            continue;
        }
        if (base[0] == '\0') {
            snprintf(full, sizeof(full), "%s", name);
        } else if (snprintf(full, sizeof(full), "%s/%s", base, name) >= (int)sizeof(full)) {
            continue;
        }
        if (files_push(files, nfiles, cap, lba, nbytes, full) != 0) {
            goto out;
        }
    }
    rc = 0;
out:
    free(data);
    return rc;
}

static int walk_iso(FILE *fp, struct IsoFile **files, size_t *nfiles) {
    unsigned char pvd[LOGICAL];
    unsigned char *pt = NULL;
    struct IsoDir *dirs = NULL;
    uint32_t pt_size;
    uint32_t pt_lba;
    uint32_t pt_secs;
    uint32_t pt_alloc;
    uint32_t s;
    uint32_t i;
    int ndirs = 0;
    size_t cap = 0;
    int d;
    int rc = -1;

    *files = NULL;
    *nfiles = 0;
    if (read_user(fp, 16, pvd) != 0 || pvd[0] != 1 || memcmp(pvd + 1, "CD001", 5) != 0) {
        return -1;
    }
    pt_size = ru32(pvd + 132);
    pt_lba = ru32(pvd + 140);
    if (pt_size == 0 || pt_size > MAX_DIR_BYTES || pt_lba == 0) {
        return -1;
    }
    pt_secs = (pt_size + LOGICAL - 1u) / LOGICAL;
    pt_alloc = pt_secs * LOGICAL;
    pt = (unsigned char *)malloc(pt_alloc);
    dirs = (struct IsoDir *)calloc(MAX_DIRS, sizeof(*dirs));
    if (pt == NULL || dirs == NULL) {
        goto out;
    }
    for (s = 0; s < pt_secs; s++) {
        if (read_user(fp, pt_lba + s, pt + s * LOGICAL) != 0) {
            goto out;
        }
    }
    i = 0;
    while (i + 8 <= pt_size && ndirs < MAX_DIRS) {
        unsigned name_len = pt[i];
        unsigned step;
        if (name_len > 127 || i + 8u + name_len > pt_size) {
            break;
        }
        dirs[ndirs].extent = ru32(pt + i + 2);
        dirs[ndirs].parent = ru16(pt + i + 6);
        if (name_len == 1 && pt[i + 8] == 0) {
            dirs[ndirs].name[0] = '\0';
        } else if (copy_name(dirs[ndirs].name, sizeof(dirs[ndirs].name), pt + i + 8,
                             name_len) != 0) {
            goto out;
        }
        ndirs++;
        step = 8u + name_len + (name_len & 1u);
        if (step == 0 || i + step < i) {
            goto out;
        }
        i += step;
    }
    if (ndirs < 1) {
        goto out;
    }
    for (d = 0; d < ndirs; d++) {
        if (walk_dir(fp, dirs, ndirs, d, files, nfiles, &cap) != 0) {
            goto out;
        }
    }
    rc = 0;
out:
    free(pt);
    free(dirs);
    if (rc != 0) {
        free(*files);
        *files = NULL;
        *nfiles = 0;
    }
    return rc;
}

static struct IsoFile *find_lba(struct IsoFile *files, size_t nfiles, uint32_t lba) {
    size_t i;
    for (i = 0; i < nfiles; i++) {
        if (files[i].lba == lba) {
            return &files[i];
        }
    }
    return NULL;
}

static struct IsoFile *find_named_file(struct IsoFile *files, size_t nfiles,
                                       const char *name) {
    size_t i;
    for (i = 0; i < nfiles; i++) {
        const char *base = files[i].path;
        const char *s;
        for (s = files[i].path; *s; s++) {
            if (*s == '/') {
                base = s + 1;
            }
        }
        if (strcasecmp(base, name) == 0) {
            return &files[i];
        }
    }
    return NULL;
}

static int install_span(FILE *bin, const char *dest, unsigned long long off,
                        unsigned long long nbytes, int user_payload) {
    char tmp[1100];
    unsigned char *chunk = NULL;
    FILE *out = NULL;
    unsigned long long done = 0;
    int rc = -1;
    unsigned long long sz = 0;

    if (nbytes == 0) {
        return 0;
    }
    if (file_size(dest, &sz) == 0 && sz == nbytes) {
        return 0;
    }
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", dest) >= (int)sizeof(tmp)) {
        return -1;
    }
    if (mkdir_parent(dest) != 0) {
        return -1;
    }
    chunk = (unsigned char *)malloc(COPY_CHUNK);
    out = fopen(tmp, "wb");
    if (chunk == NULL || out == NULL) {
        goto out;
    }
    if (user_payload) {
        uint32_t lba = (uint32_t)(off / SECTOR_BYTES);
        unsigned long long left = nbytes;
        if (off % SECTOR_BYTES != 0) {
            goto out;
        }
        while (left > 0) {
            unsigned char sec[SECTOR_BYTES];
            size_t n = left > LOGICAL ? LOGICAL : (size_t)left;
            if (seek_read(bin, (unsigned long long)lba * SECTOR_BYTES, sec, SECTOR_BYTES) !=
                0) {
                goto out;
            }
            if (done == 0 && !is_mode2_sync(sec)) {
                goto out;
            }
            if (fwrite(sec + USER_OFF, 1, n, out) != n) {
                goto out;
            }
            left -= n;
            done += n;
            lba++;
            account(n);
        }
    } else {
        if (fseeko(bin, (off_t)off, SEEK_SET) != 0) {
            goto out;
        }
        while (done < nbytes) {
            size_t want = COPY_CHUNK;
            size_t nread;
            if ((unsigned long long)want > nbytes - done) {
                want = (size_t)(nbytes - done);
            }
            nread = fread(chunk, 1, want, bin);
            if (nread != want) {
                goto out;
            }
            if (done == 0 && (nread < 16 || !is_mode2_sync(chunk))) {
                goto out;
            }
            if (fwrite(chunk, 1, nread, out) != nread) {
                goto out;
            }
            done += nread;
            account(nread);
        }
    }
    if (fclose(out) != 0) {
        out = NULL;
        goto out;
    }
    out = NULL;
    if (replace_runtime_file(tmp, dest) != 0) {
        goto out;
    }
    rc = 0;
out:
    if (out != NULL) {
        fclose(out);
    }
    if (rc != 0) {
        remove(tmp);
    }
    free(chunk);
    return rc;
}

static unsigned long long disc_min_bytes(void) {
    unsigned long long min_bytes = 17ull * SECTOR_BYTES;
    int id;

    for (id = 0; id < FILE_COUNT; id++) {
        int lba = FILE_SECTORS[id];
        unsigned sectors = FILE_SECTOR_COUNTS[id];
        unsigned long long end;
        if (lba <= 0 || sectors == 0) {
            continue;
        }
        end = ((unsigned long long)lba + (unsigned long long)sectors) * SECTOR_BYTES;
        if (end > min_bytes) {
            min_bytes = end;
        }
    }
    return min_bytes;
}

static int write_json(const char *root, const struct Job *jobs, int njobs) {
    char dest[1024];
    char tmp[1100];
    FILE *fp;
    int i;
    int first = 1;

    if (join_root(root, RUNTIME_MAP, dest, sizeof(dest)) != 0) {
        return -1;
    }
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", dest) >= (int)sizeof(tmp)) {
        return -1;
    }
    fp = fopen(tmp, "wb");
    if (fp == NULL) {
        return -1;
    }
    if (fprintf(fp,
                "{\n  \"version\": \"%s\",\n  \"executable\": \"%s\",\n"
                "  \"files\": {\n", RUNTIME_VERSION,
                RUNTIME_EXECUTABLE) < 0) {
        fclose(fp);
        remove(tmp);
        return -1;
    }
    for (i = 0; i < njobs; i++) {
        if (!first && fprintf(fp, ",\n") < 0) {
            fclose(fp);
            remove(tmp);
            return -1;
        }
        first = 0;
        if (fprintf(fp,
                    "    \"%u\": {\n"
                    "      \"path\": \"%s\",\n"
                    "      \"lba\": %u,\n"
                    "      \"sectors\": %u,\n"
                    "      \"bytes\": %u,\n"
                    "      \"form\": %d\n"
                    "    }",
                    jobs[i].id, jobs[i].path, jobs[i].lba, jobs[i].sectors,
                    jobs[i].iso_bytes, jobs[i].form) < 0) {
            fclose(fp);
            remove(tmp);
            return -1;
        }
    }
    if (fprintf(fp, "\n  }\n}\n") < 0 || fclose(fp) != 0) {
        remove(tmp);
        return -1;
    }
    if (replace_runtime_file(tmp, dest) != 0) {
        remove(tmp);
        return -1;
    }
    return 0;
}

static int extract_from_bin(const char *root, const char *bin_path) {
    FILE *bin = NULL;
    struct IsoFile *files = NULL;
    struct Job *jobs = NULL;
    struct IsoFile *exe = NULL;
    size_t nfiles = 0;
    int njobs = 0;
    int id;
    int wrote = 0;
    int skipped = 0;
    char msg[1100];
    char dest[1024];
    int rc = -1;

    g_done = 0;
    g_total = 0;
    g_since = 0;
    g_mark = 0;

    bin = fopen(bin_path, "rb");
    if (bin == NULL) {
        dw3_str_restore_emit("cannot open disc image", 0, 0);
        return -1;
    }
    if (walk_iso(bin, &files, &nfiles) != 0) {
        dw3_str_restore_emit("disc image is not Mode2/2352 ISO9660", 0, 0);
        goto out;
    }
    exe = find_named_file(files, nfiles, RUNTIME_EXECUTABLE);
    if (exe == NULL || exe->bytes == 0) {
        if (find_named_file(files, nfiles, RUNTIME_OTHER_EXECUTABLE) != NULL) {
            snprintf(msg, sizeof(msg),
                     "disc region mismatch: expected %s, found %s",
                     RUNTIME_EXECUTABLE, RUNTIME_OTHER_EXECUTABLE);
            dw3_str_restore_emit(msg, 0, 0);
        } else {
            snprintf(msg, sizeof(msg), "%s is not on the disc",
                     RUNTIME_EXECUTABLE);
            dw3_str_restore_emit(msg, 0, 0);
        }
        goto out;
    }
    jobs = (struct Job *)calloc((size_t)FILE_COUNT, sizeof(*jobs));
    if (jobs == NULL) {
        goto out;
    }
    for (id = 0; id < FILE_COUNT; id++) {
        int lba = FILE_SECTORS[id];
        unsigned sectors = FILE_SECTOR_COUNTS[id];
        struct IsoFile *meta;
        unsigned long long cap;
        uint32_t nbytes;
        if (lba <= 0 || sectors == 0) {
            continue;
        }
        meta = find_lba(files, nfiles, (uint32_t)lba);
        if (meta == NULL) {
            continue;
        }
        jobs[njobs].id = (uint32_t)id;
        jobs[njobs].lba = (uint32_t)lba;
        jobs[njobs].sectors = sectors;
        jobs[njobs].form = ends_with_ci(meta->path, ".str") ? 2 : 1;
        snprintf(jobs[njobs].path, sizeof(jobs[njobs].path), "%s", meta->path);
        if (jobs[njobs].form == 2) {
            jobs[njobs].iso_bytes = meta->bytes;
        } else {
            cap = (unsigned long long)sectors * LOGICAL;
            nbytes = meta->bytes;
            if ((unsigned long long)nbytes > cap) {
                nbytes = (uint32_t)cap;
            }
            jobs[njobs].iso_bytes = nbytes;
        }
        njobs++;
    }
    if (njobs == 0) {
        dw3_str_restore_emit("FILE_SECTORS matched no ISO paths", 0, 0);
        goto out;
    }

    if (mkdir_p(root) != 0) {
        goto out;
    }
    for (id = 0; id < njobs; id++) {
        unsigned long long expect;
        unsigned long long sz = 0;
        if (join_root(root, jobs[id].path, dest, sizeof(dest)) != 0) {
            goto out;
        }
        if (jobs[id].form == 2) {
            expect = (unsigned long long)jobs[id].sectors * SECTOR_BYTES;
        } else {
            expect = jobs[id].iso_bytes;
        }
        if (expect > 0 && file_size(dest, &sz) == 0 && sz == expect) {
            skipped++;
            continue;
        }
        g_total += expect;
    }
    if (join_root(root, RUNTIME_EXECUTABLE, dest, sizeof(dest)) != 0) {
        goto out;
    }
    {
        unsigned long long sz = 0;
        if (!(file_size(dest, &sz) == 0 && sz == exe->bytes)) {
            g_total += exe->bytes;
        } else {
            skipped++;
        }
    }

    snprintf(msg, sizeof(msg), "extracting runtime from %s", bin_path);
    dw3_str_restore_emit(msg, 0, g_total);

    for (id = 0; id < njobs; id++) {
        unsigned long long expect;
        unsigned long long sz = 0;
        int raw;
        if (join_root(root, jobs[id].path, dest, sizeof(dest)) != 0) {
            goto out;
        }
        raw = jobs[id].form == 2;
        expect = raw ? (unsigned long long)jobs[id].sectors * SECTOR_BYTES
                     : jobs[id].iso_bytes;
        if (file_size(dest, &sz) == 0 && sz == expect) {
            continue;
        }
        if (expect == 0) {
            FILE *empty;
            if (mkdir_parent(dest) != 0) {
                goto out;
            }
            empty = fopen(dest, "wb");
            if (empty == NULL) {
                goto out;
            }
            fclose(empty);
            wrote++;
            continue;
        }
        if (install_span(bin, dest, (unsigned long long)jobs[id].lba * SECTOR_BYTES,
                         expect, raw ? 0 : 1) != 0) {
            dw3_str_restore_emit("extract write failed", g_done, g_total);
            goto out;
        }
        wrote++;
    }
    if (join_root(root, RUNTIME_EXECUTABLE, dest, sizeof(dest)) != 0) {
        goto out;
    }
    {
        unsigned long long sz = 0;
        if (!(file_size(dest, &sz) == 0 && sz == exe->bytes)) {
            if (install_span(bin, dest, (unsigned long long)exe->lba * SECTOR_BYTES,
                             exe->bytes, 1) != 0) {
                dw3_str_restore_emit("executable extract failed", g_done, g_total);
                goto out;
            }
            wrote++;
        }
    }
    pump(1);
    if (join_root(root, "boot_tim.bin", dest, sizeof(dest)) == 0) {
        char exe_path[1024];
        if (join_root(root, RUNTIME_EXECUTABLE, exe_path, sizeof(exe_path)) != 0 ||
            write_boot_tim(exe_path, dest) != 0) {
            snprintf(msg, sizeof(msg), "boot TIM tail missing in %s",
                     RUNTIME_EXECUTABLE);
            dw3_str_restore_emit(msg, g_done, g_total);
            goto out;
        }
    }
    if (write_json(root, jobs, njobs) != 0) {
        goto out;
    }
    snprintf(msg, sizeof(msg), "runtime extract wrote=%d skipped=%d", wrote, skipped);
    dw3_str_restore_emit(msg, g_done, g_total);
    rc = 0;
out:
    free(jobs);
    free(files);
    if (bin != NULL) {
        fclose(bin);
    }
    return rc;
}

int dw3_runtime_ensure(const char *root) {
    char bin[1024];

    if (root == NULL || root[0] == '\0' || strlen(root) >= 480) {
        return -1;
    }
    (void)try_local_boot_tim(root);
    if (tree_ready(root)) {
        return ensure_memcard(root);
    }
#if defined(VERSION_EU) && VERSION_EU
    /* The EU image is expected beside its runtime root (or in its parent).
     * files_eu.json is written last and is the completion marker: until it
     * validates against the SLES executable and every mapped file, boot must
     * not hand the partial tree to CDFS. Keeping the disc beside the extracted
     * files also avoids moving/copying a 600+ MiB tree on the SD card. */
    if (dw3_str_find_disc(root, bin, sizeof(bin), disc_min_bytes()) != 0) {
        if (dir_exists(root)) {
            dw3_str_restore_emit(
                "EU runtime incomplete; place the matching EU BIN/CUE beside it",
                0, 0);
            return -1;
        }
        return 0;
    }
    if (extract_from_bin(root, bin) != 0) {
        return -1;
    }
    if (!tree_ready(root)) {
        dw3_str_restore_emit("EU runtime validation failed; map not accepted", 0, 0);
        return -1;
    }
    return ensure_memcard(root);
#else
    if (FILE_COUNT < 1 || FILE_COUNT > MAX_FILES) {
        if (dir_exists(root)) {
            return ensure_memcard(root);
        }
        return 0;
    }
    if (dw3_str_find_disc(root, bin, sizeof(bin), disc_min_bytes()) != 0) {
        if (dir_exists(root)) {
            return ensure_memcard(root);
        }
        return 0;
    }
    if (extract_from_bin(root, bin) != 0) {
        return -1;
    }
    return ensure_memcard(root);
#endif
}
