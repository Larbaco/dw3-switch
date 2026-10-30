/* Host test of P6.38's platform independent part (port/region/
 * dw3_region_relaunch.c): the override argument, the argument string of the
 * Switch's envSetNextLoad, the PC's execv argv, and "is the target region's data
 * on the card". Usage: region_relaunch_test <scratch dir> (must exist, empty). */
#include "dw3_region_relaunch.h"

#include "fake_iso.h"

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

static void make_tree(const char *base, const char *r) {
    char dir[512];

    snprintf(dir, sizeof(dir), "%s/%s", base, r);
    mkdir(dir, 0777);
    touch(dir, strcmp(r, "us") == 0 ? "files_us.json" : "files_eu.json");
    touch(dir, strcmp(r, "us") == 0 ? "SLUS_014.36" : "SLES_039.36");
}

static int same(char **argv, const char *const *want) {
    int i;

    for (i = 0; want[i] != NULL; i++) {
        if (argv[i] == NULL || strcmp(argv[i], want[i]) != 0) {
            return 0;
        }
    }
    return argv[i] == NULL;
}

int main(int argc, char **argv) {
    char base[300], p[400], args[64];
    char *a1[] = {"dw3", "--relaunch-region", "eu"};
    char *a2[] = {"dw3", "--relaunch-region=US"};
    char *a3[] = {"dw3", "--relaunch-region", "xx"};
    char *a4[] = {"dw3", "--region", "us", "--relaunch-region"};
    char *a5[] = {"dw3", "--runtime", "/x/us", "--region", "us", "--memcard-dir", "m",
                  "--relaunch-region", "us", "--config-dir=c"};
    char **out;
    int n;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <scratch dir>\n", argv[0]);
        return 2;
    }

    /* the override argument */
    CHECK(dw3_relaunch_arg_region(3, a1) == DW3_REGION_EU);
    CHECK(dw3_relaunch_arg_region(2, a2) == DW3_REGION_US);
    CHECK(dw3_relaunch_arg_region(3, a3) == DW3_REGION_NONE);
    CHECK(dw3_relaunch_arg_region(4, a4) == DW3_REGION_NONE); /* no value */
    CHECK(dw3_relaunch_arg_region(1, a1) == DW3_REGION_NONE);  /* argv[0] only */
    CHECK(dw3_relaunch_arg_region(0, a1) == DW3_REGION_NONE);

    /* the Switch argument string: path, then the override; quoted when it has a space */
    n = dw3_relaunch_nx_args("sdmc:/switch/dw3-switch.nro", DW3_REGION_EU, args, sizeof(args));
    CHECK(n > 0 && strcmp(args, "sdmc:/switch/dw3-switch.nro --relaunch-region eu") == 0 &&
          (size_t)n == strlen(args));
    n = dw3_relaunch_nx_args("sdmc:/switch/my games/dw3.nro", DW3_REGION_US, p, sizeof(p));
    CHECK(n > 0 && strcmp(p, "\"sdmc:/switch/my games/dw3.nro\" --relaunch-region us") == 0);
    CHECK(dw3_relaunch_nx_args("sdmc:/a.nro", DW3_REGION_EU, args, 10) == -1); /* does not fit */
    CHECK(dw3_relaunch_nx_args("", DW3_REGION_EU, args, sizeof(args)) == -1);
    CHECK(dw3_relaunch_nx_args(NULL, DW3_REGION_EU, args, sizeof(args)) == -1);
    CHECK(dw3_relaunch_nx_args("sdmc:/a.nro", DW3_REGION_NONE, args, sizeof(args)) == -1);
    /* the string round-trips through the parser the new process uses */
    {
        char *tok[] = {"sdmc:/a.nro", "--relaunch-region", "eu"};

        CHECK(dw3_relaunch_arg_region(3, tok) == DW3_REGION_EU);
    }

    /* the PC argv: --region, --runtime and an older override go (with their values) */
    out = dw3_relaunch_pc_argv(10, a5, DW3_REGION_EU);
    CHECK(out != NULL);
    if (out != NULL) {
        const char *want[] = {"dw3", "--memcard-dir", "m", "--config-dir=c",
                              "--relaunch-region", "eu", NULL};

        CHECK(same(out, want));
        CHECK(dw3_relaunch_arg_region(6, out) == DW3_REGION_EU);
        free(out);
    }
    out = dw3_relaunch_pc_argv(1, a1, DW3_REGION_US);
    CHECK(out != NULL);
    if (out != NULL) {
        const char *want[] = {"dw3", "--relaunch-region", "us", NULL};

        CHECK(same(out, want));
        free(out);
    }

    /* is the target's data on the card */
    snprintf(base, sizeof(base), "%s/card", argv[1]);
    mkdir(base, 0777);
    CHECK(!dw3_relaunch_target_present(base, DW3_REGION_EU));
    CHECK(!dw3_relaunch_target_present(base, DW3_REGION_NONE));
    CHECK(!dw3_relaunch_target_present(NULL, DW3_REGION_EU));
    make_tree(base, "us");
    CHECK(dw3_relaunch_target_present(base, DW3_REGION_US));
    CHECK(!dw3_relaunch_target_present(base, DW3_REGION_EU));
    make_tree(base, "eu");
    CHECK(dw3_relaunch_target_present(base, DW3_REGION_EU));
    /* a disc image waiting to be extracted counts (issue #322) */
    snprintf(base, sizeof(base), "%s/card2", argv[1]);
    mkdir(base, 0777);
    make_tree(base, "us");
    snprintf(p, sizeof(p), "%s/eu.bin", base);
    CHECK(make_iso(p, "SLES_039.36") == 0);
    CHECK(dw3_relaunch_target_present(base, DW3_REGION_EU));

    if (g_fail) {
        return 1;
    }
    printf("OK: region relaunch helpers\n");
    return 0;
}
