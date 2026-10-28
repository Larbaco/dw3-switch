/* Find the owner's Mode2/2352 disc image for the STR rewrite.
 *
 * boot_tim.bin lives in the runtime root and is never a disc. A CUE whose
 * FILE name is missing still yields the only other .bin in that directory
 * when the file is large enough to cover the sectors about to be copied.
 * The runtime directory is tried first, then its parent.
 */
#ifndef PORT_STR_DISC_H
#define PORT_STR_DISC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The combined build (issue #322) has already identified which image belongs to
 * the running region (port/region/dw3_disc.c); dw3_str_find_disc returns it
 * first. NULL or "" clears it. */
void dw3_str_set_disc_hint(const char *bin_path);

/* 0 and writes the image path. min_bytes is the smallest file that can
 * cover those reads (0 skips the size check). */
int dw3_str_find_disc(const char *runtime_root, char *out, size_t n,
                      unsigned long long min_bytes);

#ifdef __cplusplus
}
#endif

#endif /* PORT_STR_DISC_H */
