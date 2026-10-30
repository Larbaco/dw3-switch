/* P6.35 phase 1 host proofs: the step resolver with synthetic states, and the
 * built-in table checks. `walkthrough_test list-ids` prints the table ids for
 * test_walkthrough.py. Plain C, no game data. */
#include "port_walkthrough.h"

#include <stdio.h>
#include <string.h>

static int g_fail;
#define CHECK(c, ...)                                   \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: ", __LINE__);         \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
            ++g_fail;                                   \
        }                                               \
    } while (0)

/* a fake FLAGS_00: a few set (code, value) pairs */
typedef struct Fake { unsigned codes[40]; int n; int calls; } Fake;

static int fake_check(void *ctx, unsigned code, unsigned value) {
    Fake *f = (Fake *)ctx;
    int i;

    ++f->calls;
    for (i = 0; i < f->n; ++i) {
        if (f->codes[i] == code) return value == 1;
    }
    return value == 0;
}

static DW3WtState mk(int progress, int region, Fake *f) {
    DW3WtState st;

    st.progress = progress;
    st.half = 0;
    st.area = 0x13;
    st.region = region;
    st.check = fake_check;
    st.ctx = f;
    return st;
}

static const char *id_of(const DW3WtResult *r) { return r->step ? r->step->id : "-"; }

static void expect_step(int progress, int region, int flag_0d, const char *id) {
    Fake f;
    DW3WtState st;
    DW3WtResult r;

    memset(&f, 0, sizeof(f));
    if (flag_0d) f.codes[f.n++] = WT_FLAG(0x40, 0x0D);
    st = mk(progress, region, &f);
    dw3_wt_resolve(&st, &r);
    CHECK(r.status == WT_OK && strcmp(id_of(&r), id) == 0,
          "progress %d flag %d region %d: got status %d step %s, want %s", progress, flag_0d,
          region, r.status, id_of(&r), id);
}

/* The current step at `progress` with the game's flags / items `c0..c2` set (0 = unused). */
static void expect_with(int progress, unsigned c0, unsigned c1, unsigned c2, const char *id) {
    int region;

    for (region = WT_REGION_US; region <= WT_REGION_EU; ++region) {
        Fake f;
        DW3WtState st;
        DW3WtResult r;

        memset(&f, 0, sizeof(f));
        if (c0) f.codes[f.n++] = c0;
        if (c1) f.codes[f.n++] = c1;
        if (c2) f.codes[f.n++] = c2;
        st = mk(progress, region, &f);
        dw3_wt_resolve(&st, &r);
        CHECK(r.status == WT_OK && strcmp(id_of(&r), id) == 0,
              "progress 0x%x flags %x %x %x region %d: got status %d step %s, want %s", progress,
              c0, c1, c2, region, r.status, id_of(&r), id);
    }
}

static void expect_status(int progress, int region, int want) {
    Fake f;
    DW3WtState st;
    DW3WtResult r;

    memset(&f, 0, sizeof(f));
    st = mk(progress, region, &f);
    dw3_wt_resolve(&st, &r);
    CHECK(r.status == want && r.step == NULL, "progress %d region %d: status %d (want %d)",
          progress, region, r.status, want);
}

/* synthetic table: boundaries, an EU only row, an open upper bound, a gap */
static const DW3WtStep k_syn[] = {
    {"t.a", "tc", "x", -1, WT_MAIN, 0, WT_REGION_ALL, 2, 4, 1, 0, {{0, 0}, {0, 0}}, 0, {{0, 0}, {0, 0}}},
    {"t.b", "tc", "x", -1, WT_MAIN, 0, WT_REGION_ALL, 5, 6, 2, 0, {{0, 0}, {0, 0}}, 1, {{0x4001, 1}, {0, 0}}},
    {"t.c", "tc", "x", -1, WT_MAIN, 0, WT_REGION_ALL, 5, 6, 3, 1, {{0x4001, 1}, {0, 0}}, 0, {{0, 0}, {0, 0}}},
    {"t.eu", "tc", "x", -1, WT_MAIN, 0, WT_REGION_EU, 0x2D, 0x2D, 4, 0, {{0, 0}, {0, 0}}, 0, {{0, 0}, {0, 0}}},
    {"t.end", "tc", "x", -1, WT_MAIN, 0, WT_REGION_ALL, 9, -1, 5, 0, {{0, 0}, {0, 0}}, 0, {{0, 0}, {0, 0}}},
    {"t.side", "tc", "x", -1, WT_SIDE, WT_CAT_OTHER, WT_REGION_ALL, 0, -1, 6, 0, {{0, 0}, {0, 0}}, 0, {{0, 0}, {0, 0}}},
};
#define NSYN ((int)(sizeof(k_syn) / sizeof(k_syn[0])))

