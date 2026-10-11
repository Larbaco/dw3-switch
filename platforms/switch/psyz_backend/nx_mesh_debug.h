/* Sampled GLES mesh / present counters for nxlink P5.2 debugging.
 * Enabled only when DW3_NX_DEBUG_MESH=1 (default ON for --nxlink CMake builds).
 * See docs/NXLINK_DEBUG.md. */
#ifndef DW3_NX_MESH_DEBUG_H
#define DW3_NX_MESH_DEBUG_H

#include <stdint.h>

#ifndef DW3_NX_DEBUG_MESH
#define DW3_NX_DEBUG_MESH 0
#endif

#if DW3_NX_DEBUG_MESH

/* Counters accumulated since the previous take_window (typically 60 VSyncs). */
typedef struct Dw3NxMeshDebugWindow {
    uint64_t tris_enqueued;
    uint64_t tris_flushed;
    uint64_t flush_us;
    uint64_t upload_us;
    uint64_t present_us;
    uint32_t frames_mesh; /* presents that composited the mesh FBO */
    uint32_t frames_blit; /* presents that used the opaque VRAM blit */
    uint32_t soft_fallback; /* triangle polys that fell through to software */
    int mesh_available;
} Dw3NxMeshDebugWindow;

void dw3_nx_mesh_debug_note_enqueue(int nidx);
void dw3_nx_mesh_debug_note_flush(uint32_t flush_us, uint32_t upload_us,
                                  int tris);
void dw3_nx_mesh_debug_note_present(int use_mesh, uint32_t present_us);
void dw3_nx_mesh_debug_note_soft_fallback(void);
/* Logs the first N distinct poly codes that hit the GLES path (once each). */
void dw3_nx_mesh_debug_note_poly_code(int code, int textured);
/* Copy window totals into *out and zero the accumulators. */
void dw3_nx_mesh_debug_take_window(Dw3NxMeshDebugWindow *out);

#else /* !DW3_NX_DEBUG_MESH */

#define dw3_nx_mesh_debug_note_enqueue(nidx) ((void)0)
#define dw3_nx_mesh_debug_note_flush(flush_us, upload_us, tris) ((void)0)
#define dw3_nx_mesh_debug_note_present(use_mesh, present_us) ((void)0)
#define dw3_nx_mesh_debug_note_soft_fallback() ((void)0)
#define dw3_nx_mesh_debug_note_poly_code(code, textured) ((void)0)

#endif /* DW3_NX_DEBUG_MESH */

#endif /* DW3_NX_MESH_DEBUG_H */
