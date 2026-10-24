/* P9.2 port options shell — stays on MODE_TITLE; never sets menu choice. */
#include "port_options.h"
#include "port_audio.h"
#include "port_battle_menu.h"
#include "port_options_cfg.h"
#include "port_pacing.h"
#include "port_turbo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DW3_PORT_OPTIONS_TEST
#define PO_INFO(...) fprintf(stderr, __VA_ARGS__)
#define PO_WARN(...) fprintf(stderr, __VA_ARGS__)
#else
#include <psyz/log.h>
/* psyz_log, not stderr: on Switch stderr only reaches nxlink, and the load
 * and save lines are the check in sdmc:/switch/dw3/dw3.log. */
#define PO_INFO(...) INFOF(__VA_ARGS__)
#define PO_WARN(...) WARNF(__VA_ARGS__)
#endif

#if !defined(DW3_PORT_OPTIONS_TEST) && defined(DW3_TITLE_NATIVE_OVERLAY)
/* The native overlay draws the options screen (platforms/pc/src). */
#include "title_overlay_art.h"
#endif

#ifndef DW3_PORT_OPTIONS_TEST
#include "port_text.h"
#include <libgpu.h>
#include "engine/pad.h"
#include "engine/sound.h"
/* Psyz_VideoAlloc* capture is PC/PSY-Z only — compile out on Switch. */
#if defined(DW3_PC_HOST)
#include <psyz/video.h>
#include <stdlib.h>
#endif
#endif

/* DW3_PORT_OPT_ENCOUNTER_RATE (5), DW3_PORT_OPT_TEXT_SPEED (6) and
 * DW3_PORT_OPT_BATTLE_MENU (7) are in port_options.h: port/pacing/pacing.c and
 * port/battle_menu/battle_menu_memory.c read them. */
enum {
    DW3_PORT_OPT_COUNT = 8,
    DW3_PORT_OPT_WIDESCREEN = 0,
    DW3_PORT_OPT_FAST_FORWARD = 1,
    DW3_PORT_OPT_RESTART = 2,
    DW3_PORT_OPT_MUSIC_VOLUME = 3,
    DW3_PORT_OPT_SFX_VOLUME = 4
};

typedef struct Dw3PortOptStub {
    const char *label;
    const char *key; /* P9.4 option file key */
    int value; /* 0/1, 0/2..4 (Fast-forward), 0..10 (volumes), 0..4 (Encounter
                * rate) or 0..2 (Text speed, Battle cursor); Widescreen
                * defaults ON, turbo starts inactive, volumes default to 10,
                * pacing and Battle cursor to 0 (1x, Normal, Off) */
    int def;   /* value with no option file (the row's initial value) */
    const char *env; /* optional DW3_* variable that pins it for one run */
} Dw3PortOptStub;

static int g_open;
static int g_cursor;
static int g_open_frames;
static int g_logged_open;
static Dw3PortOptStub g_stubs[DW3_PORT_OPT_COUNT] = {
    {"Widescreen", "widescreen", 1, 1, NULL}, /* owner request: on */
    /* R3 toggles to this speed in field/battle; normal boot remains 1x. */
    {"Fast-forward", "fast_forward", DW3_TURBO_SPEED_DEFAULT,
     DW3_TURBO_SPEED_DEFAULT, NULL},
    {"Restart hotkey", "restart_hotkey", 0, 0, NULL},
    /* P6.17: 0..10, 10 = the original mix (docs/P6.17_VOLUME.md) */
    {"Music volume", "music_volume", DW3_AUDIO_VOLUME_MAX,
     DW3_AUDIO_VOLUME_MAX, "DW3_MUSIC_VOLUME"},
    {"SFX volume", "sfx_volume", DW3_AUDIO_VOLUME_MAX, DW3_AUDIO_VOLUME_MAX,
     "DW3_SFX_VOLUME"},
    /* P6.18: 0 = 1x, 1 = 0.5x, 2 = Off, 3 = 1.5x, 4 = 2x (stored values;
     * the screen steps OFF, 0.5x, 1x, 1.5x, 2x, docs/P6.18_PACING.md) */
    {"Encounter rate", "encounter_rate", DW3_ENCOUNTER_1X, DW3_ENCOUNTER_1X,
     "DW3_ENCOUNTER_RATE"},
    /* P6.18: 0 = Normal, 1 = Fast, 2 = Instant */
    {"Text speed", "text_speed", DW3_TEXT_NORMAL, DW3_TEXT_NORMAL,
     "DW3_TEXT_SPEED"},
    /* P7.9: 0 = Off (the original menu), 1 = Command, 2 = Command+Tech. The
     * key is the one that was file-only before the row existed. */
    {"Battle cursor", DW3_BATTLE_MENU_MEMORY_KEY, DW3_BATTLE_MENU_MEMORY_OFF,
     DW3_BATTLE_MENU_MEMORY_OFF, NULL},
};