static void syn(int progress, int region, unsigned flag, int want_status, const char *want_id) {
    Fake f;
    DW3WtState st;
    DW3WtResult r;

    memset(&f, 0, sizeof(f));
    if (flag) f.codes[f.n++] = flag;
    st = mk(progress, region, &f);
    dw3_wt_resolve_table(k_syn, NSYN, &st, &r);
    CHECK(r.status == want_status && strcmp(id_of(&r), want_id) == 0,
          "syn progress %d region %d flag %x: status %d step %s (want %d %s)", progress, region,
          flag, r.status, id_of(&r), want_status, want_id);
}

static void test_builtin(void) {
    int n, i, j;
    const DW3WtStep *t = dw3_wt_steps(&n);

    /* every first/last value of a step */
    expect_step(0, WT_REGION_US, 0, "ch01.s01");
    expect_step(1, WT_REGION_US, 0, "ch01.s02");
    expect_step(2, WT_REGION_US, 0, "ch01.s03");
    expect_step(3, WT_REGION_US, 0, "ch01.s04");
    expect_step(3, WT_REGION_US, 1, "ch01.s05"); /* flag 0x400D set: after event 50 */
    expect_step(4, WT_REGION_US, 0, "ch01.s06");
    expect_step(4, WT_REGION_US, 1, "ch01.s06");
    /* EU resolves the same rows (no EU-only rows before the extra chapter) */
    expect_step(0, WT_REGION_EU, 0, "ch01.s01");
    expect_step(3, WT_REGION_EU, 1, "ch01.s05");
    expect_step(4, WT_REGION_EU, 0, "ch01.s06");
    /* the rest of the first half, on both regions: one row per progress value, and the rows
     * that share a value move on when the game's flag or item says so */
    expect_with(5, 0, 0, 0, "ch02.s01");
    expect_with(6, 0, 0, 0, "ch02.s02");
    expect_with(7, 0, 0, 0, "ch02.s03");
    expect_with(8, 0, 0, 0, "ch02.s04");
    expect_with(8, WT_FLAG(0x1A, 0x17), 0, 0, "ch02.s05");
    expect_with(8, WT_FLAG(0x1A, 0x17), WT_FLAG(0x1A, 0x18), WT_FLAG(0x1A, 0x19), "ch02.s05");
    expect_with(8, WT_FLAG(0x1A, 0x17), WT_FLAG(0x1A, 0x1A), 0, "ch02.s06");
    expect_with(9, 0, 0, 0, "ch02.s07");
    expect_with(9, WT_ITEM(0, 0x15), 0, 0, "ch02.s07"); /* the item alone does not skip the talk */
    expect_with(9, WT_FLAG(0x1A, 0x1B), 0, 0, "ch02.s08");
    expect_with(9, WT_FLAG(0x1A, 0x1B), WT_ITEM(0, 0x15), 0, "ch02.s09");
    expect_with(0xA, 0, 0, 0, "ch02.s10");
    expect_with(0xB, 0, 0, 0, "ch03.s01");
    expect_with(0xC, 0, 0, 0, "ch03.s02");
    expect_with(0xD, 0, 0, 0, "ch03.s03");
    expect_with(0xD, WT_FLAG(0x1C, 0x0D), 0, 0, "ch03.s04");
    expect_with(0xE, 0, 0, 0, "ch03.s05");
    expect_with(0xF, 0, 0, 0, "ch04.s01");
    expect_with(0x10, 0, 0, 0, "ch04.s02");
    expect_with(0x11, 0, 0, 0, "ch04.s03");
    expect_with(0x12, 0, 0, 0, "ch05.s01");
    expect_with(0x13, 0, 0, 0, "ch05.s02");
    expect_with(0x14, 0, 0, 0, "ch05.s03");
    expect_with(0x14, WT_FLAG(0x40, 0x03), 0, 0, "ch05.s04");
    expect_with(0x15, 0, 0, 0, "ch05.s05");
    expect_with(0x16, 0, 0, 0, "ch05.s06");
    expect_with(0x17, 0, 0, 0, "ch05.s07");
    expect_with(0x17, WT_FLAG(0x40, 0x50), 0, 0, "ch05.s08");
    expect_with(0x18, 0, 0, 0, "ch06.s01");
    expect_with(0x19, 0, 0, 0, "ch06.s02");
    expect_with(0x1A, 0, 0, 0, "ch06.s03");
    expect_with(0x1B, 0, 0, 0, "ch07.s01");
    expect_with(0x1C, 0, 0, 0, "ch07.s02");
    expect_with(0x1D, 0, 0, 0, "ch07.s03");
    expect_with(0x1E, 0, 0, 0, "ch08.s01");
    expect_with(0x1F, 0, 0, 0, "ch08.s02");
    expect_with(0x20, 0, 0, 0, "ch08.s03");
    expect_with(0x20, WT_FLAG(0x40, 0x66), 0, 0, "ch08.s04");
    expect_with(0x21, 0, 0, 0, "ch08.s05");
    expect_with(0x21, WT_FLAG(0x40, 0x57), 0, 0, "ch08.s06");
    expect_with(0x21, WT_FLAG(0x40, 0x57), WT_FLAG(0x40, 0x59), 0, "ch08.s07");
    expect_with(0x22, 0, 0, 0, "ch09.s01");
    expect_with(0x23, 0, 0, 0, "ch09.s02");
    expect_with(0x24, 0, 0, 0, "ch09.s03");
    expect_with(0x25, 0, 0, 0, "ch10.s01");
    expect_with(0x25, WT_FLAG(0x40, 0x60), 0, 0, "ch10.s02");
    expect_with(0x26, 0, 0, 0, "ch10.s03");
    expect_with(0x27, 0, 0, 0, "ch10.s04");
    expect_with(0x28, 0, 0, 0, "ch10.s05");
    expect_with(0x29, 0, 0, 0, "ch10.s06");
    expect_with(0x29, WT_FLAG(0x40, 0x74), 0, 0, "ch10.s07");
    /* every value of the first half resolves, with no flag set and with all of them set
     * (all rows done: the last offered one) */
    {
        int p, i, n;
        const DW3WtStep *t = dw3_wt_steps(&n);

        for (p = 0; p <= 0x29; ++p) {
            int region;

            for (region = WT_REGION_US; region <= WT_REGION_EU; ++region) {
                Fake f;
                DW3WtState st;
                DW3WtResult r;
                int k;

                memset(&f, 0, sizeof(f));
                st = mk(p, region, &f);
                dw3_wt_resolve(&st, &r);
                CHECK(r.status == WT_OK, "progress 0x%x, no flags: status %d", p, r.status);
                memset(&f, 0, sizeof(f));
                for (i = 0; i < n; ++i) {
                    for (k = 0; k < t[i].ndone && f.n < 40; ++k) f.codes[f.n++] = t[i].done[k].code;
                    for (k = 0; k < t[i].nunlock && f.n < 40; ++k) f.codes[f.n++] = t[i].unlock[k].code;
                }
                st = mk(p, region, &f);
                dw3_wt_resolve(&st, &r);
                CHECK(r.status == WT_OK && r.step != NULL && r.step->from_progress <= p &&
                      (r.step->to_progress < 0 || p <= r.step->to_progress),
                      "progress 0x%x, all flags: status %d", p, r.status);
            }
        }
    }
    /* beyond the first half: valid progress values the table does not cover */
    expect_status(0x2A, WT_REGION_US, WT_ABOVE);
    expect_status(0x2B, WT_REGION_EU, WT_ABOVE);
    expect_status(WT_PROGRESS_MAX, WT_REGION_US, WT_ABOVE);
    /* EU extra chapter value on US must say "not recognised", not crash */
    expect_status(0x2D, WT_REGION_US, WT_ABOVE);
    expect_status(0x2D, WT_REGION_EU, WT_ABOVE);
    /* unknown values */
    expect_status(-1, WT_REGION_US, WT_UNKNOWN);
    expect_status(0x8000, WT_REGION_US, WT_UNKNOWN);
    expect_status(WT_PROGRESS_MAX + 1, WT_REGION_EU, WT_UNKNOWN);
    /* SIDE rows never decide the current step */
    for (i = 0; i < n; ++i) {
        if (t[i].kind == WT_SIDE) {
            CHECK(strcmp(t[i].id, "ch01.x01") == 0, "unexpected side row %s", t[i].id);
        }
    }
    /* table checks: unique ids, ordered, MAIN ranges ordered */
    for (i = 0; i < n; ++i) {
        CHECK(t[i].nunlock <= WT_MAX_CONDS && t[i].ndone <= WT_MAX_CONDS, "cond count %s", t[i].id);
        CHECK(t[i].to_progress < 0 || t[i].to_progress >= t[i].from_progress, "range %s", t[i].id);
        CHECK(t[i].region == WT_REGION_ALL || t[i].region == WT_REGION_US || t[i].region == WT_REGION_EU,
              "region %s", t[i].id);
        for (j = i + 1; j < n; ++j) {
            CHECK(strcmp(t[i].id, t[j].id) != 0, "duplicate id %s", t[i].id);
        }
        if (i > 0) {
            CHECK(t[i].from_progress >= t[i - 1].from_progress, "order %s", t[i].id);
            CHECK(t[i].order > t[i - 1].order, "order field %s", t[i].id);
        }
        if (t[i].kind == WT_MAIN && i + 1 < n && t[i + 1].kind == WT_MAIN) {
            /* a MAIN row does not run past the next one's start unless it shares the value */
            CHECK(t[i].to_progress < 0 || t[i].to_progress <= t[i + 1].from_progress,
                  "overlap %s / %s", t[i].id, t[i + 1].id);
        }
    }
}

