/* P5.2 — GLES3 mesh FBO for OT triangle polys (battle 3D).
 * Shaders adapted from vendor/psyz sdl3_gl.c (ES 3.00). UI stays on CPU VRAM. */
#include "nx_mesh.h"
#include "nx_mesh_debug.h"
#include "nx_output_filter.h"
#include "nx_res_probe.h"
#include "nx_vram_pack.h"
#ifdef DW3_NX_GAME_IMAGE
#include "port_battle_options.h"
#else
/* The devkitPro Makefile sample (host_hello) compiles this unit without the
 * port: nothing is ever held. */
static int dw3_port_battle_options_holding(void) { return 0; }
#endif
#include "nx_game_api.h" /* last: a `both` image redirects the game calls */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GLES3/gl3.h>

#if DW3_NX_DEBUG_MESH
#include <switch.h>

static uint64_t mesh_now_us(void) {
    return armTicksToNs(armGetSystemTick()) / 1000ULL;
}
#endif

/* The probe's clock, independent of DW3_NX_DEBUG_MESH. */
#define probe_now_us() dw3_nx_res_probe_clock_us()

enum { MESH_MAX_VTX = 8192, MESH_MAX_IDX = MESH_MAX_VTX / 4 * 6 };

typedef enum {
    BLEND_ADD = 0,
    BLEND_SUB,
    BLEND_SUB_OPAQUE
} MeshBlend;

static bool s_ready;
static bool s_had_content;
/* P9.7 slice 2: the battle is held under the port Options screen, so no frame
 * draws. The mesh FBO still holds the last picture; the per-frame "had mesh"
 * flag (cleared after every present) is replaced by what it was on the last
 * frame that drew, for as long as the battle is held. */
static bool s_content_before_hold;
/* CPU VRAM rectangle the GL sample texture has not seen yet: [x0,x1)×[y0,y1).
 * Empty when x1 <= x0. LoadImage / ClearImage / MoveImage grow it; the next
 * flush packs and uploads only this box instead of the full 1024×512. */
static int s_dirty_x0;
static int s_dirty_y0;
static int s_dirty_x1 = DW3_NX_VRAM_W;
static int s_dirty_y1 = DW3_NX_VRAM_H;
static uint64_t s_prims_flushed;

static int s_off_x;
static int s_off_y;
static int s_area_x0;
static int s_area_y0;
static int s_area_x1 = DW3_NX_VRAM_W - 1;
static int s_area_y1 = DW3_NX_VRAM_H - 1;
static GLuint s_mesh_tex;
static GLuint s_mesh_fbo;
/* P6.1 side gutters: a second VRAM-sized FBO where a gutter draw lands
 * shifted so the display's left edge is at x = s_gutter_px. */
static GLuint s_gutter_tex;
static GLuint s_gutter_fbo;
static int s_gutter_px;
static int s_gutter_x0;
static int s_gutter_w;
static GLuint s_sample_tex;
/* P6.11 phase 0 (test aid, docs/P6.11_INTERNAL_RES.md): the mesh and gutter
 * targets are s_scale times the VRAM size. The vertices, the draw offset and
 * the OT order do not change: the vertex shader writes NDC across the whole
 * target, so only the viewport, the scissors, the clears and the gutter blit
 * are multiplied. 1 (the default) is the shipped path. */
enum { MESH_SCALE_MAX = 3 };
static int s_scale = 1;
static int s_scale_want = 1;
static int s_filter_linear;
static int s_dither_ps1;
static GLuint s_prog;
static GLuint s_blit_prog;
static GLuint s_vbo;
static GLuint s_ebo;
static GLuint s_blit_vbo;
static GLint s_u_draw_offset = -1;
static GLint s_u_tex_vram = -1;
static GLint s_u_dither_div = -1;
static GLint s_blit_uv0 = -1;
static GLint s_blit_uv1 = -1;
static GLint s_blit_tex = -1;
static GLint s_blit_pos = -1;
static GLint s_blit_t = -1;
/* Output filter (nx_output_filter.h): the blit program with a bilinear or sharp
 * lookup, built the first time a filter is wanted (battle only). */
static GLuint s_blit_prog_f;
static GLint s_blit_f_uv0 = -1;
static GLint s_blit_f_uv1 = -1;
static GLint s_blit_f_tex = -1;
static GLint s_blit_f_flt = -1;
static int s_blit_f_failed;
static int s_out_filter; /* DW3_NX_FILTER_* for the next dw3_nx_mesh_present */

static Dw3NxMeshVertex s_vtx[MESH_MAX_VTX];
static uint16_t s_idx[MESH_MAX_IDX];
static int s_n_vtx;
static int s_n_idx;

static uint8_t s_pack_rgba[DW3_NX_VRAM_W * DW3_NX_VRAM_H * 4];

