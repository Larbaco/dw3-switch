# Controls and hotkeys

Reference for the native game hosts, audited against `main` on 2026-10-14.
These mappings come from source; they do not establish new hardware acceptance.
The Switch Makefile `host_hello` sample has different controls and is not covered.

## Normal pad controls

Switch face buttons follow physical PS1 positions, not Nintendo's usual A-to-confirm
convention. A **Button layout** option (PlayStation or Nintendo A/B) is a
planned Options row, ROADMAP P9.8; until it exists the table below is the only
layout, and when it lands this table has to be re-audited for both layouts and
no new chord may use A or B. The PC columns describe the PSY-Z SDL3 keyboard map. PC gamepads use
SDL's positional mapping; controller labels and SDL device mappings may vary.

| PS1 logical button | PC keyboard | Switch physical button |
|---|---|---|
| D-pad | Arrow keys | D-pad (left stick also supplies movement) |
| Cross | X | B |
| Circle | D | A |
| Triangle | S | X |
| Square | Z | Y |
| Start | Enter | Plus (+) |
| Select | Backspace | Minus (−) |
| L1 / R1 | Q / R | L / R |
| L2 / R2 | W / E | ZL / ZR |
| L3 / R3 | 1 / 2 | Left / right stick click |

The game decides each button's action in its current scene. For the title and
port options:

- Press Start (Enter / Plus) to reveal the title choices. Up/Down selects a row;
  Cross (X / B) confirms. The Switch GLES and PC SDL3 GPU presentations use
  **New Game / Options / Load Game / Quit game** (EU plaques use the game's
  current-language words). Options opens the Port options shell. **Quit game**
  (P6.28, the last row) opens a Yes / No window with the cursor on No: Cross
  answers, the D-pad moves between Yes and No, Triangle or Circle (PC S or D;
  Switch X or A) cancel. Yes flushes the saves and ends the session: the Switch
  NRO returns to hbmenu, on PC the window closes. HOME still exits as before.
- In Port options, Up/Down changes the selected setting. Cross **or Circle**
  (PC X or D; Switch B or A) toggles it. Triangle (PC S; Switch X) returns to
  the selected title row. Opening the shell does not toggle the first setting.
  On the two volume rows (Music volume, SFX volume) Left / Right step the
  volume down / up from 10 to 0 (a held direction repeats; it stops at the
  ends), and Cross / Circle step up and wrap 10 to 0. See
  [P6.17_VOLUME.md](P6.17_VOLUME.md).
