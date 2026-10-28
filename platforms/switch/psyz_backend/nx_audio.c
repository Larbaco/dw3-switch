/* P4.3 — audren output thread fed by PSY-Z SPU mixer @ 44.1 kHz.
 *
 * Mirrors vendor/psyz/psyz/src/psp/psp_audio.c: a dedicated thread pulls
 * stereo s16 frames via Psyz_SpuPullSamples and pushes them to the host
 * device. Software SPU stays in psyz_spu.c when linked; until then weak
 * stubs supply a 440 Hz tone so audio_hello can exercise the path.
 */
#include "nx.h"

#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#ifdef DW3_NX_GAME_IMAGE
#include <psyz/spu.h>

#include "../../../port/audio/audio_health.h"
void dw3_port_audio_health_exit(void);
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define N_CHANNELS 2
#define SAMPLE_RATE 44100 /* PSYZ_SPU_SAMPLE_RATE */
#define BUF_FRAMES 1024   /* ~23 ms @ 44.1 kHz */
#define NUM_BUFFERS 4
#define VOICE_ID 0

/* --- Weak SPU stand-ins (overridden when full PSY-Z is linked) -------- */

__attribute__((weak)) void Psyz_SpuInit(void) {
    /* no-op stub */
}

__attribute__((weak)) void Psyz_SpuPullSamples(short *out, int num_frames) {
    /* 440 Hz stereo tone @ ~30% amplitude for bring-up without PSY-Z. */
    static double phase;
    const double step = 2.0 * M_PI * 440.0 / (double)SAMPLE_RATE;
    for (int i = 0; i < num_frames; i++) {
        short s = (short)(0.3 * 32767.0 * sin(phase));
        phase += step;
        if (phase >= 2.0 * M_PI) {
            phase -= 2.0 * M_PI;
        }
        out[i * 2 + 0] = s;
        out[i * 2 + 1] = s;
    }
}

/* --- audren driver state --------------------------------------------- */

static const AudioRendererConfig s_ar_config = {
    .output_rate = AudioRendererOutputRate_48kHz, /* bus; voice is 44.1 */
    .num_voices = 24,
    .num_effects = 0,
    .num_sinks = 1,
    .num_mix_objs = 1,
    .num_mix_buffers = 2,
};

static AudioDriver s_drv;
static AudioDriverWaveBuf s_wavebufs[NUM_BUFFERS];
static void *s_mempool;
static size_t s_mempool_size;
static size_t s_buf_bytes;

static Thread s_thread;
/* Recursive, and it has to be. PSY-Z's audio lock is re-entrant by contract,
 * not by accident: the mixer pulls CD samples from inside the SPU tick
 * (psyz_spu.c spu_tick -> Psyz_CdPullSamples) and Psyz_CdPullSamples takes the
 * audio lock (libcd.c). So the audio thread re-enters this lock from inside it
 * on every buffer fill. Both other PSY-Z backends are recursive — SDL mutexes
 * are documented recursive, and the PSP implements recursion by hand with
 * lock_owner/lock_depth (psp_audio.c) — while libnx's plain Mutex is not, and
 * blocks on the re-entry without ever comparing the owner. A plain Mutex here
 * wedged the whole port: the audio thread deadlocked against itself, and the
 * game thread then blocked on the same lock in its first libcd call
 * (CdInit -> CD_cw(CdlDemute) -> psyz_demute -> Psyz_AudioLock), which is
 * exactly where the boot stopped. */
static RMutex s_mutex;
static volatile int s_stop;
static volatile int s_paused;
static volatile uint64_t s_underruns;
static volatile uint64_t s_buffers_queued;
static int s_audio_init;
static int s_thread_started;
static int s_audren_inited;
static int s_driver_inited;

void Psyz_AudioDestroy(void);

#ifdef DW3_NX_GAME_IMAGE
/* --- audio health (#188: "audio dies after many techs") -------------------
 * One `dw3-audio: health` line every HEALTH_BUFFERS buffers (~30 s) and a
 * warning when the final output is silent for a long stretch while a music
 * track was requested and the music volume is above 0. Measured on the very
 * buffers audren plays, so a dead mixer, a dead sequence and a muted bus are
 * told apart by the voice counts next to the peak. Counted here, read and
 * logged by the audio thread. */