static void test_synthetic(void) {
    /* below, first, last, gap, open end */
    syn(0, WT_REGION_US, 0, WT_BELOW, "-");
    syn(1, WT_REGION_US, 0, WT_BELOW, "-");
    syn(2, WT_REGION_US, 0, WT_OK, "t.a");
    syn(4, WT_REGION_US, 0, WT_OK, "t.a");
    /* t.b is current until its done flag is set, then t.c (unlock on the same flag) */
    syn(5, WT_REGION_US, 0, WT_OK, "t.b");
    syn(6, WT_REGION_US, 0x4001, WT_OK, "t.c");
    syn(7, WT_REGION_US, 0, WT_NO_MATCH, "-"); /* a gap inside the table */
    syn(8, WT_REGION_US, 0, WT_NO_MATCH, "-");
    syn(9, WT_REGION_US, 0, WT_OK, "t.end");
    syn(0x63, WT_REGION_US, 0, WT_OK, "t.end");
    /* EU only row: skipped on US (the value falls in a gap there), used on EU */
    syn(0x2D, WT_REGION_US, 0, WT_OK, "t.end");
    syn(0x2D, WT_REGION_EU, 0, WT_OK, "t.eu");
    syn(0x2E, WT_REGION_EU, 0, WT_OK, "t.end");
    /* unknown */
    syn(-5, WT_REGION_US, 0, WT_UNKNOWN, "-");
    syn(0x8000, WT_REGION_EU, 0, WT_UNKNOWN, "-");
    syn(0x64, WT_REGION_US, 0, WT_UNKNOWN, "-");
}

