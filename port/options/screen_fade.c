/* The level of the whole-picture fade to black; see port_screen_fade.h. */
#include "port_screen_fade.h"

static int g_level;

void dw3_port_screen_fade_set(int level) {
    g_level = level < 0 ? 0 : level > 255 ? 255 : level;
}

int dw3_port_screen_fade_level(void) {
    return g_level;
}
