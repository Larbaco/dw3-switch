/* P9.4 registry persistence driver (no disc / GPU). One process = one
 * "session"; the Python harness runs several against the same directory.
 *   options_persist_test <dir> [step ...]
 * Steps run in order after dw3_port_options_persist_init(<dir>):
 *   show               print the three switch values
 *   volumes            print the Music / SFX volume steps
 *   pacing             print the Encounter rate / Text speed modes
 *   battle             print the Battle cursor mode (battle_menu_memory)
 *   layout             print the Button layout (0 PlayStation, 1 Nintendo)
 *   region             print the Region at start row (0 Ask, 1 US, 2 EU; DW3_REGION_IMAGE build)
 *   open | close       open / close the Options screen
 *   row N              put the cursor on registry row N (opens its group, no keys)
 *   nav N              the same with the keys: Down / Confirm / Down ... as a
 *                      player would (cursor and select sounds are played)
 *   press              Cross on the current row
 *   left | right       D-pad Left / Right (volume rows)
 *   back               Triangle (close with the cancel sound)
 *   up | down          D-pad Up / Down (one row, cursor sound)
 *   gains              print the Q15 gains last handed to the mixer
 *   env I L            apply_env(I, L) (DW3_WIDESCREEN / DW3_TURBO stand-in)
 *   fileint KEY DEF    print dw3_port_options_file_int(KEY, DEF) (file-only keys)
 *   raw HEX            hand the pad host's raw word (a clear bit = pressed) to the map
 *   tick               one Options tick with nothing pressed
 *   map                print the map in force (ten digits) and its layout
 *   noinit             skip persist_init (session-only)
 */
#include "port_audio.h"
#include "port_button_map.h"
#include "port_options.h"
#include "port_options_groups.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dw3_port_options_test_set_pressed(unsigned mask);
void dw3_port_options_test_goto_row(int reg);

enum { BIT_UP = 4, BIT_RIGHT = 5, BIT_DOWN = 6, BIT_LEFT = 7, BIT_CROSS = 13,
       BIT_TRIANGLE = 14 };

/* Stand-in for port/audio/audio_bus.c: remember what the registry applied. */
static int g_music_step = -1;
static int g_sfx_step = -1;
static int g_apply_calls;

void dw3_port_audio_apply(int music_step, int sfx_step) {
    g_music_step = music_step;
    g_sfx_step = sfx_step;
    g_apply_calls++;
    /* stderr, in order with the registry's own lines: the harness checks that
     * the new gain is applied before the feedback sound is played. */
    fprintf(stderr, "test: audio apply music=%d sfx=%d\n", music_step, sfx_step);
}

static void show(void) {
    printf("widescreen=%d fast_forward=%d restart_hotkey=%d\n",
           dw3_port_options_level(0), dw3_port_options_level(1),
           dw3_port_options_level(2));
}

static void volumes(void) {
    printf("music_volume=%d sfx_volume=%d\n", dw3_port_options_level(3),
           dw3_port_options_level(4));
}

static void pacing(void) {
    printf("encounter_rate=%d text_speed=%d\n", dw3_port_options_level(5),
           dw3_port_options_level(6));
}

static void battle(void) {
    printf("battle_menu_memory=%d\n", dw3_port_options_level(7));
}

static void layout(void) {
    printf("button_layout=%d\n", dw3_port_options_level(9));
}

static void region(void) {
    printf("region_row=%d\n", dw3_port_options_level(10));
}

static void autosave(void) {
    printf("autosave=%d\n", dw3_port_options_level(8));
}

static void gains(void) {
    printf("music_step=%d sfx_step=%d music_gain=%d sfx_gain=%d calls=%d\n",
           g_music_step, g_sfx_step, dw3_port_audio_gain_q15(g_music_step),
           dw3_port_audio_gain_q15(g_sfx_step), g_apply_calls);
}

static void press(int bit) {
    dw3_port_options_test_set_pressed(1u << bit);
    dw3_port_options_tick();
    dw3_port_options_test_set_pressed(0);
}

/* Walk to registry row `reg` with the keys: back to the group list, down to
 * the row's group, open it, down to the row. */
