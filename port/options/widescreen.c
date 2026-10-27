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
#define DW3_WS_STDWTITL 0x1D0
#define DW3_WS_STPLNMET 0x206
#define DW3_WS_STCRDABM 0x185
#define DW3_WS_STCRDDEK 0x1CA
#define DW3_WS_STCRDSHP 0x1CE
#define DW3_WS_STGTRAIN 0x1F2
#define DW3_WS_STGDGLAB 0x1EC
#else
#define DW3_WS_FIELDSTG 0x158
#define DW3_WS_FIGHTSTG 0x159
#define DW3_WS_STSTATUS 0x1F9
#define DW3_WS_STITSHOP 0x1F7
#define DW3_WS_STGMCARD 0x1E3
#define DW3_WS_STDGNAME 0x154
#define DW3_WS_STFGTREP 0x1C2
#define DW3_WS_STDWTITL 0x1C1
#define DW3_WS_STPLNMET 0x1F8
#define DW3_WS_STCRDABM 0x177
#define DW3_WS_STCRDDEK 0x1BC
#define DW3_WS_STCRDSHP 0x1C0
#define DW3_WS_STGTRAIN 0x1F6
#define DW3_WS_STGDGLAB 0x1DF
#endif

enum { DW3_PORT_OPT_WIDESCREEN = 0 };

static int g_env_read;
static int g_applied = -1;
/* dw3_port_widescreen_frame calls; the title backdrop's last draw (0085) */
static unsigned g_frame_no;
static unsigned g_title_frame = 0x80000000u;

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
    /* The card album and the deck editor (patch 0100): their scrolling
     * backdrop reaches the gutters */
    if (mode_file == DW3_WS_STCRDABM || mode_file == DW3_WS_STCRDDEK) {
        return DW3_WIDESCREEN_MENU_PX;
    }
    /* The card packs shop (patch 0102), the gym (0103) and the Digimon Lab
     * (0104): the same backdrop */
    if (mode_file == DW3_WS_STCRDSHP || mode_file == DW3_WS_STGTRAIN || mode_file == DW3_WS_STGDGLAB) {
        return DW3_WIDESCREEN_MENU_PX;
    }
    /* The player name screen (Digimon Online registration): its scrolling
     * backdrop and side wings reach the gutters (patch 0087) */
    if (mode_file == DW3_WS_STPLNMET) {
        return DW3_WIDESCREEN_MENU_PX;
    }
    /* The title overlay also plays the movies and the still screen: only the
     * title screen, whose backdrop is drawn mirrored into the gutters (patch
     * 0085), gets them. The backdrop marks every frame it is drawn. */
    if (mode_file == DW3_WS_STDWTITL && g_frame_no - g_title_frame <= 1u) {
        return DW3_WIDESCREEN_MENU_PX;
    }
    return 0;
}

/* The screen a restart lands on is drawing: the title once its backdrop is
 * drawn (US and EU), or the EU language select (restart.c fades in then). */
int dw3_port_widescreen_new_screen_ready(void) {
    if (g_frame_no - g_title_frame <= 1u) {
        return 1;
    }
#if VERSION_EU
    return dw3_overlay_current_mode_file() == 0x165; /* CNTY_SEL */
#else
    return 0;
#endif
}

