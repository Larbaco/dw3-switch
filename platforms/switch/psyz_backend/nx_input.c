/* PSY-Z input backend for the Switch (P4.4): hid/pad -> PS1 pad wire frames.
 *
 * Template: psyz/src/psp/psp_input.c. Only the input *source* lives here — the
 * PSY-Q layer (PadInitDirect / PadGetState / PadRead) is portable and stays in
 * port/psyz_missing/psyz_pad_host.c, which talks to PSY-Z's Psyz_Pads* API.
 *
 * Buttons are mapped by physical position, the convention players expect from a
 * PS1 title on Switch hardware:
 *
 *   Switch A (east)  -> PS1 Circle   (right)   PADRright
 *   Switch B (south) -> PS1 Cross    (bottom)  PADRdown
 *   Switch X (north) -> PS1 Triangle (top)     PADRup
 *   Switch Y (west)  -> PS1 Square   (left)    PADRleft
 */
#include "nx.h"

#include <string.h>

#include <switch.h>

#include <psyz.h>
#include <psyz/input.h>
#include <libetc.h> /* the PAD button defines live here, not in psyz.h */

/* PSY-Z's include/psyz/input.h defines this; kept as a fallback so the file
 * still describes the wire format if that header moves. */
#ifndef PSYZ_PAD_BUF_LEN
#define PSYZ_PAD_BUF_LEN 34
#endif

static PadState s_pad;

void dw3_nx_input_init(void) {
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&s_pad);
}

/* hid buttons -> PSY-Q pad word, active high (the frame inverts below). */
static u_long held_buttons(u64 k) {
    u_long pressed = 0;

    if (k & HidNpadButton_A) {
        pressed |= PADRright;
    }
    if (k & HidNpadButton_B) {
        pressed |= PADRdown;
    }
    if (k & HidNpadButton_X) {
        pressed |= PADRup;
    }
    if (k & HidNpadButton_Y) {
        pressed |= PADRleft;
    }

    if (k & HidNpadButton_Up) {
        pressed |= PADLup;
    }
    if (k & HidNpadButton_Down) {
        pressed |= PADLdown;
    }
    if (k & HidNpadButton_Left) {
        pressed |= PADLleft;
    }
    if (k & HidNpadButton_Right) {
        pressed |= PADLright;
    }

    if (k & HidNpadButton_L) {
        pressed |= PADL1;
    }
    if (k & HidNpadButton_R) {
        pressed |= PADR1;
    }
    if (k & HidNpadButton_ZL) {
        pressed |= PADL2;
    }
    if (k & HidNpadButton_ZR) {
        pressed |= PADR2;
    }
    if (k & HidNpadButton_StickL) {
        pressed |= PADi; /* L3; PSY-Z has no PADL3 alias */
    }
    if (k & HidNpadButton_StickR) {
        pressed |= PADj; /* R3; PSY-Z has no PADR3 alias */
    }

    if (k & HidNpadButton_Plus) {
        pressed |= PADstart;
    }
    if (k & HidNpadButton_Minus) {
        pressed |= PADselect;
    }

    return pressed;
}

/* hid reports a signed stick; PS1 wants 0..0xFF around 0x80, with Y inverted. */
static int stick_axis(int32_t value, int invert) {
    int scaled = 0x80 + (int)(((int64_t)value * 127) / 32767);

    if (invert) {
        scaled = 0x100 - scaled;
    }
    if (scaled < 0) {
        scaled = 0;
    }
    if (scaled > 0xFF) {
        scaled = 0xFF;
    }
    return scaled;
}

void MyPadInit(int mode) {
    (void)mode;
    dw3_nx_input_init();
}

void Psyz_PadsPoll(void) {
    char frame[PSYZ_PAD_BUF_LEN];
    PsyzControllerKind kind;
    HidAnalogStickState stick;
    u_long pressed;

    padUpdate(&s_pad);
    pressed = held_buttons(padGetButtons(&s_pad));
    stick = padGetStickPos(&s_pad, 0);

    /* The game picks the controller kind through PadInfoMode/PadSetMainMode;
     * ask PSY-Z what it wants rather than assuming. */
    kind = Psyz_PadsSetKind(0, 0, PSYZ_CTRL_QUERY_KIND);

    memset(frame, 0, sizeof(frame));
    frame[0] = 0x00;
    frame[1] = (char)kind;
    switch (kind) {
    case PSYZ_CTRL_ANALOG_PAD:
    case PSYZ_CTRL_ANALOG_STICK:
        /* See sdl3_common.h BuildPadFrame: PSY-Q's high byte (d-pad,
         * Start/Select) goes in wire byte 2 and the low byte (face buttons,
         * L1/R1) in wire byte 3, both active low. */
        frame[2] = (char)(~(pressed >> 8) & 0xFF);
        frame[3] = (char)(~pressed & 0xFF);
        /* A Switch controller has one stick: it drives the PS1 left stick and
         * the right one stays centred. */
        frame[4] = (char)0x80;
        frame[5] = (char)0x80;
        frame[6] = (char)stick_axis(stick.x, 0);
        frame[7] = (char)stick_axis(stick.y, 1);
        break;
    case PSYZ_CTRL_DIGITAL_PAD:
        frame[2] = (char)(~(pressed >> 8) & 0xFF);
        frame[3] = (char)(~pressed & 0xFF);
        break;
    case PSYZ_CTRL_DISCONNECTED:
        memset(frame, 0xFF, sizeof(frame));
        break;
    default:
        frame[1] = (char)PSYZ_CTRL_DIGITAL_PAD;
        frame[2] = (char)(~(pressed >> 8) & 0xFF);
        frame[3] = (char)(~pressed & 0xFF);
        break;
    }
    Psyz_PadsSet(0, frame, sizeof(frame));

    /* Only one controller is mapped for now (P4.4); port 1 is disconnected. */
    memset(frame, 0xFF, sizeof(frame));
    Psyz_PadsSet(1, frame, sizeof(frame));
}
