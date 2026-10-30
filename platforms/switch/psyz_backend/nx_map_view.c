/* P6.32 phase 0: the full-screen map screen on the Switch GLES present.
 *
 * The same picture as the PC SDL3 GPU overlay (platforms/pc/src/
 * map_view_overlay.c): a backdrop over the whole 16:9 picture (the map's own
 * colours, dimmed, from a 32 x 18 texture the linear filter stretches; black
 * when there is none), the map
 * texture (the captured map, or a CPU-scaled copy) placed and zoomed by the
 * quad's own rectangle with a scissor to the picture, then the HUD canvas.
 * State, scalers and the pad are in port/map_view/. Drawn after the game
 * composite and before the title overlay. Textures live in the GLES driver's
 * libc heap and are released 30 draws after the screen closes. */
#ifdef DW3_NX_GAME_IMAGE
#include "nx_map_view.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <GLES3/gl3.h>

#include "map_view.h"
#include "nx_game_api.h" /* last: a `both` image redirects the game calls */

void psyz_log(unsigned int level, const char *file, unsigned int line,
              const char *func, const char *fmt, ...);
#define MV_LOG(...) psyz_log(1, "nx_map_view.c", __LINE__, __func__, __VA_ARGS__)

static GLuint s_prog, s_vbo, s_map_tex, s_hud_tex, s_backdrop_tex;
static GLuint s_bank_tex[DW3_MV_BANK_N]; /* the game's cursor and balloon */
static unsigned s_bank_gen = ~0u;
static GLint s_attr_pos = -1, s_attr_t = -1, s_uni_tex = -1;
static unsigned s_map_gen = ~0u, s_hud_gen = ~0u, s_backdrop_gen = ~0u;
static int s_map_w, s_map_h, s_idle, s_failed, s_linear = -1, s_logged;
static GLenum s_last_draw_err;
static int s_skip_logged; /* "waiting for the map texture" once per open */

static long long now_us(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

static GLuint compile(GLenum type, const char *src) {
    GLuint sh = glCreateShader(type);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char msg[200];

        msg[0] = 0;
        glGetShaderInfoLog(sh, sizeof(msg), NULL, msg);
        MV_LOG("dw3-map: switch shader compile FAILED (%s): %s",
               type == GL_VERTEX_SHADER ? "vertex" : "fragment", msg);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static int ensure_program(void) {
    static const char *VS =
        "attribute vec2 a_pos;\nattribute vec2 a_t;\nvarying vec2 v_t;\n"
        "void main(){v_t=a_t;gl_Position=vec4(a_pos,0.0,1.0);}\n";
    static const char *FS =
        "precision mediump float;\nvarying vec2 v_t;\nuniform sampler2D u_tex;\n"
        "void main(){gl_FragColor=texture2D(u_tex,v_t);}\n";
    GLuint vs, fs;
    GLint ok = 0;

    if (s_prog && s_vbo) return 1;
    if (s_failed) return 0;
    vs = compile(GL_VERTEX_SHADER, VS);
    fs = compile(GL_FRAGMENT_SHADER, FS);
    if (!vs || !fs) {
        MV_LOG("dw3-map: switch program not built: map view disabled");
        s_failed = 1;
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
        MV_LOG("dw3-map: switch program link FAILED: map view disabled");
        glDeleteProgram(s_prog);
        s_prog = 0;
        s_failed = 1;
        return 0;
    }
    s_attr_pos = glGetAttribLocation(s_prog, "a_pos");
    s_attr_t = glGetAttribLocation(s_prog, "a_t");
    s_uni_tex = glGetUniformLocation(s_prog, "u_tex");
    MV_LOG("dw3-map: switch program ready (attr pos=%d t=%d, uniform tex=%d)",
           s_attr_pos, s_attr_t, s_uni_tex);
    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, NULL, GL_DYNAMIC_DRAW);
    return 1;
}

static GLuint make_texture(int w, int h, GLint filter) {
    GLuint tex = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 NULL);
    return tex;
}

