/* Switch renderer (P4.7 Stage 3 + P5.2 mesh GPU):
 * PSY-Z's Draw_* contract on top of PSY-Z's CPU rasterizer for 2D, with OT
 * triangle polys (battle mesh) routed to a GLES3 FBO — see docs/P5.2_BATTLE_GPU.md.
 *
 * Fifteen of the twenty functions the link asks for are pure forwards to that
 * vtable (or thin wrappers). Display state is recorded here for nx_gpu.c.
 *
 * This file reaches into PSY-Z's private headers (src/draw.h, and
 * src/platform/rasterizer.h for the vtable) because the rasterizer has no public
 * API. Note the rasterizer's RECT dance: rasterizer.h does
 * `#define RECT PS1_RECT`, includes draw.h and undefs it, so the image-handling
 * signatures below have to say PS1_RECT to match what the compiler saw.
 *
 * Draw_SetHorizontalGrid is deliberately absent: the link does not ask for it,
 * so defining it here would only risk a duplicate symbol.
 */
#include <psyz.h>
#include <psyz/video.h>

#include "rasterizer.h"
#include "nx.h"
#include "nx_battle_mode.h"
#include "nx_mesh.h"
#include "nx_mesh_debug.h"
#include "overlay_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* US / EU FILE_FIGHTSTG (engine/overlay.h). Mesh GPU is battle-only. */
#define DW3_FILE_FIGHTSTG_US 0x159
#define DW3_FILE_FIGHTSTG_EU 0x167

/* GP0 code bits (same as PSY-Z sdl3_draw.h). */
#define SEMITRANSP 0x02
#define TEXTURED 0x04
#define EXTRA_VERTEX 0x08
#define GOURAUD 0x10
#define TRIANGLE 0x20

#define TWIN_PACK(and_x, and_y, or_x, or_y)                                    \
    ((unsigned int)(and_x) | ((unsigned int)(and_y) << 8) |                    \
     ((unsigned int)(or_x) << 16) | ((unsigned int)(or_y) << 24))

static uint16_t s_cur_tpage;
static int s_dither_env;
static uint32_t s_cur_twin = TWIN_PACK(0xFF, 0xFF, 0x00, 0x00);
static int s_area_x0;
static int s_area_y0;
static int s_area_x1 = 1023;
static int s_area_y1 = 511;

static short s11(short v) { return (short)(((v & 0x7FF) ^ 1024) - 1024); }

/* preview.9 routed field POLY_GT through the mesh FBO + black-discard UI
 * composite and corrupted interiors while also costing ~1.4× field dt.
 * Only FIGHTSTG (mode overlay file) may enqueue. */
static int s_was_fightstg;
static int s_fight_enter_logged;

static int mesh_battle_active(void) {
    int32_t mode_file = dw3_overlay_current_mode_file();
    int fight = mode_file == DW3_FILE_FIGHTSTG_US ||
                mode_file == DW3_FILE_FIGHTSTG_EU;

    if (fight && !s_was_fightstg) {
        /* Fresh sample upload after field LoadImages; drop any stale FBO flag. */
        dw3_nx_mesh_mark_vram_dirty();
        dw3_nx_mesh_discard();
        if (!s_fight_enter_logged) {
            s_fight_enter_logged = 1;
            fprintf(stderr,
                    "nx_mesh: FIGHTSTG battle_mode=%s (GLES mesh %s)\n",
                    dw3_nx_battle_mode_token(),
                    dw3_nx_battle_mode_use_soft()
                        ? "off — software RasterTriangle"
                        : "on");
        }
    } else if (!fight && s_was_fightstg) {
        dw3_nx_mesh_discard();
    }
    s_was_fightstg = fight;
    return fight;
}

