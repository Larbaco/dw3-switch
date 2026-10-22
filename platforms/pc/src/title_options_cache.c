#include "title_options_cache.h"
#include "title_overlay_art.h"

#include <stdlib.h>
#include <string.h>

/* Layout of the Port options screen in canvas pixels. Must cover everything
 * options_screen() draws for its FOCUS / VALUE / DESCRIPTION layers; the
 * unit test proves it against the full reference render. */
enum {
    ROW_X = 132, ROW_Y = 212, ROW_W = 696, ROW_H = 52, ROW_STEP = 68,
    VALUE_X = 684, VALUE_W = 144,
    DESC_Y = 500,
};

static uint8_t *g_static;
static uint8_t *g_canvas;
static int g_valid;
static unsigned g_art_generation;
static int g_cursor = -1;
static int g_values[DW3_TITLE_OPTION_ROWS];
/* The host must still receive the whole canvas. */
static int g_full_pending = 1;
/* update() reported rects that commit() has not acknowledged yet. */
static int g_awaiting_commit;

static Dw3OptionsRect row_rect(int row) {
    return (Dw3OptionsRect){ROW_X, ROW_Y + ROW_STEP * row, ROW_W, ROW_H};
}
static Dw3OptionsRect value_rect(int row) {
    return (Dw3OptionsRect){VALUE_X, ROW_Y + ROW_STEP * row, VALUE_W, ROW_H};
}
static Dw3OptionsRect description_rect(void) {
    return (Dw3OptionsRect){ROW_X, DESC_Y, ROW_W, ROW_H};
}

static int intersect(Dw3OptionsRect a, Dw3OptionsRect b, Dw3OptionsRect *out) {
    const int x0 = a.x > b.x ? a.x : b.x;
    const int y0 = a.y > b.y ? a.y : b.y;
    const int x1 = a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w;
    const int y1 = a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h;
    if (x0 >= x1 || y0 >= y1) return 0;
    if (out) *out = (Dw3OptionsRect){x0, y0, x1 - x0, y1 - y0};
    return 1;
}

static void restore_static(Dw3OptionsRect r) {
    for (int y = 0; y < r.h; ++y)
        memcpy(g_canvas + (size_t)(r.y + y) * DW3_OPTIONS_CANVAS_PITCH + r.x * 4,
               g_static + (size_t)(r.y + y) * DW3_OPTIONS_CANVAS_PITCH + r.x * 4,
               (size_t)r.w * 4);
}

static void draw(Dw3OptionsRect r, int layer, int row, int value) {
    dw3_pc_title_overlay_options_component(g_canvas, DW3_OPTIONS_CANVAS_PITCH,
        layer, row, value, r.x, r.y, r.w, r.h, 0);
}

/* Draw the focus box, values and description that fall inside rect. */
static void draw_state_in(Dw3OptionsRect rect, int cursor, const int *values) {
    Dw3OptionsRect part;
    for (int row = 0; row < DW3_TITLE_OPTION_ROWS; ++row) {
        if (cursor == row && intersect(rect, row_rect(row), &part))
            draw(part, DW3_TITLE_OPTIONS_FOCUS, row, 0);
        if (intersect(rect, value_rect(row), &part))
            draw(part, DW3_TITLE_OPTIONS_VALUE, row, values[row]);
    }
    if (cursor >= 0 && cursor < DW3_TITLE_OPTION_ROWS &&
        intersect(rect, description_rect(), &part))
        draw(part, DW3_TITLE_OPTIONS_DESCRIPTION, cursor, 0);
}

