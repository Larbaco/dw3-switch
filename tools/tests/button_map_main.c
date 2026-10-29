/* Host test of port/options/button_map.c (P9.8, docs/CONTROLS_BUNDLE.md): the
 * presets, the permutation rules, the cfg digits, the permute of a raw pad word
 * and the log lines. Built with -DDW3_BUTTON_MAP_TEST. */
#include "port_button_map.h"

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

/* A raw word with only the given PS buttons pressed (bits clear). */
static unsigned word_with(const int *buttons, int n) {
    unsigned w = 0xFFFFu;
    int i;

    for (i = 0; i < n; i++) {
        w &= ~(1u << dw3_button_raw_bit(buttons[i]));
    }
    return w;
}

static void test_presets(void) {
    unsigned char m[DW3_BTN_COUNT];
    char text[DW3_BTN_COUNT + 1];

    CHECK(dw3_button_map_preset(DW3_LAYOUT_PLAYSTATION, m) == 0, "ps preset");
    dw3_button_map_format(m, text);
    CHECK(strcmp(text, "0123456789") == 0, "identity digits %s", text);
    CHECK(dw3_button_map_preset(DW3_LAYOUT_NINTENDO, m) == 0, "nin preset");
    dw3_button_map_format(m, text);
    /* docs/CONTROLS_BUNDLE.md 2.4: Cross<-Circle position (A), Circle<-Triangle
     * position (X), Triangle<-Cross position (B). */
    CHECK(strcmp(text, "1203456789") == 0, "nintendo digits %s", text);
    CHECK(dw3_button_map_valid(m), "nintendo is a permutation");
    CHECK(dw3_button_map_preset(7, m) == -1, "unknown layout refused");
    CHECK(dw3_button_layout_parse("nintendo") == DW3_LAYOUT_NINTENDO, "parse n");
    CHECK(dw3_button_layout_parse("playstation") == DW3_LAYOUT_PLAYSTATION,
          "parse p");
    CHECK(dw3_button_layout_parse("custom") == -1, "custom is phase 2");
    CHECK(dw3_button_layout_parse("Nintendo") == -1, "case matters");
    CHECK(dw3_button_layout_parse(NULL) == -1, "NULL");
}

static void test_validate_and_parse(void) {
    unsigned char m[DW3_BTN_COUNT];
    unsigned char bad[DW3_BTN_COUNT] = {0, 0, 2, 3, 4, 5, 6, 7, 8, 9};
    unsigned char big[DW3_BTN_COUNT] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10};
    char text[DW3_BTN_COUNT + 1];

    CHECK(!dw3_button_map_valid(bad), "a repeated source is not a permutation");
    CHECK(!dw3_button_map_valid(big), "source 10 is out of range");
    CHECK(dw3_button_map_parse("9876543210", m) == 0, "reverse parses");
    dw3_button_map_format(m, text);
    CHECK(strcmp(text, "9876543210") == 0, "round trip %s", text);
    CHECK(dw3_button_map_parse("012345678", m) == -1, "nine digits");
    CHECK(dw3_button_map_parse("01234567890", m) == -1, "eleven digits");
    CHECK(dw3_button_map_parse("0023456789", m) == -1, "repeat");
    CHECK(dw3_button_map_parse("01234567ab", m) == -1, "letters");
    CHECK(dw3_button_map_parse(NULL, m) == -1, "NULL");
}

static void test_permute(void) {
    unsigned char id[DW3_BTN_COUNT];
    unsigned char nin[DW3_BTN_COUNT];
    unsigned char swap[DW3_BTN_COUNT] = {0, 1, 2, 3, 4, 5, 6, 7, 9, 8};
    int cross[1] = {DW3_BTN_CROSS};
    int circle[1] = {DW3_BTN_CIRCLE};
    int tri[1] = {DW3_BTN_TRIANGLE};
    int sq[1] = {DW3_BTN_SQUARE};
    int r3[1] = {DW3_BTN_R3};
    int l3[1] = {DW3_BTN_L3};
    unsigned w;
    unsigned all;
    int i;

    dw3_button_map_preset(DW3_LAYOUT_PLAYSTATION, id);
    dw3_button_map_preset(DW3_LAYOUT_NINTENDO, nin);

    /* Identity: every 16-bit word comes back unchanged. */
    for (w = 0; w <= 0xFFFFu; w += 0x101u) {
        CHECK(dw3_button_map_permute(id, w) == w, "identity %04x", w);
    }
    CHECK(dw3_button_map_permute(id, 0x1FFFFu) == 0xFFFFu, "high bits dropped");

    /* Nintendo: the source at the Circle position (Switch A) is Cross. */
    CHECK(dw3_button_map_permute(nin, word_with(circle, 1)) ==
              word_with(cross, 1), "A (Circle position) -> Cross");
    CHECK(dw3_button_map_permute(nin, word_with(cross, 1)) ==
              word_with(tri, 1), "B (Cross position) -> Triangle");
    CHECK(dw3_button_map_permute(nin, word_with(tri, 1)) ==
              word_with(circle, 1), "X (Triangle position) -> Circle");
    CHECK(dw3_button_map_permute(nin, word_with(sq, 1)) ==
              word_with(sq, 1), "Y (Square position) stays Square");
    /* Chords keep every member. */
    {
        int src[3] = {DW3_BTN_CIRCLE, DW3_BTN_CROSS, DW3_BTN_SQUARE};
        int dst[3] = {DW3_BTN_CROSS, DW3_BTN_TRIANGLE, DW3_BTN_SQUARE};

        CHECK(dw3_button_map_permute(nin, word_with(src, 3)) ==
                  word_with(dst, 3), "three faces at once");
    }
    /* Everything else passes through for both presets: Select 0, Start 3, the
     * d-pad 4..7 and the shoulders, triggers and clicks. */
    for (i = 0; i < 16; i++) {
        unsigned only = 0xFFFFu & ~(1u << i);
        int face = i == 12 || i == 13 || i == 14;

        if (!face) {
            CHECK(dw3_button_map_permute(nin, only) == only,
                  "bit %d passes through Nintendo", i);
        }
    }
    /* Nothing pressed and everything pressed are fixed points. */
    CHECK(dw3_button_map_permute(nin, 0xFFFFu) == 0xFFFFu, "idle");
    all = 0u;
    CHECK(dw3_button_map_permute(nin, all) == all, "all pressed");
    /* A custom swap (phase 2 uses the same function). */
    CHECK(dw3_button_map_valid(swap), "swap valid");
    CHECK(dw3_button_map_permute(swap, word_with(l3, 1)) == word_with(r3, 1),
          "L3 source -> R3");
    CHECK(dw3_button_map_permute(swap, word_with(r3, 1)) == word_with(l3, 1),
          "R3 source -> L3");
}

