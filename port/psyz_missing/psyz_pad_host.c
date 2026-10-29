/*
 * Host libpad (PSY-Q 4.2-style) on top of PSY-Z InitPAD / Psyz_Pads*.
 *
 * PSY-Z's PadGetState is a NOT_IMPLEMENTED stub that always returns
 * PadStateDiscon; PadInitDirect is missing. Without these, the decomp pad
 * path clears every frame and PAD_PRESSED never sees input.
 *
 * Symbols that also exist in libpsyz (PadGetState, PadStartCom, …) are
 * provided as __wrap_* and selected via -Wl,--wrap=… on dw3-pc.
 */
#include <libetc.h>
#include "dw3_replay.h"
#include <psyz/input.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef PadStateDiscon
#define PadStateDiscon 0
#endif
#ifndef PadStateFindCTP1
#define PadStateFindCTP1 2
#endif
#ifndef InfoModeCurID
#define InfoModeCurID 1
#endif
#ifndef InfoModeCurExID
#define InfoModeCurExID 2
#endif
#ifndef InfoModeCurExOffs
#define InfoModeCurExOffs 3
#endif
#ifndef InfoModeIdTable
#define InfoModeIdTable 4
#endif
/* DualShock actuator probes (libpad.h values; PSY-Z ships no libpad.h). */
#ifndef InfoActFunc
#define InfoActFunc 1
#endif
#ifndef InfoActSub
#define InfoActSub 2
#endif
#ifndef InfoActSize
#define InfoActSize 3
#endif
#ifndef InfoActCurr
#define InfoActCurr 4
#endif
#ifndef PadMaxCurr
#define PadMaxCurr 60
#endif
#ifndef PadStateStable
#define PadStateStable 6
#endif

/* PSY-Z libapi (avoid libapi.h type clashes with decomp headers). */
int InitPAD(char *bufA, int lenA, char *bufB, int lenB);
long StartPAD(void);

/* P10.4 rumble hook behind __wrap_PadSetAct: PAD.act bytes -> platform
 * motors. Weak idle default below; the Switch build links a strong
 * Psyz_PadsSetAct from platforms/switch/psyz_backend/nx_rumble.c. */
void Psyz_PadsSetAct(int port, const unsigned char *act, int len);

enum { DW3_PAD_BUF_LEN = 34 };

/* pollPadState treats PadStateStable as "the pad can be read AND its
 * actuators can be aligned" (src/engine/pad/port.c). Stall that handoff for
 * the first connected polls so the boot looks like a real pad's negotiate
 * cycle, then flip to Stable: alignActuators -> PAD_FLAG_ALIGNING ->
 * PAD_FLAG_VIBRATION, which is what lets setVibration reach PadSetAct. */
#define DW3_PAD_STABLE_POLLS 4

static unsigned char *g_pad_rx[2];
static int g_pad_started;
static int g_logged_connected;
static int g_logged_buttons;
static int g_logged_main_mode;
static int g_logged_stable;
static int g_stable_polls[2];

static int port_index(int port) { return (port >> 4) & 1; }

static void dw3_pad_host_debug_buttons(void);
static void dw3_auto_pad_inject(void);
static void dw3_pad_publish_mask(unsigned mask);
static void dw3_replay_vsync(void);
/* port/map_view/map_view.c: the right stick's vertical axis, for the zoom of
 * the full-screen map (-127..127, up positive). In a `both` image the call goes
 * to the running region's copy (dw3_game_api.h). */
#if defined(DW3_BOTH) && DW3_BOTH
#include "dw3_game_api.h"
#else
extern void dw3_map_view_set_zoom_stick(int raw_up);
/* port/options/button_map.c (P9.8): the raw pad word through the layout map. */
extern unsigned dw3_button_map_apply(unsigned word);
#endif
/* Replay masks: bits above the 16 pad bits stand for the right stick (the
 * recording cannot hold an analog value): 0x10000 up, 0x20000 down. */
#define DW3_REPLAY_RSTICK_UP 0x10000u
#define DW3_REPLAY_RSTICK_DOWN 0x20000u
static void dw3_replay_record_sample(void);
static int buf_connected(const unsigned char *buf);

/* Raw PS1 digital bits in PadData.buttons (active-low in the wire buffer).
 * US readPadButtons remaps face buttons from RAW_* (12/13/14). */
