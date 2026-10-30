/* P6.32 phase 6: the game-state half of the field hotkey (field_map.c). Private to the map
 * view; the game side is port_map_view.h. */
#ifndef DW3_FIELD_MAP_H
#define DW3_FIELD_MAP_H

#ifdef __cplusplus
extern "C" {
#endif

/* The clocks and counters a held field must not advance, for the open / return lines. */
typedef struct Dw3FieldMapClocks {
    int frame, time;                     /* GFX.frameCount, GFX.time */
    int play_frames, play_h, play_m, play_s; /* GAME.playTime */
    int rand_index;                      /* RANDOM.index: the roll sequence */
    int progress, battle_steps;          /* GAME.progress; steps to the next random battle */
} Dw3FieldMapClocks;

/* What the hold ran on while it lasted (and was put back). */
typedef struct Dw3FieldMapGiven {
    int time_ran;       /* GFX.timeCounter ticks the vsync callback added (8.8 vsyncs) */
    int frames_ran;     /* frames drawFrame counted */
    int rolls;          /* RANDOM.next calls of the main loop */
    int vram_bytes;     /* VRAM given back */
} Dw3FieldMapGiven;

/* Keeps the VRAM the Map screen overwrites, the clocks, the roll sequence and the field layer's
 * scroll (set to 0 for the Map screen). 1 on success; 0 = out of memory (nothing is held). */
int dw3_field_map_snapshot(void);
/* Puts them all back (no-op when nothing was kept). */
void dw3_field_map_restore(Dw3FieldMapGiven *given);
void dw3_field_map_clocks(Dw3FieldMapClocks *c);

#ifdef __cplusplus
}
#endif

#endif /* DW3_FIELD_MAP_H */
