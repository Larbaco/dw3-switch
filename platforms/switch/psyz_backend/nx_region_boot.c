/* P6.24: the Switch side of the combined image's boot flow; see
 * nx_region_boot.h.
 *
 * Drawing is the crash notice's: scissor clears (dw3_nx_gpu_fill_rect) with the
 * 8x8 HUD font, one clear per horizontal run of lit glyph pixels, centred with
 * an integer scale. The text is the launcher's (port/region/dw3_launcher.c), so
 * the in-image picker reads exactly like the stop-gap launcher's. The frame is
 * redrawn only when the input or the countdown changes. */
#if defined(DW3_BOTH) && DW3_BOTH /* the combined image only */

#include "nx.h"
#include "nx_region_boot.h"

#include "dw3_launcher.h"

#include "../source/debug_overlay_font.h"
#include "../source/debug_overlay_glyph.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <psyz/log.h>
#include <switch.h>

#define REGION_POLL_NS 16000000ULL /* about one frame */
#define MINUS_SAMPLE_FRAMES 12     /* about 200 ms: the pad reports a moment late */
#define LINE_H 12                  /* 8 px glyph + 4 px gap, as the crash notice */
#define PAD_PX 10
#define MAX_COLS 80
#define MAX_LINES 32

typedef struct Rgb {
    float r, g, b;
} Rgb;

static const Rgb k_bg = {0.02f, 0.03f, 0.06f};
static const Rgb k_panel = {0.07f, 0.09f, 0.15f};
static const Rgb k_bar = {0.20f, 0.55f, 0.95f};
static const Rgb k_body = {0.92f, 0.94f, 1.00f};
static const Rgb k_title = {1.00f, 0.72f, 0.30f};
static const Rgb k_hint = {0.55f, 0.85f, 1.00f};
static const Rgb k_pick = {0.55f, 1.00f, 0.60f};

static PadState s_pad;
static int s_pad_ready;

static void pad_ready(void) {
    if (!s_pad_ready) {
        padInitializeDefault(&s_pad);
        padUpdate(&s_pad); /* the first update only primes it */
        s_pad_ready = 1;
    }
}

static uint64_t now_ms(void) { return armTicksToNs(armGetSystemTick()) / 1000000ULL; }

static void rect(unsigned fb_h, int x, int y, int w, int h, const Rgb *c) {
    dw3_nx_gpu_fill_rect(x, (int)fb_h - (y + h), w, h, c->r, c->g, c->b, 1.0f);
}

static void glyph(unsigned fb_h, int x, int y, int scale, char ch, const Rgb *c) {
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
            rect(fb_h, x + gx * scale, y + gy * scale, run * scale, scale, c);
            gx += run;
        }
    }
}

/* Splits `text` into lines (the launcher's text starts and ends with blank
 * lines; they are kept: they are the margins). */
static int split_lines(const char *text, char lines[MAX_LINES][MAX_COLS + 1], int *max_cols) {
    int n = 0;
    const char *p = text;

    *max_cols = 0;
    while (*p != '\0' && n < MAX_LINES) {
        int len = 0;

        while (p[len] != '\0' && p[len] != '\n') {
            len++;
        }
        if (len > MAX_COLS) {
            len = MAX_COLS;
        }
        memcpy(lines[n], p, (size_t)len);
        lines[n][len] = '\0';
        if (len > *max_cols) {
            *max_cols = len;
        }
        n++;
        p += len;
        while (*p != '\0' && *p != '\n') {
            p++; /* an over-long line is cut */
        }
        if (*p == '\n') {
            p++;
        }
    }
    return n;
}

static const Rgb *line_color(const char *line, int index) {
    if (strstr(line, "Digimon World 3") != NULL && index < 4) {
        return &k_title;
    }
    if (strstr(line, "Starting ") != NULL || strstr(line, "Press A to start") != NULL ||
        strstr(line, ">") != NULL) {
        return &k_pick;
    }
    if (strstr(line, "Left / Right") != NULL || strstr(line, "Y  always") != NULL ||
        strstr(line, "Hold -") != NULL || strstr(line, "+  quit") != NULL) {
        return &k_hint;
    }
    return &k_body;
}