/* P9.4 persistence. The registry values above are what the game reads; this
 * is the option file behind them. dw3_port_options_persist_init() fills the
 * values from the file once at boot (before any host reads a flag), and every
 * change made on the Options screen is written back at once, so a HOME exit,
 * a kill or a crash cannot lose it and no applet hook is needed. */
static int g_persist_on;
static char g_cfg_dir[DW3_OPTCFG_PATH_MAX];
static Dw3OptCfg g_cfg;          /* the loaded file; keeps unknown keys */
static int g_saved[DW3_PORT_OPT_COUNT]; /* what the file holds */
static unsigned g_env_pinned;    /* values forced by DW3_WIDESCREEN/DW3_TURBO */
static int g_dirty;              /* a write failed; retried by _flush */

#ifdef DW3_PORT_OPTIONS_TEST
/* Button bits match engine/pad.h. */
enum {
    PO_BIT_SELECT = 0,
    PO_BIT_UP = 4,
    PO_BIT_RIGHT = 5,
    PO_BIT_DOWN = 6,
    PO_BIT_LEFT = 7,
    PO_BIT_CIRCLE = 12,
    PO_BIT_CROSS = 13,
    PO_BIT_TRIANGLE = 14
};
static unsigned g_test_pressed;
void dw3_port_options_test_set_pressed(unsigned mask) {
    g_test_pressed = mask;
}
static int po_pressed(int bit) {
    return (int)((g_test_pressed >> bit) & 1u);
}
#else
static int po_pressed(int bit) {
    return PAD_PRESSED(bit);
}
#endif

/* Hand the cursor and values to the native overlay, which regenerates its
 * options screen only when they change. */
static void po_push_view(void) {
#if !defined(DW3_PORT_OPTIONS_TEST) && defined(DW3_TITLE_NATIVE_OVERLAY)
    int values[DW3_PORT_OPT_COUNT];
    int i;

    for (i = 0; i < DW3_PORT_OPT_COUNT; i++) {
        values[i] = g_stubs[i].value;
    }
    dw3_pc_title_overlay_set_options_view(g_cursor, values,
                                          DW3_PORT_OPT_COUNT);
#endif
}

void dw3_port_options_open(void) {
    if (g_open) {
        return;
    }
    g_open = 1;
    g_cursor = 0;
    g_open_frames = 0;
    g_logged_open = 0;
    po_push_view();
    fprintf(stderr, "dw3-port-options: open (stay MODE_TITLE)\n");
    fflush(stderr);
}

void dw3_port_options_close(void) {
    if (!g_open) {
        return;
    }
    g_open = 0;
    g_open_frames = 0;
    fprintf(stderr, "dw3-port-options: close (resume runOptions)\n");
    fflush(stderr);
    dw3_port_options_flush();
}

int dw3_port_options_is_open(void) {
    return g_open != 0;
}

int dw3_port_options_count(void) {
    return DW3_PORT_OPT_COUNT;
}

const char *dw3_port_options_label(int index) {
    if (index < 0 || index >= DW3_PORT_OPT_COUNT) {
        return NULL;
    }
    return g_stubs[index].label;
}

int dw3_port_options_value(int index) {
    if (index < 0 || index >= DW3_PORT_OPT_COUNT) {
        return 0;
    }
    return g_stubs[index].value != 0;
}

void dw3_port_options_set(int index, int value) {
    if (index < 0 || index >= DW3_PORT_OPT_COUNT) {
        return;
    }
    g_stubs[index].value = value != 0;
}

static int po_is_volume(int index);
static void po_audio_apply(void);

int dw3_port_options_level(int index) {
    if (index < 0 || index >= DW3_PORT_OPT_COUNT) {
        return 0;
    }
    return g_stubs[index].value;
}

