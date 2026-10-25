#ifndef DW3_PORT_TITLE_OPTIONS_H
#define DW3_PORT_TITLE_OPTIONS_H

/* Title rows retain the owner's sprites and fit above the copyright. */
#define DW3_TITLE_NEW_GAME 0
#define DW3_TITLE_PORT_OPTIONS 1
#define DW3_TITLE_CONTINUE 2
/* P6.28: the native overlay's fourth row, below Continue (not drawn, and not
 * reachable, without the overlay: dw3_port_title_quit_enabled()). */
#define DW3_TITLE_QUIT 3
#if defined(DW3_PC_HOST)
#define DW3_TITLE_ROW_Y(row) (143 + 16 * (row))
#else
#define DW3_TITLE_ROW_Y(row) (149 + 16 * (row))
#endif

/* Uses the game's resident font sheet; no bundled title artwork. */
void dw3_port_title_options_draw(int layer_id, int x, int y, int selected);

/* Hide the native title overlay (menu plaques, logo, fade); used by restart. */
void dw3_port_title_overlay_reset(void);

#endif
