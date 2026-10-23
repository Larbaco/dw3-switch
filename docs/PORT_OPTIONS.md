# Port options menu

Controls, physical mappings, and diagnostic chord overlap: [HOTKEYS.md](HOTKEYS.md).

Phase 9 goal note. See [ROADMAP.md](ROADMAP.md) P9.1–P9.5.

Vanilla Digimon World 3 / 2003 menus stay as the decomp implements them.
Anything this port adds that **did not exist on the original PS1 game**
(host polish, native QoL parity, texture packs, fast-forward, etc.) should be
reachable from a **port options** entry on the title / main menu, with
per-feature toggles and defaults that preserve original behaviour.

## Scope

| In scope | Out of scope |
|---|---|
| Toggle port-only features | Replacing or restyling every vanilla menu |
| Persist options on owner storage (PC dir / `sdmc:/switch/dw3/…`) | Shipping a default “modded” experience with no vanilla path |
| Same menu on PC and Switch hosts | Requiring a companion app to change settings |
| Clear labels: feature → which phase/doc owns it | Mixing port flags into the PS1 `.mcr` by default |

EU `CNTY_SEL` / `LANGUAGE` and US/EU disc selection are **original** (or
original-adjacent) flows — expose them where the game already does (see
P6.2). The port menu covers **port additions**, not a second language
select unless we add a port-only override.

## Layering

| Layer | Responsibility |
|---|---|
| Title / main menu hook | Entry point into port options (minimal glue; prefer `port/` over editing decomp in place) |
| `port/` | Option registry, defaults, apply-on-boot / apply-on-change |
| Owner config file | `port_options.cfg` (key=value) in the runtime root, see Persistence |
| `platforms/*` | Input/present only; no feature policy in the Switch tree |

## Toggle registry (seed)

Populate as features ship. Every non-native feature should land here or be
explicitly marked N/A in this note.

