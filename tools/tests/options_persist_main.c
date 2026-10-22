/* P9.4 registry persistence driver (no disc / GPU). One process = one
 * "session"; the Python harness runs several against the same directory.
 *   options_persist_test <dir> [step ...]
 * Steps run in order after dw3_port_options_persist_init(<dir>):
 *   show               print the values
 *   open | close       open / close the Options screen
 *   row N              move the cursor to row N
 *   press              Cross on the current row
 *   env I L            apply_env(I, L) (DW3_WIDESCREEN / DW3_TURBO stand-in)
 *   noinit             skip persist_init (session-only)
 */
#include "port_options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dw3_port_options_test_set_pressed(unsigned mask);

enum { BIT_UP = 4, BIT_DOWN = 6, BIT_CROSS = 13 };

static void show(void) {
    printf("widescreen=%d fast_forward=%d restart_hotkey=%d\n",
           dw3_port_options_level(0), dw3_port_options_level(1),
           dw3_port_options_level(2));
}

static void press(int bit) {
    dw3_port_options_test_set_pressed(1u << bit);
    dw3_port_options_tick();
    dw3_port_options_test_set_pressed(0);
}

int main(int argc, char **argv) {
    int i;
    int cursor = 0;

    if (argc < 2) {
        return 2;
    }
    if (!(argc > 2 && strcmp(argv[2], "noinit") == 0)) {
        (void)dw3_port_options_persist_init(argv[1]);
    }
    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "show") == 0) {
            show();
        } else if (strcmp(argv[i], "open") == 0) {
            dw3_port_options_open();
            cursor = 0;
        } else if (strcmp(argv[i], "close") == 0) {
            dw3_port_options_close();
        } else if (strcmp(argv[i], "row") == 0 && i + 1 < argc) {
            int want = atoi(argv[++i]);

            while (cursor < want) {
                press(BIT_DOWN);
                cursor++;
            }
            while (cursor > want) {
                press(BIT_UP);
                cursor--;
            }
        } else if (strcmp(argv[i], "press") == 0) {
            press(BIT_CROSS);
        } else if (strcmp(argv[i], "env") == 0 && i + 2 < argc) {
            dw3_port_options_apply_env(atoi(argv[i + 1]), atoi(argv[i + 2]));
            i += 2;
        }
    }
    return 0;
}