static void test_names(void) {
    CHECK(strcmp(dw3_button_ps_name(DW3_BTN_CROSS), "cross") == 0, "ps name");
    CHECK(strcmp(dw3_button_source_name(DW3_BTN_CROSS), "B") == 0,
          "Cross position is B");
    CHECK(strcmp(dw3_button_source_name(DW3_BTN_CIRCLE), "A") == 0,
          "Circle position is A");
    CHECK(strcmp(dw3_button_source_name(DW3_BTN_TRIANGLE), "X") == 0, "X");
    CHECK(strcmp(dw3_button_source_name(DW3_BTN_SQUARE), "Y") == 0, "Y");
    CHECK(strcmp(dw3_button_source_label(DW3_BTN_R3), "Right stick click") == 0,
          "R3 label");
}

/* The state: default identity, the load / change lines, the first press. */
static void test_state(void) {
    int cross[1] = {DW3_BTN_CROSS};
    int circle[1] = {DW3_BTN_CIRCLE};
    unsigned a = word_with(circle, 1);
    unsigned b = word_with(cross, 1);

    CHECK(dw3_button_map_layout() == DW3_LAYOUT_PLAYSTATION, "boots playstation");
    CHECK(dw3_button_map_apply(0xFFFFu) == 0xFFFFu, "idle passes");
    CHECK(dw3_button_map_apply(b) == b, "default: B stays Cross");
    dw3_button_map_load(DW3_LAYOUT_PLAYSTATION, "default");
    CHECK(dw3_button_map_apply(b) == b, "loaded default");
    dw3_button_map_load(DW3_LAYOUT_NINTENDO, "cfg");
    CHECK(dw3_button_map_layout() == DW3_LAYOUT_NINTENDO, "loaded nintendo");
    CHECK(dw3_button_map_apply(a) == b, "nintendo: A -> Cross");
    CHECK(dw3_button_map_apply(a) == b, "apply is idempotent per frame");
    dw3_button_map_change(DW3_LAYOUT_PLAYSTATION, "screen");
    CHECK(dw3_button_map_apply(0xFFFFu) == 0xFFFFu, "nothing held after the change");
    CHECK(dw3_button_map_apply(b) == b, "back to playstation");
    dw3_button_map_log_invalid("button_layout is not playstation or nintendo");
}

/* The press that changes the layout must not be read in the new map. */
static void test_swallow(void) {
    int circle[1] = {DW3_BTN_CIRCLE};
    int cross[1] = {DW3_BTN_CROSS};
    int start[1] = {DW3_BTN_L1};
    unsigned a = word_with(circle, 1);
    unsigned b = word_with(cross, 1);
    unsigned l = word_with(start, 1);
    unsigned idle = 0xFFFFu;

    dw3_button_map_load(DW3_LAYOUT_PLAYSTATION, "default");
    /* B (the Cross position) is held while the row switches to Nintendo */
    CHECK(dw3_button_map_apply(b) == b, "before: B is Cross");
    dw3_button_map_change(DW3_LAYOUT_NINTENDO, "screen");
    CHECK(dw3_button_map_apply(b) == idle, "B still held: not a Triangle press");
    CHECK(dw3_button_map_apply(b & l) == idle, "held buttons stay swallowed");
    CHECK(dw3_button_map_apply(idle) == idle, "released");
    CHECK(dw3_button_map_apply(a) == b, "after the release the map is live: A is Cross");
    /* A held while switching back to PlayStation: no Circle press */
    dw3_button_map_change(DW3_LAYOUT_PLAYSTATION, "screen");
    CHECK(dw3_button_map_apply(a) == idle, "A still held: not a Circle press");
    CHECK(dw3_button_map_apply(idle) == idle, "released again");
    CHECK(dw3_button_map_apply(a) == a, "PlayStation: A is Circle");
    /* Start and the d-pad are never swallowed */
    dw3_button_map_change(DW3_LAYOUT_NINTENDO, "screen");
    CHECK(dw3_button_map_apply(0xFFFFu & ~(1u << 3)) == (0xFFFFu & ~(1u << 3)),
          "Start passes while swallowing");
    dw3_button_map_load(DW3_LAYOUT_PLAYSTATION, "default");
}

int main(void) {
    test_swallow();
    test_presets();
    test_validate_and_parse();
    test_permute();
    test_names();
    test_state();
    if (g_fail) {
        return 1;
    }
    printf("OK: button map\n");
    return 0;
}
