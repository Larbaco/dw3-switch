/* P5.5 stage sweep: an opt-in test run that walks every loadable stage of the
 * game, one after the other, logs what happened to each, and survives a crash
 * (docs/P5.5_STAGE_SWEEP.md, ROADMAP P5.5).
 *
 * Exists only in builds that define DW3_CHEATS (the same gate as the P6.20
 * Cheats screen: a --no-cheats build is the same game as before). Off unless
 * port_options.cfg holds `stage_sweep=1` (PC replay runs may use the
 * environment variable DW3_STAGE_SWEEP=1 instead). It never writes a save: the
 * auto-save is held off while it is on, and the sweep never opens the memory
 * card screen. Its own files are two small text files beside port_options.cfg
 * (sdmc:/switch/dw3/stage_sweep_us.txt or _eu.txt, the progress per region;
 * D3: the keys are global and every log line ends with region=us|eu).
 *
 * The game side is one hook (patch 0178) in updateModeTask, which calls
 * dw3_sweep_tick() once per game frame with the current game mode and
 * requests the mode it returns. The sweep goes STAGSLCT (the debug stage
 * select, mode 0x1500) -> stage -> STAGSLCT -> next stage ..., as the PC P3.8
 * sweep did (tools/stage_sweep.py).
 *
 * Log lines (once per event, prefix "dw3-sweep:"):
 *   stage WSTAGxxx ok in N ms (frames F, mem free M)
 *   stage WSTAGxxx FAILED: <reason>
 *   done X/Y ok, failed: WSTAGaaa (reason), ...
 */
#ifndef DW3_PORT_STAGE_SWEEP_H
#define DW3_PORT_STAGE_SWEEP_H

#ifdef __cplusplus
extern "C" {
#endif

/* The debug stage select, where the sweep starts and returns to. */
#define DW3_SWEEP_SELECT_MODE 0x1500

/* 1 when the sweep is switched on (read once; later calls are free). */
int dw3_sweep_enabled(void);

/* One game frame of the mode task. `mode` is GAME.funcs.getMode(). Returns
 * the game mode the caller must request now (requestMode(mode, 0)), or 0. */
int dw3_sweep_tick(int mode);

/* 1 while a sweep owns the game (enabled and not yet finished). Holds the
 * auto-save off and the stage select from acting on its cursor. After the
 * summary the game is the owner's again. */
int dw3_sweep_active(void);

/* Test support: the stage table and the pure helpers. */
int dw3_sweep_stage_count(void);
int dw3_sweep_stage_mode(int index);
int dw3_sweep_stage_number(int index);
/* Directory of the progress file (default: the options directory). */
void dw3_sweep_set_dir(const char *dir);
/* Forget everything and re-read the key and the file (host tests). */
void dw3_sweep_test_reset(void);
/* Test hook: the clock, in ms (0 = the real one). */
void dw3_sweep_test_set_clock(unsigned long ms);
/* Test hook: limit the sweep to the given modes (NULL = all). */
void dw3_sweep_test_only(const int *modes, int n);
/* Test hook: switch on without the key. */
void dw3_sweep_test_enable(int on, int frames);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_STAGE_SWEEP_H */
