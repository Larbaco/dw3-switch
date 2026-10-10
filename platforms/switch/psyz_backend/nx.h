/* Shared declarations for the Switch PSY-Z native libnx backend.
 * P4.2 wires EGL + OpenGL ES present; other units remain stubs. */
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

#endif /* DW3_SWITCH_PSYZ_NX_H */
