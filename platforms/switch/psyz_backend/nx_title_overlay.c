/* Native-resolution title menu and logo on the Switch GLES present.
 *
 * Same content as the PC SDL3 GPU overlay (platforms/pc/src/
 * title_menu_overlay.c): a 960x720 transparent canvas in five states
 * (PRESS START, New Game / Options / Load Game focused, and the Port options
 * screen, which fades in over the title and out again), plus the
 * original logo (US, or the EU bank of the current language) resampled from
 * runtime VRAM into a 720x326 texture. EU labels regenerate the canvas.
 *
 * The four title states are rasterized once per GL context. The Port options
 * state is kept in a shared CPU cache (platforms/pc/src/title_options_cache.c)
 * and only the rectangles that changed are sent to the texture with
 * glTexSubImage2D, packed contiguously because GL_UNPACK_ROW_LENGTH is not in
 * GLES2. The cache is built when the title menu first shows (so opening the
 * screen is a normal update), released when the overlay stops drawing
 * (restart, leaving the title) and rebuilt, with a full upload, on the next
 * title visit.
 *
 * Both are drawn over the game picture's 4:3 viewport after the frame is
 * composited and before the debug HUD. The game loop and the present run on
 * the same thread; the logo is kept on the CPU and uploaded on the next draw
 * so a dock/handheld context loss can rebuild every GL object.
 *
 * Memory: the 16.2 MB atlas, the logo texture and the Yes / No window texture
 * live in the GLES driver's libc heap. They are released after
 * TEXTURE_IDLE_FRAMES draws in which they were not needed (the field, a
 * battle, a movie) and rebuilt lazily on the next use: the title ones by
 * ensure_gl (restart, Return to title, EU language select), the Yes / No one
 * by the draw. */
#include "nx_title_overlay.h"

#include "../../pc/src/title_options_cache.h"
#include "../../pc/src/title_overlay_art.h"
#include "nx_debug_overlay.h"
#include "port_options.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GLES3/gl3.h>

/* Per-update timing for nxlink and HUD builds only; the release image does
 * not read the clock and does not log from this path. */
#if defined(DW3_NXLINK_STDIO) || DW3_NX_DEBUG_OVERLAY
#define DW3_NX_MENU_TIMING 1
#include <switch/arm/counter.h>
#include "../source/debug_overlay_text.h"
#else
#define DW3_NX_MENU_TIMING 0
#endif

/* nx_log.c: stderr/nxlink, sdmc:/switch/dw3/dw3.log and the debug string. */
void psyz_log(unsigned int level, const char *file, unsigned int line,
              const char *func, const char *fmt, ...);
#define TITLE_LOG(...) psyz_log(1, "nx_title_overlay.c", __LINE__, __func__, \
                                __VA_ARGS__)

enum {
    CANVAS_W = 960,
    CANVAS_H = 720,
    /* PRESS START, the four menu rows (P6.28 added Quit game), the options. */
    STATE_COUNT = 6,
    STATE_OPTIONS = 5,
    /* Port options screen fade, in alpha steps per frame (about 13 frames). */
    OPTIONS_FADE_STEP = 20,
    ART_TEX_H = CANVAS_H * STATE_COUNT,
    LOGO_W = 720,
    LOGO_H = 326,
    /* Logo placement in canvas pixels, as on PC. */
    LOGO_X0 = 120,
    LOGO_Y0 = 47,
    LOGO_X1 = 840,
    LOGO_Y1 = 373,
    /* Draws without a use before a texture is released (~1.5 s at 60 fps), so
     * a restart that flickers the overlay off for a few frames keeps it. */
    TEXTURE_IDLE_FRAMES = 90,
};

static GLuint s_prog;
static GLuint s_art_tex;
static GLuint s_logo_tex;
/* P6.27 / P6.28 Yes / No window: its own small texture, rendered on the CPU
 * when its kind, focus or language changes and uploaded on the next draw. */