static const char *MESH_VS =
    "#version 300 es\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "layout(location = 0) in vec2 a_pos;\n"
    "layout(location = 1) in vec4 a_tex;\n"
    "layout(location = 2) in vec4 a_color;\n"
    "layout(location = 3) in vec4 a_twin;\n"
    "uniform vec2 u_drawOffset;\n"
    "out vec4 v_color;\n"
    "out vec2 v_rawUV;\n"
    "flat out uint v_tpage;\n"
    "flat out uint v_clut;\n"
    "flat out uint v_textureMode;\n"
    "flat out uint v_subPixelMask;\n"
    "flat out uint v_texelShift;\n"
    "flat out uint v_indexShift;\n"
    "flat out uint v_indexMask;\n"
    "flat out uint v_dither;\n"
    "flat out ivec2 v_pageBase;\n"
    "flat out uvec4 v_texWindow;\n"
    "void main() {\n"
    "  float x = ((a_pos.x + u_drawOffset.x) / 512.0) - 1.0;\n"
    "  float y = ((a_pos.y + u_drawOffset.y) / 256.0) - 1.0;\n"
    /* Same GL FBO convention as PSY-Z sdl3_gl: PS1 y=0 → NDC -1 (FB bottom),
     * which matches the CPU VRAM present UV (top of screen samples low t). */
    "  gl_Position = vec4(x, y, 0.0, 1.0);\n"
    "  v_color = a_color;\n"
    "  v_clut = uint(a_tex.z);\n"
    "  uint texWord = uint(a_tex.w);\n"
    "  v_tpage = texWord & 0x1FFu;\n"
    "  v_dither = (texWord & 0x4000u) != 0u ? 1u : 0u;\n"
    "  v_rawUV = a_tex.xy;\n"
    "  v_subPixelMask = 0u;\n"
    "  v_texelShift = 0u;\n"
    "  v_indexShift = 0u;\n"
    "  v_indexMask = 0u;\n"
    "  if ((texWord & 0x8000u) != 0u) {\n"
    "    v_textureMode = 0u;\n"
    "  } else if ((v_tpage & 0x180u) >= 0x100u) {\n"
    "    v_textureMode = 1u;\n"
    "    v_color.rgb *= 2.0;\n"
    "  } else {\n"
    "    v_textureMode = 2u;\n"
    "    v_color.rgb *= 2.0;\n"
    "    if ((v_tpage & 0x80u) != 0u) {\n"
    "      v_subPixelMask = 1u; v_texelShift = 1u;\n"
    "      v_indexShift = 8u; v_indexMask = 0xFFu;\n"
    "    } else {\n"
    "      v_subPixelMask = 3u; v_texelShift = 2u;\n"
    "      v_indexShift = 4u; v_indexMask = 0xFu;\n"
    "    }\n"
    "  }\n"
    "  v_pageBase = ivec2(int((v_tpage % 32u) % 16u) * 64,\n"
    "                     int((v_tpage % 32u) / 16u) * 256);\n"
    "  v_texWindow = uvec4(a_twin);\n"
    "}\n";

static const char *MESH_FS =
    "#version 300 es\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "precision highp sampler2D;\n"
    "in vec4 v_color;\n"
    "in vec2 v_rawUV;\n"
    "flat in uint v_tpage;\n"
    "flat in uint v_clut;\n"
    "flat in uint v_textureMode;\n"
    "flat in uint v_subPixelMask;\n"
    "flat in uint v_texelShift;\n"
    "flat in uint v_indexShift;\n"
    "flat in uint v_indexMask;\n"
    "flat in uint v_dither;\n"
    "flat in ivec2 v_pageBase;\n"
    "flat in uvec4 v_texWindow;\n"
    "uniform sampler2D u_texVram;\n"
    "uniform float u_ditherDiv;\n"
    "out vec4 FragColor;\n"
    /* No dFdx/dFdy: Tegra/nouveau GLES helpers can widen UV steps and push
     * CLUT samples off the page (black FBO while tris_flush stays huge). */
    "uvec2 resolveTexel() {\n"
    "  uvec2 texel = uvec2(clamp(floor(v_rawUV + 1.0 / 512.0), vec2(0.0),\n"
    "                            vec2(255.0)));\n"
    "  return (texel & v_texWindow.xy) | v_texWindow.zw;\n"
    "}\n"
    "uint rgb5551ToU16(vec4 c) {\n"
    "  uint r = uint(c.r * 31.0 + 0.5);\n"
    "  uint g = uint(c.g * 31.0 + 0.5);\n"
    "  uint b = uint(c.b * 31.0 + 0.5);\n"
    "  uint a = uint(c.a + 0.5);\n"
    "  return r | (g << 5u) | (b << 10u) | (a << 15u);\n"
    "}\n"
    "const mat4 ditherMatrix = mat4(\n"
    "  -4.0, +0.0, -3.0, +1.0,\n"
    "  +2.0, -2.0, +3.0, -1.0,\n"
    "  -3.0, +1.0, -4.0, +0.0,\n"
    "  +3.0, -1.0, +2.0, -2.0);\n"
    "vec3 applyDither(vec3 c) {\n"
    "  if (v_dither == 0u) return c;\n"
    /* u_ditherDiv is 1.0 (exact) unless DW3_RES_DITHER=ps1 at Nx, which
     * makes each PS1 pixel keep one matrix cell. */
    "  int dx = int(gl_FragCoord.x / u_ditherDiv) & 3;\n"
    "  int dy = int(gl_FragCoord.y / u_ditherDiv) & 3;\n"
    "  float off = ditherMatrix[dy][dx];\n"
    "  vec3 c8 = c * 255.0 + off;\n"
    "  return clamp(floor(c8 / 8.0), 0.0, 31.0) / 31.0;\n"
    "}\n"
    "void main() {\n"
    "  vec4 texColor;\n"
    "  if (v_textureMode == 0u) {\n"
    "    texColor = vec4(1.0, 1.0, 1.0, 2.0);\n"
    "  } else if (v_textureMode == 1u) {\n"
    "    texColor = texelFetch(u_texVram, v_pageBase + ivec2(resolveTexel()), 0);\n"
    "  } else {\n"
    "    ivec2 texel = ivec2(resolveTexel());\n"
    "    uint subPixel = uint(texel.x) & v_subPixelMask;\n"
    "    ivec2 texelPos = v_pageBase + ivec2(texel.x >> int(v_texelShift), texel.y);\n"
    "    uint word16 = rgb5551ToU16(texelFetch(u_texVram, texelPos, 0));\n"
    "    uint colorIdx = (word16 >> (subPixel * v_indexShift)) & v_indexMask;\n"
    "    ivec2 clutBase = ivec2(int((v_clut % 64u) * 16u), int(v_clut / 64u));\n"
    "    texColor = texelFetch(u_texVram, clutBase + ivec2(int(colorIdx), 0), 0);\n"
    "  }\n"
    "  if (texColor == vec4(0.0, 0.0, 0.0, 0.0)) {\n"
    "    FragColor = vec4(0.0);\n"
    "    return;\n"
    "  }\n"
    "  bool isSemiTrans = v_color.a < 0.75;\n"
    "  bool colorSemiTrans = texColor.a > 0.0;\n"
    "  vec3 modColor;\n"
    "  if (v_textureMode == 0u) {\n"
    "    modColor = texColor.rgb * v_color.rgb;\n"
    "  } else {\n"
    "    vec3 tex5 = floor(texColor.rgb * 31.0 + 0.5);\n"
    "    vec3 col8 = min(floor(v_color.rgb * 127.5 + 0.5), vec3(255.0));\n"
    "    vec3 prod8 = min(tex5 * col8 / 16.0, vec3(255.0));\n"
    "    modColor = v_dither != 0u ? prod8 / 255.0 : floor(prod8 / 8.0) / 31.0;\n"
    "  }\n"
    "  modColor = applyDither(modColor);\n"
    "  if (colorSemiTrans && isSemiTrans) {\n"
    "    uint abr = (v_tpage & 0x60u) >> 5u;\n"
    "    if (abr == 0u) FragColor = vec4(modColor * 0.5, 0.5);\n"
    "    else if (abr == 1u) FragColor = vec4(modColor, 0.0);\n"
    "    else if (abr == 2u) FragColor = vec4(modColor, 0.0);\n"
    "    else FragColor = vec4(modColor * 0.25, 0.0);\n"
    "  } else {\n"
    "    FragColor = vec4(modColor, 1.0);\n"
    "  }\n"
    "}\n";

