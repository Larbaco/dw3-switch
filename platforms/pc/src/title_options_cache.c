#include "title_options_cache.h"
#include "title_overlay_art.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
 * They are composed ahead of time by a low-priority worker thread (the host
 * starts it: dw3_options_cache_prewarm_begin / _worker), never on the frame
 * that needs them unless the worker has not finished (then the open composes
 * synchronously, as before). Three slots: title, in game, and the one being
 * composed. The slot table is guarded by a spin lock held for a few
 * instructions; the pixels are drawn outside it. */
enum { SLOTS = 3, BAND_ROWS = 48 };
enum { S_EMPTY, S_COMPLETE, S_WORKER, S_MAIN };
typedef struct StaticSlot {
    uint8_t *px;
    unsigned art_generation, locked;
    int page, cheats_on;
    int state;
    unsigned stamp;     /* last use, for eviction */
} StaticSlot;
static StaticSlot g_slots[SLOTS];
static unsigned g_stamp;
static atomic_flag g_lock = ATOMIC_FLAG_INIT;
static atomic_int g_worker_running;
static unsigned g_worker_mask;
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

static void tbl_lock(void) {
    while (atomic_flag_test_and_set_explicit(&g_lock, memory_order_acquire)) {
    }
}
static void tbl_unlock(void) {
    atomic_flag_clear_explicit(&g_lock, memory_order_release);
}

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

/* Caller holds the lock. */
static StaticSlot *find_complete(unsigned art, unsigned locked, int page,
                                 int cheats_on) {
    for (int i = 0; i < SLOTS; ++i) {
        StaticSlot *s = &g_slots[i];
        if (s->state == S_COMPLETE && s->art_generation == art &&
            s->locked == locked && s->page == page && s->cheats_on == cheats_on)
            return s;
    }
    return NULL;
}

/* Caller holds the lock: an empty slot, else the least recently used complete
 * one that the canvas does not mirror; claimed for `state`. NULL when none
 * (or memory is short). */
static StaticSlot *claim_slot(int state, unsigned art, unsigned locked,
                              int page, int cheats_on) {
    StaticSlot *victim = NULL;
    const size_t bytes = (size_t)DW3_OPTIONS_CANVAS_PITCH * DW3_OPTIONS_CANVAS_H;

    for (int i = 0; i < SLOTS; ++i) {
        StaticSlot *s = &g_slots[i];
        if (s->state == S_WORKER || s->state == S_MAIN) continue;
        if (s->px && s->px == g_static) continue;
        if (!victim || s->state == S_EMPTY ||
            (victim->state != S_EMPTY && s->stamp < victim->stamp))
            victim = s;
        if (s->state == S_EMPTY) break;
    }
    if (!victim) return NULL;
    if (!victim->px) victim->px = (uint8_t *)malloc(bytes);
    if (!victim->px) return NULL;
    victim->state = state;
    victim->art_generation = art;
    victim->locked = locked;
    victim->page = page;
    victim->cheats_on = cheats_on;
    return victim;
}

/* Draw a claimed slot's whole static layer, band by band. Returns 0 when the
 * art changed under the worker (the slot is then stale: caller drops it). */
