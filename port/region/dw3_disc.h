/* Disc images waiting on the card (issue #322).
 *
 * The separate builds find a Mode2/2352 BIN/CUE in the runtime folder or in its
 * parent (dw3_str_find_disc, port/movies/str_disc.c). The combined image has
 * two runtime folders (<base>/us and <base>/eu) and one parent (<base>), so it
 * also has to tell WHICH region an image in the parent belongs to: the
 * executable in the ISO9660 root directory says so (SLUS_014.36 = US,
 * SLES_039.36 = EU). Platform independent: plain stdio/dirent, tested on the
 * host with fake directory trees (tools/tests/region_boot_main.c). */
#ifndef DW3_DISC_H
#define DW3_DISC_H

#include <stddef.h>

#include "dw3_region.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DW3_DISC_PATH 512

typedef struct Dw3DiscHit {
    int found;
    int in_parent;           /* 1: the image sits in <base>, not in <base>/<r> */
    char path[DW3_DISC_PATH]; /* the BIN (or the image the region folder holds) */
} Dw3DiscHit;

typedef struct Dw3DiscScan {
    Dw3DiscHit hit[2];       /* [0] US, [1] EU (index = region - DW3_REGION_US) */
    int unidentified;        /* images in <base> that are neither a US nor an EU disc */
    char unidentified_first[DW3_DISC_PATH];
} Dw3DiscScan;

/* Which region the Mode2/2352 image `bin_path` is: the ISO9660 root directory
 * holds SLUS_014.36 (US) or SLES_039.36 (EU). DW3_REGION_NONE when it cannot
 * be read or holds neither. */
Dw3Region dw3_disc_region_of(const char *bin_path);

/* Looks for an image for each region that `tree` says has no complete tree:
 *   1. any .cue/.bin/.iso in <base>/<r>/ (boot_tim.bin is never one); the
 *      runtime extraction reads that folder, so that region counts as present;
 *   2. else an image in <base>/ (the parent) whose executable names region r.
 * Regions whose tree is complete are not looked at. */
void dw3_disc_scan(const char *base, const Dw3RegionProbe *tree, Dw3DiscScan *out);

#ifdef __cplusplus
}
#endif

#endif /* DW3_DISC_H */
