/* P9.4 option file store: parse / serialize / load / atomic save.
 * Usage: options_cfg_test <scratch dir> (must exist and be empty). */
#include "port_options_cfg.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static int g_fail;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond);          \
            g_fail = 1;                                                      \
        }                                                                    \
    } while (0)

static void write_file(const char *dir, const char *suffix, const char *text) {
    char path[DW3_OPTCFG_PATH_MAX];
    FILE *fp;

    dw3_optcfg_path(dir, suffix, path, sizeof(path));
    fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        g_fail = 1;
        return;
    }
    fputs(text, fp);
    fclose(fp);
}

static int file_exists(const char *dir, const char *suffix) {
    char path[DW3_OPTCFG_PATH_MAX];
    FILE *fp;

    dw3_optcfg_path(dir, suffix, path, sizeof(path));
    fp = fopen(path, "rb");
    if (fp != NULL) {
        fclose(fp);
        return 1;
    }
    return 0;
}

static void test_parse(void) {
    static Dw3OptCfg c;
    const char text[] =
        "# comment\r\n"
        "version=1\r\n"
        "\r\n"
        "  widescreen = 0  \n"
        "fast_forward=3\n"
        "garbage line without equals\n"
        "=novalue_key\n"
        "bad key=1\n"
        "future_key=hello world\n"
        "widescreen=1\n" /* later duplicate wins */
        "; other comment\n"
        "end=1";         /* no trailing newline */

    dw3_optcfg_parse(&c, text, sizeof(text) - 1);
    CHECK(c.version == 1);
    CHECK(c.complete == 1);
    CHECK(c.skipped == 3);
    CHECK(c.count == 3);
    CHECK(strcmp(dw3_optcfg_get(&c, "widescreen"), "1") == 0);
    CHECK(strcmp(dw3_optcfg_get(&c, "fast_forward"), "3") == 0);
    CHECK(strcmp(dw3_optcfg_get(&c, "future_key"), "hello world") == 0);
    CHECK(dw3_optcfg_get(&c, "end") == NULL);
    CHECK(dw3_optcfg_get(&c, "version") == NULL);
    CHECK(dw3_optcfg_get(&c, "missing") == NULL);

    /* Empty and no-version text. */
    dw3_optcfg_parse(&c, "", 0);
    CHECK(c.count == 0 && c.version == DW3_OPTCFG_VERSION && !c.complete);
    dw3_optcfg_parse(&c, "a=1\n", 4);
    CHECK(c.version == DW3_OPTCFG_VERSION && c.count == 1 && !c.complete);

    /* Bad / newer versions. */
    dw3_optcfg_parse(&c, "version=abc\nx=1\n", 16);
    CHECK(c.version == DW3_OPTCFG_VERSION && c.skipped == 1);
    dw3_optcfg_parse(&c, "version=0\n", 10);
    CHECK(c.version == DW3_OPTCFG_VERSION && c.skipped == 1);
    dw3_optcfg_parse(&c, "version=7\nx=1\n", 14);
    CHECK(c.version == 7);

    /* Over-long key and value are skipped, not truncated. */
    {
        char longline[200];

        memset(longline, 'k', sizeof(longline));
        memcpy(longline + 100, "=1\n", 4);
        dw3_optcfg_parse(&c, longline, strlen(longline));
        CHECK(c.count == 0 && c.skipped == 1);
        memset(longline, 'v', sizeof(longline));
        memcpy(longline, "key=", 4);
        longline[150] = '\0';
        dw3_optcfg_parse(&c, longline, strlen(longline));
        CHECK(c.count == 0 && c.skipped == 1);
    }

    /* Binary junk does not crash and yields nothing usable. */
    {
        char junk[256];
        int i;

        for (i = 0; i < 256; i++) {
            junk[i] = (char)(i * 7 + 3);
        }
        dw3_optcfg_parse(&c, junk, sizeof(junk));
        CHECK(c.count <= DW3_OPTCFG_MAX_ENTRIES);
    }

    /* Table full: further keys are skipped, existing ones still replaced. */
    {
        char big[2048];
        size_t len = 0;
        int i;

        for (i = 0; i < DW3_OPTCFG_MAX_ENTRIES + 5; i++) {
            len += (size_t)sprintf(big + len, "k%d=%d\n", i, i);
        }
        dw3_optcfg_parse(&c, big, len);
        CHECK(c.count == DW3_OPTCFG_MAX_ENTRIES && c.skipped == 5);
        CHECK(dw3_optcfg_set(&c, "k0", "9") == 0);
        CHECK(dw3_optcfg_set(&c, "brand_new", "1") == -1);
    }
}

