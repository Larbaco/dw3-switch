/* MOD loader: manifest, pack discovery, validated PNG loads. See mod_pack.h. */
#include "mod_pack.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The vendored decoder lives in png_stb.c (its own warnings policy). */
extern unsigned char *dw3_mod_stb_decode(const unsigned char *data, size_t len,
                                         int *w, int *h);
extern void dw3_mod_stb_free(void *p);

enum { MOD_MANIFEST_MAX = 65536, MOD_PNG_MAX = 64 * 1024 * 1024, MOD_PACKS_MAX = 64 };

static void set_why(char *why, size_t cap, const char *fmt, const char *a,
                    long b) {
    if (why && cap) snprintf(why, cap, fmt, a ? a : "", b);
}

/* --- the JSON subset -------------------------------------------------------*/

typedef struct Jp {
    const char *p, *end;
} Jp;

static void j_ws(Jp *j) {
    while (j->p < j->end && (*j->p == ' ' || *j->p == '\t' || *j->p == '\n' ||
                             *j->p == '\r')) ++j->p;
}

/* Skips one string (j->p on the opening quote); returns 0, or -1. */
static int j_skip_string(Jp *j) {
    if (j->p >= j->end || *j->p != '"') return -1;
    ++j->p;
    while (j->p < j->end && *j->p != '"') {
        if (*j->p == '\\') ++j->p;
        ++j->p;
    }
    if (j->p >= j->end) return -1;
    ++j->p;
    return 0;
}

static int j_skip_value(Jp *j, int depth) {
    j_ws(j);
    if (j->p >= j->end || depth > 16) return -1;
    if (*j->p == '"') return j_skip_string(j);
    if (*j->p == '{' || *j->p == '[') {
        char close = *j->p == '{' ? '}' : ']';

        ++j->p;
        j_ws(j);
        if (j->p < j->end && *j->p == close) {
            ++j->p;
            return 0;
        }
        for (;;) {
            if (close == '}') {
                if (j_skip_string(j) != 0) return -1;
                j_ws(j);
                if (j->p >= j->end || *j->p != ':') return -1;
                ++j->p;
            }
            if (j_skip_value(j, depth + 1) != 0) return -1;
            j_ws(j);
            if (j->p >= j->end) return -1;
            if (*j->p == ',') {
                ++j->p;
                j_ws(j);
                continue;
            }
            if (*j->p == close) {
                ++j->p;
                return 0;
            }
            return -1;
        }
    }
    /* number, true, false, null */
    while (j->p < j->end && *j->p != ',' && *j->p != '}' && *j->p != ']' &&
           *j->p != ' ' && *j->p != '\n' && *j->p != '\r' && *j->p != '\t') ++j->p;
    return 0;
}

/* Finds the value of `path` ("a.b") in the object text; *val points at the
 * value's first byte. Returns 0 when found, 1 when absent, -1 when the text is
 * not valid enough to tell. */
static int j_find(const char *s, size_t len, const char *path, const char **val,
                  const char **vend) {
    Jp j;
    const char *dot = strchr(path, '.');
    size_t seg = dot ? (size_t)(dot - path) : strlen(path);

    j.p = s;
    j.end = s + len;
    j_ws(&j);
    if (j.p >= j.end || *j.p != '{') return -1;
    ++j.p;
    j_ws(&j);
    if (j.p < j.end && *j.p == '}') return 1;
    for (;;) {
        const char *k0, *k1;
        int match;

        j_ws(&j);
        k0 = j.p;
        if (j_skip_string(&j) != 0) return -1;
        k1 = j.p - 1;
        match = (size_t)(k1 - (k0 + 1)) == seg &&
                strncmp(k0 + 1, path, seg) == 0;
        j_ws(&j);
        if (j.p >= j.end || *j.p != ':') return -1;
        ++j.p;
        j_ws(&j);
        if (match) {
            const char *v0 = j.p;

            if (j_skip_value(&j, 0) != 0) return -1;
            if (dot) return j_find(v0, (size_t)(j.p - v0), dot + 1, val, vend);
            *val = v0;
            *vend = j.p;
            return 0;
        }
        if (j_skip_value(&j, 0) != 0) return -1;
        j_ws(&j);
        if (j.p >= j.end) return -1;
        if (*j.p == ',') {
            ++j.p;
            continue;
        }
        if (*j.p == '}') return 1;
        return -1;
    }
}

