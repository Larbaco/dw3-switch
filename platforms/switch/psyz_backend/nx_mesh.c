/* P5.2 — GLES3 mesh FBO for OT triangle polys (battle 3D).
 * Shaders adapted from vendor/psyz sdl3_gl.c (ES 3.00). UI stays on CPU VRAM. */
#include "nx_mesh.h"
#include "nx_mesh_debug.h"
#include "nx_vram_pack.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GLES3/gl3.h>

#ifndef DW3_NX_MESH_DIAG
#define DW3_NX_MESH_DIAG 0
#endif

#if DW3_NX_DEBUG_MESH
#include <switch.h>

static uint64_t mesh_now_us(void) {
    return armTicksToNs(armGetSystemTick()) / 1000ULL;
}
#endif

enum { MESH_MAX_VTX = 8192, MESH_MAX_IDX = MESH_MAX_VTX / 4 * 6 };

typedef enum {
    BLEND_ADD = 0,
    BLEND_SUB,
    BLEND_SUB_OPAQUE
} MeshBlend;

static bool s_ready;
static bool s_had_content;
static bool s_sample_dirty = true;
static uint64_t s_prims_flushed;

static int s_off_x;
static int s_off_y;
static int s_area_x0;
static int s_area_y0;
static int s_area_x1 = DW3_NX_VRAM_W - 1;
static int s_area_y1 = DW3_NX_VRAM_H - 1;
static GLuint s_mesh_tex;
static GLuint s_mesh_fbo;
static GLuint s_sample_tex;
static GLuint s_prog;
static GLuint s_blit_prog;
static GLuint s_ssquad_prog;
static GLuint s_vbo;
static GLuint s_ebo;
static GLuint s_blit_vbo;
static GLuint s_ssquad_vbo;
static GLint s_u_draw_offset = -1;
static GLint s_u_tex_vram = -1;
static GLint s_blit_uv0 = -1;
static GLint s_blit_uv1 = -1;
static GLint s_blit_tex = -1;
static GLint s_blit_pos = -1;
static GLint s_blit_t = -1;
static GLint s_ssquad_color = -1;
static GLint s_ssquad_pos = -1;

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
    "  int dx = int(gl_FragCoord.x) & 3;\n"
    "  int dy = int(gl_FragCoord.y) & 3;\n"
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

