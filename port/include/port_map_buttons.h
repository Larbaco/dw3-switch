/* D1 (docs/OPTIONS_REORG.md section 3.10): the Map buttons screen under
 * Options > Controls. Eight rows: the seven map_view_* bindings of the
 * full-screen map (map_view_core.h, MV_ACT_*: open, zoom out, zoom in, centre,
 * previous scaler, next scaler, open from the field) and the action row Reset
 * map buttons. The screen is port/options/port_options.c; this is its logic.
 *
 * The cfg keys, their words (cross square l1 r1 l2 r2 l3 select) and defaults
 * are the file-only ones of P6.32 phase 4, unchanged: a file edited by hand
 * still works. A binding is a PS button; the button map in force decides
 * which console button makes it (the same rule as the view). */
#ifndef DW3_PORT_MAP_BUTTONS_H
#define DW3_PORT_MAP_BUTTONS_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DW3_MAPBTN_ROWS = 8,
    DW3_MAPBTN_RESET = 7 /* the action row; rows 0..6 are MV_ACT_* */
};

/* The code the overlay draws for row `row` (0 for the Reset row): the PS button
 * index of port_button_map.h in the low four bits (10 = Select, which is not a
 * PS button), and the source position of the console button that makes it under
 * the button map in force plus one in the bits above (0 when there is none:
 * Select). */
int dw3_mapbtn_value(int row);

/* Move row `row` to the next (dir > 0) or previous (dir < 0) bindable button.
 * A button another of the six view actions already has is swapped: that action
 * takes the one this row leaves, so the six always differ. The field key may
 * share a button with a view action and never takes Cross (the field's talk
 * button). Writes the seven keys at once and logs the change. Returns 1 when
 * something changed. */
int dw3_mapbtn_step(int row, int dir);

/* Put the seven defaults back (the Reset map buttons row), also in game. */
void dw3_mapbtn_reset(void);

/* Test and log support: the pad bit now bound to action `act` (MV_ACT_*). */
int dw3_mapbtn_bit(int act);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_MAP_BUTTONS_H */
