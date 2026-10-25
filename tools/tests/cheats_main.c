/* P6.20 Cheats screen driver (no disc / GPU): the cheat store, its rows on
 * the Options screen, the hooks' answers and the save marks.
 *   cheats_test <cfgdir> <memdir> [step ...]
 * Steps run in order after dw3_port_options_persist_init(<cfgdir>) and
 * dw3_cheats_init(<memdir>), with an empty card file on port 0 and port 1:
 *   options | close    open / close the Options screen
 *   ingame             open it from the field menu (P9.7: Cheats is locked)
 *   square             Square on the Options screen (opens Cheats)
 *   row N              move the Cheats cursor to row N (Up / Down)
 *   press | back       Cross / Triangle
 *   show               print the five row values and the open flags
 *   show2              print the page 2 values (money warp item count give)
 *   left | right | l1 | r1   the pad buttons that step the page 2 rows
 *   money CUR MAX      print dw3_cheats_take_money(CUR, MAX) and the result
 *   item               print dw3_cheats_take_item()
 *   warp               print dw3_cheats_take_warp()
 *   getenv NAME        print dw3_cheats_getenv(NAME) (or "(null)")
 *   battle             dw3_cheats_battle_begin()
 *   win                print dw3_cheats_take_instant_win()
 *   armed              print dw3_cheats_battle_now_armed()
 *   started            dw3_cheats_battle_now_started()
 *   rearm SPEC         dw3_cheats_soak_arm(SPEC) (P5.1 soak driver)
 *   title              dw3_cheats_title_started()
 *   marked             print dw3_cheats_game_marked()
 *   save P S SEED      write slot S of card P with bytes from SEED, then
 *                      dw3_cheats_save_written
 *   load P S SEED      dw3_cheats_save_loaded with the bytes of SEED
 *   count              print the marked saves the screen would show
 *   sounds             print the cursor / select / cancel sound counts
 */
#include "port_cheats.h"
#include "port_options.h"
#include "memcard_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dw3_port_options_test_set_pressed(unsigned mask);
int dw3_port_options_test_sound_count(int kind);
int dw3_cheat_saves_count(int log);

enum { BIT_UP = 4, BIT_RIGHT = 5, BIT_DOWN = 6, BIT_LEFT = 7, BIT_L1 = 10,
       BIT_R1 = 11, BIT_CROSS = 13, BIT_TRIANGLE = 14, BIT_SQUARE = 15 };
enum { SLOT_SIZE = 0x26BC, SLOT0 = 0x200 + 0x80 * 2 };

void dw3_port_audio_apply(int music_step, int sfx_step) {
    (void)music_step;
    (void)sfx_step;
}

static void press(int bit) {
    dw3_port_options_test_set_pressed(1u << bit);
    dw3_port_options_tick();
    dw3_port_options_test_set_pressed(0);
}

static void fill(unsigned char *buf, int seed) {
    int i;

    for (i = 0; i < SLOT_SIZE; i++) {
        buf[i] = (unsigned char)(seed * 31 + i * 7);
    }
}

static int offset_of(int slot) {
    return SLOT0 + slot * SLOT_SIZE;
}