static int j_string(const char *s, size_t len, const char *path, char *out,
                    size_t cap) {
    const char *v, *e;
    size_t n;

    if (j_find(s, len, path, &v, &e) != 0 || *v != '"' || e - v < 2) return -1;
    n = (size_t)(e - v - 2);
    if (n >= cap) n = cap - 1;
    memcpy(out, v + 1, n);
    out[n] = 0;
    return 0;
}

static int j_int(const char *s, size_t len, const char *path, int *out) {
    const char *v, *e;
    char buf[24];
    size_t n;
    char *end;
    long val;

    if (j_find(s, len, path, &v, &e) != 0) return -1;
    n = (size_t)(e - v);
    if (n == 0 || n >= sizeof(buf)) return -1;
    memcpy(buf, v, n);
    buf[n] = 0;
    val = strtol(buf, &end, 10);
    if (end == buf || *end != 0) return -1;
    *out = (int)val;
    return 0;
}

static int j_true(const char *s, size_t len, const char *path) {
    const char *v, *e;

    return j_find(s, len, path, &v, &e) == 0 && e - v == 4 &&
           strncmp(v, "true", 4) == 0;
}

/* Does the array under `path` hold the string / number `item`? */
static int j_array_has(const char *s, size_t len, const char *path,
                       const char *item) {
    const char *v, *e, *p;
    size_t n = strlen(item);

    if (j_find(s, len, path, &v, &e) != 0 || *v != '[') return 0;
    for (p = v + 1; p < e; ++p) {
        if (*p == '"') {
            if ((size_t)(e - p) > n + 1 && strncmp(p + 1, item, n) == 0 &&
                p[1 + n] == '"') return 1;
            ++p;
            while (p < e && *p != '"') ++p;
        } else if (*p >= '0' && *p <= '9') {
            if (strncmp(p, item, n) == 0 && !(p[n] >= '0' && p[n] <= '9')) return 1;
            while (p < e && *p >= '0' && *p <= '9') ++p;
        }
    }
    return 0;
}

int dw3_mod_manifest_parse(const char *json, size_t len, Dw3ModManifest *out,
                           char *why, size_t why_cap) {
    int n, scale;
    char key[4];
    const char *v0, *v1;

    memset(out, 0, sizeof(*out));
    if (!json || len == 0 || len > MOD_MANIFEST_MAX) {
        set_why(why, why_cap, "mod.json is empty or too large", NULL, 0);
        return -1;
    }
    if (j_find(json, len, "format", &v0, &v1) < 0) {
        set_why(why, why_cap, "mod.json is not valid JSON", NULL, 0);
        return -1;
    }
    if (j_int(json, len, "format", &n) != 0) {
        set_why(why, why_cap, "mod.json has no \"format\"", NULL, 0);
        return -1;
    }
    if (n > DW3_MOD_FORMAT || n < 1) {
        if (why && why_cap) {
            snprintf(why, why_cap, "mod.json format %d is not supported (loader reads %d)",
                     n, DW3_MOD_FORMAT);
        }
        return -1;
    }
    out->format = n;
    if (j_string(json, len, "id", out->id, sizeof(out->id)) != 0 || !out->id[0]) {
        set_why(why, why_cap, "mod.json has no \"id\"", NULL, 0);
        return -1;
    }
    j_string(json, len, "name", out->name, sizeof(out->name));
    j_string(json, len, "version", out->version, sizeof(out->version));
    j_string(json, len, "generator", out->generator, sizeof(out->generator));
    j_string(json, len, "map.filter", out->filter, sizeof(out->filter));
    out->region_us = j_array_has(json, len, "regions", "us");
    out->region_eu = j_array_has(json, len, "regions", "eu");
    for (scale = DW3_MOD_SCALE_MIN; scale <= DW3_MOD_SCALE_MAX; ++scale) {
        snprintf(key, sizeof(key), "%d", scale);
        if (j_array_has(json, len, "map.scales", key)) out->scales |= 1u << scale;
    }
    out->reveal = j_true(json, len, "map.reveal");
    return 0;
}