static void test_edges(void) {
    DW3WtState st;
    DW3WtResult r;
    char line[160];
    Fake f;

    /* a missing callback fails conditions closed: the unlock row is not offered */
    memset(&f, 0, sizeof(f));
    st = mk(3, WT_REGION_US, &f);
    st.check = NULL;
    dw3_wt_resolve(&st, &r);
    CHECK(r.status == WT_OK && strcmp(id_of(&r), "ch01.s04") == 0, "null check: %s", id_of(&r));
    /* empty and NULL tables */
    dw3_wt_resolve_table(NULL, 0, &st, &r);
    CHECK(r.status == WT_UNKNOWN, "null table");
    dw3_wt_resolve_table(k_syn, 0, &st, &r);
    CHECK(r.status == WT_UNKNOWN, "empty table");
    /* a table with only EU rows has no MAIN row for US */
    {
        static const DW3WtStep eu_only[] = {
            {"e.a", "e", "x", -1, WT_MAIN, 0, WT_REGION_EU, 0, 3, 1, 0, {{0, 0}, {0, 0}}, 0, {{0, 0}, {0, 0}}},
        };
        st = mk(1, WT_REGION_US, &f);
        dw3_wt_resolve_table(eu_only, 1, &st, &r);
        CHECK(r.status == WT_UNKNOWN, "eu-only table on US: %d", r.status);
        st = mk(1, WT_REGION_EU, &f);
        dw3_wt_resolve_table(eu_only, 1, &st, &r);
        CHECK(r.status == WT_OK, "eu-only table on EU: %d", r.status);
    }
    /* the log line format (AGENTS.md rule 12) */
    memset(&f, 0, sizeof(f));
    f.codes[f.n++] = WT_FLAG(0x40, 0x0D);
    st = mk(3, WT_REGION_US, &f);
    st.half = 0;
    st.area = 19;
    dw3_wt_resolve(&st, &r);
    dw3_wt_format_line(line, sizeof(line), &st, &r);
    CHECK(strcmp(line, "dw3-walkthrough: progress 3 half 0 area 19 -> chapter ch01 step ch01.s05 "
                       "(rule progress+flag)") == 0, "line: %s", line);
    st = mk(0x8000, WT_REGION_US, &f);
    dw3_wt_resolve(&st, &r);
    dw3_wt_format_line(line, sizeof(line), &st, &r);
    CHECK(strcmp(line, "dw3-walkthrough: progress 32768 half 0 area 19 -> chapter - step - "
                       "(rule unknown)") == 0, "line: %s", line);
    /* area and half helpers */
    CHECK(dw3_wt_area_of_mode(0x207) == 0x13, "asuka area");
    CHECK(dw3_wt_area_of_mode(0x2D8) == WT_AREA_NONE, "real world has no area");
    CHECK(dw3_wt_half_of_mode(0x26F) == 0 && dw3_wt_half_of_mode(0x270) == 1 &&
          dw3_wt_half_of_mode(0x2D7) == -1, "half boundaries");
    CHECK(dw3_wt_area_of_mode(-1) == WT_AREA_NONE, "negative mode");
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "list-ids") == 0) {
        int n, i;
        const DW3WtStep *t = dw3_wt_steps(&n);

        /* ID id chapter area kind region map_area from to order ndone done0 nunlock unlock0 */
        for (i = 0; i < n; ++i) {
            printf("ID %s %s %s %d %d %d %d %d %d %d %u %d %u\n", t[i].id, t[i].chapter, t[i].area,
                   t[i].kind, t[i].region, t[i].map_area, t[i].from_progress, t[i].to_progress,
                   t[i].order, t[i].ndone, t[i].ndone ? (unsigned)t[i].done[0].code : 0u,
                   t[i].nunlock, t[i].nunlock ? (unsigned)t[i].unlock[0].code : 0u);
        }
        return 0;
    }
    test_builtin();
    test_synthetic();
    test_edges();
    if (g_fail) {
        printf("%d failure(s)\n", g_fail);
        return 1;
    }
    printf("OK: walkthrough resolver\n");
    return 0;
}