static GLuint s_dialog_tex;
static int s_dialog_active;
static int s_dialog_kind;
static int s_dialog_focus;
static int s_dialog_dirty;
static unsigned s_dialog_language = ~0u;
static GLuint s_vbo;
static GLint s_attr_pos = -1;
static GLint s_attr_t = -1;
static GLint s_uni_tex = -1;
static GLint s_uni_sub = -1;
static GLint s_uni_alpha = -1;
static int s_gl_failed;
static unsigned s_atlas_builds;
/* Consecutive draws without the title (or the Yes / No window) on screen. */
static int s_title_idle;
static int s_dialog_idle;

static int s_selection;
static int s_overlay_active;
static int s_logo_active;
static uint8_t *s_logo_rgba;
static int s_logo_dirty;
/* Title edge-fade darkness, 0..255 (STDWTITL_tickEdgeFade). */
static int s_fade;
/* dw3_pc_title_overlay_generation() of the uploaded canvas (EU labels). */
static unsigned s_art_generation;
/* dw3_pc_title_overlay_options_generation() of the uploaded options state;
 * only the fallback below (cache canvases could not be allocated) uses it. */
static unsigned s_options_generation;
/* Contiguous staging for one dirty rectangle (GLES2 has no row length). */
static uint8_t *s_options_pack;
static size_t s_options_pack_bytes;
static int s_options_cache_failed_logged;
/* The cache was built (and the options region filled) for this title visit. */
static int s_options_cache_warm;
/* Port options screen opacity, 0..255. */
static int s_options_alpha;

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint sh = glCreateShader(type);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(sh, sizeof(log), NULL, log);
        TITLE_LOG("dw3-title-overlay: shader failed: %s", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint make_texture(int w, int h, const void *pixels) {
    GLuint tex = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels);
    return tex;
}

/* Atlas states 0..STATE_OPTIONS-1 (the four title states and PRESS START) are
 * rasterized on the CPU (no art is shipped), one at a time, and uploaded as
 * they are needed; STATE_OPTIONS is left transparent: the shared cache fills it
 * on the next open. A fresh or released atlas is not rebuilt in one go (that
 * was ~0.3 s in one frame): the state on screen is built at once and the others
 * one per frame after it (pump_states). */
enum { STATES_ALL = (1 << STATE_OPTIONS) - 1 };
static unsigned s_states_ready;

