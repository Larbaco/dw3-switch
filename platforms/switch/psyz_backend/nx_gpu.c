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
 * A scanline is approximated as 64 us, as PSY-Z does. mode == 0 presents the
 * VRAM the rasterizer produced (see dw3_nx_gpu_present below); eglSwapBuffers
 * already blocks on vsync at swap interval 1, so that call *is* the frame
 * pacing — no extra sleep.
 */
int Psyz_VideoVSync(int mode) {
    static uint32_t last_vsync_us;
    static int entered;
    uint64_t now_us64 = armTicksToNs(armGetSystemTick()) / 1000ULL;
    uint32_t now_us = (uint32_t)now_us64;
    unsigned short lines;

    /* One-shot: VSync() calls this *before* the vsync callback, so if the boot
     * hangs with no callback marker, this says whether it got this far.
     * psyz_log directly rather than PSY-Z's INFOF macro: this file must not
     * include PSY-Z headers next to the EGL ones (they redirect libc names). */
    if (!entered) {
        entered = 1;
        psyz_log(1, "nx_gpu.c", __LINE__, __func__,
                 "Psyz_VideoVSync first call (mode=%d)", mode);
    }

    if (mode < 0) {
        return (int)(now_us64 * 60 / 1000000ULL);
    }
    /* 32-bit subtraction, so the ~71 min microsecond wrap cancels out. */
    lines = (unsigned short)((now_us - last_vsync_us) >> 6);
    if (mode == 0) {
#ifdef DW3_NX_GAME_IMAGE
        Dw3NxDisplay disp;

        dw3_nx_switch_gpu_display(&disp);
        if (disp.on) {
            /* One-shot: "the screen is black" has two very different causes —
             * the game never unmasked the display, or it did and drew into a
             * VRAM the presenter is not showing. This separates them, and the
             * rect is the other half of it: a present of the wrong VRAM
             * rectangle looks exactly like a wrong picture. */
            static int logged_on;

            if (!logged_on) {
                logged_on = 1;
                psyz_log(1, "nx_gpu.c", __LINE__, __func__,
                         "display on: vram %d,%d %dx%d -> surface %ux%u",
                         disp.x, disp.y, disp.width, disp.height, s_width,
                         s_height);
            }
            dw3_nx_gpu_present(dw3_nx_switch_gpu_vram(), disp.x, disp.y,
                               disp.width, disp.height);
        } else {
            /* SetDispMask said "no display"; on hardware that is a black
             * screen, so do not keep showing the last frame. */
            dw3_nx_gpu_clear(0.0f, 0.0f, 0.0f, 1.0f);
        }
#endif
        dw3_nx_gpu_swap();
        if (entered == 1) {
            entered = 2;
            psyz_log(1, "nx_gpu.c", __LINE__, __func__,
                     "first present+swap done");
        }
        last_vsync_us = (uint32_t)(armTicksToNs(armGetSystemTick()) / 1000ULL);
    }
    return lines;
}

/* --- VRAM present (P4.7 Stage 3) --------------------------------------- */

#define DW3_NX_VRAM_W 1024
#define DW3_NX_VRAM_H 512

static GLuint s_vram_tex;
static GLuint s_quad_prog;
static GLuint s_quad_vbo;
static GLint s_attr_pos = -1;
static GLint s_attr_t = -1;
static GLint s_uni_uv0 = -1;
static GLint s_uni_uv1 = -1;
static GLint s_uni_tex = -1;

/* PS1 VRAM is 1024x512 RGB555 and GL_UNSIGNED_SHORT_5_5_5_1 is that layout with
 * the mask bit landing in alpha, so the upload needs no conversion. */
static const char *QUAD_VS =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_t;\n"
    "varying vec2 v_t;\n"
    "void main() { v_t = a_t; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";

