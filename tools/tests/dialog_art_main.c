/* Host test of the P6.27 / P6.28 artwork (platforms/pc/src/title_overlay_art.c):
 * the Yes / No window in the five languages, both questions and both cursor
 * positions, and the title menu's fourth plaque. P6.38 adds the restart
 * question of the Region at start row (title / in game, US / EU) and the
 * one-button notice.
 *
 * argv[1], when given, is a directory that receives every rendered window and
 * title state as a PPM (the evidence of the PR; never committed). */
#include "title_overlay_art.h"

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

static void write_ppm(const char *dir, const char *name, const uint8_t *rgba,
                      int w, int h, int pitch) {
    char path[512];
    FILE *fp;
    int x;
    int y;

    snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    fp = fopen(path, "wb");
    if (!fp) {
        return;
    }
    fprintf(fp, "P6\n%d %d\n255\n", w, h);
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            const uint8_t *p = rgba + (size_t)y * pitch + x * 4;
            /* Over a mid-grey backdrop, so the translucent shadow shows. */
            int a = p[3];
            uint8_t px[3];
            px[0] = (uint8_t)((p[0] * a + 90 * (255 - a)) / 255);
            px[1] = (uint8_t)((p[1] * a + 90 * (255 - a)) / 255);
            px[2] = (uint8_t)((p[2] * a + 90 * (255 - a)) / 255);
            fwrite(px, 1, 3, fp);
        }
    }
    fclose(fp);
}

/* Tops of the fully opaque plaque bodies down column x (the shadow is
 * translucent): returns how many, and the tops in tops[]. */
static int plaque_tops(const uint8_t *rgba, int pitch, int x, int *tops, int max) {
    int n = 0;
    int y;
    int prev = 0;

    for (y = 300; y < 720; y++) {
        const int solid = rgba[(size_t)y * pitch + x * 4 + 3] == 255;

        if (solid && !prev && n < max) {
            tops[n++] = y;
        }
        prev = solid;
    }
    return n;
}

/* Pixels of the warning line's colour (255, 224, 140) in rows y0..y1 of the window. */
static long warn_pixels(const uint8_t *rgba, int pitch, int y0, int y1) {
    long n = 0;
    int x;
    int y;

    for (y = y0; y < y1; y++) {
        for (x = 0; x < DW3_DIALOG_W; x++) {
            const uint8_t *p = rgba + (size_t)y * pitch + x * 4;

            if (p[3] == 255 && p[0] > 200 && p[1] > 150 && p[1] < 235 && p[2] > 80 && p[2] < 170) {
                n++;
            }
        }
    }
    return n;
}

static long opaque_pixels(const uint8_t *rgba, int w, int h, int pitch, int x0,
                          int y0, int x1, int y1) {
    long n = 0;
    int x;
    int y;

    for (y = y0; y < y1 && y < h; y++) {
        for (x = x0; x < x1 && x < w; x++) {
            if (rgba[(size_t)y * pitch + x * 4 + 3]) {
                n++;
            }
        }
    }
    return n;
}