void dw3_port_options_set_level(int index, int level) {
    if (index < 0 || index >= DW3_PORT_OPT_COUNT) {
        return;
    }
    g_stubs[index].value = level;
    if (po_is_volume(index)) {
        po_audio_apply();
    }
}


static int po_is_volume(int index) {
    return index == DW3_PORT_OPT_MUSIC_VOLUME ||
           index == DW3_PORT_OPT_SFX_VOLUME;
}

/* The P6.18 rows and the Battle cursor hold a mode, 0 = the original
 * behaviour. */
static int po_is_mode(int index) {
    return index == DW3_PORT_OPT_ENCOUNTER_RATE ||
           index == DW3_PORT_OPT_TEXT_SPEED ||
           index == DW3_PORT_OPT_BATTLE_MENU;
}

/* Rows that Left / Right step and Confirm cycles: the volumes and the modes. */
static int po_is_stepped(int index) {
    return po_is_volume(index) || po_is_mode(index);
}

/* Highest stored value of a stepped row. */
static int po_value_max(int index) {
    return po_is_volume(index) ? DW3_AUDIO_VOLUME_MAX
           : index == DW3_PORT_OPT_ENCOUNTER_RATE ? DW3_ENCOUNTER_MODE_MAX
           : index == DW3_PORT_OPT_BATTLE_MENU ? DW3_BATTLE_MENU_MEMORY_TECH
                                                   : DW3_PACING_MODE_MAX;
}

/* The order Left / Right and Confirm walk a stepped row in, by position. The
 * stored values never change (a file written by an older build keeps its
 * meaning), so Encounter rate, stored 0 1x / 1 0.5x / 2 Off / 3 1.5x / 2x 4,
 * is stepped OFF, 0.5x, 1x, 1.5x, 2x. Every other row steps in value order. */
static const int po_encounter_order[DW3_ENCOUNTER_MODE_MAX + 1] = {
    DW3_ENCOUNTER_OFF, DW3_ENCOUNTER_HALF, DW3_ENCOUNTER_1X,
    DW3_ENCOUNTER_1_5X, DW3_ENCOUNTER_2X
};

static int po_value_at(int index, int pos) {
    return index == DW3_PORT_OPT_ENCOUNTER_RATE ? po_encounter_order[pos] : pos;
}

static int po_pos_of(int index, int value) {
    int pos;

    if (index != DW3_PORT_OPT_ENCOUNTER_RATE) {
        return value;
    }
    for (pos = 0; pos <= DW3_ENCOUNTER_MODE_MAX; pos++) {
        if (po_encounter_order[pos] == value) {
            return pos;
        }
    }
    return 0;
}

/* Last position of a stepped row. */
static int po_step_max(int index) {
    return po_value_max(index);
}

/* A value the row can hold: ON/OFF, OFF / 2x..4x for Fast-forward, a volume
 * step 0..10, an Encounter rate 0..4, a Text speed 0..2 or a Battle cursor
 * 0..2. */
static int po_valid(int index, int v) {
    if (index == DW3_PORT_OPT_FAST_FORWARD) {
        return v == 0 || (v >= DW3_TURBO_SPEED_MIN && v <= DW3_TURBO_SPEED_MAX);
    }
    if (po_is_stepped(index)) {
        return v >= 0 && v <= po_value_max(index);
    }
    return v == 0 || v == 1;
}

#ifdef DW3_PORT_OPTIONS_TEST
/* Host tests link no mixer; a test that wants the values defines its own. */
__attribute__((weak)) void dw3_port_audio_apply(int music_step, int sfx_step) {
    (void)music_step;
    (void)sfx_step;
}
#endif

/* Hand the two volume steps to the mixer (port/audio/audio_bus.c). */
static void po_audio_apply(void) {
    dw3_port_audio_apply(g_stubs[DW3_PORT_OPT_MUSIC_VOLUME].value,
                         g_stubs[DW3_PORT_OPT_SFX_VOLUME].value);
}

/* DW3_MUSIC_VOLUME / DW3_SFX_VOLUME (0..10), DW3_ENCOUNTER_RATE (0..4, the
 * stored values) and DW3_TEXT_SPEED (0..2) pin a value for this run: it is used like any other
 * value but never written by an unrelated change (the same rule as
 * DW3_WIDESCREEN and DW3_TURBO). An invalid value is ignored. */
