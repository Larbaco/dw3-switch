/* Host check for platforms/switch/source/frame_dt.h. gcc, no libnx. */
#include "frame_dt.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

static void expect(const char *name, int cond) {
    printf("%s=%d\n", name, cond ? 1 : 0);
    if (!cond) {
        g_fail = 1;
    }
}

int main(void) {
    Dw3FrameDt st;
    uint64_t dt;
    unsigned i;

    memset(&st, 0, sizeof(st));
    dt = 99;
    expect("pump0", dw3_frame_dt_sample(&st, 0, 0, &dt) == 0);
    expect("dt_still_99", dt == 99);

    for (i = 1; i < DW3_FRAME_DT_PUMPS; i++) {
        if (dw3_frame_dt_sample(&st, i, (uint64_t)i * 1000ull, &dt) != 0) {
            expect("early_sample", 0);
            break;
        }
    }
    expect("no_line_before_60", i == DW3_FRAME_DT_PUMPS);
    expect("dt_untouched", dt == 99);

    /* First multiple arms the baseline, including a zero timestamp. */
    expect("arm", dw3_frame_dt_sample(&st, 60, 0, &dt) == 0);
    expect("dt_after_arm", dt == 99);

    expect("sample_120",
           dw3_frame_dt_sample(&st, 120, 1002000ull, &dt) == 1);
    expect("dt_120", dt == 1002000ull);

    expect("skip_121", dw3_frame_dt_sample(&st, 121, 1002001ull, &dt) == 0);
    expect("dt_unchanged", dt == 1002000ull);

    expect("sample_180",
           dw3_frame_dt_sample(&st, 180, 1002000ull + 900000ull, &dt) == 1);
    expect("dt_180", dt == 900000ull);

    return g_fail;
}
