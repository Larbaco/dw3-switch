/* P4.3 / #188: the audio health counters and their log line
 * (port/audio/audio_health.c). Synthetic numbers only. */
#include "audio_health.h"

#include <stdio.h>
#include <string.h>

static int fail(const char *what) {
    printf("FAIL: %s\n", what);
    return 1;
}

static int has(const char *line, const char *want) {
    if (strstr(line, want) == NULL) {
        printf("missing \"%s\" in:\n  %s\n", want, line);
        return 0;
    }
    return 1;
}

int main(void) {
    Dw3AudioHealth h;
    Dw3AudioQueueWin q;
    char line[384];
    int i;

    /* A healthy minute: voices 0..9, all key-ons fine, queue 3..4 of 4. */
    dw3_audio_health_reset(&h);
    for (i = 0; i < 10; i++) {
        dw3_audio_health_note_voices(&h, (unsigned)i);
    }
    for (i = 0; i < 5; i++) {
        if (dw3_audio_health_note_keyon(&h, i) != 0) return fail("ok key-on warned");
    }
    memset(&q, 0, sizeof(q));
    q.samples = 4;
    q.depth_min = 3;
    q.depth_max = 4;
    q.depth_sum = 14; /* avg 3.50 */
    q.depth_cap = 4;
    q.underruns = 2;
    dw3_audio_health_format(&h, &q, 60, 0, line, sizeof(line));
    puts(line);
    if (!has(line, "dw3-audio: underruns 2 in last 60 s (total 2)") ||
        !has(line, "queue depth min/avg/max 3/3.50/4 of 4") ||
        !has(line, "voices in use min/avg/max 0/4.5/9 of 24") ||
        !has(line, "key-on 5 ok 0 failed (total 5 ok, 0 failed)")) {
        return fail("healthy window");
    }
    if (strstr(line, "WARNING") != NULL) return fail("healthy window warns");

    /* Second window: the window cleared, the total kept; one more underrun. */
    dw3_audio_health_note_voices(&h, 24);
    q.underruns = 3;
    dw3_audio_health_format(&h, &q, 60, 0, line, sizeof(line));
    puts(line);
    if (!has(line, "underruns 1 in last 60 s (total 3)") ||
        !has(line, "min/avg/max 24/24.0/24 of 24") ||
        !has(line, "all in use in 1 of 1 samples") ||
        !has(line, "key-on 0 ok 0 failed (total 5 ok, 0 failed)")) {
        return fail("second window");
    }

    /* Leak signature: key-ons fail. First failure and every 50th warn. */
    for (i = 1; i <= 120; i++) {
        int w = dw3_audio_health_note_keyon(&h, -1);

        if (w != (i == 1 || i == 50 || i == 100)) {
            printf("failure %d warn=%d\n", i, w);
            return fail("warning rate limit");
        }
    }
    dw3_audio_health_format(&h, NULL, 60, 0, line, sizeof(line));
    puts(line);
    if (!has(line, "underruns n/a in last 60 s (host mixer)") ||
        !has(line, "queue depth n/a") ||
        !has(line, "key-on 0 ok 120 failed (total 5 ok, 120 failed)") ||
        !has(line, "WARNING voices ran out (#188 leak signature)")) {
        return fail("leak window");
    }

    /* The at-exit line carries the run maxima, not the last window's. */
    dw3_audio_health_note_voices(&h, 2);
    dw3_audio_health_format(&h, &q, 17, 1, line, sizeof(line));
    puts(line);
    if (!has(line, "dw3-audio: exit: underruns 0 in last 17 s (total 3)") ||
        !has(line, "voices in use min/avg/max 2/2.0/24 of 24") ||
        !has(line, "all in use in 1 of 1 samples")) {
        return fail("exit totals");
    }
    if (!has(line, "(total 5 ok, 120 failed)")) return fail("exit line");

    /* Voice counts above 24 are clamped. */
    dw3_audio_health_reset(&h);
    dw3_audio_health_note_voices(&h, 99);
    dw3_audio_health_format(&h, NULL, 1, 0, line, sizeof(line));
    if (!has(line, "min/avg/max 24/24.0/24 of 24")) return fail("clamp");

    printf("OK: audio health counters\n");
    return 0;
}
