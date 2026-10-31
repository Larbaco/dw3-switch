/* Host test of the P9.7 locked rows of the Port options screen
 * (platforms/pc/src/title_overlay_art.c, title_options_cache.c): with
 * Widescreen locked (and the Cheats group locked) only the label, the value and
 * the message line of that row change on the Display page, the Sound page does
 * not change at all; the retained canvas of the cache is
 * byte-identical to the reference render both when the lock appears and when
 * the cursor and the values move afterwards; the lock note is lettered in all
 * five languages. argv[1], when given, receives each state as a PPM (evidence,
 * never committed). */
#include "title_overlay_art.h"
#include "title_options_cache.h"
#include "port_options_groups.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond);        \
            g_fail = 1;                                                    \
        }                                                                  \
    } while (0)

enum { W = 960, H = 720, PITCH = W * 4 };

static void render(uint8_t *buf) {
    memset(buf, 0, (size_t)PITCH * H);
    dw3_pc_title_overlay_art(buf, PITCH, DW3_TITLE_ART_OPTIONS);
}

/* Number of differing pixels inside a rectangle. */
static long diff_in(const uint8_t *a, const uint8_t *b, int x0, int y0, int x1, int y1) {
    long n = 0;
    int x, y;

    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            if (memcmp(a + (size_t)y * PITCH + x * 4, b + (size_t)y * PITCH + x * 4, 4)) {
                n++;
            }
        }
    }
    return n;
}

static void write_ppm(const char *dir, const char *name, const uint8_t *rgba) {
    char path[512];
    FILE *fp;
    int i;

    snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    fp = fopen(path, "wb");
    if (!fp) {
        return;
    }
    fprintf(fp, "P6\n%d %d\n255\n", W, H);
    for (i = 0; i < W * H; i++) {
        const uint8_t *p = rgba + (size_t)i * 4;
        int a = p[3];
        uint8_t px[3] = {(uint8_t)((p[0] * a + 60 * (255 - a)) / 255),
                         (uint8_t)((p[1] * a + 60 * (255 - a)) / 255),
                         (uint8_t)((p[2] * a + 60 * (255 - a)) / 255)};
        fwrite(px, 1, 3, fp);
    }
    fclose(fp);
}

/* The cache's canvas must equal the reference render of the same state. */
static void check_cache(uint8_t *ref, const char *what) {
    Dw3OptionsDirty dirty;
    const uint8_t *pix;

    CHECK(dw3_options_cache_update(&dirty) == 0);
    dw3_options_cache_commit();
    pix = dw3_options_cache_pixels();
    CHECK(pix != NULL);
    render(ref);
    if (pix && memcmp(pix, ref, (size_t)PITCH * H) != 0) {
        fprintf(stderr, "cache differs from the reference: %s\n", what);
        g_fail = 1;
    }
}