enum {
    DW3_RAW_SELECT = 1 << 0,
    DW3_RAW_START = 1 << 3,
    DW3_RAW_UP = 1 << 4,
    DW3_RAW_RIGHT = 1 << 5,
    DW3_RAW_DOWN = 1 << 6,
    DW3_RAW_LEFT = 1 << 7,
    DW3_RAW_TRIANGLE = 1 << 12,
    DW3_RAW_CIRCLE = 1 << 13,
    DW3_RAW_CROSS = 1 << 14,
    DW3_RAW_SQUARE = 1 << 15
};

/*
 * The controller mode ID table libpad's PadInfoMode/PadSetMainMode index into
 * (libetc.h: InfoModeCurExOffs is the offset of the *active* mode in this
 * table; PadSetMainMode's `offs` is the offset of the mode to switch to). A
 * DualShock only ever exposes a digital pad (0x41) and an analog pad (0x73).
 * Those are exactly the two the decomp recognises: readPadButtons takes
 * PadInfoMode(InfoModeCurExID, 0) and compares it to PAD_ID_DIGITAL (4) and
 * PAD_ID_ANALOG (7), i.e. the kind's high nibble. PSY-Z's ANALOG_STICK (0x53,
 * high nibble 5) is deliberately absent — it matches neither.
 */
static const unsigned char g_pad_mode_table[] = {
    (unsigned char)PSYZ_CTRL_DIGITAL_PAD,
    (unsigned char)PSYZ_CTRL_ANALOG_PAD,
};

/* Offset of `kind` in the table above, or 0 when it is not a table entry. */
static int mode_table_offset(unsigned char kind) {
    int i;

    for (i = 0; i < (int)sizeof(g_pad_mode_table); i++) {
        if (g_pad_mode_table[i] == kind) {
            return i;
        }
    }
    return 0;
}

/*
 * P6.15 regression replay (off by default).
 *
 * Pad input is recorded and replayed at this layer, keyed by the game frame
 * (the PadChkVsync count), never by wall-clock time. That makes a run
 * independent of host speed, Xvfb timing and turbo.
 *
 *   DW3_REPLAY=<file>         replay: <file> supplies the pad mask each frame
 *   DW3_REPLAY_RECORD=<file>  record: log every change of the real pad mask
 *
 * The recording format and parser are platform independent and live in
 * port/replay/dw3_replay.c (see port/include/dw3_replay.h). The shot
 * callback is set by the host (platforms/pc/src/main.c writes a PPM); the pad
 * layer holds no video code. See docs/P6.15_REPLAY_TESTS.md.
 *
 * Switch: a release NRO never replays. Only a build configured with
 * -DDW3_NX_REPLAY=ON (compile-time define DW3_NX_REPLAY) reads
 * DW3_NX_REPLAY_PATH (sdmc:/switch/dw3/replay.rec) instead of the
 * environment, and reports checkpoints as `replay shot <frame> <name>` lines
 * through the log sinks (svcOutputDebugString under Ryubing) plus a final
 * `replay end`. Without that define none of the Switch path exists.
 */
void (*dw3_pad_host_shot_cb)(const char *name, unsigned frame);

#ifdef DW3_NX_REPLAY
#ifndef DW3_NX_REPLAY_PATH
#define DW3_NX_REPLAY_PATH "sdmc:/switch/dw3/replay.rec"
#endif
#endif

static Dw3Replay g_rp;
static int g_rp_state; /* 0 untried, 1 replay, 2 record, 3 off */
static unsigned g_rp_frame;
static unsigned g_rec_last = 0xFFFFFFFFu;

/* P5.1 PC memory sampler: the game frame (PadChkVsync count) of a replay or
 * record run; 0 when neither is active. */
unsigned dw3_pad_host_frame(void) {
    return g_rp_frame;
}
static FILE *g_rec_fp;

static void replay_shot_fire(const char *name, unsigned frame, void *ctx) {
    (void)ctx;
#ifdef DW3_NX_REPLAY
    /* The only guest log that is live under Ryubing is stderr routed to
     * svcOutputDebugString (see boot_main.c). One line per checkpoint. */
    fprintf(stderr, "replay shot %u %s\n", frame, name);
    fflush(stderr);
#endif
    if (dw3_pad_host_shot_cb != NULL) {
        dw3_pad_host_shot_cb(name, frame);
    }
}

