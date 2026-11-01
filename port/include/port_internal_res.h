/* P6.11 internal resolution (docs/P6.11_INTERNAL_RES.md). The Options rows
 * Internal resolution (1x / 2x / 3x) and Native res while turbo are decided here,
 * once per game frame, and handed to the host through four functions the PC and
 * the Switch backends define. Platform independent. */
#ifndef DW3_PORT_INTERNAL_RES_H
#define DW3_PORT_INTERNAL_RES_H

#ifdef __cplusplus
extern "C" {
#endif

/* The highest multiplier the port draws at (the row's 3x). */
#define DW3_INTERNAL_RES_MAX 3

/* --- what the host provides (platforms/pc, platforms/switch) --------------- */

/* The highest multiplier the machine offers right now, 1..DW3_INTERNAL_RES_MAX:
 * the PC offers 3x; the Switch reads its clamp table for the console mode
 * (handheld or docked, platforms/switch/psyz_backend/nx_res_clamp.h). Called
 * every frame; it may change with a dock change. */
int dw3_platform_max_internal_res(void);

/* The host draws at `active` (1..3) from the next frame, and keeps the targets
 * of `reserve` (1 = none, else 2..3) allocated so a later switch between 1 and
 * `reserve` allocates nothing. Called only when either changed, at a frame
 * boundary (before the frame's first draw). */
void dw3_platform_apply_internal_res(int active, int reserve);

/* 1 = the frame about to be played will not be shown (fast-forward shows one in
 * `speed`) and its battle mesh need not be drawn. Called once per game frame,
 * before the frame draws; hosts that cannot skip ignore it. */
void dw3_platform_set_mesh_skip(int skip);

/* "handheld", "docked" or "pc": the mode word of the log lines. */
const char *dw3_platform_res_mode(void);

/* --- what the game side provides ------------------------------------------- */

/* The multiplier the row asks for, clamped to dw3_platform_max_internal_res():
 * 1..3. Not changed by fast-forward. */
int dw3_port_internal_res_target(void);

/* The multiplier in force this frame: target, or 1 while fast-forward is on and
 * Native res while turbo is ON. */
int dw3_port_internal_res_active(void);

/* Once per game frame, from dw3_port_turbo_poll() (after the turbo state is
 * known, before the frame draws): applies the multiplier and the mesh skip for
 * this frame, logging `dw3-res:` lines on every change. */
void dw3_port_internal_res_poll(void);

/* Idempotent: re-reads the rows and pushes a change to the host now. The
 * Options screen calls it after every row change, boot after the file is read. */
void dw3_port_internal_res_sync(void);

/* After port_options.cfg is loaded: reads DW3_INTERNAL_RES (1..3, and on the PC
 * the old DW3_PC_INTERNAL_RES) and applies the first state. */
void dw3_port_internal_res_boot(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_INTERNAL_RES_H */
