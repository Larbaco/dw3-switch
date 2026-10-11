/* One-shot battle present dumps for OLED triage (SD card, not nxlink bulk).
 * Enabled when DW3_NX_DUMP_VRAM=1 (default ON for --nxlink CMake builds). */
#ifndef DW3_NX_VRAM_DUMP_H
#define DW3_NX_VRAM_DUMP_H

#include <stdint.h>

#ifndef DW3_NX_DUMP_VRAM
#define DW3_NX_DUMP_VRAM 0
#endif

#if DW3_NX_DUMP_VRAM

/* First mesh+ui present only: write display-rect CPU VRAM + mesh FBO to
 * sdmc:/switch/dw3/ and log one nxlink line. Later calls are no-ops. */
void dw3_nx_vram_dump_battle_once(const uint16_t *vram, int dx, int dy, int dw,
                                  int dh, int mesh_active);

#else

#define dw3_nx_vram_dump_battle_once(vram, dx, dy, dw, dh, mesh_active) \
    ((void)0)

#endif /* DW3_NX_DUMP_VRAM */

#endif /* DW3_NX_VRAM_DUMP_H */
