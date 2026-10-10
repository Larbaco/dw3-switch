/*
 * P3.7 present wave: PSY-Z leaves DMACallback as NOT_IMPLEMENTED and
 * DecDCTout does not raise the MDEC-out DMA IRQ. The title movie player
 * waits on DecDCToutCallback (slice → LoadImage → isdone), so playback
 * stalls after the STR ring fills.
 *
 * Also: sdl3_gpu LoadImage always treats payloads as RGB5551, but opening
 * STRs are 24-bit. Convert halfword-packed RGB888 → RGB5551 in pixel
 * coordinates before the real LoadImage so present/FB dump show real frames.
 *
 * Linked with -Wl,--wrap=DMACallback,DecDCTout,LoadImage,PutDispEnv.
 */
#include <libetc.h>
#include <libgpu.h>
#include <libpress.h>
#include <psyz/video.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { DW3_DMA_CHANS = 8, DW3_DMA_MDEC_OUT = 1 };

static void (*g_dma_cb[DW3_DMA_CHANS])(void);
static int g_disp_rgb24;
static unsigned g_slice_cb_count;
static unsigned g_rgb24_loads;
static unsigned g_dout_depth;
static unsigned g_dout_top_done;
static int g_fb_dumped;

static void dw3_present_dump_fb(const char *why) {
    const char *path;
    unsigned char *rgb;
    int w = 0;
    int h = 0;
    FILE *fp;
    size_t nbytes;
    size_t wrote;
    long nonzero = 0;
    size_t i;

    if (g_fb_dumped) {
        return;
    }
    path = getenv("DW3_FB_DUMP");
    if (path == NULL || path[0] == '\0') {
        return;
    }
    rgb = Psyz_VideoAllocCapturedFrame(&w, &h);
    if (rgb == NULL || w <= 0 || h <= 0) {
        rgb = Psyz_VideoAllocVramDump(&w, &h);
    }
    if (rgb == NULL || w <= 0 || h <= 0) {
        fprintf(stderr, "dw3-present: FB dump failed (%s)\n", why);
        return;
    }
    nbytes = (size_t)w * (size_t)h * 3u;
    for (i = 0; i < nbytes; i++) {
        if (rgb[i] != 0) {
            nonzero++;
        }
    }
    fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "dw3-present: cannot write %s\n", path);
        free(rgb);
        return;
    }
    fprintf(fp, "P6\n%d %d\n255\n", w, h);
    wrote = fwrite(rgb, 1, nbytes, fp);
    fclose(fp);
    free(rgb);
    g_fb_dumped = 1;
    fprintf(stderr,
            "dw3-present: FB dump %s %dx%d wrote=%zu nonzero=%ld (%s)\n", path,
            w, h, wrote, nonzero, why);
}

void *__real_DMACallback(int dma, void (*func)());
void __real_DecDCTout(u_long *buf, int size);
int __real_LoadImage(RECT *rect, u_long *p);
DISPENV *__real_PutDispEnv(DISPENV *env);

void *__wrap_DMACallback(int dma, void (*func)()) {
    void *prev = NULL;

    (void)__real_DMACallback; /* stub logs NOT_IMPLEMENTED; we own the table */
    if (dma >= 0 && dma < DW3_DMA_CHANS) {
        prev = (void *)g_dma_cb[dma];
        g_dma_cb[dma] = func;
    }
    return prev;
}

void __wrap_DecDCTout(u_long *buf, int size) {
    g_dout_depth++;
    __real_DecDCTout(buf, size);
    /* Soft MDEC finishes synchronously; raise MDEC-out DMA callback. */
    if (g_dma_cb[DW3_DMA_MDEC_OUT] != NULL) {
        g_dma_cb[DW3_DMA_MDEC_OUT]();
        g_slice_cb_count++;
        if (g_slice_cb_count == 1u) {
            fprintf(stderr,
                    "dw3-present: MDEC-out DMA callback armed (first slice)\n");
        }
    }
    g_dout_depth--;
    if (g_dout_depth == 0u) {
        g_dout_top_done++;
        if (g_dout_top_done <= 3u) {
            fprintf(stderr,
                    "dw3-present: DecDCTout frame done #%u slices=%u "
                    "rgb24_loads=%u rgb24=%d\n",
                    g_dout_top_done, g_slice_cb_count, g_rgb24_loads,
                    g_disp_rgb24);
        }
        /* Dump after a RGB24 frame so PutDispEnv(isrgb24) has latched. */
        if (g_disp_rgb24 && g_rgb24_loads >= 20u && !g_fb_dumped) {
            dw3_present_dump_fb("after RGB24 frame");
        }
    }
}

DISPENV *__wrap_PutDispEnv(DISPENV *env) {
    if (env != NULL) {
        g_disp_rgb24 = env->isrgb24 ? 1 : 0;
    }
    return __real_PutDispEnv(env);
}

static u16 *rgb888_packed_to_5551(const u8 *src, int pixel_w, int h) {
    size_t n = (size_t)pixel_w * (size_t)h;
    u16 *out;
    int y, x;

    out = (u16 *)malloc(n * sizeof(u16));
    if (out == NULL) {
        return NULL;
    }
    for (y = 0; y < h; y++) {
        const u8 *row = src + (size_t)y * (size_t)pixel_w * 3u;
        u16 *dst = out + (size_t)y * (size_t)pixel_w;
        for (x = 0; x < pixel_w; x++) {
            unsigned r = row[0] >> 3;
            unsigned g = row[1] >> 3;
            unsigned b = row[2] >> 3;
            dst[x] = (u16)(r | (g << 5) | (b << 10) | 0x8000);
            row += 3;
        }
    }
    return out;
}

int __wrap_LoadImage(RECT *rect, u_long *p) {
    RECT pr;
    u16 *converted;
    int pixel_w;
    int rc;
    int use_rgb24;

    if (rect == NULL || p == NULL || rect->w <= 0 || rect->h <= 0) {
        return __real_LoadImage(rect, p);
    }
    /* PutDispEnv(isrgb24) often latches after the first movie frame; STR
     * slices are halfword-width multiples of 3 and tall (≥192). */
    use_rgb24 =
        g_disp_rgb24 || ((rect->w % 3) == 0 && rect->h >= 192);
    if (!use_rgb24) {
        return __real_LoadImage(rect, p);
    }

    /* PS1 24-bit: RECT is in halfwords; pixels = halfwords * 2 / 3. */
    pixel_w = rect->w * 2 / 3;
    if (pixel_w <= 0) {
        return __real_LoadImage(rect, p);
    }
    converted = rgb888_packed_to_5551((const u8 *)p, pixel_w, rect->h);
    if (converted == NULL) {
        return __real_LoadImage(rect, p);
    }
    pr = *rect;
    pr.x = (short)(rect->x * 2 / 3);
    pr.w = (short)pixel_w;
    rc = __real_LoadImage(&pr, (u_long *)converted);
    free(converted);
    g_rgb24_loads++;
    if (g_rgb24_loads == 1u || g_rgb24_loads == 20u) {
        fprintf(stderr,
                "dw3-present: RGB24 LoadImage #%u %dx%d hw -> %dx%d px at "
                "(%d,%d)\n",
                g_rgb24_loads, rect->w, rect->h, pr.w, pr.h, pr.x, pr.y);
    }
    return rc;
}
