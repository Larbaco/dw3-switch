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

/* Preview.18 blue triage: env DW3_MESH_DIAG=1 or -DDW3_NX_MESH_DIAG=ON.
 * Magenta ClearImage + constant bright-green MESH_VS verts on gpu-solid
 * (OLED: greens paint; clear often overpainted). Post-solid magenta NDC rim
 * falsifies overpaint vs clear-band miss. Yellow NDC first-tri + cyan corner.
 * gpu-tex: textured vs solid enqueue/flush counts (UI skipped under
 * mesh_only_diag — expected). Soft default unchanged.
 * Default off — does not change release soft battle. */
int dw3_nx_mesh_diag_enabled(void);

/* DIAG + gpu-solid: tiny cyan NDC corner marker (present proven by ssquad2).
 * x,y,w,h are the presented VRAM crop. Also paints its right magenta rim. */
void dw3_nx_mesh_diag_ssquad(int x, int y, int w, int h);

/* Drop pending verts / "had content" without drawing (leave battle → field). */
void dw3_nx_mesh_discard(void);

/* CPU VRAM changed (LoadImage / Clear / Move); next flush re-uploads sample. */
void dw3_nx_mesh_mark_vram_dirty(void);

void dw3_nx_mesh_set_offset(int x, int y);
void dw3_nx_mesh_set_area(int x0, int y0, int x1, int y1);
void dw3_nx_mesh_set_tpage(uint16_t tpage, int dither_env);
void dw3_nx_mesh_set_twin(uint32_t twin);

void dw3_nx_mesh_clear_rect(int x, int y, int w, int h, float r, float g,
                            float b);

/* Enqueue a triangle list (indices into v[]). Flushes automatically if full. */
void dw3_nx_mesh_enqueue(const Dw3NxMeshVertex *v, int nverts,
                         const uint16_t *idx, int nidx,
                         const uint16_t *cpu_vram);

void dw3_nx_mesh_flush(const uint16_t *cpu_vram);

bool dw3_nx_mesh_has_content(void);
uint64_t dw3_nx_mesh_prims_flushed(void);

/* Blit the mesh FBO display rect into the current draw framebuffer's viewport
 * rectangle (vx,vy,vw,vh) in window pixels. */
void dw3_nx_mesh_present(int dx, int dy, int dw, int dh, int vx, int vy, int vw,
                         int vh);

/* Read the mesh FBO display rect into tightly packed RGBA8888 (GL origin).
 * Returns bytes written, or 0 on failure. */
size_t dw3_nx_mesh_read_rect_rgba(int dx, int dy, int dw, int dh, uint8_t *dst,
                                  size_t dst_cap);

/* Drop the per-frame "had mesh" flag after present (FBO contents stay). */
void dw3_nx_mesh_end_frame(void);

#endif /* DW3_NX_MESH_H */
