/* P5.2 — GLES3 battle-mesh layer (see docs/P5.2_BATTLE_GPU.md).
 * No GL types in this header: switch_gpu.c must not include EGL next to PSY-Z. */
#ifndef DW3_NX_MESH_H
#define DW3_NX_MESH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DW3_NX_MESH_TPAGE_NOTEXTURE 0x8000u
#define DW3_NX_MESH_TPAGE_DITHER 0x4000u

typedef struct Dw3NxMeshVertex {
    int16_t x;
    int16_t y;
    uint16_t u;
    uint16_t v;
    uint16_t clut;
    uint16_t tpage;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    uint32_t twin;
} Dw3NxMeshVertex;

/* After EGL is current. Returns false → caller keeps full software path. */
bool dw3_nx_mesh_init(void);
void dw3_nx_mesh_shutdown(void);
/* Dock/undock tears down GL; call again after a new ES3 context is current. */
bool dw3_nx_mesh_reinit(void);

bool dw3_nx_mesh_available(void);

/* Drop pending verts / "had content" without drawing (leave battle → field). */
void dw3_nx_mesh_discard(void);

/* CPU VRAM changed (LoadImage / Clear / Move); next flush re-uploads sample. */
void dw3_nx_mesh_mark_vram_dirty(void);
/* Same, limited to one VRAM rect (x,y,w,h); the upload covers the union. */
void dw3_nx_mesh_mark_vram_dirty_rect(int x, int y, int w, int h);

void dw3_nx_mesh_set_offset(int x, int y);
void dw3_nx_mesh_set_area(int x0, int y0, int x1, int y1);
void dw3_nx_mesh_set_tpage(uint16_t tpage, int dither_env);
void dw3_nx_mesh_set_twin(uint32_t twin);

void dw3_nx_mesh_clear_rect(int x, int y, int w, int h, float r, float g,
                            float b);

/* Background fill in GPU battle modes (DRAWENV isbg TILE): clear the rect to
 * the colour and mark the frame as mesh content so present composites the CPU
 * VRAM over the FBO even before the first triangle of the frame. The caller
 * flushes pending triangles first so OT order is kept. */
void dw3_nx_mesh_fill_bg(int x, int y, int w, int h, float r, float g, float b);

/* P6.1 side gutters (Psyz_VideoSetGutter): triangles drawn into a drawing
 * area spanning [x0, x0 + w) and h rows (any height when h <= 0) also land in
 * a gutter FBO, px columns wider on each side; px = 0 turns them off. */
void dw3_nx_mesh_set_gutter(int px, int x0, int w, int h);
/* Black gutters for rows [y0, y1) (a buffer about to be drawn). */
void dw3_nx_mesh_clear_gutter_rows(int y0, int y1);
bool dw3_nx_mesh_has_gutters(void);

/* Enqueue a triangle list (indices into v[]). Flushes automatically if full. */
void dw3_nx_mesh_enqueue(const Dw3NxMeshVertex *v, int nverts,
                         const uint16_t *idx, int nidx,
                         const uint16_t *cpu_vram);

void dw3_nx_mesh_flush(const uint16_t *cpu_vram);

bool dw3_nx_mesh_has_content(void);
uint64_t dw3_nx_mesh_prims_flushed(void);

/* Blit the mesh FBO display rect into the current draw framebuffer's viewport
 * rectangle (vx,vy,vw,vh) in window pixels. With gutters on and dw the gutter
 * width, the blit is the display plus both gutters. */
void dw3_nx_mesh_present(int dx, int dy, int dw, int dh, int vx, int vy, int vw,
                         int vh);

/* Read the mesh FBO display rect into tightly packed RGBA8888 (GL origin).
 * Returns bytes written, or 0 on failure. */
size_t dw3_nx_mesh_read_rect_rgba(int dx, int dy, int dw, int dh, uint8_t *dst,
                                  size_t dst_cap);

/* Drop the per-frame "had mesh" flag after present (FBO contents stay). */
void dw3_nx_mesh_end_frame(void);

#endif /* DW3_NX_MESH_H */
