/* P6.22 pass 4: the 16-bit VRAM present.
 *
 * The CPU used to expand RGB555 to RGBA8888 (nx_vram_pack.c) and upload 4 bytes
 * a pixel. The present now uploads the VRAM halfwords as they are, in a
 * GL_R16UI texture, and these fragment shaders do the expansion. They are
 * written to give, for every one of the 65 536 halfwords, the byte the pack
 * gives:
 *
 *   R, G, B  (c5 * 255 + 15) / 31 with integer division, c5 = bits 0-4, 5-9,
 *            10-14 (the same arithmetic as color_5to8 in nx_vram_pack.c)
 *   mask     bit 15, which the pack carries in alpha. The present writes alpha
 *            1; the mask only decides the black pixels the partner view keeps
 *            (UI_FS below), exactly as the old `c.a > 0.5` did.
 *
 * Sampling is a texture() on a usampler2D with NEAREST filtering, so which
 * texel a screen pixel reads is chosen by the same hardware path and the same
 * u_uv0 / u_uv1 / v_t as the RGBA8888 texture it replaces. The value that comes
 * out is the halfword, not a float, so there is no rounding to argue about.
 *
 * Needs an OpenGL ES 3.0 context (GLSL ES 3.00, R16UI, GL_UNPACK_ROW_LENGTH);
 * the ES 2 context and a failure to build these fall back to the RGBA8888
 * path. tools/present16_gpu_test.c compiles this header's strings on a real GL
 * ES 3 context and compares every halfword with the pack.
 */
#ifndef DW3_NX_PRESENT16_H
#define DW3_NX_PRESENT16_H

#include <stddef.h>
#include <stdint.h>

#include "nx_output_filter.h"

/* Include after <GLES3/gl3.h>. */

/* The texture coordinate arithmetic of every Nearest program is highp. mediump
 * is only 10 bits of mantissa (a driver may run it in fp16, as Mesa llvmpipe
 * does), which cannot address a texel of a 1024 wide texture: at 4.5x it picked
 * the neighbouring texel for 8% of the pixels, and up to half of them when the
 * rectangle sits in the right half of the VRAM. highp float is mandatory in a
 * GLSL ES 3.00 fragment shader; GLSL ES 1.00 has it only when the driver says so.
 * On a driver whose mediump is fp32 (Mesa nouveau) nothing changes. */
#define DW3_NX_P32_PRECISION                                                   \
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"                                      \
    "precision highp float;\n"                                                 \
    "#else\n"                                                                  \
    "precision mediump float;\n"                                               \
    "#endif\n"

/* The RGBA8888 path (nx_vram_pack.c), GLSL ES 1.00: kept for the ES 2 context,
 * for DW3_PRESENT_16=0 and as the reference the GPU test compares against. */
static const char *const DW3_NX_P32_VS =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_t;\n"
    "varying vec2 v_t;\n"
    "void main() { v_t = a_t; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";

static const char *const DW3_NX_P32_QUAD_FS =
    DW3_NX_P32_PRECISION
    "varying vec2 v_t;\n"
    "uniform vec2 u_uv0;\n"
    "uniform vec2 u_uv1;\n"
    "uniform sampler2D u_tex;\n"
    "void main() {\n"
    "  gl_FragColor = vec4(texture2D(u_tex, u_uv0 + v_t * (u_uv1 - u_uv0)).rgb, 1.0);\n"
    "}\n";

/* Overlay CPU VRAM (sprites/UI). Drop cleared black so the mesh FBO shows.
 * Inside u_keep (x0, y0, x1, y1 in quad units; the partner view, drawn on the
 * software path, with the mask bit on so every pixel it writes has bit 15)
 * black with bit 15 (alpha, see nx_vram_pack.c) is a drawn pixel, such as a
 * dark mask texel or a pupil, and stays. */
static const char *const DW3_NX_P32_UI_FS =
    DW3_NX_P32_PRECISION
    "varying vec2 v_t;\n"
    "uniform vec2 u_uv0;\n"
    "uniform vec2 u_uv1;\n"
    "uniform vec4 u_keep;\n"
    "uniform sampler2D u_tex;\n"
    "void main() {\n"
    "  vec4 c = texture2D(u_tex, u_uv0 + v_t * (u_uv1 - u_uv0));\n"
    "  if (c.r + c.g + c.b < 0.002) {\n"
    "    bool keep = c.a > 0.5 && v_t.x >= u_keep.x && v_t.y >= u_keep.y &&\n"
    "                v_t.x < u_keep.z && v_t.y < u_keep.w;\n"
    "    if (!keep) discard;\n"
    "  }\n"
    "  gl_FragColor = vec4(c.rgb, 1.0);\n"
    "}\n";

