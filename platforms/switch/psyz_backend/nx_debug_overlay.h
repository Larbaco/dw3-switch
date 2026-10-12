/* On-screen debug HUD for OLED triage. Default ON for --nxlink CMake builds.
 * See docs/DEBUG_OVERLAY.md. */
#ifndef DW3_NX_DEBUG_OVERLAY_H
#define DW3_NX_DEBUG_OVERLAY_H

#include <stdint.h>

#ifndef DW3_NX_DEBUG_OVERLAY
#define DW3_NX_DEBUG_OVERLAY 0
#endif

#if DW3_NX_DEBUG_OVERLAY

#include "../source/debug_overlay_text.h"

/* Publish the latest 60-pump sample (same cadence as frame dt / mesh dbg). */
void dw3_nx_debug_overlay_note_sample(uint64_t dt_us,
                                      const Dw3DebugOverlayMeshSnap *mesh);

/* Last present path: non-zero = mesh FBO + UI discard, else opaque blit. */
void dw3_nx_debug_overlay_note_present(int use_mesh);

/* Second pass after game composite; before eglSwapBuffers. */
void dw3_nx_debug_overlay_draw(uint32_t fb_w, uint32_t fb_h);

/* Drop GLES objects (dock recreate / gpu_shutdown). */
void dw3_nx_debug_overlay_invalidate_gl(void);

/* On-screen crash/assert overlay still open; SD report is docs/P5.4_CRASH.md. */
/* void dw3_nx_debug_overlay_crash_hook(const char *msg); */

#else /* !DW3_NX_DEBUG_OVERLAY */

#define dw3_nx_debug_overlay_note_sample(dt_us, mesh) ((void)0)
#define dw3_nx_debug_overlay_note_present(use_mesh) ((void)0)
#define dw3_nx_debug_overlay_draw(fb_w, fb_h) ((void)0)
#define dw3_nx_debug_overlay_invalidate_gl() ((void)0)

#endif /* DW3_NX_DEBUG_OVERLAY */

#endif /* DW3_NX_DEBUG_OVERLAY_H */
