/* Test stand-in for platforms/pc/src/menu_bg_tile.c (it reads the disc): a
 * synthetic 96 x 96 tile, so the host tests can run the Options art in the mode
 * where the scrolling background is a separate quad (D4 / A18). No game data. */
#include "../../platforms/pc/src/menu_bg_tile.h"

int dw3_menu_bg_tile_load(uint8_t *rgba, double *ms, char *why, size_t why_cap) {
    (void)why;
    (void)why_cap;
    for (int y = 0; y < MENU_BG_TILE; ++y)
        for (int x = 0; x < MENU_BG_TILE; ++x) {
            uint8_t *p = rgba + ((size_t)y * MENU_BG_TILE + (size_t)x) * 4;
            p[0] = (uint8_t)(x * 2);
            p[1] = (uint8_t)(y * 2);
            p[2] = (uint8_t)(80 + ((x / 48 + y / 48) & 1) * 100);
            p[3] = 255;
        }
    *ms = 0.0;
    return 0;
}
