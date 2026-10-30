/* Host test of the pure part of port/options/switch_glyphs.c (P10.6): the art,
 * the role-to-palette maps, the 4 bpp packing of a cell, the RGBA decode of an
 * override PNG and the plan per button map. Built with -DDW3_GLYPHS_TEST and
 * linked with port/options/button_map.c (-DDW3_BUTTON_MAP_TEST). With the
 * argument `dump` it prints the compiled-in art, for the Python test to compare
 * with what tools/generate_switch_glyphs.py draws. */
#include "port_glyphs.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

#define CHECK(cond, ...)                                      \
    do {                                                      \
        if (!(cond)) {                                        \
            printf("FAIL line %d: ", __LINE__);               \
            printf(__VA_ARGS__);                              \
            printf("\n");                                     \
            g_fail = 1;                                       \
        }                                                     \
    } while (0)

static void test_art(void) {
    int g;
    int i;

    for (g = 0; g < DW3_GLYPH_COUNT; g++) {
        const unsigned char *a = dw3_glyph_art(g);
        int count[DW3_ROLE_COUNT] = {0, 0, 0, 0, 0};

        CHECK(a != NULL, "art %d", g);
        if (a == NULL) {
            continue;
        }
        for (i = 0; i < DW3_GLYPH_PIXELS; i++) {
            CHECK(a[i] < DW3_ROLE_COUNT, "glyph %d pixel %d role %d", g, i, a[i]);
            count[a[i] < DW3_ROLE_COUNT ? a[i] : 0]++;
        }
        CHECK(count[DW3_ROLE_OUTLINE] > 20 && count[DW3_ROLE_BODY] > 20 &&
                  count[DW3_ROLE_LETTER] > 5 && count[DW3_ROLE_SHADE] > 3,
              "glyph %s roles %d/%d/%d/%d", dw3_glyph_name(g),
              count[1], count[2], count[3], count[4]);
        /* The disc is 11 wide: the last column of the cell stays clear. */
        for (i = 0; i < DW3_GLYPH_H; i++) {
            CHECK(a[i * DW3_GLYPH_W + 11] == DW3_ROLE_CLEAR, "glyph %d column 11", g);
        }
    }
    CHECK(dw3_glyph_art(-1) == NULL && dw3_glyph_art(DW3_GLYPH_COUNT) == NULL,
          "art out of range");
    CHECK(strcmp(dw3_glyph_name(DW3_GLYPH_ZR), "ZR") == 0, "name ZR");
    CHECK(strcmp(dw3_glyph_name(99), "?") == 0, "name out of range");
}

static void test_from_cap(void) {
    CHECK(dw3_glyph_from_cap("A") == DW3_GLYPH_A, "A");
    CHECK(dw3_glyph_from_cap("B") == DW3_GLYPH_B, "B");
    CHECK(dw3_glyph_from_cap("X") == DW3_GLYPH_X, "X");
    CHECK(dw3_glyph_from_cap("Y") == DW3_GLYPH_Y, "Y");
    CHECK(dw3_glyph_from_cap("L") == DW3_GLYPH_L, "L");
    CHECK(dw3_glyph_from_cap("R") == DW3_GLYPH_R, "R");
    CHECK(dw3_glyph_from_cap("ZL") == DW3_GLYPH_ZL, "ZL");
    CHECK(dw3_glyph_from_cap("ZR") == DW3_GLYPH_ZR, "ZR");
    CHECK(dw3_glyph_from_cap("L3") == DW3_GLYPH_NONE, "stick click has no glyph");
    CHECK(dw3_glyph_from_cap("R3") == DW3_GLYPH_NONE, "stick click has no glyph");
    CHECK(dw3_glyph_from_cap("?") == DW3_GLYPH_NONE, "?");
    CHECK(dw3_glyph_from_cap("a") == DW3_GLYPH_NONE, "case matters");
    CHECK(dw3_glyph_from_cap(NULL) == DW3_GLYPH_NONE, "NULL");
}

