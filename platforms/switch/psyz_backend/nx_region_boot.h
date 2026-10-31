/* P6.24: the Switch side of the combined image's boot flow.
 *
 * The decisions, the stored choice and the log lines are port/region/
 * dw3_region_boot.c (host-tested). This file supplies what only the console
 * has: the hold-Minus sample, the picker and setup screens drawn on the EGL
 * surface with the 8x8 HUD font (the same plain drawing as the crash notice,
 * no shader or texture), and the disc-image check. Only used by a
 * `DW3_VERSION=both` build. */
#ifndef DW3_NX_REGION_BOOT_H
#define DW3_NX_REGION_BOOT_H

#include "dw3_region_boot.h"

/* Runs the whole flow for `base` (sdmc:/switch/dw3). `forced` / `forced_by`
 * skip it (replay build, DW3_REGION). Brings the GPU up itself when it has a
 * screen to show (dw3_nx_gpu_init is idempotent). `relaunch` (P6.38) is the
 * region a previous process asked for with --relaunch-region, or NONE. */
void dw3_nx_region_boot(const char *base, Dw3Region forced, const char *forced_by,
                        Dw3Region relaunch, Dw3RegionBootOut *out);

#endif /* DW3_NX_REGION_BOOT_H */
