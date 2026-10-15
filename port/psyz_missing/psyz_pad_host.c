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

/* PSY-Z libapi (avoid libapi.h type clashes with decomp headers). */
int InitPAD(char *bufA, int lenA, char *bufB, int lenB);
long StartPAD(void);

enum { DW3_PAD_BUF_LEN = 34 };

static unsigned char *g_pad_rx[2];
static int g_pad_started;
static int g_logged_connected;
static int g_logged_buttons;
static int g_logged_main_mode;

static int port_index(int port) { return (port >> 4) & 1; }

static void dw3_pad_host_debug_buttons(void);
static void dw3_auto_pad_inject(void);
static void dw3_pad_publish_mask(unsigned mask);
static void dw3_replay_vsync(void);
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
 * File format (text, one directive per line, '#' comments):
 *   pad  <frame> <hexmask>   from this frame on, hold this raw mask
 *   shot <frame> <name>      checkpoint: call dw3_pad_host_shot_cb(name)
 *   end  <frame>             exit(0) at this frame
 * Masks use the DW3_RAW_* bits (1 = pressed). The shot callback is set by the
 * host (platforms/pc/src/main.c writes a PPM); the pad layer holds no video
 * code. See docs/P6.15_REPLAY_TESTS.md.
 */
void (*dw3_pad_host_shot_cb)(const char *name, unsigned frame);

enum { DW3_REPLAY_MAX = 4096 };
typedef struct {
    unsigned frame;
    unsigned mask;
    char name[48];
} Dw3ReplayEv;

static Dw3ReplayEv g_rp_pad[DW3_REPLAY_MAX];
static Dw3ReplayEv g_rp_shot[DW3_REPLAY_MAX];
static int g_rp_npad, g_rp_nshot, g_rp_pad_i, g_rp_shot_i;
static unsigned g_rp_end;
static int g_rp_state; /* 0 untried, 1 replay, 2 record, 3 off */
static unsigned g_rp_frame;
static unsigned g_rp_mask;
static unsigned g_rec_last = 0xFFFFFFFFu;
static FILE *g_rec_fp;

static int replay_load(const char *path) {
    FILE *fp = fopen(path, "r");
    char line[160];

    if (fp == NULL) {
        fprintf(stderr, "dw3-replay: cannot open %s\n", path);
        return -1;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        unsigned f, m;
        char name[48];

        if (sscanf(line, " pad %u %x", &f, &m) == 2 && g_rp_npad < DW3_REPLAY_MAX) {
            g_rp_pad[g_rp_npad].frame = f;
            g_rp_pad[g_rp_npad++].mask = m;
        } else if (sscanf(line, " shot %u %47s", &f, name) == 2 &&
                   g_rp_nshot < DW3_REPLAY_MAX) {
            g_rp_shot[g_rp_nshot].frame = f;
            strcpy(g_rp_shot[g_rp_nshot++].name, name);
        } else if (sscanf(line, " end %u", &f) == 1) {
            g_rp_end = f;
        }
    }
    fclose(fp);
    fprintf(stderr, "dw3-replay: loaded %s pad=%d shot=%d end=%u\n", path,
            g_rp_npad, g_rp_nshot, g_rp_end);
    return 0;
}

static void dw3_replay_vsync(void) {
    const char *play;
    const char *rec;

    if (g_rp_state == 3) {
        return;
    }
    if (g_rp_state == 0) {
        play = getenv("DW3_REPLAY");
        rec = getenv("DW3_REPLAY_RECORD");
        g_rp_state = 3;
        if (play != NULL && play[0] != '\0') {
            if (replay_load(play) == 0) {
                g_rp_state = 1;
            }
        } else if (rec != NULL && rec[0] != '\0') {
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
        if (g_rp_state == 3) {
            return;
        }
    }
    g_rp_frame++;
    if (g_rp_state == 1) {
        while (g_rp_shot_i < g_rp_nshot &&
               g_rp_shot[g_rp_shot_i].frame <= g_rp_frame) {
            if (dw3_pad_host_shot_cb != NULL) {
                dw3_pad_host_shot_cb(g_rp_shot[g_rp_shot_i].name, g_rp_frame);
            }
            g_rp_shot_i++;
        }
        if (g_rp_end != 0 && g_rp_frame >= g_rp_end) {
            fprintf(stderr, "dw3-replay: end at frame %u\n", g_rp_frame);
            fflush(stderr);
            exit(0);
        }
        while (g_rp_pad_i < g_rp_npad &&
               g_rp_pad[g_rp_pad_i].frame <= g_rp_frame) {
            g_rp_mask = g_rp_pad[g_rp_pad_i++].mask;
        }
        dw3_pad_publish_mask(g_rp_mask);
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

static void refresh_port(int idx) {
    if (idx < 0 || idx > 1 || g_pad_rx[idx] == NULL) {
        return;
    }
    Psyz_PadsGet(idx, (char *)g_pad_rx[idx], DW3_PAD_BUF_LEN);
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
    return PadStateFindCTP1;
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

int PadInfoAct(int port, int actno, int term) {
    (void)port;
    (void)actno;
    (void)term;
    return 0;
}

void __wrap_PadSetAct(int port, unsigned char *data, int len) {
    (void)port;
    (void)data;
    (void)len;
}

int __wrap_PadSetActAlign(int port, char *data) {
    (void)port;
    (void)data;
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
