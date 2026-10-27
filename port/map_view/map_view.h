/* P6.32 phase 0 spike: the new full-screen map screen (option D), glue between
 * the game's Map screen (patch 0146), the shared core (map_view_core.h) and
 * the two host overlays (PC SDL3 GPU, Switch GLES). Design and results:
 * docs/P6.32_MAP_ZOOM.md. Port option: file key `map_screen` (1 = on, the
 * PoC default). */
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

/* --- host overlays ---------------------------------------------------------*/
enum { DW3_MV_HUD_W = 640, DW3_MV_HUD_H = 360 };
typedef struct Dw3MapViewDraw {
    int active;
    unsigned tex_gen;          /* changes when the map texture must be sent */
    const uint8_t *tex_rgba;   /* valid until dw3_map_view_tex_taken() */
    int tex_w, tex_h;
    int linear;                /* 1: bilinear sampler, 0: nearest */
    float x0, y0, x1, y1;      /* the map's rectangle, fractions of the picture */
    unsigned hud_gen;
    const uint8_t *hud_rgba;   /* DW3_MV_HUD_W x DW3_MV_HUD_H, straight alpha */
} Dw3MapViewDraw;
void dw3_map_view_get_draw(Dw3MapViewDraw *d);
/* The host copied tex_rgba into its own texture: the CPU copy can go. */
void dw3_map_view_tex_taken(unsigned gen);

#ifdef __cplusplus
}
#endif

#endif /* DW3_MAP_VIEW_H */
