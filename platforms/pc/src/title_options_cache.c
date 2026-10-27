#include "title_options_cache.h"
#include "title_overlay_art.h"

#include <stdlib.h>
#include <string.h>

/* Layout of the Port options screen in canvas pixels. Must cover everything
 * options_screen() draws for its FOCUS / VALUE / DESCRIPTION layers; the
 * unit test proves it against the full reference render. Nine rows of 34 px
 * on a 38 px pitch (title_overlay_art.c OPT_ROW_*): the window is 600 px
 * high, y 60..660, inside the 720 px canvas. The value cell starts at 560: the
 * widest word, "Command+Tech", is 214 px wide and ends at 780. */
enum {
    ROW_X = 132, ROW_Y = 150, ROW_W = 696, ROW_H = 34, ROW_STEP = 38,
    VALUE_X = 560, VALUE_W = 268,
    DESC_Y = 524, DESC_H = 52,
};

/* Static layers (everything but the focus box, values and description) kept
 * per layout, so opening the screen again, or opening it in game after the
 * title (a different locked-row layout), does not rasterize the window.
 * Two slots: the title layout and the in-game layout. A slot is drawn a band
 * of rows at a time by prewarm_tick() while the screen is closed. */
enum { SLOTS = 2, BAND_ROWS = 16 };
typedef struct StaticSlot {
    uint8_t *px;
    unsigned art_generation, locked;
    int page, cheats_on;
    int rows_done;      /* 0..CANVAS_H; CANVAS_H = complete */
    unsigned stamp;     /* last use, for eviction */
} StaticSlot;
static StaticSlot g_slots[SLOTS];
static unsigned g_stamp;
static const uint8_t *g_static;   /* the complete slot the canvas mirrors */
static uint8_t *g_canvas;
static int g_valid;
static unsigned g_art_generation;
static int g_cursor = -1;
static int g_values[DW3_TITLE_OPTION_ROWS];
/* The host must still receive the whole canvas. */
static int g_full_pending = 1;
/* update() reported rects that commit() has not acknowledged yet. */
static int g_awaiting_commit;
/* dw3_pc_title_overlay_options_layout() of the static layer (the Options or
 * the Cheats page, the cheats master switch, P9.7 the locked rows). */
static unsigned g_layout;

