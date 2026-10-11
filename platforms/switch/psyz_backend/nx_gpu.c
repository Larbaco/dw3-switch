/* P4.2 — native present via EGL + OpenGL ES 2 (mesa/nouveau on libnx).
 * Modelled on switchbrew/switch-examples graphics/opengl/es2gears.
 * PSY-Z InitPlatform / VRAM blit lands later; this owns the host surface. */
#include "nx.h"
#include "nx_mesh.h"
#include "nx_mesh_debug.h"
#include "nx_vram_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#if DW3_NX_DEBUG_MESH
static uint64_t gpu_now_us(void) {
    return armTicksToNs(armGetSystemTick()) / 1000ULL;
}
#endif

int dw3_nx_backend_sketch_version(void) {
    return 4; /* P4.2 present + P4.3 audio + P5.2 mesh FBO */
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
    dw3_nx_mesh_shutdown();
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
    EGLConfig config;
    EGLint num_configs = 0;
    int use_es3 = 0;

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

    /* Prefer ES3 for the battle mesh path; fall back to ES2 (software only). */
    {
        static const EGLint fb_es3[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 0,
            EGL_STENCIL_SIZE, 0,
            EGL_NONE,
        };
        eglChooseConfig(s_display, fb_es3, &config, 1, &num_configs);
        if (num_configs > 0) {
            use_es3 = 1;
        }
    }
    if (num_configs == 0) {
        static const EGLint fb_es2[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 0,
            EGL_STENCIL_SIZE, 0,
            EGL_NONE,
        };
        eglChooseConfig(s_display, fb_es2, &config, 1, &num_configs);
    }
    if (num_configs == 0) {
        fprintf(stderr, "nx_gpu: eglChooseConfig failed: 0x%x\n", eglGetError());
        goto fail_display;
    }

    s_surface = eglCreateWindowSurface(s_display, config, win, NULL);
    if (s_surface == EGL_NO_SURFACE) {
        fprintf(stderr, "nx_gpu: eglCreateWindowSurface failed: 0x%x\n", eglGetError());
        goto fail_display;
    }

    {
        const EGLint ctx_attribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, use_es3 ? 3 : 2,
            EGL_NONE,
        };
        s_context = eglCreateContext(s_display, config, EGL_NO_CONTEXT, ctx_attribs);
    }
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
    if (use_es3) {
        if (!dw3_nx_mesh_init()) {
            fprintf(stderr, "nx_gpu: mesh init failed; software triangles only\n");
        }
    } else {
        fprintf(stderr, "nx_gpu: ES2 context; mesh GPU path disabled\n");
    }
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
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, (GLsizei)s_width, (GLsizei)s_height);
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void dw3_nx_gpu_fill_rect(int x, int y, int w, int h, float r, float g, float b,
                          float a) {
    if (!s_ready || w <= 0 || h <= 0 || s_width == 0 || s_height == 0) {
        return;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (w <= 0 || h <= 0) {
        return;
    }
    if ((uint32_t)x >= s_width || (uint32_t)y >= s_height) {
        return;
    }
    if ((uint32_t)x + (uint32_t)w > s_width) {
        w = (int)(s_width - (uint32_t)x);
    }
    if ((uint32_t)y + (uint32_t)h > s_height) {
        h = (int)(s_height - (uint32_t)y);
    }
    glEnable(GL_SCISSOR_TEST);
    glScissor((GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h);
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
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
static float dw3_nx_display_aspect(int range_w, int range_h, int pal);

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
            /* Size only. The movie double-buffer flips x every frame; logging
             * that fills the guest log and is not a mode change. */
            static int prev_w = -1;
            static int prev_h = -1;

            if (disp.width != prev_w || disp.height != prev_h) {
                prev_w = disp.width;
                prev_h = disp.height;
                psyz_log(1, "nx_gpu.c", __LINE__, __func__,
                         "display on: vram %d,%d %dx%d -> surface %ux%u",
                         disp.x, disp.y, disp.width, disp.height, s_width,
                         s_height);
            }
            dw3_nx_gpu_present(dw3_nx_switch_gpu_vram(), disp.x, disp.y,
                               disp.width, disp.height,
                               dw3_nx_display_aspect(disp.range_w, disp.range_h,
                                                     disp.pal));
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

static GLuint s_vram_tex;
static GLuint s_quad_prog;
static GLuint s_ui_prog;
static GLuint s_quad_vbo;
static GLint s_attr_pos = -1;
static GLint s_attr_t = -1;
static GLint s_uni_uv0 = -1;
static GLint s_uni_uv1 = -1;
static GLint s_uni_tex = -1;
static GLint s_ui_attr_pos = -1;
static GLint s_ui_attr_t = -1;
static GLint s_ui_uv0 = -1;
static GLint s_ui_uv1 = -1;
static GLint s_ui_tex = -1;

/* Scratch for one tight rectangle. A full 1024×512 frame still fits; present
 * only fills and uploads the sampled (dw × dh) region. */
static uint8_t s_vram_rgba[DW3_NX_VRAM_W * DW3_NX_VRAM_H * 4];

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

/* Overlay CPU VRAM (sprites/UI). Drop cleared black so the mesh FBO shows. */
static const char *UI_FS =
    "precision mediump float;\n"
    "varying vec2 v_t;\n"
    "uniform vec2 u_uv0;\n"
    "uniform vec2 u_uv1;\n"
    "uniform sampler2D u_tex;\n"
    "void main() {\n"
    "  vec4 c = texture2D(u_tex, u_uv0 + v_t * (u_uv1 - u_uv0));\n"
    "  if (c.r + c.g + c.b < 0.002) discard;\n"
    "  gl_FragColor = c;\n"
    "}\n";

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

static GLuint link_simple(GLuint vs, const char *fs_src) {
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    GLuint prog;
    GLint ok = 0;

    if (!fs) {
        return 0;
    }
    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glDeleteShader(fs);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        fprintf(stderr, "nx_gpu: program link failed: %s\n", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
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

    if (!vs) {
        return false;
    }
    s_quad_prog = link_simple(vs, QUAD_FS);
    s_ui_prog = link_simple(vs, UI_FS);
    glDeleteShader(vs);
    if (!s_quad_prog || !s_ui_prog) {
        return false;
    }

    s_attr_pos = glGetAttribLocation(s_quad_prog, "a_pos");
    s_attr_t = glGetAttribLocation(s_quad_prog, "a_t");
    s_uni_uv0 = glGetUniformLocation(s_quad_prog, "u_uv0");
    s_uni_uv1 = glGetUniformLocation(s_quad_prog, "u_uv1");
    s_uni_tex = glGetUniformLocation(s_quad_prog, "u_tex");
    s_ui_attr_pos = glGetAttribLocation(s_ui_prog, "a_pos");
    s_ui_attr_t = glGetAttribLocation(s_ui_prog, "a_t");
    s_ui_uv0 = glGetUniformLocation(s_ui_prog, "u_uv0");
    s_ui_uv1 = glGetUniformLocation(s_ui_prog, "u_uv1");
    s_ui_tex = glGetUniformLocation(s_ui_prog, "u_tex");

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
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    return true;
}

/* Same rule as sdl3_common.h GetCurrentGameAspectRatio(PSYZ_ASPECT_DISPLAY):
 * (4/3) * (horizontal_range/256) / (vertical_range / vref). The ranges are
 * the CRT beam window, not the VRAM rectangle. A 320x480 24-bit movie keeps
 * vertical_range 240, so this is 4:3. Using dw/dh instead made a 2:3 strip. */
static float dw3_nx_display_aspect(int range_w, int range_h, int pal) {
    float vref = pal ? 288.0f : 240.0f;
    float horiz = range_w > 0 ? (float)range_w : 256.0f;
    float vert = range_h > 0 ? (float)range_h : 240.0f;

    return (4.0f / 3.0f) * (horiz / 256.0f) / (vert / vref);
}

void dw3_nx_gpu_present(const uint16_t *vram, int dx, int dy, int dw, int dh,
                        float src_aspect) {
    float dst_aspect;
    int vx;
    int vy;
    int vw;
    int vh;
    int px;
    int py;
    int pw;
    int ph;
    int use_mesh;
#if DW3_NX_DEBUG_MESH
    uint64_t present_t0 = 0;
#endif

    if (!s_ready || !vram || dw <= 0 || dh <= 0) {
        return;
    }
    /* Flush any deferred mesh before we composite. */
    dw3_nx_mesh_flush(vram);
    use_mesh = dw3_nx_mesh_has_content();
#if DW3_NX_DEBUG_MESH
    /* present_us is composite/blit only; flush/upload are counted separately. */
    present_t0 = gpu_now_us();
#endif

    /* A dock/handheld switch recreates the EGL context, which drops every GL
     * object. glIsTexture reports the stale names as invalid, so this heals
     * itself instead of the mode-change path having to remember to. */
    if (!s_quad_prog || !glIsTexture(s_vram_tex)) {
        if (!present_init()) {
            return;
        }
    }

    /* Shader samples (dx, dy, dw, dh). Upload that rectangle into the
     * texture created in present_init. A rect that clamps to nothing skips
     * the frame the same way a zero size does. */
    if (dw3_nx_pack_rgb555_rect(vram, dx, dy, dw, dh, s_vram_rgba, &px, &py,
                                &pw, &ph) == 0) {
        return;
    }

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_vram_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, px, py, pw, ph, GL_RGBA, GL_UNSIGNED_BYTE,
                    s_vram_rgba);

    /* Fit the display aspect into the framebuffer. Full height, side bars
     * on 16:9. Do not crop, and do not use the VRAM pixel ratio: dw/dh for
     * the movie is 320/480. */
    if (src_aspect <= 0.0f) {
        src_aspect = 4.0f / 3.0f;
    }
    dst_aspect = (float)s_width / (float)s_height;
    if (src_aspect > dst_aspect) {
        vw = (int)s_width;
        vh = (int)((float)s_width / src_aspect + 0.5f);
    } else {
        vh = (int)s_height;
        vw = (int)((float)s_height * src_aspect + 0.5f);
    }
    vx = ((int)s_width - vw) / 2;
    vy = ((int)s_height - vh) / 2;
    {
        static int prev_vw = -1;
        static int prev_vh = -1;
        static uint64_t prev_mesh_prims;
#if DW3_NX_DEBUG_MESH
        static int prev_use_mesh = -1;
#endif

        if (vw != prev_vw || vh != prev_vh) {
            prev_vw = vw;
            prev_vh = vh;
            psyz_log(1, "nx_gpu.c", __LINE__, __func__,
                     "present viewport %d,%d %dx%d aspect %.3f on %ux%u "
                     "(vram %d,%d %dx%d) mesh_avail=%d",
                     vx, vy, vw, vh, src_aspect, s_width, s_height, dx, dy,
                     dw, dh, dw3_nx_mesh_available() ? 1 : 0);
        }
#if DW3_NX_DEBUG_MESH
        if (use_mesh != prev_use_mesh) {
            prev_use_mesh = use_mesh;
            psyz_log(1, "nx_gpu.c", __LINE__, __func__,
                     "present path: %s",
                     use_mesh ? "mesh+ui" : "opaque_blit");
        }
#endif
        if (use_mesh) {
            uint64_t prims = dw3_nx_mesh_prims_flushed();

            if (prims != prev_mesh_prims &&
                (prev_mesh_prims == 0 || (prims - prev_mesh_prims) >= 1000u)) {
                prev_mesh_prims = prims;
                psyz_log(1, "nx_gpu.c", __LINE__, __func__,
                         "mesh gpu: %llu tris flushed (composite on)",
                         (unsigned long long)prims);
            }
        }
    }

    /* Mesh first (3D), then CPU VRAM. With mesh: discard cleared black so UI
     * sprites sit on top. Without mesh: opaque blit as before. */
    if (use_mesh) {
        dw3_nx_mesh_present(dx, dy, dw, dh, vx, vy, vw, vh);
        glViewport(vx, vy, vw, vh);
        glUseProgram(s_ui_prog);
        glUniform2f(s_ui_uv0, (float)dx / (float)DW3_NX_VRAM_W,
                    (float)dy / (float)DW3_NX_VRAM_H);
        glUniform2f(s_ui_uv1, (float)(dx + dw) / (float)DW3_NX_VRAM_W,
                    (float)(dy + dh) / (float)DW3_NX_VRAM_H);
        glUniform1i(s_ui_tex, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_vram_tex);
        glBindBuffer(GL_ARRAY_BUFFER, s_quad_vbo);
        glEnableVertexAttribArray((GLuint)s_ui_attr_pos);
        glVertexAttribPointer((GLuint)s_ui_attr_pos, 2, GL_FLOAT, GL_FALSE,
                              sizeof(float) * 4, (const void *)0);
        glEnableVertexAttribArray((GLuint)s_ui_attr_t);
        glVertexAttribPointer((GLuint)s_ui_attr_t, 2, GL_FLOAT, GL_FALSE,
                              sizeof(float) * 4,
                              (const void *)(sizeof(float) * 2));
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    } else {
        glViewport(vx, vy, vw, vh);
        glUseProgram(s_quad_prog);
        glUniform2f(s_uni_uv0, (float)dx / (float)DW3_NX_VRAM_W,
                    (float)dy / (float)DW3_NX_VRAM_H);
        glUniform2f(s_uni_uv1, (float)(dx + dw) / (float)DW3_NX_VRAM_W,
                    (float)(dy + dh) / (float)DW3_NX_VRAM_H);
        glUniform1i(s_uni_tex, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_vram_tex);
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
#if DW3_NX_DEBUG_MESH
    dw3_nx_mesh_debug_note_present(use_mesh,
                                   (uint32_t)(gpu_now_us() - present_t0));
#endif
    dw3_nx_mesh_end_frame();
}