static int write_packet(Dw3NxMeshVertex *v, int code, int n, u_long *packet,
                        uint16_t *p_out) {
    int w;

    if (!n) {
        return 0;
    }
    v->x = s11(((short *)packet)[0]);
    v->y = s11(((short *)packet)[1]);
    packet++;
    n--;
    if (!n) {
        return 1;
    }
    w = 1;
    if (code & TEXTURED) {
        v->u = ((u_char *)packet)[0];
        v->v = ((u_char *)packet)[1];
        *p_out = ((u_short *)packet)[1];
        w++;
        packet++;
        n--;
        if (!n) {
            return w;
        }
    } else {
        *p_out = 0;
    }
    if (code & GOURAUD) {
        v++;
        v->r = ((u_char *)packet)[0];
        v->g = ((u_char *)packet)[1];
        v->b = ((u_char *)packet)[2];
        v->a = (code & SEMITRANSP) ? 0x80 : 0xFF;
        w++;
    }
    return w;
}

static void fixup_flip_uv(Dw3NxMeshVertex *v, int has_four) {
    int fix_u = (v[0].x > v[1].x) ^ (v[0].u > v[1].u);
    int fix_v = (v[0].y > v[1].y) ^ (v[0].v > v[1].v);

    fix_u |= (v[0].x > v[2].x) ^ (v[0].u > v[2].u);
    fix_v |= (v[0].y > v[2].y) ^ (v[0].v > v[2].v);
    if (fix_u) {
        v[0].u++;
        v[1].u++;
        v[2].u++;
        if (has_four) {
            v[3].u++;
        }
    }
    if (fix_v) {
        v[0].v++;
        v[1].v++;
        v[2].v++;
        if (has_four) {
            v[3].v++;
        }
    }
}

static int can_poly_dither(int is_gouraud, int is_textured, int is_shade_tex) {
    if (!s_dither_env) {
        return 0;
    }
    if (Psyz_VideoGetDitheringMode() == PSYZ_DITHER_OFF) {
        return 0;
    }
    return is_gouraud || (is_textured && is_shade_tex);
}

/* Returns words consumed when handled on GPU, or 0 to fall back to software. */
static int try_push_mesh_poly(u_long *packets, int max_len) {
    int code;
    int is_poly;
    int is_textured;
    int is_gouraud;
    int is_shade_tex;
    int len;
    Dw3NxMeshVertex v[5];
    uint16_t idx[6];
    uint16_t tpage = 0;
    uint16_t clut = 0;
    uint16_t pad2 = 0;
    uint16_t pad3 = 0;
    int wr;
    int n_vertices;
    int n_indices;
    int i;

    if (!dw3_nx_mesh_available() || !mesh_battle_active() || max_len <= 0 ||
        !packets) {
        return 0;
    }
    /* soft / soft+bbox: FIGHTSTG stays gated, triangles fill CPU VRAM. */
    if (dw3_nx_battle_mode_use_soft()) {
        return 0;
    }
    code = (int)(*packets >> 24) & 0xFF;
    is_poly = !(code & 0x40);
    if (!is_poly || !(code & TRIANGLE)) {
        return 0;
    }

    is_textured = (code & TEXTURED) != 0;
    is_gouraud = (code & GOURAUD) != 0;
    is_shade_tex = !((code & 1) && is_textured);
    len = max_len;
    memset(v, 0, sizeof(v));

    if (is_shade_tex) {
        v[0].r = (unsigned char)(*packets >> 0);
        v[0].g = (unsigned char)(*packets >> 8);
        v[0].b = (unsigned char)(*packets >> 16);
    } else {
        v[0].r = v[0].g = v[0].b = 0x80;
    }
    v[0].a = (code & SEMITRANSP) ? 0x80 : 0xFF;
    packets++;
    len--;

    wr = write_packet(&v[0], code, len, packets, &clut);
    packets += wr;
    len -= wr;
    wr = write_packet(&v[1], code, len, packets, &tpage);
    packets += wr;
    len -= wr;
    wr = write_packet(&v[2], code, len, packets, &pad2);
    packets += wr;
    len -= wr;

    idx[0] = 0;
    idx[1] = 1;
    idx[2] = 2;
    if (code & EXTRA_VERTEX) {
        idx[3] = 1;
        idx[4] = 3;
        idx[5] = 2;
        n_vertices = 4;
        n_indices = 6;
        wr = write_packet(&v[3], code, len, packets, &pad3);
        packets += wr;
        len -= wr;
    } else {
        n_vertices = 3;
        n_indices = 3;
    }
    if (is_gouraud && wr == 2 + is_textured) {
        packets--;
        len++;
    }

    if (is_textured) {
        fixup_flip_uv(v, code & EXTRA_VERTEX);
    } else {
        clut = 0xFFFF;
        tpage = (uint16_t)(s_cur_tpage | DW3_NX_MESH_TPAGE_NOTEXTURE);
    }
    if (can_poly_dither(is_gouraud, is_textured, is_shade_tex)) {
        tpage = (uint16_t)(tpage | DW3_NX_MESH_TPAGE_DITHER);
    }
    if (!is_gouraud || !is_shade_tex) {
        for (i = 1; i < n_vertices; i++) {
            v[i].r = v[0].r;
            v[i].g = v[0].g;
            v[i].b = v[0].b;
            v[i].a = v[0].a;
        }
    }
    /* gpu-solid (#79 ladder): ignore texture/CLUT; vertex colour only. */
    if (dw3_nx_battle_mode_use_solid()) {
        tpage = (uint16_t)(tpage | DW3_NX_MESH_TPAGE_NOTEXTURE);
        /* DIAG: constant magenta verts — if screen stays setBgColor blue,
         * fragments are not landing on the presented band. */
        if (dw3_nx_mesh_diag_enabled()) {
            for (i = 0; i < n_vertices; i++) {
                v[i].r = 255;
                v[i].g = 0;
                v[i].b = 255;
                v[i].a = 0xFF;
            }
        }
    }
    for (i = 0; i < n_vertices; i++) {
        v[i].clut = clut;
        v[i].tpage = tpage;
        v[i].twin = s_cur_twin;
    }

    dw3_nx_mesh_enqueue(v, n_vertices, idx, n_indices,
                        (const uint16_t *)SoftwareRasterizer_GetVram());
    dw3_nx_mesh_debug_note_poly_code(code, is_textured);
    return max_len - len;
}

