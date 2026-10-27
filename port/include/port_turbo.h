/* P6.4 fast-forward ("turbo"), shared by the PC and Switch game hosts. */
#ifndef DW3_PORT_TURBO_H
#define DW3_PORT_TURBO_H

#ifdef __cplusplus
extern "C" {
#endif

#define DW3_TURBO_SPEED_MIN 2
#define DW3_TURBO_SPEED_MAX 4
#define DW3_TURBO_SPEED_DEFAULT 2

/* Once per frame, after PAD.update: R3 (right stick click on the Switch,
 * key 2 on the PSY-Z keyboard map) toggles fast-forward on and off.
 * DW3_TURBO=2..4 in the environment turns it on at boot at that speed;
 * DW3_TURBO=0 turns the option off. */
void dw3_port_turbo_poll(void);

/* Game frames per presented frame right now: 1 when fast-forward is off,
 * the option is OFF, or the mode is not field, battle or the battle report
 * (the results screen after a won battle); title, movies, menus and saves
 * always run at normal speed. Hosts read it every vsync. */
int dw3_port_turbo_speed(void);

/* Menus inside the field run at 1.5x, not at the full setting: the field menu
 * task (patch 0176) calls dw3_port_turbo_menu_frame() on each update, and while
 * that is current in the field, dw3_port_turbo_menu_active() is 1.
 * dw3_port_turbo_speed() is not changed by it (it stays the field's factor). */
void dw3_port_turbo_menu_frame(void);
int dw3_port_turbo_menu_active(void);

/* Game frames per vsync times two (2 = 1x, 3 = the menu's 1.5x, 2 * speed
 * otherwise). Hosts that pace by time (PC) use it instead of speed(). */
int dw3_port_turbo_rate_x2(void);

/* Hosts that skip presents (Switch) call it once per game frame at the present:
 * 1 = show this frame, 0 = skip it. At whole speeds that is one in `speed`; at
 * 1.5x it alternates 2 and 1 game frames per present. */
int dw3_port_turbo_present_due(void);

#ifdef DW3_PORT_TURBO_TEST
void dw3_port_turbo_test_set(int r3_held, int mode_file);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_TURBO_H */
