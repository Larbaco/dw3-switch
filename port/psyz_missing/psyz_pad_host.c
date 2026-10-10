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

/* PSY-Z libapi (avoid libapi.h type clashes with decomp headers). */
int InitPAD(char *bufA, int lenA, char *bufB, int lenB);
long StartPAD(void);

enum { DW3_PAD_BUF_LEN = 34 };

static unsigned char *g_pad_rx[2];
static int g_pad_started;
static int g_logged_connected;
static int g_logged_buttons;

static int port_index(int port) { return (port >> 4) & 1; }

static void dw3_pad_host_debug_buttons(void);
static void dw3_auto_pad_inject(void);

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

static void dw3_auto_pad_inject(void) {
    const char *env;
    unsigned mask = 0;
    static unsigned frame;
    char buf[DW3_PAD_BUF_LEN];
    int confirm = 0;
    int walk = 0;
    int start = 0;
    int cancel = 0;

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
    if (!confirm && !walk && !start && !cancel) {
        return;
    }
    frame++;
    if (walk) {
        mask |= DW3_RAW_UP;
    }
    /* Pulse buttons so PAD_PRESSED sees edges for dialogue / menus. */
    if (confirm && (frame % 8u) == 1u) {
        mask |= DW3_RAW_CROSS;
    }
    if (start && frame >= 90u && frame < 98u) {
        mask |= DW3_RAW_START;
    }
    if (cancel && frame >= 240u && (frame % 16u) == 1u) {
        mask |= DW3_RAW_TRIANGLE;
    }
    memset(buf, 0, sizeof(buf));
    buf[0] = 0x00;
    buf[1] = (char)PSYZ_CTRL_DIGITAL_PAD;
    buf[2] = (char)((~(mask >> 8)) & 0xFF);
    buf[3] = (char)((~mask) & 0xFF);
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
    dw3_auto_pad_inject();
    refresh_port(0);
    refresh_port(1);
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

    (void)offs;
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
        return 0;
    default:
        return 0;
    }
}

int PadSetMainMode(int port, int offs, int lock) {
    (void)port;
    (void)offs;
    (void)lock;
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
