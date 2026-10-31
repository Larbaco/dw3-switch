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
    DW3_CONFIRM_RETURN = 1, /* field menu: the P6.9 restart path */
    /* P6.38, owned by the Options screen (Region at start row): "Restart in EU
     * now?", and the one-button notice "Restart the game to apply." */
    DW3_CONFIRM_REGION = 2,
    DW3_CONFIRM_NOTICE = 3
};

/* 1 when this build has the native overlay that draws the window (PC SDL3 GPU
 * and the Switch). Without it neither entry is offered. */
int dw3_port_confirm_available(void);

/* The title menu's fourth row, Quit game (always on where it is available). */
int dw3_port_title_quit_enabled(void);

/* The field menu's last row, Return to title (always there where the native
 * overlay is; independent of the Restart hotkey setting, which only gates the
 * chord). */
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

/* P6.38: the Region at start row's windows. The Options screen opens them,
 * ticks them (dw3_port_confirm_tick_owned, every frame it is open) and waits
 * for them; the title and field menus' own dw3_port_confirm_tick() leaves
 * them alone, so a window is never answered twice in one frame.
 *   open_region: "Restart in <target> now?" (target 1 US / 2 EU), plus
 *                "Progress since your last save will be lost." when in_game;
 *                the cursor starts on No;
 *   open_notice: "Restart the game to apply.", one OK button. */
void dw3_port_confirm_open_region(int target, int in_game);
void dw3_port_confirm_open_notice(void);
/* 1 while a window opened by the two calls above is up. */
int dw3_port_confirm_owned_is_open(void);
void dw3_port_confirm_tick_owned(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_CONFIRM_H */