#define HEALTH_BUFFERS ((30 * SAMPLE_RATE) / BUF_FRAMES)
#define SILENT_PEAK 4
#define SILENT_WARN_SEC 10
static int s_win_peak;
static unsigned s_win_buffers, s_win_audible;
static unsigned long long s_win_energy, s_win_samples;
static unsigned s_silent_run;      /* consecutive silent, unpaused buffers */
static unsigned s_silent_next_warn;/* next run length (buffers) to warn at */
static unsigned s_silent_reported; /* run length of the last warning */
static volatile unsigned s_music_requests;
static volatile int s_music_seq = -1, s_music_sep = -1;
static volatile unsigned long long s_music_at; /* buffers_queued at the request */
static volatile int s_music_step = 10;
static int s_bus_stats_on;

/* --- queue depth window for the `dw3-audio: underruns N in last 60 s` line
 * (port/audio/audio_bus.c reads it through dw3_host_audio_queue_window).
 * Written by the audio thread once per pass, read and cleared by the game
 * thread once a minute; one small mutex, not the audio lock. */
static Mutex s_qw_mutex;
static Dw3AudioQueueWin s_qw;

static void queue_note(unsigned depth) {
    mutexLock(&s_qw_mutex);
    if (s_qw.samples == 0 || depth < s_qw.depth_min) s_qw.depth_min = depth;
    if (depth > s_qw.depth_max) s_qw.depth_max = depth;
    s_qw.depth_sum += depth;
    s_qw.samples++;
    s_qw.depth_cap = NUM_BUFFERS;
    s_qw.underruns = s_underruns;
    mutexUnlock(&s_qw_mutex);
}

void dw3_host_audio_queue_window(Dw3AudioQueueWin *out) {
    mutexLock(&s_qw_mutex);
    s_qw.underruns = s_underruns;
    s_qw.depth_cap = NUM_BUFFERS;
    *out = s_qw;
    s_qw.samples = s_qw.depth_min = s_qw.depth_max = 0;
    s_qw.depth_sum = 0;
    mutexUnlock(&s_qw_mutex);
}

static unsigned buffers_to_sec(unsigned long long b) {
    return (unsigned)(b * BUF_FRAMES / SAMPLE_RATE);
}

/* Hooks called by port/audio/audio_bus.c (weak there; PC has none). */
void dw3_host_audio_note_music(int seq, int sep) {
    s_music_seq = seq;
    s_music_sep = sep;
    s_music_at = s_buffers_queued;
    s_music_requests++;
}

void dw3_host_audio_note_volume(int music_step) { s_music_step = music_step; }

