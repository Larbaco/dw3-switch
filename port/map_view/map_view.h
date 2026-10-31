/* P6.32: the new full-screen map screen (option D), glue between
 * the game's Map screen (patch 0146), the shared core (map_view_core.h) and
 * the two host overlays (PC SDL3 GPU, Switch GLES). Design and results:
 * docs/P6.32_MAP_ZOOM.md. Port option: file key `map_screen` (1 = on; off by
 * default since phase 1). */
#ifndef DW3_MAP_VIEW_H
#define DW3_MAP_VIEW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "port_map_view.h" /* the game side: dw3_port_map_view_frame */

/* --- boot (both hosts) ---------------------------------------------------
 * After the config directory and the runtime (cdfs) are known: remembers
 * `<config_dir>/cache` (Switch: sdmc:/switch/dw3/cache) as the cache folder
 * and creates it. */
void dw3_map_view_init(const char *config_dir);
/* The background precompute (blocking; the host runs it on a low-priority
 * thread on a spare core): reads the map files straight from the runtime tree
 * (cdfs), builds the captured map and the CPU scalers for both halves and
 * writes them to the cache. Frees everything it allocated before returning. */
void dw3_map_view_precompute_run(void);
/* BUG-1: ask the precompute to stop at the next safe point (between two images,
 * so at most one scaler build, 2 to 6 s on the Switch, after the call). The host
 * calls it on the way out, then joins its thread. Safe to call at any time. */
void dw3_map_view_precompute_cancel(void);

/* The host's thread starter for the CPU scalers (Lanczos2, FSR 1), so the game
 * keeps taking input while they build. Run fn(arg) on a low-priority thread
 * and return 0; return -1 when it cannot (the build then runs on the caller).
 * Defined by the host; map_view.c has a weak fallback that returns -1.
 * DW3_MAP_SYNC=1 forces the synchronous build (the replays need the frame at
 * which the scaler changes to be fixed). */
int dw3_map_view_async_start(void (*fn)(void *), void *arg);

/* The right stick's vertical axis for the zoom, -127..127, up positive (a PS1
 * analog byte distance from the centre, raw). Hosts call it every frame; the
 * dead zone and the scaling are done here. 0 when no stick. */
void dw3_map_view_set_zoom_stick(int raw_up);
/* --- host overlays ---------------------------------------------------------*/
enum { DW3_MV_HUD_W = 640, DW3_MV_HUD_H = 360 };
/* The backdrop behind the map (phase 3): the map's own colours, 32 x 18 cells,
 * stretched over the whole picture with a linear filter. */
enum { DW3_MV_BACKDROP_W = 32, DW3_MV_BACKDROP_H = 18 };
/* The game's own sprites that sit on the map (the cursor, the balloon of the
 * current area): composed from the VRAM into small RGBA images when the view
 * opens (the bank), drawn by the host as quads in map space. */
/* Bank 12 is the name box's plate (OBS-3): the game's frame sprite 0x3B with its
 * shadow, drawn by the host over the map; the game's own pixels are used only
 * for the text inside it. */
enum { DW3_MV_BANK_N = 13, DW3_MV_SPRITES = 4 };
typedef struct Dw3MapViewBankImg {
    const uint8_t *rgba; /* straight alpha, NULL when the frame was not built */
    int w, h;
} Dw3MapViewBankImg;
typedef struct Dw3MapViewSprite {
    int bank;                /* index into the bank */
    float x0, y0, x1, y1;    /* fractions of the picture */
} Dw3MapViewSprite;

typedef struct Dw3MapViewDraw {
    int active;
    unsigned tex_gen;          /* changes when the map texture must be sent */
    const uint8_t *tex_rgba;   /* valid until dw3_map_view_tex_taken() */
    int tex_w, tex_h;
    int linear;                /* 1: bilinear sampler, 0: nearest */
    float x0, y0, x1, y1;      /* the map's rectangle, fractions of the picture */
    unsigned hud_gen;
    const uint8_t *hud_rgba;   /* DW3_MV_HUD_W x DW3_MV_HUD_H, straight alpha */
    unsigned backdrop_gen;     /* changes when the backdrop must be sent */
    const uint8_t *backdrop_rgba; /* DW3_MV_BACKDROP_W x _H, opaque; NULL = black */
    unsigned bank_gen;         /* changes when the bank images must be sent */
    Dw3MapViewBankImg bank[DW3_MV_BANK_N];
    int nspr;                  /* sprites to draw, in order (map, then these) */
    Dw3MapViewSprite spr[DW3_MV_SPRITES];
    /* The name box: the game draws its text window itself, so the host leaves
     * this rectangle of the picture to the game's own pixels (fractions of the
     * picture); hole = 0 when there is none. With the plate built (bank 12,
     * drawn as a sprite) the rectangle is the plate's flat inside, a few pixels
     * from its border, so no pixel of the game lands on the border. */
    int hole;
    float hx0, hy0, hx1, hy1;
} Dw3MapViewDraw;
void dw3_map_view_get_draw(Dw3MapViewDraw *d);
/* The host copied tex_rgba into its own texture: the CPU copy can go. */
void dw3_map_view_tex_taken(unsigned gen);

#ifdef __cplusplus
}
#endif

#endif /* DW3_MAP_VIEW_H */
