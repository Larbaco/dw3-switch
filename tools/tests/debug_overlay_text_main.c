/* Host driver for platforms/switch/source/debug_overlay_text.h */
#include "debug_overlay_text.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    char lines[DW3_DBG_OVERLAY_MAX_LINES][DW3_DBG_OVERLAY_LINE_CHARS];
    Dw3DebugOverlayMeshSnap mesh;
    Dw3DebugOverlayBuildFlags build;
    int n;
    int i;

    printf("fps60=%.1f\n", dw3_debug_overlay_fps(1000000ull));
    printf("fps30=%.1f\n", dw3_debug_overlay_fps(2000000ull));
    printf("fps0=%.1f\n", dw3_debug_overlay_fps(0));

    memset(&mesh, 0, sizeof(mesh));
    mesh.valid = 1;
    mesh.mesh_available = 1;
    mesh.tris_enqueued = 10;
    mesh.tris_flushed = 8;
    mesh.flush_us = 100;
    mesh.upload_us = 20;
    mesh.present_us = 50;
    mesh.frames_mesh = 58;
    mesh.frames_blit = 2;
    mesh.soft_fallback = 0;
    build.nxlink = 1;
    build.mesh_dbg = 1;
    build.dump_vram = 1;
    build.mem_dbg = 1;

    {
        Dw3DebugOverlayMemSnap mem;
        Dw3DebugOverlayMenuSnap menu;

        memset(&menu, 0, sizeof(menu));
        menu.valid = 1;
        menu.updates = 4;
        menu.compose_us = 310;
        menu.upload_us = 120;
        menu.total_max_us = 900;
        memset(&mem, 0, sizeof(mem));
        mem.valid = 1;
        mem.heap_ok = 1;
        mem.heap_used_kib = 1200;
        mem.heap_arena_kib = 8192;
        mem.heap_used_hw_kib = 1400;
        mem.file_cache_kib = 200;
        mem.file_cache_hw_kib = 250;
        mem.libc_used_kib = 400;
        mem.libc_used_hw_kib = 450;

        n = dw3_debug_overlay_format(lines, DW3_DBG_OVERLAY_MAX_LINES,
                                     1000000ull, 0x15a, 1, "soft", &mesh, &mem,
                                     &menu, &build);
    }
    printf("line_count=%d\n", n);
    for (i = 0; i < n; i++) {
        printf("L%d=%s\n", i, lines[i]);
    }
    return 0;
}