| Feature area | Examples | Roadmap |
|---|---|---|
| Display / timing | Integer scale, **widescreen** (wanted; default 4:3), PAL pacing 50 vs 60, NTSC host options | Phase 6 (+ P4 present) |
| Convenience | **Fast-forward**: R3 toggles 2x/3x/4x in field and battle; row picks the speed or OFF ([P6.4](P6.4_FAST_FORWARD.md)) | Phase 6 |
| Localization | Active language pack (if port-selected) | Phase 6 |
| Community QoL parity | Share EXP, quest log HUD, NPC bubbles, Fast Start, … | Phase 7 |
| Texture packs | Enable overlay / select pack name | Phase 8 |
| Audio | **Music volume** and **SFX volume**, 0..10 steps, default 10 = the original mix ([P6.17](P6.17_VOLUME.md)) | Phase 6 |
| Original-game fixes | **None are toggles.** P7.7's HUD Digivolution name (patch `0065`) and Claw steal on the killing blow (`0066`) are always on; reasoning in [P7.7_BUG_FIXES.md](P7.7_BUG_FIXES.md) | Phase 7 |
| Pacing | **Encounter rate** (OFF / 0.5x / 1x / 1.5x / 2x) and **Text speed** (Normal / Fast / Instant), both default to the original game ([P6.18](P6.18_PACING.md)) | Phase 6 |
| Battle menu memory | **Battle cursor** (OFF / Command / Command+Tech): the battle command menu and technique list open on your last pick; key `battle_menu_memory` 0 / 1 / 2, off by default (a row since P7.9's follow-up, it was file-only) ([P7.9](P7.9_BATTLE_MENU_MEMORY.md)) | Phase 7 |
| Switch host | Rumble on/intensity, confirm A/B layout, `swkbd` name, CRT/sharp, system-language hint | Phase 10 (+ P4.4 / P5.3 always-on lifecycle) |
| Restart game | Optional L1 + R1 + Start + Select in the shared PS1 pad map; OFF by default | Phase 6.9 |

Defaults: **off** or equivalent to stock PS1 behaviour unless a feature is
purely host presentation with no gameplay impact (document exceptions).
Widescreen is the documented exception: it defaults **on** (owner request
after the preview.22 OLED pass). It is a real toggle since the P6.1 first
pass, saved by P9.4: field and battle draw side gutters, everything else
stays 4:3; `DW3_WIDESCREEN=0` turns it off at boot. See
[P6.1_WIDESCREEN.md](P6.1_WIDESCREEN.md).
Fast-forward's row defaults to **2x**, which only arms R3: the game still
starts and stays at normal speed until R3 is pressed, so stock behaviour is
kept. OFF disables R3. See [P6.4_FAST_FORWARD.md](P6.4_FAST_FORWARD.md).
The two volume rows default to **10**, which leaves the audio output
bit-identical to a build without them (unity gain, the mixer skips the
multiply). See [P6.17_VOLUME.md](P6.17_VOLUME.md).
The two pacing rows default to **1x** and **Normal**: the hooks (patches 0062
and 0063) only report to the port, so a run with the defaults is
pixel-identical to a build without them. See [P6.18_PACING.md](P6.18_PACING.md).

Always-on (not toggles; see [SWITCH_FEATURES.md](SWITCH_FEATURES.md)):
focus-loss pause + delta clamp, memcard write-through/flush, rotating
`.mcr` backups, docked/handheld re-present, vsync wait.

## Persistence

Implemented by P9.4 (PC and Switch). The registry keeps its values in
`port_options.cfg`, one small text file per region runtime root:

| Host | File |
|---|---|
| PC | `<runtime root>/port_options.cfg`, i.e. `runtime/us/` or `runtime/eu/` (`DW3_RUNTIME` / `--runtime`) |
| Switch | `sdmc:/switch/dw3/us/port_options.cfg`, `sdmc:/switch/dw3/eu/port_options.cfg` |

US and EU have their own file because each region has its own runtime root.
`DW3_CONFIG_DIR` (both hosts; PC also `--config-dir`) moves the file to
another directory; the replay runner uses it so test runs never touch the
owner's file. The file is never written into the memory card or the `.mcr`.
It sits next to the runtime data, so it is not part of any save and is
gitignored with the rest of `runtime/`.

### Format

```
# dw3-switch port options (docs/PORT_OPTIONS.md)
version=1
widescreen=1
fast_forward=2
restart_hotkey=0
music_volume=10
sfx_volume=10
encounter_rate=0
text_speed=0
battle_menu_memory=0
end=1
```

File-only keys (a setting with no row on the Options screen) are written by
hand into the same file, before `end=1`; the game reads them through
`dw3_port_options_file_int` and the Options screen keeps them when it
rewrites the file. There is none today: `battle_menu_memory=0|1|2` (P7.9, the
battle command menu and technique list open on the last pick) was the first and
became the **Battle cursor** row, with the same key and values, so a file written
by hand for P7.9 is read by the row and written back by it.

Plain `key=value` lines, no JSON. `#` and `;` start a comment line, blank
lines are ignored, keys are `[A-Za-z0-9_.-]`, values are single-line text
(the options use integers). `end=1` is always the last line the port writes.
Rules, implemented in `port/options/options_cfg.c` and `port/options/port_options.c`:

| Case | Behaviour |
|---|---|
| No file | Defaults (Widescreen ON, Fast-forward 2x armed, Restart hotkey OFF, Music volume 10, SFX volume 10, Encounter rate 1x, Text speed Normal, Battle cursor OFF). The file is first written on the first change, not at boot. |
| Missing key | That option keeps its default. |
| Invalid value (not an integer, or out of range: Fast-forward must be 0 or 2..4, the volumes 0..10, `encounter_rate` 0..4, `text_speed` 0..2, `battle_menu_memory` 0..2, the others 0 or 1) | That option falls back to its default and a `dw3-options: <key>=<v> is not valid` line is logged. The bad line is replaced on the next save. |
| Malformed line, over-long key or value, junk | Skipped and counted (`ignored N malformed line(s)` in the log). |
| File over 4 KiB, or unreadable | Defaults, logged; the next change replaces it. |
| Unknown key | Ignored by the game, kept in memory and written back in its original position, so a build with fewer rows does not delete a newer build's settings (up to 48 keys). |
| `version=N` newer than 1 | Known keys are still read, the rest is kept, and the version is written back as it was. A missing or bad version counts as 1. |
| Duplicate key | The last one wins. |

A file written before P6.17 has no `music_volume` / `sfx_volume` keys; both
read as 10, and the first change adds them without touching the rest. A file
written before P6.18 has no `encounter_rate` / `text_speed` keys; they read as
0 (1x, Normal) the same way. A file with no `battle_menu_memory` key (anything
before the Battle cursor row) reads as 0 (OFF); the first change writes it. A file
that has it from P7.9 (0, 1, 2) is read as it was; a value that is none of those
(P7.9 treated it as off silently) is now logged as not valid and replaced by 0 on
the next save.

`encounter_rate`: 0 = 1x (the game), 1 = 0.5x, 2 = OFF (no random battles),
3 = 1.5x, 4 = 2x. The values 0..2 are the ones P6.18 shipped and are never
renumbered; 3 and 4 were added later, so a file written by an older build reads
exactly as before and an older build reading a 3 or 4 falls back to 1x. The
screen steps them in the order OFF, 0.5x, 1x, 1.5x, 2x (stored 2, 1, 0, 3, 4):
the order is in the screen only, never in the file.
`text_speed`: 0 = Normal (the game), 1 = Fast, 2 = Instant. Both are integers
so the file never holds a word that a later build would have to keep
understanding.

Adding a row means one entry in the registry table (`key`, default, valid
range); nothing else in the store changes.

### When it is loaded and saved

- **Load:** once, in each host's `init_paths`, right after the runtime and
  memcard directories are set up and before `dw3_game_main()`. Widescreen,
  the fast-forward row and the restart hotkey therefore have their saved
  values from the first frame (widescreen applies at boot, not after the
  first visit to Options). The log line is
  `dw3-options: <path> (widescreen=W fast_forward=F restart_hotkey=R
  music_volume=M sfx_volume=S encounter_rate=E text_speed=T battle_menu_memory=B)`
  (PC stderr; Switch `dw3.log`). The volumes
  are handed to the SPU mixer at the same point (`dw3-audio:` line when they
  are not 10).
- **Save: on every change**, i.e. each Confirm on a row, not on leaving the
  screen and never per frame. Reason: a change is a button press on a tiny
  file, so a write costs a few milliseconds on the SD card in a menu, and a
  HOME exit, a console sleep, a kill or a crash while the Options screen is
  still open cannot lose it. Saving when the screen closes would need hooks
  in the P10.2 applet focus/exit paths and the P6.9 restart; saving on
  change needs none, and there is nothing to flush on HOME. A failed write
  (SD full or busy) is logged, keeps the live value, and is retried when the
  screen closes and at process exit (`dw3_port_options_flush`).
- **Restart (P6.9):** the in-process restart does not re-read the file; the
  registry already equals it. A host relaunch (PC restart, return from
  hbmenu) reads it again.
- Left / Right on a volume row save at once like any other change, so
  holding a direction writes the file once per step (a few bytes each). The
  Encounter rate, Text speed and Battle cursor rows do the same.
- Fast-forward's *row* (OFF / 2x / 3x / 4x) is saved. Whether R3 currently
  has it running is not: the game always starts at normal speed.

### Per-run overrides are not saved

`DW3_WIDESCREEN=0|1`, `DW3_TURBO=0|2|3|4`, `DW3_MUSIC_VOLUME=0..10` /
`DW3_SFX_VOLUME=0..10` and `DW3_ENCOUNTER_RATE=0..4` (the stored values) / `DW3_TEXT_SPEED=0..2`
(an invalid value is ignored) still force a value for
that run (tests, replay recordings, audio measurements). They win over the file, but another toggle on
the Options screen writes the value the file had for the forced row, not the
forced one. Toggling the forced row itself is a user choice and is saved.

### Atomic write (FAT-safe)

`port_options.cfg.tmp` is written and closed, the old file is removed, and
the temp file is renamed into place. The unlink is needed because FAT/exFAT
(the Switch SD) and Windows refuse to rename over an existing file; it is the
same rule the flat saves use (`port/memcard/memcard_host.c`). If the process
dies between the remove and the rename, the next launch finds no
`port_options.cfg` but a complete `.tmp` (it ends with `end=1`) and uses it
(`recovered ... .tmp` in the log); a torn `.tmp` is ignored. If the write
fails before the remove, the old file is untouched.

### Restart hotkey

The restart hotkey was the first live setting in the registry. It is OFF by
default and, since P9.4, stays on once enabled. When enabled, the chord
flushes dirty P5.3 slots before returning the game to its title boot flow. A
failed flush cancels the restart. This does not format, delete, or change
clean save files. See [P6.9_RESTART.md](P6.9_RESTART.md).

### Tests and validation (P9.4)

- `python3 -m pytest tools/tests/test_options_persist.py`: store (parse,
  serialize round trip, bad numbers, junk, over-long lines, full table,
  newer version, atomic write, repeated overwrite, `.tmp` recovery, torn
  `.tmp`, oversized file, missing directory) and registry (defaults, toggles
  surviving a "restart" as a second process, save on each change, bad
  values, unknown keys kept, env overrides not saved, unwritable directory
  keeps the old file), plus the P6.17 volume rows (defaults, Left / Right
  and Confirm steps with clamping and wrap, round trip, bad values, a file
  from before P6.17, `DW3_MUSIC_VOLUME` / `DW3_SFX_VOLUME` not saved by an
  unrelated change, key order).
- `tools/replay_tests.sh --only us-options-persist --only eu-options-persist`
  (PC) and `tools/replay_ryubing.sh --only us-options-persist ...`
  (Ryubing) run two phases on one config dir: phase 1 turns Widescreen OFF,
  Fast-forward 2x -> 3x and Restart hotkey ON; phase 2 is a new process whose
  Options screen and log must show the same values.
- `tools/replay_tests.sh --only us-volume-persist --only eu-volume-persist`
  (P6.17): Music volume 10 -> 7 and SFX volume 10 -> 3 with Left in one
  process, read back by the next; the manifest requires the `saved ...
  music_volume=7 sfx_volume=3` and the read-back lines in each phase's log.
- `tools/replay_tests.sh --only us-pacing-persist --only eu-pacing-persist`
  (P6.18, log-only: no reference captures): Encounter rate 1x -> 0.5x (Left) and Text
  speed Normal -> Instant in one process, read back by the next; the manifest
  requires the `saved ... encounter_rate=1 text_speed=2 battle_menu_memory=0` and
  the read-back line. The unit tests add the two rows (defaults, Left / Right / Confirm
  steps with clamping and wrap, round trip, bad values, a file from before
  P6.18, `DW3_ENCOUNTER_RATE` / `DW3_TEXT_SPEED` not saved by an unrelated
  change, key order).
- `tools/replay_tests.sh --only us-battle-cursor-persist --only eu-battle-cursor-persist`
  (P7.9 row): phase 1 goes down to the last row and steps Battle cursor Off ->
  Command -> Command+Tech (captures of each word); phase 2 is a new process that
  goes to the row again (capture) and reads `battle_menu_memory=2` back. The
  manifest requires the `battle cursor command` and `battle cursor
  command+technique` lines and the `saved ... battle_menu_memory=2` and
  read-back lines, and forbids `is not valid`.
- `test_options_persist.py` (P7.9 row): the row's default, its steps with clamping
  and the Confirm wrap, the file value of P7.9 read and written back, bad values,
  a file without the key, the choice sound.
- `test_options_persist.py::test_file_only_int_key`: a key with no row is read
  from the file, falls back to its default when absent or not a number, and is
  still in the file after the Options screen rewrites it.
- OLED: pending. Toggle, HOME, close the game, relaunch from hbmenu.

## Entry investigation (P9.2)

Vanilla US/EU title (`STDWTITL`) has no Options item: after PRESS START the
menu only offers **New Game** / **Continue** (`menus/title/menu.c`
`runOptions`). Confirm is `PAD_CROSS`; choices `1` / `2` / `3` drive
`STDWTITL_leaveTitle` → `MODE_NEW_GAME` / `MODE_CONTINUE` / opening movie.
There is no third TIM sprite for a Config label.

| Piece | Location |
|---|---|
| Menu list + pad SM | `vendor/dw3_decomp/src/menus/title/menu.c` |
| Mode leave / fade | `…/title/title.c` (`STDWTITL_stepTitle` / `leaveTitle`) |
| Overlay root | `…/title/scene.c` (`STDWTITL_start`) |
| Cursor XY | `…/title/data/stdwtitl.c` (`STDWTITL_menuCursorPositions`) |
| Host link | `port/CMakeLists.txt` → `DW3_TITLE_SOURCES` |

**Recommended hook:** keep the shell in new `port/options/`; add a thin
`PORT_HOST` patch on `runOptions` / `STDWTITL_tickMenu` so opening Config
never sets `choice` and never calls `leaveTitle`. Stay on `MODE_TITLE`
until the shell closes (Triangle / Back), then resume the vanilla options
substate. Prefer a third cursor row drawn with a shared `port/` text path
(PC + Switch); Select-only (`PAD_SELECT`, Switch **Minus**) is an acceptable
MVP if the third-row drawer is not ready. Do **not** put the product entry
in `nx_debug_overlay` / battle-mode chords (`platforms/switch`).

**Risks:** accidental `choice` 1/2 while the shell is open; input passthrough
confirming Continue underneath; PRESS START timeout (`choice=3`) if entry is
wired too early; clipping a third row near y=194 on 240p.

Full write-up (session notes, not git): 
`~/.cursor/projects/mnt-windows-Jogos-dw3-switch/notes/p92-main-menu-port-options-entry.txt`.

## Title row (P9.2 follow-up)

The title now offers **Start / Port options / Continue** (the first item
keeps the owner's original New Game/Start sprite). Up/Down moves one row
at a time without wrapping. Confirm on the middle row opens the existing
shell; Triangle closes it and leaves the middle row selected. Confirm
does not toggle a stub on the opening frame. New Game and Continue retain
their original title `choice` values and blink/fade flow.

Patch `0045-PORT_HOST-p92-title-options-row.patch` extends the game menu
through `PORT_HOST`; `port/options/title_entry.c` draws the added label
with the game's resident proportional font. The original title labels are
pre-rendered sprites, so this new label uses the in-game font rather than
an identical new title sprite. No owner artwork or generated art is bundled
into the runtime.

**Presentation (title-options UI pass).** The Switch and baseline PC row is
made to read as the third original button rather than a debug overlay:

- The original sprite backgrounds are 164x16 with the selection frame one
  pixel outside. The added row now uses the same 164x16 one-pixel
  black/grey stripe background (it was 14 high, which left a visible 2 px
  gap between rows). Rows sit at y=149/165/181 on a 16-pixel pitch, so the
  three backgrounds butt together and the selected row's frame is the only
  separator. The last frame line stays above the copyright text.
- The label glyph quads are scaled 5/4 with a one-pixel bold pass and a
  drop shadow so the weight and height are closer to the heavier original
  lettering (the resident font is about 8 px tall, the sprite lettering
  about 11). The glyphs are vertically centred on the capital P.
- Selected: font blue row at full brightness. Unselected: the same blue row
  at about 60 percent brightness. The font's dark-blue row lost the letter
  counters at this size and was unreadable on a 720p window.
- Navigation is unchanged: Up/Down move one row without wrapping, Confirm
  on the middle row opens the shell, Triangle returns to the middle row.

The original cursor frame highlights only the selected row. Draw ordering keeps that
frame above the added background.
PRESS START retains its separate two-sprite spread animation, then the
three-row layout appears when the options state starts.

Select/Backspace remains a shortcut; shoulder-button combinations do not
open the shell, preserving the Switch diagnostic chords. The clipped
standalone shortcut hint is no longer drawn because the visible row is
the entry. Independent PC visual/input validation passed as recorded below.
Switch hardware validation passed below; P9.3 effects and P9.4 persistence remain open.

### PC Digivice title-menu follow-up

The PC SDL3 GPU title presents **New Game / Options / Load Game** in a
transparent 960×720 RGBA overlay, drawn over the original title image by one
SDL3 GPU texture and one six-vertex quad. The texture contains three visual states;
selection changes the lighting without moving the plaque or label. Plaques
have navy faces, steel-blue bevels, clipped corner hardware, amber fasteners,
internal rails, and legible centered text. The Switch draws the same
canvas and logo over its GLES present (`DW3_TITLE_NATIVE_OVERLAY`, set by PC
`sdl3_gpu` and by the Switch game image), with focus starting on New Game.
The #117 concept atlas remains a design reference (the mockup is kept
out of Git because it shows game artwork) and are not packaged as
runtime art. Selecting `PSYZ_RENDERER=sdl3_gl` keeps the earlier PS1-primitive
title drawing path and does not compile the SDL3 GPU overlay.

US-runtime validation under Xvfb/Vulkan at `DW3_PC_INTERNAL_RES=3` captured
New Game and Options selection plus P9.2 shell open in an exact 1280×720
window. Automated shell return and Load Game selection were not confirmed.
**Load Game** maps to the existing vanilla Continue /
`STGMCARD` flow; **Options** opens the existing P9.2 shell and the overlay
hides while it is open. Captures are local under
`/tmp/dw3-pc-title-menu-redo/evidence-redesign/`: `hires-final-new-game.png`,
`hires-verify-options.png`, and `hires-verify-options-shell.png`. The 960×720
4:3 viewport is centered, so the 1280×720 output has pillarboxes. Windows/DXIL
and macOS/Metal runtime checks
remain open.

The labels preserve the original flow: New Game uses vanilla choice 1;
Options opens the P9.2 shell and its current no-op stubs; Load Game maps to
vanilla Continue (choice 2) and the original `STGMCARD` flow. It does not
implement another save loader. PC defaults focus to New Game so it matches the
concept. The build and patch application check pass. Earlier 320×240 primitive
captures and 1280×960 window notes are superseded by the GPU validation above.
The Linux/Vulkan run used the US runtime and `DW3_PC_INTERNAL_RES=3`; Windows
and macOS runtime rendering remain untested.

## PC bring-up (P9.2 MVP shortcut)

While **New Game / Continue** are shown on the title menu, press
**Select** to open the port options shell. On the PSY-Z SDL keyboard map
that is **Backspace**. Close with **Triangle** (**S** on the keyboard).
The shell stays on `MODE_TITLE` and does not set the title menu `choice`
(New Game / Continue / timeout still work after close).

| Action | Pad | PC keyboard (PSY-Z) | Switch |
|---|---|---|---|
| Open Config | `PAD_SELECT` | **Backspace** | Minus (alone; avoid debug chords) |
| Close / Back | `PAD_TRIANGLE` | **S** | **X** (Triangle) |
| Confirm toggle | `PAD_CROSS` | **X** | **B** (Cross; stub no-ops for now) |

Implementation: `port/options/`, `port/ui/port_text.c`, patch
`0044-PORT_HOST-p92-title-port-options.patch`. Stub toggles (widescreen
default **off**, fast-forward) are document-only until P9.3.

Headless smoke: `DW3_AUTO_PAD=start,select` then
`DW3_PORT_OPTIONS_FB_DUMP=/tmp/dw3-p92-options-ttb/shell.ppm`
`DW3_PORT_OPTIONS_QUIT_AFTER_DUMP=1`.

## PC shortcut validation (2026-10-12, before the third row)

Tested `main` commit `1d33f4d` with the owner's US runtime, separate
temporary memory-card storage, and the `sdl3_gpu` PC build. The SDL build
used `SDL_X11_XSCRNSAVER=OFF` because that optional development dependency
was absent. The two `test_port_options.py` checks passed.

The automated title-to-shell framebuffer dump was legible at 320x240;
both stub values started OFF. A separate keyboard test on a private Xvfb
display confirmed the following after the intro and PRESS START flow:

- Backspace opens the shell while New Game / Continue are shown.
- X changes the selected stub, Down moves to the second row, and X changes
  that stub too. Both show ON and log a safe no-op; the title remains active.
- S closes the shell and returns to the same Continue selection.
- Backspace reopens it with the in-process stub values retained; S closes
  it again. This is not disk persistence (P9.4 is still open).
- X on Continue after closing reaches the vanilla memory-card load screen
  (`STGMCARD_createScreen loading=1`), confirming the title flow resumes.

Local evidence is under `/tmp/dw3-menu-evidence/`: `shell.ppm`,
`isolated-sequence.png`, `isolated-toggled.png`, `isolated-closed.png`,
`isolated-reopened.png`, `isolated-continue.png`, `ttb.log`, and
`isolated.log`. Screenshots include owner-disc artwork and are not committed.
UI automation must use a private display, never the owner's active desktop.

**Historical visual follow-up:** the title hint `Select/Backspace: Port options` clips
at the right edge of its 240-pixel panel. The shell itself is legible.
No feature code changed in this validation. The three-row follow-up above
replaces that hint with the visible entry. Switch row entry/return validation
is recorded below; diagnostic chord coverage was not part of that test.

## PC three-row validation checkpoint (2026-10-12)

Independent validation tested code commit `632cb70` (PR #100). The later
`5b59e4f` merge changed documentation only. On resumption, the retained
captures, scripts and logs in `/tmp/dw3-menu-entry-evidence` were inspected;
this checkpoint records that completed run, not a new hardware test.
No feature code or assets changed for this record.

The PC host used SDL_GPU/Vulkan lavapipe on private Xvfb display `:198`,
with dummy audio and a separate temporary memcard directory. Keyboard
input targeted that private display, never the owner's active desktop.
Xvfb did not provide effective vsync: the uncapped run measured about
396 FPS, making the existing frame-based repeat look too fast. The final
smoke used the existing `Psyz_VideoSetVsyncMode(2)` 60 Hz limiter through
GDB; no input or runtime implementation was changed.

- At 60 Hz, Up/Down taps of 55 ms traverse all three rows. One-second
  holds clamp at Start/Continue; releasing then tapping reaches the middle.
- Confirm on Port options opens both stubs OFF without confirm passthrough.
  Triangle/S returns to the middle row. Select/Backspace also opens the shell.
- Stub toggles work within the session; a process restart restores OFF/OFF,
  as expected while persistence is absent. The TTB framebuffer dump captures
  the shell text.
- Start retains choice 1 and reaches `FIELDSTG` stage 474 (Junior street);
  Continue retains choice 2 and reaches `STGMCARD` with `loading=1`.
- Captures show the opaque striped row and the original cursor only on the
  selected item. The resident proportional font is legible, but is not an
  identical title-label sprite.
- Tools suite: **79 passed, 2 skipped** in the retained pytest log.

Representative local evidence: `v4-middle.png`, `v4-shell-defaults.png`,
`v4-continue-load.png`, `newgame-flow.png`, `restart-stubs-off.png`,
`shell-ttb.png`, `sequence60hz.log`, `newgame60hz.log` and `pytest-latest.log`.
These temporary files are not repository assets or durable CI artifacts.
Gamepad input and Switch presentation/input were not covered by this PC
keyboard smoke; the later OLED checkpoint below completes P9.2.
P9.3 effects and P9.4 persistence remain open.

## OLED preview.20 validation (2026-10-12)

The owner tested the nxlink game binary from `v0.6.5-preview.20`, source
`ecefe18e11d679a73ec0cf16a7592fffb5d0ed84`, in handheld mode. At the title,
Plus shows Start / Port options / Continue; D-pad selects Port options,
B opens the shell, both stubs show OFF, and X closes back to the middle row.
Owner confirmation: “Sim, aparece OFF e volta corretamente”. Together with
the PC TTB/input checkpoint above, this meets P9.2's entry/return done-when.

The mapping in `platforms/switch/psyz_backend/nx_input.c` is positional:
B = Cross (confirm), X = Triangle (close), A = Circle, Y = Square,
Plus = Start and Minus = Select. PC keyboard X is Cross; PC S is Triangle.
The tested physical controller model was not recorded; this does not
validate every controller model or PC gamepad configuration.

The switches remain safe no-op stubs. Hardware toggle effects and persistence
were not tested; P9.3/P9.4 remain open and the overall Phase 9 gate is open.

## Options screen (2026-10-14)

On the PC SDL3 GPU path and the Switch, Options opens its own screen instead
of the old text panel. The title backdrop stays, dimmed (also over the PC
widescreen side extensions); the logo, the menu and the credits (the native
footer, and the game's credit sprites if they were the fallback) are hidden
(patches 0056 and 0073). On the Switch the screen fades in
over about 13 frames and fades out over the menu and logo as they return;
the PC SDL3 GPU shader has no alpha input, so it switches at once. The SDL3
GL path keeps the text panel.

The screen copies the game's own windows, as the memory card screen draws
them: a white bezel shaded on its lower right, a dark rim and a blue-violet
inner line around a navy (0,0,49) body; a stepped blue (0,66,156) rule with
the window title, in capitals, right of the step ("OPTIONS", or the EU
lettering for German, Spanish and Italian); plain white text; the white
pointer, shaded grey on its back, on the focused choice, which sits in the
game's choice box (0,8,74 with a blue border); and a cyan (16,156,247) rule
above a message line that explains the focused choice. Values sit on the
right (ON/OFF, Fast-forward OFF/2x/3x/4x, the volumes OFF and 1..10); OFF is
greyed. Button hints use
the host's buttons: B Change / X Back on the Switch, Cross / Triangle on PC.
Row and message text is English on every disc for now. The text is DejaVu
Sans lettering generated by `tools/generate_pc_title_lettering.py`; no game
art or font is shipped.

`port/options/port_options.c` stays the registry and the input logic (rows,
toggles, Triangle back). It hands the cursor and values to
`dw3_pc_title_overlay_set_options_view`, which bumps the canvas generation
only on a change. The PC and Switch hosts keep the options canvas in the shared
cache `title_options_cache.c` and upload only the rectangles that changed
(old and new row plus description on a cursor move, one cell on a value
change, the whole canvas on open or a language change); the reference render
is `title_overlay_art.c`, state `DW3_TITLE_ART_OPTIONS`, selectable on PC with
`DW3_MENU_COMPOSITION=reference`. See
[MENU_COMPOSITION_PLAN](MENU_COMPOSITION_PLAN.md).

Validated in Ryubing on Xvfb with the US disc and the EU disc (language 2):
open from the Options row, move over the rows, cycle Fast-forward through
3x / 4x / OFF, enable the restart hotkey, close back to the Options row, then
Load Game reaches the memory-card screen. The PC SDL3 GPU build was checked
on Xvfb (US): Backspace opens it, X cycles Fast-forward, S closes it.
OLED validation is pending. Captures are kept outside Git under
`tmp/evidence/port-options-screen/`.

**Five rows (P6.17).** Music volume and SFX volume are rows 4 and 5, in
the same canvas and the same dirty-rectangle cache. To fit them the window is
taller (576 px instead of 528, centred at y 72..648 in the 960x720 canvas)
and the row pitch is 60 px instead of 68, so the first row, the pointer, the
rule and message line, the button hints and the bottom rule all moved; the
x geometry is unchanged. The message line is 666 and 677 px wide for the two
new rows, inside the 696 px message box (the same screen is used on the US and
EU discs; only the window title differs). On a volume row Left / Right step the
volume (a held direction repeats), Cross / Circle step up and wrap 10 -> 0;
step 0 shows as a greyed OFF, steps 1..10 as numbers. Up/Down still do not
wrap. `tools/tests/test_options_cache.py` renders every volume step on both
volume rows from every cursor row against the full reference render, so the
dirty rectangles still cover everything. The generated lettering for the new
words is in `title_overlay_lettering.inc` (`tools/generate_pc_title_lettering.py`;
the earlier entries are byte-identical).

**Seven rows (P6.18).** Encounter rate (1x / 0.5x / OFF) and Text speed
(Normal / Fast / Instant) are rows 6 and 7, appended so the registry indexes of
the first five stay. Seven rows no longer fit at the P6.17 pitch, so the screen
uses a compact list instead of scrolling or pages: the window is 600 px high
(y 60..660 of the 720 px canvas, 60 px free above and below, which keeps it
on screen at 4:3 on PC and on the Switch), a row is 42 px high on a 48 px pitch
(it was 52 on 60), and the row, value and button text is 26 px lettering
(it was 30). Seven rows, the message line and the hints all stay on one
screen, so Up / Down and the cache work as before; a scrolling list would have
redrawn every visible row on each scroll and a tab would have added a second
canvas and a new input. Values: Encounter rate `OFF` (greyed) / `0.5x` / `1x` / `1.5x` / `2x`;
Text speed `Normal` / `Fast` / `Instant`. On both rows Left / Right step (no
wrap) and Cross / Circle step up and wrap. The message lines are 618 and 626 px
wide ("Random battles only; story and boss battles stay." and "How fast
dialogue is typed out. Normal = original."), inside the 696 px message box
(the Encounter rate line later became "Random battles only: OFF to 2x. Story
battles stay.", 634 px).
The cache keeps its three rectangles for a cursor move (the two 696x42 rows
and the 696x52 message line, 0.37 MB against 2.76 MB for the whole canvas) and
one 168x42 cell for a value change; at most 3 + 7 rectangles per update, so
`DW3_OPTIONS_MAX_DIRTY` stays 10. `tools/tests/test_options_cache.py` renders
every mode of the two rows from every cursor row against the full reference
render. The row, value and button lettering was regenerated at 26 px by
`tools/generate_pc_title_lettering.py` (the title and the message lines are
byte-identical to before).

**Eight rows (P7.9 follow-up, owner request after the rc6 OLED test).** The
**Battle cursor** row (OFF / Command / Command+Tech, key `battle_menu_memory`, the
file-only key of P7.9 promoted) is row 8, appended so the registry indexes of the
first seven stay (`DW3_PORT_OPT_BATTLE_MENU` = 7). Eight rows need a tighter
list, so the window and everything outside the rows stay where they were and the
row pitch drops: a row is **38 px high on a 42 px pitch** (it was 42 on 48), the
first row starts at y 150 (it was 154) and the last ends at y 482, 26 px above the
message rule at y 508 (the gap was 24); the 26 px lettering, the pointer and the
focus box keep their size and fit (the focus box interior is 32 px, the pointer
26). Labels and values are the same x as before; the value cell of the cache is
268 px wide (from x 560, it was 168) because **Command+Tech** is 214 px wide and
ends at x 780 like the other values. Words: `Battle cursor` (168 px),
`Command` and `Command+Tech`, `OFF` greyed as on the other rows; message line
"Battle menu opens on your last pick. Off = original." (644 px, inside the 696 px
message box). Left / Right step (no wrap) and Cross / Circle step up and wrap 2 ->
0, plays the choice sound on a change like every row (the Options sound feedback
of #200: `select` on a change, `cursor` on a row move, silence at an end), and
logs `dw3-port-options: battle cursor off|command|command+technique`. The cache
sends one 696x38 rect per row on a cursor move (two of them plus the 696x52
message line: 356,352 bytes against 2.76 MB for the canvas) and a 268x38 cell for
a value change (40,736 bytes); at most 3 + 6 rectangles per update (the two
cursor rows cover their own value cells), so `DW3_OPTIONS_MAX_DIRTY` stays 10.
`tools/tests/test_options_cache.py` renders every mode of the three mode rows
from every cursor row against the full reference render. Layout checked on the PC
build at 1280x960 (4:3 window), and in a 1920x1080 and a 1280x720 window under
Xvfb (the screen is a 4:3 canvas centred with side bars, as the seven row one
was; Widescreen ON or OFF changes nothing on this screen): evidence in
`tmp/evidence/p79-row/`. The lettering was regenerated by
`tools/generate_pc_title_lettering.py` (the existing words are byte-identical).

**Encounter rate steps (after the rc4 OLED test).** The row has five values
and is stepped OFF, 0.5x, 1x, 1.5x, 2x: Left / Right move along that order and
stop at the ends; Cross / Circle step up and **wrap 2x -> OFF**. The stored values
(0 = 1x, 1 = 0.5x, 2 = OFF, 3 = 1.5x, 4 = 2x) are not in screen order; the
overlay still draws by stored value, so the cache and its dirty rectangles are
unchanged (one 168x42 value cell, `1.5x` is 57 px wide). The message line of
the row is "Random battles only: OFF to 2x. Story battles stay.".

### Sound feedback on the Options screen

The screen answers the pad with the game's own menu sounds, so the player can
hear the SFX volume take effect (owner request after the rc4 OLED test). The
sounds are the ones the title menu itself plays, all key-ons of the system
bank (bank 1, `SYSTEM00..02`), which the title already uses, so no bank has to
be loaded for the screen:

| Pad | Sound (`engine/sound.h`) | Where the game uses it |
|---|---|---|
| Up / Down moves the cursor to another row (not at the first / last row) | `SOUND_CURSOR` | the title menu's New Game / Continue cursor |
| Cross / Circle on a row, or Left / Right that changes a value (not at the end of a row) | `SOUND_SELECT` | the title menu's choice, and the Port options entry itself (patch 0044) |
| Triangle (close the screen) | `SOUND_MENU_CANCEL` | the cancel of the game's field, item and status menus (the title has no back) |

No sound plays when nothing changes, as in the game's own menus. The calls
are in the shared `port/options/port_options.c` (`po_sound`, through
`SOUND.playSound`): no decomp patch and no host code, so PC and Switch behave
alike. The sounds are key-ons, so P6.17 puts them on the **SFX bus**: they
follow the SFX volume row. On a change of that row the new gain is applied
(`dw3_port_audio_apply`) and only then is the sound played, so the player hears
the new level; a Music volume change plays it at the current SFX level. Every
sound logs `dw3-port-options: sound <cursor|select|cancel> (sfx volume N, voice V)`
(`voice` is the SPU voice of the key-on) and a missing bank would log
`sound ... not played: bank N is not loaded`. Evidence: [P6.17](P6.17_VOLUME.md#sound-feedback-on-the-options-screen).

## Title-screen visual reference

The proposed title-screen labels and orange/blue Digivice button styling are
documented in [TITLE_MENU_DESIGN.md](TITLE_MENU_DESIGN.md). The linked atlas
is a visual concept; it does not indicate that the generated artwork is wired
into the game renderer.

### Restart help wording

The native Options screen says the combo "restarts the game" on PC and
Switch. It must not promise a direct return to the title: the EU build
re-enters its language-selection boot flow, while US returns through its
ordinary title flow. This is a wording correction, not a hotkey behavior
change.
