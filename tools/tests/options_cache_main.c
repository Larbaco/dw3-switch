/* Host proof for the retained Port options canvas (title_options_cache.c):
 * every dirty-region update must leave both the canvas and a texture mirror
 * (fed only by the reported rects) byte-identical to a full reference render
 * of dw3_pc_title_overlay_art(DW3_TITLE_ART_OPTIONS). */
#include "../../platforms/pc/src/title_options_cache.h"
#include "../../platforms/pc/src/title_overlay_art.h"
#include "../../port/include/port_options_groups.h"

#include <pthread.h>
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

static void *worker_main(void *arg) {
    (void)arg;
    dw3_options_cache_prewarm_worker();
    return NULL;
}

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

/* Highest stored value of registry row `reg` (Fast-forward uses `turbo`). */
static int reg_max(int reg) {
    return reg == 0 || reg == 2 || reg == 8 ? 1 : reg == 1 ? 3
           : reg == 3 || reg == 4 ? 10 : reg == 5 ? 4 : 2;
}
static const int turbo[4] = {0, 2, 3, 4};
static int reg_value(int reg, int n) { return reg == 1 ? turbo[n] : n; }

/* Page of a list screen (group < 0: the group list) with the Cheats switch. */
static void list_page(int group, int scroll, int cheats_on) {
    dw3_pc_title_overlay_set_options_page(dw3_optlist_page(group, scroll), cheats_on);
}

