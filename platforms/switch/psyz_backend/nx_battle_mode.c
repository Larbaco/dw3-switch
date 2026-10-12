/* Runtime battle render mode cycle for OLED A/B (FIGHTSTG only). */
#include "nx_battle_mode.h"

#include "nx.h"
#include "nx_mesh.h"

#include <stdio.h>
#include <stdlib.h>
#include <switch.h>

#ifndef DW3_NX_MESH_SOFT_BATTLE
#define DW3_NX_MESH_SOFT_BATTLE 1
#endif

enum {
    MODE_CHORD = HidNpadButton_Minus | HidNpadButton_ZL | HidNpadButton_ZR,
    MODE_COUNT = 4
};

static Dw3NxBattleMode s_mode = DW3_NX_BATTLE_SOFT;
static int s_inited;
static int s_chord_was_down;
static int s_bbox_stub_logged;

static const char *mode_token(Dw3NxBattleMode mode) {
    switch (mode) {
    case DW3_NX_BATTLE_SOFT:
        return "soft";
    case DW3_NX_BATTLE_SOFT_BBOX:
        return "soft+bbox";
    case DW3_NX_BATTLE_GPU_SOLID:
        return "gpu-solid";
    case DW3_NX_BATTLE_GPU_TEX:
        return "gpu-tex";
    default:
        return "soft";
    }
}

static void log_mode(Dw3NxBattleMode mode) {
    fprintf(stderr, "nx_mesh: battle_mode=%s\n", mode_token(mode));
    if (mode == DW3_NX_BATTLE_SOFT_BBOX && !s_bbox_stub_logged) {
        s_bbox_stub_logged = 1;
        /* TODO(P5.2 H): real empty-bbox / edge-walk skip in RasterTriangle. */
        fprintf(stderr,
                "nx_mesh: soft+bbox not yet — using soft (see "
                "P5.2_BATTLE_GPU_IDEAS.md idea H)\n");
    }
}

void dw3_nx_battle_mode_init(void) {
    const char *env;

    if (s_inited) {
        return;
    }
    s_inited = 1;
    env = getenv("DW3_MESH_SOFT");
    if (env && env[0] == '0' && env[1] == '\0') {
        s_mode = DW3_NX_BATTLE_GPU_TEX;
    } else if (env && env[0] == '1' && env[1] == '\0') {
        s_mode = DW3_NX_BATTLE_SOFT;
    } else if (DW3_NX_MESH_SOFT_BATTLE) {
        s_mode = DW3_NX_BATTLE_SOFT;
    } else {
        s_mode = DW3_NX_BATTLE_GPU_TEX;
    }
    log_mode(s_mode);
}

void dw3_nx_battle_mode_poll(void) {
    u64 held;
    int down;

    dw3_nx_battle_mode_init();
    held = dw3_nx_input_buttons_raw();
    down = ((held & MODE_CHORD) == MODE_CHORD) ? 1 : 0;
    if (down && !s_chord_was_down) {
        s_mode = (Dw3NxBattleMode)(((int)s_mode + 1) % MODE_COUNT);
        dw3_nx_mesh_discard();
        dw3_nx_mesh_mark_vram_dirty();
        log_mode(s_mode);
    }
    s_chord_was_down = down;
}

Dw3NxBattleMode dw3_nx_battle_mode_get(void) {
    dw3_nx_battle_mode_init();
    return s_mode;
}

const char *dw3_nx_battle_mode_token(void) {
    return mode_token(dw3_nx_battle_mode_get());
}

int dw3_nx_battle_mode_use_soft(void) {
    Dw3NxBattleMode mode = dw3_nx_battle_mode_get();

    return mode == DW3_NX_BATTLE_SOFT || mode == DW3_NX_BATTLE_SOFT_BBOX;
}

int dw3_nx_battle_mode_use_solid(void) {
    return dw3_nx_battle_mode_get() == DW3_NX_BATTLE_GPU_SOLID;
}
