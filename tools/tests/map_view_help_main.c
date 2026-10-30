/* Host test of the map view's help line (P6.32 phase 7, port/map_view/map_view_help.c):
 * the names per layout, custom map and binding on the PC and on the Switch (text and
 * glyphs), the words in the five languages, and that every line fits the 640 px bar
 * whatever buttons the player has. Built with -DDW3_GLYPHS_TEST and linked with
 * port/options/button_map.c (-DDW3_BUTTON_MAP_TEST), switch_glyphs.c and
 * map_view_core.c. */
#include "map_view_help.h"
#include "port_glyphs.h"

#include <stdio.h>
#include <stdlib.h>
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

static const MvHelpMode PC = {0, 0}, SW = {1, 0}, SWG = {1, 1};

static void build(const unsigned char map[DW3_BTN_COUNT], const MvBindings *b, const MvHelpMode *m,
                  int lang, char l1[MV_HELP_LINE_CAP], char l2[MV_HELP_LINE_CAP]) {
    char names[MV_HS_COUNT][MV_HELP_NAME_CAP];

    dw3_mv_help_names(map, b, m, names);
    CHECK(dw3_mv_help_line(lang, 0, names, l1, MV_HELP_LINE_CAP) > 0, "line 1 lang %d", lang);
    CHECK(dw3_mv_help_line(lang, 1, names, l2, MV_HELP_LINE_CAP) > 0, "line 2 lang %d", lang);
}

static void same(const char *got, const char *want, const char *what) {
    CHECK(strcmp(got, want) == 0, "%s: got \"%s\", want \"%s\"", what, got, want);
}

static void preset(int layout, unsigned char map[DW3_BTN_COUNT]) {
    CHECK(dw3_button_map_preset(layout, map) == 0, "preset %d", layout);
}

static void test_english_unchanged(void) {
    unsigned char ps[DW3_BTN_COUNT];
    MvBindings b;
    char l1[MV_HELP_LINE_CAP], l2[MV_HELP_LINE_CAP];

    preset(DW3_LAYOUT_PLAYSTATION, ps);
    dw3_mv_bindings_default(&b);
    /* the PoC / phase 4 strings, byte for byte (the pictures that show them did not change) */
    build(ps, &b, &PC, MV_HELP_EN, l1, l2);
    same(l1, "PAD/L-STICK CURSOR  R-STICK/L1/R1 ZOOM  L2/R2 SCALER", "pc line 1");
    same(l2, "SELECT CENTRE ON YOUR AREA  CIRCLE HUD  TRI BACK", "pc line 2");
    build(ps, &b, &SW, MV_HELP_EN, l1, l2);
    same(l1, "PAD/L-STICK CURSOR  R-STICK/L/R ZOOM  ZL/ZR SCALER", "switch line 1");
    same(l2, "MINUS CENTRE ON YOUR AREA  A HUD  X BACK", "switch line 2");
}

