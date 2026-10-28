/* GLES corner HUD. Empty object file when DW3_NX_DEBUG_OVERLAY is off. */
#include "nx_debug_overlay.h"

#if DW3_NX_DEBUG_OVERLAY

#include "nx.h"
#include "nx_battle_mode.h"
#include "nx_mesh.h"
#include "nx_mesh_debug.h"
#include "nx_vram_dump.h"
#include "overlay_host.h"
#include "nx_game_api.h" /* last: a `both` image redirects the game calls */
#include "../source/debug_overlay_font.h"
#include "../source/debug_overlay_glyph.h"

#include <stdio.h>
#include <string.h>

#include <GLES3/gl3.h>
#include <switch.h>

enum {
    FONT_W = 8,
    FONT_H = 8,
    /* Logical padding / gaps (texture pixels); scaled with the glyphs. */
    HUD_PAD_X = 4,
    HUD_PAD_Y = 4,
    HUD_LINE_GAP = 2,
    HUD_COLS = DW3_DBG_OVERLAY_LINE_CHARS,
    HUD_ROWS = DW3_DBG_OVERLAY_MAX_LINES,
    HUD_TEX_W = HUD_PAD_X * 2 + HUD_COLS * FONT_W,
    HUD_TEX_H = HUD_PAD_Y * 2 + HUD_ROWS * FONT_H +
                (HUD_ROWS > 1 ? (HUD_ROWS - 1) * HUD_LINE_GAP : 0),
    /* Minus + L + R — see docs/DEBUG_OVERLAY.md */
    TOGGLE_CHORD = HidNpadButton_Minus | HidNpadButton_L | HidNpadButton_R
};

/* Integer NN scale: ~2× at 720p (16px glyphs), ~3× at 1080p. */
static int hud_scale_for_fb(uint32_t fb_w, uint32_t fb_h, int content_w,
                            int content_h) {
    int scale = (int)((fb_h * 2u) / 720u);

    if (scale < 2) {
        scale = 2;
    }
    if (scale > 6) {
        scale = 6;
    }
    while (scale > 1 &&
           (content_w * scale > (int)fb_w || content_h * scale > (int)fb_h)) {
        scale--;
    }
    return scale;
}

static int s_visible = 1; /* default ON for nxlink builds */
static int s_chord_was_down;
static uint64_t s_dt_us;
static int s_use_mesh;
static Dw3DebugOverlayMeshSnap s_mesh;
static Dw3DebugOverlayMemSnap s_mem;
static Dw3DebugOverlayMenuSnap s_menu;
static char s_lines[HUD_ROWS][DW3_DBG_OVERLAY_LINE_CHARS];
static int s_line_count;
static int s_dirty = 1;

static GLuint s_prog;
static GLuint s_tex;
static GLuint s_vbo;
static GLint s_attr_pos = -1;
static GLint s_attr_t = -1;
static GLint s_uni_tex = -1;
static uint8_t s_pixels[HUD_TEX_W * HUD_TEX_H * 4];

static GLuint compile_shader(GLenum type, const char *src) {
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
        "void main(){\n"
        "  vec4 c=texture2D(u_tex,v_t);\n"
        "  if(c.a<0.5) discard;\n"
        "  gl_FragColor=c;\n"
        "}\n";
    GLuint vs;
    GLuint fs;
    GLint ok = 0;
    static const float quad_init[] = {
        -1.f, -1.f, 0.f, 1.f, 1.f, -1.f, 1.f, 1.f,
        -1.f, 1.f,  0.f, 0.f, 1.f,  1.f, 1.f, 0.f,
    };

    if (s_prog && glIsTexture(s_tex)) {
        return 1;
    }

    vs = compile_shader(GL_VERTEX_SHADER, VS);
    fs = compile_shader(GL_FRAGMENT_SHADER, FS);
    if (!vs || !fs) {
        if (vs) {
            glDeleteShader(vs);
        }
        if (fs) {
            glDeleteShader(fs);
        }
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
        return 0;
    }
    s_attr_pos = glGetAttribLocation(s_prog, "a_pos");
    s_attr_t = glGetAttribLocation(s_prog, "a_t");
    s_uni_tex = glGetUniformLocation(s_prog, "u_tex");

    glGenTextures(1, &s_tex);
    glBindTexture(GL_TEXTURE_2D, s_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, HUD_TEX_W, HUD_TEX_H, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);

    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_init), quad_init, GL_DYNAMIC_DRAW);
    return 1;
}