static const char *BLIT_VS =
    "#version 300 es\n"
    "precision mediump float;\n"
    "layout(location = 0) in vec2 a_pos;\n"
    "layout(location = 1) in vec2 a_t;\n"
    "out vec2 v_t;\n"
    "void main() { v_t = a_t; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";

static const char *BLIT_FS =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec2 v_t;\n"
    "uniform vec2 u_uv0;\n"
    "uniform vec2 u_uv1;\n"
    "uniform sampler2D u_tex;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "  FragColor = texture(u_tex, u_uv0 + v_t * (u_uv1 - u_uv0));\n"
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
        fprintf(stderr, "nx_mesh: shader compile failed: %s\n", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint link_program(const char *vs_src, const char *fs_src) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    GLuint prog;
    GLint ok = 0;

    if (!vs || !fs) {
        if (vs) {
            glDeleteShader(vs);
        }
        if (fs) {
            glDeleteShader(fs);
        }
        return 0;
    }
    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        fprintf(stderr, "nx_mesh: program link failed: %s\n", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static void destroy_gl(void) {
    dw3_nx_res_probe_gl_shutdown();
    if (s_prog) {
        glDeleteProgram(s_prog);
        s_prog = 0;
    }
    if (s_blit_prog) {
        glDeleteProgram(s_blit_prog);
        s_blit_prog = 0;
    }
    if (s_blit_prog_f) {
        glDeleteProgram(s_blit_prog_f);
        s_blit_prog_f = 0;
    }
    s_blit_f_failed = 0;
    if (s_vbo) {
        glDeleteBuffers(1, &s_vbo);
        s_vbo = 0;
    }
    if (s_ebo) {
        glDeleteBuffers(1, &s_ebo);
        s_ebo = 0;
    }
    if (s_blit_vbo) {
        glDeleteBuffers(1, &s_blit_vbo);
        s_blit_vbo = 0;
    }
    if (s_mesh_fbo) {
        glDeleteFramebuffers(1, &s_mesh_fbo);
        s_mesh_fbo = 0;
    }
    if (s_mesh_tex) {
        glDeleteTextures(1, &s_mesh_tex);
        s_mesh_tex = 0;
    }
    if (s_gutter_fbo) {
        glDeleteFramebuffers(1, &s_gutter_fbo);
        s_gutter_fbo = 0;
    }
    if (s_gutter_tex) {
        glDeleteTextures(1, &s_gutter_tex);
        s_gutter_tex = 0;
    }
    if (s_sample_tex) {
        glDeleteTextures(1, &s_sample_tex);
        s_sample_tex = 0;
    }
    s_ready = false;
}

static uint8_t color_5to8(unsigned c5) {
    return (uint8_t)((c5 * 255u + 15u) / 31u);
}

static bool sample_dirty(void) {
    return s_dirty_x1 > s_dirty_x0 && s_dirty_y1 > s_dirty_y0;
}

static void mark_dirty_full(void) {
    s_dirty_x0 = 0;
    s_dirty_y0 = 0;
    s_dirty_x1 = DW3_NX_VRAM_W;
    s_dirty_y1 = DW3_NX_VRAM_H;
}

/* Upload the dirty box with the STP bit in alpha (needed by the CLUT shader). */
static void upload_sample_vram(const uint16_t *cpu_vram) {
    int x0 = s_dirty_x0;
    int y0 = s_dirty_y0;
    int bw = s_dirty_x1 - s_dirty_x0;
    int bh = s_dirty_y1 - s_dirty_y0;
    int y;
    uint8_t *dst = s_pack_rgba;

    if (!cpu_vram || bw <= 0 || bh <= 0) {
        return;
    }
    for (y = 0; y < bh; y++) {
        const uint16_t *row =
            cpu_vram + (size_t)(y0 + y) * (size_t)DW3_NX_VRAM_W + (size_t)x0;
        int x;

        for (x = 0; x < bw; x++) {
            uint16_t v = row[x];

            dst[0] = color_5to8(v & 0x1Fu);
            dst[1] = color_5to8((v >> 5) & 0x1Fu);
            dst[2] = color_5to8((v >> 10) & 0x1Fu);
            dst[3] = (v & 0x8000u) ? 0xFFu : 0x00u;
            dst += 4;
        }
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_sample_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x0, y0, bw, bh, GL_RGBA,
                    GL_UNSIGNED_BYTE, s_pack_rgba);
    dw3_nx_mesh_debug_note_upload((uint32_t)bw * (uint32_t)bh);
    s_dirty_x0 = 0;
    s_dirty_y0 = 0;
    s_dirty_x1 = 0;
    s_dirty_y1 = 0;
}

static void set_blend(MeshBlend mode) {
    switch (mode) {
    case BLEND_SUB:
        glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_ADD);
        glBlendFuncSeparate(GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
        break;
    case BLEND_SUB_OPAQUE:
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    case BLEND_ADD:
    default:
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        break;
    }
}

static bool is_subtract(const Dw3NxMeshVertex *v) {
    return v->a == 0x80 && (v->tpage & 0x60u) == 0x40u;
}

static bool is_untextured(const Dw3NxMeshVertex *v) {
    return (v->tpage & DW3_NX_MESH_TPAGE_NOTEXTURE) != 0;
}

/* Whether the drawing area spans the display's columns, which takes the
 * gutters. Any non-empty band of rows does, as in PSY-Z's rasterizer: a layer
 * clipped to a band (the area banner closing) reaches the gutters there. */
static bool gutter_active(void) {
    return s_gutter_px > 0 && s_gutter_fbo && s_area_x0 == s_gutter_x0 &&
           s_area_x1 == s_gutter_x0 + s_gutter_w - 1 && s_area_y1 >= s_area_y0;
}

/* Clears a rect of a VRAM-sized FBO, clipped to it. */
static void clear_fbo_rect(GLuint fbo, int x, int y, int w, int h, float r,
                           float g, float b) {
    GLint prev_fbo = 0;

    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > DW3_NX_VRAM_W) {
        w = DW3_NX_VRAM_W - x;
    }
    if (y + h > DW3_NX_VRAM_H) {
        h = DW3_NX_VRAM_H - y;
    }
    if (!fbo || w <= 0 || h <= 0) {
        return;
    }
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x * s_scale, y * s_scale, w * s_scale, h * s_scale);
    glClearColor(r, g, b, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
}

static void apply_scissor(void) {
    int x0 = s_area_x0;
    int y0 = s_area_y0;
    int x1 = s_area_x1;
    int y1 = s_area_y1;
    int w;
    int h;

    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 >= DW3_NX_VRAM_W) {
        x1 = DW3_NX_VRAM_W - 1;
    }
    if (y1 >= DW3_NX_VRAM_H) {
        y1 = DW3_NX_VRAM_H - 1;
    }
    w = x1 - x0 + 1;
    h = y1 - y0 + 1;
    if (w <= 0 || h <= 0) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    glEnable(GL_SCISSOR_TEST);
    glScissor(x0 * s_scale, y0 * s_scale, w * s_scale, h * s_scale);
}

