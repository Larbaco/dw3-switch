/* P6.24 slice 2: the pure helpers of the Switch launcher
 * (port/region/dw3_launcher.c): NRO lookup and every screen's text.
 * Usage: launcher_test <scratch dir> (must exist and be empty). */
#include "dw3_launcher.h"

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

static void touch(const char *dir, const char *name) {
    char path[512];
    FILE *fp;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        g_fail = 1;
        return;
    }
    fputs("x", fp);
    fclose(fp);
}

int main(int argc, char **argv) {
    const char *dir;
    char path[512], want[512], text[2048];
    Dw3Picker p;
    size_t n;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <scratch dir>\n", argv[0]);
        return 2;
    }
    dir = argv[1];

    /* NRO lookup: none -> documented name, 0. */
    CHECK(dw3_launcher_nro_path(dir, DW3_REGION_US, path, sizeof(path)) == 0);
    snprintf(want, sizeof(want), "%s/dw3-us.nro", dir);
    CHECK(strcmp(path, want) == 0);
    CHECK(dw3_launcher_nro_path(dir, DW3_REGION_NONE, path, sizeof(path)) == 0);
    CHECK(path[0] == '\0');

    /* Release-style name is the second candidate. */
    touch(dir, "dw3-switch-eu.nro");
    CHECK(dw3_launcher_nro_path(dir, DW3_REGION_EU, path, sizeof(path)) == 1);
    snprintf(want, sizeof(want), "%s/dw3-switch-eu.nro", dir);
    CHECK(strcmp(path, want) == 0);
    CHECK(dw3_launcher_nro_path(dir, DW3_REGION_US, path, sizeof(path)) == 0);

    /* The documented name wins when both exist. */
    touch(dir, "dw3-eu.nro");
    CHECK(dw3_launcher_nro_path(dir, DW3_REGION_EU, path, sizeof(path)) == 1);
    snprintf(want, sizeof(want), "%s/dw3-eu.nro", dir);
    CHECK(strcmp(path, want) == 0);
    /* The region trees are not mixed up. */
    touch(dir, "dw3-us.nro");
    CHECK(dw3_launcher_nro_path(dir, DW3_REGION_US, path, sizeof(path)) == 1);
    snprintf(want, sizeof(want), "%s/dw3-us.nro", dir);
    CHECK(strcmp(path, want) == 0);
    /* A tiny buffer truncates but stays terminated. */
    CHECK(dw3_launcher_nro_path(dir, DW3_REGION_US, path, 4) == 0 || path[3] == '\0');

    /* Picker text: marks the selection, shows the countdown, then A. */
    dw3_picker_init(&p, DW3_REGION_EU);
    n = dw3_launcher_text(DW3_SCREEN_PICKER, &p, NULL, DW3_REGION_NONE, NULL, 0,
                          text, sizeof(text));
    CHECK(n == strlen(text) && n > 0);
    CHECK(strstr(text, "> EU <") != NULL);
    CHECK(strstr(text, "> US <") == NULL);
    CHECK(strstr(text, "Starting EU in 3...") != NULL);
    CHECK(strstr(text, "off") != NULL);
    CHECK(strstr(text, "Minus") != NULL);
    dw3_picker_step(&p, DW3_PK_Y, 10);
    dw3_launcher_text(DW3_SCREEN_PICKER, &p, NULL, DW3_REGION_NONE, NULL, 0,
                      text, sizeof(text));
    CHECK(strstr(text, "ON") != NULL);
    CHECK(strstr(text, "Starting") == NULL);
    CHECK(strstr(text, "Press A to start EU.") != NULL);
    dw3_picker_step(&p, DW3_PK_LEFT, 10);
    dw3_launcher_text(DW3_SCREEN_PICKER, &p, NULL, DW3_REGION_NONE, NULL, 0,
                      text, sizeof(text));
    CHECK(strstr(text, "> US <") != NULL);
    /* A picker screen without a picker is empty, not a crash. */
    CHECK(dw3_launcher_text(DW3_SCREEN_PICKER, NULL, NULL, DW3_REGION_NONE, NULL, 0,
                            text, sizeof(text)) == 0);

    /* Message screens name what the player needs. */
    dw3_launcher_text(DW3_SCREEN_NO_DATA, NULL, "sdmc:/switch/dw3", DW3_REGION_NONE,
                      NULL, 0, text, sizeof(text));
    CHECK(strstr(text, "sdmc:/switch/dw3/us") != NULL);
    CHECK(strstr(text, "sdmc:/switch/dw3/eu") != NULL);
    CHECK(strstr(text, "+  quit") != NULL);
    dw3_launcher_text(DW3_SCREEN_NO_NRO, NULL, NULL, DW3_REGION_EU,
                      "sdmc:/switch/dw3/dw3-eu.nro", 0, text, sizeof(text));
    CHECK(strstr(text, "sdmc:/switch/dw3/dw3-eu.nro") != NULL);
    CHECK(strstr(text, "EU") != NULL);
    dw3_launcher_text(DW3_SCREEN_NO_LOADER, NULL, NULL, DW3_REGION_US,
                      "sdmc:/switch/dw3/dw3-us.nro", 0, text, sizeof(text));
    CHECK(strstr(text, "cannot start another program") != NULL);
    CHECK(strstr(text, "+  quit") != NULL);
    dw3_launcher_text(DW3_SCREEN_LOAD_FAILED, NULL, NULL, DW3_REGION_US,
                      "sdmc:/x.nro", 0x1234u, text, sizeof(text));
    CHECK(strstr(text, "0x1234") != NULL);

    /* Always terminated, even into a tiny buffer. */
    n = dw3_launcher_text(DW3_SCREEN_NO_DATA, NULL, NULL, DW3_REGION_NONE, NULL, 0,
                          text, 16);
    CHECK(n == strlen(text) && n == 15);

    if (!g_fail) {
        puts("OK: launcher");
    }
    return g_fail;
}