static void test_pack(void) {
    unsigned char out[DW3_GLYPH_PACKED];
    unsigned char roles[DW3_GLYPH_PIXELS];
    static const unsigned char row0_us[6] = {0x00, 0x20, 0x22, 0x22, 0x00, 0x00};
    static const unsigned char row0_eu[6] = {0x00, 0x90, 0x99, 0x99, 0x00, 0x00};
    const unsigned char *us = dw3_glyph_index_map(0);
    const unsigned char *eu = dw3_glyph_index_map(1);
    int i;

    /* Glyph A row 0 is the roles 000111110000 (the top of the rim). */
    CHECK(dw3_glyph_pack(dw3_glyph_art(DW3_GLYPH_A), us, out) == 0, "pack us");
    CHECK(memcmp(out, row0_us, 6) == 0,
          "us row 0: %02X %02X %02X %02X %02X %02X", out[0], out[1], out[2],
          out[3], out[4], out[5]);
    CHECK(dw3_glyph_pack(dw3_glyph_art(DW3_GLYPH_A), eu, out) == 0, "pack eu");
    CHECK(memcmp(out, row0_eu, 6) == 0,
          "eu row 0: %02X %02X %02X %02X %02X %02X", out[0], out[1], out[2],
          out[3], out[4], out[5]);
    /* The low nibble is the left pixel: rim then shade -> 2 | 15 << 4. */
    memset(roles, 0, sizeof(roles));
    roles[0] = DW3_ROLE_OUTLINE;
    roles[1] = DW3_ROLE_SHADE;
    roles[DW3_GLYPH_PIXELS - 2] = DW3_ROLE_LETTER;
    roles[DW3_GLYPH_PIXELS - 1] = DW3_ROLE_BODY;
    CHECK(dw3_glyph_pack(roles, us, out) == 0, "pack us 2");
    CHECK(out[0] == 0xF2, "us rim 2, shade 15 -> 0xF2, got %02X", out[0]);
    CHECK(out[DW3_GLYPH_PACKED - 1] == (unsigned char)(us[3] | (us[2] << 4)),
          "last byte %02X", out[DW3_GLYPH_PACKED - 1]);
    /* Every packed nibble is a palette index the region's map names. */
    for (i = 0; i < DW3_GLYPH_COUNT; i++) {
        int k;
        int ok = 1;

        CHECK(dw3_glyph_pack(dw3_glyph_art(i), eu, out) == 0, "pack eu glyph %d", i);
        for (k = 0; k < DW3_GLYPH_PACKED; k++) {
            int a = out[k] & 15;
            int b = out[k] >> 4;
            int j;
            int fa = 0;
            int fb = 0;

            for (j = 0; j < DW3_ROLE_COUNT; j++) {
                fa |= eu[j] == a;
                fb |= eu[j] == b;
            }
            ok &= fa && fb;
        }
        CHECK(ok, "glyph %d packs only mapped indices", i);
    }
    /* The US icon takes the text colour: only indices 0, 2 and 15 of the original. */
    for (i = 0; i < DW3_ROLE_COUNT; i++) {
        CHECK(us[i] == 0 || us[i] == 2 || us[i] == 15, "us index %d", us[i]);
    }
    /* A bad role or a map entry beyond 15 is refused. */
    roles[5] = DW3_ROLE_COUNT;
    CHECK(dw3_glyph_pack(roles, us, out) == -1, "bad role refused");
    {
        unsigned char bad[DW3_ROLE_COUNT] = {0, 16, 2, 3, 4};

        roles[5] = 0;
        roles[7] = DW3_ROLE_OUTLINE;
        CHECK(dw3_glyph_pack(roles, bad, out) == -1, "bad index refused");
    }
}

static void test_rgba(void) {
    static const unsigned char rgb[DW3_ROLE_COUNT][3] = {
        {0, 0, 0}, {224, 224, 224}, {40, 40, 48}, {255, 255, 255}, {112, 112, 120}};
    unsigned char rgba[DW3_GLYPH_PIXELS * 4];
    unsigned char roles[DW3_GLYPH_PIXELS];
    char why[96];
    int i;

    /* An image of the compiled-in art as the PNG loader would give it. */
    for (i = 0; i < DW3_GLYPH_PIXELS; i++) {
        int r = dw3_glyph_art(DW3_GLYPH_Y)[i];

        rgba[i * 4 + 0] = rgb[r][0];
        rgba[i * 4 + 1] = rgb[r][1];
        rgba[i * 4 + 2] = rgb[r][2];
        rgba[i * 4 + 3] = r ? 255 : 0;
    }
    CHECK(dw3_glyph_roles_from_rgba(rgba, 12, 12, roles, why, sizeof(why)) == 0,
          "decode: %s", why);
    CHECK(memcmp(roles, dw3_glyph_art(DW3_GLYPH_Y), DW3_GLYPH_PIXELS) == 0,
          "decode round trip");
    CHECK(dw3_glyph_roles_from_rgba(rgba, 11, 12, roles, why, sizeof(why)) == -1 &&
              strstr(why, "11x12") != NULL, "wrong size: %s", why);
    rgba[10 * 4] = 1; /* one pixel of no role colour */
    rgba[10 * 4 + 3] = 255;
    CHECK(dw3_glyph_roles_from_rgba(rgba, 12, 12, roles, why, sizeof(why)) == -1 &&
              strstr(why, "not a role colour") != NULL, "bad colour: %s", why);
}

