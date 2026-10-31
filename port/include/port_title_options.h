#ifndef DW3_PORT_TITLE_OPTIONS_H
#define DW3_PORT_TITLE_OPTIONS_H

#include "port_autosave.h"
#include "port_confirm.h"

/* Title rows retain the owner's sprites and fit above the copyright. */
#define DW3_TITLE_NEW_GAME 0
#define DW3_TITLE_PORT_OPTIONS 1
#define DW3_TITLE_CONTINUE 2
/* P6.28: the native overlay's fourth row, below Continue (not drawn, and not
 * reachable, without the overlay: dw3_port_title_quit_enabled()). */
#define DW3_TITLE_QUIT 3
/* P6.31 follow-up: with an auto-save file (and the native overlay) Continue
 * (row 2) loads it and the old Continue becomes a separate Load row (3), which
 * pushes Quit game to row 4. Without the file the menu is exactly as before. */
#define DW3_TITLE_LOAD 3
#if defined(DW3_PC_HOST)
#define DW3_TITLE_ROW_Y(row) (143 + 16 * (row))
#else
#define DW3_TITLE_ROW_Y(row) (149 + 16 * (row))
#endif

/* 1 when the title menu has the extra Load row (see DW3_TITLE_LOAD). */
static inline int dw3_port_title_load_row(void) {
    return dw3_port_title_quit_enabled() && dw3_autosave_file_present();
}
/* The row Quit game is on: 3, or 4 with the Load row. */
static inline int dw3_port_title_quit_row(void) {
    return dw3_port_title_load_row() ? DW3_TITLE_QUIT + 1 : DW3_TITLE_QUIT;
}

/* Uses the game's resident font sheet; no bundled title artwork. */
void dw3_port_title_options_draw(int layer_id, int x, int y, int selected);

/* Hide the native title overlay (menu plaques, logo, fade); used by restart. */
void dw3_port_title_overlay_reset(void);

/* PRESS START only when the game is ready to answer it (patch 0199,
 * port/options/title_ready.c). `begin`: the title menu task starts. `frame`:
 * one tick on PRESS START before the original reads the pad; returns 1 when
 * the prompt may be drawn: this tick reads Start (and, EU, the logo has been
 * composed or 30 frames passed). `drawn`: the menu is about
 * to draw PRESS START (the frame the proof line reports). */
void dw3_port_title_prompt_begin(void);
int dw3_port_title_prompt_frame(int substate, int start_pressed);
void dw3_port_title_prompt_drawn(void);

#endif