static void test_int(void) {
    int v = 99;

    CHECK(dw3_optcfg_parse_int("0", &v) == 0 && v == 0);
    CHECK(dw3_optcfg_parse_int("12", &v) == 0 && v == 12);
    CHECK(dw3_optcfg_parse_int("-3", &v) == 0 && v == -3);
    v = 5;
    CHECK(dw3_optcfg_parse_int("", &v) == -1 && v == 5);
    CHECK(dw3_optcfg_parse_int("-", &v) == -1);
    CHECK(dw3_optcfg_parse_int("1x", &v) == -1);
    CHECK(dw3_optcfg_parse_int(" 1", &v) == -1);
    CHECK(dw3_optcfg_parse_int("on", &v) == -1);
    CHECK(dw3_optcfg_parse_int("99999999999999", &v) == -1);
    CHECK(dw3_optcfg_parse_int(NULL, &v) == -1);
}

static void test_roundtrip(void) {
    static Dw3OptCfg a, b;
    char buf[DW3_OPTCFG_TEXT_MAX];
    char tiny[8];
    size_t n;

    dw3_optcfg_init(&a);
    CHECK(dw3_optcfg_set(&a, "widescreen", "0") == 0);
    CHECK(dw3_optcfg_set(&a, "fast_forward", "4") == 0);
    CHECK(dw3_optcfg_set(&a, "zz_unknown", "keep me") == 0);
    CHECK(dw3_optcfg_set(&a, "widescreen", "1") == 0); /* replace in place */
    CHECK(dw3_optcfg_set(&a, "bad key", "1") == -1);
    CHECK(dw3_optcfg_set(&a, "k", "a\nb") == -1);
    CHECK(dw3_optcfg_set(&a, "", "1") == -1);
    CHECK(a.count == 3);

    n = dw3_optcfg_serialize(&a, buf, sizeof(buf));
    CHECK(n > 0 && n == strlen(buf));
    CHECK(strstr(buf, "version=1\n") != NULL);
    CHECK(strcmp(buf + n - 6, "end=1\n") == 0);
    CHECK(strstr(buf, "widescreen=1\nfast_forward=4\nzz_unknown=keep me\n")
          != NULL);
    CHECK(dw3_optcfg_serialize(&a, tiny, sizeof(tiny)) == 0);

    dw3_optcfg_parse(&b, buf, n);
    CHECK(b.complete && b.skipped == 0 && b.count == 3 && b.version == 1);
    CHECK(strcmp(dw3_optcfg_get(&b, "zz_unknown"), "keep me") == 0);
    CHECK(strcmp(dw3_optcfg_get(&b, "fast_forward"), "4") == 0);

    /* A newer file version is written back as it was read. */
    b.version = 9;
    n = dw3_optcfg_serialize(&b, buf, sizeof(buf));
    CHECK(strstr(buf, "version=9\n") != NULL);
}