int main(void) {
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

        /* The group list (the Options page): every cursor, with and without
         * the Cheats group; the values are ignored there. */
        for (int cheats = 0; cheats < 2; ++cheats) {
            list_page(-1, 0, cheats);
            for (int c = 0; c < 4 + cheats; ++c) {
                step(lang, c, v, 1, &d);
                steps++;
                if (d.full && c) fail("unexpected full upload (groups)", lang, c, v);
            }
        }
        list_page(-1, 0, 0);
        step(lang, 0, v, 1, &d);
        /* A scrolled list (a longer list than today's: the page code carries
         * the scroll) and its arrows, from every cursor. */
        for (int sc = 1; sc < 3; ++sc) {
            list_page(-1, sc, 1);
            for (int c = sc; c < 5; ++c) {
                step(lang, c, v, 1, &d);
                steps++;
            }
        }
        list_page(-1, 0, 0);

        /* Every group's sub-screen: every cursor row, every value of every
         * row (two languages; the others get the random jumps below). */
        for (int g = 0; g < 4; ++g) {
            const Dw3OptGroup *grp = &dw3_optgroups[g];
            list_page(g, 0, 0);
            for (int k = 0; k < DW3_TITLE_OPTION_ROWS; ++k) v[k] = 0;
            step(lang, 0, v, 1, &d);
            if (!d.full || !(d.rebuilt || d.cached)) fail("group page did not rebuild", lang, g, v);
            for (int c = 0; c < grp->nrows && (lang == DW3_TITLE_LANG_EN || lang == DW3_TITLE_LANG_DE); ++c)
                for (int k = 0; k < grp->nrows; ++k)
                    for (int n = 0; n <= reg_max(grp->rows[k]); ++n) {
                        v[k] = reg_value(grp->rows[k], n);
                        step(lang, c, v, 1, &d);
                        steps++;
                        if (d.full) fail("unexpected full upload", lang, c, v);
                    }
        }

        /* Cursor-only and value-only costs on the Gameplay page. */
        list_page(DW3_OPTGRP_GAMEPLAY, 0, 0);
        v[0] = 3; v[1] = 1; v[2] = 1; v[3] = 2; v[4] = 1;
        step(lang, 0, v, 1, NULL);
        step(lang, 1, v, 1, &d);
        if (d.bytes > g_max_cursor_bytes) g_max_cursor_bytes = d.bytes;
        if (d.count != 3) fail("cursor move is not old row + new row + description", lang, 1, v);
        v[0] = 4;
        step(lang, 1, v, 1, &d);
        if (d.count != 1) fail("value change is not one cell", lang, 1, v);
        if (d.bytes > g_max_value_bytes) g_max_value_bytes = d.bytes;

        /* The rows locked in game (Widescreen, the Cheats group): the static
         * layer follows, and Sound / Gameplay do not change with it. */
        {
            static const unsigned game_locked = 1u | (1u << 16);
            dw3_pc_title_overlay_set_options_locked(game_locked);
            list_page(-1, 0, 1);
            for (int c = 0; c < 5; ++c) step(lang, c, v, 1, &d);
            list_page(DW3_OPTGRP_DISPLAY, 0, 0);
            v[0] = 1;
            step(lang, 0, v, 1, &d);
            v[0] = 0;
            step(lang, 0, v, 1, &d);
            list_page(DW3_OPTGRP_SOUND, 0, 0);
            step(lang, 1, v, 1, &d);
            if ((dw3_pc_title_overlay_options_lock_key(dw3_optlist_page(DW3_OPTGRP_SOUND, 0), game_locked) &
                 0x7fffffffu) != 0)
                fail("Sound page depends on the locked rows", lang, 1, v);
            dw3_pc_title_overlay_set_options_locked(0);
        }
        list_page(DW3_OPTGRP_GAMEPLAY, 0, 0);

        /* Random multi-row jumps within the group pages. */
        for (int i = 0; i < 60; ++i) {
            const int g = (int)rnd(4);
            const Dw3OptGroup *grp = &dw3_optgroups[g];
            const int c = (int)rnd((unsigned)grp->nrows);
            list_page(g, 0, 0);
            for (int k = 0; k < grp->nrows; ++k)
                v[k] = reg_value(grp->rows[k], (int)rnd((unsigned)reg_max(grp->rows[k]) + 1));
            step(lang, c, v, 1, &d);
            steps++;
        }
        list_page(-1, 0, 0);
        for (int k = 0; k < DW3_TITLE_OPTION_ROWS; ++k) v[k] = 0;
        step(lang, 1, v, 1, &d);

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
        /* P6.20: the group list with the Cheats group, then the Cheats
         * page with the master switch off and on: a page or master change
         * rebuilds the static layer; cursor moves and values are dirty rects
         * like on the Options page. */
        dw3_pc_title_overlay_set_options_page(DW3_TITLE_OPTIONS_PAGE_OPTIONS, 1);
        step(lang, 0, v, 1, &d);
        if (!d.full || !(d.rebuilt || d.cached)) fail("Cheats group did not rebuild", lang, 0, v);
        for (int master = 0; master < 2; ++master) {
            int cv[DW3_TITLE_OPTION_ROWS] = {master, 0, 0, 0, 0, 0, 0, 0, 0};
            dw3_pc_title_overlay_set_options_page(DW3_TITLE_OPTIONS_PAGE_CHEATS, master);
            step(lang, 0, cv, 1, &d);
            if (!d.full || !(d.rebuilt || d.cached)) fail("cheats page did not rebuild", lang, 0, cv);
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
            if (!d.full || !(d.rebuilt || d.cached)) fail("cheats page 2 did not rebuild", lang, 0, cv);
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
            double t0;
            dw3_pc_title_overlay_set_options_locked(0);
            dw3_options_cache_idle();
            if (dw3_options_cache_pixels() != NULL) fail("pixels survive idle", lang, 0, v);
            if (!dw3_options_cache_prewarm_begin(game_mask)) fail("prewarm not needed after a language change", lang, 0, v);
            if (dw3_options_cache_prewarm_begin(game_mask)) fail("second worker started", lang, 0, v);
            /* The worker thread composes while this thread keeps updating
             * the live view: neither may disturb the other's pixels. */
            {
                pthread_t th;
                pthread_create(&th, NULL, worker_main, NULL);
                t0 = 0;
                for (int i = 0; i < 40; ++i) {
                    v[3] = i % 11;
                    step(lang, i % 4, v, 1, &d);
                    t0 += 1;
                }
                pthread_join(th, NULL);
            }
            if (dw3_options_cache_prewarm_begin(game_mask)) fail("layers not complete after the worker", lang, 0, v);
            dw3_options_cache_idle();
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
