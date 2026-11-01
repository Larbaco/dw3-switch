/* Host test of P9.7 (port/options/field_options.c and the in-game mode of
 * port/options/port_options.c): the Options row's text, opening the screen
 * from the field menu, the rows that are locked in game, the rows that apply
 * at once, and closing it. The pad, the sound, the mixer and the overlay are
 * stubs. Built with -DDW3_PORT_OPTIONS_TEST -DDW3_FIELD_OPTIONS_TEST. */
#include "port_field_options.h"
#include "port_options.h"
#include "port_options_groups.h"

#include "title_overlay_art.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dw3_port_options_test_set_pressed(unsigned mask);
void dw3_port_options_test_goto_row(int reg);
int dw3_port_options_test_sound_count(int kind);

enum { UP = 4, RIGHT = 5, DOWN = 6, LEFT = 7, CIRCLE = 12, CROSS = 13,
       TRIANGLE = 14 };

/* --- stubs --- */
static int g_confirm_available = 1;
static int g_music, g_sfx, g_audio_calls;
static int g_state_active = -9, g_state_selected = -9, g_state_calls;
static int g_language = -1;

int dw3_port_confirm_available(void) { return g_confirm_available; }
void dw3_port_audio_apply(int music_step, int sfx_step) {
    g_music = music_step;
    g_sfx = sfx_step;
    g_audio_calls++;
}
void dw3_pc_title_menu_set_state(int active, int selected) {
    g_state_active = active;
    g_state_selected = selected;
    g_state_calls++;
}
void dw3_pc_title_overlay_set_options_language(int lang) { g_language = lang; }
void dw3_pc_title_overlay_set_options_locked(uint64_t mask) { (void)mask; }

static int g_fail;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond);        \
            g_fail = 1;                                                    \
        }                                                                  \
    } while (0)

static void press(int bit) {
    dw3_port_options_test_set_pressed(1u << bit);
    dw3_port_field_options_tick();
    dw3_port_options_test_set_pressed(0);
}

static void title_press(int bit) {
    dw3_port_options_test_set_pressed(1u << bit);
    dw3_port_options_tick();
    dw3_port_options_test_set_pressed(0);
}

