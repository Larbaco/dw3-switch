/* P5.4 PR 2 — next-boot crash notice.
 *
 * When dw3_nx_crash_install found (and rotated) a report from the previous
 * run, dw3_nx_crash_notice_show draws a short English text panel for a few
 * up to a minute, or until A / B / + is pressed, then returns to the normal boot.
 * With no previous report it returns at once and draws nothing. The text is
 * laid out here, without libnx or GL, so the host test
 * (tools/tests/crash_notice_main.c) can check it; nx_crash_notice.c draws it
 * with the 8x8 HUD font and dw3_nx_gpu_fill_rect. See docs/P5.4_CRASH.md.
 */
#ifndef DW3_NX_CRASH_NOTICE_H
#define DW3_NX_CRASH_NOTICE_H

#include "nx_crash.h"

#include <stddef.h>
#include <string.h>

/* Panel grid: columns of 8x8 glyphs and the most lines it can hold. */
#define DW3_NOTICE_COLS 60
#define DW3_NOTICE_MAX_LINES 16
/* Reason lines kept before the rest is cut with "...". */
#define DW3_NOTICE_REASON_LINES 3

/* Timing (ms): shown at most this long (the owner could not read the 8 s
 * version in time; the panel now waits for A and gives up after a minute so
 * a forgotten console still boots), and a button only counts after
 * DW3_NOTICE_MIN_MS so the press that launched the NRO cannot skip it. */
#define DW3_NOTICE_SHOW_MS 60000u
#define DW3_NOTICE_MIN_MS 1000u

/* Logical layout in font pixels (scaled by an integer at draw time). */
#define DW3_NOTICE_GLYPH 8
#define DW3_NOTICE_LINE_H 12 /* 8 px glyph + 4 px gap */
#define DW3_NOTICE_PAD 10
#define DW3_NOTICE_BAR_H 4 /* red bar along the top of the panel */

enum {
    DW3_NOTICE_BODY = 0,
    DW3_NOTICE_TITLE = 1,
    DW3_NOTICE_LABEL = 2, /* dimmer: field values, file hints */
    DW3_NOTICE_HINT = 3,  /* the countdown / button line */
};

typedef struct Dw3NoticeLine {
    char text[DW3_NOTICE_COLS + 1];
    int style;
} Dw3NoticeLine;

typedef struct Dw3NoticeLayout {
    Dw3NoticeLine line[DW3_NOTICE_MAX_LINES];
    int count;
} Dw3NoticeLayout;

static inline void dw3_notice_push(Dw3NoticeLayout *lay, const char *text,
                                   size_t len, int style) {
    Dw3NoticeLine *ln;

    if (lay->count >= DW3_NOTICE_MAX_LINES) {
        return;
    }
    ln = &lay->line[lay->count++];
    if (len > DW3_NOTICE_COLS) {
        len = DW3_NOTICE_COLS;
    }
    memcpy(ln->text, text, len);
    ln->text[len] = '\0';
    ln->style = style;
}

/* "<label><value>" word-wrapped to DW3_NOTICE_COLS, continuation lines
 * indented under the value; at most max_lines, the last one ending in "..."
 * when the value did not fit. Non-printable bytes become '?'. */
static inline void dw3_notice_field(Dw3NoticeLayout *lay, const char *label,
                                    const char *value, int max_lines,
                                    int style) {
    size_t indent = strlen(label);
    size_t width;
    size_t vlen;
    size_t pos = 0;
    int lines = 0;

    if (indent > DW3_NOTICE_COLS / 2) {
        indent = DW3_NOTICE_COLS / 2;
    }
    width = DW3_NOTICE_COLS - indent;
    if (!value || !value[0]) {
        value = "(none)";
    }
    vlen = strlen(value);
    while (value[pos] == ' ') {
        pos++;
    }
    do {
        char buf[DW3_NOTICE_COLS + 1];
        size_t take = vlen - pos;
        size_t i;
        int last = lines + 1 >= max_lines;

        if (take > width) {
            /* Break at the last space inside the width, if any. */
            size_t cut = width;
            while (cut > 0 && value[pos + cut] != ' ') {
                cut--;
            }
            take = cut > width / 3 ? cut : width;
        }
        memset(buf, ' ', indent);
        if (lines == 0) {
            memcpy(buf, label, indent);
        }
        for (i = 0; i < take; i++) {
            char c = value[pos + i];
            buf[indent + i] = (c >= 32 && c <= 126) ? c : '?';
        }
        pos += take;
        while (pos < vlen && value[pos] == ' ') {
            pos++;
        }
        if (last && pos < vlen) {
            /* Out of lines: end this one in "..." within the width. */
            if (take > width - 3) {
                take = width - 3;
            }
            memcpy(buf + indent + take, "...", 3);
            take += 3;
            pos = vlen;
        }
        dw3_notice_push(lay, buf, indent + take, style);
        lines++;
    } while (pos < vlen && lines < max_lines);
}