static int upload_state(int state) {
    const size_t canvas_bytes = (size_t)CANVAS_W * CANVAS_H * 4;
    uint8_t *pixels = (uint8_t *)malloc(canvas_bytes);

    if (!pixels) {
        return 0;
    }
    dw3_pc_title_overlay_art(pixels, CANVAS_W * 4, state == 0 ? -1 : state - 1);
    glBindTexture(GL_TEXTURE_2D, s_art_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, state * CANVAS_H, CANVAS_W, CANVAS_H,
                    GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    free(pixels);
    s_states_ready |= 1u << state;
    return 1;
}

/* The atlas content is stale (new texture, or EU labels changed): the options
 * region must be sent again and every title state rebuilt. */
static void atlas_content_reset(void) {
    s_art_generation = dw3_pc_title_overlay_generation();
    s_states_ready = 0;
    s_options_generation = ~0u;
    dw3_options_cache_invalidate();
}

#if DW3_NX_MENU_TIMING
static uint64_t timing_now_us(void) {
    return armTicksToNs(armGetSystemTick()) / 1000ULL;
}
#endif

/* Fallback when the cache canvases cannot be allocated: render the whole
 * options canvas and upload it, as before the cache. */
static void update_options_reference(void) {
    uint8_t *options;

    if (s_options_generation == dw3_pc_title_overlay_options_generation()) {
        return;
    }
    options = (uint8_t *)malloc((size_t)CANVAS_W * CANVAS_H * 4);
    if (!options) {
        return;
    }
    memset(options, 0, (size_t)CANVAS_W * CANVAS_H * 4);
    dw3_pc_title_overlay_art(options, CANVAS_W * 4, DW3_TITLE_ART_OPTIONS);
    glBindTexture(GL_TEXTURE_2D, s_art_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, STATE_OPTIONS * CANVAS_H, CANVAS_W,
                    CANVAS_H, GL_RGBA, GL_UNSIGNED_BYTE, options);
    free(options);
    s_options_generation = dw3_pc_title_overlay_options_generation();
}

#if DW3_NX_MENU_TIMING
typedef struct MenuTiming {
    unsigned session_updates;
    unsigned batch;
    uint64_t compose_sum, upload_sum, bytes_sum;
    uint32_t compose_max, upload_max, total_max;
} MenuTiming;
static MenuTiming s_timing;

static void timing_flush(void) {
    if (s_timing.batch) {
        const unsigned n = s_timing.batch;
        TITLE_LOG("dw3-title-overlay: options %u updates compose avg=%u max=%u us "
                  "upload avg=%u max=%u us bytes avg=%u",
                  n, (unsigned)(s_timing.compose_sum / n), s_timing.compose_max,
                  (unsigned)(s_timing.upload_sum / n), s_timing.upload_max,
                  (unsigned)(s_timing.bytes_sum / n));
    }
    s_timing.batch = 0;
    s_timing.compose_sum = s_timing.upload_sum = s_timing.bytes_sum = 0;
    s_timing.compose_max = s_timing.upload_max = 0;
}

static void timing_note(const Dw3OptionsDirty *d, uint32_t compose_us,
                        uint32_t upload_us) {
    const uint32_t total = compose_us + upload_us;
    Dw3DebugOverlayMenuSnap snap;

    s_timing.session_updates++;
    if (total > s_timing.total_max) s_timing.total_max = total;
    if (d->full || d->rebuilt || s_timing.session_updates <= 8) {
        TITLE_LOG("dw3-title-overlay: options update #%u %s rects=%d bytes=%u "
                  "compose=%u us upload=%u us",
                  s_timing.session_updates,
                  d->rebuilt ? "rebuild" : d->full ? "full" : "dirty", d->count,
                  (unsigned)d->bytes, compose_us, upload_us);
    } else {
        s_timing.batch++;
        s_timing.compose_sum += compose_us;
        s_timing.upload_sum += upload_us;
        s_timing.bytes_sum += d->bytes;
        if (compose_us > s_timing.compose_max) s_timing.compose_max = compose_us;
        if (upload_us > s_timing.upload_max) s_timing.upload_max = upload_us;
        if (s_timing.batch >= 30) timing_flush();
    }
    snap.valid = 1;
    snap.updates = s_timing.session_updates;
    snap.compose_us = compose_us;
    snap.upload_us = upload_us;
    snap.total_max_us = s_timing.total_max;
    dw3_nx_debug_overlay_note_menu(&snap);
}
#endif

/* Bring the options region of the atlas up to date: compose with the shared
 * component, upload only the dirty rectangles. */
static void update_options_texture(void) {
    Dw3OptionsDirty d;
#if DW3_NX_MENU_TIMING
    const uint64_t t0 = timing_now_us();
    uint64_t t1;
#endif

    if (dw3_options_cache_update(&d) != 0) {
        if (!s_options_cache_failed_logged) {
            TITLE_LOG("dw3-title-overlay: options cache unavailable; "
                      "using the full-canvas path");
            s_options_cache_failed_logged = 1;
        }
        update_options_reference();
        return;
    }
    s_options_generation = dw3_pc_title_overlay_options_generation();
    if (d.count == 0) {
        return;
    }
#if DW3_NX_MENU_TIMING
    t1 = timing_now_us();
#endif
    glBindTexture(GL_TEXTURE_2D, s_art_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    for (int i = 0; i < d.count; ++i) {
        const Dw3OptionsRect r = d.rects[i];
        const uint8_t *src;

        if (r.w == DW3_OPTIONS_CANVAS_W) {
            /* Full-width rows are already contiguous in the canvas. */
            src = dw3_options_cache_pixels() +
                  (size_t)r.y * DW3_OPTIONS_CANVAS_PITCH;
        } else {
            const size_t need = (size_t)r.w * r.h * 4;
            if (need > s_options_pack_bytes) {
                uint8_t *grown = (uint8_t *)realloc(s_options_pack, need);
                if (!grown) {
                    /* Leave the update uncommitted: the next frame resends
                     * the whole canvas. */
                    return;
                }
                s_options_pack = grown;
                s_options_pack_bytes = need;
            }
            dw3_options_cache_pack_rect(r, s_options_pack);
            src = s_options_pack;
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, r.x, STATE_OPTIONS * CANVAS_H + r.y,
                        r.w, r.h, GL_RGBA, GL_UNSIGNED_BYTE, src);
    }
    dw3_options_cache_commit();
#if DW3_NX_MENU_TIMING
    timing_note(&d, (uint32_t)(t1 - t0), (uint32_t)(timing_now_us() - t1));
#endif
}

/* Free the retained canvases once nothing of the overlay is on screen. */
static void release_options_cache(void) {
    dw3_options_cache_release();
    free(s_options_pack);
    s_options_pack = NULL;
    s_options_pack_bytes = 0;
    s_options_cache_warm = 0;
#if DW3_NX_MENU_TIMING
    timing_flush();
    memset(&s_timing, 0, sizeof(s_timing));
#endif
}

/* Drop the title-only textures (atlas, logo) once the title has not been on
 * screen for a while; ensure_gl rebuilds them on the next visit. The program,
 * the buffer and the CPU logo stay. */
static void release_title_textures(void) {
    if (!s_art_tex && !s_logo_tex) {
        return;
    }
    if (s_art_tex) glDeleteTextures(1, &s_art_tex);
    if (s_logo_tex) glDeleteTextures(1, &s_logo_tex);
    s_art_tex = 0;
    s_logo_tex = 0;
    /* A rebuilt atlas and logo texture are empty: send them again. */
    s_logo_dirty = s_logo_rgba != NULL;
    dw3_options_cache_invalidate();
    TITLE_LOG("dw3-title-overlay: title textures released (not on screen)");
}

static void release_dialog_texture(void) {
    if (!s_dialog_tex) {
        return;
    }
    glDeleteTextures(1, &s_dialog_tex);
    s_dialog_tex = 0;
    s_dialog_dirty = 1;
    TITLE_LOG("dw3-title-overlay: confirm window texture released");
}

/* The shader program and the quad buffer, shared by the title canvas and the
 * Yes / No window (which also shows in the field, where the canvas is not
 * built). */
static int ensure_program(void) {
    static const char *VS =
        "attribute vec2 a_pos;\n"
        "attribute vec2 a_t;\n"
        "varying vec2 v_t;\n"
        "void main(){v_t=a_t;gl_Position=vec4(a_pos,0.0,1.0);}\n";
    static const char *FS =
        "precision mediump float;\n"
        "varying vec2 v_t;\n"
        "uniform sampler2D u_tex;\n"
        "uniform float u_sub;\n"
        "uniform float u_alpha;\n"
        "void main(){vec4 c=texture2D(u_tex,v_t);\n"
        "  gl_FragColor=vec4(max(c.rgb-vec3(u_sub),0.0),c.a*u_alpha);}\n";
    GLuint vs;
    GLuint fs;
    GLint ok = 0;

    if (s_prog && s_vbo) {
        return 1;
    }
    if (s_gl_failed) {
        return 0;
    }
    vs = compile_shader(GL_VERTEX_SHADER, VS);
    fs = compile_shader(GL_FRAGMENT_SHADER, FS);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        s_gl_failed = 1;
        return 0;
    }
    s_prog = glCreateProgram();
    glAttachShader(s_prog, vs);
    glAttachShader(s_prog, fs);
    glLinkProgram(s_prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glGetProgramiv(s_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        glDeleteProgram(s_prog);
        s_prog = 0;
        s_gl_failed = 1;
        return 0;
    }
    s_attr_pos = glGetAttribLocation(s_prog, "a_pos");
    s_attr_t = glGetAttribLocation(s_prog, "a_t");
    s_uni_tex = glGetUniformLocation(s_prog, "u_tex");
    s_uni_sub = glGetUniformLocation(s_prog, "u_sub");
    s_uni_alpha = glGetUniformLocation(s_prog, "u_alpha");
    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, NULL, GL_DYNAMIC_DRAW);
    return 1;
}

/* Create the atlas and the logo texture on the first use in a GL context, and
 * again after release_title_textures. No state is rasterized here (see
 * pump_states); an EU label change only marks the states stale. */
static int ensure_gl(void) {
    if (s_prog && glIsTexture(s_art_tex)) {
        /* EU labels change the title canvases (and the options text). */
        if (s_art_generation != dw3_pc_title_overlay_generation()) {
            atlas_content_reset();
            TITLE_LOG("dw3-title-overlay: canvas regenerated "
                      "(generation %u)", s_art_generation);
        }
        return 1;
    }
    if (s_gl_failed) {
        return 0;
    }
    /* A recreated context left these names dangling; drop them. The program
     * and the buffer are kept when the Yes / No window already built them in
     * this context. */
    s_art_tex = 0;
    s_logo_tex = 0;
    if (!ensure_program()) {
        return 0;
    }
#if DW3_NX_MENU_TIMING
    const uint64_t build_t0 = timing_now_us();
#endif
    s_art_tex = make_texture(CANVAS_W, ART_TEX_H, NULL);
    {
        /* The options region is drawn (faded) before the cache fills it. */
        void *zero = calloc((size_t)CANVAS_W * CANVAS_H, 4);

        if (!zero) {
            glDeleteTextures(1, &s_art_tex);
            s_art_tex = 0;
            glDeleteProgram(s_prog);
            s_prog = 0;
            s_gl_failed = 1;
            return 0;
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, STATE_OPTIONS * CANVAS_H,
                        CANVAS_W, CANVAS_H, GL_RGBA, GL_UNSIGNED_BYTE, zero);
        free(zero);
    }
    atlas_content_reset();
    s_logo_tex = make_texture(LOGO_W, LOGO_H, NULL);
    s_logo_dirty = s_logo_rgba != NULL;
    TITLE_LOG("dw3-title-overlay: %dx%d RGBA texture, %d states (GLES)",
              CANVAS_W, ART_TEX_H, STATE_COUNT);
#if DW3_NX_MENU_TIMING
    TITLE_LOG("dw3-title-overlay: atlas create #%u %u us",
              ++s_atlas_builds, (unsigned)(timing_now_us() - build_t0));
#else
    ++s_atlas_builds;
#endif
    return 1;
}

/* Rasterize and upload the atlas state `need` (-1: none on screen) if it is
 * not there yet, else the next missing one: at most one per frame. */
static void pump_states(int need) {
    int state = -1;
#if DW3_NX_MENU_TIMING
    const uint64_t t0 = timing_now_us();
#endif

    if (s_states_ready == STATES_ALL) {
        return;
    }
    if (need >= 0 && need < STATE_OPTIONS && !(s_states_ready & (1u << need))) {
        state = need;
    } else {
        for (int i = 0; i < STATE_OPTIONS; ++i) {
            if (!(s_states_ready & (1u << i))) {
                state = i;
                break;
            }
        }
    }
    if (state >= 0 && upload_state(state)) {
#if DW3_NX_MENU_TIMING
        TITLE_LOG("dw3-title-overlay: atlas state %d %u us%s", state,
                  (unsigned)(timing_now_us() - t0),
                  state == need ? " (needed)" : "");
#endif
    }
}

/* Draw texture rect (u0,v0)-(u1,v1) into canvas rect (x0,y0)-(x1,y1) of the
 * 4:3 viewport already set by the caller. Canvas y grows downward. */
static void draw_quad(GLuint tex, float x0, float y0, float x1, float y1,
                      float u0, float v0, float u1, float v1) {
    const float l = x0 / CANVAS_W * 2.0f - 1.0f;
    const float r = x1 / CANVAS_W * 2.0f - 1.0f;
    const float t = 1.0f - y0 / CANVAS_H * 2.0f;
    const float b = 1.0f - y1 / CANVAS_H * 2.0f;
    const float quad[16] = {
        l, b, u0, v1,
        r, b, u1, v1,
        l, t, u0, v0,
        r, t, u1, v0,
    };

    glBindTexture(GL_TEXTURE_2D, tex);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad), quad);
    glEnableVertexAttribArray((GLuint)s_attr_pos);
    glVertexAttribPointer((GLuint)s_attr_pos, 2, GL_FLOAT, GL_FALSE,
                          sizeof(float) * 4, (const void *)0);
    glEnableVertexAttribArray((GLuint)s_attr_t);
    glVertexAttribPointer((GLuint)s_attr_t, 2, GL_FLOAT, GL_FALSE,
                          sizeof(float) * 4, (const void *)(sizeof(float) * 2));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void dw3_nx_title_overlay_draw(int vx, int vy, int vw, int vh) {
    const int options_open = dw3_port_options_is_open();
    /* The options screen has no logo; it comes back as the screen fades. */
    int logo = s_logo_active && s_logo_rgba && !options_open;

    if (options_open) {
        s_options_alpha += OPTIONS_FADE_STEP;
        if (s_options_alpha > 255) s_options_alpha = 255;
    } else {
        s_options_alpha -= OPTIONS_FADE_STEP;
        if (s_options_alpha < 0) s_options_alpha = 0;
    }
    const int title_visible = s_overlay_active || logo || s_options_alpha;
    const int dialog = s_dialog_active;

    if (title_visible) {
        s_title_idle = 0;
    } else if (s_title_idle < TEXTURE_IDLE_FRAMES &&
               ++s_title_idle == TEXTURE_IDLE_FRAMES) {
        release_title_textures();
    }
    if (dialog) {
        s_dialog_idle = 0;
    } else if (s_dialog_idle < TEXTURE_IDLE_FRAMES &&
               ++s_dialog_idle == TEXTURE_IDLE_FRAMES) {
        release_dialog_texture();
    }
    if (!title_visible && !dialog) {
        /* Nothing is shown (restart, mode change): drop the retained canvases. */
        release_options_cache();
        return;
    }
    if (vw <= 0 || vh <= 0) {
        return;
    }
    if (title_visible ? !ensure_gl() : !ensure_program()) {
        return;
    }
    if (title_visible) {
        pump_states(s_overlay_active && !options_open ? s_selection : -1);
    }
    if (s_options_alpha || (s_overlay_active && !s_options_cache_warm)) {
        /* Build the cache as soon as the title menu is up, so the first open
         * of the screen is a normal dirty-region update, not a raster. */
        s_options_cache_warm = 1;
        update_options_texture();
    }
#if DW3_NX_MENU_TIMING
    else if (s_timing.session_updates) {
        /* The screen has faded out: report the session and start a new one. */
        timing_flush();
        s_timing.session_updates = 0;
        s_timing.total_max = 0;
    }
#endif
    /* The title's display is 4:3; fit the canvas to it. With the gutters of
     * patch 0085 the picture is wider and the display is its full-height
     * middle, which is the same fit. */
    const int wide_x = vx;
    const int wide_w = vw;
    {
        int w = vw;
        int h = vw * 3 / 4;
        if (h > vh) {
            h = vh;
            w = vh * 4 / 3;
        }
        vx += (vw - w) / 2;
        vy += (vh - h) / 2;
        vw = w;
        vh = h;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    /* The equation too: other layers (battle mesh) change it. */
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                        GL_ONE_MINUS_SRC_ALPHA);
    glViewport(vx, vy, vw, vh);
    glUseProgram(s_prog);
    glUniform1i(s_uni_tex, 0);
    /* Follow the title's edge fade: it subtracts its level at the centre and
     * twice that at the edges; the overlay spans both, so use 1.5x. */
    glUniform1f(s_uni_sub, (float)s_fade * 1.5f / 255.0f);
    glUniform1f(s_uni_alpha, 1.0f);
    glActiveTexture(GL_TEXTURE0);

    if (logo) {
        if (s_logo_dirty) {
            glBindTexture(GL_TEXTURE_2D, s_logo_tex);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, LOGO_W, LOGO_H, GL_RGBA,
                            GL_UNSIGNED_BYTE, s_logo_rgba);
            s_logo_dirty = 0;
        }
        draw_quad(s_logo_tex, LOGO_X0, LOGO_Y0, LOGO_X1, LOGO_Y1,
                  0.0f, 0.0f, 1.0f, 1.0f);
    }
    if (s_overlay_active && !options_open) {
        const float v0 = (float)(s_selection * CANVAS_H) / (float)ART_TEX_H;
        const float v1 = (float)((s_selection + 1) * CANVAS_H) /
                         (float)ART_TEX_H;
        draw_quad(s_art_tex, 0.0f, 0.0f, CANVAS_W, CANVAS_H,
                  0.0f, v0, 1.0f, v1);
    }
    if (s_options_alpha) {
        const float v0 = (float)(STATE_OPTIONS * CANVAS_H) / (float)ART_TEX_H;
        const float v1 = (float)((STATE_OPTIONS + 1) * CANVAS_H) /
                         (float)ART_TEX_H;
        glUniform1f(s_uni_alpha, (float)s_options_alpha / 255.0f);
        draw_quad(s_art_tex, 0.0f, 0.0f, CANVAS_W, CANVAS_H,
                  0.0f, v0, 1.0f, v1);
        if (vx > wide_x) {
            /* The gutters (patch 0085): one texel of the screen's uniformly
             * dimmed corner over each, as the PC overlay does. */
            const float u = 0.5f / (float)CANVAS_W;
            const float v = ((float)(STATE_OPTIONS * CANVAS_H) + 0.5f) /
                            (float)ART_TEX_H;

            glViewport(wide_x, vy, vx - wide_x, vh);
            draw_quad(s_art_tex, 0.0f, 0.0f, CANVAS_W, CANVAS_H, u, v, u, v);
            glViewport(vx + vw, vy, wide_x + wide_w - (vx + vw), vh);
            draw_quad(s_art_tex, 0.0f, 0.0f, CANVAS_W, CANVAS_H, u, v, u, v);
            glViewport(vx, vy, vw, vh);
        }
    }

    if (dialog) {
        if (!glIsTexture(s_dialog_tex)) {
            s_dialog_tex = make_texture(DW3_DIALOG_W, DW3_DIALOG_H, NULL);
            s_dialog_dirty = 1;
        }
        if (s_dialog_dirty) {
            uint8_t *pixels = (uint8_t *)malloc((size_t)DW3_DIALOG_W *
                                                DW3_DIALOG_H * 4);
            if (pixels) {
                dw3_pc_dialog_art(pixels, DW3_DIALOG_W * 4, s_dialog_kind,
                                  s_dialog_focus);
                glBindTexture(GL_TEXTURE_2D, s_dialog_tex);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, DW3_DIALOG_W,
                                DW3_DIALOG_H, GL_RGBA, GL_UNSIGNED_BYTE,
                                pixels);
                free(pixels);
                s_dialog_dirty = 0;
            }
        }
        if (!s_dialog_dirty) {
            glUniform1f(s_uni_sub, 0.0f);
            glUniform1f(s_uni_alpha, 1.0f);
            draw_quad(s_dialog_tex, DW3_DIALOG_X, DW3_DIALOG_Y,
                      DW3_DIALOG_X + DW3_DIALOG_W,
                      DW3_DIALOG_Y + DW3_DIALOG_H, 0.0f, 0.0f, 1.0f, 1.0f);
        }
    }

    glDisable(GL_BLEND);
}