int main(int argc, char **argv) {
    static const char *const lang_name[5] = {"en", "fr", "de", "es", "it"};
    const char *dir = argc > 1 ? argv[1] : NULL;
    uint8_t *open_ref = (uint8_t *)malloc((size_t)PITCH * H);
    uint8_t *locked = (uint8_t *)malloc((size_t)PITCH * H);
    uint8_t *other = (uint8_t *)malloc((size_t)PITCH * H);
    int values[DW3_TITLE_OPTION_ROWS] = {1, 2, 0, 10, 10, 0, 0, 0, 0};
    unsigned layout;
    int lang;

    if (!open_ref || !locked || !other) {
        return 2;
    }
    for (lang = 0; lang < 5; lang++) {
        long label, value, message, other_row, window;

        dw3_pc_title_overlay_set_options_language(lang);
        dw3_pc_title_overlay_set_options_page(dw3_optlist_page(DW3_OPTGRP_DISPLAY, 0), 0);
        dw3_pc_title_overlay_set_options_locked(0);
        dw3_pc_title_overlay_set_options_view(0, values, DW3_TITLE_OPTION_ROWS);
        render(open_ref);
        layout = dw3_pc_title_overlay_options_layout();
        dw3_pc_title_overlay_set_options_locked(1u);
        CHECK(dw3_pc_title_overlay_options_layout() == layout + 1);
        dw3_pc_title_overlay_set_options_locked(1u); /* same mask: nothing bumps */
        CHECK(dw3_pc_title_overlay_options_layout() == layout + 1);
        render(locked);
        /* The Widescreen plaque's label and value and the message line
         * changed ... */
        label = diff_in(open_ref, locked, 160, 124, 500, 174);
        value = diff_in(open_ref, locked, 500, 124, 760, 174);
        message = diff_in(open_ref, locked, 120, 532, 840, 592);
        CHECK(label > 100);
        CHECK(value > 20);
        CHECK(message > 500);
        /* ... and nothing else did: not the header plaque (the backdrop is
         * dimmed deeper in game, so only plaques are compared). */
        other_row = diff_in(open_ref, locked, 260, 48, 700, 82);
        CHECK(other_row == 0);
        window = 0;
        CHECK(window == 0);
        if (dir) {
            char name[64];

            snprintf(name, sizeof(name), "options-locked-%s", lang_name[lang]);
            write_ppm(dir, name, locked);
        }
        /* The Sound page depends on the lock only by the deeper dim in game: its
         * plaques, rows and message strip are the same pixels. */
        dw3_pc_title_overlay_set_options_page(dw3_optlist_page(DW3_OPTGRP_SOUND, 0), 0);
        dw3_pc_title_overlay_set_options_view(0, values, DW3_TITLE_OPTION_ROWS);
        dw3_pc_title_overlay_set_options_locked(1u);
        render(other);
        dw3_pc_title_overlay_set_options_locked(0);
        render(open_ref);
        CHECK(diff_in(open_ref, other, 260, 48, 700, 82) == 0);    /* header plaque */
        CHECK(diff_in(open_ref, other, 200, 138, 760, 170) == 0 &&
              diff_in(open_ref, other, 200, 204, 760, 236) == 0); /* the two rows */
        CHECK(diff_in(open_ref, other, 150, 540, 800, 585) == 0); /* message strip */
        CHECK(diff_in(open_ref, other, 0, 0, 100, 100) > 0);      /* the backdrop is deeper */
        /* D1: the group list (scrolled to show items 2..7) greys the Advanced plaque
         * (both its rows are locked in game) and the Cheats plaque, and nothing
         * else: Display has other rows, so Widescreen alone does not grey it. */
        dw3_pc_title_overlay_set_options_page(dw3_optlist_page(-1, 2), 1);
        render(open_ref);
        dw3_pc_title_overlay_set_options_locked((1u << 24) | (1u << 25) | DW3_TITLE_LOCK_CHEATS);
        render(locked);
        CHECK(diff_in(open_ref, locked, 260, 140 + 66, 700, 170 + 66) == 0); /* Menus */
        CHECK(diff_in(open_ref, locked, 260, 140 + 66 * 3, 700, 170 + 66 * 3) == 0); /* Saves */
        CHECK(diff_in(open_ref, locked, 230, 124 + 66 * 4, 730, 174 + 66 * 4) > 100); /* Cheats */
        CHECK(diff_in(open_ref, locked, 230, 124 + 66 * 5, 730, 174 + 66 * 5) > 100); /* Advanced */
        dw3_pc_title_overlay_set_options_locked(1u);
        render(locked);
        CHECK(diff_in(open_ref, locked, 260, 140, 700, 170) == 0);       /* Widescreen alone: Gameplay */
        dw3_pc_title_overlay_set_options_locked(0);
    }

    /* Retained canvas: byte-identical to the reference after the lock appears
     * and while the cursor and the values move, locked and not. */
    dw3_pc_title_overlay_set_options_language(DW3_TITLE_LANG_EN);
    dw3_pc_title_overlay_set_options_locked(0);
    dw3_pc_title_overlay_set_options_page(dw3_optlist_page(DW3_OPTGRP_DISPLAY, 0), 0);
    dw3_pc_title_overlay_set_options_view(0, values, DW3_TITLE_OPTION_ROWS);
    check_cache(other, "open, unlocked");
    dw3_pc_title_overlay_set_options_locked(1u | DW3_TITLE_LOCK_CHEATS);
    check_cache(other, "lock appears (full rebuild)");
    values[0] = 0;
    dw3_pc_title_overlay_set_options_view(0, values, DW3_TITLE_OPTION_ROWS);
    check_cache(other, "locked, Widescreen value drawn");
    dw3_pc_title_overlay_set_options_page(dw3_optlist_page(DW3_OPTGRP_SOUND, 0), 0);
    dw3_pc_title_overlay_set_options_view(1, values, DW3_TITLE_OPTION_ROWS);
    check_cache(other, "locked, Sound page, cursor on SFX volume");
    values[1] = 7;
    dw3_pc_title_overlay_set_options_view(1, values, DW3_TITLE_OPTION_ROWS);
    check_cache(other, "locked, SFX volume 7");
    dw3_pc_title_overlay_set_options_page(DW3_TITLE_OPTIONS_PAGE_OPTIONS, 1);
    dw3_pc_title_overlay_set_options_view(4, values, DW3_TITLE_OPTION_ROWS);
    check_cache(other, "locked, group list, cursor on Cheats");
    dw3_pc_title_overlay_set_options_locked(0);
    check_cache(other, "lock gone (full rebuild)");
    dw3_options_cache_release();

    /* Out-of-range lock bits are ignored. */
    dw3_pc_title_overlay_set_options_locked(0xFFFFFFFFu);
    CHECK(dw3_pc_title_overlay_options_locked() ==
          (((1u << 28) - 1u) | DW3_TITLE_LOCK_CHEATS));
    dw3_pc_title_overlay_set_options_locked(0);

    free(open_ref);
    free(locked);
    free(other);
    if (g_fail) {
        return 1;
    }
    printf("OK: locked rows art\n");
    return 0;
}
