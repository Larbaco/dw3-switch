#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "title_overlay_art.h"

static double now_ms(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &t)) exit(2);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

int main(int argc, char **argv) {
    if (argc != 3 || (strcmp(argv[1], "0") && strcmp(argv[1], "1"))) return 2;
    char *end;
    errno = 0;
    long count = strtol(argv[2], &end, 10);
    if (errno || *end || count < 1 || count > 1000000) return 2;
    int five = argv[1][0] == '1';
    size_t bytes = 960 * 720 * 4, total = bytes * (five ? 5 : 1);
    uint8_t *pixels = malloc(total);
    if (!pixels) return 2;
    for (long i = 0; i < count; ++i) {
        int values[3] = {i % 2, 2 + i % 3, i % 2};
        dw3_pc_title_overlay_set_options_view(i % 3, values, 3);
        double start = now_ms();
        const int states[] = {-1, 0, 1, 2, DW3_TITLE_ART_OPTIONS};
        for (int state = 0; state < (five ? 5 : 1); ++state) {
            uint8_t *dst = pixels + state * bytes;
            /* Match the host's extra clear, in addition to the art clear. */
            memset(dst, 0, bytes);
            dw3_pc_title_overlay_art(dst, 960 * 4, five ? states[state] : DW3_TITLE_ART_OPTIONS);
        }
        printf("%ld,%.6f\n", i, now_ms() - start);
    }
    unsigned long long checksum = 0;
    for (size_t i = 0; i < total; ++i) checksum += pixels[i];
    fprintf(stderr, "bytes=%zu checksum=%llu\n", total, checksum);
    free(pixels);
    return 0;
}
