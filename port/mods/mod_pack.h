/* MOD loader (docs/MOD_MAP_UPSCALE.md): packs of replacement art under
 * <mods dir>/<pack>/ (Switch sdmc:/switch/dw3/mods, PC <config dir>/mods).
 * Pure C: stdio, dirent and the vendored stb_image PNG decoder only, so the
 * PC and Switch hosts share it and tools/tests can build it with a plain
 * compiler. A pack is derived data made on the user's machine from their own
 * disc; nothing here embeds or ships any of it.
 *
 * Layout (manifest format 1):
 *   <pack>/mod.json
 *   <pack>/map/map_<region>_h<half>@<N>x.png            the bare map, N times
 *                                                       the source size
 *   <pack>/map/reveal/<region>_h<half>/<kind>_<id>@<N>x.png
 *       one reveal sprite (kind towns|areas, id = the sheet's frame id), N
 *       times the frame's box, straight alpha
 * Every function reports a reason in `why` when it refuses, so the caller can
 * log one line and fall back to the original art. */
#ifndef DW3_MOD_PACK_H
#define DW3_MOD_PACK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DW3_MOD_FORMAT = 1,       /* the manifest format this loader reads */
    DW3_MOD_SCALE_MIN = 2,
    DW3_MOD_SCALE_MAX = 4,
    DW3_MOD_MAX_DIM = 8192,   /* a file wider or taller is refused */
    DW3_MOD_WHY = 160
};

typedef struct Dw3ModImage {
    int w, h;
    uint8_t *rgba; /* malloc'd, w * h * 4, straight alpha */
} Dw3ModImage;

typedef struct Dw3ModManifest {
    int format;
    char id[64];
    char name[96];
    char version[24];
    char filter[16];     /* hint: nearest, linear, sharp ("" = none) */
    char generator[64];
    int region_us, region_eu;
    unsigned scales;     /* bit N set: the pack has maps at N x */
    int reveal;          /* the pack has reveal sprites */
} Dw3ModManifest;

typedef struct Dw3ModPack {
    char dir[512];
    Dw3ModManifest man;
} Dw3ModPack;

/* Parses mod.json (a small JSON subset: objects, strings, numbers, arrays of
 * numbers, true/false/null; unknown keys ignored). Returns 0, or -1 with a
 * reason: not JSON, no "id", "format" greater than the loader's, ... */
int dw3_mod_manifest_parse(const char *json, size_t len, Dw3ModManifest *out,
                           char *why, size_t why_cap);

/* Finds the pack to use under `mods_dir`: the folder named `want` when it is
 * not NULL/empty, else the first folder (sorted by name) whose manifest is
 * valid and lists `region` ("us" / "eu"). Returns 0, or -1 with a reason
 * ("no mods folder", "no pack for region eu", ...). */
int dw3_mod_pack_find(const char *mods_dir, const char *want, const char *region,
                      Dw3ModPack *out, char *why, size_t why_cap);

/* Loads one PNG as RGBA; refuses anything unreadable, over DW3_MOD_MAX_DIM or
 * over 64 MiB on disk. Returns 0, or -1. */
int dw3_mod_png_load(const char *path, Dw3ModImage *out, char *why,
                     size_t why_cap);
void dw3_mod_image_free(Dw3ModImage *img);

/* The map of `half` for `region`: the highest scale <= max_scale the pack lists
 * whose file exists and is exactly src_w * N by src_h * N. A file of any other
 * size is refused with a reason. *scale gets N; the image is opaque. */
int dw3_mod_load_map(const Dw3ModPack *pack, const char *region, int half,
                     int src_w, int src_h, int max_scale, Dw3ModImage *out,
                     int *scale, char *why, size_t why_cap);

/* One reveal sprite at exactly `scale` x its box (w x h source pixels). */
int dw3_mod_load_sprite(const Dw3ModPack *pack, const char *region, int half,
                        const char *kind, int frame, int scale, int w, int h,
                        Dw3ModImage *out, char *why, size_t why_cap);

/* Straight-alpha blend of `src` over `dst` with src's top left on dst pixel
 * (x, y); clipped. */
void dw3_mod_blend(Dw3ModImage *dst, const Dw3ModImage *src, int x, int y);

#ifdef __cplusplus
}
#endif

#endif /* DW3_MOD_PACK_H */
