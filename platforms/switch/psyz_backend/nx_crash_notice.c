/* P5.4 PR 2 — next-boot crash notice (see nx_crash_notice.h).
 *
 * Runs once at boot, right after the EGL surface is up and before the
 * runtime check / game, only when dw3_nx_crash_previous() says the install
 * latch rotated a report from the previous run. Otherwise it returns before
 * touching the GPU or the pad, so a boot without a crash is unchanged.
 *
 * Drawing: the cheapest path the host already has in every build, the same
 * scissor clears (dw3_nx_gpu_clear / dw3_nx_gpu_fill_rect) the STR-restore
 * bar uses, with the 8x8 HUD font: one clear per horizontal run of lit glyph
 * pixels. No shader, texture or allocation. The panel is centred on the
 * whole surface (1280x720 handheld, 1920x1080 docked) with an integer scale;
 * nothing of the game is on screen yet, so the widescreen option does not
 * apply. The frame is redrawn only when the countdown changes; between
 * redraws the last presented frame stays on screen and the loop sleeps.
 *
 * Bounded: it returns after DW3_NOTICE_SHOW_MS (60 s) whatever the pad does,
 * a loop cap ends it even if the clock does not advance, and an applet exit
 * request (HOME, close) ends it at once.
 */
#include "nx.h"
#include "nx_crash.h"
#include "nx_crash_notice.h"

#include "../source/debug_overlay_font.h"
#include "../source/debug_overlay_glyph.h"

#include <switch.h>

#define NOTICE_POLL_NS 16000000ULL /* ~one frame between pad polls */

static uint64_t notice_now_ms(void) {
    return armTicksToNs(armGetSystemTick()) / 1000000ULL;
}

typedef struct NoticeRgb {
    float r, g, b;
} NoticeRgb;

static const NoticeRgb k_bg = {0.02f, 0.03f, 0.06f};
static const NoticeRgb k_panel = {0.07f, 0.09f, 0.15f};
static const NoticeRgb k_bar = {0.82f, 0.18f, 0.18f};
static const NoticeRgb k_style[] = {
    {0.92f, 0.94f, 1.00f}, /* DW3_NOTICE_BODY */
    {1.00f, 0.72f, 0.30f}, /* DW3_NOTICE_TITLE */
    {0.68f, 0.74f, 0.86f}, /* DW3_NOTICE_LABEL */
    {0.55f, 0.85f, 1.00f}, /* DW3_NOTICE_HINT */
};

/* Top-left coordinates in, GL's bottom-left origin out. */
static void notice_rect(unsigned fb_h, int x, int y, int w, int h,
                        const NoticeRgb *c) {
    dw3_nx_gpu_fill_rect(x, (int)fb_h - (y + h), w, h, c->r, c->g, c->b,
                         1.0f);
}

static void notice_glyph(unsigned fb_h, int x, int y, int scale, char ch,
                         const NoticeRgb *c) {
    const uint8_t *rows = dw3_font8x8_glyph(ch);
    int gy;

    for (gy = 0; gy < 8; gy++) {
        int gx = 0;

        while (gx < 8) {
            int run;

            if (!dw3_dbg_glyph_pixel(rows[gy], gx)) {
                gx++;
                continue;
            }
            run = 1;
            while (gx + run < 8 && dw3_dbg_glyph_pixel(rows[gy], gx + run)) {
                run++;
            }
            notice_rect(fb_h, x + gx * scale, y + gy * scale, run * scale,
                        scale, c);
            gx += run;
        }
    }
}

