/* Host check for platforms/switch/psyz_backend/nx_crash_notice.h (P5.4 PR 2
 * next-boot crash notice text layout and scale). gcc, no libnx. */
#include "nx_crash_notice.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

static void expect(const char *name, int cond) {
    printf("%s=%d\n", name, cond ? 1 : 0);
    if (!cond) {
        g_fail = 1;
    }
}

static void fill(Dw3NxCrashPrev *p, const char *version, const char *reason,
                 const char *mode, const char *stage) {
    memset(p, 0, sizeof(*p));
    snprintf(p->version, sizeof(p->version), "%s", version);
    snprintf(p->reason, sizeof(p->reason), "%s", reason);
    snprintf(p->mode_file, sizeof(p->mode_file), "%s", mode);
    snprintf(p->stage_file, sizeof(p->stage_file), "%s", stage);
    snprintf(p->kept, sizeof(p->kept), "%s",
             "sdmc:/switch/dw3/crash_report_prev.txt");
}

static int all_fit(const Dw3NoticeLayout *lay) {
    int i;

    for (i = 0; i < lay->count; i++) {
        if (strlen(lay->line[i].text) > DW3_NOTICE_COLS) {
            return 0;
        }
    }
    return lay->count <= DW3_NOTICE_MAX_LINES;
}

static int find(const Dw3NoticeLayout *lay, const char *text) {
    int i;

    for (i = 0; i < lay->count; i++) {
        if (strcmp(lay->line[i].text, text) == 0) {
            return i;
        }
    }
    return -1;
}

/* Panel size in surface pixels at the scale the notice would pick. */
static int panel_fits(unsigned w, unsigned h, int lines) {
    int s = dw3_notice_scale(w, h, lines);
    unsigned pw = (unsigned)(DW3_NOTICE_COLS * DW3_NOTICE_GLYPH +
                             2 * DW3_NOTICE_PAD) * (unsigned)s;
    unsigned ph = (unsigned)(lines * DW3_NOTICE_LINE_H + 2 * DW3_NOTICE_PAD +
                             DW3_NOTICE_BAR_H) * (unsigned)s;
    return pw <= w && ph <= h;
}

int main(void) {
    static Dw3NoticeLayout lay;
    Dw3NxCrashPrev p;
    int i;
    int r;

    /* A user-exception report, as the latch fills it. */
    fill(&p, "0.6.5-preview.30", "user exception", "0x207 (STSTATUS)",
         "0x0 (none)");
    dw3_notice_layout(&lay, &p, 8);
    for (i = 0; i < lay.count; i++) {
        printf("line%02d=%d|%s\n", i, lay.line[i].style, lay.line[i].text);
    }
    expect("title", lay.count > 0 && strcmp(lay.line[0].text,
                                             "The last run crashed.") == 0 &&
                        lay.line[0].style == DW3_NOTICE_TITLE);
    expect("version", find(&lay, "Version: 0.6.5-preview.30") > 0);
    expect("reason", find(&lay, "Reason:  user exception") > 0);
    expect("mode", find(&lay, "Mode:    0x207 (STSTATUS)") > 0);
    expect("stage", find(&lay, "Stage:   0x0 (none)") > 0);
    expect("path", find(&lay, "sdmc:/switch/dw3/crash_report_prev.txt") > 0);
    expect("hint", lay.count > 0 &&
                       strcmp(lay.line[lay.count - 1].text,
                              "Press A to continue (8)") == 0 &&
                       lay.line[lay.count - 1].style == DW3_NOTICE_HINT);
    expect("fits", all_fit(&lay));
    dw3_notice_layout(&lay, &p, 12);
    expect("hint_two_digits",
           strcmp(lay.line[lay.count - 1].text, "Press A to continue (12)") == 0);

    /* A long assert reason wraps under the value, at spaces, 3 lines max. */
    fill(&p, "0.6.5-preview.30-3-gabcdef0",
         "assert nx_crash.c:812 crash_force_assert: s_force_ptr != 0 && "
         "\"DW3_FORCE_CRASH=assert\" && some_other_condition_here(1, 2, 3) "
         "&& yet_another_long_condition_that_will_not_fit_at_all(4, 5, 6) "
         "&& more",
         "0x1c1 (STDWTITL)", "0x0 (none)");
    dw3_notice_layout(&lay, &p, 8);
    r = find(&lay, "Reason:  assert nx_crash.c:812 crash_force_assert:");
    expect("wrap_first", r > 0);
    expect("wrap_indent", r > 0 && r + 2 < lay.count &&
                              strncmp(lay.line[r + 1].text, "         ", 9) == 0 &&
                              lay.line[r + 1].text[9] != ' ' &&
                              strncmp(lay.line[r + 2].text, "         ", 9) == 0);
    expect("wrap_cut", r > 0 && r + 2 < lay.count &&
                           strlen(lay.line[r + 2].text) >= 3 &&
                           strcmp(lay.line[r + 2].text +
                                      strlen(lay.line[r + 2].text) - 3,
                                  "...") == 0);
    expect("wrap_next_field",
           r > 0 && r + 3 < lay.count &&
               strncmp(lay.line[r + 3].text, "Mode:    ", 9) == 0);
    expect("wrap_fits", all_fit(&lay));

    /* One unbroken word: hard break at the width. */
    {
        char word[160];
        memset(word, 'w', sizeof(word) - 1);
        word[sizeof(word) - 1] = '\0';
        fill(&p, "v", word, "m", "s");
        dw3_notice_layout(&lay, &p, 1);
        r = find(&lay, "Version: v");
        expect("hard_break", r > 0 && strlen(lay.line[r + 1].text) ==
                                          DW3_NOTICE_COLS &&
                                 strlen(lay.line[r + 2].text) == DW3_NOTICE_COLS);
        expect("hard_break_fits", all_fit(&lay));
    }

    /* Empty fields and non-printable bytes. */
    fill(&p, "", "bad\tbyte\x01", "", "");
    dw3_notice_layout(&lay, &p, 0);
    expect("empty_version", find(&lay, "Version: (none)") > 0);
    expect("nonprint", find(&lay, "Reason:  bad?byte?") > 0);
    expect("hint_zero", strcmp(lay.line[lay.count - 1].text,
                               "Press A to continue (0)") == 0);

    /* Scale: integer, fits handheld and docked. */
    expect("scale_720", dw3_notice_scale(1280, 720, lay.count) == 2);
    expect("scale_1080", dw3_notice_scale(1920, 1080, lay.count) == 3);
    expect("scale_tiny", dw3_notice_scale(320, 240, lay.count) == 1);
    expect("fit_720", panel_fits(1280, 720, DW3_NOTICE_MAX_LINES));
    expect("fit_1080", panel_fits(1920, 1080, DW3_NOTICE_MAX_LINES));

    /* Countdown. */
    expect("secs_0", dw3_notice_seconds_left(0) == 8);
    expect("secs_999", dw3_notice_seconds_left(999) == 8);
    expect("secs_1000", dw3_notice_seconds_left(1000) == 7);
    expect("secs_7999", dw3_notice_seconds_left(7999) == 1);
    expect("secs_end", dw3_notice_seconds_left(8000) == 0 &&
                           dw3_notice_seconds_left(60000) == 0);
    return g_fail;
}
