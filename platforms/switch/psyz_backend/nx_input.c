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
#include "nx_battle_mode.h"
#include "nx_stick.h"

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
static u64 s_last_buttons;

void dw3_nx_input_init(void) {
    psyz_log(1, "nx_input.c", __LINE__, __func__, "padConfigureInput");
    /* NpadStandard is Pro (FullKey), a Joy-Con pair in the rails
     * (Handheld), and a Joy-Con pair off the rails (JoyDual).
     * padInitializeDefault reads player 1 and Handheld. */
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

void MyPadInit(int mode) {
    (void)mode;
    dw3_nx_input_init();
}

void Psyz_PadsPoll(void) {
    char frame[PSYZ_PAD_BUF_LEN];
    PsyzControllerKind kind;
    HidAnalogStickState stick;
    u_long pressed;

    /* One-shot: PSY-Z reads pads on every vsync, and the boot stopped between
     * the first video call returning and the vsync callback — which is
     * ReadPadsOnVsync() -> here. If this logs and the next one does not,
     * padUpdate() is what blocks. */
    {
        static int entered;

        if (!entered) {
            entered = 1;
            psyz_log(1, "nx_input.c", __LINE__, __func__, "PadsPoll entered");
        }
    }

    padUpdate(&s_pad);
    {
        static int logged;

        if (!logged) {
            logged = 1;
            psyz_log(1, "nx_input.c", __LINE__, __func__,
                     "padUpdate returned");
        }
    }
    s_last_buttons = padGetButtons(&s_pad);
    pressed = held_buttons(s_last_buttons);
    /* Left stick of whichever style libnx merged into sticks[0]. See
     * nx_stick.h. The d-pad, face buttons, Start and Select stay in
     * `pressed` and are written below even when this stick is centred. */
    stick = padGetStickPos(&s_pad, DW3_NX_LEFT_STICK);

    /* PSY-Z/libapi defaults every port to DIGITAL_PAD. readPadButtons only
     * turns the left stick into d-pad bits for ANALOG_PAD, so a connected
     * Switch pad is published as that kind. PadSetMainMode can still select
     * digital; the next poll puts analog back, because that is the mode the
     * stick needs. Disconnect is not rewritten. */
    kind = Psyz_PadsSetKind(0, 0, PSYZ_CTRL_QUERY_KIND);
    {
        PsyzControllerKind published = dw3_nx_publish_kind(kind);

        if (published != kind) {
            Psyz_PadsSetKind(0, 0, published);
            kind = published;
        }
    }

    memset(frame, 0, sizeof(frame));
    frame[0] = 0x00;
    frame[1] = (char)kind;
    switch (kind) {
    case PSYZ_CTRL_ANALOG_PAD:
        /* See sdl3_common.h BuildPadFrame: PSY-Q's high byte (d-pad,
         * Start/Select) goes in wire byte 2 and the low byte (face buttons,
         * L1/R1) in wire byte 3, both active low. */
        frame[2] = (char)(~(pressed >> 8) & 0xFF);
        frame[3] = (char)(~pressed & 0xFF);
        /* Right stick stays centred. The left stick is the PS1 left stick:
         * wire bytes 6 and 7, which the decomp reads as analog[2] and
         * analog[3]. Y is inverted in dw3_nx_stick_axis. */
        frame[4] = (char)0x80;
        frame[5] = (char)0x80;
        frame[6] = (char)dw3_nx_stick_axis(stick.x, 0);
        frame[7] = (char)dw3_nx_stick_axis(stick.y, 1);
        {
            static int logged_stick;

            /* Wait until libnx reports a style, so this one line is the
             * controller model (Handheld / FullKey / JoyDual) and not the
             * empty poll before the pad is connected. */
            if (!logged_stick && padGetStyleSet(&s_pad) != 0) {
                logged_stick = 1;
                psyz_log(1, "nx_input.c", __LINE__, __func__,
                         "left stick kind=0x%02x style=0x%x handheld=%d "
                         "deadzone=%d",
                         (unsigned)kind, (unsigned)padGetStyleSet(&s_pad),
                         padIsHandheld(&s_pad) ? 1 : 0, DW3_NX_STICK_DEADZONE);
            }
        }
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

    /* FIGHTSTG A/B: Minus+ZL+ZR cycles soft → soft+bbox → gpu-solid → gpu-tex. */
    dw3_nx_battle_mode_poll();
}

uint64_t dw3_nx_input_buttons_raw(void) { return (uint64_t)s_last_buttons; }
