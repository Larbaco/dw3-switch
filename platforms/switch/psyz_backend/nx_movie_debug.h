/* Movie producer/consumer counters for nxlink P3.7 debugging.
 * Enabled only when DW3_NX_DEBUG_MOVIE=1 (default ON for --nxlink CMake
 * builds). See docs/P3.7_MOVIE_PACING_DIAGNOSTICS.md. */
#ifndef DW3_NX_MOVIE_DEBUG_H
#define DW3_NX_MOVIE_DEBUG_H

#include <stdint.h>

#ifndef DW3_NX_DEBUG_MOVIE
#define DW3_NX_DEBUG_MOVIE 0
#endif

#if DW3_NX_DEBUG_MOVIE

/* Counters accumulated since the previous take_window (typically 60 VSyncs).
 * `fc_skipped` counts STR frames that the producer never published between two
 * handed-over frames: the consumer's frameCount jumps by more than one. A
 * restart (frameCount going backwards, e.g. the movie replayed) does not add
 * to it. `restarts` counts those backwards jumps. */
typedef struct Dw3NxMovieDebugWindow {
    uint32_t handed;     /* StGetNext hand-overs in this window */
    uint64_t fc_skipped; /* STR frames missing between hand-overs */
    uint32_t restarts;   /* frameCount went backwards */
} Dw3NxMovieDebugWindow;

/* Called by the StGetNext wrapper on every hand-over with the STR frameCount
 * of the frame just taken. */
void dw3_nx_movie_debug_note_handover(uint32_t fc);

/* Copy window totals into *out and zero the accumulators. */
void dw3_nx_movie_debug_take_window(Dw3NxMovieDebugWindow *out);

#else /* !DW3_NX_DEBUG_MOVIE */

#define dw3_nx_movie_debug_note_handover(fc) ((void)0)

#endif /* DW3_NX_DEBUG_MOVIE */

#endif /* DW3_NX_MOVIE_DEBUG_H */