static void draw_indices(int start, int end) {
    glDrawElements(GL_TRIANGLES, end - start, GL_UNSIGNED_SHORT,
                   (const void *)(sizeof(uint16_t) * (size_t)start));
}

/* Draws the queued triangles into the bound FBO, in OT order, with the
 * blend each needs. */
static void draw_batches(void) {
    int start = 0;

    while (start < s_n_idx) {
        const Dw3NxMeshVertex *v = &s_vtx[s_idx[start]];
        bool need_sub = is_subtract(v);
        int end = start + 3;

        while (end < s_n_idx) {
            if (is_subtract(&s_vtx[s_idx[end]]) != need_sub) {
                break;
            }
            end += 3;
        }
        if (need_sub) {
            int cur = start;

            while (cur < end) {
                int group = cur + 3;
                bool unt = is_untextured(&s_vtx[s_idx[cur]]);

                while (group < end) {
                    if (is_untextured(&s_vtx[s_idx[group]]) != unt) {
                        break;
                    }
                    group += 3;
                }
                set_blend(BLEND_SUB);
                draw_indices(cur, group);
                if (!unt) {
                    set_blend(BLEND_SUB_OPAQUE);
                    draw_indices(cur, group);
                }
                cur = group;
            }
            start = end;
        } else {
            set_blend(BLEND_ADD);
            draw_indices(start, end);
            start = end;
        }
    }
}

