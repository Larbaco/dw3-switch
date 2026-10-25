/* P6.24 slice 1: the region decision table, the picker state machine and the
 * stored choice (port/region/dw3_region.c).
 * Usage: region_test <scratch dir> (must exist and be empty). */
#include "dw3_region.h"

#include "port_options_cfg.h"

#include <stdio.h>
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

static void touch(const char *dir, const char *rel) {
    char path[512];
    FILE *fp;

    snprintf(path, sizeof(path), "%s/%s", dir, rel);
    fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        g_fail = 1;
        return;
    }
    fputs("x", fp);
    fclose(fp);
}

static Dw3BootDecision decide(int us, int eu, Dw3Region forced, Dw3Region pref,
                              Dw3Region last, int minus) {
    Dw3RegionInput in;

    memset(&in, 0, sizeof(in));
    in.probe.has_us = us;
    in.probe.has_eu = eu;
    in.forced = forced;
    in.pref = pref;
    in.last = last;
    in.minus_held = minus;
    return dw3_region_decide(&in);
}

static void test_parse(void) {
    CHECK(dw3_region_parse("us") == DW3_REGION_US);
    CHECK(dw3_region_parse("EU") == DW3_REGION_EU);
    CHECK(dw3_region_parse("Us") == DW3_REGION_US);
    CHECK(dw3_region_parse("ask") == DW3_REGION_NONE);
    CHECK(dw3_region_parse("") == DW3_REGION_NONE);
    CHECK(dw3_region_parse("usa") == DW3_REGION_NONE);
    CHECK(dw3_region_parse(NULL) == DW3_REGION_NONE);
    CHECK(strcmp(dw3_region_name(DW3_REGION_EU), "eu") == 0);
}

static void test_decide(void) {
    Dw3BootDecision d;
    const Dw3Region N = DW3_REGION_NONE, U = DW3_REGION_US, E = DW3_REGION_EU;

    /* No data: the setup message, whatever is stored. */
    d = decide(0, 0, N, E, E, 1);
    CHECK(d.action == DW3_BOOT_NO_DATA);
    /* One region: boot it, no picker; preference, last and Minus ignored. */
    d = decide(1, 0, N, E, E, 1);
    CHECK(d.action == DW3_BOOT_REGION && d.region == U);
    d = decide(0, 1, N, U, U, 1);
    CHECK(d.action == DW3_BOOT_REGION && d.region == E);
    d = decide(1, 0, N, N, N, 0);
    CHECK(d.action == DW3_BOOT_REGION && d.region == U);
    /* Both, ask (default): picker, default entry = last, else US. */
    d = decide(1, 1, N, N, N, 0);
    CHECK(d.action == DW3_BOOT_PICK && d.region == U);
    d = decide(1, 1, N, N, E, 0);
    CHECK(d.action == DW3_BOOT_PICK && d.region == E);
    /* Both, remembered: boot it, no picker. */
    d = decide(1, 1, N, E, U, 0);
    CHECK(d.action == DW3_BOOT_REGION && d.region == E);
    d = decide(1, 1, N, U, E, 0);
    CHECK(d.action == DW3_BOOT_REGION && d.region == U);
    /* Both, remembered, Minus held: the picker comes back. */
    d = decide(1, 1, N, E, U, 1);
    CHECK(d.action == DW3_BOOT_PICK && d.region == U);
    /* Forced wins over everything, even a missing probe. */
    d = decide(1, 1, E, U, U, 1);
    CHECK(d.action == DW3_BOOT_REGION && d.region == E);
    d = decide(0, 0, U, N, N, 0);
    CHECK(d.action == DW3_BOOT_REGION && d.region == U);
}

static void test_picker(void) {
    Dw3Picker p;
    unsigned t;

    /* Countdown: 3 s, no input, boots the default without "always". */
    dw3_picker_init(&p, DW3_REGION_EU);
    CHECK(p.sel == DW3_REGION_EU && dw3_picker_seconds_left(&p) == 3);
    for (t = 0; t < 2900; t += 100) {
        CHECK(dw3_picker_step(&p, 0, 100) == DW3_PICKER_RUNNING);
    }
    CHECK(dw3_picker_seconds_left(&p) == 1);
    CHECK(dw3_picker_step(&p, 0, 100) == DW3_PICKER_CHOSEN);
    CHECK(p.chosen == DW3_REGION_EU && p.timed_out && !p.always);

    /* A D-pad press cancels the countdown: it never boots under the finger. */
    dw3_picker_init(&p, DW3_REGION_US);
    dw3_picker_step(&p, 0, 1000);
    CHECK(dw3_picker_step(&p, DW3_PK_RIGHT, 100) == DW3_PICKER_RUNNING);
    CHECK(p.sel == DW3_REGION_EU && p.cancelled && dw3_picker_seconds_left(&p) == -1);
    CHECK(dw3_picker_step(&p, 0, 60000) == DW3_PICKER_RUNNING);
    CHECK(dw3_picker_step(&p, DW3_PK_LEFT, 100) == DW3_PICKER_RUNNING);
    CHECK(p.sel == DW3_REGION_US);
    CHECK(dw3_picker_step(&p, DW3_PK_A, 16) == DW3_PICKER_CHOSEN);
    CHECK(p.chosen == DW3_REGION_US && !p.timed_out && !p.always);

    /* Y toggles "always" (and cancels the countdown); A confirms. */
    dw3_picker_init(&p, DW3_REGION_US);
    dw3_picker_step(&p, DW3_PK_Y, 16);
    CHECK(p.always && p.cancelled);
    dw3_picker_step(&p, DW3_PK_Y, 16);
    CHECK(!p.always);
    dw3_picker_step(&p, DW3_PK_Y, 16);
    dw3_picker_step(&p, DW3_PK_RIGHT | DW3_PK_A, 16);
    CHECK(p.state == DW3_PICKER_CHOSEN && p.chosen == DW3_REGION_EU && p.always);

    /* A at once boots the highlighted entry; + quits; a finished picker stays. */
    dw3_picker_init(&p, DW3_REGION_US);
    CHECK(dw3_picker_step(&p, DW3_PK_A, 16) == DW3_PICKER_CHOSEN);
    CHECK(dw3_picker_step(&p, DW3_PK_RIGHT, 16) == DW3_PICKER_CHOSEN);
    CHECK(p.chosen == DW3_REGION_US);
    dw3_picker_init(&p, DW3_REGION_EU);
    CHECK(dw3_picker_step(&p, DW3_PK_PLUS, 16) == DW3_PICKER_QUIT);

    /* A bad default falls back to US. */
    dw3_picker_init(&p, DW3_REGION_NONE);
    CHECK(p.sel == DW3_REGION_US);
}