static const char *plan_of(const unsigned char *map, int eu, char *text, unsigned cap) {
    Dw3GlyphCell cells[DW3_GLYPH_CELLS];

    dw3_glyph_plan(map, eu, cells);
    dw3_glyph_plan_text(cells, text, cap);
    return text;
}

static void test_plan(void) {
    unsigned char ps[DW3_BTN_COUNT];
    unsigned char nin[DW3_BTN_COUNT];
    unsigned char custom[DW3_BTN_COUNT] = {8, 1, 2, 6, 4, 5, 3, 7, 0, 9};
    Dw3GlyphCell cells[DW3_GLYPH_CELLS];
    char text[96];

    dw3_button_map_preset(DW3_LAYOUT_PLAYSTATION, ps);
    dw3_button_map_preset(DW3_LAYOUT_NINTENDO, nin);

    /* PlayStation layout: Cross is the B position, Circle A, Square Y, Triangle X. */
    CHECK(dw3_glyph_plan(ps, 1, cells) == 4, "eu patches four cells");
    CHECK(cells[0].ps_button == DW3_BTN_CROSS && cells[0].n == 27 &&
              cells[0].glyph == DW3_GLYPH_B, "eu cross");
    CHECK(cells[1].ps_button == DW3_BTN_CIRCLE && cells[1].n == 47 &&
              cells[1].glyph == DW3_GLYPH_A, "eu circle");
    CHECK(cells[2].ps_button == DW3_BTN_SQUARE && cells[2].n == 50 &&
              cells[2].glyph == DW3_GLYPH_Y, "eu square");
    CHECK(cells[3].ps_button == DW3_BTN_TRIANGLE && cells[3].n == 52 &&
              cells[3].glyph == DW3_GLYPH_X, "eu triangle");
    CHECK(strcmp(plan_of(ps, 1, text, sizeof(text)),
                 "n=27:B n=47:A n=50:Y n=52:X") == 0, "eu ps text: %s", text);
    /* US: n = 27 is the multiplication sign, left alone (docs/P10.6_GLYPHS.md). */
    CHECK(dw3_glyph_plan(ps, 0, cells) == 3, "us patches three cells");
    CHECK(cells[0].glyph == DW3_GLYPH_NONE, "us cross keeps the original");
    CHECK(strcmp(plan_of(ps, 0, text, sizeof(text)),
                 "n=27:orig n=47:A n=50:Y n=52:X") == 0, "us ps text: %s", text);

    /* Nintendo: Cross is A, Circle X, Triangle B. */
    CHECK(strcmp(plan_of(nin, 1, text, sizeof(text)),
                 "n=27:A n=47:X n=50:Y n=52:B") == 0, "eu nintendo text: %s", text);
    CHECK(strcmp(plan_of(nin, 0, text, sizeof(text)),
                 "n=27:orig n=47:X n=50:Y n=52:B") == 0, "us nintendo text: %s", text);

    /* A custom map: Cross on the left stick click (no glyph), Square on ZL. */
    CHECK(dw3_button_map_valid(custom), "custom map is a permutation");
    CHECK(dw3_glyph_plan(custom, 1, cells) == 3, "custom eu: stick click keeps its icon");
    CHECK(strcmp(plan_of(custom, 1, text, sizeof(text)),
                 "n=27:orig n=47:A n=50:ZL n=52:X") == 0, "custom text: %s", text);
    /* A too small buffer ends the text cleanly. */
    plan_of(ps, 1, text, 10);
    CHECK(strlen(text) < 10, "short buffer");
}

