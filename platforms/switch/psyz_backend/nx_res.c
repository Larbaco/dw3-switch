/* P6.11 internal resolution, the Switch host's half. port/options/internal_res.c
 * decides the multiplier once per game frame and calls the four
 * dw3_platform_* functions below; nx_mesh.c owns the targets
 * (dw3_nx_mesh_apply_res). The clamp table is nx_res_clamp.h. */
#include "nx_res.h"
#include "nx.h"
#include "nx_mesh.h"
#include "nx_res_clamp.h"

#include <stdarg.h>
#include <stdio.h>

void dw3_nx_res_log(const char *fmt, ...) {
    char buf[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    psyz_log(1, "nx_res.c", __LINE__, __func__, "%s", buf);
}

const char *dw3_nx_res_mode_name(void) {
    Dw3NxGpuInfo info = {0};

    dw3_nx_gpu_get_info(&info);
    return info.docked ? "docked" : "handheld";
}

#ifdef DW3_NX_GAME_IMAGE
#include "port_internal_res.h"
#include "nx_game_api.h" /* last: a `both` image redirects the game calls */

int dw3_platform_max_internal_res(void) {
    Dw3NxGpuInfo info = {0};

    dw3_nx_gpu_get_info(&info);
    return info.docked ? DW3_NX_RES_MAX_DOCKED : DW3_NX_RES_MAX_HANDHELD;
}

void dw3_platform_apply_internal_res(int active, int reserve) {
    dw3_nx_mesh_apply_res(active, reserve);
}

void dw3_platform_set_mesh_skip(int skip) {
    dw3_nx_mesh_frame_begin(skip);
}

const char *dw3_platform_res_mode(void) {
    return dw3_nx_res_mode_name();
}
#endif