static void draw_text(const char *text) {
    static char lines[MAX_LINES][MAX_COLS + 1];
    Dw3NxGpuInfo info;
    int cols, n, i, scale, panel_w, panel_h, px, py;

    n = split_lines(text, lines, &cols);
    dw3_nx_gpu_get_info(&info);
    {
        unsigned w = (unsigned)(cols + 2) * 8u + 2u * PAD_PX;
        unsigned h = (unsigned)n * LINE_H + 2u * PAD_PX + 4u;
        unsigned sx = info.width / w;
        unsigned sy = info.height / h;

        scale = (int)(sx < sy ? sx : sy);
        if (scale < 1) {
            scale = 1;
        }
        if (scale > 4) {
            scale = 4;
        }
        panel_w = (int)w * scale;
        panel_h = (int)h * scale;
    }
    px = ((int)info.width - panel_w) / 2;
    py = ((int)info.height - panel_h) / 2;
    dw3_nx_gpu_clear(k_bg.r, k_bg.g, k_bg.b, 1.0f);
    rect(info.height, px, py, panel_w, panel_h, &k_panel);
    rect(info.height, px, py, panel_w, 4 * scale, &k_bar);
    for (i = 0; i < n; i++) {
        const Rgb *c = line_color(lines[i], i);
        int x = px + PAD_PX * scale;
        int y = py + (4 + PAD_PX + i * LINE_H) * scale;
        const char *s;

        for (s = lines[i]; *s; s++, x += 8 * scale) {
            if (*s != ' ') {
                glyph(info.height, x, y, scale, *s, c);
            }
        }
    }
    dw3_nx_gpu_swap();
}

static unsigned pad_edges(void) {
    uint64_t down = padGetButtonsDown(&s_pad);
    unsigned e = 0;

    if (down & (HidNpadButton_Left | HidNpadButton_StickLLeft)) {
        e |= DW3_PK_LEFT;
    }
    if (down & (HidNpadButton_Right | HidNpadButton_StickLRight)) {
        e |= DW3_PK_RIGHT;
    }
    if (down & HidNpadButton_A) {
        e |= DW3_PK_A;
    }
    if (down & HidNpadButton_Y) {
        e |= DW3_PK_Y;
    }
    if (down & HidNpadButton_Plus) {
        e |= DW3_PK_PLUS;
    }
    return e;
}

static void io_log(void *ud, const char *line) {
    (void)ud;
    INFOF("%s", line);
}

static int io_sample_minus(void *ud) {
    int i, held = 0;

    (void)ud;
    pad_ready();
    for (i = 0; i < MINUS_SAMPLE_FRAMES; i++) {
        padUpdate(&s_pad);
        if (padGetButtons(&s_pad) & HidNpadButton_Minus) {
            held = 1;
        }
        svcSleepThread(REGION_POLL_NS);
    }
    return held;
}

/* Leaves a plain black frame behind, and waits until the buttons are up so the
 * press that confirmed the picker cannot reach the title screen. */
static void finish_screen(void) {
    uint64_t t0 = now_ms();

    while (appletMainLoop() && now_ms() - t0 < 600) {
        padUpdate(&s_pad);
        if (padGetButtons(&s_pad) == 0) {
            break;
        }
        svcSleepThread(REGION_POLL_NS);
    }
    dw3_nx_gpu_clear(0.0f, 0.0f, 0.0f, 1.0f);
    dw3_nx_gpu_swap();
}

