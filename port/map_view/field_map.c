/* P6.32 phase 6 (patch 0148): what the field hotkey takes from the game when it opens the
 * full-screen map over the walking field, and gives back when the view closes. Policy, logs and
 * the key live in map_view.c; the patch (src/field/field_mode/field.c) pauses the field's tasks
 * and creates the Map screen's root.
 *
 * The field is held by the engine's own task pause (Task.paused: a paused task in TASK_RUN
 * skips its update and its children), so no actor, event, encounter step, tile stream or
 * script moves. What still runs while it is held is what the engine runs outside the tasks:
 *   - the vsync callback adds to the game's clocks (GFX.timeCounter, GAME.playTime) and
 *     drawFrame advances the frame counters;
 *   - the main loop takes a roll (RANDOM.next) every iteration;
 *   - the Map screen's overlay loads its textures over the field's VRAM (0x280..0x3FF).
 * All three are put back at the close, as patch 0165 does for the battle's clock, so the
 * field resumes with the clocks, the roll sequence and every texture it had. */
#include "field_map.h"

#include "engine/game.h"

#include <libgpu.h>

/* engine/game.h declares the game's libc; the host's headers clash with it (as in battle_options.c) */
void *malloc(unsigned long size);
void free(void *ptr);

enum {
    FM_LAYER = 0x1002,          /* FIELD_LAYER_MAP: the map, its objects and characters, and the menus */
    FM_AX = 320, FM_AW = 704,   /* VRAM kept: everything right of the two 320 x 240 frame buffers... */
    FM_BY = 480, FM_BH = 32     /* ...and the strip under them */
};

static struct {
    int have;
    s32 frame_count, frame_counter, time, time_counter, frame_time, frame_time_counter;
    PlayTime play;
    s32 rand_index;
    int layer_ok;
    s32 scroll_x, scroll_y;
    u_char *a, *b;
} g_snap;

static void *grab(RECT *rect) {
    void *buf = malloc((size_t)rect->w * rect->h * 2);

    if (buf) {
        DrawSync(0);
        StoreImage(rect, (u_long *)buf);
        DrawSync(0);
    }
    return buf;
}

int dw3_field_map_snapshot(void) {
    RECT ra = {FM_AX, 0, FM_AW, 512};
    RECT rb = {0, FM_BY, FM_AX, FM_BH};
    Layer *layer;

    if (g_snap.have) return 1;
    g_snap.a = grab(&ra);
    g_snap.b = grab(&rb);
    if (!g_snap.a || !g_snap.b) {
        free(g_snap.a);
        free(g_snap.b);
        g_snap.a = g_snap.b = NULL;
        return 0;
    }
    g_snap.frame_count = GFX.frameCount;
    g_snap.frame_counter = GFX.frameCounter;
    g_snap.time = GFX.time;
    g_snap.time_counter = GFX.timeCounter;
    g_snap.frame_time = GFX.frameTime;
    g_snap.frame_time_counter = GFX.frameTimeCounter;
    g_snap.play = GAME.playTime;
    g_snap.rand_index = RANDOM.index;
    layer = GFX.funcs.getLayer(FM_LAYER);
    g_snap.layer_ok = layer != NULL;
    if (layer) {
        /* the Map screen's sprites follow the layer's scroll by default; the picture it
         * draws is laid out for a layer that does not scroll (SCREEN_LAYER) */
        g_snap.scroll_x = layer->scrollX;
        g_snap.scroll_y = layer->scrollY;
        layer->scrollX = 0;
        layer->scrollY = 0;
    }
    g_snap.have = 1;
    return 1;
}

void dw3_field_map_restore(Dw3FieldMapGiven *given) {
    RECT ra = {FM_AX, 0, FM_AW, 512};
    RECT rb = {0, FM_BY, FM_AX, FM_BH};
    Layer *layer;

    given->time_ran = given->frames_ran = given->rolls = given->vram_bytes = 0;
    if (!g_snap.have) return;
    given->time_ran = GFX.timeCounter - g_snap.time_counter;
    given->frames_ran = GFX.frameCount - g_snap.frame_count;
    given->rolls = (RANDOM.index - g_snap.rand_index) & (RANDOM_COUNT - 1);
    GFX.frameCount = g_snap.frame_count;
    GFX.frameCounter = g_snap.frame_counter;
    GFX.time = g_snap.time;
    GFX.timeCounter = g_snap.time_counter;
    GFX.frameTime = g_snap.frame_time;
    GFX.frameTimeCounter = g_snap.frame_time_counter;
    GAME.playTime = g_snap.play;
    RANDOM.index = g_snap.rand_index;
    layer = GFX.funcs.getLayer(FM_LAYER);
    if (layer && g_snap.layer_ok) {
        layer->scrollX = g_snap.scroll_x;
        layer->scrollY = g_snap.scroll_y;
    }
    DrawSync(0);
    LoadImage(&ra, (u_long *)g_snap.a);
    LoadImage(&rb, (u_long *)g_snap.b);
    DrawSync(0);
    given->vram_bytes = (int)((size_t)ra.w * ra.h * 2 + (size_t)rb.w * rb.h * 2);
    free(g_snap.a);
    free(g_snap.b);
    g_snap.a = g_snap.b = NULL;
    g_snap.have = 0;
}

void dw3_field_map_clocks(Dw3FieldMapClocks *c) {
    c->frame = GFX.frameCount;
    c->time = GFX.time;
    c->play_frames = GAME.playTime.frames;
    c->play_h = GAME.playTime.hours;
    c->play_m = GAME.playTime.minutes;
    c->play_s = GAME.playTime.seconds;
    c->rand_index = RANDOM.index;
    c->progress = GAME.progress;
    c->battle_steps = GAME.battleSteps;
}