void dw3_port_widescreen_title_backdrop(void) {
    g_title_frame = g_frame_no;
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

static void *menu_band(void *sheet, int frame, int split, int extend) {
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
    int fillIndex = -1;
    int fillFrom = INT32_MIN;
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
    /* extend: the plate's body tile (the part most often repeated in the
       frame, not the one that ends it) is the fill, from where the last one
       ends */
    if (extend) {
        int best = 0;

        for (k = 0; k < count; k++) {
            int index = src[3 + 3 * k];
            int same = 0;
            int j;

            for (j = 0; j < count; j++) {
                same += src[3 + 3 * j] == index;
            }
            if (same > best && parts[index * 7 + 2] > 0) {
                best = same;
                fillIndex = index;
            }
        }
        if (fillIndex < 0 || best < 2) {
            return sheet;
        }
        for (k = 0; k < count; k++) {
            if (src[3 + 3 * k] == fillIndex && src[3 + 3 * k + 1] + parts[fillIndex * 7 + 2] > fillFrom) {
                fillFrom = src[3 + 3 * k + 1] + parts[fillIndex * 7 + 2];
            }
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

            if (extend ? (index != fillIndex || x + w != fillFrom) : (x >= split || x + w != seamEnd || w <= 0)) {
                continue;
            }
            for (fx = extend ? fillFrom : seamEnd; fx < seamEnd + 2 * shift; fx += w) {
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

void *dw3_port_widescreen_menu_band(void *sheet, int frame, int split) {
    return menu_band(sheet, frame, split, 0);
}

void *dw3_port_widescreen_menu_extend(void *sheet, int frame) {
    return menu_band(sheet, frame, 0x7FFF, 1);
}

/* One-frame sheets for patches 0085-0087, in the format above: a frame's part
 * list (with added parts) and a copy of the parts table (with added parts). */
enum { WIDE_MAX_PARTS = 400, WIDE_MAX_INDEX = 160, WIDE_STRIP = 8 };

typedef struct WideSheet {
    int16_t row;
    int16_t blend;
    int n;
    int16_t list[WIDE_MAX_PARTS][3];
    int nparts;
    int16_t parts[WIDE_MAX_INDEX][7];
} WideSheet;

static WideSheet g_wide;
static int32_t g_wide_storage[(16 + 6 * WIDE_MAX_PARTS + 14 * WIDE_MAX_INDEX) / 4 + 4];

/* Frame `frame` of `sheet` into g_wide; 0 when it does not fit */
static int wide_load(const void *sheet, int frame) {
    const int32_t *head = sheet;
    const uint8_t *base = sheet;
    const uint8_t *ids;
    const int16_t *src;
    const int16_t *parts;
    int count;
    int maxIndex = 0;
    int i;
    int k;

    if (sheet == NULL) {
        return 0;
    }
    ids = base + head[1];
    for (i = 0; ids[i] != frame; i++) {
        if (ids[i] == 0xFF || i >= 255) {
            return 0;
        }
    }
    src = (const int16_t *)(base + head[i + 2]);
    parts = (const int16_t *)(base + head[0]);
    count = src[0];
    if (count <= 0 || count > WIDE_MAX_PARTS) {
        return 0;
    }
    g_wide.row = src[1];
    g_wide.blend = src[2];
    for (k = 0; k < count; k++) {
        int index = src[3 + 3 * k];

        if (index < 0 || index >= WIDE_MAX_INDEX) {
            return 0;
        }
        maxIndex = index > maxIndex ? index : maxIndex;
        g_wide.list[k][0] = (int16_t)index;
        g_wide.list[k][1] = src[3 + 3 * k + 1];
        g_wide.list[k][2] = src[3 + 3 * k + 2];
    }
    g_wide.n = count;
    g_wide.nparts = maxIndex + 1;
    memcpy(g_wide.parts, parts, (size_t)g_wide.nparts * BAND_PART_SIZE);
    return 1;
}

/* g_wide as a sheet with the one frame `frame` */
static void *wide_emit(int frame) {
    int16_t *out = (int16_t *)g_wide_storage;
    int16_t *list = out + 8 + 3;

    g_wide_storage[1] = 12;
    g_wide_storage[2] = 16;
    ((uint8_t *)out)[12] = (uint8_t)frame;
    ((uint8_t *)out)[13] = 0xFF;
    out[8] = (int16_t)g_wide.n;
    out[9] = g_wide.row;
    out[10] = g_wide.blend;
    memcpy(list, g_wide.list, (size_t)g_wide.n * 6);
    g_wide_storage[0] = (int32_t)((const uint8_t *)(list + 3 * g_wide.n) - (const uint8_t *)out);
    memcpy(list + 3 * g_wide.n, g_wide.parts, (size_t)g_wide.nparts * BAND_PART_SIZE);
    return g_wide_storage;
}

void *dw3_port_widescreen_periodic(void *sheet, int frame, int x, int y, int dx, int dy) {
    int gutter = dw3_port_widescreen_px();
    int n0;
    int sorted = 1;
    int i;
    int j;
    int k;

    if (gutter <= 0 || (dx == 0 && dy == 0) || !wide_load(sheet, frame)) {
        return sheet;
    }
    n0 = g_wide.n;
    for (i = 1; i < n0; i++) {
        if (g_wide.list[i][1] < g_wide.list[i - 1][1] ||
            (g_wide.list[i][1] == g_wide.list[i - 1][1] && g_wide.list[i][2] < g_wide.list[i - 1][2])) {
            sorted = 0;
        }
    }
    for (k = -3; k <= 3; k++) {
        if (k == 0) {
            continue;
        }
        for (i = 0; i < n0; i++) {
            const int16_t *p = g_wide.parts[g_wide.list[i][0]];
            int px = g_wide.list[i][1] + k * dx;
            int py = g_wide.list[i][2] + k * dy;
            int sx = x + px;
            int sy = y + py;
            int dup = 0;

            /* only copies that show in a gutter */
            if (!(sx < 0 || sx + p[2] > 320) || sx + p[2] <= -gutter || sx >= 320 + gutter ||
                sy + p[3] <= 0 || sy >= 240) {
                continue;
            }
            for (j = 0; j < g_wide.n && !dup; j++) {
                dup = g_wide.list[j][0] == g_wide.list[i][0] && g_wide.list[j][1] == px &&
                      g_wide.list[j][2] == py;
            }
            if (dup) {
                continue;
            }
            if (g_wide.n >= WIDE_MAX_PARTS) {
                return sheet;
            }
            g_wide.list[g_wide.n][0] = g_wide.list[i][0];
            g_wide.list[g_wide.n][1] = (int16_t)px;
            g_wide.list[g_wide.n][2] = (int16_t)py;
            g_wide.n++;
        }
    }
    if (sorted) {
        /* the pattern's own order (columns, then rows) with the copies in it */
        for (i = n0; i < g_wide.n; i++) {
            int16_t e[3];

            memcpy(e, g_wide.list[i], sizeof(e));
            for (j = i; j > 0 && (g_wide.list[j - 1][1] > e[1] ||
                                  (g_wide.list[j - 1][1] == e[1] && g_wide.list[j - 1][2] > e[2]));
                 j--) {
                memcpy(g_wide.list[j], g_wide.list[j - 1], sizeof(e));
            }
            memcpy(g_wide.list[j], e, sizeof(e));
        }
    } else {
        /* the copies under the original parts */
        int16_t tmp[WIDE_MAX_PARTS][3];
        int added = g_wide.n - n0;

        memcpy(tmp, g_wide.list[n0], (size_t)added * 6);
        memmove(g_wide.list[added], g_wide.list[0], (size_t)n0 * 6);
        memcpy(g_wide.list[0], tmp, (size_t)added * 6);
    }
    return wide_emit(frame);
}

void *dw3_port_widescreen_wings(void *sheet, int frame, int x) {
    int gutter = dw3_port_widescreen_px();
    int16_t out[WIDE_MAX_PARTS][3];
    int n = 0;
    int i;

    if (gutter <= 0 || !wide_load(sheet, frame)) {
        return sheet;
    }
    for (i = 0; i < g_wide.n; i++) {
        int index = g_wide.list[i][0];
        int sx = x + g_wide.list[i][1];
        int w = g_wide.parts[index][2];
        int side;

        for (side = 0; side < 2; side++) {
            int from;
            int to;
            int strip;
            int fx;

            if (side == 0 ? sx != 0 : sx + w != 320) {
                continue;
            }
            if (w < WIDE_STRIP || g_wide.nparts >= WIDE_MAX_INDEX) {
                return sheet;
            }
            /* a part of the outer WIDE_STRIP columns of this one */
            strip = g_wide.nparts++;
            memcpy(g_wide.parts[strip], g_wide.parts[index], BAND_PART_SIZE);
            if (side == 1) {
                g_wide.parts[strip][0] = (int16_t)(g_wide.parts[index][0] + w - WIDE_STRIP);
            }
            g_wide.parts[strip][2] = WIDE_STRIP;
            from = side == 0 ? -gutter : 320;
            to = side == 0 ? 0 : 320 + gutter;
            for (fx = from; fx < to; fx += WIDE_STRIP) {
                int at = fx + WIDE_STRIP > to ? to - WIDE_STRIP : fx;

                if (n >= WIDE_MAX_PARTS - 1) {
                    return sheet;
                }
                out[n][0] = (int16_t)strip;
                out[n][1] = (int16_t)(at - x);
                out[n][2] = g_wide.list[i][2];
                n++;
            }
        }
        if (n >= WIDE_MAX_PARTS) {
            return sheet;
        }
        memcpy(out[n], g_wide.list[i], 6);
        n++;
    }
    memcpy(g_wide.list, out, (size_t)n * 6);
    g_wide.n = n;
    return wide_emit(frame);
}

void dw3_port_widescreen_frame(void) {
    int px = dw3_port_widescreen_px();
    int prev;

    g_frame_no++;
    if (px == g_applied) {
        return;
    }
    prev = g_applied;
    g_applied = px;
    if (Psyz_VideoSetGutter((unsigned)px) != 0) {
        WARNF("dw3-widescreen: renderer has no gutters (%d px)", px);
    } else {
        /* psyz_log, not stderr: on Switch stderr only reaches nxlink, and
         * this line is the check in sdmc:/switch/dw3/dw3.log. */
        INFOF("dw3-widescreen: gutter %d px per side (was %d; %s) mode file "
              "0x%x frame %u", px, prev, px ? "16:9" : "4:3 picture",
              (unsigned)dw3_overlay_current_mode_file(), g_frame_no);
    }
}

int dw3_port_widescreen_copy_gutters(int src_y, int h, int dx, int dy) {
    int px;

    if (dw3_port_widescreen_px() == 0) {
        return 0;
    }
    px = Psyz_VideoCopyGutters(src_y, h, dx, dy);
    INFOF("dw3-widescreen: gutters copied beside the image at x %d: %d px", dx, px);
    return px > 0 ? px : 0;
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