void dw3_nx_title_overlay_invalidate_gl(void) {
    if (s_prog) glDeleteProgram(s_prog);
    if (s_art_tex) glDeleteTextures(1, &s_art_tex);
    if (s_logo_tex) glDeleteTextures(1, &s_logo_tex);
    if (s_dialog_tex) glDeleteTextures(1, &s_dialog_tex);
    if (s_vbo) glDeleteBuffers(1, &s_vbo);
    s_prog = 0;
    s_art_tex = 0;
    s_logo_tex = 0;
    s_dialog_tex = 0;
    s_dialog_dirty = 1;
    s_vbo = 0;
    /* Dock/handheld context change: the new texture needs the whole canvas. */
    dw3_options_cache_invalidate();
}

/* --- Hooks called by port/options (same names as the PC overlay) --- */

int dw3_pc_title_menu_set_logo_rgba(const uint8_t *rgba, int width,
                                    int height, int pitch) {
    if (!rgba || width != LOGO_W || height != LOGO_H || pitch < width * 4) {
        return -1;
    }
    if (!s_logo_rgba) {
        s_logo_rgba = (uint8_t *)malloc((size_t)LOGO_W * LOGO_H * 4);
        if (!s_logo_rgba) {
            return -1;
        }
    }
    for (int y = 0; y < LOGO_H; ++y) {
        memcpy(s_logo_rgba + (size_t)y * LOGO_W * 4,
               rgba + (size_t)y * (size_t)pitch, (size_t)LOGO_W * 4);
    }
    s_logo_dirty = 1;
    TITLE_LOG("dw3-title-logo: resampled logo %dx%d received", width, height);
    return 0;
}

