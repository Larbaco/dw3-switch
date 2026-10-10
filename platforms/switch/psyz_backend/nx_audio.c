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
#include <string.h>
#include <switch.h>

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
static Mutex s_mutex;
static volatile int s_stop;
static volatile int s_paused;
static volatile uint64_t s_underruns;
static volatile uint64_t s_buffers_queued;
static int s_audio_init;
static int s_thread_started;
static int s_audren_inited;
static int s_driver_inited;

void Psyz_AudioDestroy(void);

static void fill_wavebuf(AudioDriverWaveBuf *wb) {
    short *pcm = (short *)wb->data_raw;

    mutexLock(&s_mutex);
    if (s_paused) {
        memset(pcm, 0, s_buf_bytes);
    } else {
        Psyz_SpuPullSamples(pcm, BUF_FRAMES);
    }
    mutexUnlock(&s_mutex);

    armDCacheFlush(pcm, s_buf_bytes);
}

static void audio_thread(void *arg) {
    (void)arg;

    while (!s_stop) {
        int queued_this_pass = 0;

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
            fprintf(stderr, "nx_audio: audrvUpdate failed: 0x%x\n", rc);
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
    mutexInit(&s_mutex);
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
        mutexLock(&s_mutex);
    }
}

void Psyz_AudioUnlock(void) {
    if (s_audio_init) {
        mutexUnlock(&s_mutex);
    }
}

void Psyz_AudioPause(void) { s_paused = 1; }

void Psyz_AudioUnpause(void) { s_paused = 0; }

bool dw3_nx_audio_init(void) { return Psyz_AudioInit() == 0; }

void dw3_nx_audio_shutdown(void) { Psyz_AudioDestroy(); }

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
