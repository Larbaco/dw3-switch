/* P6.17: volume step -> Q15 gain. No dependencies, so tools/tests builds it
 * with a plain host compiler. */
#include "port_audio.h"

int dw3_port_audio_gain_q15(int step) {
    if (step <= 0) {
        return 0;
    }
    if (step >= DW3_AUDIO_VOLUME_MAX) {
        return 0x8000;
    }
    /* step^2 / 100 of 0x8000, rounded to nearest */
    return (step * step * 0x8000 + 50) / 100;
}