static int slot_compose(StaticSlot *s, int check_generation) {
    for (int y = 0; y < DW3_OPTIONS_CANVAS_H; y += BAND_ROWS) {
        int n = DW3_OPTIONS_CANVAS_H - y;
        if (n > BAND_ROWS) n = BAND_ROWS;
        if (check_generation &&
            dw3_pc_title_overlay_generation() != s->art_generation)
            return 0;
        dw3_pc_title_overlay_options_static_band(s->px, DW3_OPTIONS_CANVAS_PITCH,
            s->locked, s->page, s->cheats_on, y, n);
    }
    return 1;
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
        const unsigned locked = dw3_pc_title_overlay_options_locked();
        const int page = dw3_pc_title_overlay_options_page();
        const int cheats_on = dw3_pc_title_overlay_options_cheats_on();
        g_layout = dw3_pc_title_overlay_options_layout();
        /* Open or language/label change: take the static layer of this
         * layout from the cache; if the worker has not composed it yet,
         * compose it here, as before the worker existed. */
        tbl_lock();
        slot = find_complete(art_generation, locked, page, cheats_on);
        if (slot) {
            slot->stamp = ++g_stamp;
            g_static = slot->px;
            from_cache = 1;
        } else {
            slot = claim_slot(S_MAIN, art_generation, locked, page, cheats_on);
        }
        tbl_unlock();
        if (!slot) {
            dw3_options_cache_release();
            if (out) memset(out, 0, sizeof(*out));
            return -1;
        }
        if (!from_cache) {
            slot_compose(slot, 0);
            tbl_lock();
            slot->state = S_COMPLETE;
            slot->stamp = ++g_stamp;
            g_static = slot->px;
            tbl_unlock();
        }
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
    tbl_lock();
    g_static = NULL;
    tbl_unlock();
    g_valid = 0;
    g_cursor = -1;
    g_full_pending = 1;
    g_awaiting_commit = 0;
}

void dw3_options_cache_release(void) {
    /* Teardown: the worker must not be running (hosts call this from the
     * frame thread when nothing is shown or at exit; a worker that is still
     * composing keeps its slot, which stays allocated). */
    dw3_options_cache_idle();
    tbl_lock();
    for (int i = 0; i < SLOTS; ++i) {
        if (g_slots[i].state == S_WORKER) continue;
        free(g_slots[i].px);
        memset(&g_slots[i], 0, sizeof(g_slots[i]));
    }
    g_static = NULL;
    tbl_unlock();
}

int dw3_options_cache_layers_ready(unsigned in_game_locked) {
    const unsigned art = dw3_pc_title_overlay_generation();
    const int cheats = dw3_pc_title_overlay_options_cheats_on();
    int ready;

    tbl_lock();
    ready = find_complete(art, 0, 0, cheats) &&
            find_complete(art, in_game_locked, 0, cheats);
    tbl_unlock();
    return ready;
}

int dw3_options_cache_prewarm_begin(unsigned in_game_locked) {
    const unsigned art = dw3_pc_title_overlay_generation();
    const int cheats = dw3_pc_title_overlay_options_cheats_on();
    int need;

    if (atomic_load(&g_worker_running)) return 0;
    tbl_lock();
    need = !find_complete(art, 0, 0, cheats) ||
           !find_complete(art, in_game_locked, 0, cheats);
    tbl_unlock();
    if (!need) return 0;
    g_worker_mask = in_game_locked;
    atomic_store(&g_worker_running, 1);
    return 1;
}

void dw3_options_cache_prewarm_cancel(void) {
    atomic_store(&g_worker_running, 0);
}

void dw3_options_cache_prewarm_worker(void) {
    for (int k = 0; k < 2; ++k) {
        const unsigned art = dw3_pc_title_overlay_generation();
        const unsigned locked = k == 0 ? 0u : g_worker_mask;
        const int cheats = dw3_pc_title_overlay_options_cheats_on();
        StaticSlot *slot;
        double t0;

        tbl_lock();
        slot = find_complete(art, locked, 0, cheats);
        if (!slot) slot = claim_slot(S_WORKER, art, locked, 0, cheats);
        else slot = NULL;
        tbl_unlock();
        if (!slot) continue;
        t0 = now_ms();
        if (slot_compose(slot, 1)) {
            tbl_lock();
            slot->state = S_COMPLETE;
            slot->stamp = ++g_stamp;
            tbl_unlock();
            fprintf(stderr, "dw3-port-options: prewarm %s ready in %.0f ms "
                    "(worker)\n", k == 0 ? "title" : "in-game", now_ms() - t0);
            fflush(stderr);
        } else {
            tbl_lock();
            slot->state = S_EMPTY;
            tbl_unlock();
        }
    }
    atomic_store(&g_worker_running, 0);
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
