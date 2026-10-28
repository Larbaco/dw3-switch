/* EU language select (CNTY_SEL, mode file 0x165): the port also confirms the
 * highlighted language with the game's confirm button (logical Cross), not only
 * with Start. Patch 0187; notes in docs/PORT_OPTIONS.md. */
#ifndef DW3_PORT_LANG_SELECT_H
#define DW3_PORT_LANG_SELECT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Which button confirmed the screen. */
#define DW3_LANG_CONFIRM_NONE 0
#define DW3_LANG_CONFIRM_START 1
#define DW3_LANG_CONFIRM_CROSS 2

/* Decides what confirmed the screen on this frame from the two pressed edges.
 * Start wins when both arrive together (the original button). Returns one of the
 * DW3_LANG_CONFIRM_* values; NONE means no confirmation. */
int dw3_port_lang_select_confirm(int start_pressed, int cross_pressed);

/* The player confirmed `language` (the value stored in LANGUAGE, 2..6 on the EU
 * disc) with `how` (DW3_LANG_CONFIRM_START or _CROSS). Logs one line. */
void dw3_port_lang_select_chosen(int language, int how);

#ifdef __cplusplus
}
#endif

#endif
