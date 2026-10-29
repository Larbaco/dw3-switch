/* P9.8 confirm / cancel layout: the permutation of the PS buttons and the log
 * of it. Pure policy first (presets, validation, the cfg digits, the permute of
 * a raw pad word), then the one map the pad host reads. docs/CONTROLS_BUNDLE.md.
 * C90: this file is built as gnu89 with the rest of port/. */
#include "port_button_map.h"

#include <stdio.h>
#include <string.h>

#if defined(__SWITCH__) && defined(DW3_TITLE_NATIVE_OVERLAY) && \
    !defined(DW3_BUTTON_MAP_TEST) && !defined(DW3_PORT_OPTIONS_TEST)
#include "title_overlay_art.h"
#define BM_PUBLISH_CAPS 1
#endif

#ifdef DW3_BUTTON_MAP_TEST
#include <stdio.h>
#define BM_LOG(...)                    \
    do {                               \
        fprintf(stderr, __VA_ARGS__);  \
        fputc('\n', stderr);           \
    } while (0)
#else
#include <psyz/log.h>
/* psyz log, not stderr: on the Switch these lines are the checks in
 * sdmc:/switch/dw3/dw3.log (AGENTS.md rule 12). */
#define BM_LOG(...) INFOF(__VA_ARGS__)
#endif

/* Raw bits of PadData.buttons (the engine's PAD_ numbers of the same buttons;
 * a bit is clear while pressed). Cross is raw bit 14: the engine's rotation
 * (readPadButtons) keeps the hardware Cross as the game's confirm. */
static const int g_raw_bit[DW3_BTN_COUNT] = {
    14, /* Cross */
    13, /* Circle */
    12, /* Triangle */
    15, /* Square */
    10, /* L1 */
    11, /* R1 */
    8,  /* L2 */
    9,  /* R2 */
    1,  /* L3 */
    2   /* R3 */
};

static const char *const g_ps_name[DW3_BTN_COUNT] = {
    "cross", "circle", "triangle", "square", "l1", "r1", "l2", "r2", "l3", "r3"};
static const char *const g_ps_label[DW3_BTN_COUNT] = {
    "Cross", "Circle", "Triangle", "Square", "L1", "R1", "L2", "R2", "L3", "R3"};
/* The Switch button at each source position. */
static const char *const g_src_name[DW3_BTN_COUNT] = {
    "B", "A", "X", "Y", "L", "R", "ZL", "ZR", "StickL", "StickR"};
static const char *const g_src_label[DW3_BTN_COUNT] = {
    "B", "A", "X", "Y", "L", "R", "ZL", "ZR", "Left stick click",
    "Right stick click"};

/* Cross<-A (Circle position), Circle<-X (Triangle position), Triangle<-B
 * (Cross position); Square and everything else unchanged. */
static const unsigned char g_nintendo[DW3_BTN_COUNT] = {1, 2, 0, 3, 4, 5, 6, 7, 8, 9};
static const unsigned char g_identity[DW3_BTN_COUNT] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

/* Raw bits of Select and Start: reserved, never remapped (docs/CONTROLS_BUNDLE.md
 * 2.3). In the prompt of the Customize screen they cancel it. */
enum { RAW_SELECT = 0, RAW_START = 3 };

int dw3_button_map_preset(int layout, unsigned char out[DW3_BTN_COUNT]) {
    if (layout == DW3_LAYOUT_PLAYSTATION) {
        memcpy(out, g_identity, DW3_BTN_COUNT);
        return 0;
    }
    if (layout == DW3_LAYOUT_NINTENDO) {
        memcpy(out, g_nintendo, DW3_BTN_COUNT);
        return 0;
    }
    return -1;
}

int dw3_button_map_valid(const unsigned char m[DW3_BTN_COUNT]) {
    unsigned seen = 0;
    int i;

    for (i = 0; i < DW3_BTN_COUNT; i++) {
        if (m[i] >= DW3_BTN_COUNT || (seen >> m[i]) & 1u) {
            return 0;
        }
        seen |= 1u << m[i];
    }
    return 1;
}

void dw3_button_map_format(const unsigned char m[DW3_BTN_COUNT],
                           char out[DW3_BTN_COUNT + 1]) {
    int i;

    for (i = 0; i < DW3_BTN_COUNT; i++) {
        out[i] = (char)('0' + (m[i] < 10 ? m[i] : 0));
    }
    out[DW3_BTN_COUNT] = '\0';
}

