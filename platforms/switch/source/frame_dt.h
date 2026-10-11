/* 60-pump spacing for the Switch vsync log. No libnx types: boot_main.c
 * feeds it a microsecond timestamp, and the host test compiles this header
 * with gcc. */
#ifndef DW3_FRAME_DT_H
#define DW3_FRAME_DT_H

#include <stdint.h>

/* Samples are 60 vsync pumps apart. */
#define DW3_FRAME_DT_PUMPS 60u

typedef struct Dw3FrameDt {
    uint64_t prev_us;
    int have_prev; /* zero-init. The first sample only stores prev_us. */
} Dw3FrameDt;

/* Returns 1 and writes *dt_us when pumps is a positive multiple of 60 and a
 * previous sample exists. *dt_us is now_us minus that previous sample.
 * Returns 0 for every other pump, and for the first sample. */
static inline int dw3_frame_dt_sample(Dw3FrameDt *st, unsigned pumps,
                                      uint64_t now_us, uint64_t *dt_us) {
    if (pumps == 0 || (pumps % DW3_FRAME_DT_PUMPS) != 0) {
        return 0;
    }
    if (!st->have_prev) {
        st->prev_us = now_us;
        st->have_prev = 1;
        return 0;
    }
    *dt_us = now_us - st->prev_us;
    st->prev_us = now_us;
    return 1;
}

#endif /* DW3_FRAME_DT_H */