static void flush_locked(const uint16_t *cpu_vram) {
    GLint prev_fbo = 0;
    int tris;
#if DW3_NX_DEBUG_MESH
    uint64_t t0;
    uint64_t t1;
    uint64_t t2;
#endif

    if (!s_ready || s_n_vtx == 0 || s_n_idx == 0) {
        s_n_vtx = 0;
        s_n_idx = 0;
        return;
    }

    tris = s_n_idx / 3;
#if DW3_NX_DEBUG_MESH
    t0 = mesh_now_us();
    t1 = t0;
#endif
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
    /* Full 1024×512 RGBA pack+upload dominates OLED when done every flush.
     * Fight-menu idle rarely touches VRAM; reuse the GL sample until dirty. */
    if (sample_dirty()) {
        uint64_t probe_t = 0;

        if (dw3_nx_res_probe_on()) {
            probe_t = probe_now_us();
        }
        upload_sample_vram(cpu_vram);
        if (probe_t) {
            dw3_nx_res_probe_note_upload((uint32_t)(probe_now_us() - probe_t));
        }
#if DW3_NX_DEBUG_MESH
        t1 = mesh_now_us();
#endif
    }

    dw3_nx_res_probe_mesh_begin();
    glBindFramebuffer(GL_FRAMEBUFFER, s_mesh_fbo);
    glViewport(0, 0, DW3_NX_VRAM_W * s_scale, DW3_NX_VRAM_H * s_scale);
    apply_scissor();
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glUseProgram(s_prog);
    glUniform2f(s_u_draw_offset, (float)s_off_x, (float)s_off_y);
    glUniform1i(s_u_tex_vram, 0);
    glUniform1f(s_u_dither_div, s_dither_ps1 ? (float)s_scale : 1.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_sample_tex);

    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    (GLsizeiptr)(sizeof(Dw3NxMeshVertex) * (size_t)s_n_vtx),
                    s_vtx);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_ebo);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
                    (GLsizeiptr)(sizeof(uint16_t) * (size_t)s_n_idx), s_idx);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_SHORT, GL_FALSE, sizeof(Dw3NxMeshVertex),
                          (const void *)offsetof(Dw3NxMeshVertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_SHORT, GL_FALSE,
                          sizeof(Dw3NxMeshVertex),
                          (const void *)offsetof(Dw3NxMeshVertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                          sizeof(Dw3NxMeshVertex),
                          (const void *)offsetof(Dw3NxMeshVertex, r));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_FALSE,
                          sizeof(Dw3NxMeshVertex),
                          (const void *)offsetof(Dw3NxMeshVertex, twin));

    draw_batches();
    if (gutter_active()) {
        /* Again into the gutter FBO, shifted so the display starts at
         * s_gutter_px and clipped to the display plus both gutters. */
        glBindFramebuffer(GL_FRAMEBUFFER, s_gutter_fbo);
        glScissor(0, s_area_y0 * s_scale,
                  (s_gutter_w + 2 * s_gutter_px) * s_scale,
                  (s_area_y1 - s_area_y0 + 1) * s_scale);
        glUniform2f(s_u_draw_offset,
                    (float)(s_off_x + s_gutter_px - s_area_x0),
                    (float)s_off_y);
        draw_batches();
    }

    dw3_nx_res_probe_mesh_end(tris);
    s_prims_flushed += (uint64_t)tris;
    s_had_content = true;
    s_n_vtx = 0;
    s_n_idx = 0;

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
    glDisable(GL_SCISSOR_TEST);
    /* set_blend leaves the last batch's equation behind. A subtractive batch
     * (BLEND_SUB, e.g. the battle's fade-out) left GL_FUNC_REVERSE_SUBTRACT
     * current after the battle, and every later blended draw (debug HUD,
     * native title menu and logo) subtracted itself from the frame: dark
     * HUD text after a battle, a black title after a restart. */
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ZERO);
    glDisable(GL_BLEND);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(3);
#if DW3_NX_DEBUG_MESH
    t2 = mesh_now_us();
    dw3_nx_mesh_debug_note_flush((uint32_t)(t2 - t0), (uint32_t)(t1 - t0),
                                 tris);
#endif
}

static int clamp_scale(int n) {
    return n < 1 ? 1 : (n > MESH_SCALE_MAX ? MESH_SCALE_MAX : n);
}

/* The test aids, read once (DW3_NX_TEST_ENV bakes them into a measurement
 * NRO). Unset = scale 1, nearest, the fine dither: the shipped behaviour. */
static void read_test_env(void) {
    static int done;
    const char *v;

    if (done) {
        return;
    }
    done = 1;
    v = getenv("DW3_NX_MESH_SCALE");
    if (v && v[0]) {
        s_scale_want = clamp_scale(atoi(v));
        s_scale = s_scale_want;
    }
    v = getenv("DW3_NX_MESH_FILTER");
    s_filter_linear = v && (v[0] == 'l' || v[0] == 'L');
    v = getenv("DW3_RES_DITHER");
    s_dither_ps1 = v && (v[0] == 'p' || v[0] == 'P');
    if (s_scale != 1 || s_filter_linear || s_dither_ps1) {
        fprintf(stderr, "nx_mesh: TEST AID scale=%d filter=%s dither=%s\n",
                s_scale, s_filter_linear ? "linear" : "nearest",
                s_dither_ps1 ? "ps1" : "fine");
    }
}

/* Gives the two targets their size for s_scale. The caller has bound nothing
 * it needs kept. Returns false on a GL error (out of memory). */
static bool alloc_targets(void) {
    GLenum filt = s_filter_linear ? GL_LINEAR : GL_NEAREST;
    int w = DW3_NX_VRAM_W * s_scale;
    int h = DW3_NX_VRAM_H * s_scale;
    GLuint tex[2] = {s_mesh_tex, s_gutter_tex};
    int i;

    while (glGetError() != GL_NO_ERROR) {
    }
    for (i = 0; i < 2; i++) {
        if (!tex[i]) {
            continue;
        }
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filt);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filt);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);
    }
    return glGetError() == GL_NO_ERROR;
}

static uint64_t target_bytes(void) {
    return (uint64_t)DW3_NX_VRAM_W * DW3_NX_VRAM_H * 4u * (uint64_t)s_scale *
           (uint64_t)s_scale * ((s_mesh_tex ? 1u : 0u) + (s_gutter_tex ? 1u : 0u));
}