static const char *const DW3_NX_P16_VS =
    "#version 300 es\n"
    "in vec2 a_pos;\n"
    "in vec2 a_t;\n"
    "out vec2 v_t;\n"
    "void main() { v_t = a_t; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";

/* highp float, see DW3_NX_P32_PRECISION: the texel the sampler reads is the one
 * under the pixel centre whatever the driver does with mediump. The colour work
 * is integer. */
#define DW3_NX_P16_FS_HEAD                                                     \
    "#version 300 es\n"                                                        \
    "precision highp float;\n"                                                 \
    "precision highp int;\n"                                                   \
    "precision highp usampler2D;\n"                                            \
    "in vec2 v_t;\n"                                                           \
    "uniform vec2 u_uv0;\n"                                                    \
    "uniform vec2 u_uv1;\n"                                                    \
    "uniform usampler2D u_tex;\n"                                              \
    "out highp vec4 o_color;\n"                                                \
    "uint c8(uint c5) { return (c5 * 255u + 15u) / 31u; }\n"                   \
    "highp vec3 rgb555(uint v) {\n"                                            \
    "  return vec3(float(c8(v & 31u)), float(c8((v >> 5) & 31u)),\n"           \
    "              float(c8((v >> 10) & 31u))) / 255.0;\n"                     \
    "}\n"

/* Opaque blit: the picture as it is. */
static const char *const DW3_NX_P16_QUAD_FS =
    DW3_NX_P16_FS_HEAD
    "void main() {\n"
    "  uint v = texture(u_tex, u_uv0 + v_t * (u_uv1 - u_uv0)).r;\n"
    "  o_color = vec4(rgb555(v), 1.0);\n"
    "}\n";

/* Overlay CPU VRAM (sprites/UI) over the battle mesh: drop cleared black so the
 * mesh shows. Inside u_keep (x0, y0, x1, y1 in quad units; the partner view,
 * drawn on the software path with the mask bit on) black with bit 15 is a drawn
 * pixel and stays. All-zero RGB is exactly (v & 0x7FFF) == 0. */
static const char *const DW3_NX_P16_UI_FS =
    DW3_NX_P16_FS_HEAD
    "uniform vec4 u_keep;\n"
    "void main() {\n"
    "  uint v = texture(u_tex, u_uv0 + v_t * (u_uv1 - u_uv0)).r;\n"
    "  if ((v & 0x7FFFu) == 0u) {\n"
    "    bool keep = (v & 0x8000u) != 0u && v_t.x >= u_keep.x &&\n"
    "                v_t.y >= u_keep.y && v_t.x < u_keep.z &&\n"
    "                v_t.y < u_keep.w;\n"
    "    if (!keep) discard;\n"
    "  }\n"
    "  o_color = vec4(rgb555(v), 1.0);\n"
    "}\n";

/* The same two programs with the output filter (nx_output_filter.h: bilinear or
 * sharp bilinear, chosen by u_flt.x). They fetch the halfwords with texelFetch,
 * clamped to the rectangle shown, and expand each tap with rgb555() above, so the
 * colours are the ones the Nearest programs give. They are built only when the
 * player picks a filter. */
static const char *const DW3_NX_P16_QUAD_FS_F =
    DW3_NX_P16_FS_HEAD DW3_NX_FLT_COMMON
    "uint tap(ivec2 i, ivec2 lo, ivec2 hi) {\n"
    "  return texelFetch(u_tex, clamp(i, lo, hi), 0).r;\n"
    "}\n"
    "void main() {\n" DW3_NX_FLT_RECT("u_tex")
    "  highp vec3 c = rgb555(tap(i, lo, hi));\n"
    "  if (w.x != 0.0 || w.y != 0.0) {\n"
    "    highp vec3 c10 = rgb555(tap(i + ivec2(1, 0), lo, hi));\n"
    "    highp vec3 c01 = rgb555(tap(i + ivec2(0, 1), lo, hi));\n"
    "    highp vec3 c11 = rgb555(tap(i + ivec2(1, 1), lo, hi));\n"
    "    c = mix(mix(c, c10, w.x), mix(c01, c11, w.x), w.y);\n"
    "  }\n"
    "  o_color = vec4(c, 1.0);\n"
    "}\n";

/* A texel the Nearest overlay program drops (no colour, no mask bit outside
 * u_keep) is dropped here too, by the texel under the fragment, and does not
 * enter the blend: the weights of the kept taps are renormalised. */
