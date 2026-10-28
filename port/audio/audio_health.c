/* P4.3 / #188: audio health counters. See audio_health.h. */
#include "audio_health.h"

#include <stdio.h>
#include <string.h>

void dw3_audio_health_reset(Dw3AudioHealth *h) { memset(h, 0, sizeof(*h)); }

void dw3_audio_health_note_voices(Dw3AudioHealth *h, unsigned in_use) {
    if (in_use > DW3_AUDIO_VOICES) {
        in_use = DW3_AUDIO_VOICES;
    }
    if (h->v_samples == 0 || in_use < h->v_min) {
        h->v_min = in_use;
    }
    if (in_use > h->v_max) {
        h->v_max = in_use;
    }
    if (in_use > h->v_max_total) {
        h->v_max_total = in_use;
    }
    if (in_use == DW3_AUDIO_VOICES) {
        h->v_full++;
        h->v_full_total++;
    }
    h->v_sum += in_use;
    h->v_samples++;
}

int dw3_audio_health_note_keyon(Dw3AudioHealth *h, int voice) {
    if (voice >= 0) {
        h->ko_ok++;
        h->ko_ok_total++;
        return 0;
    }
    h->ko_fail++;
    h->ko_fail_total++;
    h->fail_warned++;
    return h->fail_warned == 1 || h->fail_warned % 50 == 0;
}

int dw3_audio_health_format(Dw3AudioHealth *h, const Dw3AudioQueueWin *q,
                            unsigned secs, int final, char *buf, size_t size) {
    char qpart[96];
    char upart[80];
    unsigned vmin = h->v_samples ? h->v_min : 0;
    unsigned vmax = final ? h->v_max_total : h->v_max;
    unsigned vfull = final ? h->v_full_total : h->v_full;
    unsigned long long vavg10 =
        h->v_samples ? (h->v_sum * 10ull) / h->v_samples : 0;
    int n;

    if (q != NULL) {
        unsigned long long avg100 =
            q->samples ? (q->depth_sum * 100ull) / q->samples : 0;

        snprintf(upart, sizeof(upart), "underruns %llu in last %u s (total %llu)",
                 q->underruns - h->u_start, secs, q->underruns);
        snprintf(qpart, sizeof(qpart),
                 "queue depth min/avg/max %u/%llu.%02llu/%u of %u",
                 q->samples ? q->depth_min : 0, avg100 / 100, avg100 % 100,
                 q->depth_max, q->depth_cap);
        h->u_start = q->underruns;
    } else {
        snprintf(upart, sizeof(upart),
                 "underruns n/a in last %u s (host mixer)", secs);
        snprintf(qpart, sizeof(qpart), "queue depth n/a");
    }
    n = snprintf(buf, size,
                 "dw3-audio: %s%s, %s, voices in use min/avg/max %u/%llu.%llu/"
                 "%u of %u (all in use in %u of %u samples), key-on %u ok %u "
                 "failed (total %llu ok, %llu failed)%s",
                 final ? "exit: " : "", upart, qpart, vmin, vavg10 / 10,
                 vavg10 % 10, vmax, DW3_AUDIO_VOICES, vfull, h->v_samples,
                 h->ko_ok, h->ko_fail, h->ko_ok_total, h->ko_fail_total,
                 h->ko_fail ? "; WARNING voices ran out (#188 leak signature)"
                            : "");
    h->v_samples = h->v_min = h->v_max = h->v_full = 0;
    h->v_sum = 0;
    h->ko_ok = h->ko_fail = 0;
    h->reports++;
    return n;
}
