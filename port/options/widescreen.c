/* P6.1 widescreen policy shared by PC and Switch game hosts. The renderer
 * owns the gutters (Psyz_VideoSetGutter); this only picks their width. */
#include "port_widescreen.h"

#include "overlay_host.h"
#include "port_options.h"

#include <psyz/log.h>
#include <psyz/video.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if VERSION_EU
#define DW3_WS_FIELDSTG 0x166
#define DW3_WS_FIGHTSTG 0x167
#define DW3_WS_STSTATUS 0x207
#define DW3_WS_STITSHOP 0x205
#define DW3_WS_STGMCARD 0x1EE
#define DW3_WS_STDGNAME 0x162
#define DW3_WS_STFGTREP 0x1D1
#else
#define DW3_WS_FIELDSTG 0x158
#define DW3_WS_FIGHTSTG 0x159
#define DW3_WS_STSTATUS 0x1F9
#define DW3_WS_STITSHOP 0x1F7
#define DW3_WS_STGMCARD 0x1E3
#define DW3_WS_STDGNAME 0x154
#define DW3_WS_STFGTREP 0x1C2
#endif

enum { DW3_PORT_OPT_WIDESCREEN = 0 };

static int g_env_read;
static int g_applied = -1;

int dw3_port_widescreen_enabled(void) {
    if (!g_env_read) {
        const char *env = getenv("DW3_WIDESCREEN");

        g_env_read = 1;
        /* On by default; DW3_WIDESCREEN=0 boots in 4:3. */
        if (env != NULL && (env[0] == '0' || env[0] == '1')) {
            dw3_port_options_apply_env(DW3_PORT_OPT_WIDESCREEN, env[0] == '1');
        }
    }
    return dw3_port_options_value(DW3_PORT_OPT_WIDESCREEN);
}

int dw3_port_widescreen_px(void) {
    int32_t mode_file;

    if (!dw3_port_widescreen_enabled()) {
        return 0;
    }
    mode_file = dw3_overlay_current_mode_file();
    if (mode_file == DW3_WS_FIELDSTG) {
        return DW3_WIDESCREEN_FIELD_PX;
    }
    if (mode_file == DW3_WS_FIGHTSTG) {
        return DW3_WIDESCREEN_BATTLE_PX;
    }
    /* Menus whose scrolling background reaches the gutters (patch 0053), and
     * the battle report (patch 0074) */
    if (mode_file == DW3_WS_STSTATUS || mode_file == DW3_WS_STITSHOP ||
        mode_file == DW3_WS_STGMCARD || mode_file == DW3_WS_STDGNAME ||
        mode_file == DW3_WS_STFGTREP) {
        return DW3_WIDESCREEN_MENU_PX;
    }
    return 0;
}

int dw3_port_widescreen_menu_shift(void) {
    return dw3_port_widescreen_px();
}

/* A sprite sheet (engine/gfx.h) is three or more s32 byte offsets from its
 * start: to its parts (7 s16 each: u, v, w, h, clut x, clut y, mode), to its
 * frame ids (bytes) and one per frame to the frame's list: s16 part count,
 * CLUT row, blend, then s16 part index, x, y per part (the last ones are
 * drawn on top). The band is written as a one-frame sheet of the same form. */
enum { BAND_PART_SIZE = 14, BAND_MAX_PARTS = 64, BAND_MAX_PART_INDEX = 256 };