/* --- forwards to the portable rasterizer --------------------------------- */

void Draw_Reset(void) {
    software_rasterizer.reset();
    s_cur_tpage = 0;
    s_dither_env = 0;
    s_cur_twin = TWIN_PACK(0xFF, 0xFF, 0x00, 0x00);
    dw3_nx_mesh_set_tpage(0, 0);
    dw3_nx_mesh_set_twin(s_cur_twin);
}

void Draw_SetTexpageMode(ParamDrawTexpageMode *p) {
    unsigned short mode;

    software_rasterizer.texpage_mode(p);
    if (!p) {
        return;
    }
    mode = *(unsigned short *)p;
    s_cur_tpage = (uint16_t)(mode & 0x1FFu);
    s_dither_env = (mode & 0x200) ? 1 : 0;
    dw3_nx_mesh_set_tpage(s_cur_tpage, s_dither_env);
}

void Draw_SetTextureWindow(unsigned int mask_x, unsigned int mask_y,
                           unsigned int off_x, unsigned int off_y) {
    software_rasterizer.texture_window(mask_x, mask_y, off_x, off_y);
    mask_x &= 0x1F;
    mask_y &= 0x1F;
    s_cur_twin = TWIN_PACK((unsigned char)~(mask_x * 8),
                           (unsigned char)~(mask_y * 8),
                           (unsigned char)((off_x & mask_x) * 8),
                           (unsigned char)((off_y & mask_y) * 8));
    dw3_nx_mesh_set_twin(s_cur_twin);
}

void Draw_SetAreaStart(int x, int y) {
    software_rasterizer.area_start(x, y);
    s_area_x0 = x;
    s_area_y0 = y;
    dw3_nx_mesh_set_area(s_area_x0, s_area_y0, s_area_x1, s_area_y1);
}

void Draw_SetAreaEnd(int x, int y) {
    software_rasterizer.area_end(x, y);
    s_area_x1 = x;
    s_area_y1 = y;
    dw3_nx_mesh_set_area(s_area_x0, s_area_y0, s_area_x1, s_area_y1);
}

void Draw_SetOffset(int x, int y) {
    if (mesh_battle_active()) {
        dw3_nx_mesh_flush((const uint16_t *)SoftwareRasterizer_GetVram());
    } else {
        dw3_nx_mesh_discard();
    }
    software_rasterizer.offset(x, y);
    dw3_nx_mesh_set_offset(x, y);
}