static int replay_open_play(void) {
    const char *play;
    int bad = 0;

#ifdef DW3_NX_REPLAY
    play = DW3_NX_REPLAY_PATH;
#else
    play = getenv("DW3_REPLAY");
    if (play == NULL || play[0] == '\0') {
        return -1;
    }
#endif
    if (dw3_replay_load(&g_rp, play, &bad) != 0) {
        fprintf(stderr, "dw3-replay: cannot open %s\n", play);
        return -1;
    }
    fprintf(stderr, "dw3-replay: loaded %s pad=%u shot=%u end=%u skipped=%d\n",
            play, (unsigned)g_rp.npad, (unsigned)g_rp.nshot, g_rp.end, bad);
#ifdef DW3_NX_REPLAY
    fprintf(stderr, "replay begin pad=%u shot=%u end=%u\n", (unsigned)g_rp.npad,
            (unsigned)g_rp.nshot, g_rp.end);
    fflush(stderr);
#endif
    return 0;
}

static void dw3_replay_vsync(void) {
    const char *rec;
    unsigned mask = 0;

    if (g_rp_state == 3) {
        return;
    }
    if (g_rp_state == 0) {
        g_rp_state = 3;
        if (replay_open_play() == 0) {
            g_rp_state = 1;
        }
#ifndef DW3_NX_REPLAY
        else if ((rec = getenv("DW3_REPLAY_RECORD")) != NULL && rec[0] != '\0') {
            g_rec_fp = fopen(rec, "w");
            if (g_rec_fp != NULL) {
                fprintf(g_rec_fp,
                        "# dw3 pad recording (frame = PadChkVsync count)\n");
                g_rp_state = 2;
                if (getenv("DW3_AUTO_PAD") != NULL) {
                    fprintf(stderr,
                            "dw3-replay: ignoring DW3_AUTO_PAD while recording real input\n");
                }
            }
        }
#else
        (void)rec;
#endif
        if (g_rp_state == 3) {
            return;
        }
    }
    g_rp_frame++;
    if (g_rp_state == 1) {
        /* Heartbeat for the runner's hang watchdog and a soak timeline:
         * one line every 600 game frames (~10 s of game time). */
        if (g_rp_frame % 600u == 0u) {
            fprintf(stderr, "dw3-replay: frame %u\n", g_rp_frame);
            fflush(stderr);
        }
        if (dw3_replay_advance(&g_rp, g_rp_frame, replay_shot_fire, NULL,
                               &mask)) {
            fprintf(stderr, "dw3-replay: end at frame %u\n", g_rp_frame);
#ifdef DW3_NX_REPLAY
            fprintf(stderr, "replay end %u\n", g_rp_frame);
#endif
            fflush(stderr);
            exit(0);
        }
        dw3_map_view_set_zoom_stick((mask & DW3_REPLAY_RSTICK_UP) ? 127
                                    : (mask & DW3_REPLAY_RSTICK_DOWN) ? -127 : 0);
        dw3_pad_publish_mask(mask & 0xFFFFu);
    }
}

/* Record mode: log the real pad (read from the RX buffer after refresh). */
static void dw3_replay_record_sample(void) {
    unsigned char *buf = g_pad_rx[0];
    unsigned mask;

    if (g_rp_state != 2 || buf == NULL || !buf_connected(buf)) {
        return;
    }
    mask = (~(((unsigned)buf[3] << 8) | buf[2])) & 0xFFFFu;
    if (mask != g_rec_last) {
        g_rec_last = mask;
        fprintf(g_rec_fp, "pad %u 0x%04x\n", g_rp_frame, mask);
        fflush(g_rec_fp);
    }
}

