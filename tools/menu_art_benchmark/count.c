#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "title_overlay_art.h"
extern unsigned long long bench_calls, bench_rejected, bench_opaque, bench_alpha;
int main(void) {
    uint8_t *pixels = malloc(960 * 720 * 4);
    if (!pixels) return 2;
    for (int state = -1; state <= 3; ++state) {
        bench_calls = bench_rejected = bench_opaque = bench_alpha = 0;
        dw3_pc_title_overlay_art(pixels, 960 * 4, state);
        printf("state=%d calls=%llu rejected=%llu opaque=%llu translucent=%llu\n",
               state, bench_calls, bench_rejected, bench_opaque, bench_alpha);
    }
    free(pixels);
    return 0;
}
