/* P6.31 auto-save (off by default). Policy, file and load live in
 * port/autosave/autosave.c; the game side is small hooks (patches 0140, 0141,
 * 0142). The auto-save is its own card file, autosave.bin. Design, defaults and the open questions: docs/P6.31_AUTOSAVE.md. */
#ifndef DW3_PORT_AUTOSAVE_H
#define DW3_PORT_AUTOSAVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* The key of port_options.cfg behind the Options screen's Auto-save row
 * (DW3_PORT_OPT_AUTOSAVE in port_options.h). */
#define DW3_AUTOSAVE_KEY "autosave"

enum {
    DW3_AUTOSAVE_OFF = 0,  /* the original game: nothing is ever written */
    DW3_AUTOSAVE_AREA = 1, /* after each area change, to autosave.bin */
    DW3_AUTOSAVE_MODE_MAX = 1
};

/* Why the policy did not save. DW3_AUTOSAVE_GO is "save now". Order is part of
 * the log text (dw3_autosave_skip_name) and of the unit tests. */
typedef enum Dw3AutosaveVerdict {
    DW3_AUTOSAVE_GO = 0,
    DW3_AUTOSAVE_SKIP_OFF,        /* the option is OFF */
    DW3_AUTOSAVE_SKIP_NOT_FIELD,  /* not in a field map */
    DW3_AUTOSAVE_SKIP_NO_MENU,    /* the game would not open its field menu
                                     here (the story before progress 4) */
    DW3_AUTOSAVE_SKIP_BATTLE,     /* a battle is starting or running */
    DW3_AUTOSAVE_SKIP_BUSY,       /* event, talk, warp, menu, inn */
    DW3_AUTOSAVE_SKIP_TRANSITION, /* the area name banner or a fade holds the
                                     player (FieldState.frozen) */
    DW3_AUTOSAVE_SKIP_BASELINE,   /* first idle frame of a loaded or new game:
                                     the area is not "changed", only noted */
    DW3_AUTOSAVE_SKIP_SAME_AREA,  /* the area is the one already seen */
    DW3_AUTOSAVE_SKIP_NO_TARGET,  /* unused since the separate file (kept so the
                                     verdict numbers stay) */
    DW3_AUTOSAVE_SKIP_CARD,       /* no card file for the target, or the slot
                                     holds another game */
    DW3_AUTOSAVE_SKIP_COUNT
} Dw3AutosaveVerdict;

/* What the policy looks at. Plain integers, so the unit tests need no game. */
typedef struct Dw3AutosaveCtx {
    int option;          /* DW3_AUTOSAVE_* */
    int in_field;        /* a field map (mode 0x2xx / 0x3xx) is running */
    int menu_allowed;    /* GAME.progress >= 4: the field menu may open */
    int battle;          /* FieldState.battleStarting */
    int busy;            /* FieldState.busy or .acting or .menuOpen */
    int frozen;          /* FieldState.frozen: banner or fade */
    int mode;            /* the field mode now */
    int last_mode;       /* the mode seen at the last decision, -1 unknown */
    int has_target;      /* always 1 since the separate file */
} Dw3AutosaveCtx;

/* The decision. Transient reasons (battle, busy, transition, no menu) do not
 * consume the area change: asking again on a later frame can still save. */
Dw3AutosaveVerdict dw3_autosave_decide(const Dw3AutosaveCtx *ctx);
const char *dw3_autosave_skip_name(Dw3AutosaveVerdict v);
/* 1 when the verdict leaves the area change pending (ask again). */
int dw3_autosave_verdict_is_transient(Dw3AutosaveVerdict v);

/* --- Game hooks (patches 0140, 0141) ------------------------------------- */

/* The field is idle on `mode` (the first frame the field menu may open). The
 * flags are FieldState's. Returns 1 when the caller must now build a snapshot
 * and call dw3_autosave_commit. Cheap when the option is OFF. */
typedef struct Dw3AutosaveField {
    int mode;
    int progress;
    int battle_starting;
    int busy;
    int acting;
    int menu_open;
    int frozen;
} Dw3AutosaveField;
int dw3_autosave_field_idle(const Dw3AutosaveField *f);

/* A game was loaded (any slot, or the auto-save): the loaded area is the
 * baseline, loading is not an area change. */
void dw3_autosave_note_loaded(void);
/* The title menu started (new game or load follows, or a return to title):
 * no baseline; autosave.bin is looked at again for the Continue row. */
void dw3_autosave_title_started(void);
/* 1 when autosave.bin holds a save this build can continue (cached at the
 * title start and after each write). The title menu shows Load as a separate
 * row, and Continue loads the auto-save, only then. */
int dw3_autosave_file_present(void);
/* Continue: copy the auto-save's game bytes (`size` = GAME_SAVE_SIZE) to
 * dst. `region` = SAVE_REGION_US (1) or _EU (2) of this build. Checks the
 * file again (checksum, version); -1 = cannot be loaded, dst unchanged. Marks
 * the load for the cheat sidecar (autosave.cheats) and resets the baseline. */
int dw3_autosave_load(unsigned char *dst, int size, int region);

/* What the game side collects, from GAME (see patch 0140). `save` is the
 * game's save bytes as STGMCARD_fillSave leaves them (field position of the
 * current area, checksum and version done). */
enum { DW3_AUTOSAVE_SAVE_MAX = 0x26C4 };
typedef struct Dw3AutosaveSnapshot {
    const unsigned char *save;
    int save_size;
    int mode;               /* the field mode */
    unsigned char name[0x18];
    int money;
    unsigned char play_time[12]; /* PlayTime, raw */
    int partners[3];        /* GameState.partners[member].unlocked */
    short levels[3];
} Dw3AutosaveSnapshot;

/* Write the snapshot to autosave.bin (slot 0 of a card-format file, rename-
 * atomic; created on the first write, replaced on every one, so a new game
 * overwrites the previous auto-save). Rolls back if the write fails. 0 on
 * success, -1 when nothing was written. Never touches save0.bin / save1.bin. */
int dw3_autosave_commit(const Dw3AutosaveSnapshot *snap);

/* The Options screen's current mode (DW3_AUTOSAVE_*). */
int dw3_autosave_mode(void);

/* The area string index the game's save list shows for a field mode (the
 * decomp's STGMCARD_prevModes), or 0 for a mode that table does not list. */
int dw3_autosave_area_of_mode(int mode);
/* The place string index the list shows (STGMCARD_places[0]: a generic one). */
enum { DW3_AUTOSAVE_PLACE = 43 };

/* Test hooks: forget the state, or choose the mode without the registry. */
void dw3_autosave_test_reset(void);
void dw3_autosave_test_set_mode(int mode);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_AUTOSAVE_H */