static void test_layouts(void) {
    unsigned char m[DW3_BTN_COUNT];
    MvBindings b;
    char l1[MV_HELP_LINE_CAP], l2[MV_HELP_LINE_CAP];
    const char *vals[MV_ACT_COUNT] = {"r2", "square", "cross", "l3", "l1", "r1"};
    char why[200];

    dw3_mv_bindings_default(&b);
    /* Nintendo: Circle is made by the Triangle position (Switch X), Triangle by the Cross
     * position (Switch B); the shoulders are the same */
    preset(DW3_LAYOUT_NINTENDO, m);
    build(m, &b, &SW, MV_HELP_EN, l1, l2);
    same(l1, "PAD/L-STICK CURSOR  R-STICK/L/R ZOOM  ZL/ZR SCALER", "nintendo switch line 1");
    same(l2, "MINUS CENTRE ON YOUR AREA  X HUD  B BACK", "nintendo switch line 2");
    build(m, &b, &PC, MV_HELP_EN, l1, l2);
    same(l2, "SELECT CENTRE ON YOUR AREA  TRI HUD  CROSS BACK", "nintendo pc line 2");
    /* a custom map: the Square and R2 positions swapped (digits 0127456389), so zoom out
     * on L1 and the scaler pair L2 / R2 follow the swap */
    CHECK(dw3_button_map_parse("0127456389", m) == 0, "custom map parses");
    build(m, &b, &SW, MV_HELP_EN, l1, l2);
    same(l1, "PAD/L-STICK CURSOR  R-STICK/L/R ZOOM  ZL/Y SCALER", "custom switch line 1");
    CHECK(dw3_button_map_parse("4567012389", m) == 0, "custom map 2 parses");
    build(m, &b, &SW, MV_HELP_EN, l1, l2);
    same(l1, "PAD/L-STICK CURSOR  R-STICK/B/A ZOOM  X/Y SCALER", "custom 2 switch line 1");
    build(m, &b, &PC, MV_HELP_EN, l1, l2);
    same(l1, "PAD/L-STICK CURSOR  R-STICK/CROSS/CIRCLE ZOOM  TRI/SQUARE SCALER", "custom 2 pc line 1");
    same(l2, "SELECT CENTRE ON YOUR AREA  R1 HUD  L2 BACK", "custom 2 pc line 2");
    /* a rebinding: open R2 (not in the line), zoom out Square, zoom in Cross, centre L3,
     * scaler L1 / R1 (the phase 4 evidence) under the PlayStation layout */
    preset(DW3_LAYOUT_PLAYSTATION, m);
    CHECK(dw3_mv_bindings_resolve(&b, vals, why, sizeof(why)) == 0, "rebind resolves: %s", why);
    build(m, &b, &PC, MV_HELP_EN, l1, l2);
    same(l1, "PAD/L-STICK CURSOR  R-STICK/SQUARE/CROSS ZOOM  L1/R1 SCALER", "rebind pc line 1");
    same(l2, "L3 CENTRE ON YOUR AREA  CIRCLE HUD  TRI BACK", "rebind pc line 2");
    build(m, &b, &SW, MV_HELP_EN, l1, l2);
    same(l1, "PAD/L-STICK CURSOR  R-STICK/Y/B ZOOM  L/R SCALER", "rebind switch line 1");
    same(l2, "L3 CENTRE ON YOUR AREA  A HUD  X BACK", "rebind switch line 2");
    /* the same rebinding under the Nintendo layout: Cross is made by the Circle position (A) */
    preset(DW3_LAYOUT_NINTENDO, m);
    build(m, &b, &SW, MV_HELP_EN, l1, l2);
    same(l1, "PAD/L-STICK CURSOR  R-STICK/Y/A ZOOM  L/R SCALER", "rebind nintendo switch line 1");
    same(l2, "L3 CENTRE ON YOUR AREA  X HUD  B BACK", "rebind nintendo switch line 2");
}

static void test_glyphs(void) {
    unsigned char m[DW3_BTN_COUNT];
    MvBindings b;
    char names[MV_HS_COUNT][MV_HELP_NAME_CAP];
    char l1[MV_HELP_LINE_CAP], l2[MV_HELP_LINE_CAP], shown[300];

    dw3_mv_bindings_default(&b);
    preset(DW3_LAYOUT_PLAYSTATION, m);
    dw3_mv_help_names(m, &b, &SWG, names);
    /* zoom out L1 -> L, zoom in R1 -> R, scaler L2 / R2 -> ZL / ZR, centre -> Minus (no glyph),
     * Circle -> A, Triangle -> X */
    CHECK((unsigned char)names[MV_HS_ZOOM_OUT][0] == MV_HELP_GLYPH0 + DW3_GLYPH_L &&
          names[MV_HS_ZOOM_OUT][1] == 0, "zoom out is the L glyph");
    CHECK((unsigned char)names[MV_HS_ZOOM_IN][0] == MV_HELP_GLYPH0 + DW3_GLYPH_R, "zoom in R");
    CHECK((unsigned char)names[MV_HS_SCALER_PREV][0] == MV_HELP_GLYPH0 + DW3_GLYPH_ZL, "scaler prev ZL");
    CHECK((unsigned char)names[MV_HS_SCALER_NEXT][0] == MV_HELP_GLYPH0 + DW3_GLYPH_ZR, "scaler next ZR");
    CHECK(strcmp(names[MV_HS_CENTRE], "MINUS") == 0, "Minus has no glyph: %s", names[MV_HS_CENTRE]);
    CHECK((unsigned char)names[MV_HS_HUD][0] == MV_HELP_GLYPH0 + DW3_GLYPH_A, "hud A");
    CHECK((unsigned char)names[MV_HS_BACK][0] == MV_HELP_GLYPH0 + DW3_GLYPH_X, "back X");
    build(m, &b, &SWG, MV_HELP_EN, l1, l2);
    same(dw3_mv_help_log_text(l1, shown, sizeof(shown)),
         "PAD/L-STICK CURSOR  R-STICK/[L]/[R] ZOOM  [ZL]/[ZR] SCALER", "glyph line 1 for the log");
    same(dw3_mv_help_log_text(l2, shown, sizeof(shown)),
         "MINUS CENTRE ON YOUR AREA  [A] HUD  [X] BACK", "glyph line 2 for the log");
    /* a glyph is 13 px, a character 8 */
    CHECK(dw3_mv_help_width("AB") == 16, "width of text");
    CHECK(dw3_mv_help_width(l1) == (int)(strlen(l1) - 4) * 8 + 4 * 13, "width with 4 glyphs");
    /* L3 and R3 (a map that puts a PS button on a stick click) have no glyph */
    {
        MvBindings r;
        const char *vals[MV_ACT_COUNT] = {NULL, "l3", NULL, NULL, NULL, NULL};
        char why[200];

        CHECK(dw3_mv_bindings_resolve(&r, vals, why, sizeof(why)) == 0, "l3 binding: %s", why);
        CHECK(dw3_button_map_parse("0123456798", m) == 0, "L3 map parses");
        dw3_mv_help_names(m, &r, &SWG, names);
        CHECK(strcmp(names[MV_HS_ZOOM_OUT], "R3") == 0, "L3 bound, made by the R3 click: %s",
              names[MV_HS_ZOOM_OUT]);
    }
    /* glyphs on the PC mode are never drawn: the flag only means something with Switch names */
    {
        const MvHelpMode pcg = {0, 1};

        preset(DW3_LAYOUT_PLAYSTATION, m);
        dw3_mv_help_names(m, &b, &pcg, names);
        CHECK(strcmp(names[MV_HS_ZOOM_OUT], "L1") == 0, "pc with glyphs shows labels: %s",
              names[MV_HS_ZOOM_OUT]);
    }
}

