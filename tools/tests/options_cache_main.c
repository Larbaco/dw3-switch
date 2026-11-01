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

enum { DW3_CHEAT_ROWS_T = 10 };
/* Page of a list screen (group < 0: the group list) with the Cheats switch. */
static void list_page(int group, int scroll, int cheats_on) {
    dw3_pc_title_overlay_set_options_page(dw3_optlist_page(group, scroll), cheats_on);
}

#ifdef WITH_MENU_BG_TILE
/* D4 / A18: with a tile the title backdrop is transparent (the hosts draw the
 * scrolling quad under the canvas), the dimmed 3x texture is handed out, and the
 * in-game layout keeps its near-opaque dim. */
static void tile_checks(void) {
    Dw3OptionsDirty d;
    int v[DW3_TITLE_OPTION_ROWS] = {0};
    const uint8_t *tex;

    dw3_pc_title_overlay_menu_tile_prepare();
    tex = dw3_pc_title_overlay_menu_bg_texture();
    if (!tex) { printf("FAIL: no menu background texture with a tile\n"); g_failures++; return; }
    /* texel (x, y) of the 288 texture = tile texel (x / 3, y / 3), dimmed by 90/255 */
    for (int y = 0; y < DW3_MENU_BG_TEX; y += 7)
        for (int x = 0; x < DW3_MENU_BG_TEX; x += 5) {
            const uint8_t *t = tex + ((size_t)y * DW3_MENU_BG_TEX + (size_t)x) * 4;
            const int tx = x / DW3_MENU_BG_SCALE, ty = y / DW3_MENU_BG_SCALE;
            const int want_r = (tx * 2) * (255 - 90) / 255, want_g = (ty * 2) * (255 - 90) / 255;
            if (t[0] != want_r || t[1] != want_g || t[3] != 255) {
                printf("FAIL: texture texel %d,%d is %d,%d,%d,%d\n", x, y, t[0], t[1], t[2], t[3]);
                g_failures++;
                return;
            }
        }
    /* the tile repeats every period: the texture's own wrap is the host's job, but
     * the data must be the same one period apart */
    list_page(-1, 0, 0);
    dw3_options_cache_release();
    dw3_pc_title_overlay_set_options_view(0, v, DW3_TITLE_OPTION_ROWS);
    if (dw3_options_cache_update(&d) != 0) { printf("FAIL: update\n"); g_failures++; return; }
    {
        const uint8_t *px = dw3_options_cache_pixels();
        int clear = 0, total = 0;
        for (int y = 0; y < 720; y += 11)
            for (int x = 0; x < 960; x += 13) {
                total++;
                if (px[(size_t)y * PITCH + (size_t)x * 4 + 3] == 0) clear++;
            }
        /* the corner is backdrop only; the plaques are a minority of the sampled points */
        if (px[3] != 0 || clear * 2 < total) {
            printf("FAIL: the title canvas backdrop is not transparent (corner alpha %d, %d of %d clear)\n",
                   px[3], clear, total);
            g_failures++;
        }
        /* a clear pixel carries the dimmed average colour, not black, so a linear
         * sample across a plaque edge does not darken */
        if (px[0] == 0 && px[1] == 0 && px[2] == 0) {
            printf("FAIL: the clear backdrop is black (fringe)\n");
            g_failures++;
        }
    }
    printf("tile checks done\n");
}
#endif

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
            if (!d.full || !(d.rebuilt || d.cached || d.based)) fail("group page did not rebuild", lang, g, v);
            /* P9.9 slice 3: the first group may have to compose the shared
             * layer; every other group is made over it (never a raster). */
            if (g > 0 && !(d.based || d.cached)) fail("group open was a full raster", lang, g, v);
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
        /* P6.20 / P9.9: the group list with the Cheats group, then the Cheats
         * sub-screen (ten rows, six on screen, scrolled) with the master
         * switch off and on. Opening it rebuilds the static layer once; a
         * scroll step, the master switch, cursor moves and values are dirty
         * rects (the labels and arrows are drawn per update). */
        dw3_pc_title_overlay_set_options_page(DW3_TITLE_OPTIONS_PAGE_OPTIONS, 1);
        step(lang, 0, v, 1, &d);
        if (!d.full || !(d.rebuilt || d.cached || d.based)) fail("Cheats group did not rebuild", lang, 0, v);
        {
            static const int top[DW3_CHEAT_ROWS_T] = {1, 1, 1, 1, 7, 3, 13, 359, 99, 1};
            static const int low[DW3_CHEAT_ROWS_T] = {0, 0, 0, 0, 0, 0, 0, 43, 1, 0};
            for (int master = 0; master < 2; ++master) {
                int cv[DW3_TITLE_OPTION_ROWS] = {master, 0, 0, 0, 0, 0, 0, 43, 10, 0};
                dw3_pc_title_overlay_set_options_page(DW3_TITLE_OPTIONS_PAGE_OPTIONS, 1);
                step(lang, 0, v, 1, &d);
                list_page(DW3_OPTGRP_CHEATS, 0, master);
                step(lang, 0, cv, 1, &d);
                if (!d.full || !(d.rebuilt || d.cached || d.based)) fail("cheats page did not rebuild", lang, 0, cv);
                if (d.rebuilt) fail("opening the Cheats group must be made over the shared layer", lang, 0, cv);
                for (int sc = 0; sc <= 4; ++sc) {
                    list_page(DW3_OPTGRP_CHEATS, sc, master);
                    step(lang, sc, cv, 1, &d);
                    steps++;
                    if (d.rebuilt || d.cached || d.based || d.full || d.count > DW3_OPTIONS_MAX_DIRTY)
                        fail("a scroll step must be a dirty-row update", lang, sc, cv);
                    /* Every cursor on screen for two languages, one for the rest. */
                    for (int c = sc; c <= sc + 5; ++c) {
                        if (!(lang == DW3_TITLE_LANG_EN || lang == DW3_TITLE_LANG_DE) && c != sc) continue;
                        for (int row = 0; row < DW3_CHEAT_ROWS_T; ++row) {
                            if (row == 0 && sc != 0) continue;
                            for (int n = low[row]; n <= top[row];
                                 n += row == 7 ? 97 : row == 8 ? 29 : 1) {
                                cv[row] = n;
                                step(lang, c, cv, 1, &d);
                                steps++;
                                if (d.full) fail("unexpected full upload (cheats)", lang, c, cv);
                            }
                            cv[row] = low[row];
                        }
                    }
                }
                /* The master switch itself, on the first and the scrolled view. */
                for (int sc = 0; sc <= 4; sc += 4) {
                    int mv[DW3_TITLE_OPTION_ROWS] = {master, 0, 0, 0, 0, 0, 0, 43, 10, 0};
                    list_page(DW3_OPTGRP_CHEATS, sc, master);
                    step(lang, sc, mv, 1, &d);
                    list_page(DW3_OPTGRP_CHEATS, sc, !master);
                    mv[0] = !master;
                    step(lang, sc, mv, 1, &d);
                    steps++;
                    if (d.rebuilt || d.cached || d.based || d.full || d.count < 1)
                        fail("the master switch must be a dirty-row update", lang, sc, mv);
                }
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
            /* After the prewarm every group opens over the shared layer of its
             * layout, at the title and in game: no raster on the open. */
            for (int ingame = 0; ingame < 2; ++ingame) {
                for (int g = 0; g < 4; ++g) {
                    dw3_options_cache_idle();
                    dw3_pc_title_overlay_set_options_locked(ingame ? game_mask : 0);
                    list_page(g, 0, 0);
                    step(lang, 0, v, 1, &d);
                    if (!(d.based || d.cached) || d.rebuilt)
                        fail("group open after the prewarm was a raster", lang, g, v);
                }
            }
            list_page(-1, 0, 0);
            dw3_pc_title_overlay_set_options_locked(0);
            step(lang, 0, v, 1, &d);
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
#ifdef WITH_MENU_BG_TILE
    tile_checks();
#endif
    if (g_failures) {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    puts("OK: options cache matches the reference render");
    return 0;
}