static void test_files(const char *dir) {
    static Dw3OptCfg a, b;
    char path[DW3_OPTCFG_PATH_MAX];

    /* No file. */
    CHECK(dw3_optcfg_load(&b, dir) == DW3_OPTCFG_LOAD_MISSING);
    CHECK(b.count == 0);

    /* Save creates it, and leaves no .tmp behind. */
    dw3_optcfg_init(&a);
    dw3_optcfg_set(&a, "widescreen", "0");
    dw3_optcfg_set(&a, "future", "x");
    CHECK(dw3_optcfg_save(&a, dir) == 0);
    CHECK(file_exists(dir, ""));
    CHECK(!file_exists(dir, ".tmp"));
    CHECK(dw3_optcfg_load(&b, dir) == DW3_OPTCFG_LOAD_OK);
    CHECK(b.complete && strcmp(dw3_optcfg_get(&b, "widescreen"), "0") == 0);
    CHECK(strcmp(dw3_optcfg_get(&b, "future"), "x") == 0);

    /* Saving over an existing file works (the FAT unlink-first path) as
     * many times as needed. */
    dw3_optcfg_set(&a, "widescreen", "1");
    CHECK(dw3_optcfg_save(&a, dir) == 0);
    dw3_optcfg_set(&a, "widescreen", "0");
    CHECK(dw3_optcfg_save(&a, dir) == 0);
    CHECK(dw3_optcfg_load(&b, dir) == DW3_OPTCFG_LOAD_OK);
    CHECK(strcmp(dw3_optcfg_get(&b, "widescreen"), "0") == 0);
    CHECK(!file_exists(dir, ".tmp"));

    /* A stale (even torn) .tmp does not disturb a normal save or load. */
    write_file(dir, ".tmp", "widescreen=1\nversion=1\n");
    CHECK(dw3_optcfg_load(&b, dir) == DW3_OPTCFG_LOAD_OK);
    CHECK(strcmp(dw3_optcfg_get(&b, "widescreen"), "0") == 0);
    CHECK(dw3_optcfg_save(&a, dir) == 0);
    CHECK(!file_exists(dir, ".tmp"));

    /* A save that died between unlink and rename: only a complete .tmp. */
    dw3_optcfg_path(dir, "", path, sizeof(path));
    CHECK(remove(path) == 0);
    write_file(dir, ".tmp",
               "version=1\nwidescreen=1\nfast_forward=2\nend=1\n");
    CHECK(dw3_optcfg_load(&b, dir) == DW3_OPTCFG_LOAD_RECOVERED);
    CHECK(strcmp(dw3_optcfg_get(&b, "fast_forward"), "2") == 0);
    /* ...and a torn one (no end=1) is not trusted. */
    write_file(dir, ".tmp", "version=1\nwidescreen=1\nfast_for");
    CHECK(dw3_optcfg_load(&b, dir) == DW3_OPTCFG_LOAD_MISSING);
    CHECK(b.count == 0);
    remove(path);
    dw3_optcfg_path(dir, ".tmp", path, sizeof(path));
    remove(path);

    /* Oversized file: error, not a half parse. */
    {
        char big[DW3_OPTCFG_TEXT_MAX + 64];

        memset(big, 'a', sizeof(big) - 1);
        big[sizeof(big) - 1] = '\0';
        write_file(dir, "", big);
        CHECK(dw3_optcfg_load(&b, dir) == DW3_OPTCFG_LOAD_ERROR);
        dw3_optcfg_path(dir, "", path, sizeof(path));
        remove(path);
    }

    /* A directory that does not exist: load = missing, save = error with the
     * old state untouched, no crash. */
    {
        char gone[DW3_OPTCFG_PATH_MAX];

        snprintf(gone, sizeof(gone), "%s/does/not/exist", dir);
        CHECK(dw3_optcfg_load(&b, gone) == DW3_OPTCFG_LOAD_MISSING);
        CHECK(dw3_optcfg_save(&a, gone) == -1);
        CHECK(dw3_optcfg_load(&b, NULL) == DW3_OPTCFG_LOAD_ERROR);
        CHECK(dw3_optcfg_save(&a, "") == -1);
    }

    /* Trailing slash in the directory. */
    {
        char slash[DW3_OPTCFG_PATH_MAX];

        snprintf(slash, sizeof(slash), "%s/", dir);
        CHECK(dw3_optcfg_save(&a, slash) == 0);
        CHECK(dw3_optcfg_load(&b, dir) == DW3_OPTCFG_LOAD_OK);
        dw3_optcfg_path(dir, "", path, sizeof(path));
        remove(path);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <scratch dir>\n", argv[0]);
        return 2;
    }
    test_parse();
    test_int();
    test_roundtrip();
    test_files(argv[1]);
    if (!g_fail) {
        printf("OK: options_cfg\n");
    }
    return g_fail;
}