int main(void) {
    const unsigned char *p;
    int lang;
    int music_before;

    /* The row's text: the game's font codes, capitals, no accents; the five
     * languages differ where the words do. */
    for (lang = 1; lang <= 6; lang++) {
        p = (const unsigned char *)dw3_port_field_options_label(lang);
        CHECK(*p != 0);
        for (; *p; p++) {
            CHECK(*p >= 14 && *p <= 39);
        }
    }
    {
        char english[40];
        const unsigned char *o = (const unsigned char *)dw3_port_field_options_label(2);

        CHECK(o[0] == 28 && o[1] == 29 && o[2] == 33 && o[3] == 22 && o[4] == 28 &&
              o[5] == 27 && o[6] == 32 && o[7] == 0); /* O P T I O N S */
        strcpy(english, dw3_port_field_options_label(2));
        CHECK(strcmp(english, dw3_port_field_options_label(1)) == 0);
        CHECK(strcmp(english, dw3_port_field_options_label(0)) == 0);
        CHECK(strcmp(english, dw3_port_field_options_label(3)) == 0); /* French too */
        CHECK(strcmp(english, dw3_port_field_options_label(4)) != 0);
        CHECK(strcmp(english, dw3_port_field_options_label(5)) != 0);
        CHECK(strcmp(english, dw3_port_field_options_label(6)) != 0);
    }

    /* Offered where the native overlay is, like Return to title. */
    CHECK(dw3_port_field_options_enabled());
    g_confirm_available = 0;
    CHECK(!dw3_port_field_options_enabled());
    dw3_port_field_options_open(2);
    CHECK(!dw3_port_options_is_open());
    g_confirm_available = 1;

    /* A tick with nothing open does nothing. */
    press(CROSS);
    CHECK(!dw3_port_field_options_is_open());

    /* Open from the field menu: the screen is up, in game, the language
     * forwarded, the overlay shown and the rows locked. */
    dw3_port_field_options_open(5);
    CHECK(dw3_port_field_options_is_open());
    CHECK(dw3_port_options_is_open() && dw3_port_options_in_game());
    CHECK(g_language == DW3_TITLE_LANG_DE);
    CHECK(g_state_active == 1 && g_state_selected == -1);
    CHECK(dw3_port_options_row_locked(0));            /* Widescreen */
    CHECK(!dw3_port_options_row_locked(1) && !dw3_port_options_row_locked(2) &&
          !dw3_port_options_row_locked(3) && !dw3_port_options_row_locked(4) &&
          !dw3_port_options_row_locked(5) && !dw3_port_options_row_locked(6) &&
          !dw3_port_options_row_locked(7) && !dw3_port_options_row_locked(8));
    CHECK(dw3_port_options_locked_mask() ==
          (UINT64_C(1) | (UINT64_C(1) << DW3_PORT_OPT_CUSTOMIZE) | (UINT64_C(1) << DW3_PORT_OPT_SAVE_STORE) |
           (UINT64_C(1) << DW3_PORT_OPT_STAGE_SWEEP) | (UINT64_C(1) << DW3_PORT_OPT_SWEEP_FRAMES) |
           DW3_PORT_OPTIONS_LOCK_CHEATS));
    dw3_port_field_options_open(2); /* already open: ignored */
    CHECK(g_language == DW3_TITLE_LANG_DE);

    /* Widescreen is locked: Cross opens the Display group (a select sound),
     * Cross, Circle and the tick on its row change nothing. */
    CHECK(dw3_port_options_value(0) == 1);
    press(CROSS);
    CHECK(dw3_port_options_group() == DW3_OPTGRP_DISPLAY);
    CHECK(dw3_port_options_test_sound_count(1) == 1); /* the group open only */
    press(CROSS);
    press(CIRCLE);
    CHECK(dw3_port_options_value(0) == 1);
    CHECK(dw3_port_options_test_sound_count(1) == 1); /* no select sound */
    press(TRIANGLE);                                  /* back to the group list */
    CHECK(dw3_port_options_is_open() && dw3_port_options_group() == -1);

    /* Fast-forward cycles at once (2x -> 3x). */
    dw3_port_options_test_goto_row(1);
    CHECK(dw3_port_options_level(1) == 2);
    press(CROSS);
    CHECK(dw3_port_options_level(1) == 3);

    /* The Restart hotkey applies at once: the chord reads this value. */
    dw3_port_options_test_goto_row(2);
    CHECK(!dw3_port_options_restart_enabled());
    press(CROSS);
    CHECK(dw3_port_options_restart_enabled());
    press(CROSS);
    CHECK(!dw3_port_options_restart_enabled());

    /* Music volume: Left steps it down and the mixer gets it at once. */
    dw3_port_options_test_goto_row(3);
    music_before = g_audio_calls;
    press(LEFT);
    CHECK(dw3_port_options_level(3) == 9);
    CHECK(g_music == 9 && g_audio_calls > music_before);
    press(LEFT);
    press(LEFT);
    CHECK(dw3_port_options_level(3) == 7 && g_music == 7);

    /* Encounter rate: OFF, 0.5x, 1x, 1.5x, 2x by position; Left x2 from 1x. */
    dw3_port_options_test_goto_row(5);
    CHECK(dw3_port_options_level(5) == 0);
    press(LEFT);
    CHECK(dw3_port_options_level(5) == 1);
    press(LEFT);
    CHECK(dw3_port_options_level(5) == 2); /* stored value of OFF */

    /* Text speed, Battle cursor, Auto-save: all apply at once. */
    dw3_port_options_test_goto_row(6);
    press(RIGHT);
    CHECK(dw3_port_options_level(6) == 1);
    dw3_port_options_test_goto_row(7);
    press(RIGHT);
    CHECK(dw3_port_options_level(7) == 1);
    dw3_port_options_test_goto_row(8);
    press(CROSS);
    CHECK(dw3_port_options_level(8) == 1);

    /* Back on the locked row it still refuses. */
    dw3_port_options_test_goto_row(0);
    press(CROSS);
    CHECK(dw3_port_options_value(0) == 1);

    /* Triangle: out of the group, then closes: back to the field menu, the
     * overlay hidden, the pad not read by anything else this frame, in-game
     * mode off. */
    press(TRIANGLE);
    CHECK(dw3_port_options_is_open() && dw3_port_options_group() == -1);
    g_state_calls = 0;
    press(TRIANGLE);
    CHECK(!dw3_port_options_is_open());
    CHECK(!dw3_port_field_options_is_open());
    CHECK(!dw3_port_options_in_game());
    CHECK(g_state_active == 0 && g_state_selected == -2);
    CHECK(dw3_port_options_locked_mask() == 0);
    press(CROSS); /* a later tick is a no-op */
    CHECK(dw3_port_options_level(8) == 1 && dw3_port_options_value(0) == 1);

    /* The title opens the same screen with nothing locked. */
    dw3_port_options_open();
    CHECK(dw3_port_options_is_open() && !dw3_port_options_in_game());
    CHECK(!dw3_port_field_options_is_open());
    CHECK(!dw3_port_options_row_locked(0));
    CHECK(dw3_port_options_group() == -1);       /* opens on the group list */
    dw3_port_options_test_goto_row(0);
    title_press(CROSS); /* Widescreen toggles at the title */
    CHECK(dw3_port_options_value(0) == 0);
    dw3_port_options_close();

    /* The values changed in game are the ones the title screen shows. */
    CHECK(dw3_port_options_level(3) == 7 && dw3_port_options_level(5) == 2 &&
          dw3_port_options_level(1) == 3);
    (void)g_sfx;

    if (g_fail) {
        return 1;
    }
    printf("OK: field options\n");
    return 0;
}
