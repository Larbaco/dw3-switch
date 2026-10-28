/* P4.3 / #188: audio health counters (underruns, queue depth, voices in use,
 * key-on failures) and the log line that reports them. Pure arithmetic, no
 * host calls, so tools/tests/audio_health_main.c can test it. The glue that
 * feeds it (SPU voice polling, the Switch audren queue) is in audio_bus.c and
 * platforms/switch/psyz_backend/nx_audio.c. Design: docs/P4.3_AUDIO.md. */
#ifndef DW3_AUDIO_HEALTH_H
#define DW3_AUDIO_HEALTH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DW3_AUDIO_VOICES 24 /* PSYZ_SPU_NUM_VOICES, the PS1 voice count */

/* Output queue statistics of one window, from a host that has a queue (the
 * Switch audren thread). A host without one passes NULL and the line says
 * n/a. */
typedef struct Dw3AudioQueueWin {
    unsigned samples;              /* passes counted */
    unsigned depth_min, depth_max; /* buffers queued or playing */
    unsigned long long depth_sum;
    unsigned depth_cap;            /* buffers the host owns */
    unsigned long long underruns;  /* running total since init */
} Dw3AudioQueueWin;

typedef struct Dw3AudioHealth {
    /* window (cleared by each report) */
    unsigned v_samples;
    unsigned v_min, v_max, v_full; /* voices in use; samples with all in use */
    unsigned long long v_sum;
    unsigned ko_ok, ko_fail;
    unsigned long long u_start; /* underrun total at the window start */
    /* whole run */
    unsigned long long ko_ok_total, ko_fail_total;
    unsigned v_max_total, v_full_total;
    unsigned reports;
    unsigned fail_warned;
} Dw3AudioHealth;

void dw3_audio_health_reset(Dw3AudioHealth *h);

/* One sample of how many of the 24 voices have a non-zero envelope. */
void dw3_audio_health_note_voices(Dw3AudioHealth *h, unsigned in_use);

/* One SsUtKeyOn result (a voice number, or -1 for "no voice"). Returns 1 for
 * a failure that should log a warning now (the first, then every 50th), so a
 * leak that fails every sound cannot flood the log. */
int dw3_audio_health_note_keyon(Dw3AudioHealth *h, int voice);

/* Write the report line for the window ending now and clear the window.
 * `secs` is the window length; `final` marks the at-exit line (the whole run
 * for the key-on and voice-full totals). `q` may be NULL. Returns the length
 * written. */
int dw3_audio_health_format(Dw3AudioHealth *h, const Dw3AudioQueueWin *q,
                            unsigned secs, int final, char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* DW3_AUDIO_HEALTH_H */