static void dw3_auto_pad_inject(void) {
    const char *env;
    unsigned mask = 0;
    static unsigned frame;
    int confirm = 0;
    int walk = 0;
    int start = 0;
    int cancel = 0;
    int status = 0;
    int status_return = 0;
    int talk = 0;
    int select = 0;

    env = getenv("DW3_AUTO_PAD");
    if (env == NULL || env[0] == '\0') {
        return;
    }
    if (strstr(env, "confirm") != NULL) {
        confirm = 1;
    }
    if (strstr(env, "walk") != NULL) {
        walk = 1;
    }
    if (strstr(env, "start") != NULL) {
        start = 1;
    }
    if (strstr(env, "cancel") != NULL) {
        cancel = 1;
    }
    /* status_return: Start→Cross→Triangle UI cancel (no AUTO_MENU_RETURN). */
    if (strstr(env, "status_return") != NULL) {
        status_return = 1;
        status = 1;
    } else if (strstr(env, "status") != NULL) {
        status = 1;
    }
    if (strstr(env, "talk") != NULL) {
        talk = 1;
    }
    /* select: delayed Select pulses (P9.2 title → port options). */
    if (strstr(env, "select") != NULL) {
        select = 1;
    }
    if (!confirm && !walk && !start && !cancel && !status && !talk &&
        !select) {
        return;
    }
    frame++;
    if (walk) {
        mask |= DW3_RAW_UP;
    }
    /* Hold a few frames per pulse — PadGetState refresh between PadChkVsync
     * and readPad can miss a single-sample edge. */
    if (confirm && !status && (frame % 16u) >= 1u && (frame % 16u) <= 4u) {
        mask |= DW3_RAW_CROSS;
    }
    /* Hold Start for movie skip (PORT patch also accepts getHeld). Pulse log
     * once so headless runs show inject without flooding.
     * When `select` is also set (P9.2 title Config smoke), only pulse Start
     * early so PRESS START advances, then release so we stay on NG/Continue. */
    if (start && !status && frame >= 2u) {
        if (select) {
            if (frame < 80u && (frame % 16u) >= 1u && (frame % 16u) <= 6u) {
                mask |= DW3_RAW_START;
            }
            if (frame == 2u) {
                fprintf(stderr,
                        "dw3-pad: AUTO_PAD start pulse (with select) "
                        "from frame=%u\n",
                        frame);
                fflush(stderr);
            }
        } else {
            mask |= DW3_RAW_START;
            if (frame == 2u) {
                fprintf(stderr,
                        "dw3-pad: AUTO_PAD start hold from frame=%u\n", frame);
                fflush(stderr);
            }
        }
    }
    /* status: Start (open field menu) → Cross (MODE_STATUS) → Triangle.
     * status_return phases Triangle alone after entry so STSTATUS UI cancel
     * can return to field without DW3_AUTO_MENU_RETURN. */
    if (status) {
        if (frame >= 180u && frame < (status_return ? 900u : 2400u) &&
            (frame % 45u) >= 1u && (frame % 45u) <= 6u) {
            mask |= DW3_RAW_START;
        }
        if (frame >= 500u && frame < (status_return ? 1100u : 100000u) &&
            (frame % 16u) >= 1u && (frame % 16u) <= 4u) {
            mask |= DW3_RAW_CROSS;
        }
        if (frame >= (status_return ? 1300u : 1200u) &&
            (frame % 18u) >= 1u && (frame % 18u) <= 5u) {
            mask |= DW3_RAW_TRIANGLE;
        }
    }
    /* cancel: Triangle pulses; with FORCE_MENU into STSTATUS use a long
     * delay so the item screen reaches its choose-options substate. */
    if (cancel && !status && frame >= 600u && (frame % 18u) >= 1u &&
        (frame % 18u) <= 5u) {
        mask |= DW3_RAW_TRIANGLE;
    }
    /* talk: delayed Cross pulses for NPC face-tile talk after warp. */
    if (talk && !status && frame >= 400u && (frame % 24u) >= 1u &&
        (frame % 24u) <= 4u) {
        mask |= DW3_RAW_CROSS;
    }
    /* select: after title options spread, pulse Select so P9.2 can open
     * the port-options shell. With start+select, Start stops by frame 80. */
    if (select && !status && frame >= 160u && frame < 280u &&
        (frame % 20u) >= 1u && (frame % 20u) <= 4u) {
        mask |= DW3_RAW_SELECT;
        if (frame == 161u) {
            fprintf(stderr, "dw3-pad: AUTO_PAD select pulse from frame=%u\n",
                    frame);
            fflush(stderr);
        }
    }
    dw3_pad_publish_mask(mask);
}

static void dw3_pad_publish_mask(unsigned mask) {
    char buf[DW3_PAD_BUF_LEN];

    memset(buf, 0, sizeof(buf));
    buf[0] = 0x00;
    buf[1] = (char)PSYZ_CTRL_DIGITAL_PAD;
    /* PadData.buttons is a native u16 over bytes [2],[3]. PS1 wire order is
     * MSB at [2], but this LE host overlays the u16 as LSB-first — store the
     * active-low mask in native u16 byte order so readPadButtons sees RAW_*. */
    {
        unsigned short wire = (unsigned short)(~mask);
        buf[2] = (char)(wire & 0xFF);
        buf[3] = (char)((wire >> 8) & 0xFF);
    }
    Psyz_PadsSet(0, buf, DW3_PAD_BUF_LEN);
}

