#ifndef DW3_PORT_TITLE_OPTIONS_H
#define DW3_PORT_TITLE_OPTIONS_H

/* Title rows retain the owner's sprites and fit above the copyright. */
#define DW3_TITLE_NEW_GAME 0
#define DW3_TITLE_PORT_OPTIONS 1
#define DW3_TITLE_CONTINUE 2
#define DW3_TITLE_ROW_Y(row) (149 + 16 * (row))

/* Uses the game's resident font sheet; no bundled title artwork. */
void dw3_port_title_options_draw(int layer_id, int x, int y, int selected);

#endif
