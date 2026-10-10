/*
 * Headless check: PadInitDirect + injected Psyz frame => PadGetState connected
 * and Start bit visible in the receive buffer (game pad path prerequisites).
 *
 * Does not open a window; links psyz_pad_host.c against a tiny Psyz_Pads stub
 * when built by tools/tests/test_pad_host.py, OR against real PSY-Z via
 * platforms/pc (see that test's gcc line).
 */
#include <libpad.h>
#include <psyz/input.h>
#include <stdio.h>
#include <string.h>

enum { BUF_LEN = 34 };

static char g_frames[2][BUF_LEN];

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
    (void)port;
    (void)channel;
    (void)kind;
    return PSYZ_CTRL_DIGITAL_PAD;
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

static void make_digital_frame(char *frame, unsigned pressed_mask) {
    memset(frame, 0, BUF_LEN);
    frame[0] = 0x00;
    frame[1] = (char)PSYZ_CTRL_DIGITAL_PAD;
    /* Same packing as PSY-Z BuildPadFrame / UnpackPadBits. */
    frame[2] = (char)((~(pressed_mask >> 8)) & 0xFF);
    frame[3] = (char)((~pressed_mask) & 0xFF);
}

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

    PadInitDirect(buf0, buf1);
    PadStartCom();

    /* Disconnected until a valid frame is injected / polled. */
    state = PadGetState(0);
    if (state != PadStateDiscon) {
        /* After InitPAD memset + empty stub frames, may still be discon. OK either way. */
    }

    make_digital_frame(frame, PADstart);
    Psyz_PadsSet(0, frame, BUF_LEN);

    state = PadGetState(0);
    if (state == PadStateDiscon) {
        fprintf(stderr, "FAIL: PadGetState still Discon after connected frame\n");
        return 1;
    }
    mode = PadInfoMode(0, InfoModeCurExID, 0);
    if (mode != 4) { /* PAD_ID_DIGITAL */
        fprintf(stderr, "FAIL: PadInfoMode CurExID=%d want 4\n", mode);
        return 1;
    }
    if (buf0[0] != 0x00 || buf0[1] != (unsigned char)PSYZ_CTRL_DIGITAL_PAD) {
        fprintf(stderr, "FAIL: buf status/id %02x %02x\n", buf0[0], buf0[1]);
        return 1;
    }
    /* PSY-Z UnpackPadBits: (buf[2]<<8)|buf[3], then invert. */
    raw = ((unsigned short)buf0[2] << 8) | (unsigned short)buf0[3];
    pressed = (unsigned short)(~raw);
    if ((pressed & PADstart) == 0) {
        fprintf(stderr, "FAIL: Start not set in buffer (pressed=0x%04x)\n",
                (unsigned)pressed);
        return 1;
    }
    if (PadChkVsync() != 1) {
        fprintf(stderr, "FAIL: PadChkVsync expected 1 after StartCom\n");
        return 1;
    }

    printf("OK: PadGetState=%d CurExID=%d pressed=0x%04x (Start)\n", state, mode,
           (unsigned)pressed);
    return 0;
}