static void notice_draw(const Dw3NoticeLayout *lay, unsigned fb_w,
                        unsigned fb_h) {
    int scale = dw3_notice_scale(fb_w, fb_h, lay->count);
    int panel_w =
        (DW3_NOTICE_COLS * DW3_NOTICE_GLYPH + 2 * DW3_NOTICE_PAD) * scale;
    int panel_h = (lay->count * DW3_NOTICE_LINE_H + 2 * DW3_NOTICE_PAD +
                   DW3_NOTICE_BAR_H) *
                  scale;
    int px = ((int)fb_w - panel_w) / 2;
    int py = ((int)fb_h - panel_h) / 2;
    int i;

    dw3_nx_gpu_clear(k_bg.r, k_bg.g, k_bg.b, 1.0f);
    notice_rect(fb_h, px, py, panel_w, panel_h, &k_panel);
    notice_rect(fb_h, px, py, panel_w, DW3_NOTICE_BAR_H * scale, &k_bar);
    for (i = 0; i < lay->count; i++) {
        const Dw3NoticeLine *ln = &lay->line[i];
        const NoticeRgb *c = &k_style[ln->style & 3];
        int x = px + DW3_NOTICE_PAD * scale;
        int y = py + (DW3_NOTICE_BAR_H + DW3_NOTICE_PAD +
                      i * DW3_NOTICE_LINE_H) *
                         scale;
        const char *s;

        for (s = ln->text; *s; s++, x += DW3_NOTICE_GLYPH * scale) {
            if (*s != ' ') {
                notice_glyph(fb_h, x, y, scale, *s, c);
            }
        }
    }
}

void dw3_nx_crash_notice_show(void) {
    static Dw3NoticeLayout lay;
    Dw3NxCrashPrev prev;
    PadState pad;
    uint64_t t0;
    uint64_t elapsed = 0;
    unsigned shown_secs = ~0u;
    unsigned shown_w = 0;
    unsigned shown_h = 0;
    unsigned frames = 0;
    unsigned polls = 0;
    const unsigned max_polls =
        (unsigned)(DW3_NOTICE_SHOW_MS * 1000000ULL / NOTICE_POLL_NS) * 2u;
    const char *how = "timed out";

    if (!dw3_nx_crash_previous(&prev)) {
        return; /* no crash last run: normal boot, nothing drawn */
    }
    psyz_log(1, __FILE__, __LINE__, __func__,
             "crash notice: showing (version=%s reason=%s), up to %u ms",
             prev.version, prev.reason, DW3_NOTICE_SHOW_MS);

    /* Our own pad state (hid shared memory; input init already configured
     * it). The first update only primes it: a button held since launch is
     * not a press. */
    padInitializeDefault(&pad);
    padUpdate(&pad);
    t0 = notice_now_ms();
    for (;;) {
        Dw3NxGpuInfo info;
        unsigned secs;

        elapsed = notice_now_ms() - t0;
        if (elapsed >= DW3_NOTICE_SHOW_MS || ++polls > max_polls) {
            break;
        }
        /* The notice can last a minute now: HOME / close (ExitRequested)
         * must not wait for it (appletLockExit gives ~15 s). The pump of the
         * game drains the same messages again after the notice. */
        if (!appletMainLoop()) {
            how = "ended: exit requested";
            break;
        }
        padUpdate(&pad);
        if (elapsed >= DW3_NOTICE_MIN_MS &&
            (padGetButtonsDown(&pad) &
             (HidNpadButton_A | HidNpadButton_B | HidNpadButton_Plus)) != 0) {
            how = "dismissed by a button";
            break;
        }
        secs = dw3_notice_seconds_left((unsigned)elapsed);
        dw3_nx_gpu_get_info(&info);
        if (secs != shown_secs || info.width != shown_w ||
            info.height != shown_h) {
            dw3_notice_layout(&lay, &prev, secs);
            notice_draw(&lay, info.width, info.height);
            if (!dw3_nx_gpu_swap()) {
                how = "ended: swap failed";
                break;
            }
            frames++;
            shown_secs = secs;
            shown_w = info.width;
            shown_h = info.height;
        }
        svcSleepThread(NOTICE_POLL_NS);
    }
    /* Leave a plain frame behind, not the notice, for whatever paints next. */
    dw3_nx_gpu_clear(0.0f, 0.0f, 0.0f, 1.0f);
    dw3_nx_gpu_swap();
    psyz_log(1, __FILE__, __LINE__, __func__,
             "crash notice: %s after %llu ms (%u frames drawn at %ux%u, "
             "scale %d); continuing boot",
             how, (unsigned long long)elapsed, frames, shown_w, shown_h,
             dw3_notice_scale(shown_w, shown_h, lay.count));
}