static void release_textures(void) {
    int i;

    if (s_map_tex) glDeleteTextures(1, &s_map_tex);
    if (s_hud_tex) glDeleteTextures(1, &s_hud_tex);
    if (s_backdrop_tex) glDeleteTextures(1, &s_backdrop_tex);
    s_backdrop_tex = 0;
    s_backdrop_gen = ~0u;
    for (i = 0; i < DW3_MV_BANK_N; ++i) {
        if (s_bank_tex[i]) glDeleteTextures(1, &s_bank_tex[i]);
        s_bank_tex[i] = 0;
    }
    s_bank_gen = ~0u;
    s_map_tex = s_hud_tex = 0;
    s_map_gen = s_hud_gen = ~0u;
    s_map_w = s_map_h = 0;
    s_linear = -1;
}

void dw3_nx_map_view_invalidate_gl(void) {
    /* A recreated context left every name dangling; just forget them. */
    memset(s_bank_tex, 0, sizeof(s_bank_tex));
    s_bank_gen = ~0u;
    s_prog = s_vbo = s_map_tex = s_hud_tex = s_backdrop_tex = 0;
    s_backdrop_gen = ~0u;
    s_map_gen = s_hud_gen = ~0u;
    s_map_w = s_map_h = 0;
    s_linear = -1;
    s_failed = 0;
}