int main(int argc, char **argv) {
    static const char *const lang_name[5] = {"en", "fr", "de", "es", "it"};
    const char *dir = argc > 1 ? argv[1] : NULL;
    const int pitch = DW3_DIALOG_W * 4;
    uint8_t *a = (uint8_t *)malloc((size_t)pitch * DW3_DIALOG_H);
    uint8_t *b = (uint8_t *)malloc((size_t)pitch * DW3_DIALOG_H);
    uint8_t *title = (uint8_t *)malloc((size_t)960 * 720 * 4);
    int lang;
    int kind;
    int x;
    int y;

    if (!a || !b || !title) {
        return 2;
    }
    /* Geometry: the window sits inside the 960x720 canvas. */
    CHECK(DW3_DIALOG_X >= 0 && DW3_DIALOG_X + DW3_DIALOG_W <= 960);
    CHECK(DW3_DIALOG_Y >= 0 && DW3_DIALOG_Y + DW3_DIALOG_H <= 720);

    for (lang = DW3_TITLE_LANG_EN; lang <= DW3_TITLE_LANG_IT; lang++) {
        dw3_pc_title_overlay_set_options_language(lang);
        for (kind = DW3_DIALOG_QUIT; kind <= DW3_DIALOG_RETURN; kind++) {
            char name[64];
            long solid;
            int differ = 0;

            memset(a, 0xAA, (size_t)pitch * DW3_DIALOG_H);
            dw3_pc_dialog_art(a, pitch, kind, 0);
            memset(b, 0x55, (size_t)pitch * DW3_DIALOG_H);
            dw3_pc_dialog_art(b, pitch, kind, 1);
            /* The whole texture is written (cleared first). */
            CHECK(a[(DW3_DIALOG_W - 1) * 4 + 3] == 0 && a[(size_t)pitch * (DW3_DIALOG_H - 1) + 3] == 0);
            /* The bezel is opaque white where it starts. */
            CHECK(a[(size_t)pitch * 4 + 4 * 4 + 3] == 255);
            solid = opaque_pixels(a, DW3_DIALOG_W, DW3_DIALOG_H, pitch, 0, 0,
                                  DW3_DIALOG_W, DW3_DIALOG_H);
            CHECK(solid > (long)DW3_DIALOG_W * DW3_DIALOG_H * 9 / 10);
            /* The cursor moves: the No / Yes focus boxes change pixels,
             * the title and the message line do not. */
            for (y = 0; y < DW3_DIALOG_H; y++) {
                for (x = 0; x < DW3_DIALOG_W; x++) {
                    if (memcmp(a + (size_t)y * pitch + x * 4,
                               b + (size_t)y * pitch + x * 4, 4) != 0) {
                        differ++;
                        CHECK(y >= 150 && y < 270);
                    }
                }
            }
            CHECK(differ > 400);
            if (dir) {
                snprintf(name, sizeof(name), "dialog-%s-%s-no", lang_name[lang],
                         kind == DW3_DIALOG_QUIT ? "quit" : "return");
                write_ppm(dir, name, a, DW3_DIALOG_W, DW3_DIALOG_H, pitch);
                snprintf(name, sizeof(name), "dialog-%s-%s-yes", lang_name[lang],
                         kind == DW3_DIALOG_QUIT ? "quit" : "return");
                write_ppm(dir, name, b, DW3_DIALOG_W, DW3_DIALOG_H, pitch);
            }
        }
        /* P6.38: the restart question. Title kinds have no warning line, in-game kinds
         * have it; US and EU differ in the question line only; the cursor moves
         * between the two rows; the notice has one row and ignores the focus. */
        {
            static const char *const rname[4] = {"title-us", "title-eu", "game-us", "game-eu"};
            uint8_t *c = (uint8_t *)malloc((size_t)pitch * DW3_DIALOG_H);
            int r;

            if (!c) {
                return 2;
            }
            for (r = 0; r < 4; r++) {
                char name[64];
                int differ = 0;
                long solid;

                memset(a, 0xAA, (size_t)pitch * DW3_DIALOG_H);
                dw3_pc_dialog_art(a, pitch, DW3_DIALOG_RESTART_US + r, 0);
                memset(b, 0x55, (size_t)pitch * DW3_DIALOG_H);
                dw3_pc_dialog_art(b, pitch, DW3_DIALOG_RESTART_US + r, 1);
                solid = opaque_pixels(a, DW3_DIALOG_W, DW3_DIALOG_H, pitch, 0, 0,
                                      DW3_DIALOG_W, DW3_DIALOG_H);
                CHECK(solid > (long)DW3_DIALOG_W * DW3_DIALOG_H * 9 / 10);
                if (r >= 2) {
                    CHECK(warn_pixels(a, pitch, 126, 160) > 400);
                } else {
                    CHECK(warn_pixels(a, pitch, 126, 160) == 0);
                }
                for (y = 0; y < DW3_DIALOG_H; y++) {
                    for (x = 0; x < DW3_DIALOG_W; x++) {
                        if (memcmp(a + (size_t)y * pitch + x * 4, b + (size_t)y * pitch + x * 4, 4) != 0) {
                            differ++;
                            CHECK(y >= 150 && y < 270);
                        }
                    }
                }
                CHECK(differ > 400);
                if (r < 2) { /* US and EU: same window, only the question line differs */
                    memset(c, 0x33, (size_t)pitch * DW3_DIALOG_H);
                    dw3_pc_dialog_art(c, pitch, DW3_DIALOG_RESTART_US + (1 - r), 0);
                    differ = 0;
                    for (y = 0; y < DW3_DIALOG_H; y++) {
                        for (x = 0; x < DW3_DIALOG_W; x++) {
                            if (memcmp(a + (size_t)y * pitch + x * 4, c + (size_t)y * pitch + x * 4, 4) != 0) {
                                differ++;
                                CHECK(y >= 90 && y < 130);
                            }
                        }
                    }
                    CHECK(differ > 20);
                }
                if (dir) {
                    snprintf(name, sizeof(name), "dialog-%s-restart-%s-no", lang_name[lang], rname[r]);
                    write_ppm(dir, name, a, DW3_DIALOG_W, DW3_DIALOG_H, pitch);
                    snprintf(name, sizeof(name), "dialog-%s-restart-%s-yes", lang_name[lang], rname[r]);
                    write_ppm(dir, name, b, DW3_DIALOG_W, DW3_DIALOG_H, pitch);
                }
            }
            memset(a, 0xAA, (size_t)pitch * DW3_DIALOG_H);
            dw3_pc_dialog_art(a, pitch, DW3_DIALOG_NOTICE, 0);
            memset(b, 0x55, (size_t)pitch * DW3_DIALOG_H);
            dw3_pc_dialog_art(b, pitch, DW3_DIALOG_NOTICE, 1);
            CHECK(memcmp(a, b, (size_t)pitch * DW3_DIALOG_H) == 0); /* one lit button, any focus */
            CHECK(warn_pixels(a, pitch, 126, 160) == 0);
            CHECK(opaque_pixels(a, DW3_DIALOG_W, DW3_DIALOG_H, pitch, 0, 0, DW3_DIALOG_W,
                                DW3_DIALOG_H) > (long)DW3_DIALOG_W * DW3_DIALOG_H * 9 / 10);
            if (dir) {
                char name[64];

                snprintf(name, sizeof(name), "dialog-%s-restart-notice", lang_name[lang]);
                write_ppm(dir, name, a, DW3_DIALOG_W, DW3_DIALOG_H, pitch);
            }
            free(c);
        }
        /* The title's four plaques (no auto-save file): the Quit game one (y 540..588) is drawn in the focus
         * state and not in the others; the footer (y 610..669) is intact. */
        {
            int sel;
            long quit_pixels[5];

            for (sel = 0; sel < DW3_TITLE_MENU_ROWS - 1; sel++) {
                memset(title, 0xEE, (size_t)960 * 720 * 4);
                dw3_pc_title_overlay_art(title, 960 * 4, sel);
                quit_pixels[sel] = opaque_pixels(title, 960, 720, 960 * 4, 230,
                                                 540, 730, 588);
                CHECK(opaque_pixels(title, 960, 720, 960 * 4, 230, 384, 730, 588) > 50000);
                CHECK(opaque_pixels(title, 960, 720, 960 * 4, 252, 610, 708, 669) > 1000);
                {
                    /* One pitch for every row, Quit game included. */
                    int tops[8];
                    const int n = plaque_tops(title, 960 * 4, 300, tops, 8);

                    CHECK(n == 4);
                    CHECK(n == 4 && tops[0] == 384 && tops[1] == 436 &&
                          tops[2] == 488 && tops[3] == 540);
                }
                if (dir) {
                    char name[64];
                    snprintf(name, sizeof(name), "title-%s-%d", lang_name[lang], sel);
                    write_ppm(dir, name, title, 960, 720, 960 * 4);
                }
            }
            CHECK(quit_pixels[3] > 0 && quit_pixels[0] > 0);
            /* P6.31: with the Load row there are five plaques; Quit game is
             * row 4 at y 560..600 and Continue is lettered (row 2). */
            dw3_pc_title_overlay_set_load_row(1);
            for (sel = 0; sel < DW3_TITLE_MENU_ROWS; sel++) {
                memset(title, 0xEE, (size_t)960 * 720 * 4);
                dw3_pc_title_overlay_art(title, 960 * 4, sel);
                quit_pixels[sel] = opaque_pixels(title, 960, 720, 960 * 4, 230,
                                                 560, 730, 600);
                CHECK(opaque_pixels(title, 960, 720, 960 * 4, 230, 384, 730, 600) > 50000);
                CHECK(opaque_pixels(title, 960, 720, 960 * 4, 252, 611, 708, 669) > 1000);
                {
                    int tops[8];
                    const int n = plaque_tops(title, 960 * 4, 300, tops, 8);

                    CHECK(n == 5);
                    CHECK(n == 5 && tops[0] == 384 && tops[1] == 428 &&
                          tops[2] == 472 && tops[3] == 516 && tops[4] == 560);
                }
                if (dir) {
                    char name[64];
                    snprintf(name, sizeof(name), "title5-%s-%d", lang_name[lang], sel);
                    write_ppm(dir, name, title, 960, 720, 960 * 4);
                }
            }
            CHECK(quit_pixels[4] > 0 && quit_pixels[0] > 0);
            dw3_pc_title_overlay_set_load_row(0);
        }
    }
    /* The options screen is still its own state (4), unchanged by the menu rows. */
    CHECK(DW3_TITLE_ART_OPTIONS == DW3_TITLE_MENU_ROWS);
    memset(title, 0xEE, (size_t)960 * 720 * 4);
    dw3_pc_title_overlay_art(title, 960 * 4, DW3_TITLE_ART_OPTIONS);
    CHECK(opaque_pixels(title, 960, 720, 960 * 4, 0, 0, 960, 720) > 960 * 720 / 2);

    free(a);
    free(b);
    free(title);
    if (g_fail) {
        return 1;
    }
    printf("OK: dialog and title artwork\n");
    return 0;
}