/* --- files -----------------------------------------------------------------*/

static unsigned char *read_file(const char *path, size_t max, size_t *len,
                                char *why, size_t why_cap) {
    FILE *f = fopen(path, "rb");
    long n;
    unsigned char *buf;

    if (!f) {
        set_why(why, why_cap, "cannot open %s", path, 0);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || (size_t)n > max) {
        fclose(f);
        set_why(why, why_cap, "%s is empty or too large", path, 0);
        return NULL;
    }
    buf = (unsigned char *)malloc((size_t)n);
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        set_why(why, why_cap, "cannot read %s", path, 0);
        return NULL;
    }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

void dw3_mod_image_free(Dw3ModImage *img) {
    if (img) {
        dw3_mod_stb_free(img->rgba);
        img->rgba = NULL;
        img->w = img->h = 0;
    }
}

int dw3_mod_png_load(const char *path, Dw3ModImage *out, char *why,
                     size_t why_cap) {
    size_t len;
    unsigned char *file, *px;
    int w = 0, h = 0;

    out->rgba = NULL;
    out->w = out->h = 0;
    file = read_file(path, MOD_PNG_MAX, &len, why, why_cap);
    if (!file) return -1;
    px = dw3_mod_stb_decode(file, len, &w, &h);
    free(file);
    if (!px) {
        set_why(why, why_cap, "%s is not a valid PNG", path, 0);
        return -1;
    }
    if (w <= 0 || h <= 0 || w > DW3_MOD_MAX_DIM || h > DW3_MOD_MAX_DIM) {
        dw3_mod_stb_free(px);
        set_why(why, why_cap, "%s is too large", path, 0);
        return -1;
    }
    out->rgba = px;
    out->w = w;
    out->h = h;
    return 0;
}

/* --- discovery -------------------------------------------------------------*/

static int pack_open(const char *mods_dir, const char *name, const char *region,
                     Dw3ModPack *out, char *why, size_t why_cap) {
    char path[640];
    size_t len;
    unsigned char *json;
    int rc;

    if (strlen(mods_dir) > 400 || strlen(name) > 100) {
        set_why(why, why_cap, "pack path too long: %s", name, 0);
        return -1;
    }
    snprintf(out->dir, sizeof(out->dir), "%.400s/%.100s", mods_dir, name);
    snprintf(path, sizeof(path), "%.510s/mod.json", out->dir);
    json = read_file(path, MOD_MANIFEST_MAX, &len, why, why_cap);
    if (!json) return -1;
    rc = dw3_mod_manifest_parse((const char *)json, len, &out->man, why, why_cap);
    free(json);
    if (rc != 0) return -1;
    if (!(strcmp(region, "eu") == 0 ? out->man.region_eu : out->man.region_us)) {
        set_why(why, why_cap, "pack does not list region %s", region, 0);
        return -1;
    }
    return 0;
}

