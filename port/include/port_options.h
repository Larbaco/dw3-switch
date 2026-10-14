/* P9.2 port options shell — title-menu Config/Extra overlay (MODE_TITLE). */
#ifndef DW3_PORT_OPTIONS_H
#define DW3_PORT_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

void dw3_port_options_open(void);
void dw3_port_options_close(void);
int dw3_port_options_is_open(void);

/* Pad + stub registry while open. Does not set title menu choice. */
void dw3_port_options_tick(void);

/* Draw panel over title after STDWTITL_drawMenu. */
void dw3_port_options_draw(void);

/* One-line hint while NG/Continue are shown and shell is closed. */
void dw3_port_options_draw_hint(void);

/* Session-only registry; unavailable features remain safe no-op stubs. */
int dw3_port_options_count(void);
const char *dw3_port_options_label(int index);
int dw3_port_options_value(int index);
void dw3_port_options_set(int index, int value);
/* Raw row value: 0/1, or 0 (OFF) / 2..4 (speed) for Fast-forward. */
int dw3_port_options_level(int index);
void dw3_port_options_set_level(int index, int level);

/* Restart is opt-in and session-only until P9.4 adds owner-storage config. */
int dw3_port_options_restart_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_OPTIONS_H */