static void draw_quad(GLuint tex, float x0, float y0, float x1, float y1) {
    const float l = x0 * 2.0f - 1.0f, r = x1 * 2.0f - 1.0f;
    const float t = 1.0f - y0 * 2.0f, b = 1.0f - y1 * 2.0f;
    const float quad[16] = {
        l, b, 0.0f, 1.0f,
        r, b, 1.0f, 1.0f,
        l, t, 0.0f, 0.0f,
        r, t, 1.0f, 0.0f,
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

/* One strip of the picture (GL window rectangle sx,sy,sw,sh): the backdrop,
 * the map, the game's cursor and balloon on it (map space), the HUD canvas. */
static void draw_layers(const Dw3MapViewDraw *d, int sx, int sy, int sw, int sh) {
    int i;

    if (sw <= 0 || sh <= 0) return;
    glScissor(sx, sy, sw, sh);
    glDisable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (d->backdrop_rgba && s_backdrop_tex)
        draw_quad(s_backdrop_tex, 0.0f, 0.0f, 1.0f, 1.0f); /* linear, the whole picture */
    draw_quad(s_map_tex, d->x0, d->y0, d->x1, d->y1);
    glEnable(GL_BLEND);
    for (i = 0; i < d->nspr; ++i) {
        const Dw3MapViewSprite *sp = &d->spr[i];

        if (sp->bank < 0 || sp->bank >= DW3_MV_BANK_N || !s_bank_tex[sp->bank])
            continue;
        draw_quad(s_bank_tex[sp->bank], sp->x0, sp->y0, sp->x1, sp->y1);
    }
    draw_quad(s_hud_tex, 0.0f, 0.0f, 1.0f, 1.0f);
}

void dw3_nx_map_view_draw(int vx, int vy, int vw, int vh) {
    Dw3MapViewDraw d;

    dw3_map_view_get_draw(&d);
    if (!d.active) {
        if ((s_map_tex || s_hud_tex) && ++s_idle >= 30) {
            release_textures();
            MV_LOG("dw3-map: switch textures released");
        }
        s_logged = 0;
        s_skip_logged = 0;
        return;
    }
    s_idle = 0;
    if (vw <= 0 || vh <= 0 || !ensure_program()) return;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                        GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);
    if (!s_hud_tex) {
        s_hud_tex = make_texture(DW3_MV_HUD_W, DW3_MV_HUD_H, GL_NEAREST);
        s_hud_gen = ~0u;
    }
    if (d.tex_gen != s_map_gen && d.tex_rgba && d.tex_w > 0) {
        if (s_map_tex && (s_map_w != d.tex_w || s_map_h != d.tex_h)) {
            glDeleteTextures(1, &s_map_tex);
            s_map_tex = 0;
        }
        if (!s_map_tex) {
            s_map_tex = make_texture(d.tex_w, d.tex_h, GL_LINEAR);
            s_map_w = d.tex_w;
            s_map_h = d.tex_h;
            s_linear = 1;
        }
        glBindTexture(GL_TEXTURE_2D, s_map_tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        {
            GLint maxtex = 0;
            GLenum err;
            long long t0;

            long long clear_a = 0, i;

            for (i = 0; i < (long long)d.tex_w * d.tex_h; ++i) {
                if (d.tex_rgba[i * 4 + 3] != 255) ++clear_a;
            }
            MV_LOG("dw3-map: switch texture gen %u alpha check: %lld of %lld "
                   "texels are not opaque (255)", d.tex_gen, clear_a,
                   (long long)d.tex_w * d.tex_h);
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtex);
            (void)glGetError(); /* drop anything a neighbour left behind */
            t0 = now_us();
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, d.tex_w, d.tex_h, GL_RGBA,
                            GL_UNSIGNED_BYTE, d.tex_rgba);
            err = glGetError();
            MV_LOG("dw3-map: switch texture %dx%d upload %s (GL error 0x%x, "
                   "max texture %d, tex id %u, %lld us, %s)", d.tex_w, d.tex_h,
                   err == GL_NO_ERROR ? "ok" : "FAILED", (unsigned)err,
                   (int)maxtex, (unsigned)s_map_tex, now_us() - t0,
                   d.linear ? "linear" : "nearest");
        }
        s_map_gen = d.tex_gen;
        dw3_map_view_tex_taken(d.tex_gen);
    }
    if (d.hud_gen != s_hud_gen && d.hud_rgba && s_hud_tex) {
        glBindTexture(GL_TEXTURE_2D, s_hud_tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, DW3_MV_HUD_W, DW3_MV_HUD_H,
                        GL_RGBA, GL_UNSIGNED_BYTE, d.hud_rgba);
        s_hud_gen = d.hud_gen;
    }
    if (d.backdrop_gen != s_backdrop_gen) {
        if (d.backdrop_rgba) {
            if (!s_backdrop_tex)
                s_backdrop_tex = make_texture(DW3_MV_BACKDROP_W, DW3_MV_BACKDROP_H, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, s_backdrop_tex);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, DW3_MV_BACKDROP_W,
                            DW3_MV_BACKDROP_H, GL_RGBA, GL_UNSIGNED_BYTE,
                            d.backdrop_rgba);
            MV_LOG("dw3-map: switch backdrop %dx%d uploaded (gen %u, linear, GL "
                   "error 0x%x)", DW3_MV_BACKDROP_W, DW3_MV_BACKDROP_H,
                   d.backdrop_gen, (unsigned)glGetError());
        }
        s_backdrop_gen = d.backdrop_gen;
    }
    if (d.bank_gen != s_bank_gen) {
        int i;

        for (i = 0; i < DW3_MV_BANK_N; ++i) {
            if (s_bank_tex[i]) glDeleteTextures(1, &s_bank_tex[i]);
            s_bank_tex[i] = 0;
            if (!d.bank[i].rgba || d.bank[i].w <= 0) continue;
            s_bank_tex[i] = make_texture(d.bank[i].w, d.bank[i].h, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, s_bank_tex[i]);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, d.bank[i].w, d.bank[i].h,
                            GL_RGBA, GL_UNSIGNED_BYTE, d.bank[i].rgba);
        }
        s_bank_gen = d.bank_gen;
        MV_LOG("dw3-map: switch sprite bank uploaded (gen %u, GL error 0x%x)",
               d.bank_gen, (unsigned)glGetError());
    }
    if (!s_map_tex || s_map_gen != d.tex_gen) {
        if (!s_skip_logged) {
            s_skip_logged = 1;
            MV_LOG("dw3-map: switch map not drawn: no texture yet (tex id %u, "
                   "gen have %u want %u, cpu image %s) - only the game's own "
                   "background shows", (unsigned)s_map_tex, s_map_gen,
                   d.tex_gen, d.tex_rgba ? "present" : "missing");
        }
        return;
    }
    if (s_linear != d.linear) {
        const GLint f = d.linear ? GL_LINEAR : GL_NEAREST;

        glBindTexture(GL_TEXTURE_2D, s_map_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
        s_linear = d.linear;
    }

    {
        /* Whatever the mesh or the title overlay left enabled: a stray vertex
         * array without a buffer makes glDrawArrays fail with
         * INVALID_OPERATION, which would show exactly as "only the
         * background". Start from a known state. */
        GLint i, maxattr = 0, vao = 0, fbo = 0, prog = 0;

        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxattr);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
        glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
        if (!s_logged) {
            MV_LOG("dw3-map: switch draw state before: vao=%d fbo=%d "
                   "program=%d (ours %u), max attribs %d", (int)vao, (int)fbo,
                   (int)prog, (unsigned)s_prog, (int)maxattr);
        }
        for (i = 0; i < maxattr && i < 16; ++i) {
            glDisableVertexAttribArray((GLuint)i);
        }
        (void)glGetError();
    }
    glViewport(vx, vy, vw, vh);
    glEnable(GL_SCISSOR_TEST);
    glUseProgram(s_prog);
    glUniform1i(s_uni_tex, 0);
    if (d.hole) {
        /* The game's name box (its frame and text window) is drawn by the game
         * into its own picture: the rectangle is left alone and the rest is
         * drawn in up to four strips around it (GL rows run bottom up). */
        int hx0 = vx + (int)(d.hx0 * (float)vw + 0.5f);
        int hx1 = vx + (int)(d.hx1 * (float)vw + 0.5f);
        int hy0 = vy + vh - (int)(d.hy1 * (float)vh + 0.5f); /* bottom edge */
        int hy1 = vy + vh - (int)(d.hy0 * (float)vh + 0.5f); /* top edge */

        if (hx0 < vx) hx0 = vx;
        if (hx1 > vx + vw) hx1 = vx + vw;
        if (hy0 < vy) hy0 = vy;
        if (hy1 > vy + vh) hy1 = vy + vh;
        draw_layers(&d, vx, vy, vw, hy0 - vy);                  /* below */
        draw_layers(&d, vx, hy1, vw, vy + vh - hy1);            /* above */
        draw_layers(&d, vx, hy0, hx0 - vx, hy1 - hy0);          /* left */
        draw_layers(&d, hx1, hy0, vx + vw - hx1, hy1 - hy0);    /* right */
    } else {
        draw_layers(&d, vx, vy, vw, vh);
    }
    glDisable(GL_SCISSOR_TEST);
    {
        const GLenum derr = glGetError();

        if (derr != GL_NO_ERROR && derr != s_last_draw_err) {
            MV_LOG("dw3-map: switch draw GL error 0x%x (map tex %u hud tex %u)",
                   (unsigned)derr, (unsigned)s_map_tex, (unsigned)s_hud_tex);
        }
        s_last_draw_err = derr;
    }
    if (!s_logged) {
        s_logged = 1;
        MV_LOG("dw3-map: switch first draw viewport %d,%d %dx%d, map rect "
               "%.3f,%.3f-%.3f,%.3f, GL error 0x%x", vx, vy, vw, vh,
               (double)d.x0, (double)d.y0, (double)d.x1, (double)d.y1,
               (unsigned)glGetError());
    }
}
#endif /* DW3_NX_GAME_IMAGE */
