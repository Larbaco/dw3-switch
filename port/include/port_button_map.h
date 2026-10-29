/* P9.8 confirm / cancel layout (docs/CONTROLS_BUNDLE.md): the port decides which
 * physical button produces each PS1 button. The map is a permutation of the ten
 * remappable PS buttons, applied to the raw pad word at the pad host
 * (port/psyz_missing/psyz_pad_host.c), before the engine reads a bit.
 *
 * Two presets (PlayStation = the identity, Nintendo) and, since phase 2
 * (P6.12), a custom map the Customize screen builds one button at a time
 * (button_layout=custom + button_map=<ten digits> in port_options.cfg).
 *
 * "PS button" i is the hardware button the game sees (a raw bit of the pad word);
 * "source" j is the position the platform reported it at (positional: Switch A
 * is the Circle position, B the Cross position, X Triangle, Y Square). */
#ifndef DW3_PORT_BUTTON_MAP_H
#define DW3_PORT_BUTTON_MAP_H

#ifdef __cplusplus
extern "C" {
#endif

/* The remappable buttons, in the order of the map digits. */
enum {
    DW3_BTN_CROSS = 0,
    DW3_BTN_CIRCLE = 1,
    DW3_BTN_TRIANGLE = 2,
    DW3_BTN_SQUARE = 3,
    DW3_BTN_L1 = 4,
    DW3_BTN_R1 = 5,
    DW3_BTN_L2 = 6,
    DW3_BTN_R2 = 7,
    DW3_BTN_L3 = 8,
    DW3_BTN_R3 = 9,
    DW3_BTN_COUNT = 10
};

enum {
    DW3_LAYOUT_PLAYSTATION = 0,
    DW3_LAYOUT_NINTENDO = 1,
    DW3_LAYOUT_CUSTOM = 2,   /* P6.12: whatever map the Customize screen made */
    DW3_LAYOUT_COUNT = 3
};

/* --- pure policy (no state; unit tested) ------------------------------- */

/* The map of a preset: out[i] = the source that produces PS button i.
 * Returns 0, or -1 for an unknown layout. */
/* A preset has a fixed map; DW3_LAYOUT_CUSTOM has none (-1). */
int dw3_button_map_preset(int layout, unsigned char out[DW3_BTN_COUNT]);
/* 1 when `m` is a permutation of 0..9. */
int dw3_button_map_valid(const unsigned char m[DW3_BTN_COUNT]);
/* The cfg digits ("0123456789") of a map and back. parse returns 0, or -1 when
 * the text is not ten digits forming a permutation. */
void dw3_button_map_format(const unsigned char m[DW3_BTN_COUNT],
                           char out[DW3_BTN_COUNT + 1]);
int dw3_button_map_parse(const char *text, unsigned char out[DW3_BTN_COUNT]);
/* Permute a raw pad word (16 bits, a bit is CLEAR while its button is pressed,
 * the PadData.buttons of the engine). Start, Select and the d-pad pass
 * through. */
unsigned dw3_button_map_permute(const unsigned char m[DW3_BTN_COUNT],
                                unsigned word);
/* The raw bit of a PS button / source (the same numbering for both). */
int dw3_button_raw_bit(int button);

/* "playstation" / "nintendo" / "custom"; parse returns the layout, or -1. */
const char *dw3_button_layout_name(int layout);
int dw3_button_layout_parse(const char *text);
/* Log words: "cross" ... "r3" for a PS button; "B" "A" "X" "Y" "L" "R" "ZL"
 * "ZR" "StickL" "StickR" for the Switch button at a source position. */
const char *dw3_button_ps_name(int button);
const char *dw3_button_source_name(int source);
/* Longer words for the hotkey line: "R3", "Right stick click". */
const char *dw3_button_ps_label(int button);
const char *dw3_button_source_label(int source);

/* --- state (one map per process; the pad host reads it every frame) --- */

/* Set the layout at boot (`source`: "cfg", "default" or "env") or from the
 * Options row (`how`: "screen"). Logs `dw3-controls: map loaded ...` or
 * `layout changed ...`, the map and the hotkeys, once per call. */
void dw3_button_map_load(int layout, const char *source);
/* Boot with a custom map (button_layout=custom, button_map=...). */
void dw3_button_map_load_custom(const unsigned char m[DW3_BTN_COUNT],
                                const char *source);
/* Switch layout. A preset replaces the map; DW3_LAYOUT_CUSTOM keeps the map in
 * force and only names it custom. */
void dw3_button_map_change(int layout, const char *how);
/* P6.12 Customize: PS button `button` now comes from `source`; the PS button
 * that had that source gets the old one (the map stays a permutation) and the
 * layout becomes custom. The buttons held at that moment are read as released
 * until they are up (the press that assigned must not act). Returns the PS
 * button that was swapped in, `button` itself when nothing changed, or -1 for a
 * bad argument. */
int dw3_button_map_assign(int button, int source);
/* Back to the PlayStation identity (layout playstation), logged. */
void dw3_button_map_reset(const char *how);
/* Press-to-assign (the prompt of the Customize screen). The pad host hands the
 * raw word before the map to dw3_button_map_apply; begin() remembers which
 * sources are down so the press that opened the prompt is not taken, and poll()
 * returns the first source pressed since (0..9), DW3_CAPTURE_CANCEL when Start
 * or Select was pressed, or DW3_CAPTURE_NONE. */
enum { DW3_CAPTURE_NONE = -1, DW3_CAPTURE_CANCEL = -2 };
void dw3_button_map_capture_begin(void);
int dw3_button_map_capture_poll(void);
/* The pure part of the above, for the host tests: `raw_word` is the pad word
 * (a clear bit = pressed), `*ignore` the raw bits held since the prompt opened
 * (updated: a released button leaves it). */
int dw3_button_map_capture_step(unsigned raw_word, unsigned *ignore);
/* The raw word of the last frame as the pad host reported it, before the map
 * and the swallow (0xFFFF = nothing pressed). */
unsigned dw3_button_map_source_word(void);
int dw3_button_map_layout(void);
/* The map in force (identity until loaded). */
const unsigned char *dw3_button_map_current(void);
/* Pad host hook: the raw word of port 1 through the map. Logs the first
 * confirm press after boot and after every layout change. */
unsigned dw3_button_map_apply(unsigned word);
/* One log line: a stored value was not valid and the default applies. */
void dw3_button_map_log_invalid(const char *reason);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_BUTTON_MAP_H */
