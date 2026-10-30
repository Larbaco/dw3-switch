# Controls and hotkeys

Reference for the native game hosts, audited against `main` on 2026-10-14.
These mappings come from source; they do not establish new hardware acceptance.
The Switch Makefile `host_hello` sample has different controls and is not covered.

## Normal pad controls

By default the Switch face buttons follow physical PS1 positions, not Nintendo's
usual A-to-confirm convention. **Options, Controls, Button layout** (P9.8, built
2026-10-28) chooses between two layouts; the default, **PlayStation**, is the
table's first column and is bit-identical to the game before the option existed.
**Nintendo** makes A confirm and B cancel (the game cancels with Triangle, so B
produces Triangle, not Circle; see [CONTROLS_BUNDLE](CONTROLS_BUNDLE.md#22-the-confirm--cancel-layout-p98-is-a-preset-of-the-map)).
The layout is stored per region in `port_options.cfg` (`button_layout=playstation|nintendo|custom`),
applies at once, and works the same on US, EU and the combined build. No new
chord may use Cross or Circle, whichever physical button they are on. The PC columns
describe the PSY-Z SDL3 keyboard map; PC gamepads use SDL's positional mapping;
controller labels and SDL device mappings may vary.

| PS1 logical button | PC keyboard (PlayStation / Nintendo) | Switch, PlayStation layout (default) | Switch, Nintendo layout |
|---|---|---|---|
| D-pad | Arrow keys | D-pad (left stick also supplies movement) | same |
| Cross (confirm) | X / D | B | **A** |
| Circle (secondary: shop trade, card picks, Options also confirms) | D / S | A | **X** |
| Triangle (cancel, back) | S / X | X | **B** |
| Square | Z / Z | Y | Y |
| Start | Enter | Plus (+) | same |
| Select | Backspace | Minus (−) | same |
| L1 / R1 | Q / R | L / R | same |
| L2 / R2 | W / E | ZL / ZR | same |
| L3 / R3 | 1 / 2 | Left / right stick click | same |

Only the three face buttons A, B and X move in the Nintendo layout; Y, the
shoulders, the triggers, the sticks, Plus and Minus stay. With Nintendo on a PC
keyboard the key that produced Circle (D) confirms and the key that produced
Cross (X) cancels: the map acts on the PS button each key produces, and the
keyboard table itself is PSY-Z's.

**Custom (P6.12, built 2026-10-29).** **Options, Controls, Customize buttons**
lists the ten remappable PS buttons (Cross, Circle, Triangle, Square, L1, R1, L2,
R2, L3, R3), each with the console button that produces it now. Confirm on a row
waits for a press (Start or Select cancel, the d-pad is ignored, it gives up after
five seconds; Left / Right pick a button without a press, for a keyboard); the PS
button that had the new button gets the old one, so the map stays a permutation
and no button is ever lost. Start, Select, the d-pad and both sticks (movement,
map zoom) are not remappable, and neither are the Switch diagnostic chords. The
press that assigns is ignored until it is released. **Reset to default** puts the
PlayStation layout back at once (also in game); Customize itself is title only.
The layout row then reads **Custom**; the map is stored as `button_layout=custom`
and `button_map=` ten digits (docs/CONTROLS_BUNDLE.md section 2.4). A map that is
not a permutation, or `custom` with no map, falls back to PlayStation with one log
line; `DW3_BUTTON_MAP=default` forces the PlayStation layout for one run without
touching the file. The hotkeys follow the custom map like the layouts: after
swapping L3 and R3, fast-forward is on the button that now produces R3 (the Help
pages and the `dw3-controls: hotkeys` line say which).

**Which hotkeys follow the layout.** Port hotkeys are named in PS1 buttons and
read the game's logical buttons, so they follow the map: fast-forward (R3), the
restart chord (L1 + R1 + Start + Select), Select (Options from the title and in
a battle), the map view (Square opens it, the cancel button closes it: B in the
Nintendo layout), the Options, Cheats and Yes / No screens (Cross **or** Circle
confirms, Triangle goes back: A or X confirms and B goes back in Nintendo) and
the EU language select (Cross or Start). The Switch diagnostic chords (Minus +
L + R debug HUD, Minus + ZL + ZR renderer mode) read the physical buttons and
never move, and neither do the region picker at start-up, the launcher and the
crash notice (raw Switch buttons, outside the game). Both are listed in
[CONTROLS_BUNDLE section 1.4](CONTROLS_BUNDLE.md#14-port-code-that-reads-buttons).

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
- Port options is a list of groups (P9.9: Display, Sound, Gameplay, Saves, Controls (P9.8:
  the Button layout row; Confirm or Left / Right switch it), Help (P6.30), and
  Cheats before Controls, always listed (P51), greyed in game). Up/Down moves the cursor; Cross **or
  Circle** (PC X or D; Switch B or A) opens a group. In a group Up/Down selects a
  setting and Cross **or Circle** toggles it. Triangle (PC S; Switch X) in a group
  goes back to the list with the cursor on that group; on the list it returns to
  the selected title row (or the field menu, or the battle). Opening the shell
  does not open the first group. [PORT_OPTIONS](PORT_OPTIONS.md#groups-and-sub-screens-p99).
  On the two volume rows (Music volume, SFX volume) Left / Right step the
  volume down / up from 10 to 0 (a held direction repeats; it stops at the
  ends), and Cross / Circle step up and wrap 10 to 0. See
  [P6.17_VOLUME.md](P6.17_VOLUME.md).
- **Help** (P6.30, the last group of Options; Cross or Circle opens it, in the title, the field
  menu and a battle alike): Up / Down move over the page list; Cross or Circle opens a page. On a
  page Up / Down scroll one line (a held direction repeats), Left / Right a screenful, **L1 / R1**
  (Switch L / R) go to the previous / next page, Triangle (Switch X) goes back to the list, and
  Triangle on the list returns to the Options list. Cross and Circle do nothing on a page. The
  Cheats screen is not described in the help. See [P6.30_HELP](P6.30_HELP.md).
- On the Options screen (the list or any group), **Square** (PC **Z**, Switch
  **Y**) opens the Cheats screen (P6.20) and Triangle returns to Options. It is
  the same screen as the Cheats group, which is always on the list (P51: it was
  listed only once the master switch was on), so the key is only a shortcut; builds made with `--no-cheats` have no such screen. Inside
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
- In a battle, while the command menu (Fight, Tech, ...) waits for you, **Select**
  (PC Backspace, Switch Minus) opens the Options screen over the battle (P9.7
  slice 2). The battle stands still (no timers, gauges or enemy turn), the music
  keeps playing, and Triangle returns to the same command menu. It is ignored
  while L, R, ZL or ZR is held (so the restart chord and the Switch debug chords
  are unchanged) and at any other moment of a battle. Select does nothing else
  in a battle. See [PORT_OPTIONS.md](PORT_OPTIONS.md#options-in-battle-p97-slice-2).
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
| Options in battle | Select, while the command menu waits | Backspace | Minus | Port options over the held battle; no L1/R1/L2/R2 held; not during attacks or in the sub-menus; Widescreen and Cheats greyed; Triangle returns (P9.7 slice 2) |
| Return to title from the field menu | Start, then the last row, Cross, Yes | Enter, ..., X | Plus, ..., B | Always there; the same reset as the chord, after a Yes / No question (P6.27) |
| Quit game from the title menu | Last title row, Cross, Yes | ..., X | ..., B | Always on the native title; flushes the saves, then hbmenu (Switch) or closes the window (PC) (P6.28) |
| Open the new full-screen map (P6.32) | Square on the game's Map screen | Z | Y (both layouts) | Widescreen on and `map_screen=1` in `port_options.cfg` (**default off since phase 1**; the PoC builds had it on). It opens at **1.5x** on your area with the top and bottom bars hidden (P51); **Circle shows / hides the bars** (the name box stays). In it: **D-pad (or left stick) moves the cursor** like the game's (the view follows it when zoomed; the game's name box shows on a location), **the right stick zooms** (up in, down out, smooth, not in steps), L1 / R1 zoom in steps, **L2 / R2 (ZL / ZR) change the scaler** (Nearest is the default), Circle hides the text and the name box, the cancel button (PS1 Triangle: Switch X, or B with the Nintendo layout; PC S; it follows the layout and, later, the P6.12 remap) closes it, Square does not; R3 does nothing (no turbo, and it does not flip the fast-forward state either). Free-button table for a future field hotkey: [P6.32 section 7.5](P6.32_MAP_ZOOM.md#75-free-buttons-in-the-field-for-a-future-field-hotkey) |
| Change partner on the equip panel (P6.33) | L2 / R2 | W / E | ZL / ZR | Status, Equipment, while the slot list waits (not inside the item list); previous / next party member, wraps; the slot cursor stays. Needs 2 or more partners. On by default; `items_partner_switch=0` in `port_options.cfg` turns it off. No on-screen help line yet (the panel has no free line in the five EU languages) |
| Equip list: next bag filter (P6.34 phase 3) | Square | Z | Y | Status, Equipment, inside the item list (after Cross on a slot): this slot, all weapons, all armor and gear, worn by others (wraps); Cross on a worn-by-others row moves that item from its wearer to this Digimon. On by default; `items_bag_view=0` in `port_options.cfg` turns it off. No on-screen hint yet |
| Use another copy on another Digimon (P6.34 phase 4) | Cross on a Digimon in the Items menu | X | B | Items, a usable item, the partner step ("Choose Digimon"): after a use the screen stays, Up / Down choose another Digimon and Cross uses the next copy; Triangle (Switch X, or B with the Nintendo layout) goes back to the list, and so does the use of the last copy. On by default; `items_use_stays=0` in `port_options.cfg` turns it off |

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
and card game remain 1x; so do the Status, Items and Map screens of the field
menu and the shops. Menus that are open inside the field or a battle (the field
menu list, Options and Return to title's Yes / No question over it, and the
battle's command menu and lists) run at about 1.5x while turbo is on, not at the
full setting (owner decision 2026-10-27); walking, text, cutscenes, transitions
and battle animation keep the full speed.
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

## Combined image: asking for the region picker at launch (P51 item 4, a try)

| Action | Switch | Notes |
|---|---|---|
| Show the US / EU picker although a region is stored | hold **Minus**, or **ZL and ZR together**, while the `.nro` starts | Only when both regions are present and `region_default` is us or eu. hbmenu or Sphaira may take either button before the game sees it (Sphaira shows a toast for Minus); the log says what was seen: `dw3-region: pick buttons seen minus=M zl=Z zr=R (12 frames)`. Unverified on the console. The supported way is Options > Gameplay > **Region at start** (Ask / US / EU) |
