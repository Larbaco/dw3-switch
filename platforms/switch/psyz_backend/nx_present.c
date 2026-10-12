/* PSY-Z present/MDEC hooks that the Switch link options wrap (P4.7).
 *
 * Opening STRs are 24-bit. XA comes out of the CD bridge on its own, so the
 * movie can be audible while the picture stays on the last title frame: PSY-Z's
 * DMACallback is still NOT_IMPLEMENTED, so DecDCTout never raises the MDEC-out
 * IRQ the title player waits on (slice -> LoadImage), and a raw RGB888 upload
 * is not RGB555 in the rectangle we present. This is the same path as
 * platforms/pc/src/psyz_dma_present.c, without the PC framebuffer dump.
 *
 * This TU deliberately includes only the PSY-Z headers: those redirect libc
 * names (`open` -> `psyz_open`) under __psyz, and mixing that with the libnx and
 * EGL headers is asking for trouble.
 */
#include <psyz.h>
#include <libcd.h> /* CdControl / CdInit / CdSetDebug */
#include <libgpu.h> /* RECT / DISPENV; psyz.h alone does not pull this in */
#include <libpress.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "stgetnext_gate.h"

int __real_LoadImage(RECT *rect, u_long *p);
DISPENV *__real_PutDispEnv(DISPENV *env);
void *__real_DMACallback(int dma, void (*func)());
void __real_DecDCTReset(int mode);
void __real_DecDCTout(u_long *buf, int size);

/* --- TEMPORARY: boot-stop trace -------------------------------------------
 *
 * The boot reaches its fourth VSync() and stops. That VSync is the one after
 * LoadImage(), and the next thing the game does is a short straight line —
 * SetDispMask(1), CdInit(), CdSetDebug(), SetGraphDebug(), then a
 * `while (CdControl(CdlSetmode, ...) == 0)` — before the VSync(3) that would
 * print pump #5. Reading that line says every call in it returns immediately:
 * D_800B5718[CdlSetmode] is 0, so cd_cw skips Setloc, and CD_command's
 * CdlSetmode arm only fails on a NULL param. So one of them does not, and
 * guessing which from the source has already produced one wrong answer.
 *
 * These forwarders just say which is entered and what it returned; the game
 * ignores every one of these return values, so they cannot change behaviour.
 * Remove with the other boot-hang instrumentation.
 */
int __real_CdInit(void);
int __real_CdControl(u_char com, u_char* param, u_char* result);
int __real_CdSetDebug(int level);
int __real_SetGraphDebug(int level);
void __real_SetDispMask(int mask);

/* Same constants as the PC present. One MOVIEOPN frame is 20 strips. */
enum { DW3_DMA_CHANS = 8, DW3_DMA_MDEC_OUT = 1 };
enum { DW3_SLICE_STASH_SLOTS = 32, DW3_SLICE_STASH_BYTES = 0x4E00 };

static void (*g_dma_cb[DW3_DMA_CHANS])(void);
static int g_disp_rgb24;
static unsigned g_slice_cb_count;
static unsigned g_rgb24_loads;
static unsigned g_dout_depth;
static unsigned g_dout_top_done;
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

/* PSY-Z private soft-MDEC entry. libpress PutEnv -> MDEC_in is LP64-unsafe. */
int Psyz_MdecCommand(uint32_t command, const void *data, size_t words);

/* Ring cursors. After the picture sticks, these say whether the STR ring
 * still has a finished frame (id 2), is full of locks (id 4), or went empty. */
extern StHEADER *StRingAddr;
extern s32 StRingIdx1, StRingIdx2, StRingIdx3, StFinalSector;
extern volatile s32 StRingSize;
extern s16 Stsector_offset;

u_long __real_StGetNext(u_long **addr, u_long **header);
u_long __real_StFreeRing(u_long *base);

/* StFreeRing sets StRingIdx3 = start + nSectors. That should be the next
 * frame's header or the wrap sentinel. When it lands inside a frame, log it
 * so the drift is visible in the guest log. */
u_long __wrap_StFreeRing(u_long *base) {
    u_long rc = __real_StFreeRing(base);

    if (rc == 0 && StRingAddr != NULL && StRingIdx3 >= 0 &&
        StRingIdx3 < StRingSize) {
        StHEADER *slot = &StRingAddr[StRingIdx3];

        if (slot->id == 3 && slot->secCount != 0) {
            INFOF("trace: StFreeRing landed idx3=%d id=%u sec=%u/%u fc=%u",
                  (int)StRingIdx3, (unsigned)slot->id,
                  (unsigned)slot->secCount, (unsigned)slot->nSectors,
                  (unsigned)slot->frameCount);
        }
    }
    return rc;
}