int dw3_button_map_parse(const char *text, unsigned char out[DW3_BTN_COUNT]) {
    unsigned char tmp[DW3_BTN_COUNT];
    int i;

    if (text == NULL || strlen(text) != DW3_BTN_COUNT) {
        return -1;
    }
    for (i = 0; i < DW3_BTN_COUNT; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return -1;
        }
        tmp[i] = (unsigned char)(text[i] - '0');
    }
    if (!dw3_button_map_valid(tmp)) {
        return -1;
    }
    memcpy(out, tmp, DW3_BTN_COUNT);
    return 0;
}

int dw3_button_raw_bit(int button) {
    return button >= 0 && button < DW3_BTN_COUNT ? g_raw_bit[button] : -1;
}

unsigned dw3_button_map_permute(const unsigned char m[DW3_BTN_COUNT],
                                unsigned word) {
    unsigned out = word & 0xFFFFu;
    int i;

    for (i = 0; i < DW3_BTN_COUNT; i++) {
        unsigned bit = 1u << g_raw_bit[i];

        if (m[i] == i) {
            continue;
        }
        if ((word >> g_raw_bit[m[i]]) & 1u) {
            out |= bit;
        } else {
            out &= ~bit;
        }
    }
    return out;
}

const char *dw3_button_layout_name(int layout) {
    return layout == DW3_LAYOUT_NINTENDO ? "nintendo"
           : layout == DW3_LAYOUT_CUSTOM ? "custom" : "playstation";
}

int dw3_button_layout_parse(const char *text) {
    if (text == NULL) {
        return -1;
    }
    if (strcmp(text, "playstation") == 0) {
        return DW3_LAYOUT_PLAYSTATION;
    }
    if (strcmp(text, "nintendo") == 0) {
        return DW3_LAYOUT_NINTENDO;
    }
    if (strcmp(text, "custom") == 0) {
        return DW3_LAYOUT_CUSTOM;
    }
    return -1;
}

const char *dw3_button_ps_name(int b) {
    return b >= 0 && b < DW3_BTN_COUNT ? g_ps_name[b] : "?";
}
const char *dw3_button_source_name(int s) {
    return s >= 0 && s < DW3_BTN_COUNT ? g_src_name[s] : "?";
}
const char *dw3_button_ps_label(int b) {
    return b >= 0 && b < DW3_BTN_COUNT ? g_ps_label[b] : "?";
}
const char *dw3_button_source_label(int s) {
    return s >= 0 && s < DW3_BTN_COUNT ? g_src_label[s] : "?";
}

const char *dw3_button_cap_label(const unsigned char map[DW3_BTN_COUNT],
                                 int button) {
    static const char *const cap[DW3_BTN_COUNT] = {
        "B", "A", "X", "Y", "L", "R", "ZL", "ZR", "L3", "R3"};

    if (map == NULL || button < 0 || button >= DW3_BTN_COUNT ||
        map[button] >= DW3_BTN_COUNT) {
        return "?";
    }
    return cap[map[button]];
}

const char *dw3_button_hint_name_for(const unsigned char map[DW3_BTN_COUNT],
                                     int switch_names, int button,
                                     const char *pc_text, char *out,
                                     unsigned n) {
    const char *s = switch_names ? dw3_button_cap_label(map, button)
                                 : (pc_text != NULL ? pc_text : "?");

    if (out == NULL || n == 0) {
        return s;
    }
    snprintf(out, n, "%s", s);
    return out;
}

/* --- state ------------------------------------------------------------- */

static unsigned char g_map[DW3_BTN_COUNT] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
static int g_layout = DW3_LAYOUT_PLAYSTATION;
static int g_identity_map = 1;   /* the map is the identity: skip the permute */
static int g_first_logged;       /* the "first press" line, re-armed by a change */
/* After a layout change the buttons held at that moment are read as released
 * until all of them are up: the press that changed the layout (B on PlayStation,
 * A on Nintendo) would otherwise be a new press of another PS button in the new
 * map (Triangle, Circle) and fire a second action (back out of the group, or
 * switch the layout again, every frame while it is held). */
static int g_swallow;

int dw3_button_map_layout(void) {
    return g_layout;
}

const unsigned char *dw3_button_map_current(void) {
    return g_map;
}