static void po_stepped_env(void) {
    int row;

    for (row = 0; row < DW3_PORT_OPT_COUNT; row++) {
        const char *env;
        int n;

        if (g_stubs[row].env == NULL) {
            continue;
        }
        env = getenv(g_stubs[row].env);
        if (env == NULL || dw3_optcfg_parse_int(env, &n) != 0 ||
            !po_valid(row, n)) {
            continue;
        }
        g_stubs[row].value = n;
        g_env_pinned |= 1u << row;
    }
}

void dw3_port_options_apply_env(int index, int level) {
    if (index < 0 || index >= DW3_PORT_OPT_COUNT) {
        return;
    }
    g_stubs[index].value = level;
    g_env_pinned |= 1u << index;
    if (po_is_volume(index)) {
        po_audio_apply();
    }
}

/* Write every option to the file. An option still forced by the environment
 * keeps the value the file had, so `DW3_WIDESCREEN=0 dw3-pc` for one run is
 * not saved as the user's choice by an unrelated toggle. */
static int po_save(void) {
    int out[DW3_PORT_OPT_COUNT];
    char num[16];
    int i;

    if (!g_persist_on) {
        return 0;
    }
    for (i = 0; i < DW3_PORT_OPT_COUNT; i++) {
        out[i] = (g_env_pinned >> i) & 1u ? g_saved[i] : g_stubs[i].value;
        snprintf(num, sizeof(num), "%d", out[i]);
        if (dw3_optcfg_set(&g_cfg, g_stubs[i].key, num) != 0) {
            PO_WARN("dw3-options: cannot stage %s", g_stubs[i].key);
        }
    }
    if (dw3_optcfg_save(&g_cfg, g_cfg_dir) != 0) {
        g_dirty = 1;
        PO_WARN("dw3-options: save to %s failed (will retry on close/exit)",
                g_cfg_dir);
        return -1;
    }
    g_dirty = 0;
    for (i = 0; i < DW3_PORT_OPT_COUNT; i++) {
        g_saved[i] = out[i];
    }
    PO_INFO("dw3-options: saved %s/%s (widescreen=%d fast_forward=%d "
            "restart_hotkey=%d music_volume=%d sfx_volume=%d "
            "encounter_rate=%d text_speed=%d battle_menu_memory=%d)",
            g_cfg_dir, DW3_OPTCFG_FILE, g_saved[DW3_PORT_OPT_WIDESCREEN],
            g_saved[DW3_PORT_OPT_FAST_FORWARD], g_saved[DW3_PORT_OPT_RESTART],
            g_saved[DW3_PORT_OPT_MUSIC_VOLUME],
            g_saved[DW3_PORT_OPT_SFX_VOLUME],
            g_saved[DW3_PORT_OPT_ENCOUNTER_RATE],
            g_saved[DW3_PORT_OPT_TEXT_SPEED],
            g_saved[DW3_PORT_OPT_BATTLE_MENU]);
    return 0;
}

int dw3_port_options_file_int(const char *key, int def) {
    const char *v = g_persist_on ? dw3_optcfg_get(&g_cfg, key) : NULL;
    int n;

    if (v == NULL || dw3_optcfg_parse_int(v, &n) != 0) {
        return def;
    }
    return n;
}

void dw3_port_options_flush(void) {
    if (g_persist_on && g_dirty) {
        (void)po_save();
    }
}

