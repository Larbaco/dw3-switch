#ifndef DW3_TITLE_OPTIONS_CACHE_H
#define DW3_TITLE_OPTIONS_CACHE_H
/* Retained CPU canvas for the native Port options screen, shared by the PC
 * SDL3 GPU overlay and the Switch GLES overlay. No SDL/GL here: the host
 * asks for the dirty rectangles, copies them into its texture, and commits.
 *
 * The cache keeps the static window (dim layer, bezel, title, hints) and a
 * working canvas. A cursor move redraws only the old and new focus rows and
 * the description; a value change redraws only that value cell. The output
 * is byte-identical to dw3_pc_title_overlay_art(DW3_TITLE_ART_OPTIONS). */
#include <stddef.h>
#include <stdint.h>

enum {
    DW3_OPTIONS_CANVAS_W = 960,
    DW3_OPTIONS_CANVAS_H = 720,
    DW3_OPTIONS_CANVAS_PITCH = DW3_OPTIONS_CANVAS_W * 4,
    /* Old row, new row, description, and up to five value cells. */
    DW3_OPTIONS_MAX_DIRTY = 10,
};

typedef struct Dw3OptionsRect {
    int x, y, w, h;
} Dw3OptionsRect;

typedef struct Dw3OptionsDirty {
    Dw3OptionsRect rects[DW3_OPTIONS_MAX_DIRTY];
    int count;
    /* The single rect is the whole canvas (open, language, context, retry). */
    int full;
    /* The static layer was re-rasterized this call (open / language change). */
    int rebuilt;
    size_t bytes;
} Dw3OptionsDirty;

/* Bring the canvas up to date with dw3_pc_title_overlay_set_options_view and
 * report what the host must upload. count == 0 means nothing changed.
 * Returns 0, or -1 when the canvases cannot be allocated (the host then
 * keeps its reference path). A report that was not committed is repeated as
 * a full-canvas upload. */
int dw3_options_cache_update(Dw3OptionsDirty *out);

/* The host uploaded everything reported by the last update. */
void dw3_options_cache_commit(void);

/* The host texture was lost or replaced (resize, context loss, atlas
 * re-upload): the next update reports the whole canvas again. */
void dw3_options_cache_invalidate(void);

/* Free both canvases (teardown, restart, leaving the title). */
void dw3_options_cache_release(void);

/* Current working canvas (960x720 RGBA, pitch DW3_OPTIONS_CANVAS_PITCH), or
 * NULL before the first successful update / after release. */
const uint8_t *dw3_options_cache_pixels(void);

/* Copy one rect out of the canvas into a contiguous w*h*4 buffer (GLES2 has
 * no GL_UNPACK_ROW_LENGTH; SDL transfer buffers are also tightly packed). */
void dw3_options_cache_pack_rect(Dw3OptionsRect rect, uint8_t *dst);
#endif