void *dw3_port_widescreen_menu_band(void *sheet, int frame, int split) {
    /* s32 storage: the drawer reads the header as s32 */
    static int32_t storage[(16 + 3 * BAND_MAX_PARTS + BAND_MAX_PART_INDEX * BAND_PART_SIZE / 2) / 2 + 2];
    int16_t *out = (int16_t *)storage;
    const int32_t *head = sheet;
    const uint8_t *base = sheet;
    const uint8_t *ids;
    const int16_t *src;
    const int16_t *parts;
    int16_t *list;
    int shift = dw3_port_widescreen_menu_shift();
    int count;
    int seamEnd = INT32_MIN;
    int maxIndex = 0;
    int n;
    int i;
    int k;

    if (shift <= 0 || sheet == NULL) {
        return sheet;
    }
    ids = base + head[1];
    for (i = 0; ids[i] != frame; i++) {
        if (ids[i] == 0xFF || i >= 255) {
            return sheet;
        }
    }
    src = (const int16_t *)(base + head[i + 2]);
    parts = (const int16_t *)(base + head[0]);
    count = src[0];
    if (count <= 0 || count > BAND_MAX_PARTS) {
        return sheet;
    }
    for (k = 0; k < count; k++) {
        int index = src[3 + 3 * k];
        int x = src[3 + 3 * k + 1];

        if (index < 0 || index >= BAND_MAX_PART_INDEX) {
            return sheet;
        }
        maxIndex = index > maxIndex ? index : maxIndex;
        if (x < split && x + parts[index * 7 + 2] > seamEnd) {
            seamEnd = x + parts[index * 7 + 2];
        }
    }
    /* header (3 s32 = 6 s16), ids at byte 12, the frame at byte 16 */
    storage[1] = 12;
    storage[2] = 16;
    ((uint8_t *)out)[12] = (uint8_t)frame;
    ((uint8_t *)out)[13] = 0xFF;
    list = out + 8 + 3;
    n = 0;
    /* the fill first, so the rest is drawn over it: copies of the left
       group's last column from its end to its end + 2 * shift */
    if (seamEnd != INT32_MIN) {
        for (k = 0; k < count; k++) {
            int index = src[3 + 3 * k];
            int x = src[3 + 3 * k + 1];
            int w = parts[index * 7 + 2];
            int fx;

            if (x >= split || x + w != seamEnd || w <= 0) {
                continue;
            }
            for (fx = seamEnd; fx < seamEnd + 2 * shift; fx += w) {
                int at = fx + w > seamEnd + 2 * shift ? seamEnd + 2 * shift - w : fx;

                if (n + count >= BAND_MAX_PARTS) {
                    return sheet;
                }
                list[3 * n] = (int16_t)index;
                list[3 * n + 1] = (int16_t)(at - shift);
                list[3 * n + 2] = src[3 + 3 * k + 2];
                n++;
            }
        }
    }
    for (k = 0; k < count; k++) {
        int x = src[3 + 3 * k + 1];

        list[3 * n] = src[3 + 3 * k];
        list[3 * n + 1] = (int16_t)(x < split ? x - shift : x + shift);
        list[3 * n + 2] = src[3 + 3 * k + 2];
        n++;
    }
    out[8] = (int16_t)n;
    out[9] = src[1];
    out[10] = src[2];
    /* the parts table follows the list, its index unchanged */
    storage[0] = (int32_t)((const uint8_t *)(list + 3 * n) - (const uint8_t *)out);
    memcpy(list + 3 * n, parts, (size_t)(maxIndex + 1) * BAND_PART_SIZE);
    return storage;
}

int dw3_port_sprite_frame_bounds(const void *sheet, int frame, int *x0, int *y0, int *x1, int *y1) {
    const int32_t *head = sheet;
    const uint8_t *base = sheet;
    const uint8_t *ids;
    const int16_t *list;
    const int16_t *parts;
    int count;
    int i;
    int k;

    if (sheet == NULL) {
        return -1;
    }
    ids = base + head[1];
    for (i = 0; ids[i] != frame; i++) {
        if (ids[i] == 0xFF || i >= 255) {
            return -1;
        }
    }
    list = (const int16_t *)(base + head[i + 2]);
    parts = (const int16_t *)(base + head[0]);
    count = list[0];
    if (count <= 0 || count > BAND_MAX_PARTS) {
        return -1;
    }
    *x0 = *y0 = INT32_MAX;
    *x1 = *y1 = INT32_MIN;
    for (k = 0; k < count; k++) {
        int index = list[3 + 3 * k];
        int x = list[3 + 3 * k + 1];
        int y = list[3 + 3 * k + 2];

        if (index < 0 || index >= BAND_MAX_PART_INDEX) {
            return -1;
        }
        *x0 = x < *x0 ? x : *x0;
        *y0 = y < *y0 ? y : *y0;
        *x1 = x + parts[index * 7 + 2] > *x1 ? x + parts[index * 7 + 2] : *x1;
        *y1 = y + parts[index * 7 + 3] > *y1 ? y + parts[index * 7 + 3] : *y1;
    }
    return 0;
}

void dw3_port_widescreen_frame(void) {
    int px = dw3_port_widescreen_px();

    if (px == g_applied) {
        return;
    }
    g_applied = px;
    if (Psyz_VideoSetGutter((unsigned)px) != 0) {
        WARNF("dw3-widescreen: renderer has no gutters (%d px)", px);
    } else {
        /* psyz_log, not stderr: on Switch stderr only reaches nxlink, and
         * this line is the check in sdmc:/switch/dw3/dw3.log. */
        INFOF("dw3-widescreen: gutter %d px per side", px);
    }
}
