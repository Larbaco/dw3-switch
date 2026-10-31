/* Instant menu exit policy (port/menu_exit), no game, no disc. The option file
 * reader is a stand-in that returns g_file. */
#include "port_menu_exit.h"
#include "port_options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_file = -1; /* -1: key absent */

int dw3_port_options_file_int(const char *key, int def) {
    if (strcmp(key, DW3_MENU_INSTANT_EXIT_KEY) == 0 && g_file >= 0) {
        return g_file;
    }
    return def;
}

static int g_failed;
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                \
            g_failed++;                                                   \
        }                                                                 \
    } while (0)

int main(void) {
    /* On by default, with no file and no variable. */
    unsetenv("DW3_MENU_INSTANT_EXIT");
    CHECK(strcmp(DW3_MENU_INSTANT_EXIT_KEY, "menu_instant_exit") == 0);
    CHECK(dw3_port_menu_instant_exit_enabled() == 1);
    /* The file key decides. */
    g_file = 0;
    CHECK(dw3_port_menu_instant_exit_enabled() == 0);
    g_file = 1;
    CHECK(dw3_port_menu_instant_exit_enabled() == 1);
    /* The variable pins one run, either way, over the file. */
    setenv("DW3_MENU_INSTANT_EXIT", "0", 1);
    CHECK(dw3_port_menu_instant_exit_enabled() == 0);
    g_file = 0;
    setenv("DW3_MENU_INSTANT_EXIT", "1", 1);
    CHECK(dw3_port_menu_instant_exit_enabled() == 1);
    setenv("DW3_MENU_INSTANT_EXIT", "", 1);
    CHECK(dw3_port_menu_instant_exit_enabled() == 0);
    unsetenv("DW3_MENU_INSTANT_EXIT");

    /* The screen names of STSTATUS_screens, with and without the Card folders row. */
    CHECK(strcmp(dw3_port_menu_exit_screen_name(0, 0), "items") == 0);
    CHECK(strcmp(dw3_port_menu_exit_screen_name(1, 0), "sort") == 0);
    CHECK(strcmp(dw3_port_menu_exit_screen_name(2, 1), "map") == 0);
    CHECK(strcmp(dw3_port_menu_exit_screen_name(3, 0), "techniques") == 0);
    CHECK(strcmp(dw3_port_menu_exit_screen_name(4, 1), "status") == 0);
    CHECK(strcmp(dw3_port_menu_exit_screen_name(5, 1), "cards") == 0);
    CHECK(strcmp(dw3_port_menu_exit_screen_name(5, 0), "other") == 0);
    CHECK(strcmp(dw3_port_menu_exit_screen_name(6, 1), "other") == 0);
    CHECK(strcmp(dw3_port_menu_exit_screen_name(-1, 0), "other") == 0);

    /* The two log lines. */
    dw3_port_menu_exit_report(1, 96, 0, 0);
    dw3_port_menu_exit_report(0, 0, 2, 1);

    if (g_failed != 0) {
        printf("%d check(s) failed\n", g_failed);
        return 1;
    }
    printf("OK: menu exit policy\n");
    return 0;
}
