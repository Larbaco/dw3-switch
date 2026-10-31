/* P6.38: for host tests that build port_options.c as a combined image's game
 * copy (DW3_REGION_IMAGE) but do not exercise the restart window: the window and
 * its policy (port/options/confirm.c, region_relaunch.c) as no-ops. The window
 * itself is tested end to end by region_row_main.c. */
#include "port_confirm.h"
#include "port_region_relaunch.h"

void dw3_port_region_relaunch_reset(void) {}
void dw3_port_region_relaunch_note_step(void) {}
int dw3_port_region_relaunch_leave(int value, int in_game) {
    (void)value;
    (void)in_game;
    return 0;
}
int dw3_port_confirm_owned_is_open(void) { return 0; }
void dw3_port_confirm_tick_owned(void) {}