/* DIAG: solid colour from clip-space verts (BLIT_VS); no PS1 / GTE path. */
static const char *SSQUAD_FS =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "out vec4 FragColor;\n"
    "void main() { FragColor = u_color; }\n";

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
    if (s_prog) {
        glDeleteProgram(s_prog);
        s_prog = 0;
    }
    if (s_blit_prog) {
        glDeleteProgram(s_blit_prog);
        s_blit_prog = 0;
    }
    if (s_ssquad_prog) {
        glDeleteProgram(s_ssquad_prog);
        s_ssquad_prog = 0;
    }
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
    if (s_ssquad_vbo) {
        glDeleteBuffers(1, &s_ssquad_vbo);
        s_ssquad_vbo = 0;
    }
    if (s_mesh_fbo) {
        glDeleteFramebuffers(1, &s_mesh_fbo);
        s_mesh_fbo = 0;
    }
    if (s_mesh_tex) {
        glDeleteTextures(1, &s_mesh_tex);
        s_mesh_tex = 0;
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

/* Full VRAM upload with STP bit in alpha (needed by the CLUT shader). */
static void upload_sample_vram(const uint16_t *cpu_vram) {
    int y;
    uint8_t *dst = s_pack_rgba;

    if (!cpu_vram) {
        return;
    }
    for (y = 0; y < DW3_NX_VRAM_H; y++) {
        const uint16_t *row =
            cpu_vram + (size_t)y * (size_t)DW3_NX_VRAM_W;
        int x;

        for (x = 0; x < DW3_NX_VRAM_W; x++) {
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
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, DW3_NX_VRAM_W, DW3_NX_VRAM_H,
                    GL_RGBA, GL_UNSIGNED_BYTE, s_pack_rgba);
    s_sample_dirty = false;
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
    glScissor(x0, y0, w, h);
}

static void draw_indices(int start, int end) {
    glDrawElements(GL_TRIANGLES, end - start, GL_UNSIGNED_SHORT,
                   (const void *)(sizeof(uint16_t) * (size_t)start));
}

static void flush_locked(const uint16_t *cpu_vram) {
    int start;
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
    if (s_sample_dirty) {
        upload_sample_vram(cpu_vram);
#if DW3_NX_DEBUG_MESH
        t1 = mesh_now_us();
#endif
    }

    glBindFramebuffer(GL_FRAMEBUFFER, s_mesh_fbo);
    glViewport(0, 0, DW3_NX_VRAM_W, DW3_NX_VRAM_H);
    apply_scissor();
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glUseProgram(s_prog);
    glUniform2f(s_u_draw_offset, (float)s_off_x, (float)s_off_y);
    glUniform1i(s_u_tex_vram, 0);
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

    start = 0;
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

    s_prims_flushed += (uint64_t)tris;
    s_had_content = true;
    s_n_vtx = 0;
    s_n_idx = 0;

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
    glDisable(GL_SCISSOR_TEST);
#if DW3_NX_DEBUG_MESH
    t2 = mesh_now_us();
    dw3_nx_mesh_debug_note_flush((uint32_t)(t2 - t0), (uint32_t)(t1 - t0),
                                 tris);
#endif
}

bool dw3_nx_mesh_init(void) {
    static const float blit_quad[] = {
        -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f,  1.0f, 0.0f,
    };

    destroy_gl();

    s_prog = link_program(MESH_VS, MESH_FS);
    s_blit_prog = link_program(BLIT_VS, BLIT_FS);
    s_ssquad_prog = link_program(BLIT_VS, SSQUAD_FS);
    if (!s_prog || !s_blit_prog || !s_ssquad_prog) {
        destroy_gl();
        return false;
    }
    s_u_draw_offset = glGetUniformLocation(s_prog, "u_drawOffset");
    s_u_tex_vram = glGetUniformLocation(s_prog, "u_texVram");
    s_blit_uv0 = glGetUniformLocation(s_blit_prog, "u_uv0");
    s_blit_uv1 = glGetUniformLocation(s_blit_prog, "u_uv1");
    s_blit_tex = glGetUniformLocation(s_blit_prog, "u_tex");
    s_blit_pos = glGetAttribLocation(s_blit_prog, "a_pos");
    s_blit_t = glGetAttribLocation(s_blit_prog, "a_t");
    s_ssquad_color = glGetUniformLocation(s_ssquad_prog, "u_color");
    s_ssquad_pos = glGetAttribLocation(s_ssquad_prog, "a_pos");

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

    /* DIAG clip-space ssquad VBO (updated per ClearImage rect). */
    glGenBuffers(1, &s_ssquad_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_ssquad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, NULL, GL_DYNAMIC_DRAW);

    s_n_vtx = 0;
    s_n_idx = 0;
    s_had_content = false;
    s_sample_dirty = true;
    s_ready = true;
    fprintf(stderr, "nx_mesh: GLES3 mesh FBO ready\n");
    return true;
}

void dw3_nx_mesh_shutdown(void) { destroy_gl(); }

bool dw3_nx_mesh_reinit(void) { return dw3_nx_mesh_init(); }

bool dw3_nx_mesh_available(void) { return s_ready; }

int dw3_nx_mesh_diag_enabled(void) {
    static int cached = -1;
    const char *env;

    if (cached >= 0) {
        return cached;
    }
    env = getenv("DW3_MESH_DIAG");
    if (env && env[0] == '1' && env[1] == '\0') {
        cached = 1;
    } else if (env && env[0] == '0' && env[1] == '\0') {
        cached = 0;
    } else {
        cached = DW3_NX_MESH_DIAG ? 1 : 0;
    }
    if (cached) {
        fprintf(stderr,
                "nx_mesh: DIAG on (gpu-solid magenta clear+verts + cyan "
                "ssquad, mesh-only present; soft default unchanged)\n");
    }
    return cached;
}

void dw3_nx_mesh_diag_ssquad(int x, int y, int w, int h) {
    GLint prev_fbo = 0;
    static unsigned ssquad_log_n;
    float ndc[16];
    float x0;
    float x1;
    float y0;
    float y1;
    float mx;
    float my;

    if (!s_ready || !s_ssquad_prog || !s_ssquad_vbo || w <= 0 || h <= 0) {
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
    if (x >= DW3_NX_VRAM_W || y >= DW3_NX_VRAM_H) {
        return;
    }
    if (x + w > DW3_NX_VRAM_W) {
        w = DW3_NX_VRAM_W - x;
    }
    if (y + h > DW3_NX_VRAM_H) {
        h = DW3_NX_VRAM_H - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }

    /* Same PS1→NDC scale as MESH_VS (no drawOffset): large inset inside the
     * ClearImage rect so magenta border stays if present samples this band. */
    x0 = (float)x / 512.0f - 1.0f;
    x1 = (float)(x + w) / 512.0f - 1.0f;
    y0 = (float)y / 256.0f - 1.0f;
    y1 = (float)(y + h) / 256.0f - 1.0f;
    mx = (x1 - x0) * 0.12f;
    my = (y1 - y0) * 0.12f;
    x0 += mx;
    x1 -= mx;
    y0 += my;
    y1 -= my;
    /* TRIANGLE_STRIP: BL/BR/TL/TR with dummy UV slots (FS ignores a_t). */
    ndc[0] = x0;
    ndc[1] = y0;
    ndc[2] = 0.0f;
    ndc[3] = 1.0f;
    ndc[4] = x1;
    ndc[5] = y0;
    ndc[6] = 1.0f;
    ndc[7] = 1.0f;
    ndc[8] = x0;
    ndc[9] = y1;
    ndc[10] = 0.0f;
    ndc[11] = 0.0f;
    ndc[12] = x1;
    ndc[13] = y1;
    ndc[14] = 1.0f;
    ndc[15] = 0.0f;

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_mesh_fbo);
    glViewport(0, 0, DW3_NX_VRAM_W, DW3_NX_VRAM_H);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glUseProgram(s_ssquad_prog);
    /* Contrasting cyan on magenta clear — bypasses MESH_VS / game verts. */
    glUniform4f(s_ssquad_color, 0.0f, 1.0f, 1.0f, 1.0f);
    glBindBuffer(GL_ARRAY_BUFFER, s_ssquad_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(ndc), ndc);
    glEnableVertexAttribArray((GLuint)s_ssquad_pos);
    glVertexAttribPointer((GLuint)s_ssquad_pos, 2, GL_FLOAT, GL_FALSE,
                          sizeof(float) * 4, (const void *)0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
    s_had_content = true;

    ssquad_log_n++;
    if (ssquad_log_n <= 8u || (ssquad_log_n % 64u) == 0u) {
        fprintf(stderr, "DIAG ssquad drawn (#%u rect %d,%d %dx%d cyan)\n",
                ssquad_log_n, x, y, w, h);
    }
}

void dw3_nx_mesh_discard(void) {
    s_n_vtx = 0;
    s_n_idx = 0;
    s_had_content = false;
}

void dw3_nx_mesh_mark_vram_dirty(void) { s_sample_dirty = true; }

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
    GLint prev_fbo = 0;
    static unsigned clear_log_n;

    if (!s_ready || w <= 0 || h <= 0) {
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
    if (x >= DW3_NX_VRAM_W || y >= DW3_NX_VRAM_H) {
        return;
    }
    if (x + w > DW3_NX_VRAM_W) {
        w = DW3_NX_VRAM_W - x;
    }
    if (y + h > DW3_NX_VRAM_H) {
        h = DW3_NX_VRAM_H - y;
    }
    /* Rate-limited: first 8 + every 64th (diag: first 16 + every 16th). */
    clear_log_n++;
    {
        int diag = dw3_nx_mesh_diag_enabled();
        unsigned every = diag ? 16u : 64u;
        unsigned first = diag ? 16u : 8u;

        if (clear_log_n <= first || (clear_log_n % every) == 0u) {
            fprintf(stderr,
                    "nx_mesh: clear_rect #%u %d,%d %dx%d rgb=%.3f,%.3f,%.3f "
                    "(~u8 %u,%u,%u)%s\n",
                    clear_log_n, x, y, w, h, r, g, b,
                    (unsigned)(r * 255.0f + 0.5f),
                    (unsigned)(g * 255.0f + 0.5f),
                    (unsigned)(b * 255.0f + 0.5f), diag ? " diag" : "");
        }
    }
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_mesh_fbo);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, w, h);
    glClearColor(r, g, b, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
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

bool dw3_nx_mesh_has_content(void) { return s_had_content; }

uint64_t dw3_nx_mesh_prims_flushed(void) { return s_prims_flushed; }

void dw3_nx_mesh_present(int dx, int dy, int dw, int dh, int vx, int vy, int vw,
                         int vh) {
    if (!s_ready || !s_had_content || dw <= 0 || dh <= 0 || vw <= 0 || vh <= 0) {
        return;
    }
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glViewport(vx, vy, vw, vh);
    glUseProgram(s_blit_prog);
    glUniform2f(s_blit_uv0, (float)dx / (float)DW3_NX_VRAM_W,
                (float)dy / (float)DW3_NX_VRAM_H);
    glUniform2f(s_blit_uv1, (float)(dx + dw) / (float)DW3_NX_VRAM_W,
                (float)(dy + dh) / (float)DW3_NX_VRAM_H);
    glUniform1i(s_blit_tex, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_mesh_tex);
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

    if (!s_ready || !dst || dw <= 0 || dh <= 0) {
        return 0;
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

void dw3_nx_mesh_end_frame(void) { s_had_content = false; }