static void clear_both_targets(void) {
    GLint prev_fbo = 0;
    int i;
    GLuint fbo[2] = {s_mesh_fbo, s_gutter_fbo};

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    for (i = 0; i < 2; i++) {
        if (fbo[i]) {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
}

/* Applies s_scale_want at a frame boundary (or at init). On failure it goes
 * back to 1x, which always fits, and says so. */
static void apply_scale(void) {
    uint64_t t0 = probe_now_us();
    int ok;
    int want = s_scale_want;

    s_scale = want;
    ok = alloc_targets();
    if (ok) {
        GLint prev_fbo = 0;

        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, s_mesh_fbo);
        ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
    }
    if (!ok && want != 1) {
        fprintf(stderr, "nx_mesh: %dx targets failed, back to 1x\n", want);
        s_scale = s_scale_want = 1;
        ok = alloc_targets();
    }
    clear_both_targets();
    glFinish(); /* the allocation time below is real, not queued */
    s_n_vtx = 0;
    s_n_idx = 0;
    dw3_nx_res_probe_note_targets(
        s_scale, DW3_NX_VRAM_W * s_scale, DW3_NX_VRAM_H * s_scale,
        s_gutter_tex != 0, target_bytes(), (uint32_t)(probe_now_us() - t0),
        ok && s_scale == want);
}

int dw3_nx_mesh_scale(void) { return s_scale; }

uint64_t dw3_nx_mesh_target_bytes(void) { return s_ready ? target_bytes() : 0; }

void dw3_nx_mesh_set_scale(int n) { s_scale_want = clamp_scale(n); }

int dw3_nx_mesh_cycle_scale(void) {
    s_scale_want = s_scale_want >= MESH_SCALE_MAX ? 1 : s_scale_want + 1;
    fprintf(stderr, "nx_mesh: scale -> %dx at the next frame boundary\n",
            s_scale_want);
    return s_scale_want;
}

bool dw3_nx_mesh_init(void) {
    static const float blit_quad[] = {
        -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f,  1.0f, 0.0f,
    };

    destroy_gl();
    read_test_env();

    s_prog = link_program(MESH_VS, MESH_FS);
    s_blit_prog = link_program(BLIT_VS, BLIT_FS);
    if (!s_prog || !s_blit_prog) {
        destroy_gl();
        return false;
    }
    s_u_draw_offset = glGetUniformLocation(s_prog, "u_drawOffset");
    s_u_tex_vram = glGetUniformLocation(s_prog, "u_texVram");
    s_u_dither_div = glGetUniformLocation(s_prog, "u_ditherDiv");
    s_blit_uv0 = glGetUniformLocation(s_blit_prog, "u_uv0");
    s_blit_uv1 = glGetUniformLocation(s_blit_prog, "u_uv1");
    s_blit_tex = glGetUniformLocation(s_blit_prog, "u_tex");
    s_blit_pos = glGetAttribLocation(s_blit_prog, "a_pos");
    s_blit_t = glGetAttribLocation(s_blit_prog, "a_t");

    glGenTextures(1, &s_sample_tex);
    glBindTexture(GL_TEXTURE_2D, s_sample_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, DW3_NX_VRAM_W, DW3_NX_VRAM_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glGenTextures(1, &s_mesh_tex);
    glBindTexture(GL_TEXTURE_2D, s_mesh_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    /* Sized (and re-sized) by apply_scale below; 1x here keeps the shipped
     * allocation order. */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, DW3_NX_VRAM_W, DW3_NX_VRAM_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glGenFramebuffers(1, &s_mesh_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_mesh_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           s_mesh_tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "nx_mesh: mesh FBO incomplete\n");
        destroy_gl();
        return false;
    }

    glGenTextures(1, &s_gutter_tex);
    glBindTexture(GL_TEXTURE_2D, s_gutter_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, DW3_NX_VRAM_W, DW3_NX_VRAM_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &s_gutter_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_gutter_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           s_gutter_tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        /* Gutters are optional: the mesh layer works without them. */
        fprintf(stderr, "nx_mesh: gutter FBO incomplete, widescreen off\n");
        glDeleteFramebuffers(1, &s_gutter_fbo);
        s_gutter_fbo = 0;
        glDeleteTextures(1, &s_gutter_tex);
        s_gutter_tex = 0;
    } else {
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_vtx), NULL, GL_DYNAMIC_DRAW);
    glGenBuffers(1, &s_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(s_idx), NULL, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &s_blit_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_blit_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(blit_quad), blit_quad, GL_STATIC_DRAW);

    s_n_vtx = 0;
    s_n_idx = 0;
    s_had_content = false;
    s_content_before_hold = false;
    mark_dirty_full();
    s_ready = true;
    dw3_nx_res_probe_gl_init();
    /* Only a test aid ever leaves 1x, or asks for a log of the allocation:
     * the shipped path does not touch the targets again. */
    if (s_scale != 1 || dw3_nx_res_probe_on()) {
        apply_scale();
    }
    fprintf(stderr, "nx_mesh: GLES3 mesh FBO ready\n");
    return true;
}

void dw3_nx_mesh_shutdown(void) { destroy_gl(); }

bool dw3_nx_mesh_reinit(void) { return dw3_nx_mesh_init(); }

bool dw3_nx_mesh_available(void) { return s_ready; }

/* Partner-view areas, one per display buffer it is drawn into. */
enum { SOFT_VIEW_MAX = 4, SOFT_VIEW_MAX_AGE = 4 };
typedef struct SoftView {
    int x0;
    int y0;
    int x1;
    int y1;
    int age;
} SoftView;
static SoftView s_soft_views[SOFT_VIEW_MAX];
static int s_n_soft_views;

void dw3_nx_mesh_discard(void) {
    s_n_vtx = 0;
    s_n_idx = 0;
    s_had_content = false;
    s_content_before_hold = false;
    s_n_soft_views = 0;
}

void dw3_nx_mesh_note_soft_view(int x0, int y0, int x1, int y1) {
    int i;

    for (i = 0; i < s_n_soft_views; i++) {
        SoftView *v = &s_soft_views[i];

        if (v->x0 == x0 && v->y0 == y0 && v->x1 == x1 && v->y1 == y1) {
            v->age = 0;
            return;
        }
    }
    if (s_n_soft_views == SOFT_VIEW_MAX) {
        /* Replace the oldest. */
        int oldest = 0;

        for (i = 1; i < SOFT_VIEW_MAX; i++) {
            if (s_soft_views[i].age > s_soft_views[oldest].age) {
                oldest = i;
            }
        }
        i = oldest;
    } else {
        i = s_n_soft_views++;
    }
    s_soft_views[i].x0 = x0;
    s_soft_views[i].y0 = y0;
    s_soft_views[i].x1 = x1;
    s_soft_views[i].y1 = y1;
    s_soft_views[i].age = 0;
}

bool dw3_nx_mesh_soft_view_in(int dx, int dy, int dw, int dh, int *x, int *y,
                              int *w, int *h) {
    int i;

    for (i = 0; i < s_n_soft_views; i++) {
        const SoftView *v = &s_soft_views[i];

        if (v->x0 >= dx && v->y0 >= dy && v->x1 < dx + dw &&
            v->y1 < dy + dh && v->x1 >= v->x0 && v->y1 >= v->y0) {
            *x = v->x0 - dx;
            *y = v->y0 - dy;
            *w = v->x1 - v->x0 + 1;
            *h = v->y1 - v->y0 + 1;
            return true;
        }
    }
    return false;
}

static void soft_views_age(void) {
    int i = 0;

    while (i < s_n_soft_views) {
        if (++s_soft_views[i].age > SOFT_VIEW_MAX_AGE) {
            s_soft_views[i] = s_soft_views[--s_n_soft_views];
        } else {
            i++;
        }
    }
}

void dw3_nx_mesh_mark_vram_dirty(void) { mark_dirty_full(); }

void dw3_nx_mesh_mark_vram_dirty_rect(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }
    /* PS1 transfers wrap at the VRAM edge; keep that case conservative. */
    if (x < 0 || y < 0 || x + w > DW3_NX_VRAM_W || y + h > DW3_NX_VRAM_H) {
        mark_dirty_full();
        return;
    }
    if (!sample_dirty()) {
        s_dirty_x0 = x;
        s_dirty_y0 = y;
        s_dirty_x1 = x + w;
        s_dirty_y1 = y + h;
        return;
    }
    if (x < s_dirty_x0) {
        s_dirty_x0 = x;
    }
    if (y < s_dirty_y0) {
        s_dirty_y0 = y;
    }
    if (x + w > s_dirty_x1) {
        s_dirty_x1 = x + w;
    }
    if (y + h > s_dirty_y1) {
        s_dirty_y1 = y + h;
    }
}