int dw3_options_cache_update(Dw3OptionsDirty *out) {
    const size_t canvas_bytes = (size_t)DW3_OPTIONS_CANVAS_PITCH * DW3_OPTIONS_CANVAS_H;
    const unsigned art_generation = dw3_pc_title_overlay_generation();
    const int cursor = dw3_pc_title_overlay_options_cursor();
    const Dw3OptionsRect whole = {0, 0, DW3_OPTIONS_CANVAS_W, DW3_OPTIONS_CANVAS_H};
    Dw3OptionsDirty dirty;
    int values[DW3_TITLE_OPTION_ROWS];
    int rebuilt = 0;

    memset(&dirty, 0, sizeof(dirty));
    for (int i = 0; i < DW3_TITLE_OPTION_ROWS; ++i)
        values[i] = dw3_pc_title_overlay_options_value(i);
    if (!g_static) g_static = (uint8_t *)malloc(canvas_bytes);
    if (!g_canvas) g_canvas = (uint8_t *)malloc(canvas_bytes);
    if (!g_static || !g_canvas) {
        dw3_options_cache_release();
        if (out) memset(out, 0, sizeof(*out));
        return -1;
    }
    if (g_awaiting_commit) g_full_pending = 1;
    if (!g_valid || g_art_generation != art_generation) {
        /* Open or language/label change: re-rasterize the static layer. */
        dw3_pc_title_overlay_options_component(g_static, DW3_OPTIONS_CANVAS_PITCH,
            DW3_TITLE_OPTIONS_STATIC, -1, 0, 0, 0, DW3_OPTIONS_CANVAS_W,
            DW3_OPTIONS_CANVAS_H, 1);
        memcpy(g_canvas, g_static, canvas_bytes);
        g_art_generation = art_generation;
        g_valid = 1;
        g_cursor = -1;
        for (int i = 0; i < DW3_TITLE_OPTION_ROWS; ++i) g_values[i] = -1;
        rebuilt = 1;
        g_full_pending = 1;
        dirty.rects[dirty.count++] = whole;
    } else {
        if (cursor != g_cursor) {
            if (g_cursor >= 0 && g_cursor < DW3_TITLE_OPTION_ROWS)
                dirty.rects[dirty.count++] = row_rect(g_cursor);
            if (cursor >= 0 && cursor < DW3_TITLE_OPTION_ROWS)
                dirty.rects[dirty.count++] = row_rect(cursor);
            dirty.rects[dirty.count++] = description_rect();
        }
        for (int i = 0; i < DW3_TITLE_OPTION_ROWS; ++i) {
            int covered = 0;
            if (values[i] == g_values[i]) continue;
            /* A row rect already being redrawn contains its value cell. */
            for (int j = 0; j < dirty.count; ++j)
                if (intersect(dirty.rects[j], value_rect(i), NULL)) covered = 1;
            if (!covered) dirty.rects[dirty.count++] = value_rect(i);
        }
    }
    for (int i = 0; i < dirty.count; ++i) {
        /* A rebuilt canvas already holds the static layer. */
        if (!rebuilt) restore_static(dirty.rects[i]);
        draw_state_in(dirty.rects[i], cursor, values);
    }
    g_cursor = cursor;
    memcpy(g_values, values, sizeof(values));

    if (g_full_pending) {
        dirty.count = 0;
        dirty.rects[dirty.count++] = whole;
        dirty.full = 1;
    }
    dirty.rebuilt = rebuilt;
    for (int i = 0; i < dirty.count; ++i)
        dirty.bytes += (size_t)dirty.rects[i].w * dirty.rects[i].h * 4;
    g_awaiting_commit = dirty.count > 0;
    if (out) *out = dirty;
    return 0;
}

void dw3_options_cache_commit(void) {
    g_awaiting_commit = 0;
    g_full_pending = 0;
}

void dw3_options_cache_invalidate(void) {
    g_full_pending = 1;
}

void dw3_options_cache_release(void) {
    free(g_static);
    free(g_canvas);
    g_static = NULL;
    g_canvas = NULL;
    g_valid = 0;
    g_cursor = -1;
    g_full_pending = 1;
    g_awaiting_commit = 0;
}

const uint8_t *dw3_options_cache_pixels(void) {
    return g_valid ? g_canvas : NULL;
}

void dw3_options_cache_pack_rect(Dw3OptionsRect rect, uint8_t *dst) {
    for (int y = 0; y < rect.h; ++y)
        memcpy(dst + (size_t)y * rect.w * 4,
               g_canvas + (size_t)(rect.y + y) * DW3_OPTIONS_CANVAS_PITCH + rect.x * 4,
               (size_t)rect.w * 4);
}
