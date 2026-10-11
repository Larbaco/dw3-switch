#ifndef DW3_NX_STICK_H
#define DW3_NX_STICK_H

#include <psyz/input.h>

/*
 * Pure left-stick mapping for the Switch host. No libnx types: nx_input.c
 * passes the s32 pair libnx already stored, and the pad host test compiles
 * this header on its own.
 *
 * libnx padUpdate (nx/source/runtime/pad.c) copies analog_stick_l into
 * PadState.sticks[0] for every style it merged:
 *   HidNpadStyleTag_NpadHandheld  Joy-Con pair in the rails
 *   HidNpadStyleTag_NpadFullKey   Pro Controller
 *   HidNpadStyleTag_NpadJoyDual   Joy-Con pair, not in the rails
 * Handheld is sampled first. A stick that is not exactly (0, 0) is kept, so
 * a later Pro Controller does not replace a Joy-Con stick that has already
 * moved. Index 1 is the right stick. Field movement uses the left one only.
 */
#define DW3_NX_LEFT_STICK 0

/* Calibrated libnx samples use this signed full-scale magnitude.
 * JOYSTICK_MAX is 0x8000; the axis value that reaches it is 32767. */
#define DW3_NX_STICK_AXIS_MAX 32767

/*
 * |raw| below this publishes as PS1 centre 0x80.
 * 8192 / 32767 is 25% of full scale. The decomp still ignores a byte inside
 * (STICK_LOW, STICK_HIGH) = (0x40, 0xC0), which is about ±16512 raw counts
 * (50%). Drift under 8192 cannot walk. A tilt that only just leaves this
 * radius is still inside the decomp window, so walking starts at the same
 * PS1 thresholds as a DualShock left stick.
 */
#define DW3_NX_STICK_DEADZONE 8192

/* libnx Y is positive upward. PS1 up is a smaller byte (analog[3] <= 0x40). */
static inline int dw3_nx_stick_axis(int value, int invert) {
    int scaled;

    if (value > -DW3_NX_STICK_DEADZONE && value < DW3_NX_STICK_DEADZONE) {
        return 0x80;
    }
    scaled = 0x80 + (int)(((long long)value * 127) / DW3_NX_STICK_AXIS_MAX);
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

/*
 * The decomp's readPadButtons only copies stick bytes when
 * PadInfoMode(InfoModeCurExID) is PAD_ID_ANALOG (7). That is the high
 * nibble of PSYZ_CTRL_ANALOG_PAD (0x73). PSY-Z defaults every port to
 * PSYZ_CTRL_DIGITAL_PAD (0x41), and PSYZ_CTRL_ANALOG_STICK (0x53) has high
 * nibble 5, which the decomp ignores. Both become ANALOG_PAD. Disconnect
 * and any other kind are left alone.
 */
static inline PsyzControllerKind dw3_nx_publish_kind(PsyzControllerKind kind) {
    if (kind == PSYZ_CTRL_DIGITAL_PAD || kind == PSYZ_CTRL_ANALOG_STICK) {
        return PSYZ_CTRL_ANALOG_PAD;
    }
    return kind;
}

#endif
