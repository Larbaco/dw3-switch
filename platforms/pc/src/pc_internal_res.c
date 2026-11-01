/* P6.11 internal resolution, the PC host's half (docs/P6.11_INTERNAL_RES.md).
 * port/options/internal_res.c decides the multiplier; here it reaches PSY-Z.
 * The PC draws every layer through PSY-Z's scaled render target, so the whole
 * picture scales (the field's 2D is the same picture in N x N blocks); the
 * vertices stay integer PS1 coordinates. No clamp table: the PC offers 1x to 3x. */
#include "port_internal_res.h"

#include <psyz/video.h>
#include <stdio.h>

#if defined(DW3_PC_SDL3_GPU)
/* title_menu_overlay.c: the 3x presentation window is sized again. */
void dw3_pc_title_overlay_recheck_presentation(void);
#endif

/* The multiplier the row asks for, whatever fast-forward does: the larger of
 * what is drawn now and what is kept for the switch back. The title overlay
 * (a host module) reads it without a call into the game. */
static int g_pc_target = 1;

int dw3_pc_internal_res_target(void) {
    return g_pc_target;
}

int dw3_platform_max_internal_res(void) {
    return DW3_INTERNAL_RES_MAX;
}

const char *dw3_platform_res_mode(void) {
    return "pc";
}

void dw3_platform_set_mesh_skip(int skip) {
    (void)skip; /* the PC draws on PSY-Z's GPU path: nothing to skip */
}

void dw3_platform_apply_internal_res(int active, int reserve) {
    const int target = active > reserve ? active : reserve;

    /* The reserve first, so the switch down keeps the scaled target alive. */
    if (Psyz_VideoReserveInternalResolution(reserve > 1 ? (unsigned)reserve : 0u) != 0 ||
        Psyz_VideoSetInternalResolution((unsigned)active) != 0) {
        fprintf(stderr, "dw3-res: PSY-Z refused active=%d reserve=%d\n", active,
                reserve);
        return;
    }
    if (target != g_pc_target) {
        g_pc_target = target;
#if defined(DW3_PC_SDL3_GPU)
        dw3_pc_title_overlay_recheck_presentation();
#endif
    }
}