void Draw_SetMask(int bit0, int bit1) {
    software_rasterizer.mask(bit0, bit1);
}

void Draw_ClearImage(PS1_RECT *rect, u_char r, u_char g, u_char b) {
    if (mesh_battle_active()) {
        dw3_nx_mesh_flush((const uint16_t *)SoftwareRasterizer_GetVram());
    } else {
        dw3_nx_mesh_discard();
    }
    software_rasterizer.clear_image(rect, r, g, b);
    dw3_nx_mesh_mark_vram_dirty();
    if (mesh_battle_active() && rect && rect->w > 0 && rect->h > 0) {
        float cr = (float)r / 255.0f;
        float cg = (float)g / 255.0f;
        float cb = (float)b / 255.0f;

        /* DIAG + gpu-solid: known magenta clear (falsify setBgColor blue). */
        if (dw3_nx_mesh_diag_enabled() && dw3_nx_battle_mode_use_solid()) {
            cr = 1.0f;
            cg = 0.0f;
            cb = 1.0f;
        }
        dw3_nx_mesh_clear_rect(rect->x, rect->y, rect->w, rect->h, cr, cg, cb);
        /* DIAG: clip-space cyan ssquad after clear — splits present OK vs
         * GTE/packet/transform broken (no game verts). */
        if (dw3_nx_mesh_diag_enabled() && dw3_nx_battle_mode_use_solid()) {
            dw3_nx_mesh_diag_ssquad(rect->x, rect->y, rect->w, rect->h);
        }
    }
}

void Draw_LoadImage(PS1_RECT *rect, u_long *p) {
    if (mesh_battle_active()) {
        dw3_nx_mesh_flush((const uint16_t *)SoftwareRasterizer_GetVram());
    } else {
        dw3_nx_mesh_discard();
    }
    software_rasterizer.load_image(rect, p);
    dw3_nx_mesh_mark_vram_dirty();
}

void Draw_StoreImage(PS1_RECT *rect, u_long *p) {
    if (mesh_battle_active()) {
        dw3_nx_mesh_flush((const uint16_t *)SoftwareRasterizer_GetVram());
    } else {
        dw3_nx_mesh_discard();
    }
    software_rasterizer.store_image(rect, p);
}

void Draw_MoveImage(PS1_RECT *rect, unsigned int x, unsigned int y) {
    if (mesh_battle_active()) {
        dw3_nx_mesh_flush((const uint16_t *)SoftwareRasterizer_GetVram());
    } else {
        dw3_nx_mesh_discard();
    }
    software_rasterizer.move_image(rect, x, y);
    dw3_nx_mesh_mark_vram_dirty();
}

void Draw_ResetBuffer(void) {
    software_rasterizer.reset_buffer();
}

void Draw_FlushBuffer(void) {
    if (mesh_battle_active()) {
        dw3_nx_mesh_flush((const uint16_t *)SoftwareRasterizer_GetVram());
    } else {
        dw3_nx_mesh_discard();
    }
    software_rasterizer.flush_buffer();
}

int Draw_PushPrim(u_long *packets, int max_len) {
    int consumed = try_push_mesh_poly(packets, max_len);

    if (consumed > 0) {
        return consumed;
    }
#if DW3_NX_DEBUG_MESH
    if (packets && max_len > 0) {
        int code = (int)(*packets >> 24) & 0xFF;

        if (!(code & 0x40) && (code & TRIANGLE) &&
            (!dw3_nx_mesh_available() ||
             (mesh_battle_active() && dw3_nx_battle_mode_use_soft()))) {
            dw3_nx_mesh_debug_note_soft_fallback();
        }
    }
#endif
    return software_rasterizer.push_prim(packets, max_len);
}

int Draw_ExequeSync(void) {
    if (mesh_battle_active()) {
        dw3_nx_mesh_flush((const uint16_t *)SoftwareRasterizer_GetVram());
    } else {
        dw3_nx_mesh_discard();
    }
    return software_rasterizer.exeque_sync();
}

/* --- display state (owned here, consumed by the present) ------------------ */

