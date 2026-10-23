/* P6.17: the volume step -> Q15 gain curve (port/audio/volume.c). */
#include "port_audio.h"

#include <math.h>
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
    puts("OK: audio volume curve");
    return 0;
}