int main(int argc, char **argv) {
    static unsigned char buf[SLOT_SIZE];
    int i;
    int cursor = 0;

    if (argc < 3) {
        return 2;
    }
    (void)dw3_port_options_persist_init(argv[1]);
    memcard_host_set_dir(argv[2]);
    for (i = 0; i < 2; i++) {
        if (memcard_host_ensure(i) != 0 || !memcard_host_has_file(i)) {
            (void)memcard_host_create_file(i, "BASLUS-01436DMW3");
        }
    }
    dw3_cheats_init(argv[2]);
    for (i = 3; i < argc; i++) {
        const char *s = argv[i];

        if (strcmp(s, "options") == 0) {
            dw3_port_options_open();
        } else if (strcmp(s, "ingame") == 0) {
            dw3_port_options_open_in_game();
        } else if (strcmp(s, "close") == 0) {
            dw3_port_options_close();
        } else if (strcmp(s, "square") == 0) {
            press(BIT_SQUARE);
            cursor = 0;
        } else if (strcmp(s, "row") == 0 && i + 1 < argc) {
            int want = atoi(argv[++i]);

            while (cursor < want) {
                press(BIT_DOWN);
                cursor++;
            }
            while (cursor > want) {
                press(BIT_UP);
                cursor--;
            }
        } else if (strcmp(s, "press") == 0) {
            press(BIT_CROSS);
        } else if (strcmp(s, "back") == 0) {
            press(BIT_TRIANGLE);
        } else if (strcmp(s, "show") == 0) {
            printf("master=%d fill=%d win=%d battle=%d saves=%d open=%d "
                   "cursor=%d options=%d\n", dw3_cheats_value(0),
                   dw3_cheats_value(1), dw3_cheats_value(2),
                   dw3_cheats_value(3), dw3_cheats_value(4),
                   dw3_cheats_is_open(), dw3_cheats_cursor(),
                   dw3_port_options_is_open());
        } else if (strcmp(s, "show2") == 0) {
            printf("money=%d warp=%d id=%d count=%d give=%d cursor=%d\n",
                   dw3_cheats_value(5), dw3_cheats_value(6),
                   dw3_cheats_value(7), dw3_cheats_value(8),
                   dw3_cheats_value(9), dw3_cheats_cursor());
        } else if (strcmp(s, "left") == 0) {
            press(BIT_LEFT);
        } else if (strcmp(s, "right") == 0) {
            press(BIT_RIGHT);
        } else if (strcmp(s, "l1") == 0) {
            press(BIT_L1);
        } else if (strcmp(s, "r1") == 0) {
            press(BIT_R1);
        } else if (strcmp(s, "money") == 0 && i + 2 < argc) {
            int out = -1;
            int cur = atoi(argv[i + 1]);
            int max = atoi(argv[i + 2]);
            int took = dw3_cheats_take_money(cur, max, &out);

            printf("money=%d out=%d\n", took, took ? out : cur);
            i += 2;
        } else if (strcmp(s, "item") == 0) {
            int id = 0;
            int count = 0;
            int took = dw3_cheats_take_item(&id, &count);

            printf("item=%d id=%d count=%d\n", took, took ? id : 0,
                   took ? count : 0);
        } else if (strcmp(s, "warp") == 0) {
            int mode = 0;
            int took = dw3_cheats_take_warp(&mode);

            printf("warp=%d mode=0x%x\n", took, took ? mode : 0);
        } else if (strcmp(s, "getenv") == 0 && i + 1 < argc) {
            const char *v = dw3_cheats_getenv(argv[++i]);

            printf("getenv=%s\n", v != NULL ? v : "(null)");
        } else if (strcmp(s, "battle") == 0) {
            dw3_cheats_battle_begin();
        } else if (strcmp(s, "win") == 0) {
            printf("win=%d\n", dw3_cheats_take_instant_win());
        } else if (strcmp(s, "armed") == 0) {
            printf("armed=%d\n", dw3_cheats_battle_now_armed());
        } else if (strcmp(s, "rearm") == 0 && i + 1 < argc) {
            dw3_cheats_soak_arm(argv[++i]);
        } else if (strcmp(s, "started") == 0) {
            dw3_cheats_battle_now_started();
        } else if (strcmp(s, "title") == 0) {
            dw3_cheats_title_started();
        } else if (strcmp(s, "marked") == 0) {
            printf("marked=%d\n", dw3_cheats_game_marked());
        } else if ((strcmp(s, "save") == 0 || strcmp(s, "load") == 0) &&
                   i + 3 < argc) {
            int port = atoi(argv[i + 1]);
            int slot = atoi(argv[i + 2]);

            fill(buf, atoi(argv[i + 3]));
            if (s[0] == 's') {
                memcard_host_file_write(port, (unsigned)offset_of(slot), buf,
                                        SLOT_SIZE);
                (void)memcard_host_flush(port);
                dw3_cheats_save_written(port, slot, offset_of(slot), buf,
                                        SLOT_SIZE);
            } else {
                dw3_cheats_save_loaded(port, slot, buf, SLOT_SIZE);
            }
            i += 3;
        } else if (strcmp(s, "count") == 0) {
            printf("count=%d\n", dw3_cheat_saves_count(1));
        } else if (strcmp(s, "sounds") == 0) {
            printf("cursor=%d select=%d cancel=%d\n",
                   dw3_port_options_test_sound_count(0),
                   dw3_port_options_test_sound_count(1),
                   dw3_port_options_test_sound_count(2));
        } else {
            fprintf(stderr, "unknown step %s\n", s);
            return 2;
        }
    }
    memcard_host_shutdown();
    return 0;
}
