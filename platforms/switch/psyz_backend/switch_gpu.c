/* Switch renderer (P4.7 Stage 3): PSY-Z's Draw_* contract on top of PSY-Z's own
 * CPU rasterizer.
 *
 * vendor/psyz ships a self-contained software rasterizer
 * (src/platform/sdl3_software.c) whose DrawBackend vtable lines up with draw.h
 * almost one for one — but nothing in-tree consumes it, so upstream it is dead
 * code and this is its first caller. Driving it is far cheaper than
 * reimplementing PSP's 83 KB psp_gpu.c against GLES2, which is why the PSY-Z
 * Switch branch builds it.
 *
 * Fifteen of the twenty functions the link asks for are pure forwards to that
 * vtable. The remaining five are *display* state — which part of VRAM to show,
 * and how — which no rasterizer owns: they are recorded here and read back by
 * nx_gpu.c when it presents.
 *
 * This file reaches into PSY-Z's private headers (src/draw.h, and
 * src/platform/rasterizer.h for the vtable) because the rasterizer has no public
 * API. That is deliberate, and the coupling is the price of not writing a second
 * renderer. Note the rasterizer's RECT dance: rasterizer.h does
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

/* --- forwards to the portable rasterizer --------------------------------- */

void Draw_Reset(void) { software_rasterizer.reset(); }

void Draw_SetTexpageMode(ParamDrawTexpageMode *p) {
    software_rasterizer.texpage_mode(p);
}

void Draw_SetTextureWindow(unsigned int mask_x, unsigned int mask_y,
                           unsigned int off_x, unsigned int off_y) {
    software_rasterizer.texture_window(mask_x, mask_y, off_x, off_y);
}

void Draw_SetAreaStart(int x, int y) { software_rasterizer.area_start(x, y); }

void Draw_SetAreaEnd(int x, int y) { software_rasterizer.area_end(x, y); }

void Draw_SetOffset(int x, int y) { software_rasterizer.offset(x, y); }

void Draw_SetMask(int bit0, int bit1) {
    software_rasterizer.mask(bit0, bit1);
}

void Draw_ClearImage(PS1_RECT *rect, u_char r, u_char g, u_char b) {
    software_rasterizer.clear_image(rect, r, g, b);
}

void Draw_LoadImage(PS1_RECT *rect, u_long *p) {
    software_rasterizer.load_image(rect, p);
}

void Draw_StoreImage(PS1_RECT *rect, u_long *p) {
    software_rasterizer.store_image(rect, p);
}

void Draw_MoveImage(PS1_RECT *rect, unsigned int x, unsigned int y) {
    software_rasterizer.move_image(rect, x, y);
}

void Draw_ResetBuffer(void) { software_rasterizer.reset_buffer(); }

void Draw_FlushBuffer(void) { software_rasterizer.flush_buffer(); }

int Draw_PushPrim(u_long *packets, int max_len) {
    return software_rasterizer.push_prim(packets, max_len);
}

int Draw_ExequeSync(void) { return software_rasterizer.exeque_sync(); }

/* --- display state (owned here, consumed by the present) ------------------ */

/* Mirrors what sdl3_gpu.c does with the same five calls, including the unit
 * conversion in the range setters: the horizontal range arrives in tenths of a
 * pixel, the vertical one in scanlines.
 *
 * Not modelled yet: PAL, 24bpp and the interlace flag. The title and field
 * modes are NTSC 15bpp, so this is enough for first light; a game mode that
 * changes them will render at the wrong size rather than not at all. */
static int s_disp_on;
static int s_disp_x;
static int s_disp_y;
static int s_disp_w;
static int s_disp_h;
static int s_disp_mode_w;

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
}

void dw3_nx_switch_gpu_display(Dw3NxDisplay *out) {
    if (!out) {
        return;
    }
    out->on = s_disp_on;
    out->x = s_disp_x;
    out->y = s_disp_y;
    /* The range setters win when the game provides them; these are the fallback
     * for a mode that only calls PutDispEnv. */
    out->width = s_disp_w > 0 ? s_disp_w : (s_disp_mode_w > 0 ? s_disp_mode_w : 320);
    out->height = s_disp_h > 0 ? s_disp_h : 240;
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
