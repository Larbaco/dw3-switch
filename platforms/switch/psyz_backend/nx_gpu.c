/* P4.2 — native present via EGL + OpenGL ES 2 (mesa/nouveau on libnx).
 * Modelled on switchbrew/switch-examples graphics/opengl/es2gears.
 * PSY-Z InitPlatform / VRAM blit lands later; this owns the host surface. */
#include "nx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

int dw3_nx_backend_sketch_version(void) {
    return 3; /* P4.2 EGL present + P4.3 audren Psyz_Audio* */
}

static EGLDisplay s_display = EGL_NO_DISPLAY;
static EGLContext s_context = EGL_NO_CONTEXT;
static EGLSurface s_surface = EGL_NO_SURFACE;
static NWindow *s_win;
static uint32_t s_width;
static uint32_t s_height;
static bool s_docked;
static bool s_ready;

static void resolution_for_mode(bool docked, uint32_t *w, uint32_t *h) {
    if (docked) {
        *w = 1920;
        *h = 1080;
    } else {
        *w = 1280;
        *h = 720;
    }
}

static bool current_docked(void) {
    return appletGetOperationMode() == AppletOperationMode_Console;
}

static void deinit_egl(void) {
    if (s_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(s_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (s_context != EGL_NO_CONTEXT) {
            eglDestroyContext(s_display, s_context);
            s_context = EGL_NO_CONTEXT;
        }
        if (s_surface != EGL_NO_SURFACE) {
            eglDestroySurface(s_display, s_surface);
            s_surface = EGL_NO_SURFACE;
        }
        eglTerminate(s_display);
        s_display = EGL_NO_DISPLAY;
    }
    s_ready = false;
}

static bool init_egl(NWindow *win) {
    s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (s_display == EGL_NO_DISPLAY) {
        fprintf(stderr, "nx_gpu: eglGetDisplay failed: 0x%x\n", eglGetError());
        return false;
    }

    if (!eglInitialize(s_display, NULL, NULL)) {
        fprintf(stderr, "nx_gpu: eglInitialize failed: 0x%x\n", eglGetError());
        goto fail_display;
    }

    if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE) {
        fprintf(stderr, "nx_gpu: eglBindAPI failed: 0x%x\n", eglGetError());
        goto fail_display;
    }

    EGLConfig config;
    EGLint num_configs = 0;
    static const EGLint fb_attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_NONE,
    };
    eglChooseConfig(s_display, fb_attribs, &config, 1, &num_configs);
    if (num_configs == 0) {
        fprintf(stderr, "nx_gpu: eglChooseConfig failed: 0x%x\n", eglGetError());
        goto fail_display;
    }

    s_surface = eglCreateWindowSurface(s_display, config, win, NULL);
    if (s_surface == EGL_NO_SURFACE) {
        fprintf(stderr, "nx_gpu: eglCreateWindowSurface failed: 0x%x\n", eglGetError());
        goto fail_display;
    }

    static const EGLint ctx_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE,
    };
    s_context = eglCreateContext(s_display, config, EGL_NO_CONTEXT, ctx_attribs);
    if (s_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "nx_gpu: eglCreateContext failed: 0x%x\n", eglGetError());
        goto fail_surface;
    }

    if (!eglMakeCurrent(s_display, s_surface, s_surface, s_context)) {
        fprintf(stderr, "nx_gpu: eglMakeCurrent failed: 0x%x\n", eglGetError());
        goto fail_context;
    }

    /* Request vsync (1 = wait for refresh; ~60 Hz on Switch panels). */
    eglSwapInterval(s_display, 1);

    glViewport(0, 0, (GLsizei)s_width, (GLsizei)s_height);
    s_ready = true;
    return true;

fail_context:
    eglDestroyContext(s_display, s_context);
    s_context = EGL_NO_CONTEXT;
fail_surface:
    eglDestroySurface(s_display, s_surface);
    s_surface = EGL_NO_SURFACE;
fail_display:
    eglTerminate(s_display);
    s_display = EGL_NO_DISPLAY;
    s_ready = false;
    return false;
}