static const char *QUAD_FS =
    "precision mediump float;\n"
    "varying vec2 v_t;\n"
    "uniform vec2 u_uv0;\n"
    "uniform vec2 u_uv1;\n"
    "uniform sampler2D u_tex;\n"
    "void main() { gl_FragColor = texture2D(u_tex, u_uv0 + v_t * (u_uv1 - u_uv0)); }\n";

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint sh = glCreateShader(type);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(sh, sizeof(log), NULL, log);
        fprintf(stderr, "nx_gpu: shader %u failed: %s\n", (unsigned)type, log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static bool present_init(void) {
    /* The quad spans clip space; a_t runs 0..1 with 0 at the top-left so the
     * fragment shader can map it onto a VRAM sub-rectangle. */
    static const float quad[] = {
        /* a_pos      a_t      */
        -1.0f, -1.0f, 0.0f, 1.0f, /* bottom-left  */
        1.0f,  -1.0f, 1.0f, 1.0f, /* bottom-right */
        -1.0f, 1.0f,  0.0f, 0.0f, /* top-left     */
        1.0f,  1.0f,  1.0f, 0.0f, /* top-right    */
    };
    GLuint vs = compile_shader(GL_VERTEX_SHADER, QUAD_VS);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, QUAD_FS);
    GLint ok = 0;

    if (!vs || !fs) {
        if (vs) {
            glDeleteShader(vs);
        }
        if (fs) {
            glDeleteShader(fs);
        }
        return false;
    }
    s_quad_prog = glCreateProgram();
    glAttachShader(s_quad_prog, vs);
    glAttachShader(s_quad_prog, fs);
    glLinkProgram(s_quad_prog);
    glGetProgramiv(s_quad_prog, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(s_quad_prog, sizeof(log), NULL, log);
        fprintf(stderr, "nx_gpu: quad program link failed: %s\n", log);
        return false;
    }

    s_attr_pos = glGetAttribLocation(s_quad_prog, "a_pos");
    s_attr_t = glGetAttribLocation(s_quad_prog, "a_t");
    s_uni_uv0 = glGetUniformLocation(s_quad_prog, "u_uv0");
    s_uni_uv1 = glGetUniformLocation(s_quad_prog, "u_uv1");
    s_uni_tex = glGetUniformLocation(s_quad_prog, "u_tex");

    glGenBuffers(1, &s_quad_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    glGenTextures(1, &s_vram_tex);
    glBindTexture(GL_TEXTURE_2D, s_vram_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, DW3_NX_VRAM_W, DW3_NX_VRAM_H, 0,
                 GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, NULL);
    return true;
}

void dw3_nx_gpu_present(const uint16_t *vram, int dx, int dy, int dw, int dh) {
    float src_aspect;
    float dst_aspect;
    int vx;
    int vy;
    int vw;
    int vh;

    if (!s_ready || !vram || dw <= 0 || dh <= 0) {
        return;
    }
    /* A dock/handheld switch recreates the EGL context, which drops every GL
     * object. glIsTexture reports the stale names as invalid, so this heals
     * itself instead of the mode-change path having to remember to. */
    if (!s_quad_prog || !glIsTexture(s_vram_tex)) {
        if (!present_init()) {
            return;
        }
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_vram_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, DW3_NX_VRAM_W, DW3_NX_VRAM_H,
                    GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, vram);

    /* Letterbox rather than stretch: a 320x240 frame on a 16:9 surface would
     * otherwise be visibly wrong. */
    src_aspect = (float)dw / (float)dh;
    dst_aspect = (float)s_width / (float)s_height;
    if (src_aspect > dst_aspect) {
        vw = (int)s_width;
        vh = (int)((float)s_width / src_aspect);
    } else {
        vh = (int)s_height;
        vw = (int)((float)s_height * src_aspect);
    }
    vx = ((int)s_width - vw) / 2;
    vy = ((int)s_height - vh) / 2;
    glViewport(vx, vy, vw, vh);

    glUseProgram(s_quad_prog);
    glUniform2f(s_uni_uv0, (float)dx / (float)DW3_NX_VRAM_W,
                (float)dy / (float)DW3_NX_VRAM_H);
    glUniform2f(s_uni_uv1, (float)(dx + dw) / (float)DW3_NX_VRAM_W,
                (float)(dy + dh) / (float)DW3_NX_VRAM_H);
    glUniform1i(s_uni_tex, 0);

    glBindBuffer(GL_ARRAY_BUFFER, s_quad_vbo);
    glEnableVertexAttribArray((GLuint)s_attr_pos);
    glVertexAttribPointer((GLuint)s_attr_pos, 2, GL_FLOAT, GL_FALSE,
                          sizeof(float) * 4, (const void *)0);
    glEnableVertexAttribArray((GLuint)s_attr_t);
    glVertexAttribPointer((GLuint)s_attr_t, 2, GL_FLOAT, GL_FALSE,
                          sizeof(float) * 4,
                          (const void *)(sizeof(float) * 2));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