static Dw3OptionsRect row_rect(int row) {
    return (Dw3OptionsRect){ROW_X, ROW_Y + ROW_STEP * row, ROW_W, ROW_H};
}
static Dw3OptionsRect value_rect(int row) {
    return (Dw3OptionsRect){VALUE_X, ROW_Y + ROW_STEP * row, VALUE_W, ROW_H};
}
static Dw3OptionsRect description_rect(void) {
    return (Dw3OptionsRect){ROW_X, DESC_Y, ROW_W, DESC_H};
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

static int slot_matches(const StaticSlot *s, unsigned art, unsigned locked,
                        int page, int cheats_on) {
    return s->px && s->art_generation == art && s->locked == locked &&
           s->page == page && s->cheats_on == cheats_on;
}

/* The slot for a layout: the one already holding it (complete or part way),
 * else the least recently used slot that is not the one the canvas mirrors.
 * NULL when memory is short. */
static StaticSlot *slot_for(unsigned art, unsigned locked, int page, int cheats_on) {
    StaticSlot *victim = NULL;
    const size_t bytes = (size_t)DW3_OPTIONS_CANVAS_PITCH * DW3_OPTIONS_CANVAS_H;

    for (int i = 0; i < SLOTS; ++i)
        if (slot_matches(&g_slots[i], art, locked, page, cheats_on))
            return &g_slots[i];
    for (int i = 0; i < SLOTS; ++i) {
        if (g_slots[i].px && g_slots[i].px == g_static) continue;
        if (!victim || !g_slots[i].px ||
            (victim->px && g_slots[i].stamp < victim->stamp))
            victim = &g_slots[i];
    }
    if (!victim) return NULL;
    if (!victim->px) victim->px = (uint8_t *)malloc(bytes);
    if (!victim->px) return NULL;
    victim->art_generation = art;
    victim->locked = locked;
    victim->page = page;
    victim->cheats_on = cheats_on;
    victim->rows_done = 0;
    return victim;
}

/* Draw up to `rows` more rows of the slot's static layer. */
static void slot_advance(StaticSlot *s, int rows) {
    while (rows > 0 && s->rows_done < DW3_OPTIONS_CANVAS_H) {
        int n = DW3_OPTIONS_CANVAS_H - s->rows_done;
        if (n > rows) n = rows;
        dw3_pc_title_overlay_options_static_band(s->px, DW3_OPTIONS_CANVAS_PITCH,
            s->locked, s->page, s->cheats_on, s->rows_done, n);
        s->rows_done += n;
        rows -= n;
    }
    s->stamp = ++g_stamp;
}

int dw3_options_cache_update(Dw3OptionsDirty *out) {
    const size_t canvas_bytes = (size_t)DW3_OPTIONS_CANVAS_PITCH * DW3_OPTIONS_CANVAS_H;
    const unsigned art_generation = dw3_pc_title_overlay_generation();
    const int cursor = dw3_pc_title_overlay_options_cursor();
    const Dw3OptionsRect whole = {0, 0, DW3_OPTIONS_CANVAS_W, DW3_OPTIONS_CANVAS_H};
    Dw3OptionsDirty dirty;
    int values[DW3_TITLE_OPTION_ROWS];
    int rebuilt = 0;
    int from_cache = 0;

    memset(&dirty, 0, sizeof(dirty));
    for (int i = 0; i < DW3_TITLE_OPTION_ROWS; ++i)
        values[i] = dw3_pc_title_overlay_options_value(i);
    if (!g_canvas) g_canvas = (uint8_t *)malloc(canvas_bytes);
    if (!g_canvas) {
        dw3_options_cache_release();
        if (out) memset(out, 0, sizeof(*out));
        return -1;
    }
    if (g_awaiting_commit) g_full_pending = 1;
    if (!g_valid || g_art_generation != art_generation ||
        g_layout != dw3_pc_title_overlay_options_layout()) {
        StaticSlot *slot;
        g_layout = dw3_pc_title_overlay_options_layout();
        /* Open or language/label change: take the static layer of this
         * layout from the cache, drawing what is missing. */
        slot = slot_for(art_generation, dw3_pc_title_overlay_options_locked(),
                        dw3_pc_title_overlay_options_page(),
                        dw3_pc_title_overlay_options_cheats_on());
        if (!slot) {
            dw3_options_cache_release();
            if (out) memset(out, 0, sizeof(*out));
            return -1;
        }
        from_cache = slot->rows_done >= DW3_OPTIONS_CANVAS_H;
        slot_advance(slot, DW3_OPTIONS_CANVAS_H);
        g_static = slot->px;
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
    dirty.rebuilt = rebuilt && !from_cache;
    dirty.cached = rebuilt && from_cache;
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

/* Drop the working canvas only. The static layers stay, so the next open
 * (or an open in game) is a copy and a few redrawn rows, not a raster. */
void dw3_options_cache_idle(void) {
    free(g_canvas);
    g_canvas = NULL;
    g_static = NULL;
    g_valid = 0;
    g_cursor = -1;
    g_full_pending = 1;
    g_awaiting_commit = 0;
}

void dw3_options_cache_release(void) {
    dw3_options_cache_idle();
    for (int i = 0; i < SLOTS; ++i) {
        free(g_slots[i].px);
        memset(&g_slots[i], 0, sizeof(g_slots[i]));
    }
    g_static = NULL;
}

int dw3_options_cache_prewarm(unsigned locked, int page, int cheats_on) {
    StaticSlot *slot = slot_for(dw3_pc_title_overlay_generation(), locked, page,
                                cheats_on);

    if (!slot) return 1;   /* out of memory: the open draws it, as before */
    if (slot->rows_done >= DW3_OPTIONS_CANVAS_H) return 1;
    slot_advance(slot, BAND_ROWS);
    return slot->rows_done >= DW3_OPTIONS_CANVAS_H;
}

int dw3_options_cache_prewarm_tick(unsigned in_game_locked) {
    const int page0 = 0;   /* DW3_TITLE_OPTIONS_PAGE_OPTIONS */
    const int cheats = dw3_pc_title_overlay_options_cheats_on();

    if (!dw3_options_cache_prewarm(0, page0, cheats)) return 0;
    return dw3_options_cache_prewarm(in_game_locked, page0, cheats);
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