static int cmp_names(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

int dw3_mod_pack_find(const char *mods_dir, const char *want, const char *region,
                      Dw3ModPack *out, char *why, size_t why_cap) {
    DIR *d;
    struct dirent *e;
    char names[MOD_PACKS_MAX][96];
    char first_why[DW3_MOD_WHY];
    int n = 0, i;

    memset(out, 0, sizeof(*out));
    first_why[0] = 0;
    if (!mods_dir || !mods_dir[0]) {
        set_why(why, why_cap, "no mods folder configured", NULL, 0);
        return -1;
    }
    if (want && want[0]) {
        if (pack_open(mods_dir, want, region, out, first_why, sizeof(first_why)) == 0)
            return 0;
        if (why && why_cap) {
            snprintf(why, why_cap, "pack \"%.40s\": %.100s", want, first_why);
        }
        return -1;
    }
    d = opendir(mods_dir);
    if (!d) {
        set_why(why, why_cap, "no mods folder%s", "", 0);
        return -1;
    }
    while ((e = readdir(d)) != NULL && n < MOD_PACKS_MAX) {
        if (e->d_name[0] == '.' || strlen(e->d_name) >= sizeof(names[0])) continue;
        strcpy(names[n++], e->d_name);
    }
    closedir(d);
    qsort(names, (size_t)n, sizeof(names[0]), cmp_names);
    for (i = 0; i < n; ++i) {
        char one[DW3_MOD_WHY];

        one[0] = 0;
        if (pack_open(mods_dir, names[i], region, out, one, sizeof(one)) == 0)
            return 0;
        if (!first_why[0] && one[0]) {
            snprintf(first_why, sizeof(first_why), "pack \"%.40s\": %.100s",
                     names[i], one);
        }
    }
    if (why && why_cap) {
        snprintf(why, why_cap, "%s", first_why[0] ? first_why : "no packs in the mods folder");
    }
    return -1;
}

/* --- the art -----------------------------------------------------------------*/

int dw3_mod_load_map(const Dw3ModPack *pack, const char *region, int half,
                     int src_w, int src_h, int max_scale, Dw3ModImage *out,
                     int *scale, char *why, size_t why_cap) {
    int n;
    char first[DW3_MOD_WHY];

    first[0] = 0;
    out->rgba = NULL;
    out->w = out->h = 0;
    if (max_scale > DW3_MOD_SCALE_MAX) max_scale = DW3_MOD_SCALE_MAX;
    for (n = max_scale; n >= DW3_MOD_SCALE_MIN; --n) {
        char path[640];
        char one[DW3_MOD_WHY];
        int i;

        if (!(pack->man.scales & (1u << n))) continue;
        snprintf(path, sizeof(path), "%.400s/map/map_%.4s_h%d@%dx.png", pack->dir,
                 region, half, n);
        one[0] = 0;
        if (dw3_mod_png_load(path, out, one, sizeof(one)) != 0) {
            if (!first[0]) snprintf(first, sizeof(first), "%s", one);
            continue;
        }
        if (out->w != src_w * n || out->h != src_h * n) {
            if (!first[0]) {
                snprintf(first, sizeof(first),
                         "map_%s_h%d@%dx.png is %dx%d, expected %dx%d", region,
                         half, n, out->w, out->h, src_w * n, src_h * n);
            }
            dw3_mod_image_free(out);
            continue;
        }
        for (i = 3; i < out->w * out->h * 4; i += 4) out->rgba[i] = 255;
        *scale = n;
        return 0;
    }
    if (why && why_cap) {
        snprintf(why, why_cap, "%s", first[0] ? first : "pack has no map scale in 2..4");
    }
    return -1;
}

int dw3_mod_load_sprite(const Dw3ModPack *pack, const char *region, int half,
                        const char *kind, int frame, int scale, int w, int h,
                        Dw3ModImage *out, char *why, size_t why_cap) {
    char path[700];

    snprintf(path, sizeof(path), "%.400s/map/reveal/%.4s_h%d/%.8s_%d@%dx.png", pack->dir,
             region, half, kind, frame, scale);
    if (dw3_mod_png_load(path, out, why, why_cap) != 0) return -1;
    if (out->w != w * scale || out->h != h * scale) {
        if (why && why_cap) {
            snprintf(why, why_cap, "%s_%d@%dx.png is %dx%d, expected %dx%d", kind,
                     frame, scale, out->w, out->h, w * scale, h * scale);
        }
        dw3_mod_image_free(out);
        return -1;
    }
    return 0;
}

void dw3_mod_blend(Dw3ModImage *dst, const Dw3ModImage *src, int x, int y) {
    int sy, sx;

    if (!dst || !src || !dst->rgba || !src->rgba) return;
    for (sy = 0; sy < src->h; ++sy) {
        int dy = y + sy;
        const uint8_t *s;
        uint8_t *d;

        if (dy < 0 || dy >= dst->h) continue;
        s = src->rgba + (size_t)sy * (size_t)src->w * 4;
        for (sx = 0; sx < src->w; ++sx, s += 4) {
            int dx = x + sx, a = s[3], k;

            if (dx < 0 || dx >= dst->w || a == 0) continue;
            d = dst->rgba + ((size_t)dy * (size_t)dst->w + (size_t)dx) * 4;
            if (a == 255) {
                d[0] = s[0];
                d[1] = s[1];
                d[2] = s[2];
            } else {
                for (k = 0; k < 3; ++k) {
                    d[k] = (uint8_t)((s[k] * a + d[k] * (255 - a) + 127) / 255);
                }
            }
            d[3] = 255;
        }
    }
}
