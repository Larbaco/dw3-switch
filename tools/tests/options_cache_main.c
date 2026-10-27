/* Host proof for the retained Port options canvas (title_options_cache.c):
 * every dirty-region update must leave both the canvas and a texture mirror
 * (fed only by the reported rects) byte-identical to a full reference render
 * of dw3_pc_title_overlay_art(DW3_TITLE_ART_OPTIONS). */
#include "../../platforms/pc/src/title_options_cache.h"
#include "../../platforms/pc/src/title_overlay_art.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PITCH = DW3_OPTIONS_CANVAS_PITCH, H = DW3_OPTIONS_CANVAS_H };
#define CANVAS_BYTES ((size_t)PITCH * H)

static uint8_t *g_ref;
static uint8_t *g_tex;
static uint8_t *g_pack;
static int g_failures;
static unsigned g_rng = 12345u;
static size_t g_max_cursor_bytes, g_max_value_bytes;

static unsigned rnd(unsigned n) {
    g_rng = g_rng * 1664525u + 1013904223u;
    return (g_rng >> 8) % n;
}

static void fail(const char *what, int lang, int c, const int *v) {
    printf("FAIL: %s (lang %d cursor %d values %d/%d/%d/%d/%d/%d/%d)\n", what,
           lang, c, v[0], v[1], v[2], v[3], v[4], v[5], v[6]);
    g_failures++;
}

static int first_diff(const uint8_t *a, const uint8_t *b, int *x, int *y) {
    for (size_t i = 0; i < CANVAS_BYTES; ++i)
        if (a[i] != b[i]) {
            *x = (int)((i % PITCH) / 4);
            *y = (int)(i / PITCH);
            return 1;
        }
    return 0;
}

/* Move the shared view to (c, v), update the cache, apply the reported rects
 * to the mirror and compare canvas and mirror with a fresh reference. */
static void step(int lang, int c, const int *v, int commit, Dw3OptionsDirty *out) {
    Dw3OptionsDirty d;
    int x = 0, y = 0;

    dw3_pc_title_overlay_set_options_view(c, v, DW3_TITLE_OPTION_ROWS);
    if (dw3_options_cache_update(&d) != 0) {
        fail("update failed", lang, c, v);
        return;
    }
    for (int i = 0; i < d.count; ++i) {
        const Dw3OptionsRect r = d.rects[i];
        dw3_options_cache_pack_rect(r, g_pack);
        for (int j = 0; j < r.h; ++j)
            memcpy(g_tex + (size_t)(r.y + j) * PITCH + r.x * 4,
                   g_pack + (size_t)j * r.w * 4, (size_t)r.w * 4);
    }
    if (commit) dw3_options_cache_commit();
    dw3_pc_title_overlay_art(g_ref, PITCH, DW3_TITLE_ART_OPTIONS);
    if (first_diff(dw3_options_cache_pixels(), g_ref, &x, &y)) {
        printf("  canvas differs at %d,%d\n", x, y);
        fail("canvas != reference", lang, c, v);
    }
    if (commit && first_diff(g_tex, g_ref, &x, &y)) {
        printf("  mirror differs at %d,%d\n", x, y);
        fail("reported rects miss a change", lang, c, v);
    }
    if (out) *out = d;
}

