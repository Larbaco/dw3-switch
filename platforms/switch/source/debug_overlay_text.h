/* Host-testable text helpers for the Switch debug overlay HUD.
 * No libnx / GLES — boot and pytest compile this with plain gcc. */
#ifndef DW3_DEBUG_OVERLAY_TEXT_H
#define DW3_DEBUG_OVERLAY_TEXT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define DW3_DBG_OVERLAY_MAX_LINES 12
#define DW3_DBG_OVERLAY_LINE_CHARS 40

typedef struct Dw3DebugOverlayMeshSnap {
    int valid; /* 0 = mesh window unavailable this sample */
    int mesh_available;
    uint64_t tris_enqueued;
    uint64_t tris_flushed;
    uint64_t flush_us;
    uint64_t upload_us;
    uint64_t present_us;
    uint32_t frames_mesh;
    uint32_t frames_blit;
    uint32_t soft_fallback;
} Dw3DebugOverlayMeshSnap;

typedef struct Dw3DebugOverlayBuildFlags {
    int nxlink;
    int mesh_dbg;
    int dump_vram;
} Dw3DebugOverlayBuildFlags;

/* Hz from a 60-pump microsecond window. Returns 0 if dt_us == 0. */
static inline double dw3_debug_overlay_fps(uint64_t dt_us) {
    if (dt_us == 0) {
        return 0.0;
    }
    return (60.0 * 1000000.0) / (double)dt_us;
}

/* Fill lines[] with NUL-terminated HUD rows. Returns line count (≤ max_lines).
 * battle_mode is a short token (soft / soft+bbox / gpu-solid / gpu-tex), or
 * NULL → "soft". */
static inline int dw3_debug_overlay_format(
    char lines[][DW3_DBG_OVERLAY_LINE_CHARS], int max_lines, uint64_t dt_us,
    int32_t mode_file, int use_mesh, const char *battle_mode,
    const Dw3DebugOverlayMeshSnap *mesh,
    const Dw3DebugOverlayBuildFlags *build) {
    int n = 0;
    double fps;
    char build_buf[DW3_DBG_OVERLAY_LINE_CHARS];
    int bp = 0;

    if (!lines || max_lines <= 0) {
        return 0;
    }

#define DW3_OL_LINE(...)                                                       \
    do {                                                                       \
        if (n < max_lines) {                                                   \
            snprintf(lines[n], DW3_DBG_OVERLAY_LINE_CHARS, __VA_ARGS__);       \
            n++;                                                               \
        }                                                                      \
    } while (0)

    DW3_OL_LINE("dw3 dbg");
    fps = dw3_debug_overlay_fps(dt_us);
    if (dt_us == 0) {
        DW3_OL_LINE("fps --  dt=--");
    } else {
        DW3_OL_LINE("fps ~%.1f  dt=%llu", fps, (unsigned long long)dt_us);
    }
    DW3_OL_LINE("mode file=0x%x", (unsigned)mode_file);
    DW3_OL_LINE("battle=%s", battle_mode ? battle_mode : "soft");
    DW3_OL_LINE("mesh path=%s", use_mesh ? "mesh+ui" : "opaque");

    if (mesh && mesh->valid) {
        DW3_OL_LINE("avail=%d tris %llu/%llu", mesh->mesh_available,
                    (unsigned long long)mesh->tris_enqueued,
                    (unsigned long long)mesh->tris_flushed);
        DW3_OL_LINE("frames m=%u b=%u soft=%u", mesh->frames_mesh,
                    mesh->frames_blit, mesh->soft_fallback);
        DW3_OL_LINE("us f=%llu u=%llu p=%llu",
                    (unsigned long long)mesh->flush_us,
                    (unsigned long long)mesh->upload_us,
                    (unsigned long long)mesh->present_us);
    } else {
        DW3_OL_LINE("mesh win=n/a (no DW3_NX_DEBUG_MESH)");
    }

    build_buf[0] = '\0';
    if (build) {
        if (build->nxlink) {
            bp += snprintf(build_buf + bp, sizeof(build_buf) - (size_t)bp,
                           "%snxlink", bp ? " " : "");
        }
        if (build->mesh_dbg) {
            bp += snprintf(build_buf + bp, sizeof(build_buf) - (size_t)bp,
                           "%smesh", bp ? " " : "");
        }
        if (build->dump_vram) {
            bp += snprintf(build_buf + bp, sizeof(build_buf) - (size_t)bp,
                           "%sdump", bp ? " " : "");
        }
    }
    if (bp == 0) {
        DW3_OL_LINE("build: (none)");
    } else {
        DW3_OL_LINE("build: %s", build_buf);
    }

#undef DW3_OL_LINE
    return n;
}

#endif /* DW3_DEBUG_OVERLAY_TEXT_H */
