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

/* The output filter (nx_output_filter.h, DW3_NX_FILTER_*) of the next
 * dw3_nx_mesh_present: the composite of the battle mesh. Builds the filtered blit
 * program the first time; a failure leaves the battle picture on nearest. */
void dw3_nx_mesh_set_filter(int mode);

/* Read the mesh FBO display rect into tightly packed RGBA8888 (GL origin).
 * Returns bytes written, or 0 on failure. */
size_t dw3_nx_mesh_read_rect_rgba(int dx, int dy, int dw, int dh, uint8_t *dst,
                                  size_t dst_cap);

/* A 3D view smaller than the screen (the battle partner view) drew its
 * triangles on the software path into this drawing area (VRAM, inclusive).
 * The UI composite keeps that area's drawn black pixels, which it otherwise
 * discards as "no UI here". Kept for a few frames, one per display buffer. */
void dw3_nx_mesh_note_soft_view(int x0, int y0, int x1, int y1);
/* The noted soft view inside the display rect (dx,dy,dw,dh), relative to
 * its top-left corner. Returns false when none lies in it. */
bool dw3_nx_mesh_soft_view_in(int dx, int dy, int dw, int dh, int *x, int *y,
                              int *w, int *h);

/* P6.11 internal resolution (docs/P6.11_INTERNAL_RES.md). The mesh and gutter
 * targets are `scale` times the VRAM size; 1 is the shipped path. target_bytes
 * is 0 before init and counts every set that is allocated.
 *
 * apply_res: from the port (dw3_platform_apply_internal_res), at a frame
 * boundary. The multiplier drawn at is `active`; the Nx targets are kept
 * allocated for max(active, reserve), so switching to 1x and back (fast-forward)
 * allocates nothing. A change is deferred while a battle is held under Options.
 * frame_begin: once per game frame, before it draws; `skip` = the frame will not
 * be shown, so it draws no mesh (the targets keep the last shown frame).
 * cycle_scale: the measurement chord, a probe build only. */
int dw3_nx_mesh_scale(void);
uint64_t dw3_nx_mesh_target_bytes(void);
void dw3_nx_mesh_apply_res(int active, int reserve);
void dw3_nx_mesh_frame_begin(int skip);
/* Frames skipped since the last call (the probe line's skip= field). */
unsigned dw3_nx_mesh_take_skip_frames(void);
int dw3_nx_mesh_cycle_scale(void);
/* Drop the queued triangles without drawing them (a frame turbo does not
 * show, DW3_NX_MESH_SKIP). Leaves every other state alone. */
void dw3_nx_mesh_drop_pending(void);

/* Drop the per-frame "had mesh" flag after present (FBO contents stay). */
void dw3_nx_mesh_end_frame(void);

#endif /* DW3_NX_MESH_H */