/* The whole panel text for one countdown value (seconds_left >= 0). */
static inline void dw3_notice_layout(Dw3NoticeLayout *lay,
                                     const Dw3NxCrashPrev *p,
                                     unsigned seconds_left) {
    char hint[DW3_NOTICE_COLS + 1];
    const char *kept =
        p->kept[0] ? p->kept : "sdmc:/switch/dw3/crash_report_prev.txt";
    size_t n;

    lay->count = 0;
    dw3_notice_push(lay, "The last run crashed.", 21, DW3_NOTICE_TITLE);
    dw3_notice_push(lay, "", 0, DW3_NOTICE_BODY);
    dw3_notice_field(lay, "Version: ", p->version, 1, DW3_NOTICE_BODY);
    dw3_notice_field(lay, "Reason:  ", p->reason, DW3_NOTICE_REASON_LINES,
                     DW3_NOTICE_BODY);
    dw3_notice_field(lay, "Mode:    ", p->mode_file, 1, DW3_NOTICE_LABEL);
    dw3_notice_field(lay, "Stage:   ", p->stage_file, 1, DW3_NOTICE_LABEL);
    dw3_notice_push(lay, "", 0, DW3_NOTICE_BODY);
    dw3_notice_push(lay, "Report saved to", 15, DW3_NOTICE_BODY);
    dw3_notice_push(lay, kept, strlen(kept), DW3_NOTICE_LABEL);
    dw3_notice_push(lay, "Please attach it and dw3.log to a bug report.",
                    45, DW3_NOTICE_BODY);
    dw3_notice_push(lay, "", 0, DW3_NOTICE_BODY);
    /* No stdio: "Press A to continue (60)". */
    memcpy(hint, "Press A to continue (", 21);
    n = 21;
    if (seconds_left >= 10) {
        hint[n++] = (char)('0' + (seconds_left / 10) % 10);
    }
    hint[n++] = (char)('0' + seconds_left % 10);
    hint[n++] = ')';
    dw3_notice_push(lay, hint, n, DW3_NOTICE_HINT);
}

/* Integer glyph scale so the panel fits in ~92% of the width and ~85% of
 * the height of a fb_w x fb_h surface: 2 at 1280x720, 3 at 1920x1080. */
static inline int dw3_notice_scale(unsigned fb_w, unsigned fb_h, int lines) {
    unsigned panel_w = DW3_NOTICE_COLS * DW3_NOTICE_GLYPH + 2 * DW3_NOTICE_PAD;
    unsigned panel_h = (unsigned)lines * DW3_NOTICE_LINE_H +
                       2 * DW3_NOTICE_PAD + DW3_NOTICE_BAR_H;
    unsigned sx = (fb_w * 92u / 100u) / panel_w;
    unsigned sy = (fb_h * 85u / 100u) / (panel_h ? panel_h : 1u);
    unsigned s = sx < sy ? sx : sy;

    if (s < 1) {
        s = 1;
    }
    if (s > 6) {
        s = 6;
    }
    return (int)s;
}

/* Seconds shown in the countdown: ceil((show_ms - elapsed_ms) / 1000). */
static inline unsigned dw3_notice_seconds_left(unsigned elapsed_ms) {
    if (elapsed_ms >= DW3_NOTICE_SHOW_MS) {
        return 0;
    }
    return (DW3_NOTICE_SHOW_MS - elapsed_ms + 999u) / 1000u;
}

/* Switch only (nx_crash_notice.c). Call once, after dw3_nx_gpu_init. */
void dw3_nx_crash_notice_show(void);

#endif /* DW3_NX_CRASH_NOTICE_H */