/* Mirrors what sdl3_gpu.c does with the same five calls, including the unit
 * conversion in the range setters: the horizontal range arrives in tenths of a
 * pixel, the vertical one in scanlines.
 *
 * PAL is recorded only so the present aspect uses the 288-line vref.
 * Interlace is still ignored. 24bpp selects the movie blit height. */
static int s_disp_on;
static int s_disp_x;
static int s_disp_y;
static int s_disp_w;
static int s_disp_h;
static int s_disp_mode_w;
static int s_disp_pal;
/* 24-bit movie mode only. The 480-line bit is not enough: an early PutDispEnv
 * with disp.h=640 sets it before the title, and presenting that height crops
 * the menu. isrgb24 is the movie. */
static int s_disp_rgb24;

void Draw_DisplayEnable(unsigned int on) { s_disp_on = (int)on; }

void Draw_DisplayArea(unsigned int x, unsigned int y) {
    s_disp_x = (int)x;
    s_disp_y = (int)y;
}

void Draw_DisplayHorizontalRange(unsigned int start, unsigned int end) {
    s_disp_w = (int)(end - start) / 10;
}

void Draw_DisplayVerticalRange(unsigned int start, unsigned int end) {
    s_disp_h = (int)(end - start);
}

void Draw_SetDisplayMode(DisplayMode *mode) {
    if (!mode) {
        return;
    }
    if (mode->horizontal_resolution_368) {
        s_disp_mode_w = 368;
    } else {
        switch (mode->horizontal_resolution) {
        case 0:
            s_disp_mode_w = 256;
            break;
        case 1:
            s_disp_mode_w = 320;
            break;
        case 2:
            s_disp_mode_w = 512;
            break;
        default:
            s_disp_mode_w = 640;
            break;
        }
    }
    s_disp_rgb24 = mode->rgb24 ? 1 : 0;
    s_disp_pal = mode->pal ? 1 : 0;
}

void dw3_nx_switch_gpu_display(Dw3NxDisplay *out) {
    if (!out) {
        return;
    }
    out->on = s_disp_on;
    out->x = s_disp_x;
    out->y = s_disp_y;
    /* Blit width is the display mode, matching the PC present. The horizontal
     * range is in tenths of a dot clock and comes out 256 for this title, so
     * using it as the width cropped the right 64 columns of a 320-wide frame
     * (the "3") and left the logo sitting to the right. Height stays on the
     * vertical range, which is already in scanlines. */
    out->width = s_disp_mode_w > 0 ? s_disp_mode_w : (s_disp_w > 0 ? s_disp_w : 320);
    /* Title keeps the vertical range (scanlines). A 24-bit movie sets the
     * display to 480, but STDWTITL_onSliceDecoded writes the 416-line picture
     * at VRAM y=36. The lines above and below that are blank, and sampling
     * them is the small bar over and under the video. */
    if (s_disp_rgb24) {
        out->y = 36;
        out->height = 416;
    } else {
        out->height = s_disp_h > 0 ? s_disp_h : 240;
    }
    /* Aspect inputs. These stay the display ranges even when the blit
     * height is 480: PSY-Z does not treat that framebuffer as 2:3. */
    out->range_w = s_disp_w;
    out->range_h = s_disp_h;
    out->pal = s_disp_pal;
}

const uint16_t *dw3_nx_switch_gpu_vram(void) {
    return (const uint16_t *)SoftwareRasterizer_GetVram();
}

/* --- video state the rasterizer reads back -------------------------------- */

/* sdl3_software.c consults this per triangle (RasterTriangle). The shipped
 * renderers wire it to a user setting; nothing here exposes one yet, so AUTO
 * stands, which is also PSY-Z's documented default. */
static PsyzDitherMode s_dither = PSYZ_DITHER_AUTO;

PsyzDitherMode Psyz_VideoGetDitheringMode(void) { return s_dither; }

int Psyz_VideoSetDitheringMode(PsyzDitherMode mode) {
    if (mode != PSYZ_DITHER_AUTO && mode != PSYZ_DITHER_OFF) {
        return -1;
    }
    s_dither = mode;
    return 0;
}
