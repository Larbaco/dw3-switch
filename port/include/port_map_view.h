/* P6.32: the game side of the new full-screen map screen (patch 0146).
 * The rest (state, scalers, host overlays) is port/map_view/map_view.h. */
#ifndef DW3_PORT_MAP_VIEW_H
#define DW3_PORT_MAP_VIEW_H

#ifdef __cplusplus
extern "C" {
#endif

/* Logical pad bit numbers (vendor/dw3_decomp include/engine/pad.h). */
enum {
    DW3_MV_PAD_SELECT = 0, DW3_MV_PAD_L3 = 1, DW3_MV_PAD_R3 = 2,
    DW3_MV_PAD_START = 3, DW3_MV_PAD_UP = 4, DW3_MV_PAD_RIGHT = 5,
    DW3_MV_PAD_DOWN = 6, DW3_MV_PAD_LEFT = 7, DW3_MV_PAD_L2 = 8,
    DW3_MV_PAD_R2 = 9, DW3_MV_PAD_L1 = 10, DW3_MV_PAD_R1 = 11,
    DW3_MV_PAD_CIRCLE = 12, DW3_MV_PAD_CROSS = 13, DW3_MV_PAD_TRIANGLE = 14,
    DW3_MV_PAD_SQUARE = 15
};

/* What the game gives the new screen each frame (in) and what the screen gives
 * back (out), patch 0146. */
typedef struct Dw3MapViewIo {
    /* in: the animation the game's own Map screen is on (it keeps running,
     * frozen, underneath): STSTATUS_cursorFrames[screen->cursorFrame] and
     * screen->homeFrame. */
    int cursor_anim, home_anim;
    /* in: the bounds of the name box's frame (FILE_STATUS_SPRITES frame 0x3B,
     * drawn at y 0x14 or 0xB4 of the 320x240 menu picture), from
     * dw3_port_sprite_frame_bounds. */
    int name_x0, name_y0, name_x1, name_y1;
    /* out: the cursor in map (sprite) space, and where it is on the 320x240
     * menu picture (x runs from -gutter to 320 + gutter in 16:9). The game's
     * own cursor is set to these, with the scroll that makes
     * cursor - scroll = map position, so its area lookup and its name window
     * work on the view's cursor. */
    int cur_x, cur_y, scr_x, scr_y;
    /* in: the game's own cursor and scroll as they are when the call is made
     * (screen->cursorX/Y, scrollX/Y); the host logs that the Map screen comes
     * back with the same values it was frozen with. */
    int game_cx, game_cy, game_sx, game_sy;
} Dw3MapViewIo;

/* Called every frame of the game's Map screen from STSTATUS_updateMapScreen.
 * `sheet` is the map's sprite sheet (entry 0 of file `sheet_file`), `half` the
 * game half, (home_x, home_y) the current area's mark and spot_x / spot_y the
 * 47 spot positions in map space; visited[i] is spot i + 1's flag. `held` and
 * `pressed` are the logical pad as bit masks (bit = DW3_MV_PAD_*).
 * `sheet1` / `sheet2` are the entries 1 and 2 of the same file (the town cells
 * and the area cells) and `pieces` the reveal layer the game draws over the
 * dark map: `npieces` times 4 ints (kind 1 = town cell of sheet1, 2 = visited
 * area cell of sheet2; frame; x; y in map space, before the scroll), in draw
 * order. The host composes them into the picture it shows. Returns 1
 * while the new screen has the pad (the game then skips its own pad read and
 * the area lookup and only draws its frozen screen), 0 otherwise. Square opens
 * it (widescreen on, option file key `map_screen` = 1; the default is off); the
 * game's cancel button (PS-side Triangle bit) closes it, Square does not. While it is open the pad (D-pad and left stick) moves its cursor
 * and `io` carries the cursor back to the game (see Dw3MapViewIo). */
int dw3_port_map_view_frame(const void *sheet, int sheet_file, int half,
                            int home_x, int home_y, const int *visited,
                            const int *spot_x, const int *spot_y,
                            unsigned held, unsigned pressed, const void *sheet1,
                            const void *sheet2, const int *pieces,
                            int npieces, Dw3MapViewIo *io);
int dw3_port_map_view_active(void);

/* Patch 0147 (phase 2): the marker list and the area names, read from the game's
 * own tables; nothing is written back. */
typedef struct Dw3MapViewData {
    /* the area names' file (TEXT_FILE(TEXT_ASUKA_MAP) or the Amaterasu one: on the
     * EU disc the language is in the number) and the language: 1 = the US text,
     * else LANGUAGE (EU: 2 English, 3 French, 4 Italian, 5 German, 6 Spanish) */
    int text_file, language;
    int area;                           /* STSTATUS_areaFuncs.getArea(); its spot is area + 1 */
    int spot_sprite[47], spot_x[47], spot_y[47]; /* STSTATUS_data.spots, 1..46 (0 unused) */
    int ntowns;                         /* STSTATUS_towns 1..ntowns */
    int town_x[23], town_y[23], town_listed[23]; /* listed: the list for the progress shows it */
} Dw3MapViewData;

/* What the area-name window holds after the game's own STSTATUS_showMapArea ran on
 * the view's cursor. */
typedef struct Dw3MapViewName {
    int hovering, spot;                 /* the game's own hit test: screen->hovering, ->spot */
    int text_file, language;
    int win_visible, win_x, win_y, win_lines;
    int width;                          /* the widest line, px, as the game measures it */
    int len;                            /* font-code bytes in codes[] */
    unsigned char codes[96];
    int nchars;
    unsigned short chars[64];           /* the game's glyph map: Shift-JIS, 0x0A break, 0x8000 | icon */
    int in_table;                       /* the window's text is the table's string for `spot` */
} Dw3MapViewName;

/* 1 while the data and name calls are wanted (the option is on and widescreen is
 * on, or the screen is open). */
int dw3_port_map_view_wants(void);
void dw3_port_map_view_data(const Dw3MapViewData *d);
void dw3_port_map_view_name(const Dw3MapViewName *n);

/* Patch 0148 (phase 6): the field hotkey. The key (`map_view_field_open`, default Select)
 * opens the view straight from the walking field: the field is held (its tasks paused, the
 * clocks, the roll sequence and VRAM given back at the close), the game's own Map screen runs
 * in the field menu's slot and the view opens on it by itself. */
typedef struct Dw3FieldMapProbe {
    int menu_open, busy, acting;        /* FIELDSTG_state: why the field is not idle */
    int progress;                       /* GAME.progress (the field menu needs 4 or more) */
    int battle_steps;                   /* GAME.battleSteps: the encounter countdown */
    int player_x, player_y;             /* the player's actor position */
    int children, held, running;        /* field children; how many are paused; how many are in TASK_RUN */
} Dw3FieldMapProbe;

/* Every frame the field is in its main state (before Start is looked at). `held` and
 * `pressed` are the logical pad. Returns 1 when the session must start this frame: the key
 * went down, the option and widescreen are on, no other shoulder button is held and the
 * field is idle. A key that goes down and is refused logs why, once. */
int dw3_port_map_view_field_poll(unsigned held, unsigned pressed, const Dw3FieldMapProbe *p);
/* The patch paused the field and created the Map screen's root: snapshot (VRAM, clocks, roll
 * sequence) and log `dw3-map: field hotkey open`. */
/* Returns 0 (and logs why) when it cannot hold the field: nothing was changed. */
int dw3_port_map_view_field_begin(const Dw3FieldMapProbe *p);
/* The Map screen's root is gone (the view closed, or the screen did): give everything back
 * and log the return with the comparison against the open. */
void dw3_port_map_view_field_end(const Dw3FieldMapProbe *p);
/* 1 from `begin` to `end`. The turbo poll reads it: speed 1, R3 ignored. */
int dw3_port_map_view_field_active(void);
/* The view closed during a session: the patch kills the game's Map screen. 1 once. */
int dw3_port_map_view_field_closed(void);
/* The layer the Map screen and its text window draw into: the field's (SCREEN_LAYER does not
 * exist in the field) while a session runs, else `screen_layer`. */
int dw3_port_map_view_field_layer(int screen_layer);
/* A restart (P6.9) while a session runs: forget it. */
void dw3_port_map_view_field_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_MAP_VIEW_H */
