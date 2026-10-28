/* P6.32 phase 0: the pure parts of the full-screen map view (option D).
 *
 * No game headers, no SDL, no GL: stdint/stdio/stdlib only, so the PC and
 * Switch hosts share it and tools/tests can build it with a plain compiler.
 *   - compose: the map sprite frame, read from a 1024x512 RGB555 VRAM image
 *     (the live VRAM, or one built from the disc files), into RGBA;
 *   - disc: the TIM archive of the map (file FILE_STATUS_BG + 1) into such a
 *     VRAM, as the game's TimLoader does it;
 *   - scalers: sharp prescale, Lanczos2 (platforms/pc/src/title_logo_resample.c)
 *     and AMD FSR 1 (EASU + RCAS) on the CPU, with their timings;
 *   - view maths and the cache file format.
 * Everything derived from the owner's disc lives in memory or in the user's
 * cache folder; nothing here embeds or commits game data. */
#ifndef DW3_MAP_VIEW_CORE_H
#define DW3_MAP_VIEW_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { MV_VRAM_W = 1024, MV_VRAM_H = 512 };

typedef struct MvImage {
    int w, h;
    int ox, oy;    /* the image's top left in sprite space (the frame's box) */
    uint8_t *rgba; /* malloc'd, w * h * 4, straight alpha (always 255 here) */
    uint32_t reveal; /* hash of the revealed pieces composed into it (0: the
                      * bare map); stored in the cache file */
} MvImage;

/* The scalers, in the order the button cycles them. */
enum {
    MV_SCALER_NEAREST = 0, /* GPU nearest on the captured map (no CPU work) */
    MV_SCALER_SHARP = 1,   /* 3x integer nearest prescale, GPU bilinear */
    MV_SCALER_LANCZOS2 = 2,/* CPU Lanczos2 to 3x (title_logo_resample.c) */
    MV_SCALER_FSR = 3,     /* CPU AMD FSR 1: EASU to 3x, then RCAS */
    MV_SCALER_COUNT = 4
};
enum { MV_SCALE = 3 };

const char *dw3_mv_scaler_name(int scaler);
/* 1 when the GPU must filter the texture bilinearly (every scaler but nearest) */
int dw3_mv_scaler_linear(int scaler);
/* The next / previous scaler, wrapping. */
int dw3_mv_scaler_step(int scaler, int dir);

/* --- compose -------------------------------------------------------------
 * Draw frame `frame` of the sprite sheet `sheet` (the game's SpriteDrawer
 * layout: s32 offsets to the parts and the frame ids, then each frame's part
 * list) from `vram` into a fresh RGBA image the size of the frame's box.
 * tex_x/tex_y are SpriteDrawer.setTexture, alt_x/alt_y setAltClut (the values
 * the game passed, 0x280,0 and 0x140,0x100 for the map). *box gets the frame's
 * box in sprite space (x0, y0 of the top left). Returns 0, or -1. */
typedef struct MvBox { int x0, y0, x1, y1; } MvBox;
int dw3_mv_sheet_frame_box(const int32_t *sheet, size_t sheet_len, int frame,
                           MvBox *box);
int dw3_mv_compose(const uint16_t *vram, const int32_t *sheet,
                   size_t sheet_len, int frame, int tex_x, int tex_y,
                   int alt_x, int alt_y, MvImage *out, MvBox *box);

/* Draws frame `frame` of `sheet` over `dst` (RGBA, opaque) with the sheet's
 * origin on dst pixel (org_x, org_y): the game's town and area cells, VRAM
 * colour 0 transparent, clipped. tex_* / alt_* as for dw3_mv_compose. */
int dw3_mv_blit_frame(const uint16_t *vram, const int32_t *sheet,
                      size_t sheet_len, int frame, int tex_x, int tex_y,
                      int alt_x, int alt_y, MvImage *dst, int org_x,
                      int org_y);
/* FNV-1a over `len` bytes; `seed` 0 starts a new hash, else continues one. */
uint32_t dw3_mv_hash(const void *data, size_t len, uint32_t seed);

/* --- disc ----------------------------------------------------------------
 * `file` is the whole map image file (FILE_STATUS_BG + 1 in the first half of
 * the game, + 3 in the second). Entry 0 is an archive of TIMs (some RLEN
 * packed); like TimLoader.loadArchive they go to (0x280 + 0x40 * i, 0) with
 * their CLUT at (0x140, 0x100). `vram` must be zeroed MV_VRAM_W * MV_VRAM_H
 * halfwords. Returns the number of TIMs placed, or -1. */
int dw3_mv_disc_load_map_vram(const uint8_t *file, size_t len,
                              uint16_t *vram);
/* The same for entry 0..2 of the file, its TIMs placed from (image_x, image_y)
 * in steps of 0x40 across: the map's own entry 0 at (0x280, 0), the towns'
 * entry 1 at (0x380, 0x100) and the areas' entry 2 at (0x300, 0x100) (the
 * texture origin the reveal cells are drawn with; their 4-bit palettes are the
 * last rows of those pages). The CLUT of a TIM that has one goes to
 * (0x140, 0x100). Returns the TIMs placed, or -1. */
int dw3_mv_disc_load_entry(const uint8_t *file, size_t len, int entry,
                           int image_x, int image_y, uint16_t *vram);

/* --- scalers -------------------------------------------------------------
 * Builds the 3x image for SHARP, LANCZOS2 or FSR (NEAREST copies). *ms is the
 * wall time in milliseconds when not NULL. Returns 0, or -1. */