void dw3_nx_debug_overlay_invalidate_gl(void) {
    if (s_prog) {
        glDeleteProgram(s_prog);
        s_prog = 0;
    }
    if (s_tex) {
        glDeleteTextures(1, &s_tex);
        s_tex = 0;
    }
    if (s_vbo) {
        glDeleteBuffers(1, &s_vbo);
        s_vbo = 0;
    }
    s_dirty = 1;
}

static void put_glyph(int col, int row, char ch) {
    const uint8_t *glyph;
    int gx;
    int gy;
    int px;
    int py;

    if (col < 0 || col >= HUD_COLS || row < 0 || row >= HUD_ROWS) {
        return;
    }
    glyph = dw3_font8x8_glyph(ch);
    for (gy = 0; gy < FONT_H; gy++) {
        uint8_t bits = glyph[gy];
        for (gx = 0; gx < FONT_W; gx++) {
            px = HUD_PAD_X + col * FONT_W + gx;
            py = HUD_PAD_Y + row * (FONT_H + HUD_LINE_GAP) + gy;
            uint8_t *p = &s_pixels[(py * HUD_TEX_W + px) * 4];
            /* font8x8: LSB is left. MSB-as-left mirrored Latin on OLED. */
            if (dw3_dbg_glyph_pixel(bits, gx)) {
                p[0] = 0xE8;
                p[1] = 0xF0;
                p[2] = 0xFF;
                p[3] = 0xFF;
            } else {
                p[0] = 0;
                p[1] = 0;
                p[2] = 0;
                p[3] = 0;
            }
        }
    }
}

static void rebuild_pixels(void) {
    Dw3DebugOverlayBuildFlags build;
    int row;
    int col;
    const char *s;

    memset(s_pixels, 0, sizeof(s_pixels));
    build.nxlink =
#ifdef DW3_NXLINK_STDIO
        1
#else
        0
#endif
        ;
    build.mesh_dbg =
#if DW3_NX_DEBUG_MESH
        1
#else
        0
#endif
        ;
    build.dump_vram =
#if DW3_NX_DUMP_VRAM
        1
#else
        0
#endif
        ;
    build.mem_dbg =
#if DW3_NX_DEBUG_MEM
        1
#else
        0
#endif
        ;

    s_line_count = dw3_debug_overlay_format(
        s_lines, HUD_ROWS, s_dt_us, dw3_overlay_current_mode_file(), s_use_mesh,
        dw3_nx_battle_mode_token(), &s_mesh, &s_mem, &s_menu, &build);

    for (row = 0; row < s_line_count; row++) {
        s = s_lines[row];
        for (col = 0; s[col] != '\0' && col < HUD_COLS; col++) {
            put_glyph(col, row, s[col]);
        }
    }
    /* Soft panel behind glyphs so text stays readable on bright frames. */
    {
        int panel_h = HUD_PAD_Y * 2;
        if (s_line_count > 0) {
            panel_h += s_line_count * FONT_H +
                       (s_line_count > 1 ? (s_line_count - 1) * HUD_LINE_GAP : 0);
        } else {
            panel_h += FONT_H;
        }
        if (panel_h > HUD_TEX_H) {
            panel_h = HUD_TEX_H;
        }
        for (row = 0; row < panel_h; row++) {
            for (col = 0; col < HUD_TEX_W; col++) {
                uint8_t *p = &s_pixels[(row * HUD_TEX_W + col) * 4];
                if (p[3] == 0) {
                    p[0] = 0x08;
                    p[1] = 0x10;
                    p[2] = 0x20;
                    p[3] = 0xC0;
                }
            }
        }
    }
    s_dirty = 0;
}

static void poll_toggle(void) {
    u64 held = dw3_nx_input_buttons_raw();
    int down = ((held & TOGGLE_CHORD) == TOGGLE_CHORD) ? 1 : 0;

    if (down && !s_chord_was_down) {
        s_visible = !s_visible;
    }
    s_chord_was_down = down;
}

void dw3_nx_debug_overlay_note_sample(uint64_t dt_us,
                                      const Dw3DebugOverlayMeshSnap *mesh) {
    s_dt_us = dt_us;
    if (mesh) {
        s_mesh = *mesh;
    } else {
        memset(&s_mesh, 0, sizeof(s_mesh));
    }
    s_dirty = 1;
}

