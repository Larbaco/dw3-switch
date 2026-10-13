#ifndef DW3_PORT_RESTART_H
#define DW3_PORT_RESTART_H

#ifdef __cplusplus
extern "C" {
#endif

/* Check the opt-in chord after PAD.update; flush saves before resetting GAME. */
int dw3_port_restart_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_RESTART_H */
