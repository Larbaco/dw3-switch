/* Shared declarations for the Switch PSY-Z native libnx backend.
 * P4.2 EGL + OpenGL ES present (nx_gpu.c); P4.3 audren + Psyz_Audio* (nx_audio.c). */
#ifndef DW3_SWITCH_PSYZ_NX_H
#define DW3_SWITCH_PSYZ_NX_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

/* Sketch version bump when the linked surface changes. */
int dw3_nx_backend_sketch_version(void);

/* --- P4.2 video (nx_gpu.c) --------------------------------------------- */

typedef struct Dw3NxGpuInfo {
    uint32_t width;
    uint32_t height;
    bool docked; /* AppletOperationMode_Console */
} Dw3NxGpuInfo;

/* Create EGL + GLES2 on nwindowGetDefault(); applies docked/handheld size. */
bool dw3_nx_gpu_init(void);
void dw3_nx_gpu_shutdown(void);

/* Recreate the surface if dock/undock changed. Call once per frame. */
void dw3_nx_gpu_update_mode(void);

void dw3_nx_gpu_get_info(Dw3NxGpuInfo *out);

/* Clear + present helpers used by the gpu_hello sample (and later PSY-Z). */
void dw3_nx_gpu_clear(float r, float g, float b, float a);
/* Swap buffers; EGL swap interval 1 ≈ 60 Hz vsync when the display is 60 Hz. */
bool dw3_nx_gpu_swap(void);

/* --- P4.3 audio (nx_audio.c) ------------------------------------------- */

typedef struct Dw3NxAudioStats {
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t buf_frames;
    uint32_t num_buffers;
    uint64_t buffers_queued;
    uint64_t underruns;
    bool running;
    bool paused;
} Dw3NxAudioStats;

/* audren thread @ 44.1 kHz pulling Psyz_SpuPullSamples (PSY-Z audio API). */
bool dw3_nx_audio_init(void);
void dw3_nx_audio_shutdown(void);
void dw3_nx_audio_get_stats(Dw3NxAudioStats *out);

/* PSY-Z platform symbols implemented here (see vendor/psyz audio.h). */
int Psyz_AudioInit(void);
void Psyz_AudioDestroy(void);
void Psyz_AudioLock(void);
void Psyz_AudioUnlock(void);
void Psyz_AudioPause(void);
void Psyz_AudioUnpause(void);

#endif /* DW3_SWITCH_PSYZ_NX_H */
