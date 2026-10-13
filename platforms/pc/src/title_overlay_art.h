#ifndef DW3_PC_TITLE_OVERLAY_ART_H
#define DW3_PC_TITLE_OVERLAY_ART_H
#include <stdint.h>
/* RGBA8888 byte order, 960x720 transparent canvas. selected is 0..2 for the menu, -1 for PRESS START. */
void dw3_pc_title_overlay_art(uint8_t *rgba, int pitch, int selected);
#endif
