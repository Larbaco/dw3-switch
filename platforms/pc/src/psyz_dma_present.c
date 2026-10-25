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
 * PutDispEnv must use the same halfword→pixel X scale when isrgb24: movie
 * double-buffers sit at disp.x 0 and 480 (PS1 halfwords), but LoadImage
 * stores expanded frames at 0 and 320. Leaving disp.x=480 shows only the
 * right 160px of the frame (classic half-width glitch).
 *
 * Soft MDEC completes synchronously. STDWTITL_onSliceDecoded starts the next
 * DecDCTout before LoadImage of the completed slice, relying on async DMA +
 * two imgbufs. Nested sync callbacks overwrite those bufs before LoadImage
 * runs → macroblock garbage in VRAM. Stash each DecDCTout payload and feed
 * LoadImage from the oldest unconsumed stash for that pointer.
 *
 * Linked with
 * -Wl,--wrap=DMACallback,DecDCTReset,DecDCTout,LoadImage,PutDispEnv.
 *
 * DecDCTReset on PSY-Z only clears soft-MDEC state; Sony's MDEC_reset also
 * uploads default quant/IDCT. Without that, AC run-levels decode to flat
 * mid-grey (quant[i]==0). Host wrap uploads Psy-Q defaults via
 * Psyz_MdecCommand (libpress PutEnv→MDEC_in is LP64-unsafe on u32 tables).
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
/* One MOVIEOPN frame = 20 strips; keep headroom for nesting. */
enum { DW3_SLICE_STASH_SLOTS = 32, DW3_SLICE_STASH_BYTES = 0x4E00 };

static void (*g_dma_cb[DW3_DMA_CHANS])(void);
static int g_disp_rgb24;
static unsigned g_slice_cb_count;
static unsigned g_rgb24_loads;
static unsigned g_dout_depth;
static unsigned g_dout_top_done;
static int g_fb_dumped;
static int g_disp_x_scaled_logged;
static int g_mdec_env_logged;
static int g_slice_stash_logged;
static int g_rgb24_latch_cleared_logged;
static unsigned g_put_disp_count;

typedef struct Dw3SliceStash {
    const void *ptr;
    int nbytes;
    int consumed;
    unsigned seq;
    uint8_t data[DW3_SLICE_STASH_BYTES];
} Dw3SliceStash;

static Dw3SliceStash g_slice_stash[DW3_SLICE_STASH_SLOTS];
static unsigned g_slice_stash_write;
static unsigned g_slice_stash_seq;

static void dw3_slice_stash_push(const void *buf, int nbytes) {
    Dw3SliceStash *slot;

    if (buf == NULL || nbytes <= 0) {
        return;
    }
    if (nbytes > DW3_SLICE_STASH_BYTES) {
        nbytes = DW3_SLICE_STASH_BYTES;
    }
    slot = &g_slice_stash[g_slice_stash_write % DW3_SLICE_STASH_SLOTS];
    g_slice_stash_write++;
    slot->ptr = buf;
    slot->nbytes = nbytes;
    slot->consumed = 0;
    slot->seq = ++g_slice_stash_seq;
    memcpy(slot->data, buf, (size_t)nbytes);
    if (!g_slice_stash_logged) {
        g_slice_stash_logged = 1;
        fprintf(stderr,
                "dw3-present: DecDCTout slice stash armed (sync MDEC "
                "double-buf)\n");
    }
}

static const uint8_t *dw3_slice_stash_take(const void *buf) {
    unsigned i;
    Dw3SliceStash *best = NULL;

    if (buf == NULL || g_slice_stash_seq == 0u) {
        return NULL;
    }
    /* Newest unconsumed entry for this pointer. Nested sync callbacks
     * LoadImage on the way back out of the stack (innermost first), which
     * matches LIFO per imgbuf pointer. */
    for (i = 0; i < DW3_SLICE_STASH_SLOTS; i++) {
        Dw3SliceStash *slot = &g_slice_stash[i];

        if (slot->ptr != buf || slot->nbytes <= 0 || slot->consumed) {
            continue;
        }
        if (best == NULL || slot->seq > best->seq) {
            best = slot;
        }
    }
    if (best == NULL) {
        return NULL;
    }
    best->consumed = 1;
    return best->data;
}

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
    int x0 = -1;
    int x1 = -1;
    int y0 = -1;
    int y1 = -1;
    int x;
    int y;
    unsigned uniq = 0;
    unsigned char seen[256];

    if (g_fb_dumped) {
        return;
    }
    path = getenv("DW3_FB_DUMP");
    if (path == NULL || path[0] == '\0') {
        return;
    }
    /* Prefer full VRAM dump for movies: captured display buffer can miss
     * the RGB24 double-buffer band that LoadImage wrote. */
    rgb = Psyz_VideoAllocVramDump(&w, &h);
    if (rgb == NULL || w <= 0 || h <= 0) {
        rgb = Psyz_VideoAllocCapturedFrame(&w, &h);
    }
    if (rgb == NULL || w <= 0 || h <= 0) {
        fprintf(stderr, "dw3-present: FB dump failed (%s)\n", why);
        return;
    }
    nbytes = (size_t)w * (size_t)h * 3u;
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < nbytes; i++) {
        if (rgb[i] != 0) {
            nonzero++;
        }
        if (!seen[rgb[i]]) {
            seen[rgb[i]] = 1;
            uniq++;
        }
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            size_t off = ((size_t)y * (size_t)w + (size_t)x) * 3u;
            if (rgb[off] | rgb[off + 1] | rgb[off + 2]) {
                if (x0 < 0 || x < x0) {
                    x0 = x;
                }
                if (x1 < 0 || x > x1) {
                    x1 = x;
                }
                if (y0 < 0 || y < y0) {
                    y0 = y;
                }
                if (y1 < 0 || y > y1) {
                    y1 = y;
                }
            }
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
            "dw3-present: FB dump %s %dx%d wrote=%zu nonzero=%ld "
            "uniq8=%u bbox=%d,%d-%d,%d (%s)\n",
            path, w, h, wrote, nonzero, uniq, x0, y0, x1, y1, why);
}

