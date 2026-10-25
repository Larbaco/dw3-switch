/* P6.24 slice 2: the pure parts of the Switch launcher NRO
 * (platforms/switch/launcher/). No libnx here, so tools/tests can build it on
 * the host:
 *   - where a region's game NRO lives on the SD card,
 *   - the plain ASCII text of every screen the launcher can show.
 * The launcher draws the text on libnx's console; nothing in this file knows
 * how. */
#ifndef DW3_LAUNCHER_H
#define DW3_LAUNCHER_H

#include "dw3_region.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Default SD folder of the launcher: the region trees are `<base>/us` and
 * `<base>/eu`, the region NROs sit next to the launcher (default chosen,
 * owner can change). */
#define DW3_LAUNCHER_BASE "sdmc:/switch/dw3"

/* Finds the game NRO of a region under `base`. Candidates, in order:
 *   <base>/dw3-<r>.nro          (documented name)
 *   <base>/dw3-switch-<r>.nro   (release asset name plus the region)
 * Copies the first one that exists into `out` and returns 1. When none
 * exists `out` holds the documented name (for the message) and the result is
 * 0. Returns 0 with an empty `out` for DW3_REGION_NONE. */
int dw3_launcher_nro_path(const char *base, Dw3Region r, char *out, size_t cap);

typedef enum Dw3LauncherScreen {
    DW3_SCREEN_PICKER = 0,   /* both regions: the picker */
    DW3_SCREEN_NO_DATA,      /* neither region found: setup message */
    DW3_SCREEN_NO_NRO,       /* region chosen but its NRO is missing */
    DW3_SCREEN_NO_LOADER,    /* envSetNextLoad unavailable (not from hbloader) */
    DW3_SCREEN_LOAD_FAILED   /* envSetNextLoad returned an error */
} Dw3LauncherScreen;

/* Formats one screen into `out` (always NUL terminated, lines end in '\n').
 *   picker      : the picker state `p` (required)
 *   NO_DATA     : `base`
 *   NO_NRO      : `region` and `path` (the documented NRO name)
 *   NO_LOADER   : `region` and `path`
 *   LOAD_FAILED : `region`, `path` and `rc` (libnx Result)
 * Returns the number of characters written (not counting the NUL). */
size_t dw3_launcher_text(Dw3LauncherScreen kind, const Dw3Picker *p,
                         const char *base, Dw3Region region, const char *path,
                         unsigned rc, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* DW3_LAUNCHER_H */