int dw3_port_options_persist_init(const char *dir) {
    char path[DW3_OPTCFG_PATH_MAX];
    int rc;
    int i;
    int bad = 0;

    g_persist_on = 0;
    g_dirty = 0;
    g_env_pinned = 0;
    if (dir == NULL || dir[0] == '\0' ||
        strlen(dir) >= sizeof(g_cfg_dir) ||
        dw3_optcfg_path(dir, "", path, sizeof(path)) != 0) {
        PO_WARN("dw3-options: no usable config dir; options are session-only");
        po_stepped_env();
        po_audio_apply();
        return -1;
    }
    strcpy(g_cfg_dir, dir);
    rc = dw3_optcfg_load(&g_cfg, dir);
    g_persist_on = 1;
    for (i = 0; i < DW3_PORT_OPT_COUNT; i++) {
        g_stubs[i].value = g_stubs[i].def;
    }
    if (rc == DW3_OPTCFG_LOAD_ERROR) {
        PO_WARN("dw3-options: cannot read %s; using defaults, the next "
                "change will replace it", path);
    } else if (rc == DW3_OPTCFG_LOAD_MISSING) {
        PO_INFO("dw3-options: no %s; defaults", path);
    } else {
        const char *v;
        int n;

        if (rc == DW3_OPTCFG_LOAD_RECOVERED) {
            PO_WARN("dw3-options: %s was missing; recovered %s.tmp", path,
                    path);
        }
        if (g_cfg.version > DW3_OPTCFG_VERSION) {
            PO_WARN("dw3-options: %s is version %d (newer than %d); reading "
                    "known keys, keeping the rest", path, g_cfg.version,
                    DW3_OPTCFG_VERSION);
        }
        if (g_cfg.skipped > 0) {
            PO_WARN("dw3-options: %s: ignored %d malformed line(s)", path,
                    g_cfg.skipped);
        }
        for (i = 0; i < DW3_PORT_OPT_COUNT; i++) {
            v = dw3_optcfg_get(&g_cfg, g_stubs[i].key);
            if (v == NULL) {
                continue;
            }
            if (dw3_optcfg_parse_int(v, &n) != 0 || !po_valid(i, n)) {
                PO_WARN("dw3-options: %s=%s is not valid; using default %d",
                        g_stubs[i].key, v, g_stubs[i].def);
                bad++;
                continue;
            }
            g_stubs[i].value = n;
        }
    }
    for (i = 0; i < DW3_PORT_OPT_COUNT; i++) {
        g_saved[i] = g_stubs[i].value;
    }
    PO_INFO("dw3-options: %s (widescreen=%d fast_forward=%d "
            "restart_hotkey=%d music_volume=%d sfx_volume=%d "
            "encounter_rate=%d text_speed=%d battle_menu_memory=%d)%s", path,
            g_saved[DW3_PORT_OPT_WIDESCREEN],
            g_saved[DW3_PORT_OPT_FAST_FORWARD], g_saved[DW3_PORT_OPT_RESTART],
            g_saved[DW3_PORT_OPT_MUSIC_VOLUME],
            g_saved[DW3_PORT_OPT_SFX_VOLUME],
            g_saved[DW3_PORT_OPT_ENCOUNTER_RATE],
            g_saved[DW3_PORT_OPT_TEXT_SPEED],
            g_saved[DW3_PORT_OPT_BATTLE_MENU],
            bad ? " [some values reset]" : "");
    /* DW3_MUSIC_VOLUME / DW3_SFX_VOLUME force a value for this run only. */
    po_stepped_env();
    po_audio_apply();
    return rc == DW3_OPTCFG_LOAD_ERROR ? -1 : 0;
}

/* The words of a mode row, for the log (the overlay has its own lettering). */
static const char *po_mode_name(int index, int v) {
    static const char *const encounter[DW3_ENCOUNTER_MODE_MAX + 1] = {
        "1x", "0.5x", "off", "1.5x", "2x"};
    static const char *const text[3] = {"normal", "fast", "instant"};
    static const char *const battle[3] = {"off", "command",
                                          "command+technique"};

    if (v < 0 || v > po_value_max(index)) {
        return "?";
    }
    return index == DW3_PORT_OPT_ENCOUNTER_RATE ? encounter[v]
           : index == DW3_PORT_OPT_BATTLE_MENU ? battle[v] : text[v];
}

/* Options screen menu sound feedback (owner request after the preview.30-rc4 OLED test): the game's own title-menu sounds (cursor tick,
 * choice, and the menu cancel of its other menus). All three are key-ons of
 * the system bank (bank 1, SYSTEM00..02), so they go through the SFX bus
 * (P6.17) and follow the SFX volume row. A host test counts them instead. */
enum { PO_SND_CURSOR, PO_SND_SELECT, PO_SND_CANCEL };

#ifdef DW3_PORT_OPTIONS_TEST
static int g_test_sounds[3];
int dw3_port_options_test_sound_count(int kind) {
    return kind >= 0 && kind < 3 ? g_test_sounds[kind] : 0;
}
#endif

