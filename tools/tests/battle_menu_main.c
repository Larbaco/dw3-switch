/* P7.9 battle menu memory policy (port/battle_menu), no game, no disc. The
 * Battle cursor row of the options registry is replaced by a stand-in that
 * returns g_mode. */
#include "port_battle_menu.h"
#include "port_options.h"

#include <stdio.h>
#include <string.h>

static int g_mode;

int dw3_port_options_level(int index) {
    return index == DW3_PORT_OPT_BATTLE_MENU ? g_mode : 0;
}

static int g_failed;
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                \
            g_failed++;                                                   \
        }                                                                 \
    } while (0)

enum { ATTACK, TECH, DIGIVOLVE, SWITCH, ITEM, RUN };

static void off(int mode) {
    int c;

    g_mode = mode;
    CHECK(dw3_port_battle_menu_mode() == 0);
    /* every pick is ignored and every start is the original one */
    for (c = ATTACK; c <= RUN; c++) {
        dw3_port_battle_menu_command_picked(c);
        CHECK(dw3_port_battle_menu_start_command() == ATTACK);
        CHECK(dw3_port_battle_menu_open_command(c, 0, 0) == c);
        CHECK(dw3_port_battle_menu_open_command(c, 1, 1) == c);
    }
    dw3_port_battle_menu_tech_picked(0, 53);
    CHECK(dw3_port_battle_menu_tech_wanted(0) == 0);
}

static void command_memory(void) {
    g_mode = DW3_BATTLE_MENU_MEMORY_COMMAND;
    CHECK(dw3_port_battle_menu_start_command() == ATTACK); /* nothing yet */
    dw3_port_battle_menu_command_picked(TECH);
    CHECK(dw3_port_battle_menu_start_command() == TECH);
    CHECK(dw3_port_battle_menu_open_command(TECH, 0, 0) == TECH);
    dw3_port_battle_menu_command_picked(ITEM);
    CHECK(dw3_port_battle_menu_start_command() == ITEM);
    dw3_port_battle_menu_command_picked(SWITCH);
    CHECK(dw3_port_battle_menu_start_command() == SWITCH);
    dw3_port_battle_menu_command_picked(ATTACK);
    CHECK(dw3_port_battle_menu_start_command() == ATTACK);
    /* Run and Digivolve are not remembered; out of range is ignored */
    dw3_port_battle_menu_command_picked(TECH);
    dw3_port_battle_menu_command_picked(RUN);
    CHECK(dw3_port_battle_menu_start_command() == TECH);
    dw3_port_battle_menu_command_picked(DIGIVOLVE);
    CHECK(dw3_port_battle_menu_start_command() == TECH);
    dw3_port_battle_menu_command_picked(-1);
    dw3_port_battle_menu_command_picked(6);
    CHECK(dw3_port_battle_menu_start_command() == TECH);
    /* a line that is greyed out for the fighter falls back to Attack */
    CHECK(dw3_port_battle_menu_open_command(TECH, 1, 0) == ATTACK);
    CHECK(dw3_port_battle_menu_open_command(DIGIVOLVE, 1, 0) == ATTACK);
    CHECK(dw3_port_battle_menu_open_command(SWITCH, 1, 0) == SWITCH);
    CHECK(dw3_port_battle_menu_open_command(ITEM, 1, 1) == ITEM);
    CHECK(dw3_port_battle_menu_open_command(DIGIVOLVE, 0, 1) == ATTACK);
    CHECK(dw3_port_battle_menu_open_command(TECH, 0, 1) == TECH);
    CHECK(dw3_port_battle_menu_open_command(9, 0, 0) == ATTACK);
    /* the technique list does not remember in this mode */
    dw3_port_battle_menu_tech_picked(0, 53);
    CHECK(dw3_port_battle_menu_tech_wanted(0) == 0);
}

static void tech_memory(void) {
    g_mode = DW3_BATTLE_MENU_MEMORY_TECH;
    CHECK(dw3_port_battle_menu_tech_wanted(0) == 0);
    dw3_port_battle_menu_tech_picked(0, 54);
    dw3_port_battle_menu_tech_picked(1, 70);
    CHECK(dw3_port_battle_menu_tech_wanted(0) == 54); /* per partner */
    CHECK(dw3_port_battle_menu_tech_wanted(1) == 70);
    CHECK(dw3_port_battle_menu_tech_wanted(2) == 0);
    dw3_port_battle_menu_tech_picked(0, 55);
    CHECK(dw3_port_battle_menu_tech_wanted(0) == 55);
    /* nonsense is ignored, and out-of-range members read as nothing */
    dw3_port_battle_menu_tech_picked(0, 0);
    dw3_port_battle_menu_tech_picked(0, -2);
    dw3_port_battle_menu_tech_picked(-1, 9);
    dw3_port_battle_menu_tech_picked(8, 9);
    CHECK(dw3_port_battle_menu_tech_wanted(0) == 55);
    CHECK(dw3_port_battle_menu_tech_wanted(-1) == 0);
    CHECK(dw3_port_battle_menu_tech_wanted(8) == 0);
    /* mode 2 remembers the command too */
    dw3_port_battle_menu_command_picked(ITEM);
    CHECK(dw3_port_battle_menu_start_command() == ITEM);
    /* switching the memory off hides it without losing the rest of the code's
     * original behaviour */
    g_mode = 0;
    CHECK(dw3_port_battle_menu_tech_wanted(0) == 0);
    CHECK(dw3_port_battle_menu_start_command() == ATTACK);
}

int main(void) {
    off(0);
    off(3);
    off(-1);
    command_memory();
    tech_memory();
    dw3_port_battle_menu_tech_applied(0, 54, 0, 1);
    if (g_failed != 0) {
        return 1;
    }
    printf("OK: battle menu memory policy\n");
    return 0;
}
