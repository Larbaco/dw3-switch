/* P6.35 walkthrough, phase 1 data layer (docs/P6.35_WALKTHROUGH.md).
 *
 * Pure and host-testable: the step table, the resolver that turns a snapshot
 * of the game state into the current step, and the log line. No UI. The game
 * state is read by the hook (walkthrough_hook.c); the resolver only sees a
 * DW3WtState with the flag check injected as a callback, so one code path
 * serves US and EU and the host tests need no game. Read-only: nothing here
 * writes game state. */
#ifndef PORT_WALKTHROUGH_H
#define PORT_WALKTHROUGH_H

#ifdef __cplusplus
extern "C" {
#endif

/* the game's own condition codes (include/engine/game_state.h), spelled out so
 * the table needs no game header */
#define WT_FLAG(group, id) (((group) << 8) | (id))
#define WT_PROGRESS(n) (0x6000 | (n))
#define WT_SPECIAL(id) (0x7000 | (id))
#define WT_ITEM(kind, id) (0x8000 | ((kind) << 9) | (id))
#define WT_CARD(id) (0x9200 | (id))

#define WT_PROGRESS_MAX 0x63 /* the largest bound of the game's PROGRESS_RANGES */

enum { WT_MAIN = 0, WT_SIDE = 1 };
enum { WT_REGION_ALL = 0, WT_REGION_US = 1, WT_REGION_EU = 2 };
enum { WT_CAT_NONE = 0, WT_CAT_QUEST, WT_CAT_RECRUIT, WT_CAT_ITEM, WT_CAT_CARD,
       WT_CAT_MINIGAME, WT_CAT_OTHER };
#define WT_AREA_NONE (-1) /* STSTATUS_getArea() value outside the map */
#define WT_MAX_CONDS 2

typedef struct DW3WtCond {
    unsigned short code;  /* the game's code, for FLAGS_00.checkCondition */
    unsigned short value;
} DW3WtCond;

typedef struct DW3WtStep {
    const char *id;       /* stable, never reused: "ch01.s03" */
    const char *chapter;  /* "ch01" */
    const char *area;     /* area id for the spoiler sub-area: "asuka" */
    int map_area;         /* STSTATUS_getArea() of the step's main place, or WT_AREA_NONE */
    int kind;             /* WT_MAIN or WT_SIDE */
    int category;         /* SIDE only */
    int region;           /* WT_REGION_* */
    int from_progress;    /* smallest GAME.progress where it can be current */
    int to_progress;      /* largest, or -1 for no upper bound */
    int order;            /* within the chapter */
    int nunlock;
    DW3WtCond unlock[WT_MAX_CONDS]; /* all must hold for the step to be offered */
    int ndone;
    DW3WtCond done[WT_MAX_CONDS];   /* all hold: the step is finished */
} DW3WtStep;

typedef int (*DW3WtCheckFn)(void *ctx, unsigned code, unsigned value);

typedef struct DW3WtState {
    int progress;         /* GAME.progress */
    int half;             /* STSTATUS_getGameHalf(): 0, 1 or -1 */
    int area;             /* STSTATUS_getArea() or WT_AREA_NONE */
    int region;           /* WT_REGION_US or WT_REGION_EU */
    DW3WtCheckFn check;   /* FLAGS_00.checkCondition; may be NULL (then conditions fail) */
    void *ctx;
} DW3WtState;

enum {
    WT_OK = 0,
    WT_BELOW,     /* progress below the table */
    WT_ABOVE,     /* a valid progress the table does not cover yet */
    WT_UNKNOWN,   /* negative or above WT_PROGRESS_MAX (the 0x8000 debug value) */
    WT_NO_MATCH   /* covered by the progress range, but no row applies (region, unlock) */
};

typedef struct DW3WtResult {
    int status;
    const DW3WtStep *step;  /* NULL unless WT_OK */
    int index;              /* index of the step in the table, -1 */
    const char *rule;       /* which rule decided, for the log */
} DW3WtResult;

/* Resolves with an explicit table (tests) or the built-in one. Only MAIN rows
 * decide the current step; SIDE rows never do. */
void dw3_wt_resolve_table(const DW3WtStep *table, int count, const DW3WtState *st,
                          DW3WtResult *out);
void dw3_wt_resolve(const DW3WtState *st, DW3WtResult *out);
const DW3WtStep *dw3_wt_steps(int *count);

/* "dw3-walkthrough: progress P half H area A -> chapter C step S (rule R)" */
int dw3_wt_format_line(char *buf, unsigned size, const DW3WtState *st, const DW3WtResult *r);

/* Game side (walkthrough_hook.c): reads GAME and FLAGS_00 once, resolves, logs
 * the line to stderr. `why` names the caller ("probe", "open"). Returns the status. */
int dw3_walkthrough_log_state(const char *why);
/* The state the walkthrough screen reads: GAME.progress, the field mode (-> half,
 * area) and FLAGS_00.checkCondition as the check. Read-only. Returns 1. */
int dw3_walkthrough_snapshot(DW3WtState *st, int *mode);
/* Debug path: one call per game frame from the mode task (DW3_CHEATS builds);
 * logs only when DW3_WALKTHROUGH_PROBE is set and (progress, mode) changed. */
void dw3_walkthrough_probe_tick(void);

/* STSTATUS_mapAreas copy (walkthrough_areas.c): the map area of a field mode,
 * as STSTATUS_getArea() returns it, or WT_AREA_NONE. */
int dw3_wt_area_of_mode(int mode);
int dw3_wt_half_of_mode(int mode);

#ifdef __cplusplus
}
#endif
#endif
