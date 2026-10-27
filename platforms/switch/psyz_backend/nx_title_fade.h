/* P38: which edge-fade level (0..255) the native title overlay may subtract.
 *
 * The title's edge fade ends at 255 when a menu choice starts a mode, and the
 * overlay keeps the last level it was told. The Options screen opened from the
 * field or a battle (P9.7) is drawn by the same overlay, so it inherited that
 * 255: every texel was darkened to black and the player saw a black square.
 * Replays missed it because they start in the field and never run the title.
 * The edge fade belongs to the title only: an in-game Options screen never
 * takes it, and the level is forgotten once the title is off screen.
 * Pure arithmetic, no GL, so tools/tests/test_title_fade.py runs it on the PC. */
#ifndef DW3_NX_TITLE_FADE_H
#define DW3_NX_TITLE_FADE_H

/* The level to subtract for this frame. */
static inline int dw3_title_fade_level(int fade, int options_in_game) {
    if (options_in_game || fade < 0) {
        return 0;
    }
    return fade > 255 ? 255 : fade;
}

/* The level to keep after this frame: gone when nothing of the title shows. */
static inline int dw3_title_fade_keep(int fade, int title_elements_visible) {
    return title_elements_visible ? fade : 0;
}

#endif
