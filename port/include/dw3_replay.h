/*
 * P6.15 replay recording parser (platform independent, no I/O besides stdio).
 *
 * One recording drives the PC host (psyz_pad_host.c, DW3_REPLAY=<file>) and
 * the Switch replay build (DW3_NX_REPLAY, sdmc:/switch/dw3/replay.rec). Both
 * key it by the same game-frame counter, the PadChkVsync count.
 *
 * Text format, one directive per line, '#' starts a comment:
 *   pad  <frame> <hexmask>   from this frame on, hold this raw mask
 *   shot <frame> <name>      checkpoint (name is [A-Za-z0-9_-]+)
 *   end  <frame>             stop the run at this frame
 * Masks are the raw PS1 pad bits (1 = pressed): START 0x8, UP 0x10,
 * RIGHT 0x20, DOWN 0x40, LEFT 0x80, TRIANGLE 0x1000, CIRCLE 0x2000,
 * CROSS 0x4000, SQUARE 0x8000.
 *
 * Directives may appear in any order; each kind is applied in frame order
 * (the files are generated sorted, the parser sorts defensively).
 */
#ifndef DW3_REPLAY_H
#define DW3_REPLAY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DW3_REPLAY_NAME_MAX 48

typedef struct {
    unsigned frame;
    unsigned mask;
    char name[DW3_REPLAY_NAME_MAX];
} Dw3ReplayEv;

typedef struct {
    Dw3ReplayEv *pad;
    Dw3ReplayEv *shot;
    size_t npad, nshot, cpad, cshot;
    size_t pad_i, shot_i;
    unsigned end; /* 0 = no end directive */
    unsigned mask; /* current held mask */
} Dw3Replay;

/* Called once per checkpoint, in order. */
typedef void (*Dw3ReplayShotFn)(const char *name, unsigned frame, void *ctx);

/* Parse recording text (len bytes, need not be NUL-terminated). Returns 0 on
 * success, -1 on allocation failure. Malformed lines are skipped and counted
 * in *bad when non-NULL. */
int dw3_replay_parse(Dw3Replay *r, const char *text, size_t len, int *bad);

/* Read and parse a file. Returns 0, or -1 when it cannot be opened/read. */
int dw3_replay_load(Dw3Replay *r, const char *path, int *bad);

/* Advance to `frame`: fires pending checkpoints with frame <= `frame` through
 * `shot_fn` (may be NULL), updates the held mask. Returns 1 once the end
 * frame has been reached (the caller stops the run), else 0. *mask receives
 * the raw pad mask to publish for this frame. */
int dw3_replay_advance(Dw3Replay *r, unsigned frame, Dw3ReplayShotFn shot_fn,
                       void *ctx, unsigned *mask);

void dw3_replay_free(Dw3Replay *r);

#ifdef __cplusplus
}
#endif

#endif /* DW3_REPLAY_H */