void *__real_DMACallback(int dma, void (*func)());
void __real_DecDCTReset(int mode);
void __real_DecDCTout(u_long *buf, int size);
int __real_LoadImage(RECT *rect, u_long *p);
DISPENV *__real_PutDispEnv(DISPENV *env);

/* PSY-Z private soft-MDEC entry (libpress MDEC_in is LP64-unsafe for u32[]). */
int Psyz_MdecCommand(uint32_t command, const void *data, size_t words);

static void dw3_mdec_upload_default_tables(void) {
    DECDCTENV env;
    uint8_t quant[128];

    DecDCTGetEnv(&env);
    /* libpress DecDCTPutEnv → MDEC_in((u_long *)mdec_iq, 32) indexes a u32[]
     * table through u_long* — on LP64 that skips the first quant word and
     * feeds misaligned tables (macroblock garbage with non-zero AC). */
    memcpy(quant, env.iq_y, 64);
    memcpy(quant + 64, env.iq_c, 64);
    Psyz_MdecCommand(0x40000001u, quant, 32);
    Psyz_MdecCommand(0x60000000u, env.dct, 32);
}

void __wrap_DecDCTReset(int mode) {
    __real_DecDCTReset(mode);
    dw3_mdec_upload_default_tables();
    if (!g_mdec_env_logged) {
        g_mdec_env_logged = 1;
        fprintf(stderr,
                "dw3-present: DecDCTReset → Psyz_MdecCommand default "
                "quant/IDCT (LP64-safe)\n");
    }
}

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
    /* Preserve slice bytes before nested sync callbacks reuse imgbufs. */
    dw3_slice_stash_push(buf, size > 0 ? size * 4 : 0);
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
        if (g_dout_top_done <= 5u || g_dout_top_done == 10u ||
            (g_dout_top_done % 30u) == 0u) {
            fprintf(stderr,
                    "dw3-present: DecDCTout frame done #%u slices=%u "
                    "rgb24_loads=%u rgb24=%d\n",
                    g_dout_top_done, g_slice_cb_count, g_rgb24_loads,
                    g_disp_rgb24);
        }
        /* Dump after RGB24 present origin is latched (see PutDispEnv wrap).
         * Default frame 30: MOVIEOPN fc=1 is solid DC+EOB fade-in; fc≈30 has
         * dense AC and proves non-flat content once quant tables are loaded. */
        if (g_disp_rgb24 && g_rgb24_loads >= 20u && !g_fb_dumped) {
            unsigned want = 30u;
            const char *env_frame = getenv("DW3_FB_DUMP_FRAME");

            if (env_frame != NULL && env_frame[0] != '\0') {
                int n = atoi(env_frame);
                if (n > 0) {
                    want = (unsigned)n;
                }
            }
            if (g_dout_top_done >= want) {
                dw3_present_dump_fb("after RGB24 frame");
            }
        }
    }
}

static void dw3_maybe_dump_disp(void) {
    const char *at;
    unsigned want;
    char *end;

    at = getenv("DW3_FB_DUMP_DISP");
    if (at == NULL || at[0] == '\0' || g_fb_dumped) {
        return;
    }
    want = (unsigned)strtoul(at, &end, 0);
    if (end == at || g_put_disp_count < want) {
        return;
    }
    fprintf(stderr,
            "dw3-present: disp dump request frame=%u rgb24_loads=%u "
            "rgb24=%d (0 loads => title, never entered MDEC RGB24)\n",
            g_put_disp_count, g_rgb24_loads, g_disp_rgb24);
    dw3_present_dump_fb("disp frame");
}