static void po_sound(int kind) {
    static const char *const name[3] = {"cursor", "select", "cancel"};
#ifdef DW3_PORT_OPTIONS_TEST
    g_test_sounds[kind]++;
    fprintf(stderr, "dw3-port-options: sound %s (sfx volume %d)\n", name[kind],
            g_stubs[DW3_PORT_OPT_SFX_VOLUME].value);
    fflush(stderr);
#else
    static const s32 id[3] = {SOUND_CURSOR, SOUND_SELECT, SOUND_MENU_CANCEL};
    short voice;

    if (SOUND.playSound == NULL || findSoundBank(SOUND_BANK(id[kind])) < 0) {
        PO_WARN("dw3-port-options: sound %s not played: bank %d is not "
                "loaded", name[kind], (int)SOUND_BANK(id[kind]));
        return;
    }
    voice = SOUND.playSound(id[kind]);
    PO_INFO("dw3-port-options: sound %s (sfx volume %d, voice %d)",
            name[kind], g_stubs[DW3_PORT_OPT_SFX_VOLUME].value, (int)voice);
#endif
}

/* Move a stepped row (a volume 0..10, a pacing mode) by `delta` steps in
 * the row's order (po_value_at), clamped, and save. Plays the choice sound
 * once the new value (and, for a volume, its gain) is in effect. */
static void po_step(int index, int delta) {
    int pos = po_pos_of(index, g_stubs[index].value) + delta;
    int v;

    if (pos < 0) {
        pos = 0;
    }
    if (pos > po_step_max(index)) {
        pos = po_step_max(index);
    }
    v = po_value_at(index, pos);
    if (v == g_stubs[index].value && !((g_env_pinned >> index) & 1u)) {
        return;
    }
    g_stubs[index].value = v;
    g_env_pinned &= ~(1u << index);
    if (po_is_volume(index)) {
        fprintf(stderr, "dw3-port-options: %s %d\n",
                index == DW3_PORT_OPT_MUSIC_VOLUME ? "music volume"
                                                    : "sfx volume", v);
    } else {
        fprintf(stderr, "dw3-port-options: %s %s\n",
                index == DW3_PORT_OPT_ENCOUNTER_RATE ? "encounter rate"
                : index == DW3_PORT_OPT_BATTLE_MENU ? "battle cursor"
                                                    : "text speed",
                po_mode_name(index, v));
    }
    fflush(stderr);
    if (po_is_volume(index)) {
        po_audio_apply();
    }
    (void)po_save();
    po_sound(PO_SND_SELECT);
}

/* Confirm on a row: ON/OFF, OFF -> 2x -> 3x -> 4x -> OFF for Fast-forward, or
 * the next step of a volume / pacing row (wraps to the first after the last:
 * Encounter rate 2x -> OFF). */
static void po_toggle(int index) {
    int v;

    if (index == DW3_PORT_OPT_FAST_FORWARD) {
        v = g_stubs[index].value;
        v = v < DW3_TURBO_SPEED_MIN ? DW3_TURBO_SPEED_MIN : v + 1;
        if (v > DW3_TURBO_SPEED_MAX) {
            v = 0;
        }
        g_stubs[index].value = v;
        g_env_pinned &= ~(1u << index);
        if (v) {
            fprintf(stderr, "dw3-port-options: fast-forward %dx (R3)\n", v);
        } else {
            fprintf(stderr, "dw3-port-options: fast-forward off\n");
        }
        fflush(stderr);
        (void)po_save();
        po_sound(PO_SND_SELECT);
        return;
    }
    if (po_is_stepped(index)) {
        /* Confirm steps up and wraps; Left/Right step either way. */
        po_step(index, po_pos_of(index, g_stubs[index].value) >=
                               po_step_max(index)
                           ? -po_step_max(index) : 1);
        return;
    }
    v = g_stubs[index].value ? 0 : 1;
    g_stubs[index].value = v;
    g_env_pinned &= ~(1u << index);
    fprintf(stderr, "dw3-port-options: %s %s\n",
            index == DW3_PORT_OPT_RESTART ? "restart hotkey" : "widescreen",
            v ? "enabled" : "disabled");
    fflush(stderr);
    (void)po_save();
    po_sound(PO_SND_SELECT);
}

int dw3_port_options_restart_enabled(void) {
    return g_stubs[DW3_PORT_OPT_RESTART].value != 0;
}

