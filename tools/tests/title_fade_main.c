/* P38: the title edge fade must never darken the in-game Options screen. */
#include "nx_title_fade.h"

#include <stdio.h>

int main(void) {
    int ok = 1;

    /* The title keeps its fade (0..255, clamped). */
    printf("title_full=%d\n", dw3_title_fade_level(255, 0) == 255);
    printf("title_mid=%d\n", dw3_title_fade_level(100, 0) == 100);
    printf("clamp=%d\n", dw3_title_fade_level(300, 0) == 255 &&
                         dw3_title_fade_level(-5, 0) == 0);
    /* The field's Options screen after the title ended at 255: no darkening. */
    printf("ingame_after_title=%d\n", dw3_title_fade_level(255, 1) == 0);
    /* Title off screen forgets the level, on screen keeps it. */
    printf("forgotten=%d\n", dw3_title_fade_keep(255, 0) == 0);
    printf("kept=%d\n", dw3_title_fade_keep(255, 1) == 255);
    return ok ? 0 : 1;
}
