#ifndef DW3_MENU_BG_TILE_H
#define DW3_MENU_BG_TILE_H
/* P51 item 6: the field menu's background tile (sprite 0x1D of the status
 * sprites), one 96 x 96 period, for the Options screen. */
#include <stddef.h>
#include <stdint.h>

enum { MENU_BG_TILE = 96 };

/* Fills `rgba` (MENU_BG_TILE * MENU_BG_TILE * 4, opaque) from the disc files
 * of the running region. Returns 0, or -1 with a reason in `why`. `ms` gets the
 * time it took. Frame thread only (it reads through cdfs). */
/* Weak: the host unit tests of the overlay do not link the disc reader; the
 * caller treats a missing definition as "not available". */
int dw3_menu_bg_tile_load(uint8_t *rgba, double *ms, char *why, size_t why_cap)
    __attribute__((weak));

#endif