void dw3_nx_mesh_set_offset(int x, int y) {
    x %= DW3_NX_VRAM_W;
    y %= DW3_NX_VRAM_H;
    if (x < 0) {
        x += DW3_NX_VRAM_W;
    }
    if (y < 0) {
        y += DW3_NX_VRAM_H;
    }
    s_off_x = x;
    s_off_y = y;
}

void dw3_nx_mesh_set_area(int x0, int y0, int x1, int y1) {
    s_area_x0 = x0;
    s_area_y0 = y0;
    s_area_x1 = x1;
    s_area_y1 = y1;
}

void dw3_nx_mesh_set_tpage(uint16_t tpage, int dither_env) {
    (void)tpage;
    (void)dither_env;
}

void dw3_nx_mesh_set_twin(uint32_t twin) { (void)twin; }

void dw3_nx_mesh_clear_rect(int x, int y, int w, int h, float r, float g,
                            float b) {
    if (!s_ready || w <= 0 || h <= 0) {
        return;
    }
    clear_fbo_rect(s_mesh_fbo, x, y, w, h, r, g, b);
    /* A clear of the whole display also clears its gutters. */
    if (s_gutter_px > 0 && x == s_gutter_x0 && w == s_gutter_w) {
        clear_fbo_rect(s_gutter_fbo, 0, y, w + 2 * s_gutter_px, h, r, g, b);
    }
}

void dw3_nx_mesh_fill_bg(int x, int y, int w, int h, float r, float g,
                         float b) {
    if (!s_ready) {
        return;
    }
    dw3_nx_mesh_clear_rect(x, y, w, h, r, g, b);
    /* A background fill of the display's drawing area covers the gutters. */
    if (gutter_active() && x == s_area_x0 && w == s_area_x1 - s_area_x0 + 1 &&
        !(x == s_gutter_x0 && w == s_gutter_w)) {
        clear_fbo_rect(s_gutter_fbo, 0, y, s_gutter_w + 2 * s_gutter_px, h, r,
                       g, b);
    }
    s_had_content = true;
}

void dw3_nx_mesh_set_gutter(int px, int x0, int w, int h) {
    if (px != s_gutter_px && s_ready) {
        clear_fbo_rect(s_gutter_fbo, 0, 0, DW3_NX_VRAM_W, DW3_NX_VRAM_H, 0.0f,
                       0.0f, 0.0f);
    }
    s_gutter_px = px;
    s_gutter_x0 = x0;
    s_gutter_w = w;
    (void)h; /* the display height; any band of rows takes the gutters */
}

void dw3_nx_mesh_clear_gutter_rows(int y0, int y1) {
    if (s_ready && s_gutter_px > 0) {
        clear_fbo_rect(s_gutter_fbo, 0, y0, DW3_NX_VRAM_W, y1 - y0, 0.0f, 0.0f,
                       0.0f);
    }
}

bool dw3_nx_mesh_has_gutters(void) {
    return s_ready && s_gutter_fbo != 0;
}

void dw3_nx_mesh_enqueue(const Dw3NxMeshVertex *v, int nverts,
                         const uint16_t *idx, int nidx,
                         const uint16_t *cpu_vram) {
    int i;

    if (!s_ready || !v || !idx || nverts <= 0 || nidx <= 0) {
        return;
    }
    if (s_n_vtx + nverts > MESH_MAX_VTX || s_n_idx + nidx > MESH_MAX_IDX) {
        flush_locked(cpu_vram);
    }
    if (s_n_vtx + nverts > MESH_MAX_VTX || s_n_idx + nidx > MESH_MAX_IDX) {
        return;
    }
    memcpy(s_vtx + s_n_vtx, v, (size_t)nverts * sizeof(Dw3NxMeshVertex));
    for (i = 0; i < nidx; i++) {
        s_idx[s_n_idx + i] = (uint16_t)(idx[i] + (uint16_t)s_n_vtx);
    }
    s_n_vtx += nverts;
    s_n_idx += nidx;
    dw3_nx_mesh_debug_note_enqueue(nidx);
}

