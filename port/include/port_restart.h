#ifndef DW3_PORT_RESTART_H
#define DW3_PORT_RESTART_H

#ifdef __cplusplus
extern "C" {
#endif

/* Check the opt-in chord after PAD.update; flush saves before resetting GAME. */
int dw3_port_restart_poll(void);

/* P6.27: ask for the same reset from the field menu's Return to title (after
 * its confirmation). The next dw3_port_restart_poll() runs it: same flush, same
 * rule that a failed flush cancels it. */
void dw3_port_restart_request(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_RESTART_H */
