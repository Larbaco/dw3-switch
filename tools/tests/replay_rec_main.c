/* Host test for port/replay/dw3_replay.c (P6.15 recording parser). */
#include "dw3_replay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
static char g_shots[256];

#define CHECK(c)                                                              \
    do {                                                                      \
        if (!(c)) {                                                           \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c);              \
            g_fail = 1;                                                       \
        }                                                                     \
    } while (0)

static void on_shot(const char *name, unsigned frame, void *ctx) {
    (void)ctx;
    snprintf(g_shots + strlen(g_shots), sizeof(g_shots) - strlen(g_shots),
             "%s@%u;", name, frame);
}

int main(void) {
    static const char text[] =
        "# comment\n"
        "pad 10 0x0008\r\n"
        "pad 5 0x4000   # later line, earlier frame\n"
        "bogus line\n"
        "shot 12 first\n"
        "shot 11 zeroth\n"
        "pad 15 0x0000\n"
        "end 20";
    Dw3Replay r;
    unsigned mask = 0xFFFF;
    int bad = -1;
    unsigned f;
    int ended = 0;

    CHECK(dw3_replay_parse(&r, text, sizeof(text) - 1, &bad) == 0);
    CHECK(bad == 1);
    CHECK(r.npad == 3 && r.nshot == 2 && r.end == 20);
    for (f = 1; f <= 25 && !ended; f++) {
        ended = dw3_replay_advance(&r, f, on_shot, NULL, &mask);
        if (f == 4) {
            CHECK(mask == 0);
        }
        if (f == 5) {
            CHECK(mask == 0x4000);
        }
        if (f == 10) {
            CHECK(mask == 0x0008);
        }
        if (f == 15) {
            CHECK(mask == 0);
        }
    }
    CHECK(ended && f == 21);
    CHECK(strcmp(g_shots, "zeroth@11;first@12;") == 0);
    dw3_replay_free(&r);

    /* A big recording is not capped (the old fixed 4096 limit is gone). */
    {
        char *big = (char *)malloc(1 << 20);
        size_t n = 0;
        unsigned i;

        for (i = 0; i < 20000; i++) {
            n += (size_t)sprintf(big + n, "pad %u 0x%04x\n", i + 1, i & 0xFF);
        }
        CHECK(dw3_replay_parse(&r, big, n, &bad) == 0 && r.npad == 20000 &&
              bad == 0);
        dw3_replay_free(&r);
        free(big);
    }

    /* Empty and missing input. */
    CHECK(dw3_replay_parse(&r, "", 0, &bad) == 0 && r.npad == 0 && r.end == 0);
    dw3_replay_free(&r);
    CHECK(dw3_replay_load(&r, "/nonexistent/dw3-replay.rec", NULL) == -1);

    if (g_fail) {
        return 1;
    }
    puts("OK: replay parser");
    return 0;
}