void dw3_port_options_tick(void) {
    if (!g_open) {
        return;
    }
    g_open_frames++;
    if (!g_logged_open && g_open_frames == 1) {
        g_logged_open = 1;
        fprintf(stderr, "dw3-port-options: shell tick (Select opened)\n");
        fflush(stderr);
    }
#ifdef DW3_CHEATS
    /* P6.20: the Cheats screen has the pad while it is open; Square (Y on
     * the Switch) opens it. */
    if (dw3_cheats_is_open()) {
        dw3_cheats_tick();
        if (!dw3_cheats_is_open()) {
            po_push_view(); /* back on the Options page */
        }
        return;
    }
#ifdef DW3_PORT_OPTIONS_TEST
    if (po_pressed(15)) { /* PAD_SQUARE */
#else
    if (po_pressed(PAD_SQUARE)) {
#endif
        po_sound(PO_SND_SELECT);
        dw3_cheats_open();
        return;
    }
#endif

#ifdef DW3_PORT_OPTIONS_TEST
    /* Close is Triangle only — Select is the open chord on the title menu. */
    if (po_pressed(PO_BIT_TRIANGLE)) {
        po_sound(PO_SND_CANCEL);
        dw3_port_options_close();
        return;
    }
    if (po_pressed(PO_BIT_UP)) {
        if (g_cursor > 0) {
            g_cursor--;
            po_sound(PO_SND_CURSOR);
        }
    }
    if (po_pressed(PO_BIT_DOWN)) {
        if (g_cursor < DW3_PORT_OPT_COUNT - 1) {
            g_cursor++;
            po_sound(PO_SND_CURSOR);
        }
    }
    if (po_is_stepped(g_cursor)) {
        if (po_pressed(PO_BIT_LEFT)) {
            po_step(g_cursor, -1);
        }
        if (po_pressed(PO_BIT_RIGHT)) {
            po_step(g_cursor, 1);
        }
    }
    if (po_pressed(PO_BIT_CROSS) || po_pressed(PO_BIT_CIRCLE)) {
        po_toggle(g_cursor);
    }
#else
    if (po_pressed(PAD_TRIANGLE)) {
        po_sound(PO_SND_CANCEL);
        dw3_port_options_close();
        return;
    }
    if (po_pressed(PAD_UP) || PAD_REPEATED(PAD_UP)) {
        if (g_cursor > 0) {
            g_cursor--;
            po_sound(PO_SND_CURSOR);
        }
    }
    if (po_pressed(PAD_DOWN) || PAD_REPEATED(PAD_DOWN)) {
        if (g_cursor < DW3_PORT_OPT_COUNT - 1) {
            g_cursor++;
            po_sound(PO_SND_CURSOR);
        }
    }
    if (po_is_stepped(g_cursor)) {
        if (po_pressed(PAD_LEFT) || PAD_REPEATED(PAD_LEFT)) {
            po_step(g_cursor, -1);
        }
        if (po_pressed(PAD_RIGHT) || PAD_REPEATED(PAD_RIGHT)) {
            po_step(g_cursor, 1);
        }
    }
    if (po_pressed(PAD_CROSS) || po_pressed(PAD_CIRCLE)) {
        po_toggle(g_cursor);
    }
#endif
    po_push_view();
}

#if !defined(DW3_PORT_OPTIONS_TEST) && defined(DW3_PC_HOST) && \
    !defined(DW3_TITLE_NATIVE_OVERLAY)
static void dw3_port_options_maybe_fb_dump(void) {
    const char *path;
    unsigned char *rgb;
    int w = 0;
    int h = 0;
    FILE *fp;
    size_t nbytes;
    static int dumped;

    if (dumped || g_open_frames < 45) {
        return;
    }
    path = getenv("DW3_PORT_OPTIONS_FB_DUMP");
    if (path == NULL || path[0] == '\0') {
        return;
    }
    rgb = Psyz_VideoAllocCapturedFrame(&w, &h);
    if (rgb == NULL || w <= 0 || h <= 0) {
        rgb = Psyz_VideoAllocVramDump(&w, &h);
    }
    if (rgb == NULL || w <= 0 || h <= 0) {
        fprintf(stderr, "dw3-port-options: FB dump failed\n");
        fflush(stderr);
        return;
    }
    nbytes = (size_t)w * (size_t)h * 3u;
    fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "dw3-port-options: cannot write %s\n", path);
        fflush(stderr);
        free(rgb);
        return;
    }
    fprintf(fp, "P6\n%d %d\n255\n", w, h);
    fwrite(rgb, 1, nbytes, fp);
    fclose(fp);
    free(rgb);
    dumped = 1;
    fprintf(stderr, "dw3-port-options: TTB FB dump %dx%d -> %s\n", w, h,
            path);
    fflush(stderr);
    if (getenv("DW3_PORT_OPTIONS_QUIT_AFTER_DUMP") != NULL) {
        exit(0);
    }
}
#endif

