/* P6.24: dw3_region_resolve(), the run-time region choice of a `both` image
 * (port/region/dw3_region_rt.c): a forced region wins, then the runtime folder
 * that is given, then what is present under the base folder and the stored
 * preference. Usage: region_rt_test <scratch dir> (must exist and be empty). */
#include "dw3_region_rt.h"

#include "port_options_cfg.h"
#include "fake_iso.h"

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

static void make_tree(const char *base, const char *r) {
    char dir[512];

    snprintf(dir, sizeof(dir), "%s/%s", base, r);
    mkdir(dir, 0777);
    if (strcmp(r, "us") == 0) {
        touch(dir, "files_us.json");
        touch(dir, "SLUS_014.36");
    } else {
        touch(dir, "files_eu.json");
        touch(dir, "SLES_039.36");
    }
}

static Dw3RegionChoice resolve(const char *flag, const char *env, const char *root,
                               const char *base, const char *cfg, int *rc) {
    Dw3RegionRequest req;
    Dw3RegionChoice c;

    memset(&req, 0, sizeof(req));
    req.flag = flag;
    req.env = env;
    req.root = root;
    req.base = base;
    req.cfg_dir = cfg;
    *rc = dw3_region_resolve(&req, &c);
    return c;
}

int main(int argc, char **argv) {
    char base[200], cfg[200], root[512];
    Dw3RegionChoice c;
    int rc;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <scratch dir>\n", argv[0]);
        return 2;
    }
    snprintf(base, sizeof(base), "%s/rt", argv[1]);
    snprintf(cfg, sizeof(cfg), "%s/cfg", argv[1]);
    mkdir(base, 0777);
    mkdir(cfg, 0777);

    /* nothing present: US, said so */
    c = resolve(NULL, NULL, NULL, base, cfg, &rc);
    CHECK(rc == 0 && c.region == DW3_REGION_US && strcmp(c.source, "default") == 0);

    /* one tree present: that one, whatever the stored choice says */
    make_tree(base, "eu");
    c = resolve(NULL, NULL, NULL, base, cfg, &rc);
    CHECK(rc == 0 && c.region == DW3_REGION_EU && strcmp(c.source, "probe-one") == 0);
    CHECK(!c.probe.has_us && c.probe.has_eu);

    /* both present, nothing stored: US; last_region; then region_default wins */
    make_tree(base, "us");
    c = resolve(NULL, NULL, NULL, base, cfg, &rc);
    CHECK(rc == 0 && c.region == DW3_REGION_US && strcmp(c.source, "default") == 0);
    CHECK(c.probe.has_us && c.probe.has_eu);
    CHECK(dw3_region_cfg_write(cfg, DW3_REGION_EU, 0, DW3_REGION_NONE) == 0);
    c = resolve(NULL, NULL, NULL, base, cfg, &rc);
    CHECK(rc == 0 && c.region == DW3_REGION_EU && strcmp(c.source, "probe-last") == 0);
    CHECK(dw3_region_cfg_write(cfg, DW3_REGION_EU, 1, DW3_REGION_US) == 0);
    c = resolve(NULL, NULL, NULL, base, cfg, &rc);
    CHECK(rc == 0 && c.region == DW3_REGION_US && strcmp(c.source, "probe-pref") == 0);

    /* an explicit runtime folder is judged by what it holds, then by its name */
    snprintf(root, sizeof(root), "%s/eu", base);
    c = resolve(NULL, NULL, root, base, cfg, &rc);
    CHECK(rc == 0 && c.region == DW3_REGION_EU && strcmp(c.source, "runtime-root") == 0);
    snprintf(root, sizeof(root), "%s/elsewhere", argv[1]);
    mkdir(root, 0777);
    c = resolve(NULL, NULL, root, base, cfg, &rc);
    CHECK(rc == 0 && c.region == DW3_REGION_US && strcmp(c.source, "default") == 0);
    snprintf(root, sizeof(root), "%s/mnt/EU/", argv[1]);
    c = resolve(NULL, NULL, root, base, cfg, &rc);
    CHECK(rc == 0 && c.region == DW3_REGION_EU && strcmp(c.source, "root-name") == 0);
    c = resolve(NULL, NULL, "plus", base, cfg, &rc); /* "plus" ends in "us" but is not "/us" */
    CHECK(rc == 0 && c.region == DW3_REGION_US && strcmp(c.source, "default") == 0);

    /* a forced region wins over everything and a typo is an error */
    c = resolve("eu", "us", root, base, cfg, &rc);
    CHECK(rc == 0 && c.region == DW3_REGION_EU && strcmp(c.source, "flag") == 0);
    c = resolve(NULL, "EU", NULL, base, cfg, &rc);
    CHECK(rc == 0 && c.region == DW3_REGION_EU && strcmp(c.source, "env") == 0);
    c = resolve("xx", "us", NULL, base, cfg, &rc);
    CHECK(rc != 0 && c.region == DW3_REGION_NONE);
    c = resolve(NULL, "usa", NULL, base, cfg, &rc);
    CHECK(rc != 0);
    c = resolve("", "", NULL, base, cfg, &rc); /* empty means "not given" */
    CHECK(rc == 0 && strcmp(c.source, "probe-pref") == 0);

    /* issue #322: a disc in the parent folder counts as that region's data */
    {
        char b2[300], p[340];

        snprintf(b2, sizeof(b2), "%s/rt2", argv[1]);
        mkdir(b2, 0777);
        snprintf(p, sizeof(p), "%s/eu.bin", b2);
        CHECK(make_iso(p, "SLES_039.36") == 0);
        c = resolve(NULL, NULL, NULL, b2, cfg, &rc);
        CHECK(rc == 0 && c.region == DW3_REGION_EU && strcmp(c.source, "probe-one") == 0);
        CHECK(!c.probe.has_us && c.probe.has_eu);
        snprintf(p, sizeof(p), "%s/us.bin", b2);
        CHECK(make_iso(p, "SLUS_014.36") == 0);
        c = resolve(NULL, NULL, NULL, b2, cfg, &rc);
        CHECK(c.probe.has_us && c.probe.has_eu);
    }

    /* the plain build's answer */
    CHECK(dw3_region() == DW3_REGION_US);

    if (g_fail) {
        return 1;
    }
    printf("OK: region_rt\n");
    return 0;
}
