/* Movie producer/consumer counters. Empty TU when DW3_NX_DEBUG_MOVIE is off. */
#include "nx_movie_debug.h"

#if DW3_NX_DEBUG_MOVIE

static uint32_t s_handed;
static uint64_t s_fc_skipped;
static uint32_t s_restarts;
static uint32_t s_last_fc;
static int s_have_last;

void dw3_nx_movie_debug_note_handover(uint32_t fc) {
    s_handed++;
    if (s_have_last) {
        if (fc > s_last_fc + 1u) {
            s_fc_skipped += (uint64_t)(fc - s_last_fc - 1u);
        } else if (fc <= s_last_fc) {
            /* Frame counter went back: a new play, not a dropped frame. */
            s_restarts++;
        }
    }
    s_last_fc = fc;
    s_have_last = 1;
}

void dw3_nx_movie_debug_take_window(Dw3NxMovieDebugWindow *out) {
    if (out == 0) {
        return;
    }
    out->handed = s_handed;
    out->fc_skipped = s_fc_skipped;
    out->restarts = s_restarts;
    s_handed = 0;
    s_fc_skipped = 0;
    s_restarts = 0;
}

#endif /* DW3_NX_DEBUG_MOVIE */
