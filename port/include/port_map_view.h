/* P6.32 phase 0: the game side of the new full-screen map screen (patch 0146).
 * The rest (state, scalers, host overlays) is port/map_view/map_view.h. */
#ifndef DW3_PORT_MAP_VIEW_H
#define DW3_PORT_MAP_VIEW_H

#ifdef __cplusplus
extern "C" {
#endif

/* Logical pad bit numbers (vendor/dw3_decomp include/engine/pad.h). */
enum {
    DW3_MV_PAD_SELECT = 0, DW3_MV_PAD_L3 = 1, DW3_MV_PAD_R3 = 2,
    DW3_MV_PAD_START = 3, DW3_MV_PAD_UP = 4, DW3_MV_PAD_RIGHT = 5,
    DW3_MV_PAD_DOWN = 6, DW3_MV_PAD_LEFT = 7, DW3_MV_PAD_L2 = 8,
    DW3_MV_PAD_R2 = 9, DW3_MV_PAD_L1 = 10, DW3_MV_PAD_R1 = 11,
    DW3_MV_PAD_CIRCLE = 12, DW3_MV_PAD_CROSS = 13, DW3_MV_PAD_TRIANGLE = 14,
    DW3_MV_PAD_SQUARE = 15
};

/* Called every frame of the game's Map screen from STSTATUS_updateMapScreen.
 * `sheet` is the map's sprite sheet (entry 0 of file `sheet_file`), `half` the
 * game half, (home_x, home_y) the current area's mark and spot_x / spot_y the
 * 47 spot positions in map space; visited[i] is spot i + 1's flag. `held` and
 * `pressed` are the logical pad as bit masks (bit = DW3_MV_PAD_*).
 * `sheet1` / `sheet2` are the entries 1 and 2 of the same file (the town cells
 * and the area cells) and `pieces` the reveal layer the game draws over the
 * dark map: `npieces` times 4 ints (kind 1 = town cell of sheet1, 2 = visited
 * area cell of sheet2; frame; x; y in map space, before the scroll), in draw
 * order. The host composes them into the picture it shows. Returns 1
 * while the new screen has the pad (the game then skips its own pad read and
 * the area lookup and only draws its frozen screen), 0 otherwise. Square opens
 * it (widescreen on, option file key `map_screen` not 0); Triangle or Square
 * close it. */
int dw3_port_map_view_frame(const void *sheet, int sheet_file, int half,
                            int home_x, int home_y, const int *visited,
                            const int *spot_x, const int *spot_y,
                            unsigned held, unsigned pressed, const void *sheet1,
                            const void *sheet2, const int *pieces,
                            int npieces);
int dw3_port_map_view_active(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_MAP_VIEW_H */