int main(void) {
    static const int turbo[4] = {0, 2, 3, 4};
    Dw3OptionsDirty d;
    int v[DW3_TITLE_OPTION_ROWS] = {0};
    int steps = 0;

    g_ref = (uint8_t *)malloc(CANVAS_BYTES);
    g_tex = (uint8_t *)calloc(1, CANVAS_BYTES);
    g_pack = (uint8_t *)malloc(CANVAS_BYTES);
    if (!g_ref || !g_tex || !g_pack) return 2;

    for (int lang = DW3_TITLE_LANG_EN; lang <= DW3_TITLE_LANG_IT; ++lang) {
        dw3_pc_title_overlay_set_options_language(lang);
        for (int row = 0; row < DW3_TITLE_OPTION_ROWS; ++row) v[row] = 0;
        step(lang, 0, v, 1, &d);
        if (!d.full || !d.rebuilt || d.bytes != CANVAS_BYTES) {
            fail("first update is not a full rebuild", lang, 0, v);
        }
        step(lang, 0, v, 1, &d);
        if (d.count != 0) fail("idle update reported rects", lang, 0, v);

        /* Every state of the three switches, reached by one change at a time
         * (two languages; the others get the random jumps below), with the
         * volumes at their default and at the extremes. */
        for (int c = 0; c < DW3_TITLE_OPTION_ROWS && (lang == DW3_TITLE_LANG_EN || lang == DW3_TITLE_LANG_DE); ++c)
            for (int w = 0; w < 2; ++w)
                for (int t = 0; t < 4; ++t)
                    for (int r = 0; r < 2; ++r) {
                        const int target[DW3_TITLE_OPTION_ROWS] = {w, turbo[t], r, 10, 10, 0, 0, 0, 0};
                        for (int row = 0; row < DW3_TITLE_OPTION_ROWS; ++row) {
                            v[row] = target[row];
                            step(lang, c, v, 1, &d);
                            steps++;
                            if (d.full) fail("unexpected full upload", lang, c, v);
                        }
                    }
        /* Every volume step on both volume rows, from every cursor row. */
        for (int c = 0; c < DW3_TITLE_OPTION_ROWS && (lang == DW3_TITLE_LANG_EN || lang == DW3_TITLE_LANG_DE); ++c)
            for (int row = 3; row < DW3_TITLE_OPTION_ROWS; ++row)
                for (int n = 0; n <= 10; ++n) {
                    v[row] = n;
                    step(lang, c, v, 1, &d);
                    steps++;
                    if (d.full) fail("unexpected full upload", lang, c, v);
                }

        /* Every mode of the mode rows (Encounter rate 0..4, stored values:
         * 1x 0.5x OFF 1.5x 2x; Text speed 0..2; Battle cursor 0..2: OFF
         * Command Command+Tech; Auto-save 0..1), from every cursor row. */
        for (int c = 0; c < DW3_TITLE_OPTION_ROWS && (lang == DW3_TITLE_LANG_EN || lang == DW3_TITLE_LANG_DE); ++c)
            for (int row = 5; row < DW3_TITLE_OPTION_ROWS; ++row)
                for (int n = 0; n <= (row == 5 ? 4 : row == 8 ? 1 : 2); ++n) {
                    v[row] = n;
                    step(lang, c, v, 1, &d);
                    steps++;
                    if (d.full) fail("unexpected full upload", lang, c, v);
                }

        /* Cursor-only and value-only costs. */
        v[0] = 1; v[1] = 3; v[2] = 0; v[3] = 6; v[4] = 10; v[5] = 1; v[6] = 2; v[7] = 1; v[8] = 1;
        step(lang, 0, v, 1, NULL);
        step(lang, 1, v, 1, &d);
        if (d.bytes > g_max_cursor_bytes) g_max_cursor_bytes = d.bytes;
        if (d.count != 3) fail("cursor move is not old row + new row + description", lang, 1, v);
        v[1] = 4;
        step(lang, 1, v, 1, &d);
        if (d.count != 1 || d.bytes > g_max_value_bytes) {
            if (d.count != 1) fail("value change is not one cell", lang, 1, v);
        }
        if (d.bytes > g_max_value_bytes) g_max_value_bytes = d.bytes;

        /* Random multi-row jumps. */
        for (int i = 0; i < 60; ++i) {
            const int c = (int)rnd(DW3_TITLE_OPTION_ROWS);
            v[0] = (int)rnd(2);
            v[1] = turbo[rnd(4)];
            v[2] = (int)rnd(2);
            v[3] = (int)rnd(11);
            v[4] = (int)rnd(11);
            v[5] = (int)rnd(5);
            v[6] = (int)rnd(3);
            v[7] = (int)rnd(3);
            v[8] = (int)rnd(2);
            step(lang, c, v, 1, &d);
            steps++;
        }

        /* An update the host never acknowledged is repeated in full. */
        v[0] ^= 1;
        step(lang, 2, v, 0, &d);
        step(lang, 2, v, 1, &d);
        if (!d.full) fail("uncommitted update was not repeated in full", lang, 2, v);
        step(lang, 2, v, 1, &d);
        if (d.count != 0) fail("idle after commit reported rects", lang, 2, v);

        /* Lost texture (resize / context loss): same canvas, full resend. */
        dw3_options_cache_invalidate();
        step(lang, 2, v, 1, &d);
        if (!d.full || d.rebuilt) fail("invalidate must resend without re-raster", lang, 2, v);

#ifdef DW3_CHEATS
        /* P6.20: the Options page with the Cheats hint, then the Cheats
         * page with the master switch off and on: a page or master change
         * rebuilds the static layer; cursor moves and values are dirty rects
         * like on the Options page. */
        dw3_pc_title_overlay_set_options_page(DW3_TITLE_OPTIONS_PAGE_OPTIONS, 1);
        step(lang, 0, v, 1, &d);
        if (!d.full || !d.rebuilt) fail("hint did not rebuild", lang, 0, v);
        for (int master = 0; master < 2; ++master) {
            int cv[DW3_TITLE_OPTION_ROWS] = {master, 0, 0, 0, 0, 0, 0, 0, 0};
            dw3_pc_title_overlay_set_options_page(DW3_TITLE_OPTIONS_PAGE_CHEATS, master);
            step(lang, 0, cv, 1, &d);
            if (!d.full || !d.rebuilt) fail("cheats page did not rebuild", lang, 0, cv);
            /* Every cursor row for two languages, the first for the rest. */
            for (int c = 0; c < (lang == DW3_TITLE_LANG_EN || lang == DW3_TITLE_LANG_DE
                                 ? DW3_TITLE_CHEAT_ROWS : 1); ++c)
                for (int row = 1; row < DW3_TITLE_CHEAT_ROWS; ++row)
                    for (int n = 0; n <= (row == 4 ? 7 : 1); ++n) {
                        cv[row] = n;
                        step(lang, c, cv, 1, &d);
                        steps++;
                        if (d.full) fail("unexpected full upload (cheats)", lang, c, cv);
                    }
        }
        /* PR 2: the second page (Money, Warp, item id, count, Give item):
         * the same checks with its own value ranges. */
        for (int master = 0; master < 2; ++master) {
            static const int top[DW3_TITLE_CHEAT_ROWS] = {3, 13, 359, 99, 1};
            static const int low[DW3_TITLE_CHEAT_ROWS] = {0, 0, 43, 1, 0};
            int cv[DW3_TITLE_OPTION_ROWS] = {0, 0, 43, 10, 0, 0, 0, 0};
            dw3_pc_title_overlay_set_options_page(DW3_TITLE_OPTIONS_PAGE_CHEATS2, master);
            step(lang, 0, cv, 1, &d);
            if (!d.full || !d.rebuilt) fail("cheats page 2 did not rebuild", lang, 0, cv);
            for (int c = 0; c < (lang == DW3_TITLE_LANG_EN || lang == DW3_TITLE_LANG_FR
                                 ? DW3_TITLE_CHEAT_ROWS : 1); ++c)
                for (int row = 0; row < DW3_TITLE_CHEAT_ROWS; ++row)
                    for (int n = low[row]; n <= top[row]; n += row == 2 ? 29 : row == 3 ? 7 : 1) {
                        cv[row] = n;
                        step(lang, c, cv, 1, &d);
                        steps++;
                        if (d.full) fail("unexpected full upload (cheats 2)", lang, c, cv);
                    }
        }
        dw3_pc_title_overlay_set_options_page(DW3_TITLE_OPTIONS_PAGE_OPTIONS, 0);
        step(lang, 0, v, 1, &d);
        if (!d.rebuilt && !d.cached) fail("back to Options did not rebuild", lang, 0, v);
#endif

        /* The screen closes (idle: canvas freed, composed layers kept) and
         * opens again, with the title and the in-game layout: the layers
         * are prewarmed a band at a time, then an open is a copy that must
         * equal the reference render of that layout. */
        {
            static const unsigned game_mask = 1u | (1u << 16);
            int bands = 0;
            dw3_pc_title_overlay_set_options_locked(0);
            dw3_options_cache_idle();
            if (dw3_options_cache_pixels() != NULL) fail("pixels survive idle", lang, 0, v);
            while (!dw3_options_cache_prewarm_tick(game_mask) && bands < 200) bands++;
            if (bands < 20 || bands >= 200) fail("prewarm is not banded", lang, bands, v);
            step(lang, 0, v, 1, &d);
            if (!d.cached || d.rebuilt || !d.full) fail("open after prewarm was not a cached copy", lang, 0, v);
            dw3_options_cache_idle();
            dw3_pc_title_overlay_set_options_locked(game_mask);
            step(lang, 0, v, 1, &d);
            if (!d.cached || d.rebuilt) fail("in-game open after prewarm was not cached", lang, 0, v);
            dw3_options_cache_idle();
            dw3_pc_title_overlay_set_options_locked(0);
            step(lang, 0, v, 1, &d);
            if (!d.cached) fail("title reopen was not cached", lang, 0, v);
            /* A language change makes the layers stale. */
            dw3_options_cache_idle();
        }

        /* Restart / teardown: pixels disappear, the next open rebuilds. */
        dw3_options_cache_release();
        if (dw3_options_cache_pixels() != NULL) fail("pixels survive release", lang, 2, v);
        step(lang, 0, v, 1, &d);
        if (!d.full || !d.rebuilt) fail("post-release update is not a rebuild", lang, 0, v);
        dw3_options_cache_release();
        memset(g_tex, 0, CANVAS_BYTES);
    }
    printf("steps=%d cursor_move_bytes=%zu value_change_bytes=%zu\n", steps,
           g_max_cursor_bytes, g_max_value_bytes);
    if (g_max_cursor_bytes > CANVAS_BYTES / 4 || g_max_value_bytes > CANVAS_BYTES / 20)
        g_failures++;
    if (g_failures) {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    puts("OK: options cache matches the reference render");
    return 0;
}
