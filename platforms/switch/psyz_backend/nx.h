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

/* Create EGL + GLES3 (mesh) with GLES2 fallback on nwindowGetDefault(). */
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

/* Raw hid buttons from the last Psyz_PadsPoll / padUpdate (0 before first). */
uint64_t dw3_nx_input_buttons_raw(void);

/* hid style set of the last padUpdate (padGetStyleSet; 0 before the first
 * poll). nx_rumble.c keys its vibration route off it: handheld rails /
 * Lite (NpadHandheld), docked Pro (NpadFullKey), docked pair (NpadJoyDual). */
uint32_t dw3_nx_input_style_set(void);

/* --- P10.4 rumble (nx_rumble.c) ----------------------------------------- */

/* Platform hook behind __wrap_PadSetAct (weak idle default lives in
 * psyz_pad_host.c): forwards the 2-byte DualShock actuator amplitudes —
 * PAD.act[0] small motor, [1] big motor — to hidSendVibrationValue. */
void Psyz_PadsSetAct(int port, const unsigned char *act, int len);

/* Boot-only HD-rumble pulse; disabled unless CMake -DDW3_NX_RUMBLE_SELFTEST. */
void dw3_nx_rumble_boot_selftest(void);

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
    /* P6.1 side gutter per side in pixels (0 = off); present adds the planes
     * of dw3_nx_switch_gpu_gutter_plane left and right of the display. */
    int gutter;
} Dw3NxDisplay;

void dw3_nx_switch_gpu_display(Dw3NxDisplay *out);

/* The 1024x512 RGB555 VRAM the rasterizer draws into. Behind an accessor so
 * nx_gpu.c does not have to include PSY-Z's private rasterizer header. */
const uint16_t *dw3_nx_switch_gpu_vram(void);

/* RGB555 side gutters, side 0 = left: row y is the gutter of VRAM row y,
 * DW3_NX_GUTTER_STRIDE pixels per row (PSYZ_GUTTER_MAX). */
#define DW3_NX_GUTTER_STRIDE 64
const uint16_t *dw3_nx_switch_gpu_gutter_plane(int side);

/* Upload a VRAM region and draw it, letterboxed onto the EGL surface.
 * src_aspect is PSY-Z's display-range aspect (4:3 for this title). The
 * dw/dh arguments are only the sampled rectangle. A non-zero gutter draws
 * the side gutter planes left and right of it, the aspect widened to match. */
void dw3_nx_gpu_present(const uint16_t *vram, int dx, int dy, int dw, int dh,
                        float src_aspect, int gutter);

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

/* Set by the Switch boot host when appletMainLoop reports exit (HOME). */
void dw3_nx_set_quit_requested(void);

#endif /* DW3_SWITCH_PSYZ_NX_H */
