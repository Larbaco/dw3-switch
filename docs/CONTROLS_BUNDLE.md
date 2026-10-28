# Controls bundle: button remap, confirm layout, Switch glyphs (design)

Status: **design only** (2026-10-28). Nothing in this document is built. It is
ROADMAP "Priority work order", Tier 2 item 2: the goal notes
[P6.12](ROADMAP.md#phase-6-goal-notes) (button mapping; the former P6.13
mapping submenu was merged into it),
[P9.8](ROADMAP.md#phase-9--port-options-menu) (confirm / cancel layout) and
[P10.6](ROADMAP.md#phase-10--native-switch-features-residual) (Switch-style
glyphs). They are designed together because they share one idea: **the game
sees PS1 buttons, and the port decides which physical button produces each one
and what that button is called on screen.**

Related: [HOTKEYS.md](HOTKEYS.md) (the controls table this must keep true),
[P4.4_INPUT_STATUS.md](P4.4_INPUT_STATUS.md) (hid to pad frame),
[PORT_OPTIONS.md](PORT_OPTIONS.md) (Options screen, `port_options.cfg`),
[PER_USER_SAVES.md](PER_USER_SAVES.md) (options are global),
[TEXTURE_PACKS.md](TEXTURE_PACKS.md) (packs, art rules),
[TITLE_MENU_DESIGN.md](TITLE_MENU_DESIGN.md).

Citations are `file:line` in this repo at `main` 27df539. `decomp:` is
`vendor/dw3_decomp` and `psyz:` is `vendor/psyz` (read in the owner's full
checkout; line numbers may drift with the pinned submodule commit).

## 1. What exists today

### 1.1 Physical to PS1 (Switch and PC)

`platforms/switch/psyz_backend/nx_input.c` maps by **position** (header comment,
lines 7-14). `held_buttons()` (46-102) turns libnx hid bits into the PSY-Q pad
word: A to Circle (`PADRright`, 49-51), B to Cross (`PADRdown`, 52-54), X to
Triangle (55-57), Y to Square (58-60), D-pad (62-73), L/R to L1/R1 and ZL/ZR to
L2/R2 (75-86), stick clicks to `PADi`/`PADj` = L3/R3 (87-92), Plus to Start and
Minus to Select (94-99). `Psyz_PadsPoll()` (109-218) inverts the word into wire
bytes 2 and 3 (168-169, 200-201) and hands the frame to PSY-Z with
`Psyz_PadsSet(0, ...)` (212). The table is in P4.4_INPUT_STATUS.md ("Switch
button map").

PC: the keyboard table is PSY-Z's, `psyz:psyz/src/platform/sdl3_common.h:620-635`
(Cross X, Circle D, Triangle S, Square Z, Select Backspace, L3/R3 1/2, Start
Enter, shoulders Q/W/E/R). Gamepads use SDL's positional names
(NORTH/SOUTH/WEST/EAST, `sdl3_common.h:652-697`), so a PC pad follows the same
positional rule as the Switch. The PC table is vendored code the port does not
edit.

### 1.2 The shared pad host

`port/psyz_missing/psyz_pad_host.c` is shared by the Switch and the PC
(P4.4_INPUT_STATUS.md, "Where the pieces live"). It owns the buffers the decomp
reads (`g_pad_rx`, line 70, set by `PadInitDirect`, 462-471). Each frame the
decomp's `updatePad` calls `PadChkVsync()` (487-509), which runs the replay and
auto-pad injection (491-494) and then `refresh_port()` (432-437).
`refresh_port` copies the platform's frame into the game's buffer with
`Psyz_PadsGet(...)` (436), and `__wrap_PadGetState` calls it again (511-545).
**This copy is the one place both hosts pass through, after any platform source
and before the engine reads a bit.** The map belongs there (section 2.1).

### 1.3 What the engine does with the bits

- `decomp:src/engine/pad/port.c` `readPadButtons` rotates the three face bits:
  raw bits 12/13/14 (triangle / circle / cross, `RAW_*` in
  `decomp:include/engine/pad.h`) become logical `PAD_CIRCLE` 12, `PAD_CROSS` 13,
  `PAD_TRIANGLE` 14. The US build always does so. The EU build does so
  **unless** `LANGUAGE` is 0 ("the Japanese language keeps the buttons as they
  are"). `LANGUAGE` is 2-5 on the EU disc (`decomp:include/engine/file.h:185`).
  Result: **US and EU read the same physical Cross as logical Cross**, and the
  Japanese swap is never active in this port. The confirm button does not differ
  between the two discs.
- The engine has its own per-slot **button map** (`PadSlot.buttonMap`,
  `getButtonBit`, `swapButtons`, `resetButtonMap` in `decomp:src/engine/pad/pad.c`,
  and the repeat loop in `port.c`). Every `PAD_PRESSED` / `PAD_HELD` /
  `PAD_REPEATED` macro goes through `getButtonBit`. Nothing in the game calls
  `swapButtons`; it is the identity. It is not the right place for the port's
  map (section 2.1, "Why not the engine map").
- Which button does what. `decomp:include/engine/pad.h:16`: "The menus confirm
  with PAD_CROSS and cancel with PAD_TRIANGLE". Counting `PAD_TRIANGLE` uses in
  `decomp:src`: card battle 35, field menu 22, Digimon lab 13, item shop 12,
  battle 6. **Circle is a secondary button**: item shop trade
  (`decomp:src/menus/item_shop/trade.c:171,203,247`), card battle (hand pick,
  questions), Digimon lab, deck editor. Text boxes can wait for Cross, Circle,
  Triangle or Square (`TEXT_WAIT_BUTTONS`,
  `decomp:src/engine/text/text_window.c:887`). So **the game's cancel is
  Triangle (Switch X), not Circle (Switch A)**. The P9.8 note says "A
  cancels"; that is true only of the port's own screens, which accept Cross or
  Circle to confirm and Triangle to go back.

### 1.4 Port code that reads buttons

| Reader | Where | Reads | After a map |
|---|---|---|---|
| Options screens (list, groups) | `port/options/port_options.c:859-867` | logical Cross or Circle confirm, Triangle back, Square Cheats | follows the map |
| Cheats, Quit / Yes-No | `port/options/cheats.c:64`, `port/options/confirm.c:31-33` | logical | follows |
| Fast-forward (R3) | `port/options/turbo.c:65` | `PAD_HELD(PAD_R3)` | follows |
| Restart chord L1+R1+Start+Select | `port/options/restart.c:18-19` | logical | follows |
| Select in battle, shoulders held | `port/options/battle_options.c:50-53` | `PAD_SELECT`, L1/R1/L2/R2 | follows |
| Full-screen map view close (P6.32, `port/map_view/map_view.c`, patch `0146`) | `MV_CANCEL_BIT` = PS-side Triangle bit of the logical pad that the game's `PAD_PRESSED` reads | logical cancel (owner decision 2026-10-28: the view closes with the game's cancel button; Square, which opens it, does not close it) | follows the map automatically, and the P9.8 layout too: no code change when P6.12 lands. The game's Map screen closes with the same bit, so the two always agree. Opening stays on PS-side Square, which the remap moves with its function (the open key is the P6.32 phase 4 binding) |
| Movie skip | patch `0034` | Start | Start is reserved |
| Debug HUD Minus+L+R, renderer Minus+ZL+ZR | `nx_debug_overlay.c:34,280`, `nx_battle_mode.c:16,76` | **physical** hid bits from `dw3_nx_input_buttons_raw()` | does not follow |
| PC F4 / F6 / Escape | PSY-Z `sdl3_common.h:898-904` | host keys | not pad |

The split is intentional and must be written into HOTKEYS.md: **port hotkeys are
named in PS1 buttons and follow the map; Switch diagnostic chords are physical
and do not.**

### 1.5 Where the game draws PS button icons

ROADMAP P10.6 said this was not investigated. What the decomp shows:

- Text is drawn glyph by glyph by `decomp:src/engine/text/text_window.c`
  (about 572-640). `FONT.decode` (`decomp:src/engine/text/font.c:20-60`) returns
  kind 0 glyph, **kind 1 icon**, kind 2 control code. An icon is the two-byte
  sequence `01 n` in a string (for example `"\x01\x07"` in
  `decomp:src/debug/vibration_test/vibration_test.c:14`) or a Shift-JIS symbol
  found through `FONT_ICON_MAP` (`decomp:src/engine/text/text_tools.c:244-262`,
  114 icons).
- Icons live in three tables, one per text size: `FONT_ICONS_1/2/3[114]`
  (`text_tools.c:431, 671, 911`). A `Glyph` is `{page, u, v, clutX, clutY, w, h,
  dx, dy, advance}` (`decomp:include/engine/text.h:237-249`): a cell in a texture
  page of the font archive `FILE_FONT`, put in VRAM by `loadFont()`
  (`decomp:src/engine/text/font.c:4-9`). The tables have `#if VERSION_US` / EU
  branches, so the cells differ per disc, which fits the owner's remark that the
  two discs' icons differ. Cells are about 8x12 at size 1.
- Which of the 114 icons are the button icons is **not yet proven**. By
  Shift-JIS identity the candidates are 0x817E (x), 0x819B (circle), 0x81A0
  (square), 0x81A2 (triangle). Step 0 of phase 3 proves it by dumping the cells.
- Button art outside the font (battle HUD, shops, the title's PRESS START) is
  not catalogued. Step 0 produces that list.
- Port overlays: the Options hint row draws two small drawn buttons
  (`platforms/pc/src/title_overlay_art.c:654-655`: `button(cx,hy+12,0)` for
  Open / Change, `button(bx-22,hy+12,1)` for Back). The title hint
  `"Select/Backspace: Port options"` (`port/options/port_options.c:1143`) names
  keys in text, and HOTKEYS.md and the help lines name buttons in prose.

## 2. Design

### 2.1 Remap layer: at the pad host, on the raw word, per PS button

**Model.** The map says, for each remappable PS1 button, **which source button
produces it**. A source is a PS1 position exactly as the platform reported it
(the platform keeps its positional table, 1.1). The map is always a
**permutation**: assigning a source to a PS button gives the displaced PS button
the old source (a swap). No PS button is ever unbound and no source is used
twice, so every action the game or the port needs stays reachable, and "reset to
default" is the identity.

**Where.** In `refresh_port()` (`psyz_pad_host.c:432`), immediately after
`Psyz_PadsGet(...)` (436), permute the 16-bit button word in `g_pad_rx[0]` (port
1 only; port 2 is reported disconnected, `nx_input.c` after line 212). Rules the
code must respect:

- Work on the **raw** word, before the engine's face-button rotation, with the
  bit names at `psyz_pad_host.c:100-110` (`DW3_RAW_*`). The wire bytes are
  active low, and the byte order differs between the Switch source (wire order,
  `nx_input.c:168-169`) and the auto-pad injection (native order,
  `psyz_pad_host.c:422-427`). Settle the order from `tools/pad_host_test.c`
  before coding and pin it with a unit test.
- Skip the map while a replay plays or auto-pad injects (`g_rp_state` 1 or 2,
  `DW3_AUTO_PAD`, lines 491-494). Those masks are already logical and the replay
  tests must stay byte-identical (P6.15).
- Sticks are untouched (the engine derives the d-pad from the left stick in
  `port.c`; the right stick drives the map zoom, `psyz_pad_host.c:497-505`).
- It is shared, so a keyboard and a PC pad are covered with no PSY-Z change: the
  map acts on the PS button each key produces. Moving a function to a different
  physical key is a PSY-Z table change and is out of scope.

**Pure policy** goes in a new `port/options/button_map.c` and header: validate a
permutation, apply it to a word, the presets, the cfg value. No libnx or SDL, so
it is unit-testable like `options_cfg.c`.

**Why not the engine map** (`getButtonBit` / `swapButtons`). Every `PAD_*` macro
reads it, including the port's own hotkeys, and the repeat logic keys off it. It
is per-slot state that `initPad` clears (`PAD_DATA_SIZE`, patch `0006`), so an
in-process restart (P6.9) would reset it. And replays would no longer be
mapping-independent. A permutation of the raw word keeps one rule for everything
above the pad host.

**Per action or per button?** Per **button**. The game has hundreds of contexts
and no action table, so "per action" would need a table of every `PAD_*` use.
The UI still speaks in roles where they are stable, so the player knows what a
change does: Cross confirm, Triangle cancel / back, Square menu, Circle
secondary, L1/R1 pages, L2/R2 partner switch, R3 fast-forward.

### 2.2 The confirm / cancel layout (P9.8) is a preset of the map

"Layout" is two presets of the same permutation. P9.8 needs no second mechanism
and a remap never fights it. Each cell is the PS button the physical button
produces.

| Preset | A | B | X | Y | Notes |
|---|---|---|---|---|---|
| **PlayStation** (default) | Circle | Cross | Triangle | Square | identity, bit-identical to today |
| **Nintendo** (recommended meaning) | Cross (confirm) | Triangle (cancel) | Circle | Square | A confirms, B cancels, Y unchanged, X takes the secondary Circle |

Why not a plain A/B swap (A = Cross, B = Circle): the game cancels with
**Triangle** (1.3), so after a pure swap B would be the secondary button and the
cancel would stay on X, the opposite of Nintendo habit. The preset moves three
buttons (A, B, X) and leaves Y, shoulders, triggers and sticks alone. This
changes the P9.8 note ("X and Y keep their positions"): Y stays, X does not.
**Owner question 1.**

How the options combine:

- `button_layout` is `playstation`, `nintendo` or `custom`. Picking a preset in
  the row writes the map. Editing any button on the Customize screen sets the
  row to Custom. Picking a preset again replaces the custom map (Reset is on the
  screen too).
- US and EU use the same logical buttons (1.3), so one preset serves both discs.
- Port screens (Options, Cheats, Quit) accept Cross **or** Circle to confirm
  (`port_options.c:866`), so they stay usable under any preset. Their Back is
  Triangle, which is B under Nintendo, as a Nintendo player expects.

### 2.3 Reserved buttons and conflicts

| Item | Rule | Why |
|---|---|---|
| D-pad, left stick, right stick | not remappable | movement, map zoom |
| Start, Select (Plus, Minus) | not remappable | movie skip, title Select shortcut, restart chord and diagnostic chords all use them |
| HOME, Capture | not ours | OS |
| Remappable (10) | Cross, Circle, Triangle, Square, L1, R1, L2, R2, L3, R3 | |
| Switch debug chords | physical, never remapped | 1.4 |
| Port hotkeys (R3, restart chord, Select) | PS-named, follow the map | consistent with their labels |

Because the map is a permutation, no chord needs a conflict check against the
map itself. Standing rules: a new port chord may not use Cross or Circle (P9.8),
and the Controls screen **lists each hotkey with the physical button that
triggers it now**, computed from the map: after swapping L3 and R3 it reads
"Fast-forward: R3 (Left stick click)". The restart chord stays the four PS
buttons L1+R1+Start+Select, so remapping L1 moves that combination with it, and
the screen shows the physical names.

Recovery from a bad map: Options is always reachable (title Select, the field
menu's OPTIONS row, battle Select). The screens accept Cross or Circle and
Triangle by logical bit, and because the map is a permutation each of those
exists somewhere. A **Reset to default** row restores the identity. The
developer variable `DW3_BUTTON_MAP=default` (not saved, like `DW3_TURBO`) and
deleting the key from `port_options.cfg` do the same. The boot log prints the map
(section 5), so a bug report shows it.

### 2.4 Storage

Global, in `port_options.cfg`: one global file next to the region folders, as
decided in [PER_USER_SAVES.md](PER_USER_SAVES.md#options-scope). A controller
map is a device preference, not a save property. Existing format and limits:
`key=value`, `DW3_OPTCFG_KEY_MAX` 32, `DW3_OPTCFG_VAL_MAX` 48,
`DW3_OPTCFG_MAX_ENTRIES` 48 (`port/include/port_options_cfg.h:28-34`); unknown
keys are kept (`port/options/options_cfg.c`). Two keys, so the 48-entry cap is
not eaten:

```
button_layout=playstation     # playstation | nintendo | custom (default playstation)
button_map=1203456789         # only when custom
```

`button_map` has ten digits, one per PS button in the fixed order 0 Cross,
1 Circle, 2 Triangle, 3 Square, 4 L1, 5 R1, 6 L2, 7 R2, 8 L3, 9 R3; each digit
names the source (same numbering) that produces it. The identity is
`0123456789`; the Nintendo preset is `1203456789` (Cross from the Circle
position = A, Circle from Triangle = X, Triangle from Cross = B). A value that
is not a permutation of ten digits falls back to the identity with one log line.
Phase 3 adds `switch_glyphs=0|1` (default 0).

### 2.5 Options UI

The hidden placeholder `DW3_OPTGRP_CONTROLS` (`port/include/port_options_groups.h:20`,
table entry at `:42-51`, `built = 0`) becomes the **Controls** sub-screen, drawn
like every other group in the title menu's identity
([PORT_OPTIONS.md](PORT_OPTIONS.md#groups-and-sub-screens-p99),
TITLE_MENU_DESIGN.md): the dimmed backdrop, navy plaques with steel bevels and
amber rails, a header plaque CONTROLS, the message strip and the two hints. A
group holds up to `DW3_OPTGRP_MAX_ROWS` = 6 rows (`port_options_groups.h:24`)
and lists scroll past `DW3_OPTLIST_VISIBLE`.

1. **Controls group rows** (new registry indexes from 9): **Button layout**
   (PlayStation / Nintendo / Custom), **Customize buttons** (opens the map
   screen), **Reset to default**, **Switch glyphs** (phase 3), and the
   **Restart hotkey** row moved from Gameplay, as PORT_OPTIONS.md says it will
   be.
2. **Customize buttons** screen: one row per remappable PS button, PS name on the
   left, current physical button on the right ("Cross  <-  B"). Confirm on a row
   starts a press-to-assign prompt ("Press the button for Cross"; Back cancels;
   5 second timeout). The pressed *source* takes that row and the PS button that
   had it gets the old source, shown in the message strip before it is committed.
   Left / Right on a row cycles the sources without a press (for a PC keyboard
   with no pad attached). The prompt must read the **source** word, not the
   mapped one, so `psyz_pad_host.c` exposes a getter for the pre-map word; the
   prompt ignores Start, Select and the D-pad and says so.
3. Locking: **Customize** is title-only, like Widescreen in game, because
   remapping under a held battle is confusing; the layout row works everywhere.
4. Language: the group name, row labels, values and message lines need the five
   EU languages and English like every group (PORT_OPTIONS.md, "Languages"). PS
   button names stay PS names plus the physical name.

## 3. Glyphs (P10.6)

**Aim.** Wherever the game shows a PS button, show the *physical button that
produces it under the current map*, as a Switch-style glyph (A B X Y L R ZL ZR +
-). The rule is `glyph(ps_button) = name(source[ps_button])`. The game's Cross
icon shows `B` under PlayStation, `A` under Nintendo, and whatever it is under
Custom. So glyphs depend on the map, not only on the layout.

**Mechanism: patch the font icon cells in VRAM at runtime, not a static pack.** A
texture pack replaces a file by id (P8.2 / P8.3) and cannot follow a remap; at
best it serves two fixed layouts. The font is one archive, `FILE_FONT` (1.5),
whose icon cells are addressed by the `Glyph` tables. After `loadFont()`, and
again when the map or the switch changes, the port writes the glyph bitmaps into
the cells that `FONT_STYLES[n].icons[idx]` points to (`page, u, v, w, h`), for
the three sizes. It reads the cell geometry **from the table at run time**, so
one code path covers the US and EU cells whose numbers the `#if VERSION_*`
branches change. The bitmaps are 4 bpp indexed to the font's own palette (no
CLUT change); width and advance stay the original icon's, so strings are
unchanged and no width change touches the five EU languages. If a glyph such as
`ZL` does not fit the cell, step 0 decides between a two-letter glyph at a
smaller size and a wider cell.

**Step 0 is investigation, not code.** A dev-only PC tool (output outside git)
dumps `FONT_ICONS_1..3` cells for US and EU and the strings that use them, to
answer: which icons are the PS buttons (including L1/R1/L2/R2 and Start/Select if
present), which screens draw them, and whether any button art lives outside the
font. Anything outside the font gets an overlay hook instead.

**Port overlays.** The PC overlay's drawn buttons (`title_overlay_art.c:654-655`)
and the hint text become functions of the map (Confirm glyph = `glyph(Cross)`,
Back glyph = `glyph(Triangle)`). `port_options.c:1143`, the Options help lines
and HOTKEYS.md name buttons through one helper instead of fixed words. On the
Switch the Options hint reads B Change / X Back (TITLE_MENU_DESIGN.md:537) and
becomes A / B under Nintendo. With the glyph switch off, overlays keep the PS
names.

**Art and licence.** The glyphs are **original art made for this project**: a
plain letter or symbol in a ring or pill, in the game's pixel-font style, at the
three icon sizes. Never Nintendo assets, the Joy-Con button images or the system
font, and nothing extracted or derived from the disc. Committed under
`assets/glyphs/` (the exception in AGENTS.md, "Do not vend game assets",
2026-10-27) with a `LICENSE` (CC0 recommended) and a `PROVENANCE.md` stating that
no extracted or derived game or console asset (no tracing, img2img or
upscaling) was used, as for the map fan art
([P6.32_MAP_ZOOM.md](P6.32_MAP_ZOOM.md), asset sources). They are tiny indexed
bitmaps, so they can also be C arrays in `port/` with the same statement in
`PROVENANCE.md`.

**Option.** `switch_glyphs` (0 default = original icons, 1 = Switch glyphs).
Switch only at first; the code is shared, so PC pads can get it later. On PC the
keyboard keeps the PS names.

## 4. Phases (small PRs)

1. **Phase 1, layout, mapper and log (P9.8).** `port/options/button_map.[ch]`
   (permutation, presets, cfg value), the `refresh_port` hook, the `button_layout`
   key (playstation / nintendo only), the Controls group with one row **Button
   layout** (`built = 1`), labels in six languages, boot and change logs, unit
   tests, HOTKEYS.md and P4.4_INPUT_STATUS.md describing both layouts (a column
   per layout), and a replay run proving the default is bit-identical. No custom
   map, no glyphs. Done when the P9.8 done-when holds.
2. **Phase 2, remap (P6.12).** The `custom` value and `button_map`, the Customize
   screen with press-to-assign, Reset, the hotkey-to-physical listing, the
   Restart hotkey row moved into Controls, the pre-map getter, tests. Done when
   the P6.12 done-when holds.
3. **Phase 3, glyphs (P10.6).** Step 0 investigation PR (doc and dev tool, no
   behaviour); then the art with LICENSE and PROVENANCE.md; then the VRAM patch
   and overlay helper; then the `switch_glyphs` row. Each can be its own PR.

Each phase updates its ROADMAP row, HOTKEYS.md, PORT_OPTIONS.md and the OLED test
plan in the same PR (AGENTS.md documentation rule).

## 5. Logs (AGENTS.md rule 12)

One line per event, never per frame, through `psyz_log` on the Switch (so it
lands in `dw3.log`, rotated to `dw3.1.log` ... `dw3.4.log`) and stderr on the PC.
Prefix `dw3-controls:`.

```
dw3-controls: map loaded layout=nintendo source=cfg
dw3-controls: map cross<-A circle<-X triangle<-B square<-Y l1<-L r1<-R l2<-ZL r2<-ZR l3<-StickL r3<-StickR
dw3-controls: layout changed playstation -> nintendo (screen)
dw3-controls: assign cross<-A (circle<-B swapped in) [custom]
dw3-controls: reset to default (layout=playstation)
dw3-controls: map invalid (<reason>), using default
dw3-controls: hotkeys fast-forward=R3 (Left stick click) restart=L1+R1+Start+Select (L+R+Plus+Minus)
dw3-controls: glyphs on|off, patched N cells (sizes 1,2,3, us|eu)
dw3-controls: first press source=A -> logical cross
```

The last line, logged once on the first confirm press after boot, is the proof
line for a mapping check: it prints the physical source and the logical button it
became.

## 6. Test plan

**Unit (host):** `button_map` accepts only permutations, round-trips the cfg
value, the presets equal the table in 2.2, identity is a no-op.
`tools/pad_host_test.c` and `tools/tests/test_pad_host.py` get cases: a source
word through identity, Nintendo and a custom swap, and a replay or auto-pad word
that is **not** remapped.

**PC replays** (`tools/replay_tests.sh`,
[P6.15_REPLAY_TESTS.md](P6.15_REPLAY_TESTS.md)): the existing US and EU boot
replays must stay byte-identical with the default, which proves phase 1 changes
nothing by itself. Replays bypass the map by design, so they cannot prove the map
path; the unit test does. One extra replay with a test `port_options.cfg` set to
Nintendo checks that the cfg loads, the Options row shows it and the boot is
unharmed.

**OLED items**, each with its proof line in `dw3.log` (to be added to
[OLED_TEST_PLAN.md](OLED_TEST_PLAN.md) in the phase PRs):

| # | Check | Proof |
|---|---|---|
| C1 | Default launch behaves as before (B confirms, A secondary, X back) | `map loaded layout=playstation source=default`, then `first press source=B -> logical cross` |
| C2 | Options, Controls, Button layout Nintendo: in the title menu A confirms, B backs out | `layout changed playstation -> nintendo`, `first press source=A -> logical cross` |
| C3 | Nintendo in field and battle: A confirms, B cancels a menu, the shop's secondary action is on X | the `map ...` line shows `triangle<-B circle<-X` |
| C4 | Quit and relaunch keeps the layout, on US and EU | `map loaded layout=nintendo source=cfg` on both |
| C5 | Customize: swap L1 and R1, pages turn accordingly; Reset restores | `assign ...`, `reset to default` |
| C6 | After swapping L3 and R3, fast-forward is on the new R3 button and listed right | `hotkeys fast-forward=R3 (...)` and the `dw3-turbo:` toggle line |
| C7 | Debug chords (nxlink) unchanged by any map | HUD toggles with Minus+L+R |
| C8 | Phase 3: glyphs on, prompts and Options hints show A / B per layout; off restores the icons; US and EU, two EU languages | `glyphs on, patched N cells (...)`; the owner reports how it looks |

## 7. Open questions for the owner

1. **What does the "Nintendo" layout mean?** (a) A confirms and B cancels (Cross
   to A, Triangle to B, Circle to X, Y unchanged), or (b) a plain A/B swap (Cross
   to A, Circle to B), where the game's cancel stays on X because the game
   cancels with Triangle, not Circle. *Recommended: (a).* It matches Nintendo
   habit; it moves X, which the P9.8 note said would stay.
2. **Do port hotkeys follow the remap** (fast-forward moves with the PS button R3)
   **or stay on the physical buttons?** *Recommended: follow*, listed on the
   Controls screen with the current physical name. Switch debug chords stay
   physical either way.
3. **Glyph art.** Original art drawn for the project (letters in rings, CC0 plus
   PROVENANCE.md), Switch only first, off by default. Commit it as files under
   `assets/glyphs/` (recommended) or generate it in code? Should the PC show
   glyphs too? *Recommended: files; PC keeps PS names for now.*

Smaller choices made in this design, change them if you disagree: Start and
Select are not remappable; the map is one global setting, not per user; PC
keyboard keys cannot be moved to other keys (only the PS button each key
produces can be re-pointed), because that table is in the vendored PSY-Z.
