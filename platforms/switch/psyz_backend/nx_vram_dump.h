/* One-shot battle present dumps for OLED triage (SD card, not nxlink bulk).
 * Enabled when DW3_NX_DUMP_VRAM=1 (default ON for --nxlink CMake builds). */
#ifndef DW3_NX_VRAM_DUMP_H
#define DW3_NX_VRAM_DUMP_H

#include <stdint.h>

#ifndef DW3_NX_DUMP_VRAM
#define DW3_NX_DUMP_VRAM 0
#endif

#if DW3_NX_DUMP_VRAM

/* One-shot FIGHTSTG present dump to sdmc:/switch/dw3/. Mesh path waits until
 * prims_flushed is large enough (first ~20-tri present was all-black). Soft
 * battle (opaque blit) dumps on the first FIGHTSTG present. */
void dw3_nx_vram_dump_battle_once(const uint16_t *vram, int dx, int dy, int dw,
                                  int dh, int mesh_active,
                                  uint64_t prims_flushed);

#else

#define dw3_nx_vram_dump_battle_once(vram, dx, dy, dw, dh, mesh_active, prims) \
    ((void)0)

#endif /* DW3_NX_DUMP_VRAM */

#endif /* DW3_NX_VRAM_DUMP_H */
