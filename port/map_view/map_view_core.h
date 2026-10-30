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
    MV_SCALER_BILINEAR = 4,/* phase 3: GPU bilinear on the captured map itself
                            * (no CPU work, no shader: the sampler's filter) */
    MV_SCALER_COUNT = 5
};
enum { MV_SCALE = 3 };

const char *dw3_mv_scaler_name(int scaler);
/* 1 when the GPU must filter the texture bilinearly (every scaler but nearest) */
int dw3_mv_scaler_linear(int scaler);
/* The next / previous scaler, wrapping. */
int dw3_mv_scaler_step(int scaler, int dir);

/* --- button bindings (phase 4) --------------------------------------------
 * Which logical (PS-side) pad button does each action of the view. The pad the
 * view reads is the one the game reads, after the P6.12 / P9.8 button map, so a
 * binding names a PS button and the map decides which console button makes it:
 * rebinding here and remapping there compose. The bindings are file-only keys
 * of port_options.cfg (map_view_open, map_view_zoom_out, map_view_zoom_in,
 * map_view_centre, map_view_scaler_prev, map_view_scaler_next) holding one of
 * cross square l1 r1 l2 r2 l3 select. Not bindable, because the view uses them
 * itself: triangle (the game's cancel, closes the view), circle (the bars), r3
 * (the fast-forward key, ignored in the view), start, the d-pad. The six
 * actions must use six different buttons: a value that is not bindable falls
 * back to its default alone; two actions on the same button put the whole table
 * back to the defaults. Bit numbers are the pad's (port_map_view.h). */
enum {
    MV_ACT_OPEN = 0,        /* on the game's Map screen: opens the view */
    MV_ACT_ZOOM_OUT = 1,    /* a step down the ladder */
    MV_ACT_ZOOM_IN = 2,     /* a step up the ladder */
    MV_ACT_CENTRE = 3,      /* centre on the current area */
    MV_ACT_SCALER_PREV = 4,
    MV_ACT_SCALER_NEXT = 5,
    MV_ACT_COUNT = 6
};
typedef struct MvBindings {
    int bit[MV_ACT_COUNT];  /* the pad bit that does the action */
    int from_file[MV_ACT_COUNT]; /* 1 where the file named it (and it was taken) */
} MvBindings;

/* The cfg key ("map_view_open") and the word of the action ("open"). */
const char *dw3_mv_act_key(int act);
const char *dw3_mv_act_name(int act);
/* The default pad bit of an action: square, l1, r1, select, l2, r2. */
int dw3_mv_bind_default_bit(int act);
/* The shown name of a bindable pad bit ("Square", "L1", "Select"), "?" if none. */
const char *dw3_mv_bind_bit_name(int bit);
/* A cfg value (any case, spaces trimmed) to its pad bit; -1 unknown, -2 a button
 * the view keeps for itself (triangle, circle, r3, start, the d-pad). */
int dw3_mv_bind_parse(const char *text);
/* The PS button index of port_button_map.h (0 cross, 1 circle, 2 triangle,
 * 3 square, 4 l1 ... 9 r3) of a pad bit, or -1 for select (not remappable). */
int dw3_mv_bind_ps_button(int bit);
void dw3_mv_bindings_default(MvBindings *b);
/* Resolve the six file values (NULL = the key is absent). Returns 0 when all is
 * well, 1 when some value was not bindable (that action takes its default),
 * 2 when two actions meant the same button (every action takes its default).
 * `why` gets one line per problem (may be NULL). */
int dw3_mv_bindings_resolve(MvBindings *b, const char *const vals[MV_ACT_COUNT],
                            char *why, size_t cap);
/* The action bound to pad bit `bit`, or -1. */
int dw3_mv_bindings_action(const MvBindings *b, int bit);

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
enum { MV_ZOOM_STEPS = 7, MV_ZOOM_TOP_PLAIN = 5 };
/* 0 = FIT, then 1.0 1.25 1.5 2.0 3.0 and 4.0. P6.32 section 4.4: 4.0 only when a
 * higher-resolution source (a mod pack) is loaded, else the top is 3.0 (the
 * source cannot show more). */
