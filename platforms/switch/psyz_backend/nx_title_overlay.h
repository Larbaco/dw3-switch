/* Native-resolution title menu/logo over the GLES present (game image only;
 * the devkitPro sample build does not link the title). */
#ifndef DW3_NX_TITLE_OVERLAY_H
#define DW3_NX_TITLE_OVERLAY_H

#ifdef DW3_NX_GAME_IMAGE
/* Draw over the game picture's viewport, after the composite. */
void dw3_nx_title_overlay_draw(int vx, int vy, int vw, int vh);
/* Drop GLES objects (dock recreate / gpu_shutdown). */
void dw3_nx_title_overlay_invalidate_gl(void);
/* BUG-1: join and close the options prewarm thread before main() returns.
 * Returns 1 when it was still running after the exit budget. */
int dw3_nx_title_overlay_shutdown(void);
#else
#define dw3_nx_title_overlay_draw(vx, vy, vw, vh) ((void)0)
#define dw3_nx_title_overlay_invalidate_gl() ((void)0)
#define dw3_nx_title_overlay_shutdown() (0)
#endif

#endif /* DW3_NX_TITLE_OVERLAY_H */