void dw3_port_options_draw(void) {
#ifdef DW3_PORT_OPTIONS_TEST
    (void)g_cursor;
#elif defined(DW3_TITLE_NATIVE_OVERLAY)
    /* The native overlay draws the options screen (po_push_view). */
#else
    short ox = 0;
    short oy = 0;
    DISPENV env;
    char lines_buf[DW3_PORT_OPT_COUNT][48];
    char back_line[48];
    const char *lines[DW3_PORT_OPT_COUNT + 4];
    int n = 0;
    int i;

    if (!g_open) {
        return;
    }
#ifdef DW3_CHEATS
    if (dw3_cheats_is_open()) {
        dw3_cheats_draw_panel();
        return;
    }
#endif
    GetDispEnv(&env);
    ox = env.disp.x;
    oy = env.disp.y;

    lines[n++] = "Port options";
    lines[n++] = "------------";
    for (i = 0; i < DW3_PORT_OPT_COUNT && n < DW3_PORT_OPT_COUNT + 2; i++) {
        char *buf = lines_buf[i];
        if (po_is_mode(i)) {
            snprintf(buf, sizeof(lines_buf[i]), "%c %s: %s",
                     (i == g_cursor) ? '>' : ' ', g_stubs[i].label,
                     po_mode_name(i, g_stubs[i].value));
        } else if (po_is_volume(i)) {
            snprintf(buf, sizeof(lines_buf[i]), "%c %s: %d",
                     (i == g_cursor) ? '>' : ' ', g_stubs[i].label,
                     g_stubs[i].value);
        } else if (i == DW3_PORT_OPT_FAST_FORWARD && g_stubs[i].value) {
            snprintf(buf, sizeof(lines_buf[i]), "%c %s: %dx (R3)",
                     (i == g_cursor) ? '>' : ' ', g_stubs[i].label,
                     g_stubs[i].value);
        } else {
            snprintf(buf, sizeof(lines_buf[i]), "%c %s: %s",
                     (i == g_cursor) ? '>' : ' ', g_stubs[i].label,
                     g_stubs[i].value ? "ON" : "OFF");
        }
        lines[n++] = buf;
    }
    snprintf(back_line, sizeof(back_line), "Triangle (S): Back");
    lines[n++] = back_line;

    dw3_port_text_draw_panel((int)ox + 32, (int)oy + 32, 256, 128, lines, n);
#if defined(DW3_PC_HOST)
    dw3_port_options_maybe_fb_dump();
#endif
#endif
}

void dw3_port_options_draw_hint(void) {
#ifdef DW3_PORT_OPTIONS_TEST
    return;
#else
    short ox = 0;
    short oy = 0;
    DISPENV env;
    const char *lines[1];

    if (g_open) {
        return;
    }
    GetDispEnv(&env);
    ox = env.disp.x;
    oy = env.disp.y;
    lines[0] = "Select/Backspace: Port options";
    dw3_port_text_draw_panel((int)ox + 40, (int)oy + 210, 240, 20, lines, 1);
#endif
}

/* P6.20 (port/options/cheats.c). At the end of the file, so that a build
 * without DW3_CHEATS keeps every log line above at its line number (psyz_log
 * passes __LINE__) and compiles to the same code. */
#ifdef DW3_CHEATS
int dw3_port_options_store_int(const char *key, int value) {
    char num[16];

    if (!g_persist_on) {
        return -1; /* session-only options: the value stays live */
    }
    snprintf(num, sizeof(num), "%d", value);
    if (dw3_optcfg_set(&g_cfg, key, num) != 0) {
        PO_WARN("dw3-options: cannot stage %s", key);
        return -1;
    }
    if (po_save() != 0) {
        return -1;
    }
    PO_INFO("dw3-options: saved %s=%d", key, value);
    return 0;
}
#endif

#ifdef DW3_CHEATS
void dw3_port_options_play_sound(int kind) {
    if (kind >= PO_SND_CURSOR && kind <= PO_SND_CANCEL) {
        po_sound(kind);
    }
}
#endif

#if defined(DW3_CHEATS) && defined(DW3_PORT_OPTIONS_TEST)
int dw3_port_options_test_bit(int bit) {
    return po_pressed(bit);
}
#endif