/* P9.8 / docs/CONTROLS_BUNDLE.md: port 1's raw word through the confirm /
 * cancel layout, in place, after the platform's frame is copied and before the
 * engine reads a bit. The platform keeps its positional table (Switch A is the
 * Circle position); the map says which PS button each position produces.
 *
 * Not applied while a replay plays or DW3_AUTO_PAD injects: those masks already
 * name the PS buttons the game must see, so the replay suite stays
 * byte-identical (P6.15). DW3_REPLAY_SOURCE=1 (a dev variable of the layout
 * scenarios) says the replay masks are positions, as a real pad reports them, so
 * the map is exercised end to end. Recording stores the mapped word. */
static int g_map_mode = -1; /* -1 untried, 0 normal, 1 never (auto pad) */
static int g_map_replay_source = -1;

static void dw3_pad_apply_map(unsigned char *buf) {
    unsigned word;
    unsigned mapped;

    if (g_map_mode < 0) {
        const char *ap = getenv("DW3_AUTO_PAD");
        const char *rs = getenv("DW3_REPLAY_SOURCE");

        g_map_mode = (ap != NULL && ap[0] != '\0') ? 1 : 0;
        g_map_replay_source = (rs != NULL && rs[0] == '1') ? 1 : 0;
    }
    if (g_map_mode != 0 || (g_rp_state == 1 && !g_map_replay_source) ||
        !buf_connected(buf)) {
        return;
    }
    word = (unsigned)buf[2] | ((unsigned)buf[3] << 8);
    mapped = dw3_button_map_apply(word);
    if (mapped != word) {
        buf[2] = (unsigned char)(mapped & 0xFFu);
        buf[3] = (unsigned char)((mapped >> 8) & 0xFFu);
    }
}

static void refresh_port(int idx) {
    if (idx < 0 || idx > 1 || g_pad_rx[idx] == NULL) {
        return;
    }
    Psyz_PadsGet(idx, (char *)g_pad_rx[idx], DW3_PAD_BUF_LEN);
    if (idx == 0) {
        dw3_pad_apply_map(g_pad_rx[0]);
    }
}

static int buf_connected(const unsigned char *buf) {
    unsigned char kind;

    if (buf == NULL) {
        return 0;
    }
    if (buf[0] != 0x00) {
        return 0;
    }
    kind = buf[1];
    if (kind == 0x00 || kind == 0xFF) {
        return 0;
    }
    switch (kind & 0xF0) {
    case 0x40:
    case 0x50:
    case 0x70:
        return 1;
    default:
        return 0;
    }
}

void PadInitDirect(unsigned char *pad1, unsigned char *pad2) {
    g_pad_rx[0] = pad1;
    g_pad_rx[1] = pad2;
    g_pad_started = 0;
    g_logged_connected = 0;
    g_logged_buttons = 0;
    g_logged_main_mode = 0;
    InitPAD((char *)pad1, DW3_PAD_BUF_LEN, (char *)pad2, DW3_PAD_BUF_LEN);
    fprintf(stderr, "dw3-pad: PadInitDirect -> InitPAD\n");
}

void __wrap_PadInitMtap(unsigned char *pad1, unsigned char *pad2) {
    PadInitDirect(pad1, pad2);
}

void __wrap_PadStartCom(void) {
    StartPAD();
    g_pad_started = 1;
    fprintf(stderr, "dw3-pad: PadStartCom\n");
}

void PadStopCom(void) {
    g_pad_started = 0;
}

int PadChkVsync(void) {
    if (!g_pad_started) {
        return 0;
    }
    dw3_replay_vsync();
    if (g_rp_state != 1 && g_rp_state != 2) {
        dw3_auto_pad_inject();
    }
    refresh_port(0);
    refresh_port(1);
    if (g_rp_state != 1) {
        /* the right stick (wire byte 5, 0x80 centred, up is smaller) of an
         * analog pad; the game reads only the left one (analog[2], [3]) */
        const unsigned char *b0 = g_pad_rx[0];

        dw3_map_view_set_zoom_stick(
            (b0 != NULL && buf_connected(b0) && (b0[1] & 0xF0) == 0x70)
                ? 0x80 - (int)b0[5] : 0);
    }
    dw3_replay_record_sample();
    dw3_pad_host_debug_buttons();
    return 1;
}

