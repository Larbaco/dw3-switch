/* P7.9 battle menu memory. Remembers the last command of the player's turn
 * and, per partner, the last technique, for the session (the process): the
 * state is not saved (docs/P7.9_BATTLE_MENU_MEMORY.md says why). Off by
 * default: with `battle_menu_memory` absent or 0 in port_options.cfg every
 * function returns what the original code would use, so a run is
 * bit-identical to a build without patch 0067. */
#include "port_battle_menu.h"

#include "port_options.h"

#ifdef DW3_PORT_BATTLE_MENU_TEST
#include <stdio.h>
#define BM_LOG(...)                    \
    do {                               \
        fprintf(stderr, __VA_ARGS__);  \
        fputc('\n', stderr);           \
    } while (0)
#else
#include <psyz/log.h>
/* psyz_log, not stderr: on Switch stderr only reaches nxlink, and these lines
 * are the check in sdmc:/switch/dw3/dw3.log. */
#define BM_LOG(...) INFOF(__VA_ARGS__)
#endif

/* BATTLE_COMMAND_* of the decomp (battle_mode/menu.h) */
enum {
    CMD_ATTACK = 0,
    CMD_TECH = 1,
    CMD_DIGIVOLVE = 2,
    CMD_SWITCH = 3,
    CMD_ITEM = 4,
    CMD_RUN = 5,
    CMD_COUNT = 6
};

/* the partners the game has room for; a member outside this is not remembered */
#define BM_MEMBERS 8

static const char *const CMD_NAME[CMD_COUNT] = {
    "Attack", "Techniques", "Digivolve", "Switch", "Items", "Run"
};

static int g_command = CMD_ATTACK;
static int g_tech[BM_MEMBERS];

int dw3_port_battle_menu_mode(void) {
    int v = dw3_port_options_file_int(DW3_BATTLE_MENU_MEMORY_KEY, 0);

    return v == DW3_BATTLE_MENU_MEMORY_COMMAND ||
                   v == DW3_BATTLE_MENU_MEMORY_TECH
               ? v
               : DW3_BATTLE_MENU_MEMORY_OFF;
}

int dw3_port_battle_menu_start_command(void) {
    if (dw3_port_battle_menu_mode() == DW3_BATTLE_MENU_MEMORY_OFF) {
        return CMD_ATTACK;
    }
    return g_command;
}

int dw3_port_battle_menu_open_command(int command, int asleep,
                                      int no_digivolve) {
    int line = command;

    if (dw3_port_battle_menu_mode() == DW3_BATTLE_MENU_MEMORY_OFF) {
        return command;
    }
    if (line < 0 || line >= CMD_COUNT ||
        (asleep && line < CMD_SWITCH) ||
        (no_digivolve && line == CMD_DIGIVOLVE)) {
        line = CMD_ATTACK;
    }
    BM_LOG("dw3-battle-menu: command menu opens on %s%s", CMD_NAME[line],
           line != CMD_ATTACK ? " (remembered)" : "");
    return line;
}

void dw3_port_battle_menu_command_picked(int command) {
    if (dw3_port_battle_menu_mode() == DW3_BATTLE_MENU_MEMORY_OFF) {
        return;
    }
    /* Run ends the turn (or the battle) and Digivolve is greyed out for the
     * form it leads to: neither is a line worth coming back to. */
    if (command < 0 || command >= CMD_COUNT || command == CMD_RUN ||
        command == CMD_DIGIVOLVE) {
        return;
    }
    g_command = command;
    BM_LOG("dw3-battle-menu: remembered command %s", CMD_NAME[command]);
}

int dw3_port_battle_menu_tech_wanted(int member) {
    if (dw3_port_battle_menu_mode() != DW3_BATTLE_MENU_MEMORY_TECH ||
        member < 0 || member >= BM_MEMBERS) {
        return 0;
    }
    return g_tech[member];
}

void dw3_port_battle_menu_tech_picked(int member, int tech) {
    if (dw3_port_battle_menu_mode() != DW3_BATTLE_MENU_MEMORY_TECH ||
        member < 0 || member >= BM_MEMBERS || tech <= 0) {
        return;
    }
    g_tech[member] = tech;
    BM_LOG("dw3-battle-menu: remembered technique %d for partner %d", tech,
           member);
}

void dw3_port_battle_menu_tech_applied(int member, int tech, int page,
                                       int line) {
    BM_LOG("dw3-battle-menu: technique list of partner %d opens on technique "
           "%d (page %d, line %d, remembered)", member, tech, page, line);
}