/* 0 once the GLES program or canvas failed to build; the port then keeps the
 * game's own credit sprites. */
int dw3_pc_title_menu_overlay_usable(void) {
    return !s_gl_failed;
}

void dw3_pc_title_menu_logo_set_active(int active) {
    const int next = active && s_logo_rgba != NULL;

    if (next != s_logo_active) {
        TITLE_LOG("dw3-title-logo: active=%d", next);
    }
    s_logo_active = next;
}

void dw3_pc_title_menu_set_fade(int level) {
    s_fade = level < 0 ? 0 : (level > 255 ? 255 : level);
}

void dw3_pc_title_menu_set_state(int active, int selected) {
    /* While the options screen is up the title draws the menu with -2; keep
     * its last focus so the menu returns as the screen fades out. */
    if (dw3_port_options_is_open()) {
        return;
    }
    if (!active || selected == -2) {
        s_overlay_active = 0;
        return;
    }
    if (selected < 0) {
        s_selection = 0;
    } else if (selected >= DW3_TITLE_MENU_ROWS) {
        s_selection = DW3_TITLE_MENU_ROWS;
    } else {
        s_selection = selected + 1;
    }
    /* No fade reset here: the menu keeps drawing for a frame after the fade
     * ends, and a reset would flash it at full brightness over black. The
     * title task resets the fade when a new visit starts. */
    s_overlay_active = 1;
}

/* P6.27 / P6.28: the Yes / No window (port/options/confirm.c). Rasterized and
 * uploaded by the draw, only when its kind, focus or language changed. */
void dw3_pc_dialog_set(int active, int kind, int focus) {
    const unsigned language = dw3_pc_dialog_language_generation();

    if (!active) {
        s_dialog_active = 0;
        return;
    }
    if (kind != s_dialog_kind || focus != s_dialog_focus ||
        language != s_dialog_language || !s_dialog_active) {
        s_dialog_kind = kind;
        s_dialog_focus = focus;
        s_dialog_language = language;
        s_dialog_dirty = 1;
    }
    s_dialog_active = 1;
}