static void nav(int reg) {
    int g, k, want_g = -1, want_k = 0, ids[DW3_OPTGRP_COUNT], n, idx = 0;

    for (g = 0; g < DW3_OPTGRP_COUNT && want_g < 0; g++) {
        if (g == DW3_OPTGRP_CHEATS) continue;
        for (k = 0; k < dw3_optgroups[g].nrows; k++) {
            if (dw3_optgroups[g].rows[k] == reg) {
                want_g = g;
                want_k = k;
                break;
            }
        }
    }
    if (want_g < 0) {
        return;
    }
    if (dw3_port_options_group() >= 0) {
        press(BIT_TRIANGLE);
    }
    n = dw3_optgroups_visible(0, ids);
    for (k = 0; k < n; k++) {
        if (ids[k] == want_g) {
            idx = k;
        }
    }
    while (dw3_port_options_cursor() < idx) {
        press(BIT_DOWN);
    }
    while (dw3_port_options_cursor() > idx) {
        press(BIT_UP);
    }
    press(BIT_CROSS);
    while (dw3_port_options_cursor() < want_k) {
        press(BIT_DOWN);
    }
}

int main(int argc, char **argv) {
    int i;

    if (argc < 2) {
        return 2;
    }
    if (!(argc > 2 && strcmp(argv[2], "noinit") == 0)) {
        (void)dw3_port_options_persist_init(argv[1]);
    }
    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "show") == 0) {
            show();
        } else if (strcmp(argv[i], "volumes") == 0) {
            volumes();
        } else if (strcmp(argv[i], "pacing") == 0) {
            pacing();
        } else if (strcmp(argv[i], "battle") == 0) {
            battle();
        } else if (strcmp(argv[i], "autosave") == 0) {
            autosave();
        } else if (strcmp(argv[i], "region") == 0) {
            region();
        } else if (strcmp(argv[i], "layout") == 0) {
            layout();
        } else if (strcmp(argv[i], "open") == 0) {
            dw3_port_options_open();
        } else if (strcmp(argv[i], "close") == 0) {
            dw3_port_options_close();
        } else if (strcmp(argv[i], "row") == 0 && i + 1 < argc) {
            dw3_port_options_test_goto_row(atoi(argv[++i]));
        } else if (strcmp(argv[i], "nav") == 0 && i + 1 < argc) {
            nav(atoi(argv[++i]));
        } else if (strcmp(argv[i], "press") == 0) {
            press(BIT_CROSS);
        } else if (strcmp(argv[i], "left") == 0) {
            press(BIT_LEFT);
        } else if (strcmp(argv[i], "right") == 0) {
            press(BIT_RIGHT);
        } else if (strcmp(argv[i], "back") == 0) {
            press(BIT_TRIANGLE);
        } else if (strcmp(argv[i], "up") == 0) {
            press(BIT_UP);
        } else if (strcmp(argv[i], "down") == 0) {
            press(BIT_DOWN);
        } else if (strcmp(argv[i], "raw") == 0 && i + 1 < argc) {
            (void)dw3_button_map_apply((unsigned)strtoul(argv[++i], NULL, 16));
        } else if (strcmp(argv[i], "tick") == 0) {
            dw3_port_options_tick();
        } else if (strcmp(argv[i], "map") == 0) {
            char digits[DW3_BTN_COUNT + 1];

            dw3_button_map_format(dw3_button_map_current(), digits);
            printf("map=%s layout=%d\n", digits, dw3_button_map_layout());
        } else if (strcmp(argv[i], "gains") == 0) {
            gains();
        } else if (strcmp(argv[i], "fileint") == 0 && i + 2 < argc) {
            printf("%s=%d\n", argv[i + 1],
                   dw3_port_options_file_int(argv[i + 1], atoi(argv[i + 2])));
            i += 2;
        } else if (strcmp(argv[i], "env") == 0 && i + 2 < argc) {
            dw3_port_options_apply_env(atoi(argv[i + 1]), atoi(argv[i + 2]));
            i += 2;
        }
    }
    return 0;
}
