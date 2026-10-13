/* Gated mesh/present debug counters. Empty TU when DW3_NX_DEBUG_MESH is off. */
#include "nx_mesh_debug.h"

#if DW3_NX_DEBUG_MESH

#include "nx.h"
#include "nx_mesh.h"

enum { MESH_DEBUG_FIRST_CODES = 16 };

static Dw3NxMeshDebugWindow s_win;
static uint8_t s_seen_codes[256];
static int s_codes_logged;

void dw3_nx_mesh_debug_note_enqueue(int nidx) {
    if (nidx > 0) {
        s_win.tris_enqueued += (uint64_t)(nidx / 3);
    }
}

void dw3_nx_mesh_debug_note_flush(uint32_t flush_us, uint32_t upload_us,
                                  int tris) {
    if (tris > 0) {
        s_win.tris_flushed += (uint64_t)tris;
    }
    s_win.flush_us += (uint64_t)flush_us;
    s_win.upload_us += (uint64_t)upload_us;
}

void dw3_nx_mesh_debug_note_present(int use_mesh, uint32_t present_us) {
    s_win.present_us += (uint64_t)present_us;
    if (use_mesh) {
        s_win.frames_mesh++;
    } else {
        s_win.frames_blit++;
    }
}

void dw3_nx_mesh_debug_note_upload(uint32_t pixels) {
    s_win.uploads++;
    s_win.upload_px += (uint64_t)pixels;
}

void dw3_nx_mesh_debug_note_soft_fallback(void) { s_win.soft_fallback++; }

void dw3_nx_mesh_debug_note_poly_code(int code, int textured) {
    unsigned idx;

    if (s_codes_logged >= MESH_DEBUG_FIRST_CODES) {
        return;
    }
    idx = (unsigned)code & 0xFFu;
    if (s_seen_codes[idx]) {
        return;
    }
    s_seen_codes[idx] = 1;
    s_codes_logged++;
    psyz_log(1, "nx_mesh_debug.c", __LINE__, __func__,
             "mesh poly code=0x%02x textured=%d (#%d)", code, textured,
             s_codes_logged);
}

void dw3_nx_mesh_debug_take_window(Dw3NxMeshDebugWindow *out) {
    if (!out) {
        return;
    }
    *out = s_win;
    out->mesh_available = dw3_nx_mesh_available() ? 1 : 0;
    s_win.tris_enqueued = 0;
    s_win.tris_flushed = 0;
    s_win.flush_us = 0;
    s_win.upload_us = 0;
    s_win.uploads = 0;
    s_win.upload_px = 0;
    s_win.present_us = 0;
    s_win.frames_mesh = 0;
    s_win.frames_blit = 0;
    s_win.soft_fallback = 0;
}

#endif /* DW3_NX_DEBUG_MESH */
