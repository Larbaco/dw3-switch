/* Host test of the P9.7 locked rows of the Port options screen
 * (platforms/pc/src/title_overlay_art.c, title_options_cache.c): with
 * Widescreen locked (and the Cheats hint locked) only the label, the value and
 * the message line of that row change; the retained canvas of the cache is
 * byte-identical to the reference render both when the lock appears and when
 * the cursor and the values move afterwards; the lock note is lettered in all
 * five languages. argv[1], when given, receives each state as a PPM (evidence,
 * never committed). */
#include "title_overlay_art.h"
#include "title_options_cache.h"

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
    int row;

    if (!open_ref || !locked || !other) {
        return 2;
    }
    for (lang = 0; lang < 5; lang++) {
        long label, value, message, other_row, window;

        dw3_pc_title_overlay_set_options_language(lang);
        dw3_pc_title_overlay_set_options_locked(0);
        dw3_pc_title_overlay_set_options_view(0, values, DW3_TITLE_OPTION_ROWS);
        render(open_ref);
        layout = dw3_pc_title_overlay_options_layout();
        dw3_pc_title_overlay_set_options_locked(1u);
        CHECK(dw3_pc_title_overlay_options_layout() == layout + 1);
        dw3_pc_title_overlay_set_options_locked(1u); /* same mask: nothing bumps */
        CHECK(dw3_pc_title_overlay_options_layout() == layout + 1);
        render(locked);
        /* Row 0 label, value and the message line changed ... */
        label = diff_in(open_ref, locked, 160, 150, 540, 184);
        value = diff_in(open_ref, locked, 560, 150, 828, 184);
        message = diff_in(open_ref, locked, 132, 524, 828, 576);
        CHECK(label > 100);
        CHECK(value > 20);
        CHECK(message > 500);
        /* ... and nothing else did: not the other rows, not the window. */
        other_row = 0;
        for (row = 1; row < DW3_TITLE_OPTION_ROWS; row++) {
            other_row += diff_in(open_ref, locked, 132, 150 + 38 * row, 828, 184 + 38 * row);
        }
        CHECK(other_row == 0);
        window = diff_in(open_ref, locked, 0, 0, W, 150) +
                 diff_in(open_ref, locked, 0, 490, W, 524) +
                 diff_in(open_ref, locked, 0, 576, W, H);
        CHECK(window == 0);
        if (dir) {
            char name[64];

            snprintf(name, sizeof(name), "options-locked-%s", lang_name[lang]);
            write_ppm(dir, name, locked);
        }
        /* The lock note is not the row's own message line. */
        dw3_pc_title_overlay_set_options_view(3, values, DW3_TITLE_OPTION_ROWS);
        render(other);
        CHECK(diff_in(locked, other, 132, 524, 828, 576) > 500);
        dw3_pc_title_overlay_set_options_locked(0);
    }

    /* Retained canvas: byte-identical to the reference after the lock appears
     * and while the cursor and the values move, locked and not. */
    dw3_pc_title_overlay_set_options_language(DW3_TITLE_LANG_EN);
    dw3_pc_title_overlay_set_options_locked(0);
    dw3_pc_title_overlay_set_options_view(0, values, DW3_TITLE_OPTION_ROWS);
    check_cache(other, "open, unlocked");
    dw3_pc_title_overlay_set_options_locked(1u | DW3_TITLE_LOCK_CHEATS);
    check_cache(other, "lock appears (full rebuild)");
    dw3_pc_title_overlay_set_options_view(3, values, DW3_TITLE_OPTION_ROWS);
    check_cache(other, "locked, cursor on Music volume");
    values[3] = 7;
    dw3_pc_title_overlay_set_options_view(3, values, DW3_TITLE_OPTION_ROWS);
    check_cache(other, "locked, Music volume 7");
    dw3_pc_title_overlay_set_options_view(0, values, DW3_TITLE_OPTION_ROWS);
    check_cache(other, "locked, cursor back on Widescreen");
    values[0] = 0;
    dw3_pc_title_overlay_set_options_view(0, values, DW3_TITLE_OPTION_ROWS);
    check_cache(other, "locked, Widescreen value drawn");
    dw3_pc_title_overlay_set_options_locked(0);
    check_cache(other, "lock gone (full rebuild)");
    dw3_options_cache_release();

    /* Out-of-range lock bits are ignored. */
    dw3_pc_title_overlay_set_options_locked(0xFFFFFFFFu);
    CHECK(dw3_pc_title_overlay_options_locked() ==
          (((1u << DW3_TITLE_OPTION_ROWS) - 1u) | DW3_TITLE_LOCK_CHEATS));
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