/* StGetNext only looks at StRingAddr[StRingIdx3]. The read cursor sometimes
 * sits on a body sector (id 3, secCount > 0) of a frame whose header is
 * behind it, or on a leftover tail after that header was reused. Either way
 * the writer blocks on the slot and the picture replays the last VLC.
 *
 * A clean full-ring scan is cached until a producer attempt or ring reset.
 * stgetnext_invalidate.c invalidates BEFORE CdReady (all StCdInterrupt ring
 * writes follow it), direct ready publication, and ring clear/set. Pumps with
 * no sector budget never call CdReady, so busy-wait polls skip repeated scans.
 * The cursor keys only reject other consumer states; they do not prove safety
 * against producer reset/early-wrap ABA. No timing or frame-delivery change. */
u_long __wrap_StGetNext(u_long **addr, u_long **header) {
    Dw3StGetNextGate *empty_gate = &dw3_stgetnext_empty_gate;
    int guard = 0;

    for (;;) {
        u_long rc = __real_StGetNext(addr, header);
        StHEADER *slot;
        StHEADER *head;
        s32 start;
        s32 n;
        s32 j;

        if (rc == 0 || StRingAddr == NULL || StRingSize <= 0 ||
            StRingIdx3 < 0 || StRingIdx3 >= StRingSize || guard++ > 4) {
            /* A frame was handed over, or the ring is unusable: a verdict from
             * the previous state must not survive into this one. */
            dw3_stgetnext_gate_forget(empty_gate);
            return rc;
        }
        /* Repeat empty poll at the same ring and cursors as a poll that already
         * scanned every slot clean and no mutation hook ran: skip the scan.
         * Returns rc, exactly as the scan
         * would when it finds nothing. */
        if (dw3_stgetnext_gate_hit(empty_gate, StRingAddr, StRingSize,
                                   StRingIdx1, StRingIdx3)) {
            return rc;
        }
        slot = &StRingAddr[StRingIdx3];
        if (slot->id == 3 && slot->secCount > 0 && slot->secCount <= 31) {
            /* Recovery: the read cursor sits inside a frame body. Never trust a
             * remembered verdict while this path is in play. */
            dw3_stgetnext_gate_forget(empty_gate);
            start = StRingIdx3 - (s32)slot->secCount;
            if (start >= 0 && start < StRingSize) {
                head = &StRingAddr[start];
                if (head->frameCount == slot->frameCount && head->id == 2) {
                    INFOF("trace: StGetNext resync idx3 %d -> %d fc=%u sec=%u",
                          (int)StRingIdx3, (int)start,
                          (unsigned)slot->frameCount,
                          (unsigned)slot->secCount);
                    StRingIdx3 = start;
                    continue;
                }
                if (head->frameCount == slot->frameCount && head->id == 3) {
                    return rc;
                }
            }
            INFOF("trace: StGetNext drop orphan idx3=%d fc=%u sec=%u",
                  (int)StRingIdx3, (unsigned)slot->frameCount,
                  (unsigned)slot->secCount);
            slot->id = 0;
        }
        for (n = 1; n < StRingSize; n++) {
            j = StRingIdx3 + n;
            if (j >= StRingSize) {
                j -= StRingSize;
            }
            if (StRingAddr[j].id == 2 || StRingAddr[j].id == 1) {
                INFOF("trace: StGetNext scan idx3 %d -> %d id=%u fc=%u",
                      (int)StRingIdx3, (int)j, (unsigned)StRingAddr[j].id,
                      (unsigned)StRingAddr[j].frameCount);
                StRingIdx3 = j;
                break;
            }
        }
        if (n >= StRingSize) {
            /* The read cursor was inspected by __real_StGetNext and every other
             * slot by this scan: the whole ring is clean. Remember it so repeat
             * polls at these cursors skip the scan. */
            dw3_stgetnext_gate_remember(empty_gate, StRingAddr, StRingSize,
                                        StRingIdx1, StRingIdx3);
            return rc;
        }
    }
}

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
}

