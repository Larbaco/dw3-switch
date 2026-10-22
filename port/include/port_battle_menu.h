/* P7.9 battle menu memory: the command menu and the technique list open on
 * what the player picked last. Policy and state in
 * port/battle_menu/battle_menu_memory.c; hooks in patch 0067. Design:
 * docs/P7.9_BATTLE_MENU_MEMORY.md. */
#ifndef DW3_PORT_BATTLE_MENU_H
#define DW3_PORT_BATTLE_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

/* The file-only key of port_options.cfg (no row on the Options screen). */
#define DW3_BATTLE_MENU_MEMORY_KEY "battle_menu_memory"

enum {
    DW3_BATTLE_MENU_MEMORY_OFF = 0,     /* the original: the menu opens on Attack */
    DW3_BATTLE_MENU_MEMORY_COMMAND = 1, /* the command menu remembers */
    DW3_BATTLE_MENU_MEMORY_TECH = 2     /* + the technique list remembers */
};

/* Mode from the option file; anything but 0, 1 or 2 is 0. */
int dw3_port_battle_menu_mode(void);

/* The BATTLE_COMMAND_* the player's turn starts from (FIGHTSTG_updatePlayerTurn
 * substate 0 resets PlayerTurn.command to Attack). Off: Attack. */
int dw3_port_battle_menu_start_command(void);

/* The line the command menu opens on for `command` (PlayerTurn.command).
 * `asleep` / `no_digivolve` are the active fighter's FIGHTER_ASLEEP /
 * FIGHTER_NO_DIGIVOLVE: a remembered line that is greyed out is replaced by
 * Attack, the original start. Off: `command` unchanged. */
int dw3_port_battle_menu_open_command(int command, int asleep, int no_digivolve);

/* The player confirmed `command` in the command menu. */
void dw3_port_battle_menu_command_picked(int command);

/* Technique list of party member `member`: the technique id (SKILL_ID) to put
 * the cursor on, or 0 for the first line (Off, mode 1, nothing remembered). */
int dw3_port_battle_menu_tech_wanted(int member);

/* The player confirmed technique `tech` (SKILL_ID) for party member `member`. */
void dw3_port_battle_menu_tech_picked(int member, int tech);

/* The list opened with the cursor on `tech` (log line for the replay tests). */
void dw3_port_battle_menu_tech_applied(int member, int tech, int page, int line);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_BATTLE_MENU_H */
