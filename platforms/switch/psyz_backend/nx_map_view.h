/* P6.32 phase 0: the full-screen map screen over the GLES present (game image
 * only; the devkitPro sample build does not link the game). */
#ifndef DW3_NX_MAP_VIEW_H
#define DW3_NX_MAP_VIEW_H

#ifdef DW3_NX_GAME_IMAGE
/* Draw over the game picture's viewport after the composite, when the screen
 * is open. */
void dw3_nx_map_view_draw(int vx, int vy, int vw, int vh);
/* Drop GLES objects (dock recreate / gpu_shutdown). */
void dw3_nx_map_view_invalidate_gl(void);
#else
#define dw3_nx_map_view_draw(vx, vy, vw, vh) ((void)0)
#define dw3_nx_map_view_invalidate_gl() ((void)0)
#endif

#endif /* DW3_NX_MAP_VIEW_H */