int dw3_mv_scale(int scaler, const MvImage *src, MvImage *dst, double *ms);
void dw3_mv_image_free(MvImage *img);

/* FSR 1 pieces, exposed for the tests: EASU from src to dst (any size), and
 * RCAS in place with `attenuation` in stops (0 = sharpest, 2 = off-ish). */
void dw3_mv_fsr_easu(const MvImage *src, MvImage *dst);
void dw3_mv_fsr_rcas(MvImage *img, float attenuation);

/* --- view maths ----------------------------------------------------------
 * The zoom is in "logical" terms: 1.0 shows a source pixel as one pixel of
 * the 320x240 menu picture (so the 392 x ~320 map is bigger than the picture,
 * as on the PS1); FIT is the largest zoom that shows the whole map. The
 * viewport (vw x vh) is the whole 16:9 picture in host pixels. */
enum { MV_ZOOM_STEPS = 6 };
extern const float MV_ZOOM_TABLE[MV_ZOOM_STEPS]; /* 0 = FIT, then 1.0 ... 3.0 */
typedef struct MvView {
    int step;     /* index into MV_ZOOM_TABLE (the nearest one when zoom > 0) */
    float zoom;   /* > 0: a smooth zoom between the steps (the right stick),
                   * logical, same units as the table; 0: the step rules */
    float cx, cy; /* source pixel at the middle of the viewport */
} MvView;
/* Source pixel -> viewport pixel scale for the step (px per source px). */
float dw3_mv_view_scale(int step, int map_w, int map_h, int vw, int vh);
/* The scale and the logical zoom of the view as it is now (step or smooth). */
float dw3_mv_view_cur_scale(const MvView *v, int map_w, int map_h, int vw,
                            int vh);
float dw3_mv_view_cur_zoom(const MvView *v, int map_w, int map_h, int vw,
                           int vh);
/* Smooth zoom: multiply the zoom by `factor` (> 1 in), keep it between FIT and
 * the top of the table, no snapping to the steps (FIT is the floor). */
void dw3_mv_view_zoom_by(MvView *v, float factor, int map_w, int map_h, int vw,
                         int vh);
/* Keep the view on the map: centre when the map is smaller than the view. */
void dw3_mv_view_clamp(MvView *v, int map_w, int map_h, int vw, int vh);
/* Button step: the next table step in that direction (from a smooth zoom, the
 * next table zoom above or below it). Change the step keeping the source point under the viewport centre. */
void dw3_mv_view_zoom(MvView *v, int dir, int map_w, int map_h, int vw, int vh);
/* The map's rectangle in viewport pixels (may extend past the viewport). */
void dw3_mv_view_rect(const MvView *v, int map_w, int map_h, int vw, int vh,
                      float *x0, float *y0, float *x1, float *y1);

/* --- fixed-size overlay sprites ------------------------------------------
 * The game's sprites that sit on the map (the cursor, the balloon) are small
 * images in source pixels. A plain zoom would scale them with the map (3x at
 * the top step: giants). Their POSITIONS follow the map, their SIZES stay as at
 * zoom 1.0, the view the screen opens in (and the PS1 size): the sprite is scaled
 * about its anchor by k = scale(1.0) / cur_scale = 1/Z (Z = the logical zoom;
 * 1 at 1.0, 1/3 at the top step, above 1 below 1.0 down to FIT). The reveal layer (lit towns, visited areas) is part of the map
 * picture and keeps scaling with it. */
float dw3_mv_sprite_factor(const MvView *v, int map_w, int map_h, int vw, int vh);
/* The sprite's rectangle in viewport pixels: the anchor is the map point
 * (ax, ay) in map pixels (the image's own pixels), (ox, oy) the sprite's offset
 * from the anchor in source pixels and (w, h) its size in source pixels. With
 * `fixed` the offset and the size are scaled by k (above), else they zoom with
 * the map. */
void dw3_mv_sprite_rect(const MvView *v, int map_w, int map_h, int vw, int vh,
                        float ax, float ay, int ox, int oy, int w, int h,
                        int fixed, float *x0, float *y0, float *x1, float *y1);

/* --- name box placement --------------------------------------------------
 * The game decides above / below ONCE, when the cursor gets on a location
 * (STSTATUS_showMapArea sets hoverY then and never again). The host mirrors
 * that: the answer is latched at the entry and kept while the cursor stays on
 * a location, so a zoom or a view follow (which move the cursor's screen row
 * without the cursor moving) cannot flip the box. */
typedef struct MvNameBox {
    int hover;  /* the location of the last call, 0 = none */
    int bottom; /* latched: the box is at the bottom */
} MvNameBox;
/* `scr_y` is the cursor's row in the 240-line picture now; returns bottom. */
int dw3_mv_namebox_bottom(MvNameBox *nb, int hover, float scr_y);

/* --- cache ---------------------------------------------------------------
 * <dir>/<name>: "DW3M", version, w, h, ox, oy, reveal hash, then w * h * 4
 * RGBA bytes. The base (scaler 0) is always the bare map (reveal 0); a scaled
 * file is only used when its reveal hash is the one of the pieces the game
 * shows now, else it is rebuilt and overwritten. */
int dw3_mv_cache_write(const char *path, const MvImage *img);
int dw3_mv_cache_read(const char *path, MvImage *img);
void dw3_mv_cache_name(char *out, size_t cap, const char *dir, const char *region,
                       int half, int scaler);

#ifdef __cplusplus
}
#endif

#endif /* DW3_MAP_VIEW_CORE_H */
