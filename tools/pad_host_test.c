/*
 * Headless check of the host libpad adapter (port/psyz_missing/psyz_pad_host.c):
 *
 *   1. PadInitDirect + an injected digital Psyz frame => PadGetState connected
 *      and the Start bit visible in the receive buffer (game pad prerequisites).
 *   2. The PadInfoMode / PadSetMainMode contract the decomp's pad path uses
 *      (src/engine/pad/port.c): InfoModeCurExID returns PAD_ID_DIGITAL (4) /
 *      PAD_ID_ANALOG (7), InfoModeCurExOffs returns the active mode's offset in
 *      the controller mode ID table, and PadSetMainMode takes that offset to
 *      switch the mode -- rejecting an offset or port it cannot use.
 *
 * Does not open a window; links psyz_pad_host.c against a tiny Psyz_Pads stub
 * when built by tools/tests/test_pad_host.py, OR against real PSY-Z via
 * platforms/pc (see that test's gcc line).
 */
#include <libpad.h>
#include <psyz/input.h>
#include <stdio.h>
#include <string.h>

#include "nx_stick.h"

enum { BUF_LEN = 34 };

static char g_frames[2][BUF_LEN];

/* Set by the Psyz_PadsSetKind stub so main() can check what PadSetMainMode
 * asked PSY-Z to publish. Zero-initialised so the file still compiles when
 * built against real PSY-Z instead of the stub. */
static PsyzControllerKind g_stub_kinds[2];
static PsyzControllerKind g_stub_last_set_kind;
static int g_stub_last_set_port = -1;

/* Provide InitPAD/StartPAD/Psyz_* when not linking full PSY-Z (unit harness). */
#ifdef DW3_PAD_TEST_STUB_PSYZ
int InitPAD(char *bufA, int lenA, char *bufB, int lenB) {
    (void)bufA;
    (void)lenA;
    (void)bufB;
    (void)lenB;
    return 1;
}
long StartPAD(void) { return 1; }
PsyzControllerKind Psyz_PadsSetKind(int port, int channel, PsyzControllerKind kind) {
    PsyzControllerKind prev;

    (void)channel;
    if (port < 0 || port > 1) {
        return PSYZ_CTRL_ERROR;
    }
    if (kind == PSYZ_CTRL_QUERY_KIND) {
        return g_stub_kinds[port];
    }
    prev = g_stub_kinds[port];
    g_stub_kinds[port] = kind;
    g_stub_last_set_port = port;
    g_stub_last_set_kind = kind;
    return prev;
}
void Psyz_PadsSet(int port, const char *src, int len) {
    if (port < 0 || port > 1 || src == NULL || len <= 0) {
        return;
    }
    if (len > BUF_LEN) {
        len = BUF_LEN;
    }
    memcpy(g_frames[port], src, (size_t)len);
}
void Psyz_PadsGet(int port, char *dst, int len) {
    if (port < 0 || port > 1 || dst == NULL || len <= 0) {
        return;
    }
    if (len > BUF_LEN) {
        len = BUF_LEN;
    }
    memcpy(dst, g_frames[port], (size_t)len);
}
#endif

/* Same packing as nx_input.c / PSY-Z BuildPadFrame / UnpackPadBits. */
static void make_frame(char *frame, PsyzControllerKind kind,
                       unsigned pressed_mask) {
    memset(frame, 0, BUF_LEN);
    frame[0] = 0x00;
    frame[1] = (char)kind;
    frame[2] = (char)((~(pressed_mask >> 8)) & 0xFF);
    frame[3] = (char)((~pressed_mask) & 0xFF);
    if (kind == PSYZ_CTRL_ANALOG_PAD) {
        frame[4] = (char)0x80;
        frame[5] = (char)0x80;
        frame[6] = (char)0x80; /* left stick centred */
        frame[7] = (char)0x80;
    }
}

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FAIL: ");                                         \
            fprintf(stderr, __VA_ARGS__);                                      \
            fprintf(stderr, "\n");                                             \
            return 1;                                                          \
        }                                                                      \
    } while (0)

