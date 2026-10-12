/* Diagnostic markers in the VRAM rectangle sampled by mesh present. */
#ifndef DW3_NX_MESH_DIAG_GEOMETRY_H
#define DW3_NX_MESH_DIAG_GEOMETRY_H

#include "nx_vram_pack.h"

static inline void dw3_nx_mesh_diag_quad(float out[16], float x0, float y0,
                                         float x1, float y1) {
    const float left = x0 * 2.0f / DW3_NX_VRAM_W - 1.0f;
    const float right = x1 * 2.0f / DW3_NX_VRAM_W - 1.0f;
    const float bottom = y0 * 2.0f / DW3_NX_VRAM_H - 1.0f;
    const float top = y1 * 2.0f / DW3_NX_VRAM_H - 1.0f;
    const float quad[16] = {
        left, bottom, 0, 1, right, bottom, 1, 1,
        left, top, 0, 0, right, top, 1, 0,
    };
    int i;
    for (i = 0; i < 16; i++) out[i] = quad[i];
}

static inline int dw3_nx_mesh_diag_geometry(int x, int y, int w, int h,
                                            float corner[16], float rim[16]) {
    if (x < 0 || y < 0 || w <= 0 || h <= 0 ||
        w > DW3_NX_VRAM_W - x || h > DW3_NX_VRAM_H - y) return 0;
    dw3_nx_mesh_diag_quad(corner, (float)x, (float)y,
                           x + w * 0.075f, y + h * 0.075f);
    dw3_nx_mesh_diag_quad(rim, x + w * 0.95f, (float)y,
                           (float)(x + w), (float)(y + h));
    return 1;
}

#endif
