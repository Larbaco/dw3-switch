/* Output filter (Options > Display > Output filter, docs/OUTPUT_FILTER.md): how the
 * 2D picture is stretched from the PS1 pixels to the screen. Docked, a 320x240
 * picture is 4.5x on a 1080p TV, so with nearest sampling a PS1 pixel is 4 or 5
 * output pixels wide in turn.
 *
 *   0 Nearest         what the present always did (the programs of nx_present16.h
 *                     and nx_mesh.c, untouched: no filtered program is built)
 *   1 Bilinear        a linear blend of the four nearest texels
 *   2 Sharp bilinear  the usual one-pass form: with P = floor(output pixels per
 *                     texel) (at least 1), a texel keeps its colour over all of
 *                     itself except a seam of 1/P texel at each edge, where it
 *                     blends with the neighbour. That is the same picture as an
 *                     integer nearest prescale by P followed by a bilinear stretch
 *                     to the target, without the prescale target. At an integer
 *                     scale (handheld 3x) it is exactly Nearest.
 *
 * The filtered programs fetch the texels themselves (texelFetch on the R16UI
 * VRAM texture, which cannot be filtered by the sampler, and on the RGBA8 mesh
 * target), so the taps are clamped to the sub-rectangle that is shown and never
 * read a neighbouring part of VRAM. When the fragment sits in the middle of a
 * texel (most of them in sharp mode) it is one fetch, like Nearest.
 *
 * In the overlay program (the CPU layer drawn over the battle mesh) a texel that
 * the Nearest program would drop (black without the mask bit, outside u_keep) stays
 * dropped, decided by the texel under the fragment, and does not take part in the
 * blend of its neighbours: a sprite edge keeps its hard outline and has no dark seam.
 *
 * Needs GLSL ES 3.00 (texelFetch, textureSize). The ES 2 context keeps Nearest.
 * tools/output_filter_gpu_test.c compiles these strings on a real GL ES 3 context
 * and compares them with a CPU reference of the same arithmetic.
 */
#ifndef DW3_NX_OUTPUT_FILTER_H
#define DW3_NX_OUTPUT_FILTER_H

enum {
    DW3_NX_FILTER_NEAREST = 0,
    DW3_NX_FILTER_BILINEAR = 1,
    DW3_NX_FILTER_SHARP = 2,
    DW3_NX_FILTER_COUNT = 3
};

/* The words of the log line `dw3-present: output filter <name>`. */
static inline const char *dw3_nx_filter_name(int mode) {
    return mode == DW3_NX_FILTER_BILINEAR ? "bilinear"
           : mode == DW3_NX_FILTER_SHARP  ? "sharp"
                                          : "nearest";
}

/* GLSL shared by the three filtered programs. u_flt = (mode, viewport width,
 * viewport height): mode 1 bilinear, 2 sharp; the viewport is the size on screen
 * of the quad, so the scale is the viewport over the shown texels. flt_tap turns a
 * position t in texels (the fragment's, 0 at the left of the texture) into the
 * first of four taps and their weights; a tap at texel i is centred at i + 0.5. */
#define DW3_NX_FLT_COMMON                                                      \
    "uniform highp vec3 u_flt;\n"                                              \
    "void flt_tap(highp vec2 t, highp vec2 scale, out ivec2 i,\n"              \
    "             out highp vec2 w) {\n"                                       \
    "  highp vec2 pos = t;\n"                                                  \
    "  if (u_flt.x > 1.5) {\n"                                                 \
    "    highp vec2 p = max(floor(scale), vec2(1.0));\n"                       \
    "    highp vec2 f = fract(t) - 0.5;\n"                                     \
    "    highp vec2 r = 0.5 - 0.5 / p;\n"                                      \
    "    pos = floor(t) + (f - clamp(f, -r, r)) * p + 0.5;\n"                  \
    "  }\n"                                                                    \
    "  highp vec2 q = pos - 0.5;\n"                                            \
    "  highp vec2 b = floor(q);\n"                                             \
    "  i = ivec2(b);\n"                                                        \
    "  w = q - b;\n"                                                           \
    "}\n"

/* The texel rectangle shown, from the uniforms the programs already have
 * (u_uv0, u_uv1: its corners in 0..1 of the texture), snapped to whole texels. */
#define DW3_NX_FLT_RECT(SAMPLER)                                               \
    "  highp vec2 size = vec2(textureSize(" SAMPLER ", 0));\n"                 \
    "  highp vec2 r0 = floor(u_uv0 * size + 0.5);\n"                           \
    "  highp vec2 r1 = floor(u_uv1 * size + 0.5);\n"                           \
    "  highp vec2 t = r0 + v_t * (r1 - r0);\n"                                 \
    "  highp vec2 scale = u_flt.yz / (r1 - r0);\n"                             \
    "  ivec2 lo = ivec2(r0);\n"                                                \
    "  ivec2 hi = ivec2(r1) - 1;\n"                                            \
    "  ivec2 i;\n"                                                             \
    "  highp vec2 w;\n"                                                        \
    "  flt_tap(t, scale, i, w);\n"

/* The battle mesh target, composited under the CPU layer (nx_mesh.c BLIT_FS). */
#define DW3_NX_MESH_BLIT_FS_F                                                  \
    "#version 300 es\n"                                                        \
    "precision mediump float;\n"                                               \
    "in vec2 v_t;\n"                                                           \
    "uniform vec2 u_uv0;\n"                                                    \
    "uniform vec2 u_uv1;\n"                                                    \
    "uniform sampler2D u_tex;\n"                                               \
    "out vec4 FragColor;\n" DW3_NX_FLT_COMMON                                  \
    "vec4 tap(ivec2 i, ivec2 lo, ivec2 hi) {\n"                                \
    "  return texelFetch(u_tex, clamp(i, lo, hi), 0);\n"                       \
    "}\n"                                                                      \
    "void main() {\n" DW3_NX_FLT_RECT("u_tex")                                 \
    "  if (w.x == 0.0 && w.y == 0.0) {\n"                                      \
    "    FragColor = tap(i, lo, hi);\n"                                        \
    "    return;\n"                                                            \
    "  }\n"                                                                    \
    "  FragColor = mix(mix(tap(i, lo, hi), tap(i + ivec2(1, 0), lo, hi), w.x),\n" \
    "                  mix(tap(i + ivec2(0, 1), lo, hi),\n"                    \
    "                      tap(i + ivec2(1, 1), lo, hi), w.x), w.y);\n"        \
    "}\n"

#endif /* DW3_NX_OUTPUT_FILTER_H */
