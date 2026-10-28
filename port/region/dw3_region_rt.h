/* P6.24: the running region, as the host sees it.
 *
 * `dw3_region()` replaces the host's compile-time `#if VERSION_EU` tests:
 *   - plain `us` / `eu` images: a constant from the build's VERSION macro
 *     (inline; nothing changes);
 *   - `DW3_VERSION=both`: the region chosen at boot by dw3_region_select(),
 *     which also decides which game copy dw3_game() (dw3_game_api.h) reaches.
 *
 * dw3_region_resolve() is the choice itself, platform independent and
 * testable (tools/tests): a forced region (flag, then environment), else the
 * runtime folder that is given or present, else the stored preference. It
 * never draws a picker; the Switch picker (platforms/switch) and the PC
 * launcher use dw3_region.h's decision table before calling it. */
#ifndef DW3_REGION_RT_H
#define DW3_REGION_RT_H

#include <stddef.h>

#include "dw3_region.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(DW3_BOTH) && DW3_BOTH
Dw3Region dw3_region(void);
/* Fixes the running region (once); logs `dw3-region: ...`. Returns 0, or -1
 * for DW3_REGION_NONE or a second, different call. */
int dw3_region_select(Dw3Region r, const char *source);
#elif defined(VERSION_EU) && VERSION_EU
static inline Dw3Region dw3_region(void) { return DW3_REGION_EU; }
#else
static inline Dw3Region dw3_region(void) { return DW3_REGION_US; }
#endif

typedef struct Dw3RegionRequest {
    const char *flag;      /* --region value, or NULL */
    const char *env;       /* DW3_REGION value, or NULL */
    const char *root;      /* an explicit runtime folder (DW3_RUNTIME / --runtime), or NULL */
    const char *base;      /* folder holding us/ and eu/ (default "runtime") */
    const char *cfg_dir;   /* where port_options.cfg lives, or NULL */
} Dw3RegionRequest;

typedef struct Dw3RegionChoice {
    Dw3Region region;
    const char *source; /* flag | env | runtime-root | root-name | probe-one |
                           probe-pref | probe-last | default */
    Dw3RegionProbe probe; /* what the base folder holds (probe-* sources) */
} Dw3RegionChoice;

/* Returns 0 and fills `out`, or -1 when a flag or DW3_REGION names no region
 * (the caller stops: a typo must not boot the other disc). */
int dw3_region_resolve(const Dw3RegionRequest *req, Dw3RegionChoice *out);

#ifdef __cplusplus
}
#endif

#endif /* DW3_REGION_RT_H */