static int line_ok(const char *t) {
    for (; *t; t++) {
        const unsigned char c = (unsigned char)*t;

        if (c >= MV_HELP_GLYPH0) {
            continue;
        }
        if (c < 32 || c > 126 || c == '{' || c == '}') {
            return 0;
        }
        if (c >= 'a' && c <= 'z') {
            return 0; /* capitals only, like the rest of the bars */
        }
    }
    return 1;
}

static void test_languages(void) {
    unsigned char m[DW3_BTN_COUNT];
    MvBindings b;
    char en[2][MV_HELP_LINE_CAP], l[2][MV_HELP_LINE_CAP];
    int lang;

    CHECK(dw3_mv_help_lang(1) == MV_HELP_EN && dw3_mv_help_lang(2) == MV_HELP_EN, "US and EU english");
    CHECK(dw3_mv_help_lang(3) == MV_HELP_FR && dw3_mv_help_lang(4) == MV_HELP_IT &&
          dw3_mv_help_lang(5) == MV_HELP_DE && dw3_mv_help_lang(6) == MV_HELP_ES, "EU languages");
    CHECK(dw3_mv_help_lang(0) == MV_HELP_EN && dw3_mv_help_lang(99) == MV_HELP_EN, "unknown is english");
    CHECK(strcmp(dw3_mv_help_lang_name(MV_HELP_DE), "de") == 0, "name");
    preset(DW3_LAYOUT_NINTENDO, m);
    dw3_mv_bindings_default(&b);
    build(m, &b, &SW, MV_HELP_EN, en[0], en[1]);
    for (lang = 0; lang < MV_HELP_LANGS; lang++) {
        int i;

        build(m, &b, &SW, lang, l[0], l[1]);
        for (i = 0; i < 2; i++) {
            CHECK(line_ok(l[i]), "lang %d line %d is not plain capitals: \"%s\"", lang, i, l[i]);
            CHECK(strstr(l[i], "{") == NULL, "lang %d line %d has a token left", lang, i);
            if (lang != MV_HELP_EN) {
                CHECK(strcmp(l[i], en[i]) != 0, "lang %d line %d is not translated", lang, i);
            }
        }
        /* the names are the same in every language: the text matches the layout in all of them */
        CHECK(strstr(l[0], "ZL/ZR") != NULL, "lang %d line 1 names ZL/ZR: %s", lang, l[0]);
        CHECK(strstr(l[0], "/L/R ") != NULL, "lang %d line 1 names L/R: %s", lang, l[0]);
        CHECK(strstr(l[1], "MINUS ") == l[1], "lang %d line 2 starts with MINUS: %s", lang, l[1]);
        CHECK(strstr(l[1], " X HUD ") != NULL, "lang %d line 2 names X for the bars: %s", lang, l[1]);
        CHECK(strstr(l[1], " B ") != NULL, "lang %d line 2 names B for back: %s", lang, l[1]);
    }
    /* a line that does not fit the buffer is refused, not cut */
    {
        char names[MV_HS_COUNT][MV_HELP_NAME_CAP];
        char small[10];

        dw3_mv_help_names(m, &b, &SW, names);
        CHECK(dw3_mv_help_line(MV_HELP_EN, 0, names, small, sizeof(small)) == -1, "small buffer");
        CHECK(dw3_mv_help_line(MV_HELP_LANGS, 0, names, l[0], MV_HELP_LINE_CAP) == -1, "bad lang");
        CHECK(dw3_mv_help_line(MV_HELP_EN, 2, names, l[0], MV_HELP_LINE_CAP) == -1, "bad line");
    }
}

