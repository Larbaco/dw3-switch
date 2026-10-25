/* P9.7 (slice 2): the port Options screen in battle. Policy lives in
 * port/options/battle_options.c; the screen is port/options/port_options.c
 * (the in-game mode of slice 1), drawn by the native overlay. Patch 0165 is the
 * game side: a poll in the battle's command menu and a hold branch in the main
 * loop. docs/PORT_OPTIONS.md "Options in battle". */
#ifndef DW3_PORT_BATTLE_OPTIONS_H
#define DW3_PORT_BATTLE_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

/* What the battle shows of its own clock, for the log lines that prove the
 * freeze: the queued events, the sum of their remaining times and the sum of
 * the fighters' HP. Filled by the patch (it knows FIGHTSTG's data). */
typedef void (*Dw3BattleProbe)(int *events, int *event_time, int *hp);

/* Offered where the native overlay that draws the screen exists (PC SDL3 GPU,
 * Switch): the same condition as the field menu's Options row. */
int dw3_port_battle_options_enabled(void);

/* Called every frame by the battle's command menu while it waits for the
 * player. Select (Minus on the Switch) with no shoulder button held opens the
 * screen (in `language`'s lettering; 2 on the US disc) and freezes the battle.
 * Returns 1 when it opened this frame (the menu must not read the pad again). */
int dw3_port_battle_options_poll(int language, Dw3BattleProbe probe);

/* Called at the top of every main-loop iteration. 1 while the screen is open
 * (and for the frame in which it closes): the caller skips the task list, the
 * widescreen frame and the draw, and only presents the held picture. On the
 * first call after the close it gives the game's clocks back as they were and
 * returns 0. */
int dw3_port_battle_options_hold_frame(void);

/* 1 while the battle is held, for the presenters (the Switch keeps the 3D
 * picture of its mesh layer on screen instead of dropping it after a frame
 * without draws). */
int dw3_port_battle_options_holding(void);

/* A restart (P6.9) or a mode change while held: forget the hold. */
void dw3_port_battle_options_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_BATTLE_OPTIONS_H */