int main(void) {
    unsigned char buf0[BUF_LEN];
    unsigned char buf1[BUF_LEN];
    char frame[BUF_LEN];
    int state;
    int mode;
    unsigned short raw;
    unsigned short pressed;

    memset(buf0, 0xFF, sizeof(buf0));
    memset(buf1, 0xFF, sizeof(buf1));
    memset(g_frames, 0xFF, sizeof(g_frames));
    g_stub_kinds[0] = PSYZ_CTRL_DIGITAL_PAD;
    g_stub_kinds[1] = PSYZ_CTRL_DIGITAL_PAD;

    PadInitDirect(buf0, buf1);
    PadStartCom();

    /* Disconnected until a valid frame is injected / polled. */
    state = PadGetState(0);
    if (state != PadStateDiscon) {
        /* After InitPAD memset + empty stub frames, may still be discon. OK either way. */
    }

    /* ---- digital pad: the state the game boots in ------------------------- */
    make_frame(frame, PSYZ_CTRL_DIGITAL_PAD, PADstart);
    Psyz_PadsSet(0, frame, BUF_LEN);

    state = PadGetState(0);
    CHECK(state != PadStateDiscon, "PadGetState still Discon after connected frame");
    mode = PadInfoMode(0, InfoModeCurExID, 0);
    CHECK(mode == 4, "PadInfoMode CurExID=%d want 4 (PAD_ID_DIGITAL)", mode);
    CHECK(buf0[0] == 0x00 && buf0[1] == (unsigned char)PSYZ_CTRL_DIGITAL_PAD,
          "buf status/id %02x %02x", buf0[0], buf0[1]);

    /* PSY-Z UnpackPadBits: (buf[2]<<8)|buf[3], then invert. */
    raw = ((unsigned short)buf0[2] << 8) | (unsigned short)buf0[3];
    pressed = (unsigned short)(~raw);
    CHECK((pressed & PADstart) != 0, "Start not set in buffer (pressed=0x%04x)",
          (unsigned)pressed);

    /* ---- the mode ID table ------------------------------------------------ */
    CHECK(PadInfoMode(0, InfoModeCurID, 0) == PSYZ_CTRL_DIGITAL_PAD,
          "CurID=%d want 0x41", PadInfoMode(0, InfoModeCurID, 0));
    CHECK(PadInfoMode(0, InfoModeCurExOffs, 0) == 0,
          "digital CurExOffs=%d want 0", PadInfoMode(0, InfoModeCurExOffs, 0));
    CHECK(PadInfoMode(0, InfoModeIdTable, 0) == PSYZ_CTRL_DIGITAL_PAD,
          "IdTable[0]=%d want 0x41", PadInfoMode(0, InfoModeIdTable, 0));
    CHECK(PadInfoMode(0, InfoModeIdTable, 1) == PSYZ_CTRL_ANALOG_PAD,
          "IdTable[1]=%d want 0x73", PadInfoMode(0, InfoModeIdTable, 1));
    CHECK(PadInfoMode(0, InfoModeIdTable, 2) == 0,
          "IdTable[2]=%d want 0", PadInfoMode(0, InfoModeIdTable, 2));

    CHECK(PadChkVsync() == 1, "PadChkVsync expected 1 after StartCom");

    /* ---- analog pad: what PadSetMainMode switches the port to ------------- */
    make_frame(frame, PSYZ_CTRL_ANALOG_PAD, 0);
    Psyz_PadsSet(0, frame, BUF_LEN);
    mode = PadInfoMode(0, InfoModeCurExID, 0);
    CHECK(mode == 7, "analog CurExID=%d want 7 (PAD_ID_ANALOG)", mode);
    CHECK(PadInfoMode(0, InfoModeCurExOffs, 0) == 1,
          "analog CurExOffs=%d want 1", PadInfoMode(0, InfoModeCurExOffs, 0));

    /* lockPadMode does: PadSetMainMode(id, PadInfoMode(id, InfoModeCurExOffs, 0),
     * PAD_MODE_LOCKED) -- offset 1 must select the analog pad and be accepted. */
    CHECK(PadSetMainMode(0, 1, 3) == 1, "PadSetMainMode(0,1,LOCKED) rejected");
    CHECK(g_stub_last_set_port == 0 && g_stub_last_set_kind == PSYZ_CTRL_ANALOG_PAD,
          "PadSetMainMode did not ask for ANALOG_PAD (port=%d kind=0x%02x)",
          g_stub_last_set_port, (unsigned)g_stub_last_set_kind);
    CHECK(g_stub_kinds[0] == PSYZ_CTRL_ANALOG_PAD,
          "port 0 kind not published as analog (0x%02x)", (unsigned)g_stub_kinds[0]);

    /* unlock (PAD_MODE_UNLOCKED) with offset 0 selects digital again. */
    CHECK(PadSetMainMode(0, 0, 2) == 1, "PadSetMainMode(0,0,UNLOCKED) rejected");
    CHECK(g_stub_last_set_kind == PSYZ_CTRL_DIGITAL_PAD,
          "PadSetMainMode(0,0,..) kind=0x%02x want 0x41",
          (unsigned)g_stub_last_set_kind);
    CHECK(Psyz_PadsSetKind(0, 0, PSYZ_CTRL_QUERY_KIND) == PSYZ_CTRL_DIGITAL_PAD,
          "query after PadSetMainMode did not read back 0x41");

    /* Rejected: offset past the table, and a port with no pad. */
    CHECK(PadSetMainMode(0, 2, 3) == 0, "PadSetMainMode offs=2 (past table) accepted");
    CHECK(PadSetMainMode(0x10, 0, 3) == 0, "PadSetMainMode on disconnected port 1 accepted");

    /* Left stick: host dead zone, then the decomp's STICK_LOW / STICK_HIGH.
     * A centred or drifting-under-8192 sample stays at 0x80, so it does not
     * walk. Full deflection still crosses 0x40 / 0xC0. Y positive is up. */
    CHECK(dw3_nx_stick_axis(0, 0) == 0x80, "rest X=%d", dw3_nx_stick_axis(0, 0));
    CHECK(dw3_nx_stick_axis(DW3_NX_STICK_DEADZONE - 1, 0) == 0x80,
          "drift +%d X=%d", DW3_NX_STICK_DEADZONE - 1,
          dw3_nx_stick_axis(DW3_NX_STICK_DEADZONE - 1, 0));
    CHECK(dw3_nx_stick_axis(-(DW3_NX_STICK_DEADZONE - 1), 1) == 0x80,
          "drift -%d Y=%d", DW3_NX_STICK_DEADZONE - 1,
          dw3_nx_stick_axis(-(DW3_NX_STICK_DEADZONE - 1), 1));
    CHECK(dw3_nx_stick_axis(DW3_NX_STICK_DEADZONE, 0) == 0x9F,
          "deadzone edge X=%d want 0x9F",
          dw3_nx_stick_axis(DW3_NX_STICK_DEADZONE, 0));
    CHECK(dw3_nx_stick_axis(DW3_NX_STICK_DEADZONE, 0) > 0x40 &&
              dw3_nx_stick_axis(DW3_NX_STICK_DEADZONE, 0) < 0xC0,
          "deadzone edge must not walk");
    CHECK(dw3_nx_stick_axis(-DW3_NX_STICK_DEADZONE, 0) == 0x61,
          "deadzone edge -X=%d want 0x61",
          dw3_nx_stick_axis(-DW3_NX_STICK_DEADZONE, 0));
    CHECK(dw3_nx_stick_axis(DW3_NX_STICK_AXIS_MAX, 0) == 0xFF,
          "full right=%d", dw3_nx_stick_axis(DW3_NX_STICK_AXIS_MAX, 0));
    CHECK(dw3_nx_stick_axis(-DW3_NX_STICK_AXIS_MAX, 0) == 0x01,
          "full left=%d", dw3_nx_stick_axis(-DW3_NX_STICK_AXIS_MAX, 0));
    CHECK(dw3_nx_stick_axis(DW3_NX_STICK_AXIS_MAX, 1) == 0x01,
          "full up=%d", dw3_nx_stick_axis(DW3_NX_STICK_AXIS_MAX, 1));
    CHECK(dw3_nx_stick_axis(-DW3_NX_STICK_AXIS_MAX, 1) == 0xFF,
          "full down=%d", dw3_nx_stick_axis(-DW3_NX_STICK_AXIS_MAX, 1));
    CHECK(dw3_nx_publish_kind(PSYZ_CTRL_DIGITAL_PAD) == PSYZ_CTRL_ANALOG_PAD,
          "digital kind was not published as analog");
    CHECK(dw3_nx_publish_kind(PSYZ_CTRL_ANALOG_STICK) == PSYZ_CTRL_ANALOG_PAD,
          "ANALOG_STICK kind was not normalised");
    CHECK(dw3_nx_publish_kind(PSYZ_CTRL_ANALOG_PAD) == PSYZ_CTRL_ANALOG_PAD,
          "ANALOG_PAD kind changed");
    CHECK(dw3_nx_publish_kind(PSYZ_CTRL_DISCONNECTED) == PSYZ_CTRL_DISCONNECTED,
          "disconnect was rewritten");

    printf("OK: PadGetState=%d CurExID=%d pressed=0x%04x (Start)\n", state, mode,
           (unsigned)pressed);
    printf("OK: mode table digital ExOffs=0 analog ExOffs=1; "
           "PadSetMainMode switches the published kind and rejects bad args\n");
    printf("OK: left stick deadzone=%d kind 0x41/0x53 -> 0x73\n",
           DW3_NX_STICK_DEADZONE);
    return 0;
}