/* Every line fits the bar for every language, mode, layout, map and binding. Width only
 * adds, so the worst case is every name at its longest; the sweep checks real maps too. */
static void test_fit(void) {
    char names[MV_HS_COUNT][MV_HELP_NAME_CAP];
    char line[MV_HELP_LINE_CAP];
    unsigned char m[DW3_BTN_COUNT];
    MvBindings b;
    const MvHelpMode *modes[3];
    int lang, i, mi, n, widest = 0;
    unsigned seed = 12345u;

    modes[0] = &PC;
    modes[1] = &SW;
    modes[2] = &SWG;
    for (i = 0; i < MV_HS_COUNT; i++) {
        snprintf(names[i], MV_HELP_NAME_CAP, "SELECT"); /* the longest word any mode has: 6 */
    }
    for (lang = 0; lang < MV_HELP_LANGS; lang++) {
        for (i = 0; i < MV_HELP_LINES; i++) {
            int w;

            CHECK(dw3_mv_help_line(lang, i, names, line, sizeof(line)) > 0, "worst line");
            w = dw3_mv_help_width(line);
            if (w > widest) widest = w;
            CHECK(w <= MV_HELP_MAX_W, "worst case: lang %s line %d is %d px of %d: %s",
                  dw3_mv_help_lang_name(lang), i + 1, w, MV_HELP_MAX_W, line);
        }
    }
    printf("widest help line, every name 6 characters: %d of %d px\n", widest, MV_HELP_MAX_W);
    /* a sweep over layouts, custom maps and the six bindings, in all three modes */
    dw3_mv_bindings_default(&b);
    for (n = 0; n < 4000; n++) {
        static const char *const v[8] = {"cross", "square", "l1", "r1", "l2", "r2", "l3", "select"};
        const char *vals[MV_ACT_COUNT];
        char why[200];
        int j;

        for (j = 0; j < DW3_BTN_COUNT; j++) m[j] = (unsigned char)j;
        for (j = DW3_BTN_COUNT - 1; j > 0; j--) { /* a random permutation */
            int k;
            unsigned char t;

            seed = seed * 1103515245u + 12345u;
            k = (int)((seed >> 16) % (unsigned)(j + 1));
            t = m[j]; m[j] = m[k]; m[k] = t;
        }
        for (j = 0; j < MV_ACT_COUNT; j++) {
            seed = seed * 1103515245u + 12345u;
            vals[j] = (n % 3 == 0) ? NULL : v[(seed >> 16) % 8];
        }
        if (dw3_mv_bindings_resolve(&b, vals, why, sizeof(why)) != 0) {
            dw3_mv_bindings_default(&b);
        }
        for (mi = 0; mi < 3; mi++) {
            dw3_mv_help_names(m, &b, modes[mi], names);
            for (lang = 0; lang < MV_HELP_LANGS; lang++) {
                for (i = 0; i < MV_HELP_LINES; i++) {
                    CHECK(dw3_mv_help_line(lang, i, names, line, sizeof(line)) > 0, "line");
                    CHECK(dw3_mv_help_width(line) <= MV_HELP_MAX_W,
                          "map %d mode %d lang %d line %d: %d px", n, mi, lang, i,
                          dw3_mv_help_width(line));
                }
            }
        }
    }
}

int main(void) {
    test_english_unchanged();
    test_layouts();
    test_glyphs();
    test_languages();
    test_fit();
    if (g_fail) {
        return 1;
    }
    printf("OK: map view help\n");
    return 0;
}