static void test_probe_and_cfg(const char *scratch) {
    Dw3RegionProbe pr;
    Dw3Region pref, last;
    char sub[512], cfg_text[256];
    FILE *fp;

    dw3_region_probe(scratch, &pr);
    CHECK(!pr.has_us && !pr.has_eu);
    /* Partial trees do not count. */
    snprintf(sub, sizeof(sub), "%s/us", scratch);
    {
        char d[512];

        snprintf(d, sizeof(d), "%s/us", scratch);
        CHECK(mkdir(d, 0777) == 0);
        snprintf(d, sizeof(d), "%s/eu", scratch);
        CHECK(mkdir(d, 0777) == 0);
    }
    touch(sub, "files_us.json");
    dw3_region_probe(scratch, &pr);
    CHECK(!pr.has_us);
    touch(sub, "SLUS_014.36");
    dw3_region_probe(scratch, &pr);
    CHECK(pr.has_us && !pr.has_eu);
    snprintf(sub, sizeof(sub), "%s/eu", scratch);
    touch(sub, "files_eu.json");
    touch(sub, "SLUS_014.36"); /* the other region's executable: not enough */
    dw3_region_probe(scratch, &pr);
    CHECK(pr.has_us && !pr.has_eu);
    touch(sub, "SLES_039.36");
    dw3_region_probe(scratch, &pr);
    CHECK(pr.has_us && pr.has_eu);
    CHECK(dw3_region_runtime_complete(sub, DW3_REGION_EU));
    CHECK(!dw3_region_runtime_complete(sub, DW3_REGION_US));
    CHECK(!dw3_region_runtime_complete(NULL, DW3_REGION_US));

    /* Stored choice: missing file = ask / unknown. */
    dw3_region_cfg_read(scratch, &pref, &last);
    CHECK(pref == DW3_REGION_NONE && last == DW3_REGION_NONE);
    CHECK(dw3_region_cfg_write(scratch, DW3_REGION_EU, 0, DW3_REGION_NONE) == 0);
    dw3_region_cfg_read(scratch, &pref, &last);
    CHECK(pref == DW3_REGION_NONE && last == DW3_REGION_EU);
    CHECK(dw3_region_cfg_write(scratch, DW3_REGION_US, 1, DW3_REGION_US) == 0);
    dw3_region_cfg_read(scratch, &pref, &last);
    CHECK(pref == DW3_REGION_US && last == DW3_REGION_US);
    /* Back to ask. */
    CHECK(dw3_region_cfg_write(scratch, DW3_REGION_EU, 1, DW3_REGION_NONE) == 0);
    dw3_region_cfg_read(scratch, &pref, &last);
    CHECK(pref == DW3_REGION_NONE && last == DW3_REGION_EU);

    /* Foreign keys survive a write; junk values read as ask / unknown. */
    snprintf(sub, sizeof(sub), "%s/port_options.cfg", scratch);
    fp = fopen(sub, "wb");
    CHECK(fp != NULL);
    if (fp != NULL) {
        fputs("version=1\nwidescreen=0\nregion_default=banana\nlast_region=??\nend=1\n", fp);
        fclose(fp);
    }
    dw3_region_cfg_read(scratch, &pref, &last);
    CHECK(pref == DW3_REGION_NONE && last == DW3_REGION_NONE);
    CHECK(dw3_region_cfg_write(scratch, DW3_REGION_US, 0, DW3_REGION_NONE) == 0);
    fp = fopen(sub, "rb");
    CHECK(fp != NULL);
    if (fp != NULL) {
        size_t n = fread(cfg_text, 1, sizeof(cfg_text) - 1, fp);

        cfg_text[n] = '\0';
        fclose(fp);
        CHECK(strstr(cfg_text, "widescreen=0") != NULL);
        CHECK(strstr(cfg_text, "last_region=us") != NULL);
        CHECK(strstr(cfg_text, "region_default=banana") != NULL);
        CHECK(strstr(cfg_text, "end=1") != NULL);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <scratch dir>\n", argv[0]);
        return 2;
    }
    test_parse();
    test_decide();
    test_picker();
    test_probe_and_cfg(argv[1]);
    if (g_fail) {
        return 1;
    }
    printf("OK: region\n");
    return 0;
}