DISPENV *__wrap_PutDispEnv(DISPENV *env) {
    DISPENV *ret;
    DISPENV tmp;

    if (env == NULL) {
        return __real_PutDispEnv(env);
    }
    g_put_disp_count++;
    g_disp_rgb24 = env->isrgb24 ? 1 : 0;
    if (!env->isrgb24 || env->disp.x == 0) {
        ret = __real_PutDispEnv(env);
        dw3_maybe_dump_disp();
        return ret;
    }
    /* Match RGB24 LoadImage pixel placement (halfword x * 2/3). */
    tmp = *env;
    tmp.disp.x = (short)(env->disp.x * 2 / 3);
    if (!g_disp_x_scaled_logged) {
        g_disp_x_scaled_logged = 1;
        fprintf(stderr,
                "dw3-present: PutDispEnv rgb24 disp.x %d -> %d (pixel)\n",
                env->disp.x, tmp.disp.x);
    }
    ret = __real_PutDispEnv(&tmp);
    dw3_maybe_dump_disp();
    return ret;
}

/* P5.1: one buffer reused by every slice (the title movie converts one per
 * frame; a malloc/free pair each was 0.9 MB of churn per frame, found by the
 * allocation audit). LoadImage runs on the game thread only and the caller is
 * done with the buffer when __real_LoadImage returns (it copies). */
static u16 *g_rgb24_conv_buf;
static size_t g_rgb24_conv_cap;

static u16 *rgb888_packed_to_5551(const u8 *src, int pixel_w, int h) {
    size_t n = (size_t)pixel_w * (size_t)h;
    u16 *out;
    int y, x;

    if (n > g_rgb24_conv_cap) {
        u16 *grown = (u16 *)realloc(g_rgb24_conv_buf, n * sizeof(u16));

        if (grown == NULL) {
            return NULL;
        }
        g_rgb24_conv_buf = grown;
        g_rgb24_conv_cap = n;
    }
    out = g_rgb24_conv_buf;
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
    const uint8_t *src;
    const uint8_t *stashed;

    if (rect == NULL || p == NULL || rect->w <= 0 || rect->h <= 0) {
        return __real_LoadImage(rect, p);
    }
    /* Prefer stashed DecDCTout bytes: sync nested callbacks reuse imgbufs. */
    stashed = dw3_slice_stash_take(p);
    src = stashed != NULL ? stashed : (const uint8_t *)p;

    /* MDEC movie strips are always 24 halfwords wide. Do not reuse
     * PutDispEnv(isrgb24) here: a latched flag after STR would run later
     * LoadImage through the RGB888 converter. Title shred (R1) is the
     * TimBlock.pixels alignment in patch 0038, not this latch. */
    use_rgb24 = (rect->w == 24 && rect->h >= 192);
    if (!use_rgb24) {
        if (g_disp_rgb24) {
            g_disp_rgb24 = 0;
            if (!g_rgb24_latch_cleared_logged) {
                g_rgb24_latch_cleared_logged = 1;
                fprintf(stderr,
                        "dw3-present: non-MDEC LoadImage %dx%d — cleared "
                        "stale rgb24 latch\n",
                        rect->w, rect->h);
            }
        }
        return __real_LoadImage(rect, p);
    }

    /* PS1 24-bit: RECT is in halfwords; pixels = halfwords * 2 / 3. */
    pixel_w = rect->w * 2 / 3;
    if (pixel_w <= 0) {
        return __real_LoadImage(rect, p);
    }
    converted = rgb888_packed_to_5551(src, pixel_w, rect->h);
    if (converted == NULL) {
        return __real_LoadImage(rect, p);
    }
    pr = *rect;
    pr.x = (short)(rect->x * 2 / 3);
    pr.w = (short)pixel_w;
    rc = __real_LoadImage(&pr, (u_long *)converted);
    g_rgb24_loads++;
    if (g_rgb24_loads == 1u || g_rgb24_loads == 20u ||
        g_rgb24_loads == 581u) {
        size_t nbytes = (size_t)pixel_w * (size_t)rect->h * 3u;
        size_t i;
        unsigned minv = 255;
        unsigned maxv = 0;
        unsigned uniq = 0;
        unsigned char seen[256];
        unsigned long sum = 0;

        memset(seen, 0, sizeof(seen));
        for (i = 0; i < nbytes; i++) {
            unsigned v = src[i];
            if (v < minv)
                minv = v;
            if (v > maxv)
                maxv = v;
            sum += v;
            if (!seen[v]) {
                seen[v] = 1;
                uniq++;
            }
        }
        fprintf(stderr,
                "dw3-present: RGB24 LoadImage #%u %dx%d hw -> %dx%d px at "
                "(%d,%d) src min=%u max=%u avg=%.1f uniq8=%u stash=%d\n",
                g_rgb24_loads, rect->w, rect->h, pr.w, pr.h, pr.x, pr.y, minv,
                maxv, sum / (double)nbytes, uniq, stashed != NULL);
    }
    return rc;
}
