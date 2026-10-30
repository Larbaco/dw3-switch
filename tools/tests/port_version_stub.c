/* P6.37: the overlay art draws the port version (port_version.h); the host
 * tests that link title_overlay_art.c get a fixed text instead of the build's. */
#include <stdio.h>

#include "port_version.h"

void dw3_version_text(char *buf, size_t cap) {
    snprintf(buf, cap, "dw3-switch test US");
}
