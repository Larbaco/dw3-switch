/* P51 item 1: the battle lists' selection bar and its log. The bar is
 * FIGHTSTG_drawCursorBar, a vsync callback that hands a small ordering table
 * of DR_MOVE copies to ContinueDraw() (port/psyz_missing/psyz_missing.c, which
 * counts the hand-overs). The Items and technique lists have no sprite
 * cursor, so the bar is their only marker. Hooks: patch 0189
 * (`FIGHTSTG_createCursor`, `FIGHTSTG_updateCursor` TASK_KILL). */
#ifndef DW3_PORT_BATTLE_CURSOR_H
#define DW3_PORT_BATTLE_CURSOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* A battle list's cursor task was created. `list` names it (command, items,
 * tech, switch, switch-in, digivolve, confused). Logs once per open. */
void dw3_port_battle_cursor_open(const void *task, const char *list);

/* The cursor task was killed: the list closed on line `sel` of `count`. Logs
 * once, with whether the bar reached the GPU (`bar=submitted`) or not
 * (`bar=dropped`) while the list was open. */
void dw3_port_battle_cursor_close(const void *task, int count, int sel);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_BATTLE_CURSOR_H */
