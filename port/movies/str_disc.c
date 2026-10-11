#include "str_disc.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define STR_RAW_BYTES 2352

static int file_size(const char *path, unsigned long long *out) {
    struct stat st;

    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return -1;
    }
    *out = (unsigned long long)st.st_size;
    return 0;
}

static int bin_is_image(const char *path, unsigned long long min_bytes) {
    unsigned long long sz;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    if (file_size(path, &sz) != 0) {
        return 0;
    }
    if (sz < STR_RAW_BYTES) {
        return 0;
    }
    if (min_bytes > 0 && sz < min_bytes) {
        return 0;
    }
    return 1;
}

static int is_boot_tim(const char *name) {
    return strcasecmp(name, "boot_tim.bin") == 0;
}

static void dirname_of(const char *path, char *out, size_t n) {
    const char *slash = strrchr(path, '/');
    const char *bslash = strrchr(path, '\\');
    const char *sep = slash;
    size_t len;

    if (bslash != NULL && (sep == NULL || bslash > sep)) {
        sep = bslash;
    }
    if (sep == NULL) {
        snprintf(out, n, ".");
        return;
    }
    len = (size_t)(sep - path);
    if (len == 0) {
        snprintf(out, n, "/");
        return;
    }
    if (len >= n) {
        len = n - 1;
    }
    memcpy(out, path, len);
    out[len] = '\0';
}

static int is_absolute_path(const char *path) {
    if (path[0] == '/' || path[0] == '\\') {
        return 1;
    }
    /* sdmc:/... or C:/... */
    if (strchr(path, ':') != NULL) {
        return 1;
    }
    return 0;
}

static int join_dir(const char *dir, const char *name, char *out, size_t n) {
    size_t ld = strlen(dir);
    int need_slash = ld > 0 && dir[ld - 1] != '/' && dir[ld - 1] != '\\';

    if (is_absolute_path(name)) {
        snprintf(out, n, "%s", name);
        return 0;
    }
    if (snprintf(out, n, "%s%s%s", dir, need_slash ? "/" : "", name) >= (int)n) {
        return -1;
    }
    return 0;
}

/* FILE "name" BINARY  or  FILE name BINARY. Writes the bin path next to the cue. */
static int bin_from_cue(const char *cue_path, char *out, size_t n) {
    FILE *fp;
    char line[512];
    char dir[512];
    char name[256];

    fp = fopen(cue_path, "rb");
    if (fp == NULL) {
        return -1;
    }
    name[0] = '\0';
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *p = line;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (strncasecmp(p, "FILE", 4) != 0 || (p[4] != ' ' && p[4] != '\t')) {
            continue;
        }
        p += 4;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '"') {
            char *end;
            p++;
            end = strchr(p, '"');
            if (end == NULL || (size_t)(end - p) >= sizeof(name)) {
                fclose(fp);
                return -1;
            }
            memcpy(name, p, (size_t)(end - p));
            name[end - p] = '\0';
        } else {
            size_t i = 0;
            while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' &&
                   i + 1 < sizeof(name)) {
                name[i++] = *p++;
            }
            name[i] = '\0';
        }
        break;
    }
    fclose(fp);
    if (name[0] == '\0') {
        return -1;
    }
    dirname_of(cue_path, dir, sizeof(dir));
    return join_dir(dir, name, out, n);
}

static int ext_is(const char *name, const char *ext) {
    size_t n = strlen(name);
    size_t e = strlen(ext);
    size_t i;

    if (n < e) {
        return 0;
    }
    for (i = 0; i < e; i++) {
        if (tolower((unsigned char)name[n - e + i]) != tolower((unsigned char)ext[i])) {
            return 0;
        }
    }
    return 1;
}

/* Prefer disc.cue, then the only *.cue, then disc.bin, then the only *.bin.
 * boot_tim.bin is skipped. A cue whose FILE is missing falls through. */
static int disc_in_dir(const char *dir, char *out, size_t n, unsigned long long min_bytes) {
    char cue_named[1024];
    char bin_named[1024];
    char only_cue[1024];
    char only_bin[1024];
    char resolved[1024];
    int cues = 0;
    int bins = 0;
    DIR *d;
    struct dirent *ent;

    snprintf(cue_named, sizeof(cue_named), "%s/disc.cue", dir);
    if (bin_from_cue(cue_named, resolved, sizeof(resolved)) == 0 &&
        bin_is_image(resolved, min_bytes)) {
        snprintf(out, n, "%s", resolved);
        return 0;
    }
    snprintf(bin_named, sizeof(bin_named), "%s/disc.bin", dir);

    d = opendir(dir);
    if (d == NULL) {
        if (bin_is_image(bin_named, min_bytes)) {
            snprintf(out, n, "%s", bin_named);
            return 0;
        }
        return -1;
    }
    only_cue[0] = '\0';
    only_bin[0] = '\0';
    while ((ent = readdir(d)) != NULL) {
        char full[1024];
        if (ent->d_name[0] == '.') {
            continue;
        }
        if (snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name) >= (int)sizeof(full)) {
            continue;
        }
        if (ext_is(ent->d_name, ".cue")) {
            cues++;
            snprintf(only_cue, sizeof(only_cue), "%s", full);
        } else if (ext_is(ent->d_name, ".bin") && !is_boot_tim(ent->d_name)) {
            bins++;
            snprintf(only_bin, sizeof(only_bin), "%s", full);
        }
    }
    closedir(d);

    if (cues == 1 && bin_from_cue(only_cue, resolved, sizeof(resolved)) == 0 &&
        bin_is_image(resolved, min_bytes)) {
        snprintf(out, n, "%s", resolved);
        return 0;
    }
    if (bin_is_image(bin_named, min_bytes)) {
        snprintf(out, n, "%s", bin_named);
        return 0;
    }
    if (bins == 1 && bin_is_image(only_bin, min_bytes)) {
        snprintf(out, n, "%s", only_bin);
        return 0;
    }
    return -1;
}

int dw3_str_find_disc(const char *runtime_root, char *out, size_t n,
                      unsigned long long min_bytes) {
    const char *env;
    char parent[512];
    char resolved[1024];

    env = getenv("DW3_DISC_BIN");
    if (bin_is_image(env, min_bytes)) {
        snprintf(out, n, "%s", env);
        return 0;
    }
    env = getenv("DW3_DISC_CUE");
    if (env != NULL && env[0] != '\0' && bin_from_cue(env, resolved, sizeof(resolved)) == 0 &&
        bin_is_image(resolved, min_bytes)) {
        snprintf(out, n, "%s", resolved);
        return 0;
    }

    if (runtime_root == NULL || runtime_root[0] == '\0') {
        return -1;
    }
    if (disc_in_dir(runtime_root, out, n, min_bytes) == 0) {
        return 0;
    }
    dirname_of(runtime_root, parent, sizeof(parent));
    if (parent[0] != '\0' && strcmp(parent, ".") != 0 && strcmp(parent, runtime_root) != 0) {
        if (disc_in_dir(parent, out, n, min_bytes) == 0) {
            return 0;
        }
    }
    return -1;
}