static unsigned g_last_raw = 0xFFFFu; /* the word of the last frame, before the map */
static unsigned g_cap_ignore;          /* raw bits held since the prompt opened */

/* Make `layout` the one in force. A custom layout keeps the map as it is. */
static void bm_set(int layout) {
    if (layout == DW3_LAYOUT_CUSTOM) {
        /* the map stays: a custom map is whatever is in g_map */
    } else if (dw3_button_map_preset(layout, g_map) != 0) {
        layout = DW3_LAYOUT_PLAYSTATION;
        dw3_button_map_preset(layout, g_map);
    }
    g_layout = layout;
    g_identity_map = memcmp(g_map, g_identity, DW3_BTN_COUNT) == 0;
    g_first_logged = 0;
    g_swallow = 0;
}

/* `dw3-controls: map cross<-A circle<-X ...` and the hotkeys line. */
static void bm_log_map(void) {
    char line[256];
    char hot[160];
    size_t n = 0;
    int i;

    line[0] = '\0';
    for (i = 0; i < DW3_BTN_COUNT; i++) {
        int w = (int)strlen(dw3_button_ps_name(i)) +
                (int)strlen(dw3_button_source_name(g_map[i])) + 3;

        if (n + (size_t)w >= sizeof(line)) {
            break;
        }
        strcat(line, i ? " " : "");
        strcat(line, dw3_button_ps_name(i));
        strcat(line, "<-");
        strcat(line, dw3_button_source_name(g_map[i]));
        n = strlen(line);
    }
    BM_LOG("dw3-controls: map %s", line);

    /* The port's hotkeys are named in PS buttons and follow the map; the
     * Switch diagnostic chords are physical and do not (docs/HOTKEYS.md). */
    hot[0] = '\0';
    strcat(hot, "fast-forward=R3 (");
    strcat(hot, dw3_button_source_label(g_map[DW3_BTN_R3]));
    strcat(hot, ") restart=L1+R1+Start+Select (");
    strcat(hot, dw3_button_source_name(g_map[DW3_BTN_L1]));
    strcat(hot, "+");
    strcat(hot, dw3_button_source_name(g_map[DW3_BTN_R1]));
    strcat(hot, "+Plus+Minus)");
    BM_LOG("dw3-controls: hotkeys %s", hot);
    /* P10.6 fix-now: the caps the hint rows draw on a Switch build. */
    BM_LOG("dw3-controls: hint caps confirm=%s back=%s (cross, triangle)",
           dw3_button_cap_label(g_map, DW3_BTN_CROSS),
           dw3_button_cap_label(g_map, DW3_BTN_TRIANGLE));
#ifdef BM_PUBLISH_CAPS
    dw3_pc_title_overlay_set_hint_caps(g_map[DW3_BTN_CROSS],
                                       g_map[DW3_BTN_TRIANGLE]);
#endif
}

const char *dw3_button_hint_name(int button, const char *pc_text, char *out,
                                 unsigned n) {
#if defined(__SWITCH__)
    const int sw = 1;
#else
    const int sw = 0;
#endif
    return dw3_button_hint_name_for(g_map, sw, button, pc_text, out, n);
}

void dw3_button_map_load(int layout, const char *source) {
    bm_set(layout);
    BM_LOG("dw3-controls: map loaded layout=%s source=%s",
           dw3_button_layout_name(g_layout), source != NULL ? source : "default");
    bm_log_map();
}

void dw3_button_map_load_custom(const unsigned char m[DW3_BTN_COUNT],
                                const char *source) {
    if (m == NULL || !dw3_button_map_valid(m)) {
        dw3_button_map_log_invalid("button_map is not a permutation");
        dw3_button_map_load(DW3_LAYOUT_PLAYSTATION, "default");
        return;
    }
    memcpy(g_map, m, DW3_BTN_COUNT);
    bm_set(DW3_LAYOUT_CUSTOM);
    BM_LOG("dw3-controls: map loaded layout=custom source=%s",
           source != NULL ? source : "cfg");
    bm_log_map();
}

void dw3_button_map_change(int layout, const char *how) {
    int old = g_layout;

    bm_set(layout);
    g_swallow = 1;
    BM_LOG("dw3-controls: layout changed %s -> %s (%s)",
           dw3_button_layout_name(old), dw3_button_layout_name(g_layout),
           how != NULL ? how : "screen");
    bm_log_map();
}

