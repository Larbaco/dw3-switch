/* P6.32 phase 0: the full-screen map screen on the Switch GLES present.
 *
 * The same picture as the PC SDL3 GPU overlay (platforms/pc/src/
 * map_view_overlay.c): a black backdrop over the whole 16:9 picture, the map
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

#include <GLES3/gl3.h>

#include "map_view.h"

void psyz_log(unsigned int level, const char *file, unsigned int line,
              const char *func, const char *fmt, ...);
#define MV_LOG(...) psyz_log(1, "nx_map_view.c", __LINE__, __func__, __VA_ARGS__)

static GLuint s_prog, s_vbo, s_map_tex, s_hud_tex;
static GLint s_attr_pos = -1, s_attr_t = -1, s_uni_tex = -1;
static unsigned s_map_gen = ~0u, s_hud_gen = ~0u;
static int s_map_w, s_map_h, s_idle, s_failed, s_linear = -1, s_logged;

static GLuint compile(GLenum type, const char *src) {
    GLuint sh = glCreateShader(type);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
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
        glDeleteProgram(s_prog);
        s_prog = 0;
        s_failed = 1;
        return 0;
    }
    s_attr_pos = glGetAttribLocation(s_prog, "a_pos");
    s_attr_t = glGetAttribLocation(s_prog, "a_t");
    s_uni_tex = glGetUniformLocation(s_prog, "u_tex");
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
    if (s_map_tex) glDeleteTextures(1, &s_map_tex);
    if (s_hud_tex) glDeleteTextures(1, &s_hud_tex);
    s_map_tex = s_hud_tex = 0;
    s_map_gen = s_hud_gen = ~0u;
    s_map_w = s_map_h = 0;
    s_linear = -1;
}

void dw3_nx_map_view_invalidate_gl(void) {
    /* A recreated context left every name dangling; just forget them. */
    s_prog = s_vbo = s_map_tex = s_hud_tex = 0;
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

void dw3_nx_map_view_draw(int vx, int vy, int vw, int vh) {
    Dw3MapViewDraw d;

    dw3_map_view_get_draw(&d);
    if (!d.active) {
        if ((s_map_tex || s_hud_tex) && ++s_idle >= 30) {
            release_textures();
            MV_LOG("dw3-map: switch textures released");
        }
        s_logged = 0;
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
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, d.tex_w, d.tex_h, GL_RGBA,
                        GL_UNSIGNED_BYTE, d.tex_rgba);
        s_map_gen = d.tex_gen;
        dw3_map_view_tex_taken(d.tex_gen);
        MV_LOG("dw3-map: switch texture %dx%d uploaded (%s)", d.tex_w, d.tex_h,
               d.linear ? "linear" : "nearest");
    }
    if (d.hud_gen != s_hud_gen && d.hud_rgba && s_hud_tex) {
        glBindTexture(GL_TEXTURE_2D, s_hud_tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, DW3_MV_HUD_W, DW3_MV_HUD_H,
                        GL_RGBA, GL_UNSIGNED_BYTE, d.hud_rgba);
        s_hud_gen = d.hud_gen;
    }
    if (!s_map_tex || s_map_gen != d.tex_gen) return;
    if (s_linear != d.linear) {
        const GLint f = d.linear ? GL_LINEAR : GL_NEAREST;

        glBindTexture(GL_TEXTURE_2D, s_map_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
        s_linear = d.linear;
    }

    glViewport(vx, vy, vw, vh);
    glEnable(GL_SCISSOR_TEST);
    glScissor(vx, vy, vw, vh);
    glDisable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(s_prog);
    glUniform1i(s_uni_tex, 0);
    draw_quad(s_map_tex, d.x0, d.y0, d.x1, d.y1);
    glEnable(GL_BLEND);
    draw_quad(s_hud_tex, 0.0f, 0.0f, 1.0f, 1.0f);
    glDisable(GL_SCISSOR_TEST);
    if (!s_logged) {
        s_logged = 1;
        MV_LOG("dw3-map: switch draw viewport %d,%d %dx%d", vx, vy, vw, vh);
    }
}
#endif /* DW3_NX_GAME_IMAGE */
