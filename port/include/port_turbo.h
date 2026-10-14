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
 * the option is OFF, or the mode is not field or battle (title, movies,
 * menus and saves always run at normal speed). Hosts read it every vsync. */
int dw3_port_turbo_speed(void);

#ifdef DW3_PORT_TURBO_TEST
void dw3_port_turbo_test_set(int r3_held, int mode_file);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_TURBO_H */
