/* Host test of the Map buttons screen's logic (D1, port/options/map_buttons.c) on
 * top of the real registry file (port_options.c) and the view's binding table
 * (map_view_core.c). Built with -DDW3_MAPBTN_TEST -DDW3_PORT_OPTIONS_TEST.
 *   map_buttons_test <dir>
 * One process: prints nothing on success, "OK: map buttons" and exits 0. */
#include "map_view_core.h"
#include "port_button_map.h"
#include "port_map_buttons.h"
#include "port_options.h"

#include <stdio.h>
#include <string.h>

void dw3_port_audio_apply(int music_step, int sfx_step) {
    (void)music_step;
    (void)sfx_step;
}

static int g_fail;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond);        \
            g_fail = 1;                                                    \
        }                                                                  \
    } while (0)

enum { B_SELECT = 0, B_L3 = 1, B_L2 = 8, B_R2 = 9, B_L1 = 10, B_R1 = 11, B_CROSS = 13, B_SQUARE = 15 };

int main(int argc, char **argv) {
    unsigned gen;
    int a;
    char digits[DW3_BTN_COUNT + 1];

    if (argc < 2) {
        return 2;
    }
    (void)dw3_port_options_persist_init(argv[1]);

    /* No keys: the defaults (square, l1, r1, select, l2, r2 and select for the field). */
    CHECK(dw3_mapbtn_bit(MV_ACT_OPEN) == B_SQUARE && dw3_mapbtn_bit(MV_ACT_ZOOM_OUT) == B_L1 &&
          dw3_mapbtn_bit(MV_ACT_ZOOM_IN) == B_R1 && dw3_mapbtn_bit(MV_ACT_CENTRE) == B_SELECT &&
          dw3_mapbtn_bit(MV_ACT_SCALER_PREV) == B_L2 && dw3_mapbtn_bit(MV_ACT_SCALER_NEXT) == B_R2 &&
          dw3_mapbtn_bit(MV_ACT_FIELD_OPEN) == B_SELECT);

    /* The overlay code: the PS button in the low bits, the source of the console button + 1
     * above; Select is not a PS button. PlayStation layout: Square is source 3. */
    dw3_button_map_load(DW3_LAYOUT_PLAYSTATION, "test");
    CHECK(dw3_mapbtn_value(MV_ACT_OPEN) == (3 | (4 << 4)));
    CHECK(dw3_mapbtn_value(MV_ACT_CENTRE) == 10);
    CHECK(dw3_mapbtn_value(DW3_MAPBTN_RESET) == 0);
    /* Nintendo: the PS Square is made by the Switch X (source 2). */
    dw3_button_map_load(DW3_LAYOUT_NINTENDO, "test");
    dw3_button_map_format(dw3_button_map_current(), digits);
    CHECK(dw3_mapbtn_value(MV_ACT_OPEN) == (3 | (((int)dw3_button_map_current()[3] + 1) << 4)));
    dw3_button_map_load(DW3_LAYOUT_PLAYSTATION, "test");

    /* A move to a free button: Zoom out l1 -> r1 is taken by Zoom in, so they swap. */
    gen = dw3_port_options_keys_generation();
    CHECK(dw3_mapbtn_step(MV_ACT_ZOOM_OUT, 1) == 1);
    CHECK(dw3_port_options_keys_generation() > gen);
    CHECK(dw3_mapbtn_bit(MV_ACT_ZOOM_OUT) == B_R1);   /* the next button in the cycle */
    CHECK(dw3_mapbtn_bit(MV_ACT_ZOOM_IN) == B_L1);    /* took the one Zoom out left */
    {
        /* the six always differ */
        int b, c;
        for (b = 0; b < MV_ACT_COUNT; b++) {
            for (c = b + 1; c < MV_ACT_COUNT; c++) {
                CHECK(dw3_mapbtn_bit(b) != dw3_mapbtn_bit(c));
            }
        }
    }
    /* The seven keys are in the file, as words (the cfg format of P6.32 phase 4). */
    CHECK(strcmp(dw3_port_options_file_str("map_view_zoom_out"), "r1") == 0);
    CHECK(strcmp(dw3_port_options_file_str("map_view_zoom_in"), "l1") == 0);
    CHECK(strcmp(dw3_port_options_file_str("map_view_open"), "square") == 0);
    CHECK(dw3_port_options_file_str("map_view_field_open") != NULL);

    /* Left goes back (and swaps back). */
    CHECK(dw3_mapbtn_step(MV_ACT_ZOOM_OUT, -1) == 1);
    CHECK(dw3_mapbtn_bit(MV_ACT_ZOOM_OUT) == B_L1 && dw3_mapbtn_bit(MV_ACT_ZOOM_IN) == B_R1);

    /* The cycle wraps: cross square l1 r1 l2 r2 l3 select, then cross again. */
    for (a = 0; a < 8; a++) {
        CHECK(dw3_mapbtn_step(MV_ACT_CENTRE, 1) == 1);
    }
    CHECK(dw3_mapbtn_bit(MV_ACT_CENTRE) == B_SELECT);  /* eight steps round the whole cycle */
    {
        /* each step swapped with whoever had that button: still six different buttons */
        int b, c;
        for (b = 0; b < MV_ACT_COUNT; b++) {
            for (c = b + 1; c < MV_ACT_COUNT; c++) {
                CHECK(dw3_mapbtn_bit(b) != dw3_mapbtn_bit(c));
            }
        }
    }
    dw3_mapbtn_reset();

    /* The field key never takes Cross (the field's talk button) and may share a
     * button with a view action (no swap). Select (0) -> Cross is skipped: the next is Square. */
    CHECK(dw3_mapbtn_bit(MV_ACT_FIELD_OPEN) == B_SELECT);
    CHECK(dw3_mapbtn_step(MV_ACT_FIELD_OPEN, 1) == 1);
    CHECK(dw3_mapbtn_bit(MV_ACT_FIELD_OPEN) == B_SQUARE);
    CHECK(dw3_mapbtn_bit(MV_ACT_OPEN) == B_SQUARE);    /* shared, not swapped */
    CHECK(dw3_mapbtn_step(MV_ACT_FIELD_OPEN, -1) == 1);
    CHECK(dw3_mapbtn_bit(MV_ACT_FIELD_OPEN) == B_SELECT);
    CHECK(dw3_mapbtn_step(MV_ACT_FIELD_OPEN, -1) == 1); /* Select back one: L3 */
    CHECK(dw3_mapbtn_bit(MV_ACT_FIELD_OPEN) == B_L3);

    /* A bad row changes nothing. */
    CHECK(dw3_mapbtn_step(DW3_MAPBTN_RESET, 1) == 0);
    CHECK(dw3_mapbtn_step(-1, 1) == 0);

    /* A hand-edited file with two actions on one button is shown as the defaults
     * (the view puts every action back), and the first change writes a clean table. */
    dw3_port_options_store_str("map_view_open", "l1");
    dw3_port_options_store_str("map_view_zoom_out", "l1");
    CHECK(dw3_mapbtn_bit(MV_ACT_OPEN) == B_SQUARE && dw3_mapbtn_bit(MV_ACT_ZOOM_OUT) == B_L1);
    CHECK(dw3_mapbtn_step(MV_ACT_OPEN, 1) == 1);
    CHECK(strcmp(dw3_port_options_file_str("map_view_open"), "l1") == 0);
    CHECK(strcmp(dw3_port_options_file_str("map_view_zoom_out"), "square") == 0);

    /* Reset writes the seven defaults, every time. */
    gen = dw3_port_options_keys_generation();
    dw3_mapbtn_reset();
    CHECK(dw3_port_options_keys_generation() > gen);
    CHECK(dw3_mapbtn_bit(MV_ACT_OPEN) == B_SQUARE && dw3_mapbtn_bit(MV_ACT_ZOOM_OUT) == B_L1 &&
          dw3_mapbtn_bit(MV_ACT_ZOOM_IN) == B_R1 && dw3_mapbtn_bit(MV_ACT_CENTRE) == B_SELECT &&
          dw3_mapbtn_bit(MV_ACT_SCALER_PREV) == B_L2 && dw3_mapbtn_bit(MV_ACT_SCALER_NEXT) == B_R2 &&
          dw3_mapbtn_bit(MV_ACT_FIELD_OPEN) == B_SELECT);
    CHECK(strcmp(dw3_port_options_file_str("map_view_open"), "square") == 0);
    CHECK(strcmp(dw3_port_options_file_str("map_view_field_open"), "select") == 0);

    if (g_fail) {
        return 1;
    }
    printf("OK: map buttons\n");
    return 0;
}