static const uint8_t *dw3_slice_stash_take(const void *buf) {
    unsigned i;
    Dw3SliceStash *best = NULL;

    if (buf == NULL || g_slice_stash_seq == 0u) {
        return NULL;
    }
    /* Newest unconsumed entry for this pointer. The player nests DecDCTout
     * before LoadImage; a sync MDEC finishes the inner slices first, so the
     * LoadImage that runs on the way back out wants the matching stash.
     * The Switch build of onSliceDecoded currently emits LoadImage before the
     * nested DecDCTout, which also matches newest-first. */
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

static void dw3_mdec_upload_default_tables(void) {
    DECDCTENV env;
    uint8_t quant[128];
    int q_rc;
    int d_rc;

    DecDCTGetEnv(&env);
    memcpy(quant, env.iq_y, 64);
    memcpy(quant + 64, env.iq_c, 64);
    q_rc = Psyz_MdecCommand(0x40000001u, quant, 32);
    d_rc = Psyz_MdecCommand(0x60000000u, env.dct, 32);
    INFOF("trace: DecDCTReset quant/IDCT rc=%d,%d q0=%u q1=%u", q_rc, d_rc,
          quant[0], quant[1]);
}

static u16 *rgb888_packed_to_5551(const u8 *src, int pixel_w, int h) {
    size_t n = (size_t)pixel_w * (size_t)h;
    u16 *out;
    int y;
    int x;

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

            dst[x] = (u16)(r | (g << 5) | (b << 10) | 0x8000u);
            row += 3;
        }
    }
    return out;
}

/* Stride sample. A flat mid-grey frame (quant never uploaded) stays uniq8=1.
 * A replayed VLC buffer keeps the same sig while DecDCTout still climbs. */
static unsigned dw3_pix_sig(const uint8_t *src, int nbytes, unsigned *uniq,
                            unsigned *minv, unsigned *maxv) {
    unsigned sig = 2166136261u;
    unsigned char seen[256];
    int i;

    memset(seen, 0, sizeof(seen));
    *uniq = 0;
    *minv = 255;
    *maxv = 0;
    if (src == NULL || nbytes <= 0) {
        return 0;
    }
    for (i = 0; i < nbytes; i += 47) {
        unsigned v = src[i];

        sig = (sig ^ v) * 16777619u;
        if (v < *minv) {
            *minv = v;
        }
        if (v > *maxv) {
            *maxv = v;
        }
        if (!seen[v]) {
            seen[v] = 1;
            (*uniq)++;
        }
    }
    return sig;
}

int __wrap_LoadImage(RECT *rect, u_long *p) {
    static int logged;
    RECT pr;
    u16 *converted;
    int pixel_w;
    int rc;
    const uint8_t *src;
    const uint8_t *stashed;

    if (rect == NULL || p == NULL || rect->w <= 0 || rect->h <= 0) {
        return __real_LoadImage(rect, p);
    }
    stashed = dw3_slice_stash_take(p);
    src = stashed != NULL ? stashed : (const uint8_t *)p;
    /* MDEC strips are 24 halfwords wide. Anything else (the boot TIM) stays
     * RGB555. A latched isrgb24 must not reroute that TIM. */
    if (!(rect->w == 24 && rect->h >= 192)) {
        rc = __real_LoadImage(rect, p);
        if (!logged) {
            logged = 1;
            INFOF("trace: LoadImage(%d,%d %dx%d) -> %d", rect->x, rect->y,
                  rect->w, rect->h, rc);
        }
        return rc;
    }

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
    free(converted);
    g_rgb24_loads++;
    if (g_rgb24_loads == 1u || g_rgb24_loads == 20u) {
        unsigned uniq = 0;
        unsigned minv = 0;
        unsigned maxv = 0;
        unsigned sig = dw3_pix_sig(src, pixel_w * rect->h * 3, &uniq, &minv,
                                   &maxv);

        INFOF("trace: RGB24 LoadImage #%u %dx%d hw -> %dx%d px at (%d,%d) "
              "sig=%08x uniq=%u min=%u max=%u stash=%d",
              g_rgb24_loads, rect->w, rect->h, pr.w, pr.h, pr.x, pr.y, sig,
              uniq, minv, maxv, stashed != NULL);
    }
    return rc;
}