extern const float MV_ZOOM_TABLE[MV_ZOOM_STEPS];
typedef struct MvView {
    int step;     /* index into MV_ZOOM_TABLE (the nearest one when zoom > 0) */
    int top;      /* MV_ZOOM_STEPS - 1 allows 4.0, anything else tops at 3.0 */
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
/* The same, saying where the view rests (P6.32 phase 3, the 4.2g limits): the pan
 * limit is the image box at every zoom, so the view never shows past the picture.
 * Returns the edges the view is held at (its edge is the image's edge: it cannot go
 * further that way) and the axes on which the whole map fits (the view is centred
 * and does not pan). *want_c get the centre the view had before the clamp, so a
 * caller can say how far it wanted to go past. */
enum { MV_PAN_LEFT = 1, MV_PAN_RIGHT = 2, MV_PAN_TOP = 4, MV_PAN_BOTTOM = 8,
       MV_PAN_CENTRED_X = 16, MV_PAN_CENTRED_Y = 32 };
int dw3_mv_view_clamp_ex(MvView *v, int map_w, int map_h, int vw, int vh,
                         float *want_cx, float *want_cy);
/* The top zoom of this view (3.0, or 4.0 with a higher-resolution source). */
float dw3_mv_view_top_zoom(const MvView *v);
/* A map point (map pixels) in viewport pixels. */
void dw3_mv_view_point(const MvView *v, int map_w, int map_h, int vw, int vh,
                       float mx, float my, float *px, float *py);
/* Zoom about an anchor: put the map point (mx, my) on viewport pixel (px, py)
 * (call it after the zoom change, with the point's viewport pixel from before), then
 * clamp. Returns the clamp flags; with the map point away from the edges the
 * anchor does not move on screen, else the clamp wins and the log says so. */
int dw3_mv_view_anchor(MvView *v, float mx, float my, float px, float py,
                       int map_w, int map_h, int vw, int vh, float *want_cx,
                       float *want_cy);
/* The view follows a map point: pans only while the point is closer than
 * (margin_x, margin_y) viewport pixels to an edge. Returns the clamp flags. */
int dw3_mv_view_follow(MvView *v, float mx, float my, float margin_x,
                       float margin_y, int map_w, int map_h, int vw, int vh,
                       float *want_cx, float *want_cy);

/* --- cursor speed (phase 3) ----------------------------------------------
 * Source pixels a frame the cursor moves while a direction has been held for
 * `hold` frames: the game's ramp (2 for 10 frames, 3 for 10, then 4) up to
 * MV_PAN_ACCEL_AFTER frames, then +1 every 15 frames up to a cap that is 10 screen
 * pixels of the 240-line picture (10 / zoom source pixels, never below 4). */
enum { MV_PAN_ACCEL_AFTER = 60, MV_STICK_MOVE_DEADZONE = 16 };
float dw3_mv_pan_speed(int hold, float zoom);
/* The left stick's raw axis (-127..127) as a signed factor on that speed: 0 inside
 * the dead zone, then a smoothstep to +-1 at full tilt (never below 0.02 so a
 * barely-moved stick still moves the cursor). */
float dw3_mv_stick_unit(int raw);

/* --- the FIT backdrop (phase 3) -------------------------------------------
 * Beside the map at FIT (and below 1.0 on a tall window) the host shows the map's
 * own colours instead of black: the map averaged to MV_BACKDROP_W x MV_BACKDROP_H
 * cells, dimmed (`dim_percent` of the brightness), stretched over the whole picture
 * by the GPU's linear filter. `rgba` is MV_BACKDROP_W * MV_BACKDROP_H * 4 bytes. */
enum { MV_BACKDROP_W = 32, MV_BACKDROP_H = 18 };
int dw3_mv_backdrop(const MvImage *src, uint8_t *rgba, int dim_percent);
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

/* --- the PS1 cursor's reach, and the area-name text (phase 2) ------------
 * The game's own cursor moves in a box (STSTATUS_moveMapCursor): x from 0x16 on
 * the screen to 0x12A, y from 0xF to height - top - 0x1E = 194, plus the scroll
 * (x 0 to -0x48, y 0 to -0x60), so in map space it reaches x 22..370 and y 15..290.
 * What lies outside is the strip the PS1 cursor cannot get to. The new screen's
 * cursor covers the whole image. A location is hit from (x + 6, x + 0x12] and
 * (y + 6, y + 0x12] (STSTATUS_showMapArea). */
enum { MV_PS1_X0 = 22, MV_PS1_X1 = 370, MV_PS1_Y0 = 15, MV_PS1_Y1 = 290 };
/* The spot at (sx, sy): 0 = its whole hit box is inside the PS1 cursor's reach,
 * 1 = partly, 2 = none of it. */
int dw3_mv_hitbox_reach(int sx, int sy);
/* The cursor point (x, y) in map space: 0 inside the reach, else the strips it is
 * in, as bits: 1 left, 2 right, 4 top, 8 bottom. */
int dw3_mv_point_strip(int x, int y);
/* The name window's text (patch 0147: Shift-JIS codes of the game's glyph map,
 * 0x0A a line break, 0x8000 | n an icon) as readable ASCII for the log: the
 * full-width Latin letters and digits and the common signs as themselves, a line
 * break as " / ", the icons the names use as their text (space, full stop, apostrophe,
 * hyphen, the accented letters as UTF-8), any other icon as <iN>, anything else as
 * <XXXX>. Returns the number of codes it could not turn into text (<iN>, <XXXX>). */
int dw3_mv_name_text(const unsigned short *chars, int n, char *out, size_t cap);

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
