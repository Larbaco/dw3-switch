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

void dw3_port_audio_track_slot(int seq_access_num, int sep, int *slot_seq,
                               int *slot_sep) {
    int flat = seq_access_num * DW3_AUDIO_TRACKS_PER_SEQ + sep;

    *slot_seq = flat / DW3_AUDIO_TRACKS_PER_SEQ;
    *slot_sep = flat % DW3_AUDIO_TRACKS_PER_SEQ;
}