- On the Options screen, **Square** (PC **Z**, Switch **Y**) opens the Cheats
  screen (P6.20) and Triangle returns to Options. The hint "Square/Y Cheats" is
  drawn only once the Cheats master switch is on, so the key is not discoverable
  from the screen; builds made with `--no-cheats` have no such screen. Inside
  it Up/Down selects a row (Down from the last row of page 1, "Cheat saves",
  goes on to page 2), Cross / Circle change it. On page 2 (Money, Warp, Item
  id, Item count, Give item) Left / Right step the row's value, and L1 / R1
  (Switch L / R) step the item id and the count by ten. See
  [PORT_OPTIONS.md](PORT_OPTIONS.md#cheats-p620).
- The field menu (Start in the field) has a row **OPTIONS** (P9.7: OPTIONS,
  OPZIONI, OPTIONEN, OPCIONES) above its last row. Cross on it opens the Options
  screen over the field, with the title's keys (Up / Down choose, Left / Right and
  Cross change, Triangle goes back to the menu). Widescreen and the Cheats entry
  are greyed there. See [PORT_OPTIONS.md](PORT_OPTIONS.md#options-in-game-p97).
- The field menu (Start in the field) has a last row,
  **Return to title** (P6.27: TITLE SCREEN, RETOUR TITRE, TORNA AL TITOLO,
  TITELBILD, VOLVER AL INICIO). Cross on it opens the same Yes / No window, with the
  same keys; Yes runs the restart below (saves flushed, a failed flush cancels). The
  row does not depend on the Restart hotkey setting. See
  [PORT_OPTIONS.md](PORT_OPTIONS.md#quit-and-return-to-title-p627-p628).
- During an opening movie, press or hold Start (Enter / Plus) to skip it.
  A held button is accepted after decoding a frame; this is not a fast-forward
  control.

PC SDL3 also accepts **F4** to toggle fullscreen and **Escape** (or window close)
to quit the host. These are host controls, not PS1 buttons. Save/exit lifecycle
acceptance remains tracked separately in [P5.3_FLAT_SAVE.md](P5.3_FLAT_SAVE.md).

## Port hotkeys

| Action | PS1 logical input | PC keyboard | Switch | Requirement / limit |
|---|---|---|---|---|
| Open Port options directly | Select | Backspace | Minus | Only while the title choices are active; no L1/R1/L2/R2 held |
| Toggle fast-forward on/off | R3 | 2 | Right-stick click | Field, battle and the battle report (results screen) only; configured speed must be 2x, 3x or 4x |
| Restart game in-process | L1 + R1 + Start + Select | Q + R + Enter + Backspace | L + R + Plus + Minus | Enable **Restart hotkey** in Port options first; OFF by default |
| Options from the field menu | Start, then the row above the last, Cross | Enter, ..., X | Plus, ..., B | Port options over the field; Widescreen and Cheats greyed (P9.7) |
| Return to title from the field menu | Start, then the last row, Cross, Yes | Enter, ..., X | Plus, ..., B | Always there; the same reset as the chord, after a Yes / No question (P6.27) |
| Quit game from the title menu | Last title row, Cross, Yes | ..., X | ..., B | Always on the native title; flushes the saves, then hbmenu (Switch) or closes the window (PC) (P6.28) |

Hold all four restart buttons together; one activation is allowed per chord
press. Release the chord before another attempt. There is no timed long-press
requirement. Dirty save slots are flushed before restarting; a flush failure
cancels the restart. Successful restart resets the running game to its title
boot flow without restarting the host. Unsaved gameplay progress is lost.
The opt-in stays in memory through an in-process restart. Since P9.4 it is also
saved in `port_options.cfg` when it is toggled, so a fresh host launch keeps
the choice (OFF until the user enables it once).
See [P6.9_RESTART.md](P6.9_RESTART.md) for implementation and acceptance limits;
Switch chord and save → restart → Continue validation remain open there.

Fast-forward starts inactive at 1x, with a configured R3 speed of 2x. In Port
options, confirming its row cycles **2x → 3x → 4x → OFF → 2x**. OFF disables R3.
A press toggles turbo; release R3 before toggling again. It affects field and
battle modes only; title, movies, separately loaded menus, memory-card screens
and card game remain 1x. Menus inside the field mode can still accelerate.
Music and SFX also speed up because the sequencer ticks per game frame. Actual
speed gain is performance-dependent; OLED throughput is not established by this
document. See [P6.4_FAST_FORWARD.md](P6.4_FAST_FORWARD.md).

Widescreen is a live session toggle without a dedicated button hotkey, ON by
default on current `main` (#132). Supported scenes use 53-pixel gutters (about
16:9); title and movies remain 4:3. EU uses its 240-line picture for the same
aspect rule. See

[PORT_OPTIONS.md](PORT_OPTIONS.md) and [P6.1_WIDESCREEN.md](P6.1_WIDESCREEN.md).

## Diagnostic controls

These controls serve rendering investigation, not normal gameplay features.

| Action | Platform / input | Availability / effect |
|---|---|---|
| Show/hide debug HUD | Switch: Minus + L + R | Requires `DW3_NX_DEBUG_OVERLAY`; defaults compiled in and visible in `--nxlink` builds, off in ordinary builds |
| Cycle battle renderer | Switch: Minus + ZL + ZR | Available even without the HUD; changes FIGHTSTG rendering only |
| Show/hide VRAM view | PC SDL3: F6 | PSY-Z diagnostic view; no Switch equivalent |

Switch diagnostic chords activate when all listed buttons become held; release
before cycling again. Battle mode order is `soft` → `soft+bbox` → `gpu-solid` →
`gpu-tex` → `soft`. `soft+bbox` currently behaves as `soft`. Current `main`
boots in `gpu-tex` (#132); `-DDW3_NX_MESH_SOFT_BATTLE=ON` or
`DW3_MESH_SOFT=1` seeds soft instead. The GPU mode has known sky gaps, and
the P5.2 performance/hardware gate remains open. Field rendering is not
switched by this control. See [DEBUG_OVERLAY.md](DEBUG_OVERLAY.md) and
[P5.2_BATTLE_GPU.md](P5.2_BATTLE_GPU.md). Previously tagged previews retain
their own defaults; these changes do not apply retroactively to preview.24.

**Chord overlap:** the restart chord includes the HUD chord. In a build with
the HUD enabled, L + R + Plus + Minus can also toggle the HUD, even when restart
is disabled or a save flush cancels it. The HUD poll does not exclude Plus.
The title's Select shortcut rejects held shoulders, so it does not open Port
options when shoulders are already held. Pressing Minus first can still open
options before the rest of a chord is held; press shoulders first when forming
these combinations. Raw diagnostic buttons are also published to the game pad;
they are not consumed by a separate input layer.

Developer environment variables are startup/test configuration, not button
hotkeys. For example `DW3_WIDESCREEN=0` disables the default widescreen;
`DW3_MESH_SOFT=0|1` seeds Switch battle mode; `DW3_TURBO=2|3|4` starts turbo
active in eligible modes (a developer override to normal 1x startup).
Test automation such as `DW3_AUTO_PAD` and forced-stage
or forced-save hooks is documented in the corresponding subsystem evidence,
and must not be treated as proof of normal input or hardware acceptance.

## Source references

- [Switch pad map](../platforms/switch/psyz_backend/nx_input.c)
- PC keyboard/gamepad and host keys: pinned PSY-Z
  `vendor/psyz/psyz/src/platform/sdl3_common.h`
- [Title-entry patch](../port/patches/0045-PORT_HOST-p92-title-options-row.patch)
  and [movie-skip patch](../port/patches/0034-PORT_HOST-p37-movie-start-skip-held.patch)
- [Port options state machine](../port/options/port_options.c) and
  [restart policy](../port/options/restart.c)
- [HUD polling](../platforms/switch/psyz_backend/nx_debug_overlay.c) and
  [battle-mode polling](../platforms/switch/psyz_backend/nx_battle_mode.c)
