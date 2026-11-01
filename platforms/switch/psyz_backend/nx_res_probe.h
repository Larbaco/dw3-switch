/* P6.11 phase 0: OLED measurement aid for internal resolution scaling.
 *
 * A development test aid, not a player feature (AGENTS.md rule 8: `DW3_*`
 * environment aids are not options). Off unless DW3_NX_RES_PROBE is set; with
 * it unset every entry point below returns at its first line and the game, the
 * mesh layer and the present are unchanged. See docs/P6.11_INTERNAL_RES.md
 * ("OLED rows R0.x") for what to run and what to send back.
 *
 * Environment (bake with -DDW3_NX_TEST_ENV="K=V,K=V", docs/NXLINK_DEBUG.md):
 *   DW3_NX_RES_PROBE=1|2   1 = GPU times from GL_EXT_disjoint_timer_query
 *                          (does not stall the GPU); falls back to 2 when the
 *                          driver has no timer query.
 *                          2 = glFinish around each timed section (serialises
 *                          the CPU and the GPU: the frame is slower, the GPU
 *                          number is certain).
 *   DW3_NX_MESH_SCALE=1|2|3  initial battle mesh scale (default 1). With the
 *                          probe on, Minus + ZL + R cycles 1x -> 2x -> 3x.
 *   DW3_NX_MESH_FILTER=nearest|linear  the filter the mesh target is drawn to
 *                          the output with (default nearest).
 *   DW3_RES_DITHER=fine|ps1  fine (default): the dither matrix indexes the
 *                          fragment, so it is N times finer at Nx. ps1: it
 *                          indexes the PS1 pixel (the PS1 grid).
 *   DW3_NX_MESH_SKIP=1     measurement only: the frames fast-forward does not
 *                          show do not flush the battle mesh (P6.11 phase 3
 *                          idea). Off by default; with the probe on,
 *                          Minus + ZL + L toggles it.
 *
 * Log lines (once per 60 game frames, with `pace dbg:`; and once per
 * allocation):
 *   dw3-res-probe: targets ...   (a scale was applied)
 *   dw3-res-probe: v=1 ...       (the measurement window)
 * Field meanings are in the .c file and in the doc. */
#ifndef DW3_NX_RES_PROBE_H
#define DW3_NX_RES_PROBE_H

#include <stdbool.h>
#include <stdint.h>

/* 0 = off, 1 = timer queries (or the fallback), 2 = glFinish. Cached. */
int dw3_nx_res_probe_on(void);

/* Microseconds from the system tick; the probe's one clock. */
uint64_t dw3_nx_res_probe_clock_us(void);

/* After a GL context is current (mesh init): resolves the timer query entry
 * points and logs what the driver offers. Also after a dock change, which
 * rebuilds the context and drops every query. */
void dw3_nx_res_probe_gl_init(void);
void dw3_nx_res_probe_gl_shutdown(void);

/* The draws of one mesh flush (CPU submit time and GPU time). */
void dw3_nx_res_probe_mesh_begin(void);
void dw3_nx_res_probe_mesh_end(int tris);
/* CPU time of the sample-texture upload before the draws. */
void dw3_nx_res_probe_note_upload(uint32_t us);
/* One game frame ended (shown or not): closes the per-frame mesh figures. */
void dw3_nx_res_probe_frame_end(void);

/* The whole present of a shown frame (upload, mesh blit, UI draw). */
void dw3_nx_res_probe_present_begin(void);
void dw3_nx_res_probe_present_end(void);

/* CPU figures the present loop already has, per shown frame. */
void dw3_nx_res_probe_note_shown(uint64_t sync_us, uint64_t present_us,
                                 uint64_t swap_us);
/* A frame fast-forward did not show, and what its skip path cost. */
void dw3_nx_res_probe_note_skipped(uint64_t us);

/* A mesh scale was applied (init or change). */
void dw3_nx_res_probe_note_targets(int scale, int w, int h, int gutter_target,
                                   uint64_t bytes, uint32_t alloc_us,
                                   int ok);

/* One measurement line for the window the caller's `pace dbg:` covers. */
void dw3_nx_res_probe_emit(unsigned long long dt_us, int speed, bool docked,
                           unsigned out_w, unsigned out_h, int res);

/* The scale-cycle chord, from the pad poll (`held` = raw HidNpadButton bits). */
void dw3_nx_res_probe_poll(uint64_t held);

/* The test aids that change what is drawn. */
int dw3_nx_res_probe_skip_unshown(void);

#endif /* DW3_NX_RES_PROBE_H */