DISPENV *__wrap_PutDispEnv(DISPENV *env) {
    /* This one runs twice per frame, so it ratchets like CdControl below: the
     * boot's own calls, then powers of two. Unconditional, it is ~120 lines a
     * second once the game is drawing. */
    static unsigned calls;
    static int scaled_logged;
    DISPENV tmp;
    DISPENV *passed;
    DISPENV *r;

    passed = env;
    if (env != NULL && env->isrgb24 && env->disp.x != 0) {
        /* Movie buffers sit at halfword x 0 and 480. RGB24 LoadImage stores
         * the expanded frame at x * 2/3, so 480 must be presented at 320. */
        tmp = *env;
        tmp.disp.x = (short)(env->disp.x * 2 / 3);
        passed = &tmp;
        g_disp_rgb24 = 1;
        if (!scaled_logged) {
            scaled_logged = 1;
            INFOF("trace: PutDispEnv rgb24 disp.x %d -> %d", env->disp.x,
                  tmp.disp.x);
        }
    } else if (env != NULL) {
        g_disp_rgb24 = env->isrgb24 ? 1 : 0;
    }
    r = __real_PutDispEnv(passed);

    calls++;
    if (calls <= 8 || (calls & (calls - 1)) == 0) {
        INFOF("trace: PutDispEnv #%u -> disp(%d,%d %dx%d) rgb24=%d", calls,
              passed ? passed->disp.x : -1, passed ? passed->disp.y : -1,
              passed ? passed->disp.w : -1, passed ? passed->disp.h : -1,
              g_disp_rgb24);
    }
    return r;
}

void __wrap_SetDispMask(int mask) {
    INFOF("trace: SetDispMask(%d) ->", mask);
    __real_SetDispMask(mask);
    INFOF("trace: SetDispMask <-");
}

int __wrap_CdInit(void) {
    int r;

    INFOF("trace: CdInit ->");
    r = __real_CdInit();
    INFOF("trace: CdInit <- %d", r);
    return r;
}

int __wrap_CdSetDebug(int level) {
    int r = __real_CdSetDebug(level);

    INFOF("trace: CdSetDebug(%d) -> %d", level, r);
    return r;
}

int __wrap_SetGraphDebug(int level) {
    int r = __real_SetGraphDebug(level);

    INFOF("trace: SetGraphDebug(%d) -> %d", level, r);
    return r;
}

/* The busy-wait is the reason for the power-of-two ratchet: if this is the
 * call that never satisfies the loop, it is called millions of times, and
 * every log line is a syscall and a file write. */
int __wrap_CdControl(u_char com, u_char* param, u_char* result) {
    static unsigned calls;
    int r;

    calls++;
    if (calls <= 8 || (calls & (calls - 1)) == 0) {
        INFOF("trace: CdControl(com=%02X) call #%u ->", com, calls);
    }
    r = __real_CdControl(com, param, result);
    if (calls <= 8 || (calls & (calls - 1)) == 0) {
        INFOF("trace: CdControl(com=%02X) call #%u <- %d", com, calls, r);
    }
    return r;
}

void *__wrap_DMACallback(int dma, void (*func)()) {
    void *prev = NULL;

    /* Real DMACallback is NOT_IMPLEMENTED and returns NULL. Owning the table
     * is what lets DecDCTout raise the MDEC-out IRQ the movie player waits on. */
    (void)__real_DMACallback;
    if (dma >= 0 && dma < DW3_DMA_CHANS) {
        prev = (void *)g_dma_cb[dma];
        g_dma_cb[dma] = func;
    }
    return prev;
}

void __wrap_DecDCTReset(int mode) {
    __real_DecDCTReset(mode);
    dw3_mdec_upload_default_tables();
}

void __wrap_DecDCTout(u_long *buf, int size) {
    g_dout_depth++;
    __real_DecDCTout(buf, size);
    dw3_slice_stash_push(buf, size > 0 ? size * 4 : 0);
    if (g_dma_cb[DW3_DMA_MDEC_OUT] != NULL) {
        g_dma_cb[DW3_DMA_MDEC_OUT]();
        g_slice_cb_count++;
        if (g_slice_cb_count == 1u) {
            INFOF("trace: MDEC-out DMA callback first slice");
        }
    }
    g_dout_depth--;
    if (g_dout_depth == 0u) {
        unsigned uniq = 0;
        unsigned minv = 0;
        unsigned maxv = 0;
        unsigned sig = 0;
        int nbytes = size > 0 ? size * 4 : 0;

        g_dout_top_done++;
        if (buf != NULL && nbytes > 0) {
            sig = dw3_pix_sig((const uint8_t *)buf, nbytes, &uniq, &minv,
                              &maxv);
        }
        if (g_dout_top_done <= 5u || g_dout_top_done == 10u ||
            g_dout_top_done == 30u || (g_dout_top_done % 300u) == 0u) {
            INFOF("trace: DecDCTout frame done #%u slices=%u rgb24_loads=%u "
                  "sig=%08x",
                  g_dout_top_done, g_slice_cb_count, g_rgb24_loads, sig);
        }
    }
}