static unsigned isqrt_ull(unsigned long long v) {
    unsigned long long r = 0, bit = 1ULL << 62;

    while (bit > v) bit >>= 2;
    while (bit) {
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

/* Audio thread, no lock held. */
static void health_report(void) {
    PsyzSpuBusStats st[PSYZ_SPU_NUM_BUSES + 1];
    unsigned long long fr = 1, vf = 0;
    int bus_peak[3] = {0, 0, 0};
    int i;

    memset(st, 0, sizeof(st));
    if (s_bus_stats_on) {
        rmutexLock(&s_mutex);
        Psyz_SpuGetBusStats(st);
        Psyz_SpuEnableBusStats(1); /* clears the counters */
        rmutexUnlock(&s_mutex);
        if (st[0].frames) fr = st[0].frames;
        for (i = 0; i < PSYZ_SPU_NUM_BUSES; i++) vf += st[i].active;
        bus_peak[0] = st[0].out_peak;
        bus_peak[1] = st[1].out_peak;
        bus_peak[2] = st[PSYZ_SPU_BUS_CD].out_peak;
    }
    fprintf(stderr,
            "dw3-audio: health %us: output peak=%d rms=%u audible=%u/%u "
            "buffers, avg active voices=%.1f, bus peaks sfx=%d music=%d cd=%d, "
            "underruns=%llu, music requests=%u (last seq %d sep %d, %us ago), "
            "music volume=%d, silent run now %us, %s\n",
            buffers_to_sec(s_win_buffers), s_win_peak,
            s_win_samples ? isqrt_ull(s_win_energy / s_win_samples) : 0u,
            s_win_audible, s_win_buffers, (double)vf / (double)fr, bus_peak[0],
            bus_peak[1], bus_peak[2], (unsigned long long)s_underruns,
            s_music_requests, s_music_seq, s_music_sep,
            s_music_requests ? buffers_to_sec(s_buffers_queued - s_music_at) : 0u,
            s_music_step, buffers_to_sec(s_silent_run),
            s_paused ? "paused" : "running");
    s_win_peak = 0;
    s_win_buffers = s_win_audible = 0;
    s_win_energy = s_win_samples = 0;
}

static void health_note_buffer(const short *pcm, int paused) {
    int peak = 0;
    int i;

    if (paused) {
        return; /* HOME/sleep: silence is expected, not counted */
    }
    for (i = 0; i < BUF_FRAMES * N_CHANNELS; i++) {
        int v = pcm[i] < 0 ? -pcm[i] : pcm[i];

        if (v > peak) peak = v;
        s_win_energy += (unsigned long long)v * (unsigned long long)v;
    }
    s_win_samples += BUF_FRAMES * N_CHANNELS;
    if (peak > s_win_peak) s_win_peak = peak;
    s_win_buffers++;
    if (peak > SILENT_PEAK) {
        s_win_audible++;
        if (s_silent_reported) {
            fprintf(stderr, "dw3-audio: output audible again after %us of "
                    "silence\n", buffers_to_sec(s_silent_run));
        }
        s_silent_run = s_silent_reported = 0;
        s_silent_next_warn = (SILENT_WARN_SEC * SAMPLE_RATE) / BUF_FRAMES;
    } else {
        s_silent_run++;
        if (s_silent_next_warn == 0) {
            s_silent_next_warn = (SILENT_WARN_SEC * SAMPLE_RATE) / BUF_FRAMES;
        }
        if (s_silent_run >= s_silent_next_warn && s_music_requests &&
            s_music_step > 0) {
            fprintf(stderr,
                    "dw3-audio: WARNING output silent for %us while music is "
                    "expected (last music request seq %d sep %d %us ago, music "
                    "volume %d, %u requests so far)\n",
                    buffers_to_sec(s_silent_run), s_music_seq, s_music_sep,
                    buffers_to_sec(s_buffers_queued - s_music_at), s_music_step,
                    s_music_requests);
            s_silent_reported = s_silent_run;
            /* 10 s, then 40 s, 100 s and every 5 min after: no spam */
            s_silent_next_warn = s_silent_run < 40 * 43u
                                     ? 40 * 43u
                                     : s_silent_run < 100 * 43u
                                           ? 100 * 43u
                                           : s_silent_run + 300 * 43u;
        }
    }
}

#else
/* The hello sample has no PSY-Z headers: no health line there. */
static unsigned s_win_buffers;
static int s_bus_stats_on;
#define HEALTH_BUFFERS 0xFFFFFFFFu
static void health_report(void) {}
static void queue_note(unsigned depth) { (void)depth; }
static void health_note_buffer(const short *pcm, int paused) {
    (void)pcm;
    (void)paused;
}
#endif

static void fill_wavebuf(AudioDriverWaveBuf *wb) {
    short *pcm = (short *)wb->data_raw;

    rmutexLock(&s_mutex);
    if (s_paused) {
        memset(pcm, 0, s_buf_bytes);
    } else {
        Psyz_SpuPullSamples(pcm, BUF_FRAMES);
    }
    health_note_buffer(pcm, s_paused);
    rmutexUnlock(&s_mutex);

    armDCacheFlush(pcm, s_buf_bytes);
}

static void audio_thread(void *arg) {
    (void)arg;

    while (!s_stop) {
        int queued_this_pass = 0;
        unsigned depth = 0;

        for (int i = 0; i < NUM_BUFFERS; i++) {
            if (s_wavebufs[i].state != AudioDriverWaveBufState_Free &&
                s_wavebufs[i].state != AudioDriverWaveBufState_Done) {
                depth++;
            }
        }
        if (depth == 0 && !s_paused) {
            s_underruns++; /* every buffer played out: the output ran dry */
        }
        queue_note(depth);

        for (int i = 0; i < NUM_BUFFERS; i++) {
            AudioDriverWaveBuf *wb = &s_wavebufs[i];
            if (wb->state != AudioDriverWaveBufState_Free &&
                wb->state != AudioDriverWaveBufState_Done) {
                continue;
            }

            fill_wavebuf(wb);
            if (!audrvVoiceAddWaveBuf(&s_drv, VOICE_ID, wb)) {
                s_underruns++;
                continue;
            }
            queued_this_pass++;
            s_buffers_queued++;
        }

        Result rc = audrvUpdate(&s_drv);
        if (R_FAILED(rc)) {
            static unsigned s_update_fail;

            if (s_update_fail++ % 1000 == 0) { /* ~40 passes a second */
                fprintf(stderr, "nx_audio: audrvUpdate failed: 0x%x (%u so "
                        "far)\n", rc, s_update_fail);
            }
        }
        if (s_win_buffers >= HEALTH_BUFFERS) {
            health_report();
        }

        if (!audrvVoiceIsPlaying(&s_drv, VOICE_ID)) {
            audrvVoiceStart(&s_drv, VOICE_ID);
            if (queued_this_pass == 0) {
                s_underruns++;
            }
        }

        audrenWaitFrame();
    }
}

int Psyz_AudioInit(void) {
    if (s_audio_init) {
        return 0;
    }

    Psyz_SpuInit();
    rmutexInit(&s_mutex);
    s_stop = 0;
    s_paused = 0;
    s_underruns = 0;
    s_buffers_queued = 0;

    s_buf_bytes = (size_t)BUF_FRAMES * N_CHANNELS * sizeof(short);
    s_mempool_size = (NUM_BUFFERS * s_buf_bytes + 0xFFF) & ~((size_t)0xFFF);
    s_mempool = memalign(0x1000, s_mempool_size);
    if (!s_mempool) {
        fprintf(stderr, "nx_audio: memalign(%zu) failed\n", s_mempool_size);
        return -1;
    }
    memset(s_mempool, 0, s_mempool_size);

    Result rc = audrenInitialize(&s_ar_config);
    if (R_FAILED(rc)) {
        fprintf(stderr, "nx_audio: audrenInitialize failed: 0x%x\n", rc);
        free(s_mempool);
        s_mempool = NULL;
        return -1;
    }
    s_audren_inited = 1;

    rc = audrvCreate(&s_drv, &s_ar_config, N_CHANNELS);
    if (R_FAILED(rc)) {
        fprintf(stderr, "nx_audio: audrvCreate failed: 0x%x\n", rc);
        audrenExit();
        s_audren_inited = 0;
        free(s_mempool);
        s_mempool = NULL;
        return -1;
    }
    s_driver_inited = 1;

    int mpid = audrvMemPoolAdd(&s_drv, s_mempool, s_mempool_size);
    if (mpid < 0 || !audrvMemPoolAttach(&s_drv, mpid)) {
        fprintf(stderr, "nx_audio: mempool attach failed\n");
        Psyz_AudioDestroy();
        return -1;
    }

    static const u8 sink_channels[] = {0, 1};
    audrvDeviceSinkAdd(&s_drv, AUDREN_DEFAULT_DEVICE_NAME, 2, sink_channels);

    rc = audrvUpdate(&s_drv);
    if (R_FAILED(rc)) {
        fprintf(stderr, "nx_audio: audrvUpdate (init) failed: 0x%x\n", rc);
        Psyz_AudioDestroy();
        return -1;
    }

    rc = audrenStartAudioRenderer();
    if (R_FAILED(rc)) {
        fprintf(stderr, "nx_audio: audrenStartAudioRenderer failed: 0x%x\n",
                rc);
        Psyz_AudioDestroy();
        return -1;
    }

    if (!audrvVoiceInit(&s_drv, VOICE_ID, N_CHANNELS, PcmFormat_Int16,
                        SAMPLE_RATE)) {
        fprintf(stderr, "nx_audio: audrvVoiceInit failed\n");
        Psyz_AudioDestroy();
        return -1;
    }
    audrvVoiceSetDestinationMix(&s_drv, VOICE_ID, AUDREN_FINAL_MIX_ID);
    audrvVoiceSetMixFactor(&s_drv, VOICE_ID, 1.0f, 0, 0);
    audrvVoiceSetMixFactor(&s_drv, VOICE_ID, 1.0f, 1, 1);

    for (int i = 0; i < NUM_BUFFERS; i++) {
        memset(&s_wavebufs[i], 0, sizeof(s_wavebufs[i]));
        s_wavebufs[i].data_raw = (u8 *)s_mempool + (size_t)i * s_buf_bytes;
        s_wavebufs[i].size = s_buf_bytes;
        s_wavebufs[i].start_sample_offset = 0;
        s_wavebufs[i].end_sample_offset = BUF_FRAMES;
    }

    /* Prime buffers before starting the voice. */
    for (int i = 0; i < NUM_BUFFERS; i++) {
        fill_wavebuf(&s_wavebufs[i]);
        audrvVoiceAddWaveBuf(&s_drv, VOICE_ID, &s_wavebufs[i]);
        s_buffers_queued++;
    }
    audrvUpdate(&s_drv);
    audrvVoiceStart(&s_drv, VOICE_ID);

    rc = threadCreate(&s_thread, audio_thread, NULL, NULL, 0x10000, 0x20, -2);
    if (R_FAILED(rc)) {
        fprintf(stderr, "nx_audio: threadCreate failed: 0x%x\n", rc);
        Psyz_AudioDestroy();
        return -1;
    }
    rc = threadStart(&s_thread);
    if (R_FAILED(rc)) {
        fprintf(stderr, "nx_audio: threadStart failed: 0x%x\n", rc);
        threadClose(&s_thread);
        Psyz_AudioDestroy();
        return -1;
    }
    s_thread_started = 1;
    s_audio_init = 1;
#ifdef DW3_NX_GAME_IMAGE
    if (getenv("DW3_AUDIO_STATS") == NULL) {
        /* the health line reads the per-bus voice counts and peaks; the env
         * knob owns the same counters when it is set */
        rmutexLock(&s_mutex);
        Psyz_SpuEnableBusStats(1);
        rmutexUnlock(&s_mutex);
        s_bus_stats_on = 1;
    }
#endif
    fprintf(stderr,
            "nx_audio: Psyz_AudioInit OK (audren, %d Hz stereo, %d-frame "
            "x %d buffers)\n",
            SAMPLE_RATE, BUF_FRAMES, NUM_BUFFERS);
    return 0;
}

void Psyz_AudioDestroy(void) {
    if (s_thread_started) {
        s_stop = 1;
        threadWaitForExit(&s_thread);
        threadClose(&s_thread);
        s_thread_started = 0;
    }

    if (s_driver_inited) {
        audrvVoiceStop(&s_drv, VOICE_ID);
        audrvClose(&s_drv);
        s_driver_inited = 0;
    }
    if (s_audren_inited) {
        audrenExit();
        s_audren_inited = 0;
    }
    if (s_mempool) {
        free(s_mempool);
        s_mempool = NULL;
    }
    s_audio_init = 0;
    s_stop = 0;
    s_paused = 0;
}

void Psyz_AudioLock(void) {
    if (s_audio_init) {
        rmutexLock(&s_mutex);
    }
}

void Psyz_AudioUnlock(void) {
    if (s_audio_init) {
        rmutexUnlock(&s_mutex);
    }
}

void Psyz_AudioPause(void) { s_paused = 1; }

void Psyz_AudioUnpause(void) { s_paused = 0; }

bool dw3_nx_audio_init(void) { return Psyz_AudioInit() == 0; }

void dw3_nx_audio_shutdown(void) {
#ifdef DW3_NX_GAME_IMAGE
    dw3_port_audio_health_exit(); /* the run-total line, before the counters go */
#endif
    Psyz_AudioDestroy();
}

void dw3_nx_audio_get_stats(Dw3NxAudioStats *out) {
    if (!out) {
        return;
    }
    out->sample_rate = SAMPLE_RATE;
    out->channels = N_CHANNELS;
    out->buf_frames = BUF_FRAMES;
    out->num_buffers = NUM_BUFFERS;
    out->buffers_queued = s_buffers_queued;
    out->underruns = s_underruns;
    out->running = s_audio_init && !s_stop;
    out->paused = s_paused != 0;
}
