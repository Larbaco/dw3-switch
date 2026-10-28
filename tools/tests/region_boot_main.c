/* P6.24: the boot flow of the combined image (port/region/dw3_region_boot.c)
 * with a fake console: which data is found, when the picker is shown or
 * skipped and why, the stored choice, the log lines the OLED plan relies on.
 * Usage: region_boot_test <scratch dir> (must exist and be empty). */
#include "dw3_region_boot.h"

#include "port_options_cfg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int g_fail;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond);          \
            g_fail = 1;                                                      \
        }                                                                    \
    } while (0)

typedef struct Fake {
    char log[8192];
    int minus;          /* what sample_minus returns */
    int minus_calls;
    int picker_calls;
    int no_data_calls;
    unsigned script[8]; /* button edges, one per picker step */
    int script_len;
    int disc_us, disc_eu;
} Fake;

static void fk_log(void *ud, const char *line) {
    Fake *f = (Fake *)ud;

    strncat(f->log, line, sizeof(f->log) - strlen(f->log) - 2);
    strcat(f->log, "\n");
}

static int fk_minus(void *ud) {
    Fake *f = (Fake *)ud;

    f->minus_calls++;
    return f->minus;
}

static unsigned fk_picker(void *ud, Dw3Picker *p) {
    Fake *f = (Fake *)ud;
    unsigned ms = 0;
    int i;

    f->picker_calls++;
    for (i = 0; i < f->script_len && p->state == DW3_PICKER_RUNNING; i++) {
        dw3_picker_step(p, f->script[i], 100);
        ms += 100;
    }
    while (p->state == DW3_PICKER_RUNNING) { /* nobody presses: the countdown */
        dw3_picker_step(p, 0, 100);
        ms += 100;
    }
    return ms;
}

static void fk_no_data(void *ud, const char *base) {
    (void)base;
    ((Fake *)ud)->no_data_calls++;
}

static int fk_disc(void *ud, const char *base, Dw3Region r) {
    Fake *f = (Fake *)ud;

    (void)base;
    return r == DW3_REGION_US ? f->disc_us : f->disc_eu;
}

static void touch(const char *dir, const char *rel) {
    char path[512];
    FILE *fp;

    snprintf(path, sizeof(path), "%s/%s", dir, rel);
    fp = fopen(path, "wb");
    if (fp != NULL) {
        fputs("x", fp);
        fclose(fp);
    }
}

static void make_tree(const char *base, const char *r) {
    char dir[512];

    snprintf(dir, sizeof(dir), "%s/%s", base, r);
    mkdir(dir, 0777);
    touch(dir, strcmp(r, "us") == 0 ? "files_us.json" : "files_eu.json");
    touch(dir, strcmp(r, "us") == 0 ? "SLUS_014.36" : "SLES_039.36");
}

static void clean(const char *base) {
    char cmd[600];

    snprintf(cmd, sizeof(cmd), "rm -rf '%s' && mkdir -p '%s'", base, base);
    if (system(cmd) != 0) {
        g_fail = 1;
    }
}

static Dw3RegionBootOut run(const char *base, Fake *f, Dw3Region forced) {
    Dw3RegionBootIO io;
    Dw3RegionBootIn in;
    Dw3RegionBootOut out;

    memset(&io, 0, sizeof(io));
    io.ud = f;
    io.log = fk_log;
    io.sample_minus = fk_minus;
    io.run_picker = fk_picker;
    io.show_no_data = fk_no_data;
    io.disc_waiting = fk_disc;
    in.base = base;
    in.forced = forced;
    in.forced_by = "test";
    f->log[0] = '\0';
    f->minus_calls = f->picker_calls = f->no_data_calls = 0;
    dw3_region_boot(&in, &io, &out);
    return out;
}

static void cfg(const char *base, Dw3Region *pref, Dw3Region *last) {
    dw3_region_cfg_read(base, pref, last);
}