int __wrap_PadGetState(int port) {
    int idx = port_index(port);
    unsigned char *buf;

    if (idx < 0 || idx > 1) {
        return PadStateDiscon;
    }
    buf = g_pad_rx[idx];
    if (buf == NULL) {
        return PadStateDiscon;
    }
    refresh_port(idx);
    if (!buf_connected(buf)) {
        return PadStateDiscon;
    }
    if (!g_logged_connected) {
        g_logged_connected = 1;
        fprintf(stderr, "dw3-pad: port %d connected kind=0x%02x (PadStateFindCTP1)\n",
                idx, (unsigned)buf[1]);
    }
    /* Negotiate like a real pad: FindCTP1 until the pad settled, then jump
     * straight to Stable. pollPadState keys the vibration alignment on the
     * Stable state; FindCTP1 would keep PAD_FLAG_VIBRATION off forever. */
    if (g_stable_polls[idx] < DW3_PAD_STABLE_POLLS) {
        g_stable_polls[idx]++;
        return PadStateFindCTP1;
    }
    if (!g_logged_stable) {
        g_logged_stable = 1;
        fprintf(stderr, "dw3-pad: port %d stable after %d connected polls "
                        "(actuators aligned)\n", idx, DW3_PAD_STABLE_POLLS);
    }
    return PadStateStable;
}

int PadInfoMode(int port, int term, int offs) {
    int idx = port_index(port);
    unsigned char *buf;
    unsigned char kind;

    if (idx < 0 || idx > 1 || g_pad_rx[idx] == NULL) {
        return 0;
    }
    buf = g_pad_rx[idx];
    refresh_port(idx);
    if (!buf_connected(buf)) {
        return 0;
    }
    kind = buf[1];
    switch (term) {
    case InfoModeCurID:
        return (int)kind;
    case InfoModeCurExID:
        return (int)(kind >> 4);
    case InfoModeCurExOffs:
        /* Offset of the active mode in the mode ID table. lockPadMode passes
         * this straight back to PadSetMainMode, so the two must agree. */
        return mode_table_offset(kind);
    case InfoModeIdTable:
        /* The mode ID at the requested table offset (0 past the end). */
        if (offs < 0 || offs >= (int)sizeof(g_pad_mode_table)) {
            return 0;
        }
        return (int)g_pad_mode_table[offs];
    default:
        return 0;
    }
}

/*
 * port.c lockPadMode sends PadSetMainMode(id, PadInfoMode(id, InfoModeCurExOffs,
 * 0), PAD_MODE_LOCKED/UNLOCKED) to pin the pad's mode-selector button. `offs`
 * selects the mode to switch to from the table above, so the mode is handed to
 * PSY-Z: the platform backend builds its next frame from that kind (PC
 * sdl3_common.h BuildPadFrame, Switch nx_input.c), and the switch shows up on
 * the following PadGetState/PadChkVsync, as a real controller's would. The lock
 * bits only describe the (absent) selector button, so they are interpreted for
 * the log rather than acted on.
 */
int PadSetMainMode(int port, int offs, int lock) {
    int idx = port_index(port);
    unsigned char *buf;

    if (idx < 0 || idx > 1 || g_pad_rx[idx] == NULL) {
        return 0;
    }
    if (offs < 0 || offs >= (int)sizeof(g_pad_mode_table)) {
        return 0;
    }
    buf = g_pad_rx[idx];
    refresh_port(idx);
    if (!buf_connected(buf)) {
        return 0;
    }
    Psyz_PadsSetKind(idx, 0, (PsyzControllerKind)g_pad_mode_table[offs]);
    if (!g_logged_main_mode) {
        /* lock bit 1: 1 = change the selector state, 0 = keep it; bit 0 then
         * says locked (1) or unlocked (0). */
        const char *selector = (lock & 0x2) == 0
                                   ? "unchanged"
                                   : ((lock & 0x1) ? "locked" : "unlocked");

        g_logged_main_mode = 1;
        fprintf(stderr,
                "dw3-pad: PadSetMainMode port=%d offs=%d lock=%d -> kind=0x%02x "
                "(selector %s)\n",
                idx, offs, lock, (unsigned)g_pad_mode_table[offs], selector);
    }
    return 1;
}

