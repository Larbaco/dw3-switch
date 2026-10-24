/* P6.17: the volume step -> Q15 gain curve (port/audio/volume.c). */
#include "port_audio.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

static int fail(const char *what, int step) {
    printf("FAIL: %s (step %d)\n", what, step);
    return 1;
}

int main(void) {
    /* gain = step^2 / 100 of 0x8000, rounded to nearest. */
    static const int want[11] = {0,     328,   1311,  2949,  5243, 8192,
                                 11796, 16056, 20972, 26542, 32768};
    int step;
    int prev = -1;
    long x;

    for (step = 0; step <= DW3_AUDIO_VOLUME_MAX; step++) {
        const int g = dw3_port_audio_gain_q15(step);

        printf("step %2d gain %5d (%.1f dB)\n", step, g,
               g ? 20.0 * log10(g / 32768.0) : -999.0);
        if (g != want[step]) {
            return fail("gain table", step);
        }
        if (g <= prev) {
            return fail("gain must grow with the step", step);
        }
        prev = g;
    }
    if (dw3_port_audio_gain_q15(-5) != 0 || dw3_port_audio_gain_q15(99) != 0x8000) {
        return fail("out-of-range steps clamp", 0);
    }
    /* Step 10 is the original mix: the mixer's (sample * gain) >> 15 is the
     * identity on every 16-bit value, so even a build that multiplies at
     * unity cannot change a sample. */
    for (x = -32768; x <= 32767; x++) {
        if (((x * dw3_port_audio_gain_q15(10)) >> 15) != x) {
            return fail("unity gain is not the identity", (int)x);
        }
    }
    /* Step 0 is silence for every sample. */
    for (x = -32768; x <= 32767; x++) {
        if (((x * dw3_port_audio_gain_q15(0)) >> 15) != 0) {
            return fail("step 0 is not silent", (int)x);
        }
    }
    /* Track numbers past one SEQ row fall into the next row, like libsnd's
     * table: (0, 30), the win jingle, is the entry (1, 14). */
    {
        static const int cases[][4] = {
            /* seq sep -> seq sep */
            {0, 0, 0, 0},   {0, 15, 0, 15}, {2, 0, 2, 0},  {3, 5, 3, 5},
            {0, 16, 1, 0},  {0, 25, 1, 9},  {0, 30, 1, 14}, {0, 31, 1, 15},
            {1, 3, 1, 3},   {1, 16, 2, 0},
        };
        size_t i;

        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            int seq = -1;
            int sep = -1;

            dw3_port_audio_track_slot(cases[i][0], cases[i][1], &seq, &sep);
            if (seq != cases[i][2] || sep != cases[i][3]) {
                printf("FAIL: track (%d, %d) -> (%d, %d), want (%d, %d)\n",
                       cases[i][0], cases[i][1], seq, sep, cases[i][2],
                       cases[i][3]);
                return 1;
            }
        }
    }
    puts("OK: audio volume curve");
    return 0;
}
