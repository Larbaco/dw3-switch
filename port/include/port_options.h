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

/* Test / stub registry (document-only until P9.3 wires real flags). */
int dw3_port_options_stub_count(void);
const char *dw3_port_options_stub_label(int index);
int dw3_port_options_stub_value(int index);
void dw3_port_options_stub_set(int index, int value);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_OPTIONS_H */