static bool apply_window_geometry(bool docked) {
    uint32_t w, h;
    resolution_for_mode(docked, &w, &h);

    /* nwindowSetDimensions must run before buffers are registered (pre-EGL). */
    Result rc = nwindowSetDimensions(s_win, w, h);
    if (R_FAILED(rc)) {
        fprintf(stderr, "nx_gpu: nwindowSetDimensions(%u,%u) failed: 0x%x\n", w, h, rc);
        return false;
    }

    /* Crop to the requested rectangle so presents match the mode size. */
    rc = nwindowSetCrop(s_win, 0, 0, (s32)w, (s32)h);
    if (R_FAILED(rc)) {
        fprintf(stderr, "nx_gpu: nwindowSetCrop failed: 0x%x\n", rc);
        return false;
    }

    s_width = w;
    s_height = h;
    s_docked = docked;
    return true;
}

bool dw3_nx_gpu_init(void) {
    if (s_ready) {
        return true;
    }

    s_win = nwindowGetDefault();
    if (!s_win) {
        fprintf(stderr, "nx_gpu: nwindowGetDefault returned NULL\n");
        return false;
    }

    if (!apply_window_geometry(current_docked())) {
        return false;
    }

    if (!init_egl(s_win)) {
        return false;
    }

    fprintf(stderr, "nx_gpu: EGL/GLES2 ready %ux%u (%s)\n", s_width, s_height,
            s_docked ? "docked" : "handheld");
    return true;
}

void dw3_nx_gpu_shutdown(void) {
    deinit_egl();
    s_win = NULL;
    s_width = 0;
    s_height = 0;
}

void dw3_nx_gpu_update_mode(void) {
    if (!s_win) {
        return;
    }

    bool docked = current_docked();
    if (s_ready && docked == s_docked) {
        return;
    }

    /* Must tear down EGL before nwindowSetDimensions (buffers registered). */
    deinit_egl();
    if (!apply_window_geometry(docked)) {
        return;
    }
    if (!init_egl(s_win)) {
        return;
    }

    fprintf(stderr, "nx_gpu: mode -> %ux%u (%s)\n", s_width, s_height,
            s_docked ? "docked" : "handheld");
}

void dw3_nx_gpu_get_info(Dw3NxGpuInfo *out) {
    if (!out) {
        return;
    }
    out->width = s_width;
    out->height = s_height;
    out->docked = s_docked;
}

void dw3_nx_gpu_clear(float r, float g, float b, float a) {
    if (!s_ready) {
        return;
    }
    glViewport(0, 0, (GLsizei)s_width, (GLsizei)s_height);
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

bool dw3_nx_gpu_swap(void) {
    if (!s_ready) {
        return false;
    }
    return eglSwapBuffers(s_display, s_surface) == EGL_TRUE;
}

/* --- PSY-Z video contract (P4.7) --------------------------------------- */

/* Mirrors the timing half of sdl3_common.h's Psyz_VideoVSync: PSY-Z calls this
 * through VSync() (libapi.c) with
 *
 *   mode <  0  query — return the current frame number
 *   mode == 0  present, then wait for the next frame; return scanlines elapsed
 *   mode >  0  report scanlines elapsed without waiting
 *
 * A scanline is approximated as 64 us, as PSY-Z does. Presenting is
 * dw3_nx_gpu_swap()'s job and eglSwapBuffers already blocks on vsync at swap
 * interval 1, so that call *is* the frame pacing — no extra sleep. The real
 * renderer (Stage 3) is what will drive this once it exists.
 */
int Psyz_VideoVSync(int mode) {
    static uint32_t last_vsync_us;
    uint64_t now_us64 = armTicksToNs(armGetSystemTick()) / 1000ULL;
    uint32_t now_us = (uint32_t)now_us64;
    unsigned short lines;

    if (mode < 0) {
        return (int)(now_us64 * 60 / 1000000ULL);
    }
    /* 32-bit subtraction, so the ~71 min microsecond wrap cancels out. */
    lines = (unsigned short)((now_us - last_vsync_us) >> 6);
    if (mode == 0) {
        dw3_nx_gpu_swap();
        last_vsync_us = (uint32_t)(armTicksToNs(armGetSystemTick()) / 1000ULL);
    }
    return lines;
}
