/* Disc images waiting on the card; see dw3_disc.h (issue #322). */
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64

#include "dw3_disc.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define SECTOR_RAW 2352u
#define USER_OFF 24u
#define LOGICAL 2048u
#define PVD_LBA 16u
#define ROOT_DIR_MAX (64u * 1024u)
#define MAX_IMAGES 16

static uint32_t le32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int read_user(FILE *fp, uint32_t lba, unsigned char *out) {
    long long off = (long long)lba * SECTOR_RAW + USER_OFF;

    if (fseeko(fp, (off_t)off, SEEK_SET) != 0) {
        return -1;
    }
    return fread(out, 1, LOGICAL, fp) == LOGICAL ? 0 : -1;
}

static int ext_is(const char *name, const char *ext) {
    size_t n = strlen(name), e = strlen(ext);

    return n > e && strcasecmp(name + n - e, ext) == 0;
}

Dw3Region dw3_disc_region_of(const char *bin_path) {
    unsigned char sec[LOGICAL];
    FILE *fp;
    uint32_t lba, size, s;
    Dw3Region found = DW3_REGION_NONE;

    if (bin_path == NULL || (fp = fopen(bin_path, "rb")) == NULL) {
        return DW3_REGION_NONE;
    }
    if (read_user(fp, PVD_LBA, sec) != 0 || sec[0] != 1 || memcmp(sec + 1, "CD001", 5) != 0) {
        fclose(fp);
        return DW3_REGION_NONE;
    }
    lba = le32(sec + 156 + 2);
    size = le32(sec + 156 + 10);
    if (lba == 0 || size == 0 || size > ROOT_DIR_MAX) {
        fclose(fp);
        return DW3_REGION_NONE;
    }
    for (s = 0; s * LOGICAL < size && found == DW3_REGION_NONE; s++) {
        unsigned i = 0;

        if (read_user(fp, lba + s, sec) != 0) {
            break;
        }
        while (i + 34u <= LOGICAL && sec[i] != 0) {
            unsigned rl = sec[i], nl = sec[i + 32];
            char name[40];

            if (rl < 34u || i + rl > LOGICAL || 33u + nl > rl) {
                break;
            }
            if (nl > 0 && nl < sizeof(name)) {
                char *semi;

                memcpy(name, sec + i + 33, nl);
                name[nl] = '\0';
                semi = strchr(name, ';');
                if (semi != NULL) {
                    *semi = '\0';
                }
                if (strcasecmp(name, "SLUS_014.36") == 0) {
                    found = DW3_REGION_US;
                    break;
                }
                if (strcasecmp(name, "SLES_039.36") == 0) {
                    found = DW3_REGION_EU;
                    break;
                }
            }
            i += rl;
        }
    }
    fclose(fp);
    return found;
}

/* The BIN a CUE names (FILE "x.bin" BINARY), joined to the CUE's folder. */
static int bin_from_cue(const char *dir, const char *cue_path, char *out, size_t n) {
    FILE *fp = fopen(cue_path, "rb");
    char line[512], name[256];

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
            char *end = strchr(++p, '"');

            if (end != NULL && (size_t)(end - p) < sizeof(name)) {
                memcpy(name, p, (size_t)(end - p));
                name[end - p] = '\0';
            }
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
    return snprintf(out, n, "%s/%s", dir, name) < (int)n ? 0 : -1;
}

static void copy(char *dst, size_t n, const char *src) {
    size_t l = strlen(src);

    if (l >= n) {
        l = n - 1;
    }
    memcpy(dst, src, l);
    dst[l] = '\0';
}

static int is_file(const char *path) {
    struct stat st;

    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* Any image in <base>/<r>/: the folder the runtime extraction already reads. */
static void scan_region_folder(const char *base, Dw3Region r, Dw3DiscHit *hit) {
    char dir[DW3_DISC_PATH];
    DIR *d;
    struct dirent *ent;

    snprintf(dir, sizeof(dir), "%s/%s", base, dw3_region_name(r));
    d = opendir(dir);
    if (d == NULL) {
        return;
    }
    while ((ent = readdir(d)) != NULL) {
        if (strcasecmp(ent->d_name, "boot_tim.bin") == 0) {
            continue;
        }
        if (ext_is(ent->d_name, ".cue") || ext_is(ent->d_name, ".bin") ||
            ext_is(ent->d_name, ".iso")) {
            hit->found = 1;
            hit->in_parent = 0;
            copy(hit->path, sizeof(hit->path), dir);
            strncat(hit->path, "/", sizeof(hit->path) - strlen(hit->path) - 1);
            strncat(hit->path, ent->d_name, sizeof(hit->path) - strlen(hit->path) - 1);
            break;
        }
    }
    closedir(d);
}

static void consider(Dw3DiscScan *out, const Dw3RegionProbe *tree, const char *bin) {
    Dw3Region r = dw3_disc_region_of(bin);
    Dw3DiscHit *h;

    if (r == DW3_REGION_NONE) {
        if (out->unidentified++ == 0) {
            copy(out->unidentified_first, sizeof(out->unidentified_first), bin);
        }
        return;
    }
    if (r == DW3_REGION_US ? tree->has_us : tree->has_eu) {
        return; /* that region is complete already */
    }
    h = &out->hit[r - DW3_REGION_US];
    if (h->found && !h->in_parent) {
        return; /* an image in <base>/<r>/ takes precedence */
    }
    /* among several, the smallest name wins: no dependence on readdir order */
    if (!h->found || strcmp(bin, h->path) < 0) {
        h->found = 1;
        h->in_parent = 1;
        copy(h->path, sizeof(h->path), bin);
    }
}

void dw3_disc_scan(const char *base, const Dw3RegionProbe *tree, Dw3DiscScan *out) {
    DIR *d;
    struct dirent *ent;
    char bins[MAX_IMAGES][DW3_DISC_PATH];
    int nbins = 0, i, j;

    memset(out, 0, sizeof(*out));
    if (!tree->has_us) {
        scan_region_folder(base, DW3_REGION_US, &out->hit[0]);
    }
    if (!tree->has_eu) {
        scan_region_folder(base, DW3_REGION_EU, &out->hit[1]);
    }
    if (tree->has_us && tree->has_eu) {
        return;
    }
    d = opendir(base);
    if (d == NULL) {
        return;
    }
    while ((ent = readdir(d)) != NULL && nbins < MAX_IMAGES) {
        char full[DW3_DISC_PATH], bin[DW3_DISC_PATH];
        int dup = 0;

        if (ent->d_name[0] == '.' || strcasecmp(ent->d_name, "boot_tim.bin") == 0) {
            continue;
        }
        if (snprintf(full, sizeof(full), "%s/%s", base, ent->d_name) >= (int)sizeof(full)) {
            continue;
        }
        if (ext_is(ent->d_name, ".bin")) {
            copy(bin, sizeof(bin), full);
        } else if (!ext_is(ent->d_name, ".cue") || bin_from_cue(base, full, bin, sizeof(bin)) != 0) {
            continue;
        }
        if (!is_file(bin)) {
            continue;
        }
        for (j = 0; j < nbins; j++) {
            dup |= strcmp(bins[j], bin) == 0;
        }
        if (!dup) {
            copy(bins[nbins++], DW3_DISC_PATH, bin);
        }
    }
    closedir(d);
    for (i = 0; i < nbins; i++) {
        consider(out, tree, bins[i]);
    }
}