static unsigned io_run_picker(void *ud, Dw3Picker *p) {
    static char text[2048];
    uint64_t t0, last;
    int shown = -2;
    unsigned drawn = 0;

    (void)ud;
    if (!dw3_nx_gpu_init()) {
        INFOF("dw3-region: no screen for the picker (EGL failed); booting the default entry");
        p->chosen = p->sel;
        p->timed_out = 1;
        p->state = DW3_PICKER_CHOSEN;
        return 0;
    }
    pad_ready();
    t0 = last = now_ms();
    while (p->state == DW3_PICKER_RUNNING) {
        uint64_t now;
        unsigned e;
        int s;

        if (!appletMainLoop()) { /* HOME / close while the picker is up */
            p->state = DW3_PICKER_QUIT;
            break;
        }
        now = now_ms();
        padUpdate(&s_pad);
        e = pad_edges();
        dw3_picker_step(p, e, (unsigned)(now - last));
        last = now;
        s = dw3_picker_seconds_left(p);
        if (e != 0 || s != shown || drawn == 0) {
            shown = s;
            dw3_launcher_text(DW3_SCREEN_PICKER, p, DW3_LAUNCHER_BASE, DW3_REGION_NONE, NULL, 0,
                              text, sizeof(text));
            draw_text(text);
            drawn++;
        }
        svcSleepThread(REGION_POLL_NS);
    }
    if (p->state == DW3_PICKER_CHOSEN) {
        finish_screen();
    }
    return (unsigned)(now_ms() - t0);
}

static void io_show_no_data(void *ud, const char *base) {
    static char text[2048];

    (void)ud;
    if (!dw3_nx_gpu_init()) {
        return;
    }
    pad_ready();
    dw3_launcher_text(DW3_SCREEN_NO_DATA, NULL, base, DW3_REGION_NONE, NULL, 0, text, sizeof(text));
    draw_text(text);
    while (appletMainLoop()) {
        padUpdate(&s_pad);
        if (padGetButtonsDown(&s_pad) & (HidNpadButton_Plus | HidNpadButton_B)) {
            break;
        }
        svcSleepThread(REGION_POLL_NS);
    }
    dw3_nx_gpu_clear(0.0f, 0.0f, 0.0f, 1.0f);
    dw3_nx_gpu_swap();
}

static int has_ext(const char *name, const char *ext) {
    size_t n = strlen(name), e = strlen(ext);

    return n > e && strcasecmp(name + n - e, ext) == 0;
}

/* A disc image waiting in `<base>/<r>/` (the folder the runtime extraction
 * already reads) makes the region count as present, so a first run from a disc
 * still works. boot_tim.bin is never a disc image. */
static int io_disc_waiting(void *ud, const char *base, Dw3Region r) {
    char dir[512];
    DIR *d;
    struct dirent *ent;
    int found = 0;

    (void)ud;
    snprintf(dir, sizeof(dir), "%s/%s", base, dw3_region_name(r));
    d = opendir(dir);
    if (d == NULL) {
        return 0;
    }
    while ((ent = readdir(d)) != NULL) {
        if (strcasecmp(ent->d_name, "boot_tim.bin") == 0) {
            continue;
        }
        if (has_ext(ent->d_name, ".cue") || has_ext(ent->d_name, ".bin") ||
            has_ext(ent->d_name, ".iso")) {
            found = 1;
            break;
        }
    }
    closedir(d);
    return found;
}

void dw3_nx_region_boot(const char *base, Dw3Region forced, const char *forced_by,
                        Dw3RegionBootOut *out) {
    Dw3RegionBootIO io;
    Dw3RegionBootIn in;

    memset(&io, 0, sizeof(io));
    io.log = io_log;
    io.sample_minus = io_sample_minus;
    io.run_picker = io_run_picker;
    io.show_no_data = io_show_no_data;
    io.disc_waiting = io_disc_waiting;
    in.base = base;
    in.forced = forced;
    in.forced_by = forced_by;
    dw3_region_boot(&in, &io, out);
}

#endif /* DW3_BOTH */
