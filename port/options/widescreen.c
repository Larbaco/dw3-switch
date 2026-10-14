/* P6.1 widescreen policy shared by PC and Switch game hosts. The renderer
 * owns the gutters (Psyz_VideoSetGutter); this only picks their width. */
#include "port_widescreen.h"

#include "overlay_host.h"
#include "port_options.h"

#include <psyz/log.h>
#include <psyz/video.h>
#include <stdlib.h>

#if VERSION_EU
#define DW3_WS_FIELDSTG 0x166
#define DW3_WS_FIGHTSTG 0x167
#else
#define DW3_WS_FIELDSTG 0x158
#define DW3_WS_FIGHTSTG 0x159
#endif

enum { DW3_PORT_OPT_WIDESCREEN = 0 };

static int g_env_read;
static int g_applied = -1;

int dw3_port_widescreen_enabled(void) {
    if (!g_env_read) {
        const char *env = getenv("DW3_WIDESCREEN");

        g_env_read = 1;
        if (env != NULL && env[0] == '1') {
            dw3_port_options_set(DW3_PORT_OPT_WIDESCREEN, 1);
        }
    }
    return dw3_port_options_value(DW3_PORT_OPT_WIDESCREEN);
}

int dw3_port_widescreen_px(void) {
    int32_t mode_file;

    if (!dw3_port_widescreen_enabled()) {
        return 0;
    }
    mode_file = dw3_overlay_current_mode_file();
    if (mode_file == DW3_WS_FIELDSTG) {
        return DW3_WIDESCREEN_FIELD_PX;
    }
    if (mode_file == DW3_WS_FIGHTSTG) {
        return DW3_WIDESCREEN_BATTLE_PX;
    }
    return 0;
}

void dw3_port_widescreen_frame(void) {
    int px = dw3_port_widescreen_px();

    if (px == g_applied) {
        return;
    }
    g_applied = px;
    if (Psyz_VideoSetGutter((unsigned)px) != 0) {
        WARNF("dw3-widescreen: renderer has no gutters (%d px)", px);
    } else {
        /* psyz_log, not stderr: on Switch stderr only reaches nxlink, and
         * this line is the check in sdmc:/switch/dw3/dw3.log. */
        INFOF("dw3-widescreen: gutter %d px per side", px);
    }
}