/*
 * DualShock actuator table, consumed by alignActuators (src/engine/pad/port.c):
 * PadInfoAct(id, -1, 0) asks "how many actuators?", then each actuator's
 * InfoActSub drives `PAD.act[i] = sub & 1`. Sub 0 (a Switch port publishes no
 * alignment data) leaves PAD.act idle; setVibration writes the amplitudes the
 * game actually wants. Only analog pads report motors, matching DualShock
 * wire behavior (docs/P10.4_RUMBLE.md).
 */
int PadInfoAct(int port, int actno, int term) {
    static const int func[2] = {0x01, 0x02}; /* InfoActFunc of each motor */
    int idx = port_index(port);
    unsigned char *buf;

    if (idx < 0 || idx > 1 || g_pad_rx[idx] == NULL) {
        return 0;
    }
    buf = g_pad_rx[idx];
    refresh_port(idx);
    if (!buf_connected(buf) || buf[1] != PSYZ_CTRL_ANALOG_PAD) {
        return 0;
    }
    if (actno < 0) {
        /* Term 0 = actuator count probe (-1 == "give me the count"). */
        return 2;
    }
    if (actno > 1) {
        return 0;
    }
    switch (term) {
    case InfoActFunc:
        return func[actno];
    case InfoActSub:
        return 0;
    case InfoActSize:
        return 2; /* PAD.act is 2 bytes per pad */
    case InfoActCurr:
        return PadMaxCurr;
    default:
        return 0;
    }
}

/* Default is an idle weak symbol: hosts without motors (PC sdl3, or unit tests
 * with no Psyz_PadsSetAct stub) stay silent. On the Switch the linker picks
 * the strong Psyz_PadsSetAct in platforms/switch/psyz_backend/nx_rumble.c. */
__attribute__((weak)) void Psyz_PadsSetAct(int port, const unsigned char *act,
                                           int len) {
    (void)port;
    (void)act;
    (void)len;
}

void __wrap_PadSetAct(int port, unsigned char *data, int len) {
    static int logged;
    unsigned char tray[2] = {0x00, 0x00};

    if (data != NULL && len >= 2) {
        memcpy(tray, data, sizeof(tray));
    }
    if (!logged) {
        logged = 1;
        fprintf(stderr, "dw3-pad: PadSetAct port=0x%x act=0x%02x,0x%02x len=%d"
                        " -> Psyz_PadsSetAct\n",
                (unsigned)port, tray[0], tray[1], len);
    }
    Psyz_PadsSetAct(port, tray, (int)sizeof(tray));
}

int __wrap_PadSetActAlign(int port, char *data) {
    static int logged;

    if (!logged && data != NULL) {
        logged = 1;
        fprintf(stderr, "dw3-pad: PadSetActAlign port=0x%x act=0x%02x,0x%02x\n",
                (unsigned)port, (unsigned char)data[0],
                (unsigned char)data[1]);
    }
    return 1;
}

static void dw3_pad_host_debug_buttons(void) {
    unsigned char *buf = g_pad_rx[0];
    unsigned short raw;
    unsigned short pressed;

    if (buf == NULL || !buf_connected(buf) || g_logged_buttons) {
        return;
    }
    raw = ((unsigned short)buf[2] << 8) | (unsigned short)buf[3];
    pressed = (unsigned short)(~raw);
    if (pressed != 0) {
        g_logged_buttons = 1;
        fprintf(stderr, "dw3-pad: buttons pressed mask=0x%04x (raw bytes %02x %02x)\n",
                (unsigned)pressed, buf[2], buf[3]);
    }
}

#ifdef DW3_PAD_TEST_STUB_PSYZ
/* Unit harness: no --wrap; export the real names. */
int PadGetState(int port) { return __wrap_PadGetState(port); }
void PadStartCom(void) { __wrap_PadStartCom(); }
void PadInitMtap(unsigned char *pad1, unsigned char *pad2) {
    __wrap_PadInitMtap(pad1, pad2);
}
void PadSetAct(int port, unsigned char *data, int len) {
    __wrap_PadSetAct(port, data, len);
}
int PadSetActAlign(int port, char *data) { return __wrap_PadSetActAlign(port, data); }
#endif