#if DW3_NX_DEBUG_MEM
void dw3_nx_debug_overlay_note_mem(const Dw3NxMemBudgetSample *mem) {
    if (!mem) {
        memset(&s_mem, 0, sizeof(s_mem));
        s_dirty = 1;
        return;
    }
    s_mem.valid = 1;
    s_mem.heap_ok = mem->heap_ok;
    s_mem.heap_used_kib = mem->heap_used_bytes / 1024ull;
    s_mem.heap_arena_kib = mem->heap_arena_bytes / 1024ull;
    s_mem.heap_used_hw_kib = mem->heap_used_hw / 1024ull;
    s_mem.file_cache_kib = mem->file_cache_bytes / 1024ull;
    s_mem.file_cache_hw_kib = mem->file_cache_hw / 1024ull;
    s_mem.libc_used_kib = mem->libc_used_bytes / 1024ull;
    s_mem.libc_used_hw_kib = mem->libc_used_hw / 1024ull;
    s_dirty = 1;
}
#endif

void dw3_nx_debug_overlay_note_menu(const Dw3DebugOverlayMenuSnap *menu) {
    if (menu) {
        s_menu = *menu;
    } else {
        memset(&s_menu, 0, sizeof(s_menu));
    }
    s_dirty = 1;
}

void dw3_nx_debug_overlay_note_present(int use_mesh) {
    if (s_use_mesh != (use_mesh ? 1 : 0)) {
        s_use_mesh = use_mesh ? 1 : 0;
        s_dirty = 1;
    }
}

void dw3_nx_debug_overlay_draw(uint32_t fb_w, uint32_t fb_h) {
    int content_w;
    int content_h;
    int scale;
    int hud_w;
    int hud_h;
    int vy;
    float u1;
    float v1;
    float quad[16];

    if (fb_w < 64 || fb_h < 64) {
        return;
    }

    poll_toggle();
    if (!s_visible) {
        return;
    }
    if (!ensure_gl()) {
        return;
    }

    /* Refresh mode / build lines even between 60-pump samples. */
    s_dirty = 1;
    rebuild_pixels();

    content_w = HUD_TEX_W;
    content_h = HUD_PAD_Y * 2;
    if (s_line_count > 0) {
        content_h += s_line_count * FONT_H +
                     (s_line_count > 1 ? (s_line_count - 1) * HUD_LINE_GAP : 0);
    } else {
        content_h += FONT_H;
    }
    if (content_h > HUD_TEX_H) {
        content_h = HUD_TEX_H;
    }

    scale = hud_scale_for_fb(fb_w, fb_h, content_w, content_h);
    hud_w = content_w * scale;
    hud_h = content_h * scale;
    if (hud_w > (int)fb_w) {
        hud_w = (int)fb_w;
    }
    if (hud_h > (int)fb_h) {
        hud_h = (int)fb_h;
    }
    /* GL viewport origin is bottom-left; pin HUD to the top-left corner. */
    vy = (int)fb_h - hud_h;

    /* Sample only the used atlas region (1:1 texels), then NN-upscale via
     * viewport size * integer scale + GL_NEAREST.
     *
     * UV matches nx_gpu present / mesh blit: clip top-left samples (0,0),
     * clip bottom-right samples (u1,v1). Atlas row 0 is uploaded at GL V=0
     * (TexImage bottom), so v=0 at clip top shows the first HUD line. */
    u1 = (float)content_w / (float)HUD_TEX_W;
    v1 = (float)content_h / (float)HUD_TEX_H;
    quad[0] = -1.f;
    quad[1] = -1.f;
    quad[2] = 0.f;
    quad[3] = v1;
    quad[4] = 1.f;
    quad[5] = -1.f;
    quad[6] = u1;
    quad[7] = v1;
    quad[8] = -1.f;
    quad[9] = 1.f;
    quad[10] = 0.f;
    quad[11] = 0.f;
    quad[12] = 1.f;
    quad[13] = 1.f;
    quad[14] = u1;
    quad[15] = 0.f;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    /* The equation too: other layers (battle mesh) change it. */
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glViewport(0, vy, hud_w, hud_h);
    glBindTexture(GL_TEXTURE_2D, s_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, HUD_TEX_W, HUD_TEX_H, GL_RGBA,
                    GL_UNSIGNED_BYTE, s_pixels);

    glUseProgram(s_prog);
    glUniform1i(s_uni_tex, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad), quad);
    glEnableVertexAttribArray((GLuint)s_attr_pos);
    glVertexAttribPointer((GLuint)s_attr_pos, 2, GL_FLOAT, GL_FALSE,
                          sizeof(float) * 4, (const void *)0);
    glEnableVertexAttribArray((GLuint)s_attr_t);
    glVertexAttribPointer((GLuint)s_attr_t, 2, GL_FLOAT, GL_FALSE,
                          sizeof(float) * 4, (const void *)(sizeof(float) * 2));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisable(GL_BLEND);
    glViewport(0, 0, (GLsizei)fb_w, (GLsizei)fb_h);
}

#endif /* DW3_NX_DEBUG_OVERLAY */