void dw3_button_map_log_invalid(const char *reason) {
    BM_LOG("dw3-controls: map invalid (%s), using default",
           reason != NULL ? reason : "?");
}

int dw3_button_map_assign(int button, int source) {
    int swapped;
    unsigned char old;
    char digits[DW3_BTN_COUNT + 1];

    if (button < 0 || button >= DW3_BTN_COUNT || source < 0 ||
        source >= DW3_BTN_COUNT) {
        return -1;
    }
    if (g_map[button] == source) {
        BM_LOG("dw3-controls: assign %s<-%s (unchanged) [%s]",
               dw3_button_ps_name(button), dw3_button_source_name(source),
               dw3_button_layout_name(g_layout));
        return button;
    }
    old = g_map[button];
    for (swapped = 0; swapped < DW3_BTN_COUNT; swapped++) {
        if (g_map[swapped] == source) {
            break;
        }
    }
    g_map[swapped] = old;
    g_map[button] = (unsigned char)source;
    dw3_button_map_format(g_map, digits);
    g_layout = DW3_LAYOUT_CUSTOM;
    g_identity_map = memcmp(g_map, g_identity, DW3_BTN_COUNT) == 0;
    g_first_logged = 0;
    g_swallow = 1;
    BM_LOG("dw3-controls: assign %s<-%s (%s<-%s swapped in) [custom]",
           dw3_button_ps_name(button), dw3_button_source_name(source),
           dw3_button_ps_name(swapped), dw3_button_source_name(old));
    BM_LOG("dw3-controls: map digits %s", digits);
    bm_log_map();
    return swapped;
}

void dw3_button_map_reset(const char *how) {
    bm_set(DW3_LAYOUT_PLAYSTATION);
    g_swallow = 1;
    BM_LOG("dw3-controls: reset to default (layout=playstation) (%s)",
           how != NULL ? how : "screen");
    bm_log_map();
}

unsigned dw3_button_map_source_word(void) {
    return g_last_raw;
}

int dw3_button_map_capture_step(unsigned raw_word, unsigned *ignore) {
    unsigned down = 0;
    unsigned fresh;
    int i;

    for (i = 0; i < DW3_BTN_COUNT; i++) {
        if (((raw_word >> g_raw_bit[i]) & 1u) == 0) {
            down |= 1u << g_raw_bit[i];
        }
    }
    if (((raw_word >> RAW_SELECT) & 1u) == 0) {
        down |= 1u << RAW_SELECT;
    }
    if (((raw_word >> RAW_START) & 1u) == 0) {
        down |= 1u << RAW_START;
    }
    *ignore &= down; /* released since the prompt opened: counts again */
    fresh = down & ~*ignore;
    if (fresh & ((1u << RAW_SELECT) | (1u << RAW_START))) {
        return DW3_CAPTURE_CANCEL;
    }
    for (i = 0; i < DW3_BTN_COUNT; i++) {
        if ((fresh >> g_raw_bit[i]) & 1u) {
            return i;
        }
    }
    return DW3_CAPTURE_NONE;
}

void dw3_button_map_capture_begin(void) {
    /* Everything down now (a clear bit) is "held": ignored until let go. */
    g_cap_ignore = ~g_last_raw & 0xFFFFu;
}

int dw3_button_map_capture_poll(void) {
    return dw3_button_map_capture_step(g_last_raw, &g_cap_ignore);
}

unsigned dw3_button_map_apply(unsigned word) {
    unsigned out;

    g_last_raw = word & 0xFFFFu;
    if (g_swallow) {
        unsigned held = 0;
        int i;

        for (i = 0; i < DW3_BTN_COUNT; i++) {
            if (((word >> g_raw_bit[i]) & 1u) == 0) {
                held |= 1u << g_raw_bit[i];
            }
        }
        if (held != 0) {
            word |= held; /* still down since the change: read as released */
        } else {
            g_swallow = 0;
        }
    }
    out = g_identity_map ? (word & 0xFFFFu)
                         : dw3_button_map_permute(g_map, word);

    if (!g_first_logged && ((out >> g_raw_bit[DW3_BTN_CROSS]) & 1u) == 0) {
        g_first_logged = 1;
        BM_LOG("dw3-controls: first press source=%s -> logical cross",
               dw3_button_source_name(g_map[DW3_BTN_CROSS]));
    }
    return out;
}