int main(int argc, char **argv) {
    char base[300];
    Fake f;
    Dw3RegionBootOut o;
    Dw3Region pref, last;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <scratch dir>\n", argv[0]);
        return 2;
    }
    snprintf(base, sizeof(base), "%s/dw3", argv[1]);

    /* 1. no data: the setup screen, nothing stored */
    memset(&f, 0, sizeof(f));
    clean(base);
    o = run(base, &f, DW3_REGION_NONE);
    CHECK(o.result == DW3_RB_NO_DATA && f.no_data_calls == 1 && f.picker_calls == 0);
    CHECK(strstr(f.log, "data found us=0 eu=0 under") != NULL);
    CHECK(strstr(f.log, "decision no-data region=none: no game data found") != NULL);
    cfg(base, &pref, &last);
    CHECK(pref == DW3_REGION_NONE && last == DW3_REGION_NONE);

    /* 2. only US: boots it, no picker, no Minus sample, last_region stored */
    clean(base);
    make_tree(base, "us");
    o = run(base, &f, DW3_REGION_NONE);
    CHECK(o.result == DW3_RB_RUN && o.region == DW3_REGION_US && f.picker_calls == 0);
    CHECK(f.minus_calls == 0);
    CHECK(strstr(f.log, "data found us=1 eu=0 under") != NULL);
    CHECK(strstr(f.log, "minus_held=n/a") != NULL);
    CHECK(strstr(f.log, "decision boot region=us: only the US data is present") != NULL);
    CHECK(strstr(f.log, "picker shown") == NULL);
    CHECK(strstr(f.log, "saved last_region=us\n") != NULL);
    cfg(base, &pref, &last);
    CHECK(pref == DW3_REGION_NONE && last == DW3_REGION_US);

    /* 3. only EU */
    clean(base);
    make_tree(base, "eu");
    o = run(base, &f, DW3_REGION_NONE);
    CHECK(o.result == DW3_RB_RUN && o.region == DW3_REGION_EU && f.picker_calls == 0);
    CHECK(strstr(f.log, "decision boot region=eu: only the EU data is present") != NULL);

    /* 4. both, nothing stored: the picker, the countdown boots the US default */
    clean(base);
    make_tree(base, "us");
    make_tree(base, "eu");
    o = run(base, &f, DW3_REGION_NONE);
    CHECK(o.result == DW3_RB_RUN && o.region == DW3_REGION_US && f.picker_calls == 1);
    CHECK(f.minus_calls == 0); /* the picker comes anyway: no 200 ms sample */
    CHECK(strstr(f.log, "decision pick region=us: both present: picker") != NULL);
    CHECK(strstr(f.log, "picker shown (default entry us, timeout 3000 ms)") != NULL);
    CHECK(strstr(f.log, "picker result us by countdown after 3000 ms always=0") != NULL);
    CHECK(strstr(f.log, "saved last_region=us\n") != NULL);
    CHECK(strstr(o.source, "countdown") != NULL);
    cfg(base, &pref, &last);
    CHECK(pref == DW3_REGION_NONE && last == DW3_REGION_US);

    /* 5. the default entry is the last choice; Right + A picks EU by button */
    f.script[0] = DW3_PK_RIGHT;
    f.script[1] = DW3_PK_A;
    f.script_len = 2;
    o = run(base, &f, DW3_REGION_NONE);
    CHECK(o.region == DW3_REGION_EU && strstr(o.source, "button A") != NULL);
    CHECK(strstr(f.log, "picker shown (default entry us,") != NULL);
    CHECK(strstr(f.log, "picker result eu by button A after 200 ms always=0") != NULL);
    cfg(base, &pref, &last);
    CHECK(pref == DW3_REGION_NONE && last == DW3_REGION_EU);
    f.script_len = 0;
    o = run(base, &f, DW3_REGION_NONE); /* last = eu: the countdown boots EU */
    CHECK(o.region == DW3_REGION_EU);
    CHECK(strstr(f.log, "picker shown (default entry eu,") != NULL);
    CHECK(strstr(f.log, "picker result eu by countdown after 3000 ms") != NULL);

    /* 6. Y = don't ask again, stored with the choice */
    f.script[0] = DW3_PK_LEFT;
    f.script[1] = DW3_PK_Y;
    f.script[2] = DW3_PK_A;
    f.script_len = 3;
    o = run(base, &f, DW3_REGION_NONE);
    CHECK(o.region == DW3_REGION_US);
    CHECK(strstr(f.log, "picker result us by button A after 300 ms always=1") != NULL);
    CHECK(strstr(f.log, "saved last_region=us region_default=us") != NULL);
    cfg(base, &pref, &last);
    CHECK(pref == DW3_REGION_US && last == DW3_REGION_US);
    f.script_len = 0;

    /* 7. region_default stored: no picker; the Minus sample runs and asks again */
    f.minus = 0;
    o = run(base, &f, DW3_REGION_NONE);
    CHECK(o.region == DW3_REGION_US && f.picker_calls == 0 && f.minus_calls == 1);
    CHECK(strstr(f.log, "stored region_default=us last_region=us minus_held=0") != NULL);
    CHECK(strstr(f.log, "decision boot region=us: region_default") != NULL);
    CHECK(strstr(f.log, "picker shown") == NULL);
    f.minus = 1;
    o = run(base, &f, DW3_REGION_NONE);
    CHECK(f.picker_calls == 1 && f.minus_calls == 1);
    CHECK(strstr(f.log, "minus_held=1") != NULL);
    CHECK(strstr(f.log, "decision pick region=us: Minus held: picker") != NULL);
    CHECK(strstr(f.log, "picker shown (default entry us,") != NULL);
    cfg(base, &pref, &last);
    CHECK(pref == DW3_REGION_US); /* asking again does not clear it */
    f.minus = 0;

    /* 8. + leaves the picker: nothing started, nothing written */
    clean(base);
    make_tree(base, "us");
    make_tree(base, "eu");
    f.script[0] = DW3_PK_PLUS;
    f.script_len = 1;
    o = run(base, &f, DW3_REGION_NONE);
    CHECK(o.result == DW3_RB_QUIT && o.region == DW3_REGION_NONE);
    CHECK(strstr(f.log, "picker quit by + after 100 ms") != NULL);
    CHECK(strstr(f.log, "saved") == NULL);
    cfg(base, &pref, &last);
    CHECK(pref == DW3_REGION_NONE && last == DW3_REGION_NONE);
    f.script_len = 0;

    /* 9. forced: no probe, no picker, nothing written, one line */
    clean(base);
    make_tree(base, "us");
    make_tree(base, "eu");
    o = run(base, &f, DW3_REGION_EU);
    CHECK(o.result == DW3_RB_RUN && o.region == DW3_REGION_EU && f.picker_calls == 0);
    CHECK(strstr(f.log, "forced eu by test: no probe, no picker, nothing written") != NULL);
    CHECK(strstr(f.log, "data found") == NULL);
    cfg(base, &pref, &last);
    CHECK(pref == DW3_REGION_NONE && last == DW3_REGION_NONE);

    /* 10. a disc image waiting in a region folder makes that region present */
    clean(base);
    make_tree(base, "eu");
    f.disc_us = 1;
    o = run(base, &f, DW3_REGION_NONE);
    CHECK(f.picker_calls == 1); /* eu tree + us disc: both present, ask */
    CHECK(strstr(f.log, "data found us=1 eu=1 under") != NULL);
    CHECK(strstr(f.log, "(us: disc image, needs extraction)") != NULL);
    f.disc_us = 0;

    /* 11. a picker that does not run (no screen) cannot hang the boot */
    {
        Dw3RegionBootIO io;
        Dw3RegionBootIn in;

        clean(base);
        make_tree(base, "us");
        make_tree(base, "eu");
        memset(&io, 0, sizeof(io));
        io.ud = &f;
        io.log = fk_log;
        in.base = base;
        in.forced = DW3_REGION_NONE;
        in.forced_by = NULL;
        f.log[0] = '\0';
        dw3_region_boot(&in, &io, &o);
        CHECK(o.result == DW3_RB_QUIT || o.result == DW3_RB_RUN);
    }

    if (g_fail) {
        return 1;
    }
    printf("OK: region_boot\n");
    return 0;
}
