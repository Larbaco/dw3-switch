/* P6.27 / P6.28: the Yes / No confirmation of "Quit game" (title menu) and
 * "Return to title" (field menu), and the two entries' switches. Policy lives
 * in port/options/confirm.c; the window is drawn by the native overlay
 * (platforms/pc/src/title_overlay_art.c, dw3_pc_dialog_art). */
#ifndef DW3_PORT_CONFIRM_H
#define DW3_PORT_CONFIRM_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DW3_CONFIRM_QUIT = 0,   /* title menu: flush the saves, leave the game */
    DW3_CONFIRM_RETURN = 1  /* field menu: the P6.9 restart path */
};

/* 1 when this build has the native overlay that draws the window (PC SDL3 GPU
 * and the Switch). Without it neither entry is offered. */
int dw3_port_confirm_available(void);

/* The title menu's fourth row, Quit game (always on where it is available). */
int dw3_port_title_quit_enabled(void);

/* The field menu's last row, Return to title. Shown only while the Restart
 * hotkey setting is ON (P6.9's opt-in), so the default field menu is the
 * game's own and the replay references of every field screen stay valid. */
int dw3_port_field_return_enabled(void);

/* The row's text in the game's own font encoding (uppercase letters and
 * spaces, as the field menu's other rows), for LANGUAGE `language` of the EU
 * disc (1 and 2 English, 3 French, 4 Italian, 5 German, 6 Spanish; the US disc
 * passes 2). Valid until the next call. */
const char *dw3_port_field_return_label(int language);

/* Open the window with the cursor on No. */
void dw3_port_confirm_open(int kind);
int dw3_port_confirm_is_open(void);

/* One frame of the open window: Up / Down / Left / Right move the cursor,
 * Cross takes the choice under it, Triangle or Circle cancel. A no-op while
 * closed and on the frame that opened it. Called by the screen that opened
 * the window, after it skipped its own input. */
void dw3_port_confirm_tick(void);

/* Hide the window without a choice (restart, mode change). */
void dw3_port_confirm_close(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_CONFIRM_H */
