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
/* Scissor-clear one rectangle. Origin is the bottom-left of the surface. */
void dw3_nx_gpu_fill_rect(int x, int y, int w, int h, float r, float g, float b,
                          float a);
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

/* --- P4.5 log / time / threads (nx_log.c, nx_os.c) --------------------- */

/* Open sdmc:/switch/dw3/dw3.log. Call once the SD card is mounted; until then
 * psyz_log writes to stderr only. The file is what makes a smoke run
 * diagnosable, because Ryubing loads the .nro directly and there is no nxlink
 * to receive guest stderr (see docs/SWITCH_BACKEND.md). */
void dw3_nx_log_open(void);

/* --- P4.4 input (nx_input.c) ------------------------------------------- */

/* padConfigureInput + padInitializeDefault. Call once at boot. */
void dw3_nx_input_init(void);

/* --- P4.5 filesystem (nx_file.c) --------------------------------------- */

/* fsdevMountSdmc() + create sdmc:/switch/dw3. Call before anything opens a
 * runtime path (and before dw3_nx_log_open, which writes under it). */
bool dw3_nx_fs_mount_sd(void);

/* --- P4.7 renderer (switch_gpu.c) -------------------------------------- */

/* Which part of VRAM the game asked to display, filled from PSY-Z's
 * Draw_Display* handlers (PutDispEnv / SetDispMask / SetVideoMode). */
typedef struct Dw3NxDisplay {
    int on;    /* Draw_DisplayEnable */
    int x;     /* top-left in VRAM coordinates */
    int y;
    int width; /* on-screen size in pixels */
    int height;
    /* PSY-Z display ranges (not the VRAM blit). Horizontal is in dots,
     * vertical in scanlines. Present uses these for aspect, the same way
     * the PC host does: a 480-line 24-bit frame stays 4:3. */
    int range_w;
    int range_h;
    int pal; /* DisplayMode.pal: 0 = NTSC vref 240, 1 = PAL vref 288 */
} Dw3NxDisplay;

void dw3_nx_switch_gpu_display(Dw3NxDisplay *out);

/* The 1024x512 RGB555 VRAM the rasterizer draws into. Behind an accessor so
 * nx_gpu.c does not have to include PSY-Z's private rasterizer header. */
const uint16_t *dw3_nx_switch_gpu_vram(void);

/* Upload a VRAM region and draw it, letterboxed onto the EGL surface.
 * src_aspect is PSY-Z's display-range aspect (4:3 for this title). The
 * dw/dh arguments are only the sampled rectangle. */
void dw3_nx_gpu_present(const uint16_t *vram, int dx, int dy, int dw, int dh,
                        float src_aspect);

/* PSY-Z platform symbols (see vendor/psyz src/psyz/os.h, include/psyz/input.h,
 * include/psyz/video.h and include/psyz/log.h for the contracts). */
void psyz_log(unsigned int level, const char *file, unsigned int line,
              const char *func, const char *fmt, ...);
uint32_t Psyz_OsNowUs(void);
uintptr_t Psyz_OsThreadSelf(void);
void *Psyz_OsLockCreate(void);
void Psyz_OsLock(void *lock);
void Psyz_OsUnlock(void *lock);
void *Psyz_OsBellCreate(void);
void Psyz_OsBellRing(void *bell);
void Psyz_OsBellWait(void *bell, uint32_t timeout_us);
int Psyz_OsThreadStart(void (*fn)(void), const char *name);
void MyPadInit(int mode);
void Psyz_PadsPoll(void);
int Psyz_VideoVSync(int mode);
int Psyz_QuitRequested(void);

#endif /* DW3_SWITCH_PSYZ_NX_H */
