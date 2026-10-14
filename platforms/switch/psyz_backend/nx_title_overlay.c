/* Native-resolution title menu and logo on the Switch GLES present.
 *
 * Same content as the PC SDL3 GPU overlay (platforms/pc/src/
 * title_menu_overlay.c): a 960x720 transparent canvas in four states
 * (PRESS START, then New Game / Options / Load Game focused), plus the
 * original logo (US, or the EU bank of the current language) resampled from
 * runtime VRAM into a 720x326 texture. EU labels regenerate the canvas.
 * Both are drawn over the game picture's 4:3 viewport after the frame is
 * composited and before the debug HUD. The game loop and the present run on
 * the same thread; the logo is kept on the CPU and uploaded on the next draw
 * so a dock/handheld context loss can rebuild every GL object. */
#include "nx_title_overlay.h"

#include "../../pc/src/title_overlay_art.h"
#include "port_options.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GLES3/gl3.h>

/* nx_log.c: stderr/nxlink, sdmc:/switch/dw3/dw3.log and the debug string. */
void psyz_log(unsigned int level, const char *file, unsigned int line,
              const char *func, const char *fmt, ...);
#define TITLE_LOG(...) psyz_log(1, "nx_title_overlay.c", __LINE__, __func__, \
                                __VA_ARGS__)

enum {
    CANVAS_W = 960,
    CANVAS_H = 720,
    STATE_COUNT = 4,
    ART_TEX_H = CANVAS_H * STATE_COUNT,
    LOGO_W = 720,
    LOGO_H = 326,
    /* Logo placement in canvas pixels, as on PC. */
    LOGO_X0 = 120,
    LOGO_Y0 = 47,
    LOGO_X1 = 840,
    LOGO_Y1 = 373,
};

static GLuint s_prog;
static GLuint s_art_tex;
static GLuint s_logo_tex;
static GLuint s_vbo;
static GLint s_attr_pos = -1;
static GLint s_attr_t = -1;
static GLint s_uni_tex = -1;
static GLint s_uni_sub = -1;
static int s_gl_failed;

static int s_selection;
static int s_overlay_active;
static int s_logo_active;
static uint8_t *s_logo_rgba;
static int s_logo_dirty;
/* Title edge-fade darkness, 0..255 (STDWTITL_tickEdgeFade). */
static int s_fade;
/* dw3_pc_title_overlay_generation() of the uploaded canvas (EU labels). */
static unsigned s_art_generation;

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

/* The four menu states, generated on the CPU (no art is shipped). */
static uint8_t *build_art(void) {
    const size_t canvas_bytes = (size_t)CANVAS_W * CANVAS_H * 4;
    uint8_t *pixels = (uint8_t *)malloc(canvas_bytes * STATE_COUNT);

    if (!pixels) {
        return NULL;
    }
    s_art_generation = dw3_pc_title_overlay_generation();
    for (int state = 0; state < STATE_COUNT; ++state) {
        const int selected = state == 0 ? -1 : state - 1;
        memset(pixels + (size_t)state * canvas_bytes, 0, canvas_bytes);
        dw3_pc_title_overlay_art(pixels + (size_t)state * canvas_bytes,
                                 CANVAS_W * 4, selected);
    }
    return pixels;
}

/* Build the four menu states once per GL context. The canvas is generated on
 * the CPU (no art is shipped) and freed after the upload. */
static int ensure_gl(void) {
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
        "void main(){vec4 c=texture2D(u_tex,v_t);\n"
        "  gl_FragColor=vec4(max(c.rgb-vec3(u_sub),0.0),c.a);}\n";
    uint8_t *pixels;
    GLuint vs;
    GLuint fs;
    GLint ok = 0;

    if (s_prog && glIsTexture(s_art_tex)) {
        /* The EU disc swaps in its own labels after the first upload. */
        if (s_art_generation != dw3_pc_title_overlay_generation()) {
            pixels = build_art();
            if (pixels) {
                glBindTexture(GL_TEXTURE_2D, s_art_tex);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, CANVAS_W, ART_TEX_H,
                                GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                free(pixels);
                TITLE_LOG("dw3-title-overlay: canvas regenerated "
                          "(labels generation %u)", s_art_generation);
            }
        }
        return 1;
    }
    if (s_gl_failed) {
        return 0;
    }
    /* A recreated context left these names dangling; drop them. */
    s_prog = 0;
    s_art_tex = 0;
    s_logo_tex = 0;
    s_vbo = 0;

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

    pixels = build_art();
    if (!pixels) {
        glDeleteProgram(s_prog);
        s_prog = 0;
        s_gl_failed = 1;
        return 0;
    }
    s_art_tex = make_texture(CANVAS_W, ART_TEX_H, pixels);
    free(pixels);
    s_logo_tex = make_texture(LOGO_W, LOGO_H, NULL);
    s_logo_dirty = s_logo_rgba != NULL;

    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, NULL, GL_DYNAMIC_DRAW);
    TITLE_LOG("dw3-title-overlay: %dx%d RGBA texture, %d states (GLES)",
              CANVAS_W, ART_TEX_H, STATE_COUNT);
    return 1;
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
    /* The P9.2 shell draws in the game picture; nothing may cover it. */
    int logo = s_logo_active && s_logo_rgba && !dw3_port_options_is_open();

    if ((!s_overlay_active && !logo) || vw <= 0 || vh <= 0) {
        return;
    }
    if (!ensure_gl()) {
        return;
    }
    /* The title is 4:3 (gutter 0); fit the canvas to it. */
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
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                        GL_ONE_MINUS_SRC_ALPHA);
    glViewport(vx, vy, vw, vh);
    glUseProgram(s_prog);
    glUniform1i(s_uni_tex, 0);
    /* Follow the title's edge fade: it subtracts its level at the centre and
     * twice that at the edges; the overlay spans both, so use 1.5x. */
    glUniform1f(s_uni_sub, (float)s_fade * 1.5f / 255.0f);
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
    if (s_overlay_active) {
        const float v0 = (float)(s_selection * CANVAS_H) / (float)ART_TEX_H;
        const float v1 = (float)((s_selection + 1) * CANVAS_H) /
                         (float)ART_TEX_H;
        draw_quad(s_art_tex, 0.0f, 0.0f, CANVAS_W, CANVAS_H,
                  0.0f, v0, 1.0f, v1);
    }

    glDisable(GL_BLEND);
}

void dw3_nx_title_overlay_invalidate_gl(void) {
    if (s_prog) glDeleteProgram(s_prog);
    if (s_art_tex) glDeleteTextures(1, &s_art_tex);
    if (s_logo_tex) glDeleteTextures(1, &s_logo_tex);
    if (s_vbo) glDeleteBuffers(1, &s_vbo);
    s_prog = 0;
    s_art_tex = 0;
    s_logo_tex = 0;
    s_vbo = 0;
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
    if (dw3_port_options_is_open()) {
        active = 0;
    }
    if (!active || selected == -2) {
        s_overlay_active = 0;
        return;
    }
    if (selected < 0) {
        s_selection = 0;
    } else if (selected >= 2) {
        s_selection = 3;
    } else {
        s_selection = selected + 1;
    }
    /* No fade reset here: the menu keeps drawing for a frame after the fade
     * ends, and a reset would flash it at full brightness over black. The
     * title task resets the fade when a new visit starts. */
    s_overlay_active = 1;
}