static const char *const DW3_NX_P16_UI_FS_F =
    DW3_NX_P16_FS_HEAD DW3_NX_FLT_COMMON
    "uniform vec4 u_keep;\n"
    "uint tap(ivec2 i, ivec2 lo, ivec2 hi) {\n"
    "  return texelFetch(u_tex, clamp(i, lo, hi), 0).r;\n"
    "}\n"
    "bool shown(uint v, bool inside) {\n"
    "  return (v & 0x7FFFu) != 0u || ((v & 0x8000u) != 0u && inside);\n"
    "}\n"
    "void main() {\n" DW3_NX_FLT_RECT("u_tex")
    "  bool inside = v_t.x >= u_keep.x && v_t.y >= u_keep.y &&\n"
    "                v_t.x < u_keep.z && v_t.y < u_keep.w;\n"
    "  uint v00 = tap(i, lo, hi);\n"
    "  if (w.x == 0.0 && w.y == 0.0) {\n"
    "    if (!shown(v00, inside)) discard;\n"
    "    o_color = vec4(rgb555(v00), 1.0);\n"
    "    return;\n"
    "  }\n"
    "  uint v = tap(ivec2(clamp(floor(t), vec2(lo), vec2(hi))), lo, hi);\n"
    "  if (!shown(v, inside)) discard;\n"
    "  uint v10 = tap(i + ivec2(1, 0), lo, hi);\n"
    "  uint v01 = tap(i + ivec2(0, 1), lo, hi);\n"
    "  uint v11 = tap(i + ivec2(1, 1), lo, hi);\n"
    "  highp vec4 k = vec4((1.0 - w.x) * (1.0 - w.y), w.x * (1.0 - w.y),\n"
    "                      (1.0 - w.x) * w.y, w.x * w.y);\n"
    "  k.x = shown(v00, inside) ? k.x : 0.0;\n"
    "  k.y = shown(v10, inside) ? k.y : 0.0;\n"
    "  k.z = shown(v01, inside) ? k.z : 0.0;\n"
    "  k.w = shown(v11, inside) ? k.w : 0.0;\n"
    "  highp float sum = k.x + k.y + k.z + k.w;\n"
    "  highp vec3 c = rgb555(v);\n"
    "  if (sum > 0.0) {\n"
    "    c = (k.x * rgb555(v00) + k.y * rgb555(v10) + k.z * rgb555(v01) +\n"
    "         k.w * rgb555(v11)) / sum;\n"
    "  }\n"
    "  o_color = vec4(c, 1.0);\n"
    "}\n";

/* The 16-bit upload: the halfwords of VRAM go to an R16UI texture as they are,
 * straight from the VRAM (and gutter) planes with GL_UNPACK_ROW_LENGTH, so the
 * CPU touches nothing. The texture is bound to GL_TEXTURE_2D. (px, py, pw, ph)
 * is the clamped rectangle; with `wide` the display (dx, dy, dw) goes between
 * `gutter` columns of the left and right planes (row stride gstride), the
 * layout dw3_nx_pack_rgb555_wide packs. */
static inline void dw3_nx_p16_upload(const uint16_t *vram,
                                     const uint16_t *left,
                                     const uint16_t *right, int gstride, int px,
                                     int py, int pw, int ph, int wide,
                                     int gutter, int dx, int dy, int dw) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    if (wide) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, gstride);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gutter, ph, GL_RED_INTEGER,
                        GL_UNSIGNED_SHORT, left + (size_t)dy * (size_t)gstride);
        glTexSubImage2D(GL_TEXTURE_2D, 0, gutter + dw, 0, gutter, ph,
                        GL_RED_INTEGER, GL_UNSIGNED_SHORT,
                        right + (size_t)dy * (size_t)gstride);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 1024);
        glTexSubImage2D(GL_TEXTURE_2D, 0, gutter, 0, dw, ph, GL_RED_INTEGER,
                        GL_UNSIGNED_SHORT,
                        vram + (size_t)dy * 1024u + (size_t)dx);
    } else {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 1024);
        glTexSubImage2D(GL_TEXTURE_2D, 0, px, py, pw, ph, GL_RED_INTEGER,
                        GL_UNSIGNED_SHORT,
                        vram + (size_t)py * 1024u + (size_t)px);
    }
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

/* DW3_PRESENT_16=2: the halfwords were copied tight (dw3_nx_copy16_rect /
 * _wide); one contiguous upload of (px, py, pw, ph). */
static inline void dw3_nx_p16_upload_tight(const uint16_t *tight, int px,
                                           int py, int pw, int ph) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glTexSubImage2D(GL_TEXTURE_2D, 0, px, py, pw, ph, GL_RED_INTEGER,
                    GL_UNSIGNED_SHORT, tight);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

#endif