/* The page-turn prompt: the exact strings L1 / R1, the glyph per map. */
static void test_pageturn(void) {
    unsigned char ps[DW3_BTN_COUNT];
    unsigned char nin[DW3_BTN_COUNT];
    unsigned char custom[DW3_BTN_COUNT];
    int plan[DW3_PAGETURN_COUNT];
    const char *s;

    /* L1 is 19 05, R1 is 1F 05, nothing else (not L2, not a longer string). */
    CHECK(dw3_glyph_pageturn_match("\x19\x05") == 0, "L1");
    CHECK(dw3_glyph_pageturn_match("\x1f\x05") == 1, "R1");
    CHECK(dw3_glyph_pageturn_match("\x19\x06") == -1, "L2 is not a page turn");
    CHECK(dw3_glyph_pageturn_match("\x1f\x05\x01") == -1, "R1 followed by text");
    CHECK(dw3_glyph_pageturn_match("\x05\x05") == -1, "other letters");
    CHECK(dw3_glyph_pageturn_match("\x19") == -1, "one letter");
    CHECK(dw3_glyph_pageturn_match("") == -1, "empty");
    CHECK(dw3_glyph_pageturn_match(NULL) == -1, "NULL");

    /* The icon string: 01 n, then the terminator. */
    s = dw3_glyph_pageturn_string(0);
    CHECK(s != NULL && s[0] == 1 && (unsigned char)s[1] == DW3_PAGETURN_N0 &&
              s[2] == 0 && s[3] == 0, "L1 string");
    s = dw3_glyph_pageturn_string(1);
    CHECK(s != NULL && s[0] == 1 && (unsigned char)s[1] == DW3_PAGETURN_N0 + 1 &&
              s[2] == 0, "R1 string");
    CHECK(dw3_glyph_pageturn_string(2) == NULL && dw3_glyph_pageturn_string(-1) == NULL,
          "string out of range");
    CHECK(DW3_PAGETURN_N0 == 115 && DW3_PAGETURN_U % 4 == 0 &&
              DW3_PAGETURN_V0 + 2 * DW3_PAGETURN_V_STEP <= 256,
          "cells are whole halfwords on the page");

    dw3_button_map_preset(DW3_LAYOUT_PLAYSTATION, ps);
    dw3_button_map_preset(DW3_LAYOUT_NINTENDO, nin);
    CHECK(dw3_glyph_pageturn_plan(ps, plan) == 2 && plan[0] == DW3_GLYPH_L &&
              plan[1] == DW3_GLYPH_R, "playstation: L1 -> L, R1 -> R");
    CHECK(dw3_glyph_pageturn_plan(nin, plan) == 2 && plan[0] == DW3_GLYPH_L &&
              plan[1] == DW3_GLYPH_R, "nintendo: shoulders stay L and R");

    /* Custom: L1 on ZL, R1 on the right stick click (no glyph: keeps the text). */
    memcpy(custom, ps, sizeof(custom));
    custom[DW3_BTN_L1] = DW3_BTN_L2;
    custom[DW3_BTN_R1] = DW3_BTN_R3;
    custom[DW3_BTN_L2] = DW3_BTN_L1;
    custom[DW3_BTN_R3] = DW3_BTN_R1;
    CHECK(dw3_button_map_valid(custom), "custom map is a permutation");
    CHECK(dw3_glyph_pageturn_plan(custom, plan) == 1 && plan[0] == DW3_GLYPH_ZL &&
              plan[1] == DW3_GLYPH_NONE, "custom: L1 -> ZL, R1 keeps the text");
}

/* argv: png <dir>: every <dir>/<name>.png equals the compiled-in art. */
static int check_png_dir(const char *dir) {
    int g;
    int bad = 0;

    for (g = 0; g < DW3_GLYPH_COUNT; g++) {
        char path[512];
        char name[4];
        char why[96];
        unsigned char px[DW3_GLYPH_PIXELS];
        int k;

        for (k = 0; dw3_glyph_name(g)[k] != '\0'; k++) {
            char c = dw3_glyph_name(g)[k];

            name[k] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
        }
        name[k] = '\0';
        snprintf(path, sizeof(path), "%s/%s.png", dir, name);
        why[0] = '\0';
        if (dw3_glyph_art_from_png(path, px, why, sizeof(why)) != 0) {
            printf("png %s: %s\n", name, why);
            bad++;
        } else if (memcmp(px, dw3_glyph_art(g), DW3_GLYPH_PIXELS) != 0) {
            printf("png %s: differs from the compiled-in art\n", name);
            bad++;
        }
    }
    if (bad == 0) {
        printf("OK: png files equal the art\n");
    }
    return bad != 0;
}

static void dump(void) {
    int g;
    int y;

    for (g = 0; g < DW3_GLYPH_COUNT; g++) {
        const unsigned char *a = dw3_glyph_art(g);

        printf("%s\n", dw3_glyph_name(g));
        for (y = 0; y < DW3_GLYPH_H; y++) {
            int x;

            for (x = 0; x < DW3_GLYPH_W; x++) {
                putchar('0' + a[y * DW3_GLYPH_W + x]);
            }
            putchar('\n');
        }
    }
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "dump") == 0) {
        dump();
        return 0;
    }
    if (argc > 2 && strcmp(argv[1], "png") == 0) {
        return check_png_dir(argv[2]);
    }
    test_art();
    test_from_cap();
    test_pack();
    test_rgba();
    test_plan();
    test_pageturn();
    if (g_fail) {
        return 1;
    }
    printf("OK: switch glyphs\n");
    return 0;
}