void dw3_nx_mesh_flush(const uint16_t *cpu_vram) { flush_locked(cpu_vram); }

/* Content to composite this present: this frame's, or (battle held) the last
 * drawn frame's. */
static bool content_now(void) {
    return s_had_content ||
           (s_content_before_hold && dw3_port_battle_options_holding());
}

bool dw3_nx_mesh_has_content(void) { return content_now(); }

uint64_t dw3_nx_mesh_prims_flushed(void) { return s_prims_flushed; }

void dw3_nx_mesh_set_filter(int mode) {
    s_out_filter = mode;
    if (mode != DW3_NX_FILTER_NEAREST && s_ready && !s_blit_prog_f &&
        !s_blit_f_failed) {
        s_blit_prog_f = link_program(BLIT_VS, DW3_NX_MESH_BLIT_FS_F);
        if (!s_blit_prog_f) {
            s_blit_f_failed = 1;
            fprintf(stderr, "nx_mesh: the output filter blit failed to build; "
                            "the battle picture stays on nearest\n");
            return;
        }
        s_blit_f_uv0 = glGetUniformLocation(s_blit_prog_f, "u_uv0");
        s_blit_f_uv1 = glGetUniformLocation(s_blit_prog_f, "u_uv1");
        s_blit_f_tex = glGetUniformLocation(s_blit_prog_f, "u_tex");
        s_blit_f_flt = glGetUniformLocation(s_blit_prog_f, "u_flt");
    }
}

void dw3_nx_mesh_present(int dx, int dy, int dw, int dh, int vx, int vy, int vw,
                         int vh) {
    GLuint tex = s_mesh_tex;
    const bool filtered = s_out_filter != DW3_NX_FILTER_NEAREST && s_blit_prog_f;

    if (!s_ready || !content_now() || dw <= 0 || dh <= 0 || vw <= 0 || vh <= 0) {
        return;
    }
    if (s_gutter_px > 0 && s_gutter_fbo && dw == s_gutter_w) {
        /* Put the display between its gutters, then show all of it. */
        GLint prev_fbo = 0;

        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
        glDisable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, s_mesh_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_gutter_fbo);
        glBlitFramebuffer(dx * s_scale, dy * s_scale, (dx + dw) * s_scale,
                          (dy + dh) * s_scale, s_gutter_px * s_scale,
                          dy * s_scale, (s_gutter_px + dw) * s_scale,
                          (dy + dh) * s_scale, GL_COLOR_BUFFER_BIT,
                          GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
        tex = s_gutter_tex;
        dx = 0;
        dw += 2 * s_gutter_px;
    }
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glViewport(vx, vy, vw, vh);
    glUseProgram(filtered ? s_blit_prog_f : s_blit_prog);
    glUniform2f(filtered ? s_blit_f_uv0 : s_blit_uv0,
                (float)dx / (float)DW3_NX_VRAM_W,
                (float)dy / (float)DW3_NX_VRAM_H);
    glUniform2f(filtered ? s_blit_f_uv1 : s_blit_uv1,
                (float)(dx + dw) / (float)DW3_NX_VRAM_W,
                (float)(dy + dh) / (float)DW3_NX_VRAM_H);
    if (filtered) {
        glUniform3f(s_blit_f_flt, (float)s_out_filter, (float)vw, (float)vh);
    }
    glUniform1i(filtered ? s_blit_f_tex : s_blit_tex, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindBuffer(GL_ARRAY_BUFFER, s_blit_vbo);
    glEnableVertexAttribArray((GLuint)s_blit_pos);
    glVertexAttribPointer((GLuint)s_blit_pos, 2, GL_FLOAT, GL_FALSE,
                          sizeof(float) * 4, (const void *)0);
    glEnableVertexAttribArray((GLuint)s_blit_t);
    glVertexAttribPointer((GLuint)s_blit_t, 2, GL_FLOAT, GL_FALSE,
                          sizeof(float) * 4, (const void *)(sizeof(float) * 2));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

size_t dw3_nx_mesh_read_rect_rgba(int dx, int dy, int dw, int dh, uint8_t *dst,
                                  size_t dst_cap) {
    GLint prev_fbo = 0;
    size_t need;
    GLenum err;

    if (!s_ready || !dst || dw <= 0 || dh <= 0 || s_scale != 1) {
        return 0; /* the SD dump is 1x only */
    }
    if (dx < 0 || dy < 0 || dx + dw > DW3_NX_VRAM_W || dy + dh > DW3_NX_VRAM_H) {
        return 0;
    }
    need = (size_t)dw * (size_t)dh * 4u;
    if (dst_cap < need) {
        return 0;
    }
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_mesh_fbo);
    /* Ensure flush/draw into the FBO completed before the SD dump read. */
    glFinish();
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    while (glGetError() != GL_NO_ERROR) {
    }
    /* GL origin is bottom-left; PS1 VRAM y matches this FBO (same as present). */
    glReadPixels(dx, dy, dw, dh, GL_RGBA, GL_UNSIGNED_BYTE, dst);
    err = glGetError();
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
    if (err != GL_NO_ERROR) {
        return 0;
    }
    return need;
}

void dw3_nx_mesh_end_frame(void) {
    if (dw3_port_battle_options_holding()) {
        /* Nothing was drawn: keep what is on screen (the FBO, the partner
         * view's keep area) until the battle runs again. */
        return;
    }
    dw3_nx_res_probe_frame_end();
    s_content_before_hold = s_had_content;
    s_had_content = false;
    soft_views_age();
    if (s_scale_want != s_scale && s_ready) {
        /* A test-aid scale change, at the frame boundary. The next frame
         * redraws the whole battle, so the cleared targets cost nothing. */
        apply_scale();
        s_had_content = false;
        s_content_before_hold = false;
    }
}

void dw3_nx_mesh_drop_pending(void) {
    s_n_vtx = 0;
    s_n_idx = 0;
}
