/* P6.17 music / SFX volume: the glue between the Port options and PSY-Z's
 * SPU mixer buses (vendor/psyz psyz/spu.h). Shared by the PC and Switch
 * hosts; docs/P6.17_VOLUME.md has the design. */
#include "port_audio.h"

#include <psyz/audio.h>
#include <psyz/log.h>
#include <psyz/spu.h>

#include <stdio.h>
#include <stdlib.h>

enum {
    BUS_SFX = 0,   /* SsUtKeyOn voices and non-music tracks (the default bus) */
    BUS_MUSIC = 1  /* exclusive (music) SEP tracks */
};

static int g_music_step = DW3_AUDIO_VOLUME_MAX;
static int g_sfx_step = DW3_AUDIO_VOLUME_MAX;

void dw3_port_audio_apply(int music_step, int sfx_step) {
    if (music_step < 0) {
        music_step = 0;
    }
    if (music_step > DW3_AUDIO_VOLUME_MAX) {
        music_step = DW3_AUDIO_VOLUME_MAX;
    }
    if (sfx_step < 0) {
        sfx_step = 0;
    }
    if (sfx_step > DW3_AUDIO_VOLUME_MAX) {
        sfx_step = DW3_AUDIO_VOLUME_MAX;
    }
    Psyz_SpuSetBusGain(BUS_MUSIC, dw3_port_audio_gain_q15(music_step));
    Psyz_SpuSetBusGain(BUS_SFX, dw3_port_audio_gain_q15(sfx_step));
    /* Streamed CD/XA audio (the movies) carries the soundtrack. */
    Psyz_SpuSetCdGain(dw3_port_audio_gain_q15(music_step));
    if (music_step != g_music_step || sfx_step != g_sfx_step) {
        INFOF("dw3-audio: music volume %d (gain 0x%04x), sfx volume %d "
              "(gain 0x%04x)", music_step, dw3_port_audio_gain_q15(music_step),
              sfx_step, dw3_port_audio_gain_q15(sfx_step));
    }
    g_music_step = music_step;
    g_sfx_step = sfx_step;
}

void dw3_port_audio_note_sequence(int seq_access_num, int sep, int music) {
    int slot_seq;
    int slot_sep;

    /* A track number past 15 (the win jingle is 30) is an entry of the next
     * SEQ row; its voices carry that pair, so route that pair. */
    dw3_port_audio_track_slot(seq_access_num, sep, &slot_seq, &slot_sep);
    Psyz_SndSetSeqBus((short)slot_seq, (short)slot_sep,
                      music ? BUS_MUSIC : BUS_SFX);
}

/* --- optional bus statistics (evidence for docs/P6.17_VOLUME.md) ---------
 * DW3_AUDIO_STATS=<N> logs the per-bus mix every N game frames and clears
 * the counters. rms is over both channels of the bus signal (16-bit scale);
 * "in" is before the bus gain, "out" after it. */
static int g_stats_every = -1;
static unsigned g_stats_frame;

static unsigned isqrt64(unsigned long long v) {
    unsigned long long r = 0;
    unsigned long long bit = 1ULL << 62;

    while (bit > v) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return (unsigned)r;
}

static void log_bus(const char *name, const PsyzSpuBusStats *st) {
    unsigned long long frames = st->frames ? st->frames : 1;

    INFOF("dw3-audio: bus %-5s in_rms=%u out_rms=%u in_peak=%d out_peak=%d "
          "voice_frames=%llu",
          name, isqrt64(st->in_energy / (2 * frames)),
          isqrt64(st->out_energy / (2 * frames)), st->in_peak, st->out_peak,
          st->active);
}

void dw3_port_audio_poll(void) {
    PsyzSpuBusStats st[PSYZ_SPU_NUM_BUSES + 1];
    const char *env;

    if (g_stats_every < 0) {
        env = getenv("DW3_AUDIO_STATS");
        g_stats_every = env != NULL ? atoi(env) : 0;
        if (g_stats_every > 0) {
            Psyz_AudioLock();
            Psyz_SpuEnableBusStats(1);
            Psyz_AudioUnlock();
            INFOF("dw3-audio: bus statistics every %d frames", g_stats_every);
        }
    }
    if (g_stats_every <= 0 || ++g_stats_frame % (unsigned)g_stats_every != 0) {
        return;
    }
    Psyz_AudioLock();
    Psyz_SpuGetBusStats(st);
    Psyz_SpuEnableBusStats(1); /* clears */
    Psyz_AudioUnlock();
    INFOF("dw3-audio: window ending at frame %u: output frames=%llu "
          "music_volume=%d sfx_volume=%d", g_stats_frame, st[0].frames,
          g_music_step, g_sfx_step);
    log_bus("sfx", &st[BUS_SFX]);
    log_bus("music", &st[BUS_MUSIC]);
    log_bus("cd", &st[PSYZ_SPU_BUS_CD]);
}
