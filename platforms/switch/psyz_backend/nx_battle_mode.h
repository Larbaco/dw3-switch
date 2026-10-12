/* Runtime FIGHTSTG battle render mode (OLED A/B). Field never uses mesh.
 * See docs/DEBUG_OVERLAY.md and docs/P5.2_BATTLE_GPU.md. */
#ifndef DW3_NX_BATTLE_MODE_H
#define DW3_NX_BATTLE_MODE_H

typedef enum Dw3NxBattleMode {
    DW3_NX_BATTLE_SOFT = 0,      /* software RasterTriangle (default) */
    DW3_NX_BATTLE_SOFT_BBOX = 1, /* soft + bbox skip (stub → soft for now) */
    DW3_NX_BATTLE_GPU_SOLID = 2, /* mesh FBO, solid vertex colour */
    DW3_NX_BATTLE_GPU_TEX = 3    /* mesh FBO, textured/CLUT path */
} Dw3NxBattleMode;

/* Seed from DW3_MESH_SOFT / DW3_NX_MESH_SOFT_BATTLE. Idempotent. */
void dw3_nx_battle_mode_init(void);

/* Edge chord Minus+ZL+ZR — call after padUpdate each poll. */
void dw3_nx_battle_mode_poll(void);

Dw3NxBattleMode dw3_nx_battle_mode_get(void);

/* Short HUD / log token: soft | soft+bbox | gpu-solid | gpu-tex */
const char *dw3_nx_battle_mode_token(void);

/* Non-zero → OT triangles stay on software RasterTriangle. */
int dw3_nx_battle_mode_use_soft(void);

/* Non-zero → GLES mesh with NOTEXTURE (ignore CLUT / tex sample). */
int dw3_nx_battle_mode_use_solid(void);

#endif /* DW3_NX_BATTLE_MODE_H */
