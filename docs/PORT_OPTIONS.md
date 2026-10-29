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
| Convenience | **Fast-forward**: R3 toggles 2x/3x/4x in field, battle and the battle report (results screen); row picks the speed or OFF ([P6.4](P6.4_FAST_FORWARD.md)) | Phase 6 |
| Localization | Active language pack (if port-selected) | Phase 6 |
| Community QoL parity | Share EXP, quest log HUD, NPC bubbles, Fast Start, … | Phase 7 |
| Texture packs | Enable overlay / select pack name | Phase 8 |
| Audio | **Music volume** and **SFX volume**, 0..10 steps, default 10 = the original mix ([P6.17](P6.17_VOLUME.md)) | Phase 6 |
| Original-game fixes | **None are toggles.** P7.7's HUD Digivolution name (patch `0065`) and Claw steal on the killing blow (`0066`) are always on; reasoning in [P7.7_BUG_FIXES.md](P7.7_BUG_FIXES.md) | Phase 7 |
| Pacing | **Encounter rate** (OFF / 0.5x / 1x / 1.5x / 2x) and **Text speed** (Normal / Fast / Instant), both default to the original game ([P6.18](P6.18_PACING.md)). Instant shows every dialogue page, report line and (since 2026-10-24) the area name banner's names whole; Normal and Fast leave the banner as the game types it | Phase 6 |
| Battle menu memory | **Battle cursor** (OFF / Command / Command+Tech): the battle command menu and technique list open on your last pick; key `battle_menu_memory` 0 / 1 / 2, off by default (a row since P7.9's follow-up, it was file-only) ([P7.9](P7.9_BATTLE_MENU_MEMORY.md)) | Phase 7 |
| Button layout | **Button layout** (PlayStation positional, default / Nintendo A confirms, B cancels), ROADMAP P9.8, **built 2026-10-28**: row 9, in the Controls group, key `button_layout`, see [Button layout row](#button-layout-row-p98) | Phase 9 (P9.8) |
| Switch button glyphs | **Switch button glyphs** on/off, ROADMAP P10.6. Future goal (2026-10-23); the glyphs follow the layout | Phase 10 |
| Cheats (test aid) | A **Cheats** sub-screen for hardware testing, reached with Square / Y on the Options screen: master switch, Fill bag, Instant win, Battle now and a count of saves flagged "cheat used" (a sidecar file, never the save). A second page (PR 2) has Money, Warp (13 areas), Item id, Item count and Give item. Off by default, keys `cheat_enabled` and `cheat_instant_win` in `port_options.cfg`; built in by default, removed by `DW3_CHEATS=OFF` / `--no-cheats`. Later HP / MP. See [Cheats](#cheats-p620) and ROADMAP P6.20 | Phase 6 (P6.20) |
| Switch host | Rumble on/intensity, confirm A/B layout, `swkbd` name, CRT/sharp, system-language hint | Phase 10 (+ P4.4 / P5.3 always-on lifecycle) |
| Restart game | Optional L1 + R1 + Start + Select in the shared PS1 pad map; OFF by default | Phase 6.9 |
| Items menu | **Equip from the Items menu** (key `items_equip_from_items`, 0 or 1, **default 1**, file-only until the Menus group of P6.34 phase 5; `DW3_ITEMS_EQUIP_FROM_ITEMS=0` forces it off for a run): Cross on a weapon, armor or accessory in the Items menu opens the partner step and equips through the game's own `STSTATUS_equip` ([P6.34](P6.34_MENU_FLOW.md#12-phase-1-built-2026-10-28-findings-defaults-and-strings), patch `0188`). With 0 the Items menu is the original game's. **On by default is an orchestrator default pending the owner's answer to P6.34 Q1** | Phase 6 |
| Items menu | **Partner switch on the equip panel** (key `items_partner_switch`, 0 or 1, **default 1**, file-only: the Options window is full at nine rows, so there is no row): ZL / ZR change the party member on the Status screen's Equipment slot list without leaving ([P6.33](P6.33_ITEMS_MENU.md), patch `0128`, `DW3_ITEMS_PARTNER_SWITCH=0` forces it off for a run). A documented exception to "off by default": the owner decided on (2026-10-26) | Phase 6 |
| Map screen (P6.32 PoC) | **New full-screen map** (key `map_screen`, 0 or 1, **PoC default 1**, file-only, no row): Square on the game's Map screen opens a host-drawn full-screen map with pan, zoom and a live scaler switch; widescreen only. The real feature will default to off and get its row ([P6.32](P6.32_MAP_ZOOM.md#7-phase-0-results-2026-10-27-spike-and-scaler-poc), patch `0146`) | Phase 6 |
| Auto-save | **Auto-save** (OFF / Each area; key `autosave`, row 9): the first idle frame of a new field area writes the game to its own file, `autosave.bin`; the title's Continue loads it. Off by default; with it off the game is as before ([P6.31](P6.31_AUTOSAVE.md)) | Phase 6 (P6.31) |

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
autosave=0
button_layout=playstation
end=1
```

`cheat_enabled` and `cheat_instant_win` (P6.20, 0 or 1, default off) belong to the
[Cheats screen](#cheats-p620). They are absent until the Cheats screen changes
one. They are read by `port/options/cheats.c` as
file-only keys, are kept when the Options screen rewrites the file, and are
written only when the Cheats screen changes one. A build without cheats ignores
them and keeps them (unknown keys).

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
| Invalid value (not an integer, or out of range: Fast-forward must be 0 or 2..4, the volumes 0..10, `encounter_rate` 0..4, `text_speed` 0..2, `battle_menu_memory` 0..2, the others (`autosave` too) 0 or 1) | That option falls back to its default and a `dw3-options: <key>=<v> is not valid` line is logged. The bad line is replaced on the next save. |
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

`autosave` (P6.31, 0 or 1, default 0, row 9 of the Options screen) is read and written
like the others; it is not part of the `dw3-options:` boot and `saved` log lines (those
keep their text, which the replay scenarios match). With it on the boot logs one extra
line, `dw3-options: autosave=1 (...)`. A file from before the row has no key and reads
as OFF; an invalid value (`2`, `on`, ...) is logged and reads as OFF.

`button_layout` (P9.8) is the one key that holds a **word**, `playstation` or
`nintendo` (default `playstation`), not an integer: it names a layout, and the
custom map of P6.12 will add a third word and a `button_map` key
([CONTROLS_BUNDLE](CONTROLS_BUNDLE.md)). Another value is logged
(`dw3-options: button_layout=<v> is not valid` and `dw3-controls: map invalid
(...), using default`) and reads as PlayStation. It is not part of the
`dw3-options:` boot and `saved` log lines (their text is matched by the replay
scenarios); the boot line is `dw3-controls: map loaded layout=... source=cfg|default|env`.
`DW3_BUTTON_MAP=default` forces PlayStation for one run, is not saved and leaves
the file's value alone.

`items_partner_switch` (P6.33, 0 or 1, default 1) is a file-only key like `battle_menu_memory` was before its row:
read by `dw3_port_items_partner_switch_enabled`, kept when the Options screen rewrites the file, not part of
the `dw3-options:` log lines. A missing or invalid value reads as 1 (on).

`stage_sweep` (P5.5, 0, 1 or 2, default 0) and `stage_sweep_frames` (30 to 3600, default 90) are file-only
keys of the opt-in stage sweep: read by `port/sweep/stage_sweep.c` (builds with the Cheats screen), kept when
the Options screen rewrites the file, not part of the `dw3-options:` log lines. `1` runs or resumes the sweep,
`2` starts it over; see [P5.5_STAGE_SWEEP](P5.5_STAGE_SWEEP.md).

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
clean save files. See [P6.9_RESTART.md](P6.9_RESTART.md). Since P6.27 the field
menu also has a **Return to title** row (always, not tied to this setting), which
asks first and then runs this reset ([below](#quit-and-return-to-title-p627-p628)).

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
- `test_options_persist.py` (P6.31 row): `autosave` defaults to off and writes no
  file, Confirm toggles and saves, a second process reads it back, bad values fall
  back to off, a file from before the row, the choice sound; the unit tests of the
  feature itself are `tools/tests/test_autosave.py` ([P6.31](P6.31_AUTOSAVE.md)).
- `test_options_persist.py::test_file_only_int_key`: a key with no row is read
  from the file, falls back to its default when absent or not a number, and is
  still in the file after the Options screen rewrites it.
- Cheats (P6.20): `tools/tests/test_cheats.py`, `tools/tests/test_options_cache.py`
  (with and without cheats) and the `cheats-screen` and `cheat-*` replays; see
  [Cheats, Tests](#tests) below.
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

> Superseded by [Groups and sub-screens (P9.9)](#groups-and-sub-screens-p99)
> for the list of rows; the Cheats screen still uses this window.

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

## Groups and sub-screens (P9.9)

*Slice 1, 2026-10-27. This section is the current Options screen; the section
above it (2026-10-14 and its row-count notes) describes the single window it
replaced. Since slice 2 (2026-10-28) the Cheats screen is a sub-screen of
the same list, see "Cheats sub-screen (P9.9 slice 2)" below.*

Owner decision (2026-10-27): "esse menu de opções pode ter submenus". Options
is a **list of groups**; a group opens a **sub-screen** with its rows. The two
help screens (P6.30 technical help, P6.35 walkthrough) and the button mapping
(P6.12) are meant to be groups of this list later.

### What the player sees

The list of groups and every sub-screen are drawn in the title menu's
identity ([TITLE_MENU_DESIGN](TITLE_MENU_DESIGN.md)): the dimmed title
backdrop (deeper over the field or a battle, so the menu behind does not read
through), the same navy hardware plaques with steel bevels, amber rails and
fasteners, lit rails, lit fasteners and a short orange halo on the focused
plaque, and the same warm lettering with a one pixel dark shadow. A header
plaque names the screen (OPTIONS, or the open group). A message strip under the
list explains the focused item (a locked one says why), then the two button
hints (Open or Change, Back).

| Group | Rows (registry index, unchanged) | Notes |
|---|---|---|
| Display | Widescreen (0) | Locked in game, as before: the plaque and the row are greyed |
| Sound | Music volume (3), SFX volume (4) | |
| Gameplay | Fast-forward (1), Restart hotkey (2), Encounter rate (5), Text speed (6), Battle cursor (7) | Restart hotkey moves to Controls when P6.12 exists |
| Saves | Auto-save (8) | |
| Controls | Button layout (9) | P9.8, built: PlayStation (default) / Nintendo, see [Button layout row](#button-layout-row-p98). The remap (P6.12) adds rows here and moves Restart hotkey in |
| Cheats | the ten P6.20 cheat rows (not registry rows), a scrolling sub-screen | **Always listed** (P51 item 5; it used to appear only once the master switch was on, so nobody found it): the title opens the Cheats screen where the master switch is; greyed in game (title-only). `DW3_OPTGRP_CHEATS_LISTED` in `port_options_groups.h` (the build has the Cheats feature); `dw3-port-options: group list n=7 (cheats listed, master off, locked in game 0)` once per open |
| Help | no rows, a screen of its own (P6.30, built) | Always listed (it is not secret), after Saves and Cheats. Confirm opens the technical help (`port/help/`, [P6.30_HELP](P6.30_HELP.md)): a list of ten pages and a scrolling text page, on its own full-canvas layer that the host draws instead of the Options layer (the Options cache is untouched). Up / Down scroll a page by line, Left / Right by screenful, L1 / R1 (L / R) change page, Back goes to the page list and out. Title, field menu and battle Options all reach it |
| Walkthrough | not built | P6.35: hidden placeholder in `port_options_groups.h` (`built = 0`); flip the flag when the screen exists |

Keys: Up / Down move the cursor; **Confirm** (Cross, Circle) opens the group;
**Back** (Triangle, X on the Switch) in a group returns to the list with the
cursor on that group, Back on the list closes the screen (to the title, the
field menu or the battle, as before). On a row nothing changed: Confirm and
Left / Right do what they did, values are applied and saved at once. The
Square / Y shortcut to the Cheats screen still works from the list and from any
group (it is title-only: in game the log says it is locked), and so does the
Cheats group. The field menu and battle Options open on the list too.

Grouping is a proposal; the code takes any split, `port_options_groups.h` is
the single table. The Cheats group is listed only while the master switch is
on (the old "Square / Y Cheats" hint was drawn only then, so the cheats stay
undiscoverable on a normal install); the shortcut gets you there the first time.
That answers the owner's open question about hiding it in the safest way; it is
one line (`dw3_optgroups_visible`) to change.

### Languages

Every new label exists in US English and the five EU languages (EN, FR, DE, ES,
IT): the group names and their message lines, Open / Change / Back, the
OPTIONS title (FR was missing and uses the English word). Since the sub-screens
show them too, the registry row labels, the value words (Normal / Fast /
Instant, Command, Command+Tech) and the eight message lines were translated in
the same change (they were English on every disc). ON, OFF, the numbers, 1x,
0.5x... are the same in every language. `tools/generate_pc_title_lettering.py`
checks the widths (group name 330 px, row label 290 px, value 236 px, message
line 690 px) and fails when a text does not fit. All of it is DejaVu lettering
generated into `title_overlay_lettering.inc`; no game art or font is shipped.

### Scrolling

A list shows six plaques (`DW3_OPTLIST_VISIBLE`); a longer one scrolls with the
cursor and draws an amber arrow above or below when there is more. Today no
list is longer than five, so nothing scrolls in the shipped screens; the help
pages and the group list once it passes six will. The scroll offset is part of
the overlay's page code (`dw3_optlist_page`), so a scrolled page is its own
retained layer; `tools/tests/options_cache_main.c` renders scrolled pages
against the reference.

### Design

- `port/include/port_options_groups.h` is the one table (groups, their registry
  rows, visible list, scroll, page codes), read by `port_options.c` and by the
  overlay art. `port_options.c` has a `_Static_assert` that the registry numbers
  in it did not change.
- `port_options.c` keeps the registry, persistence and the sounds. New state:
  level (list or group), the open group, one cursor and scroll per level. It
  hands the page, the cursor (the item's place in the whole list) and the values
  of the group's rows to the overlay.
- `title_overlay_art.c`: `options_list_screen` draws both levels. The plaques
  are the title menu's (`title_plaque`, one function for the title loop and the
  Options; the title menu is pixel-identical to before, checked by the
  `--quick` replay against the old references). Options plaques are rasterized
  once per size and state into small sprites and blitted (the halo alone for the
  lit one), so a focus change on the retained canvas costs about 0.4 ms on a
  desktop CPU instead of about 2.6 ms; `box` and `pixel` got exact opaque and
  empty-destination fast paths. The Cheats group is drawn by the same function
  (slice 2).
- `title_options_cache.c` asks the art for the rectangles (row with its halo,
  value cell, message strip) of the live page and addresses rows by item index.
  The layer cache keys are (art generation, locked bits that matter for the
  page, page, Cheats switch); four slots (two group lists, the sub-screen on
  show, one spare; the fourth slot is 2.76 MB more than #298 kept, the plaque
  sprites add about 0.6 MB; the third and fourth slots are allocated on first use). The prewarm worker (#298) still composes the group list for
  the title and for in game on a low-priority thread with thread-local renderer
  state; since slice 3 it also composes the layer shared by the sub-screens (six
  slots now), and a group is made over it on first use (0.5 to 1.2 ms on a
  desktop CPU, logged as `options update group-over-base ... compose=N us`; a
  second visit is a `cached` copy). See "Opening a group no longer rasterizes".

### Logs

`dw3-port-options: group open <name> (<n> row(s), title|in game)`, `group close
<name> (back to the group list)`, `group open cheats (group|Square / Y
shortcut)`; the existing lines stay (open / close, `open for N ms`, `first
frame shown`, `longest frame gap`, `prewarm ... ready`, row changes, sounds,
`saved port_options.cfg`). Opening a group plays the choice sound, Back plays
the cancel sound, as on the other screens.

### Tests

`tools/tests/port_options_main.c` (list, open, back with the cursor kept, close,
table, scroll helpers, page codes), `options_persist_main.c` (`nav N` walks to a
row with the keys, `row N` jumps for the old tests; sound and group log
checks), `options_cache_main.c` (every group page, cursor and value, scrolled
pages, locked layouts, the cheats pages, the prewarm worker, all against the
reference render, five languages), `locked_art_main.c`, `field_options_main.c`.
Replays (`tools/replay/gen_scenarios.py`, regenerated): options-persist,
volume-persist, pacing-persist, battle-cursor-persist, options-sound,
field-options (+ the four EU languages), field-options-cheats, battle-options,
turbo-menu. Their checkpoints are the new screens; their log expectations
include the group lines.

### Not done in slice 1

- (Done in slice 2: the Cheats screen is a sub-screen in the same style.)
- The PS1-primitive fallback (SDL3 GL path) lists the groups as plain text.
- Controls, Help and Walkthrough (hidden placeholders).
- OLED validation (below).

### Cheats sub-screen (P9.9 slice 2)

*2026-10-28. The old white-bezel Cheats window and its two pages are gone.*

The Cheats group opens a sub-screen like the other groups: the dimmed title
backdrop, the OPTIONS-style header plaque (CHEATS), the navy plaques with the lit
focus cue, the title lettering with its shadow, the message strip and the two
button hints (Change, Back). It has **ten rows** (Cheats master, Fill bag,
Instant win, Battle now, Cheat saves, Money, Warp, Item id, Item count, Give
item), six on screen, so it **scrolls** with the cursor and shows an amber arrow
at the end that has more. The old page 1 / page 2 split is the scroll: the rows,
their order, their keys and their behaviour are the ones of
[Cheats (P6.20)](#cheats-p620) (Down from Cheat saves goes on to Money, Up from
Money goes back); Left / Right and L1 / R1 step Money, Warp, Item id and Item
count as before. Rows 2 to 10 are greyed while the master switch is off, with
their values. Item id and Item count are drawn as numbers in the title lettering.
The labels, values, area names and message lines are the same pre-rendered
lettering as before in the five EU languages; the generator now checks them
against the sub-screen limits (row label 290 px, value 236 px, message line
690 px: all of them fit, no lettering changed).

Entry and exit did not change: the Cheats group, or Square / Y from any list,
title only (in game the log says `cheats are locked in game`); Back (Triangle, X)
returns to the group list with the cursor on Cheats. The input and the cheat
logic stay in `port/options/cheats.c` (`dw3_cheats_tick`); only the view it hands
to the overlay changed (`dw3_optlist_page(DW3_OPTGRP_CHEATS, scroll)`, ten values,
the cursor is the row). `port_options_groups.h` has the group with 10 rows
(`DW3_OPTGRP_MAX_ROWS` is 10) whose `rows[]` are cheat rows, not registry rows;
`DW3_TITLE_OPTION_ROWS` is 10. The PS1-primitive fallback keeps its plain text
panel.

**Opening a group no longer rasterizes (slice 3, same PR).** The owner's
preview.47 log (`dw3.log`, US) shows that value changes and cursor moves were
already `dirty rects=3 ... compose=2-3 ms`. The 2.7 MB `rebuild` lines (compose
48 to 81 ms on the Switch, upload about 3 ms) came **right after
`dw3-port-options: group open <name>`**, at the title and in game: the first
visit of every group composed its whole static layer (dim backdrop, header,
plaques, labels, strip, hints) on the frame thread; only the second visit was a
`cached` copy at 3 ms. (An earlier version of this section blamed the cheats
master switch; the log shows it was every group open. The master switch change
below is a separate, smaller case that is now also a dirty-row update.)

Now a sub-screen's layer is **two parts**. The part every group shares (the dim
backdrop, the message strip frame and the Change / Back hints) is one layer per
layout (title, in game; cache page `DW3_TITLE_OPTIONS_PAGE_SUB_BASE`), composed
ahead of time by the same low-priority prewarm worker as the group lists (Switch
core 2), after them. Opening a group copies that layer (a 2.7 MB `memcpy`) and
draws only the header plaque with the group's name, up to six plaques with
their labels and the Cheats arrows over it
(`dw3_pc_title_overlay_options_static_part`, part 2). On a desktop CPU that is
0.5 to 1.2 ms (a raster was about 7 ms); the Switch is several times slower, so
the target is **under 10 ms of compose** instead of 48-80 ms. The result is the
byte-identical layer a full raster gives (the unit test compares every group,
cursor and value with the reference render), and it is kept in a slot, so
the second visit is still a `cached` copy. Only if the shared layer is missing
(the worker has not finished, or it was evicted) does the open compose it first
(a `rebuild`, as before).

Memory: the cache holds **six** slots of 2.76 MB instead of four (the group
list at the title and in game, the shared sub-screen layer at the title and in
game, the group on show, one spare): **+5.3 MB** (15.8 MB held with all six
allocated, 10.5 MB before). The slots are allocated on first use and each one
logs `dw3-port-options: options layer slot N allocated (K of 6, X MB held)`.
The shared layers and the group lists are the last to be evicted. The in-game
open path (group list from the prewarmed layer, about 28 ms with the 2.7 MB
upload) is unchanged; the extra prewarm work is on the worker.

Logs per group open: `dw3-port-options: group layer page N built over the shared
layer in X ms` (or `rebuild (no shared layer yet) in X ms`), then the usual
`dw3-title-overlay: options update #N group-over-base|cached|rebuild rects=1
bytes=2764800 compose=N us upload=N us`; the worker logs `prewarm sub-screen
title ready in N ms (worker)` and `prewarm sub-screen in-game ready in N ms
(worker)` after the two list lines.

**The Cheats master switch and scroll.** The Cheats labels and scroll arrows are
drawn per update (layer `DW3_TITLE_OPTIONS_LABEL`), one static layer serves every
scroll offset, and the master switch is not part of its key. So the master
switch, a scroll step, a cursor move and a value change on that sub-screen are
`dirty` updates (at most 9 rects, no raster).

Tests: `tools/tests/options_cache_main.c` renders every cursor and value of the
sub-screen at all five scroll offsets, master off and on, in five languages
against the full reference render, and fails when a scroll step or a master
switch is not a dirty-row update. Replays `us|eu-cheats-screen` (checkpoints
`cheats-p1`, `cheats-p2`) and `us|eu-cheats-screen2` (`cheats-warp`, the list
scrolled; `cheats-give`, the end of the list) now compare pictures.

### OLED checklist, groups and sub-screens (owner)

See [OLED_TEST_PLAN](OLED_TEST_PLAN.md) A5, A9 to A12 and their proof lines.

## Region at start row (P51 item 4)

A sixth row of the Gameplay group, **only in the combined image** (`DW3_VERSION=both`;
the plain US and EU images have no other region to start, `DW3_OPT_REGION_ROW` in
`port_options_groups.h`). Registry row 10, **Region at start: Ask / US / EU**
(Confirm cycles, Left / Right step). It is stored as `region_default` (`ask`, `us`,
`eu`) in the **top-level** `sdmc:/switch/dw3/port_options.cfg`, the picker's file,
through `dw3_region_cfg_write` (which refuses to overwrite a file it cannot read, keeps
the other keys and writes `end=1` last); the region folder's own file never gets it.
The folder is the parent of the region's config folder, or `DW3_CONFIG_DIR` when that
is set (the replays keep both files in one folder). It takes effect at the next start;
a change in game never changes the running region, and the row is not locked in game.
Ask = the picker every start (3 s countdown), the fresh-install default.

Logs: `dw3-options: region_default us read from <dir>/port_options.cfg` at boot and
`dw3-options: region_default eu saved to <dir>/port_options.cfg (takes effect at the
next start)` per change (`... NOT saved ...` when the file cannot be read or written);
the boot picker's own `dw3-region: stored region_default=... minus_held=...` line
follows. The lettering (Region at start, Ask in five languages, US, EU) is generated
by `tools/generate_pc_title_lettering.py` (`REGION_TEXT`). Tests:
`tools/tests/test_options_persist.py::test_region_row_*` (the registry built as the
combined image's game copies are, `-DDW3_REGION_IMAGE=1`), replay `us/eu-options-region`
(tier `both`: `tools/replay_tests.sh --only us-options-region --only eu-options-region
--bin-both build/pc-both/dw3-pc`).

**The hold-to-pick buttons** (a try, unverified on the console): at launch the combined
image samples the pad for 12 frames; **Minus** (hbmenu or Sphaira may take it) or **ZL and
ZR held together** ask for the picker, when both regions are present and a region is
stored. The line `dw3-region: pick buttons seen minus=M zl=Z zr=R (12 frames)` says what
the game saw, so one run settles whether the launcher passes ZL / ZR. The small launcher
(`platforms/switch/launcher`) takes ZL + ZR the same way. The Options row is the supported
way; `region_default=ask` needs no button.

## Auto-save row (P6.31)

**Nine rows (P6.31).** The **Auto-save** row (OFF / Each area, key `autosave`) is
row 9, appended so the registry indexes of the first eight stay
(`DW3_PORT_OPT_AUTOSAVE` = 8). It is a switch row: Confirm toggles it (Left / Right
do nothing), the choice sound plays, the file is written at once, and
`dw3-port-options: auto-save enabled|disabled` is logged. Nine rows need a tighter
list: a row is **34 px high on a 38 px pitch** (it was 38 on 42), the first row at
y 150 as before and the last ending at y 488, 20 px above the message rule at y 508;
the 26 px lettering, the pointer and the focus box keep their size (the focus box
interior is 28 px). The Cheats pages share the new row geometry (they are
log-only in the replays, so no reference changed); the Yes / No window keeps its own 38 px
row. The cache sends one 696x34 rect per row on a cursor move and a 268x34 cell
for a value change; at most 3 + 6 rects per update, so `DW3_OPTIONS_MAX_DIRTY` stays
10, and `tools/tests/test_options_cache.py` renders every state of the nine rows
against the full reference render. The window is full again: a tenth row needs the
P9.9 redesign (or a sub-screen like the Cheats one).

The row's label, its value word and its message line are in the five languages of
the EU disc (English on the US disc), generated by
`tools/generate_pc_title_lettering.py` (`opt_as_row_<lang>`, `opt_as_area_<lang>`,
`opt_as_desc_<lang>`): `Auto-save` / `Sauvegarde auto` / `Auto-Speichern` /
`Autoguardado` / `Salvataggio auto`, value `Each area` / `Chaque zone` / `Jedes
Gebiet` / `Cada zona` / `Ogni area`, OFF is the shared word. The other eight rows
stay English on both discs, as before. What the row does, its defaults and the
checklist: [P6.31_AUTOSAVE.md](P6.31_AUTOSAVE.md).

## Button layout row (P9.8)

**Ten rows.** **Button layout** is registry row 9 (`DW3_PORT_OPT_BUTTON_LAYOUT`,
appended so the first nine indexes stay), alone in the **Controls** group, which
is listed between Cheats and Help (so the group list now has six plaques, seven
with Cheats on, and scrolls only then). It is a stepped row: Confirm toggles
PlayStation / Nintendo, Left / Right step, the choice sound plays, the file is
written at once and the map changes before the next pad poll. The in-game and
the battle Options reach it too: the layout row is not locked anywhere.
Lettering (row, the two values, the message line `Nintendo: A confirms, B
cancels. Applies now.`, the group name and its line) is in the five languages
of the EU disc, generated by `tools/generate_pc_title_lettering.py`
(`opt_row_bl_`, `opt_val_bl_ps_`, `opt_val_bl_nin_`, `opt_desc_bl_`,
`opt_grp_controls_`, `opt_grpdesc_controls_`); `PlayStation` and `Nintendo` are
product names. What it does and why the preset moves three buttons, not two:
[CONTROLS_BUNDLE sections 2.2 and 2.3](CONTROLS_BUNDLE.md). Owner-facing answers
used while the owner is away (Q1 A confirms and B cancels, Q2 hotkeys follow the
layout, Q3 glyphs later) are recorded there as orchestrator defaults.

Logs (`dw3.log`): `dw3-controls: map loaded layout=... source=cfg|default|env`,
the map and hotkey lines, `layout changed ... (screen)`, `first press
source=A -> logical cross`; the screen's own `dw3-port-options: button layout
nintendo` and `dw3-options: saved ...`. Unit tests: `test_button_map.py`,
`test_options_persist.py` (the layout cases), `test_pad_host.py`. Replay:
`us/eu-controls-layout` (new references) and the seven `*-nintendo-*`
scenarios ([P6.15](P6.15_REPLAY_TESTS.md)). The Options hint row keeps the
PlayStation marks (PC) or the letters B and X (Switch) in both layouts until
phase 3 (P10.6) makes them follow the map.

## Cheats (P6.20)

A test aid for hardware testing: reach a battle, or a full bag, without
replaying hours of the game. Not a player feature. Decisions of the owner
(2026-10-24): it ships in **every** build (release and nxlink, US and EU) behind
a master switch that is off by default; labels in the five EU languages
(English on the US disc); a save written after a cheat is flagged; and a
build-time option removes the whole feature. ROADMAP P6.20 has the history.

### Reaching the screen

On the Options screen press **Square** (PC **Z**, Switch **Y**), or open the
Cheats group of the list. Since P9.9 slice 2 the Cheats screen is a sub-screen
of the group list in the title menu's style (the paragraphs below about "the
same window" and the 38 px rows describe the screen before that and are kept for
the history); **Triangle** (PC S, Switch X) goes back to the list.
The Options screen had nine rows since P6.31. The hint "Square/Y Cheats" is drawn
left of Change **only while the master switch is on**.

Why a hint that appears late and not a ninth row, a second page or an
always-visible hint: the eight rows fill the window (38 px rows on a 42 px
pitch, the last ends at y 482), and every pixel added to the Options screen
changes the P6.15 references (`*-options-persist`, `*-volume-persist`,
`*-battle-cursor-persist` capture it) that must not be refreshed. With the
master off the Options screen is pixel-identical to the one before P6.20, so a
player who never opens the Cheats screen sees no change at all, and the dirty
rectangle cache is untouched. Square is the free pad button on that screen (Cross
and Circle step a row, Triangle closes, Left / Right step the volumes). The cost
is that the entry is not discoverable until someone knows the key; it is in
[HOTKEYS.md](HOTKEYS.md) and the release notes.

### Rows

| Row | Kind | Values | Does |
|---|---|---|---|
| Cheats | master switch, `cheat_enabled` | OFF / ON | OFF: every cheat below is inert and greyed. Turning it off disarms the one-shots |
| Fill bag | one-shot | OFF / Ready | At the next field menu screen the bag gets 99 of every usable item, weapon, armour and accessory (ids 43..359; the key items 1..42, 360, 395..402 and the card packs 361..394 are left out). Uses patch `0070`'s `DW3_FORCE_ITEMS` hook; used up when applied |
| Instant win | toggle, `cheat_instant_win` | OFF / ON | Each battle ends as won on its first turn (once per battle; the report and the rewards follow as after a real win) |
| Battle now | one-shot | OFF / Ready | The next footstep in a random-battle area starts a random battle, even with Encounter rate OFF (the countdown is forced to its end); used up when the battle starts |
| Cheat saves | read-only | None, or 1..6 | How many saves are marked "cheat used" (see below) |

**Page 2 (PR 2; since P9.9 slice 2 it is the lower half of the scrolling list, with no "1/2" title).** Down from "Cheat saves" goes on to page 2 (the title reads
"CHEATS 1/2" / "CHEATS 2/2"; Up from its first row goes back). Its rows are
one-shots like Fill bag and Battle now: armed here at the title, applied by the
**field at its first idle frame** (the moment the field menu may open: no
event, no menu, no fade), then used up and logged. The Options screen is only
reachable from the title, and a new game or a load overwrites the game state, so
nothing can be applied on the spot.

| Row | Kind | Values | Does |
|---|---|---|---|
| Money | one-shot | OFF / +10,000 / +100,000 / MAX | Adds the amount to the money the player has (never above the game's maximum, 9,999,999); MAX sets 9,999,999. Cross / Circle / Left / Right step the value and wrap. Patch `0118` |
| Warp | one-shot | OFF / 13 areas | The field leaves for that area as an exit slot does (`FIELDSTG_leaveField(mode, 0, 0, 0, 0)`): the stage's first start point, exactly what the debug stage select (`src/debug/stage_select`) gives with `requestMode(mode, 0)`. Areas, by game mode: Asuka City 0x200, Digimon Lab 0x206, Digimon Arena 0x209, Central Park 0x21D, Wire Forest 0x21E, Tyranno Valley 0x22B, Chinlong City 0x22E, Bulk Swamp 0x233, Suzaku City 0x23E, Noise Desert 0x248, Byakko City 0x25D, Snow Mountain 0x262, Genbu City 0x26F. Patch `0120` |
| Item id | a pick | 43..359 | Left / Right +/- 1, L1 / R1 (Switch L / R) +/- 10, stops at the ends. Shown as a number: item names are text of the game's own archives, which this screen (pre-rendered lettering) does not draw. Default 43 |
| Item count | a pick | 1..99 | The same keys. Default 10 |
| Give item | one-shot | OFF / Ready | Adds Item count of Item id to the bag (at most 99 held). Patch `0119`. Unlike Fill bag (which sets 99 at the next field menu screen) it adds, and it does not wait for the field menu |

Choices made without the owner (default chosen, owner can change): the **Item
id range 43..359 is the same as Fill bag's**, so no key item (1..42, 360,
395..402: they drive the story) and no card pack (361..394) can be given; the
count is added to what the player holds; the item and warp picks are not kept
between runs; the 13 areas are the four cities, the Digimon Lab and Arena,
Asuka City and one random-battle area per region (Central Park, Wire Forest,
Tyranno Valley, Bulk Swamp, Noise Desert, Snow Mountain) so that Battle now has
somewhere to run; the area names are plain descriptive labels (translated in
the five languages), not the game's own text. A warp is the stage select's
entry, so a story-dependent map may look unfinished (events that need a flag),
the same as in the debug stage select.

Why money, items and warp are applied by the field and not at once: the
game state exists only once a game is running. **Why the field is a safe
place:** the warp leaves like an exit (the field closes through
`FIELDSTG_closeField`, the next stage starts at its first start point), and
the field menu records the place when it opens, so a save made after a warp is
a normal save of that map (with the cheat mark, see below). The replay
scenarios check the arrival and that the field menu opens there.

Everything on page 1 is off by default. The toggles persist as `cheat_enabled` and
`cheat_instant_win` in `port_options.cfg` (0 or 1; anything else is logged as
not valid and read as 0), like the other keys. The one-shots are **not**
persisted: they are armed for this run only. **No cheat writes a save by
itself.** A one-shot changes the live game, and the game's own save writes it
like anything else, which is why the save mark exists. The Fill bag message line
says that saving keeps the items. Each use is logged as a `dw3-cheats:` line in
`dw3.log`.

The `DW3_FORCE_*` variables of the PC replay hooks keep working and win over
the screen (`DW3_FORCE_ITEMS`, `DW3_FORCE_WIN`, `DW3_FORCE_ENCOUNTER`), and
those hooks never mark a save: they are development aids of the replays, not
cheats of a player. The PC-only variable `DW3_CHEATS_ARM=fill_bag,battle_now`
arms the one-shots at boot for the replay tests (they still need the master on);
PR 2 adds `money_10k` | `money_100k` | `money_max`, `warp=<game mode>` (for
example `warp=0x206`) and `item=<id>:<count>` (for example `item=47:7`; ids
outside 43..359 and counts outside 1..99 are ignored).

### The "cheat used" flag on saves

A **sidecar file** `save<N>.cheats` in the memory card directory
(`sdmc:/switch/dw3/<us|eu>/memcard/` on the Switch), next to `save<N>.bin`, in the
same `key=value` format as this file, written atomically (temp file and rename):

```
# dw3-switch: slots of save0.bin written with cheats (docs/PORT_OPTIONS.md)
slot1=<offset>:<size>:<fnv1a32 hex>
end=1
```

- *Marking.* The game in progress is marked when a cheat changes it: Fill bag
  applied, Instant win applied, a Battle now battle started, or (PR 2) Money,
  Give item or Warp applied. When the memory
  card screen then writes a slot, the slot's entry records the offset and size of
  the bytes written and their FNV-1a hash (patch `0077` calls
  `dw3_cheats_save_written` at the single place that writes a save). Writing a
  game that is not marked removes the entry, and the file is deleted when no
  entry is left. Loading a marked save keeps the game marked, so a cheated game
  stays flagged through save, load, save. Starting from the title (a new game
  or a load) clears the mark first.
- *Reading.* At boot, and on the Cheats screen, an entry counts only while the
  slot still holds the bytes it was made for (read back through the memory card
  host). A slot later overwritten by a build without cheats, or a copied
  `saveN.bin`, is not reported. The count is the last row of the Cheats screen,
  and the boot log has one `dw3-cheats: save0.bin slot 2 was written with cheats`
  line per marked slot.
- *Where it is not shown yet.* The game's own memory card screen (`STGMCARD`)
  does not display it: that needs decomp work (a new indicator in a layout the
  port does not own) and is a follow-up. PR 1 stores the flag, shows it on the
  Cheats screen and in the log.

**Why not a byte in the save.** `GameState` has two bytes, `pad1` and `pad3`
(bytes 1 and 3, outside the checksum that starts at byte 4), which the decomp
comments as "nothing reads or writes it". They were rejected:

- the ROADMAP rule (also P7.9) is that a cheat must not edit the save; the save
  image stays exactly what the game writes;
- the PS1 / emulator memory card (P9.6) shares that image, and a flagged byte
  would travel with it into a card that other tools read;
- "unused" is a claim about the code that was decompiled, not about every
  tool, patch or later format (P7.12 freezes the save format);
- a sidecar works with no decomp change beyond the one call, is trivially
  removed, and cannot corrupt a save.

The cost: the mark lives beside the file, so copying only `saveN.bin` loses it
(the hash check makes the opposite, a stale mark, harmless). It is a
transparency aid, not tamper protection.

### Build option

CMake `DW3_CHEATS`, default **ON**, in `platforms/pc/CMakeLists.txt` and
`platforms/switch/CMakeLists.txt`. OFF compiles the feature out: no Cheats
screen, no hint, no hook, no save mark, and the game is the one before P6.20.

| Build | How |
|---|---|
| Switch, cheats included (default, releases) | `bash tools/build_switch_game.sh` (add `--eu`, `--nxlink`) |
| Switch, no cheats | `bash tools/build_switch_game.sh --no-cheats` (build dir suffix `-nocheats`, e.g. `build/switch-eu-nocheats/`) |
| PC, no cheats | `cmake -S platforms/pc -B <dir> ... -DDW3_CHEATS=OFF` |

The `-DDW3_CHEATS=...` value is always passed to CMake and checked in the cache
by the Switch script, so a build directory never keeps a stale value. The option
defines `DW3_CHEATS=1` for the host and, through `port/CMakeLists.txt`, also for
the decomp objects (`_DW3_GAME_DEFS`): the overlays and stages are compiled by
custom commands that see only that list, and without it patch `0077`'s field and
battle hooks would be silently compiled out. Every line patch `0077` adds is
inside `#ifdef DW3_CHEATS` (see [PATCHES.md](PATCHES.md)).

**Bit-identical when OFF (checked).** The OFF build and a build of `main`
before the feature have byte-identical `.text`, `.rodata`, `.data` and `.bss`,
the same section sizes, no extra symbol and no `dw3-cheats` string (PC, US and
EU; `size`, `objcopy -O binary --only-section` and `nm`). To keep that true: the
psyz log macros pass `__LINE__`, so a line inserted *above* a log call in a
compiled file changes the code. Every insertion in `port_options.c` therefore sits
after its last log call (tick, draw, end of file), the cheats declarations come
in through `port_options.h`, and `memcard_host_dir()` is at the end of
`memcard_host.c`. Do not add lines above log calls in `port_options.c`,
`pacing.c` or the host files without re-running the comparison. With the master
**switch** off (the default of an ON build) the behaviour is the same game too:
the full P6.15 replay suite passes on the ON build against the unchanged
references.

### Languages and lettering

The Cheats labels are pre-rendered lettering like the Options ones, in the
language the Options screen uses (`options_lang`, from the EU language capture in
`port/options/title_logo_capture.c`; US is English). ON, OFF and the numbers
reuse the Options words. To change a word or add a language:

1. Edit the `CHEAT_TEXT` table in `tools/generate_pc_title_lettering.py`
   (`en`, `fr`, `de`, `es`, `it`: title and title2 ("... 1/2" and "... 2/2"), hint,
   Change, Back, Ready, None, the five row labels and message lines of page 1 and
   `rows2` / `desc2` of page 2, and the 13 names of `warp`, in the order of
   `g_warp_modes` in `port/options/cheats.c`; the money values and the digits of
   the two numbers are language-independent entries of `CHEAT_LETTERS`).
2. `python3 tools/generate_pc_title_lettering.py` (needs Pillow and the DejaVu
   fonts in `/usr/share/fonts/truetype/dejavu`). It rewrites
   `platforms/pc/src/title_overlay_lettering.inc`; the entries before P6.20 stay
   byte-identical, and the cheat ones sit in one `#ifdef DW3_CHEATS` block at the
   end (the file grew from 403 KB to 1.27 MB, and with page 2 to 2.5 MB; OFF
   builds do not compile them).
3. Check the widths: a message line must fit 696 px and a row label 360 px (the
   generator writes `title_opt_cht_*_width`). The German and Spanish Fill bag
   messages were shortened to fit.
4. Run `tools/tests/test_options_cache.py` and take the screenshots of each
   language (`xvfb-run`, `DW3_REPLAY_XSHOT=1`; the replay `cheats-screen` scenario
   is the route).

### Code

`port/options/cheats.c` (store, screen logic, hooks), `port/options/cheat_saves.c`
(sidecar), `port/options/cheats_internal.h`, API `port/include/port_cheats.h`;
the rows are drawn by `options_list_screen()` in `platforms/pc/src/title_overlay_art.c` (P9.9 slice 2; `cheats_screen()` is gone).
The store starts lazily on the first call (`dw3_cheats_init(memcard_host_dir())`),
so the hosts needed no change. PR 2: the rows, the three `dw3_cheats_take_*`
hooks and the warp table (`g_warp_modes`) are in `cheats.c`; patches `0118`
(money), `0119` (give item) and `0120` (warp) call the hooks from
`openFieldMenu` in `field_mode/field.c`; page 2 is the lower half of the
same list (digits drawn by `number()` from fixed-advance glyphs).

### Tests

- `python3 -m pytest tools/tests/test_cheats.py`: the store (defaults,
  persistence, master gating and disarming, bad values), the hooks (Fill bag
  answers the items hook once, the variable wins, Instant win once per battle,
  Battle now until a battle starts, the arm variable needs the master) and the
  save marks (mark, clear, replaced slot not counted, unwritable directory).
- `python3 -m pytest tools/tests/test_cheats_page2.py` (PR 2): page 2 rows inert
  while the master is off, the money modes and the clamp, the warp cycle and its
  area table, the item pick (steps, ten at a time, the 43..359 and 1..99 ends),
  Give item used up, the master off disarming page 2, the arm variable.
- `tools/tests/test_options_cache.py`, now run with and without cheats: the Cheats
  page (both pages) and the late hint, and that the Options pages are unchanged.
- Replays (log-only unless noted, see [P6.15_REPLAY_TESTS.md](P6.15_REPLAY_TESTS.md)):
  `cheats-screen`, `cheat-fill-bag[-off]`, `cheat-battle-now[-off]`,
  `cheat-instant-win[-off]`, US and EU; every `-off` twin proves nothing happens
  with the master off. PR 2: `cheats-screen2`, `cheat-money[-off]`,
  `cheat-warp[-off]`, `cheat-item[-off]` (the state change in the log: the
  money before and after, the arrival mode and the field menu opening there,
  the bag count before and after).
- Switch: compiled for US and EU, normal, `--nxlink` and `--no-cheats`.

### OLED checklist (owner)

1. Options screen: Y does nothing visible with the master off (no hint); the
   screen looks as before.
2. Open the Cheats screen with **Y**: the title and rows in the game language
   (EU: all five), Triangle (X on the Switch) returns.
3. Cheats ON: rows 2-4 become active, the "Square/Y Cheats" hint shows on Options.
   Close the game, relaunch: Cheats and Instant win keep their state, Fill bag
   and Battle now are back to OFF.
4. Fill bag Ready, leave to the game, open the field menu: Items shows 99 of
   each (no key items).
5. Instant win ON, enter a random battle: it ends as won on the first turn.
6. Battle now Ready: the next steps in a battle area start a battle even with
   Encounter rate OFF.
7. Save in the memory card screen after one of these, relaunch: Cheat saves
   shows 1, `dw3.log` has `dw3-cheats: save... was written with cheats`; save a
   clean game over that slot, relaunch: None.
8. `--no-cheats` build: Y does nothing, no `dw3-cheats` lines in `dw3.log`.

### OLED checklist, page 2 (owner)

PR 2. Build with the default (cheats included), master switch ON on page 1.

1. Cheats screen: Down from "Cheat saves" goes on to Money and the list scrolls
   (the title reads CHEATS; the page 2 rows are the lower half, the arrows show
   there is more above or below; labels in the game's language, all five in EU); Up from Money goes
   back. With the master OFF the page-2 rows are greyed and do nothing.
2. **Money:** arm +10,000 (Cross twice steps +10,000, +100,000; Left / Right
   too), go to the title's New Game or Continue and into a field: the money in
   the field menu (Start) shows the amount added. MAX: 9,999,999. The row is
   OFF again the next time you open the screen.
3. **Warp:** pick "Digimon Lab" (Cross steps through the areas), start or load a
   game: on the first idle moment in the field (not during an event) the screen
   fades and the player arrives in the Digimon Lab. Open the field menu there
   (Start), save at a memory card screen, reload: the player is in the
   Digimon Lab. Try Central Park or the Bulk Swamp too and walk: random battles
   work.
4. **Give item:** pick an id (Left / Right, L / R by ten) and a count, set Give
   item to Ready, load a game: in the field menu, Items (or Weapons, Armor,
   Accessories for equipment) lists it with the count; the item is not a key
   item. A second arm adds again, up to 99.
5. Each of the three sets the cheat mark: save, relaunch, "Cheat saves" shows
   the slot and `dw3.log` has a `dw3-cheats: ... changed the game` line.
6. `--no-cheats`: no Cheats screen, no `dw3-cheats` lines.

## Quit and return to title (P6.27, P6.28)

Two entries that end the running game on purpose, each behind a Yes / No
window. Owner request of 2026-10-24; the questions that were open were
answered with the defaults below (**default chosen, owner can change**).

| Entry | Where | Needs | Does on Yes |
|---|---|---|---|
| **Quit game** (P6.28) | Title menu, fourth and last row, a little apart from Load Game / Continue | Always there (PC SDL3 GPU and the Switch; the PC GL path has no native title menu and keeps three rows) | Flushes the dirty save slots, then ends the session: on the Switch the NRO returns to hbmenu, on PC the window closes |
| **Return to title** (P6.27) | Field menu (START in the field, and the same menu inside the status screens), last row | Always there (same builds as Quit game); independent of the Restart hotkey setting, which only gates the chord | The P6.9 reset: dirty save slots flushed, a failed flush cancels, the game goes back to its boot flow |

Words: the field row reads TITLE SCREEN (US and English), RETOUR TITRE,
TORNA AL TITOLO, TITELBILD, VOLVER AL INICIO, in the game's own font and
capitals like the other rows. The title plaque is **Quit game**, Quitter,
Beenden, Salir, Esci (the same lettering as the Options plaque, bold 29 px
DejaVu Sans Condensed). The window is lettering of the Options screen's style
(white bezel, navy body, the focus box and pointer, the Select / Cancel hints
with the same button caps), in the five EU languages (the US disc is English).

### Defaults chosen (owner can change)

1. **Confirmation, cursor on No.** Both entries ask "Quit the game?" or
   "Return to the title screen?"; the cursor starts on No. Cross answers,
   Up / Down / Left / Right move the cursor, Triangle or Circle cancel. A
   press that opens the window does not also answer it.
2. **Field menu only, not in battle.** Return to title is a row of the field
   menu. The battle menu has none, and the other screens (shops, memory card,
   card game) are not touched.
3. **Warns that progress is lost.** Both windows say "Progress since your last
   save will be lost." (translated). The game's saves are manual; the reset
   and the quit write only the dirty port save slots, as P6.9 does, and never
   save the game for the player.
4. **Return to title is the P6.9 path, unchanged.** `dw3_port_restart_request()`
   asks for it and the next `dw3_port_restart_poll()` runs the same flush and
   the same reset as the chord. A failed flush cancels the reset and the game
   stays where it was. On the EU disc the reset lands on the language select
   (P6.9), not on the title; the title follows once a language is chosen.
5. **Quit flushes like HOME, then exits.** Yes flushes the save slots
   (`dw3-port-quit: saves flushed; asking the host to quit` in `dw3.log`) and asks
   the host to end the session: the Switch sets the same quit flag as HOME's exit
   request (`dw3_nx_set_quit_requested`), the PC pushes the window's close
   event. The game loop returns from `dw3_game_main()` and the existing exit
   code runs (`memcard_host_shutdown`, the options flush). A failed flush
   cancels the quit (`dw3-port-quit: save flush failed; quit cancelled`), where
   HOME has no choice. The HOME path itself is unchanged.
6. **Quit is the last title row.** It first sat apart (12 px above it where
   the others have 4); the owner's preview.38 OLED check found the spacing
   uneven, so it now has the same pitch as the other rows (the Yes / No
   window still guards the action).
7. **The release README says Quit returns to hbmenu.** One line in
   `platforms/switch/release/README.txt`.
8. **Return to title is always visible**, decoupled from the Restart hotkey
   setting (orchestrator decision, 2026-10-25: it is a normal pause-menu feature;
   a first version hid it behind the setting). The field menu screens that show it
   have new replay references (see the PR for the list).

### Design

- `port/options/confirm.c`, `port/include/port_confirm.h`: the window's state
  machine (open, cursor, answer), the two entries' switches and the field row's
  text. Platform independent; the pad is the game's (`PAD_PRESSED`), so the
  replay recordings drive it.
- The window is drawn by the native overlay: `dw3_pc_dialog_art()` in
  `platforms/pc/src/title_overlay_art.c` (shared by the PC and the Switch)
  renders a 768 x 348 RGBA texture only when its kind, cursor or language
  changes; `title_menu_overlay.c` (SDL3 GPU) and `nx_title_overlay.c` (GLES)
  upload it and draw it over the 4:3 middle of the picture, so it also shows
  over the field. The Switch builds only the shader and the quad buffer for it
  outside the title (not the title canvas).
- The title's atlas has six states now (PRESS START, four focused rows, the
  Port options screen): `DW3_TITLE_ART_OPTIONS` moved from 3 to 4. The plaques
  are 48 px high on a 52 px pitch (they were 54 on 60) so a fourth fits above
  the credits; the credit plate and the Port options screen are unchanged.
- Patch `0123` is the game side (the two menus), `0124` a test hook; see
  [PATCHES.md](PATCHES.md). No decomp file is touched when the entries are off.
- The seven-row panel: the field menu's options panel has a sprite for five
  rows (`0x1C`) and six (`0x1B`, with the card folders). With Return to title
  that is six and seven. Seven has no sprite, so the six-row panel's bottom
  edge is covered and redrawn one row lower with five rectangles in the
  panel's own colours. Since P9.7 the Options row sits above Return to title,
  so the lists are seven and eight rows and the same drawing is repeated for
  each extra row ([Options in game](#options-in-game-p97)).

### Languages and lettering

The window's and the plaque's words are pre-rendered lettering like the
Options screen's: `DIALOG_TEXT` in `tools/generate_pc_title_lettering.py`
holds the five languages (en, fr, de, es, it). To change or add a word edit
that table, run `python3 tools/generate_pc_title_lettering.py` (Pillow and the
DejaVu fonts) and commit `platforms/pc/src/title_overlay_lettering.inc`; the
entries before it stay byte-identical. Keep each message under 650 px. The
field row's text is ASCII capitals in `dw3_port_field_return_label()` (the
game's font codes: A to Z are 14 to 39, a space is the two bytes 01 01); an
accent there would need the font's two-byte code, which is why the words avoid
them.

### Tests

- `tools/tests/test_confirm.py`: the state machine (cursor, Yes / No / cancel,
  one answer per press, the flush before the quit, a failed flush cancelling
  it, the P6.9 request) and the artwork of the window in five languages and the
  four title states (`dialog_art_main.c`, which also writes every state as a
  PPM when given a directory).
- Replay (PC, US and EU): `us/eu-title-quit` (Down to Quit game, No, again, Yes:
  the process ends by itself with status 0 and the flush line),
  `eu-title-quit-fr/de/it/es` (plaque and window in the other languages),
  `us/eu-return-title` (START in the South Sector field with the party of the
  forced battle, Return to title: No, again, Yes, back at the title, or on the
  EU disc the language select), `us/eu-return-title-rows7` (the card folders
  row, seven rows, cancelled). See
  [P6.15_REPLAY_TESTS.md](P6.15_REPLAY_TESTS.md).

### OLED checklist, Quit and Return to title (owner)

US and EU (at least two EU languages), a Switch OLED:

- [ ] Title: Down goes New Game, Options, Load Game (Continue), **Quit game**; it
      sits a little lower and is the last row; Down again does nothing.
- [ ] Cross on Quit game: the window opens with the cursor on **No**; Cross (or
      Triangle) closes it and the title is as before; the Select / Cancel hints
      show the Switch letters (B and X).
- [ ] Open it again, move to **Yes**, Cross: the NRO ends and hbmenu comes back;
      `sdmc:/switch/dw3/dw3.log` ends with `dw3-port-quit: saves flushed; asking the host
      to quit`; the card files (`save0.bin`, `save1.bin`) are intact, and the next
      launch Continues from a save made before.
- [ ] HOME and "close the software" still exit as before (`applet exit` line).
- [ ] In the field press Plus: the menu has a
      last row (TITLE SCREEN, RETOUR TITRE, ...); with Card folders (after the card
      folder item) the panel has seven rows and the last is inside it.
- [ ] Cross on it: the window, cursor on No. No: the menu is as before. Yes: the
      game goes back to the title (US) or the language select (EU); `dw3.log` has
      `dw3-port-restart: requested from the field menu` then
      `saves flushed`; a save written before still Continues.
- [ ] The row is there with Restart hotkey OFF and ON.
- [ ] Text fits in the other EU languages (the plaque and both windows).

## Options in game (P9.7)

Slice 1 of ROADMAP P9.7: the port Options screen from the **field menu**.
Battle is slice 2 ([Options in battle](#options-in-battle-p97-slice-2)). Owner's brief (2026-10-25, the owner away): work
autonomously and pick the conservative option for each open question; every
default below is **default chosen, owner can change**.

### What the player sees

Start in the field opens the game's menu. Its list now ends with **OPTIONS**
and then TITLE SCREEN (OPTIONS, OPTIONS, OPTIONEN, OPZIONI, OPCIONES on the EU
disc, in the game's own font). Cross on OPTIONS opens the same Options screen as
the title, drawn over the menu and the field by the native overlay (PC SDL3 GPU
and the Switch). Up / Down choose a row, Left / Right and Cross change it,
Triangle (B) goes back to the menu. Changes are saved at once in
`port_options.cfg` ([Persistence](#persistence)).

> **P38 (black square on the Switch).** The title's edge fade ends at 255 and the
> Switch overlay kept that level, so the Options screen opened from the field
> was drawn fully darkened: a black square. Replays start in the field and never
> run the title, so neither the PC nor the Ryubing `field-options` runs saw it.
> `nx_title_overlay.c` now ignores the fade for an in-game Options screen and
> forgets it once the title is off screen (`nx_title_fade.h`, unit test
> `tools/tests/test_title_fade.py`). OLED check: Title, Continue, START, OPTIONS.

### Defaults chosen (owner can change)

1. **Entry point: a row of the field menu**, not a hotkey (option b of the
   P9.7 note), just above Return to title and built the way patch `0123`
   built that row: the text is in the game's font (ASCII capitals, one table
   per language in `dw3_port_field_options_label`), shown in the five EU
   languages and in English on the US disc. The menu could take the row
   cleanly, so the hotkey fallback (option a) was not needed.
2. **The menu's panel.** The game's panel sprites cover five rows (`0x1C`) and six
   (`0x1B`, with the card folders row). With Options and Return to title the
   list has seven rows, eight with the card folders. `drawFieldMenuPanelRow`
   (patch `0123`, one extra row) now covers the six-row panel's bottom edge and
   redraws it lower, with a divider per row, for any number of extra rows. The
   eight-row panel ends at y 165 of the 240-pixel picture and does not touch the
   money panel on the left.
3. **Scope: the field menu only.** Battle has no entry yet. The row is in the
   menu of the field and in the same menu inside the status screens
   (`MODE_STATUS`), like Return to title.
4. **Pausing: the menu's own.** The field menu is already a paused state
   (the field task, its timers, the random-battle countdown and the auto-save
   check wait while `FIELDSTG_state.menuOpen` is set). Opening the screen from
   inside it inherits that, and the menu task simply does not read the pad while
   the screen is up (`updateFieldMenu` calls `dw3_port_field_options_tick()`
   instead of `chooseFieldMenuOption`). On Triangle the screen closes in its own
   tick, so the same press cannot also cancel the menu: no input frame leaks to
   the menu or the game. Music and the sound script keep running (the screen
   plays the game's menu sounds, so the SFX volume row is audible).
5. **Live apply**, row by row (what each one reads, and when):

| Row | In game | Why |
|---|---|---|
| Widescreen | **Greyed, locked** ("Locked in game: change it at the title screen.") | The gutter, the 16:9 map streamer (15 host slots, patch `0052`), the camera and the battle layout are set up when a mode loads. Flipping the flag in the middle of a map would leave them half old, half new. Conservative: it changes at the title. |
| Fast-forward | Applies at once | `dw3_port_turbo_poll` and `dw3_port_turbo_speed` read it every frame. R3 still switches the speed on and off; OFF disables R3 and drops an active fast-forward. |
| Restart hotkey | Applies at once | `dw3_port_restart_poll` reads it on every frame. Trivial, so not greyed. |
| Music volume, SFX volume | Applies at once | The step goes to the mixer (`dw3_port_audio_apply`) on the change. |
| Encounter rate | Applies at once | `dw3_port_pacing_encounter_step` reads it per footstep. A battle already started is not touched. |
| Text speed | Applies at once | Read when a message window opens its page. A page already typing finishes as it was. |
| Battle cursor | Applies at once | Read when the battle command menu opens. |
| Auto-save | Applies at once | Read at each area change (never while the menu is open, so not from here). |
| Cheats (Square / Y) | **Greyed, locked** | Cheats are armed at the title by design (P6.20). The hint is drawn grey when the master switch is on; Square does nothing and logs `cheats are locked in game`. |

6. **Persistence: as today.** A change is written to `port_options.cfg` the
   moment it is made (`po_save`), the same code as the title screen.
7. **Not done in this slice (still P9.7):** the battle screen (done in slice 2,
   [Options in battle](#options-in-battle-p97-slice-2)) and a live Widescreen
   switch.

### Design

- `port/options/field_options.c`, `port/include/port_field_options.h`: the
  row's text, `dw3_port_field_options_open(language)` (sets the overlay's
  lettering language, opens the screen in game and shows it), `_tick()` (one
  frame of the screen's own pad handling, then keeps the PC overlay showing or
  hides it on close) and `_is_open()`.
- `port/options/port_options.c`: `dw3_port_options_open_in_game()` sets an
  in-game flag. While it is set `dw3_port_options_row_locked()` is true for
  Widescreen, `po_step` / `po_toggle` refuse a locked row with a log line
  (`<row> is locked in game (change it at the title)`) and no sound, and Square
  does not open the Cheats screen. The flag is cleared on close, so the title
  opens everything unlocked.
- Patch `0160` (game side): the row, its Cross, the pad hand-over and the
  panel rows; see [PATCHES.md](PATCHES.md). No decomp file is touched without
  the native overlay.
- Overlay: `dw3_pc_title_overlay_set_options_locked(mask)`
  (`title_overlay_art.c`; bit i = row i, bit 16 = the Cheats hint). A locked
  row's label and value are drawn grey, the message line says why
  (`opt_lock_<lang>` lettering in the five languages, from
  `tools/generate_pc_title_lettering.py`), and the retained canvas
  (`title_options_cache.c`) rebuilds its static layer when the mask changes
  (`options_layout` is no longer cheats-only). The PC overlay is kept showing by
  `dw3_pc_title_menu_set_state(1, -1)` each frame of the open screen; the
  Switch overlay follows `dw3_port_options_is_open()` by itself, as for the
  title. Over the field the screen dims the 4:3 middle and, in widescreen, the
  gutters, as it does over the title.

### Tests

- `tools/tests/test_field_options.py` (`field_options_main.c`): the label text
  in the game's codes (five languages), the row offered only with the native
  overlay, open / close, Widescreen refused (Cross, Circle, after Up x8), every
  other row applying at once (Fast-forward cycles, Restart hotkey is read by
  `dw3_port_options_restart_enabled`, the mixer gets the music step, Encounter
  rate, Text speed, Battle cursor, Auto-save), Triangle closing and hiding the
  overlay, the title opening with nothing locked, the values kept.
  `locked_art_main.c`: a locked Widescreen row differs from the unlocked screen
  only in its label, value and message line, in the five languages, and the
  retained canvas equals the reference render when the lock appears and while the
  cursor and the values move.
  `test_cheats.py::test_cheats_entry_is_locked_in_game`: Square in game does not
  open Cheats, at the title it does.
- Switch build in Ryubing (US and EU, 12 checks each, all pass): `us/eu-field-options-nx`
  and `us/eu-field-options-release-nx` (tier `nx`). After #268 releases the title
  overlay's textures, the Options screen opens in the field with no blank or
  garbage frame (captured 3 frames after the press), and so does the Return to
  title window. A first-use draw can be invisible for a second in Ryubing while
  it compiles the shader; see the replay doc.
- Replay (PC, US and EU; [P6.15_REPLAY_TESTS.md](P6.15_REPLAY_TESTS.md)):
  `us/eu-field-options` (two phases: the menu, Options, Widescreen refuses a
  change, Music volume 10 to 7, Encounter rate 1x to OFF, close, the menu takes
  Down and Triangle and the field goes on; phase 2 is a new process whose screen
  shows the saved values and whose log reads them back from the cfg),
  `us/eu-field-options-cheats` (master on: grey hint, Square does nothing),
  `eu-field-options-fr/de/it/es` (the row and the lock note in each language).
  `us/eu-return-title(-rows7)` press Down once more (Options sits above Return to
  title); their eight-row panel is the new reference.

### Opening without a stutter (preview.42 follow-up)

preview.41 and preview.43 OLED logs: every open of the screen logged `options update #1 rebuild
rects=1 bytes=2764800 compose=166 ms upload=2.5 ms`. The cost was the static
layer (window, bezel, rules, labels, hints) of the 960x720 canvas, drawn on the
CPU on the opening frame, and it was thrown away each time the screen closed
(`release_options_cache`). Now (`title_options_cache.c`):

- The static layer is kept per layout (title: no locked rows; in game: Widescreen
  and the Cheats hint locked; and the language, page and cheats switch), two slots,
  8.3 MB (three slots, one is the worker's). Closing the screen frees only the working canvas (`dw3_options_cache_idle`).
- While the screen is closed, a **worker thread** composes the missing layers (title
  layout first, then in game) and the frame thread never draws them: Switch core 2,
  priority 0x3B, 128 KB stack (the map precompute's pattern, `nx_title_overlay.c`);
  PC a plain thread at nice 19 (`title_menu_overlay.c`). The host asks every frame
  (`dw3_options_cache_prewarm_begin`, a short locked lookup) and starts the thread
  only when a layer is missing, e.g. after boot or a language change. The worker
  draws with thread-local state only (`title_overlay_art.c`: canvas, clip, layer and
  the layout override are `_Thread_local`), into a third slot, and publishes the
  finished layer under a spin lock held for a few instructions. It logs
  `dw3-port-options: prewarm title ready in N ms (worker)` and `... in-game ...`.
  If an open comes before the layer is ready it composes synchronously, as before
  the worker existed (nothing waits on the worker).
- An open copies the retained layer into the canvas and draws the focus box, the
  values and the description, and uploads the canvas once. A language change
  invalidates the layers (their key holds the art generation) and they are composed
  again in the background.
- The pixels are the same: a band is the same raster with a clip (`box` now clamps to
  the clip first), and `tools/tests/options_cache_main.c` proves every cached open
  and every dirty update against the full reference render.

Measured on the PC (replay `us-field-options`, turbo 4):
`options update rebuild ... compose=35246 us`, first frame shown 36.5 ms after the
open request before; `options update cached ... compose=1602 us`, first frame shown
2.8 ms after. The worker needs about 30 ms per layout on the PC (threaded test: 28 to 31 ms,
while the frame thread keeps updating), none of it on the frame thread. The Switch ran the old rebuild 4.7 times slower than the PC
(166 ms against 35 ms), which puts the open at about 8 ms of compose plus 2.5 ms of
upload and the worker at roughly 150 ms per layout on core 2, off the frame thread: expected, not yet measured on the OLED
([OLED_TEST_PLAN B15](OLED_TEST_PLAN.md)).

### The in-game open without the title atlas (2026-10-28, preview.45 feedback)

Cause. The Switch overlay (`nx_title_overlay.c`) kept the Port options screen in
the last 960x720 band of one 960x5040 atlas (seven states, 19 MB) that also
holds the title's states. The textures are released 90 draws after the title
leaves the screen (#268), so an Options open from the field found no texture
and `ensure_gl` created the whole atlas and the logo texture before drawing:
`dw3-title-overlay: 960x5040 RGBA texture, 7 states (GLES)` then
`first frame shown 92.6 ms`, `longest frame gap 95.6 ms (frame 1)` (EU,
preview.45; the title's open was 20 ms because the atlas was already there).
The #298 prewarm covers the compose, not the GL objects.

Fix. The options screen has its own 960x720 texture (2.6 MB, `s_opt_tex`); the
title atlas is 960x4320 (six states, 16.5 MB) and is created only when the
title, the logo or an Options opened from the title is on screen
(`need_atlas`). Opening Options in game creates only the small texture (no
zero-fill upload either: the first cache update sends the whole canvas, which
the cached layer makes a copy). Both are released with the other title textures
after the idle frames. Logged once per creation:
`dw3-title-overlay: 960x720 RGBA options texture (GLES), create N us` and
`960x4320 RGBA title atlas, 6 states (GLES), create N us`. PC is unchanged (its
SDL3 GPU overlay keeps the textures).
Goal: first frame under 20 ms in game; **not measured on the OLED**
([OLED_TEST_PLAN B15b](OLED_TEST_PLAN.md)).

### OLED checklist, Options in game (owner)

US and EU (at least two EU languages), a Switch OLED, docked and handheld:

- [ ] Plus in the field: the menu ends with OPTIONS and TITLE SCREEN (and, after a
      card folder item, the eight-row panel closes cleanly under the last row).
- [ ] Cross on OPTIONS: the Options window over the dimmed field, Widescreen in
      grey with the "Locked in game" line; Cross on it does nothing. The sounds are
      the Options sounds.
- [ ] Music volume Left x3: the field music gets quieter at once and keeps playing.
- [ ] Encounter rate OFF: walk in a random-battle area, no battle. Back to 1x: battles
      again. Text speed Instant: the next dialogue types at once.
- [ ] Restart hotkey ON then the chord (L + R + Plus + Minus) restarts at once.
- [ ] B closes the window; the menu is as before, the cursor still on OPTIONS; B
      again returns to the field and the character walks, nothing was pressed twice.
- [ ] Quit to hbmenu and start again: the values changed in the field are still set
      (`port_options.cfg`, `dw3.log` shows `saved` lines).
- [ ] With the Cheats master ON at the title: in the field the Cheats hint is grey and
      Y does nothing.
- [ ] 16:9: the dim covers the gutters; no gap, no leftover of the window after B.

## Options in battle (P9.7, slice 2)

Slice 2 of ROADMAP P9.7: the same Options screen from a **battle**. Owner's brief
(2026-10-25, the owner away): work autonomously, the conservative option for each
open question; every default below is **default chosen, owner can change**.

### What the player sees

In a battle, while the **command menu** (Fight, Tech, DV, Tag, Item, Run) is
waiting for you, press **Select** (PC Backspace, Switch **Minus**) with no
shoulder button held. The Options window opens over the battle, which stays on
screen exactly as it was and does not move: no enemy attack, no gauge, no
animation, no message. The music keeps playing (and follows the volume you set).
Up / Down choose a row, Left / Right and Cross change it, **Triangle** (PC S,
Switch X) goes back; the command menu is then as it was, with the same cursor,
and the battle goes on from the same frame.

### Defaults chosen (owner can change)

1. **Entry: a button, not a menu row.** The battle has no menu the port controls
   (its command menu is the game's own, six rows with the game's own window art
   and no spare row), so option (a) of the P9.7 note applies. **Select / Minus**,
   because the battle code never reads Select (only the debug battle test does),
   because it is the key that already opens Options directly at the title, and
   because every chord the port owns holds a shoulder button or a stick click (R3
   fast-forward; the restart chord L + R + Plus + Minus; the Switch debug chords
   Minus + L + R and Minus + ZL + ZR). The entry therefore ignores a Select press
   while L, R, ZL or ZR is held. Plus (Start) would have mirrored the field menu,
   but it is part of the restart chord and a stray Plus is easy; Select is the more
   conservative key. No on-screen hint (a hint would change every battle reference
   picture and the game's own window layout); the key is in
   [HOTKEYS](HOTKEYS.md) and the [player guide](PLAYER_GUIDE.md).
2. **Only at a safe point: the command menu waiting for input.** The poll is in
   `FIGHTSTG_updateCommandMenu` (`TASK_RUN`), which runs only while the top
   command menu waits for a command. Select does nothing during an attack, the
   enemy's turn, messages, the technique / item / switch / digivolve lists, the
   confused menu or the report. (A later slice could add the sub-menus.) The
   command menu is the game's natural pause for the player, so nothing has to
   be done to the battle's own state to open there.
3. **Pause: the main loop does not run the battle.** While the screen is open,
   `main()` (patch `0165`) skips the whole task list, the widescreen frame and
   `drawFrame`, so **no** task runs: the battle clock (`FIGHTSTG_battle.frames`,
   from which the event queue, the model and sprite animations and the camera
   moves take their time), the queued events (the enemy's turn timer), the
   fighters' HP and gauges, the message and cursor blink, the battle sound task
   and the random countdown all stand still, and no task reads the pad. It does
   not draw either: nothing is submitted, so the display keeps the last picture
   (PC: VRAM; Switch: the same plus the mesh layer's picture, see 6).
   `RANDOM.next()` is skipped too, so the roll sequence after the pause is the one
   the battle would have had. The iteration still presents (`VSync(0)`), reads the
   pad, polls the restart chord, the file cache and the sound loader. **The
   music** is not a task: the sequencer ticks in the vsync callback, so it keeps
   playing and a volume change is heard at once. A sound effect already started
   finishes.
4. **The clocks come back.** The vsync callback keeps adding to the game's clocks
   (`GFX.timeCounter`, `frameTimeCounter`, `GAME.playTime.frames`) during the
   pause, and the battle compares absolute `GFX` times (message and arrow timers)
   and takes the next frame's length from the frame-time counter. They are put
   back to what they were at the first held frame, so the first frame after the
   pause is a normal one and the play time does not count the pause.
5. **No input leaks.** The Select that opened it is an edge already gone when the
   screen reads the pad; the Triangle that closes it is read by the screen in a
   held iteration, and the next iteration (where the battle runs again) sees
   that button only as held, not pressed. No battle task reads the pad while it
   is open.
6. **Drawing.** The same native overlay as slice 1 (PC SDL3 GPU and Switch GLES)
   draws the window over the held picture. The battle is 4:3 on both hosts (the
   16:9 gutters belong to the field); the window sits inside the 4:3 picture and
   the black bars stay black (captured in a 16:9 PC window). On the Switch the
   battle is on the GLES mesh layer, and a frame without draws would drop its
   per-frame flag and show only the CPU layer: `nx_mesh.c` keeps the last
   frame's flag, the mesh FBO and the partner view's keep area while the battle
   is held (`dw3_port_battle_options_holding()`), so the 3D picture stays. While
   held, fast-forward counts as 1x (a held Down would otherwise repeat 4x as
   fast); the battle's speed comes back on resume.
7. **Live apply**, row by row: the same as the field's table above. Widescreen
   and the Cheats entry are **greyed, locked** (the camera, the gutter and the
   battle layout are set when the mode loads). Fast-forward, Restart hotkey,
   Music volume, SFX volume apply at once (the SFX volume is audible: the screen
   plays the menu sounds). **Encounter rate**, **Text speed**, **Auto-save**
   apply at once and are read where the game next needs them (the next footstep,
   the next message page, the next area change); the battle in progress is not
   touched. **Battle cursor** also applies at once: it is read when the command
   menu *opens*, so it takes effect from the next turn's menu (the menu that is
   open keeps its cursor); it is not greyed.
8. **A restart chord while it is open** (Restart hotkey ON) forgets the hold and
   closes the screen (`dw3_port_battle_options_reset`), then restarts as always.
9. **Not offered without the native overlay** (`dw3_port_battle_options_enabled()`
   is 0 where `dw3_port_field_options_enabled()` is), like the field row.

### Design

- `port/options/battle_options.c`, `port/include/port_battle_options.h`:
  `dw3_port_battle_options_poll(language, probe)` (the entry, called by the
  command menu), `_hold_frame()` (the main loop, once per iteration; also gives
  the clocks back), `_holding()` (the presenters; true from the first iteration
  that skips the draw), `_reset()`. `probe` is a small function in
  `command_menu.c` that sums the queued events, their remaining times and the
  fighters' HP for the log. `dw3_port_options_set_origin` only changes the words
  of the open / close log lines.
- Patch `0165` ([PATCHES](PATCHES.md)): the poll in `FIGHTSTG_updateCommandMenu`
  and the hold branch in `main()`. It adds no decomp state.
- `platforms/switch/psyz_backend/nx_mesh.c`: `end_frame` and `has_content` keep
  the mesh layer's last picture while held. `port/options/turbo.c`: 1x while held.

### Tests

- `tools/tests/test_battle_options.py` (`battle_options_main.c`): the entry (not
  offered without the overlay, not without Select, not with a shoulder button
  held), the frame that opens it is still drawn, held iterations run no task and
  advance no frame, the clocks are put back at resume, the closing iteration is
  still held, a second open, a reset while held; the patch wires the poll, the
  hold branch and the reset.
- Replay (PC, US and EU; [P6.15_REPLAY_TESTS.md](P6.15_REPLAY_TESTS.md)):
  `us/eu-battle-options-ref` is the plain forced battle (Tech once, Cross to the
  report and the field) with five checkpoints; `us/eu-battle-options` is the same
  battle with Select at frame 330 (the command menu), Widescreen refusing a
  change, Music volume 10 to 7, the screen left open for **300 frames**
  (`options-held`), Triangle, and the rest of the battle shifted by 300 frames.
  The runner compares its five checkpoints taken after the pause **pixel for
  pixel with the plain run's** (`ref_map`): 0 differing pixels on both discs, in
  both the PS1 frame and the window grab, so the battle resumed on the same frame
  with the same animation, camera and rolls. The log must show
  `dw3-battle-options: held, battle state: ... events=N event_time=T hp=H` and,
  300 frames later, `resume, battle state:` with the same `gfx_frame`, `events`,
  `event_time`, `hp` and `time_counter`, and the clock line
  `the vsync clock ran on ... during the hold and is put back`. The battle ends
  with `WFIGHTMN_endBattle result=1` and the four overlay restores of a battle.
  In the plain battle nothing logs `dw3-battle-options`.
- Switch build in Ryubing (tier `nx`, log only): `us/eu-battle-options-nx` (see
  the replay doc). A 16:9 PC window (`DW3_PC_INTERNAL_RES=3`) was captured once
  as evidence.
- No existing reference changes: the entry has no on-screen hint and the pad
  recordings never press Select in a battle. The new scenarios have their own
  references; the plain run is the reference of the excursion run.

### OLED checklist, Options in battle (owner)

US and EU (at least two EU languages), a Switch OLED, docked and handheld:

- [ ] At the command menu press Minus: the Options window over the battle, which
      stays on screen (models, HUD, partner portrait): no black frame, no flicker,
      the 3D part not missing. Widescreen grey. The music keeps playing.
- [ ] Wait 30 seconds with it open: the enemy does not attack, no gauge moves,
      nothing animates behind the window.
- [ ] Music volume Left x3: quieter at once. Triangle (X): the command menu as it
      was (same cursor), the battle goes on and nothing was pressed twice.
- [ ] Open it on a later turn too, and after changing Battle cursor: the menu opens
      on the remembered command from the next turn.
- [ ] Minus during an attack animation, on the enemy's turn and in the Tech list: it
      does nothing. L + Minus or R + Minus does not open it.
- [ ] With Restart hotkey ON, the chord while it is open restarts.
- [ ] Dock or undock with it open (optional): the picture may go dark until you
      close it, then the battle is back.
- [ ] `dw3.log` has the `dw3-battle-options:` lines (open, held, resume) with the
      same `events=`, `event_time=` and `hp=` on `held` and `resume`.

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

## History moved from ROADMAP.md (2026-10-24)

Moved from the ROADMAP.md entries on 2026-10-24, word for word (two leading
spaces removed). "Above" and "below" refer to the old entry. The roadmap
keeps the current state and links here.

### P9.4 history

**Status (2026-10-24): done.** Owner confirmed on the OLED, 2026-10-24.
The 2026-10-22 status below is kept as it was moved.

**Status (2026-10-22): implemented, PC and Ryubing validated on both
discs; waiting for the OLED check.** `port_options.cfg` (key=value,
`version=1`) in each region runtime root (`runtime/<ver>/`,
`sdmc:/switch/dw3/<ver>/`), loaded in `init_paths` before any flag is
read, saved on every change on the Options screen, FAT-safe atomic write.
Format, rules and rationale: [PORT_OPTIONS.md](PORT_OPTIONS.md#persistence).
Evidence: unit tests `tools/tests/test_options_persist.py`; two-phase
replay scenarios `us/eu-options-persist` on the PC build (identical
captures on two runs) and in Ryubing (Widescreen OFF, Fast-forward 3x,
Restart hotkey ON written by one process, read back by the next; the file
on the emulated SD has the same content).
Ready to close? needs OLED: toggle, HOME->close, relaunch from hbmenu.
Blocks: P6.12, P7.6–P7.10 and a persisted P6.9 restart
setting (see [Backlog](ROADMAP.md#backlog--future-goals)); P6.17 (volume) and P6.18
(pacing) are rows added on top of it (`music_volume`, `sfx_volume`,
`encounter_rate`, `text_speed`).
**Partial OLED evidence (2026-10-22, EU, preview.30-rc4, owner):** with no
`port_options.cfg` on the SD card the first change created it and every
later change saved it (56 `dw3-options: saved` lines, no failure logged).
Not covered: relaunch from hbmenu and reading the values back, and US.
Evidence: `tmp/evidence/2026-10-22-preview30-rc4-oled/dw3.log`. Stays open.

### P6.20 history

**What exists (cited, to be reused, not rebuilt):**
- *The Options screen* ([PORT_OPTIONS.md](PORT_OPTIONS.md)): registry in
  `port/options/port_options.c` (`DW3_PORT_OPT_COUNT` = 8, the Battle cursor
  row of P7.9 is the latest), keys in `port_options.cfg`, saved on every
  change, atomic write, unknown keys kept (so an older build does not erase a
  newer build's cheat keys). The eight rows fill the window: 38 px rows on a
  42 px pitch, the last ends at y 482, 26 px above the message rule at y 508
  ([PORT_OPTIONS.md](PORT_OPTIONS.md#options-screen-2026-10-14), "Eight
  rows"), so a ninth row does not fit without changing the layout. Labels are
  pre-rendered lettering (`tools/generate_pc_title_lettering.py`).
- *Dev hooks, today environment variables read by `PORT_HOST` patches*,
  PC only and silent when unset: `DW3_START_MODE` / `DW3_PROGRESS` (boot into
  a field or menu mode, patches `0020`, `0023`), `DW3_FORCE_ENCOUNTER` /
  `DW3_FORCE_PARTY` (a forced battle, `0024`, `0040`), `DW3_FORCE_WIN` (jump
  to the report, `0025`), `DW3_FORCE_ITEMS` (fills the bag with an item id
  range, `0070`), `DW3_FORCE_MENU` / `DW3_FORCE_STAGE` (`0026`, `0030`), and
  the P7.7 / P7.9 set (`DW3_FORCE_ENEMY_HP`, `FORM`, `TECH`, `HIT`, `STEAL`,
  `EXTRA_TECHS`; `0064`, `0067`). The Switch NRO has no environment; only a
  build-time `DW3_NX_TEST_ENV` bakes some in
  (`platforms/switch/source/boot_main.c`). A cheat menu is therefore the first
  way to set them at runtime on an ordinary NRO: the hooks would read the
  port's flag store instead of `getenv`.
- *Options and aids that already do a cheat's job*, no new work needed:
  **Encounter rate OFF** is "no random encounters" (P6.18); **Text speed
  Instant** and **Fast-forward** (P6.4) are the time savers;
  `DW3_FORCE_CRASH` exercises the crash path
  ([P5.4_CRASH.md](P5.4_CRASH.md)); the nxlink HUD
  ([DEBUG_OVERLAY.md](DEBUG_OVERLAY.md)) shows the mode and the fps; the P6.15
  replay scenarios ([P6.15_REPLAY_TESTS.md](P6.15_REPLAY_TESTS.md)) drive the
  hooks on PC.
**Candidate cheats** (cheap = a port flag or an existing hook; decomp =
needs the field layout of `GameState` / party / battle structs from
`vendor/dw3_decomp`, to be located first): give items (cheap, hook `0070`
becomes a runtime action); instant battle win (cheap, hook `0025`); start a
chosen battle (cheap, `0024` / `0040`); max money / bits (decomp, one field
once found); infinite HP / MP and max stats (decomp, party and battle
structs, re-applied every frame); unlock digivolutions (decomp, the
requirement flags per form); walk speed (decomp, a field movement constant;
it changes the walk length of the replays, so only when on); warp to map
(decomp and the hardest: the hooks above act only at boot, an in-game warp
must change mode safely, see P3.8 `DW3_FORCE_STAGE`; it would also help P5.5).
**First slice:** (1) a Cheats sub-screen entered from the Options screen
(layout decision first: a scrolling list, a second page or a ninth row), with
the master switch off; (2) a runtime flag store in `port/options/` with its
`cheat_*` keys; (3) three cheap entries wired to the existing hooks (give
items, instant win, start a battle); (4) a log line per use. The decomp
cheats follow one at a time after the field is located.
**Constraints:** off by default, and a build or a config with every cheat
off is bit-identical to today (the P6.15 replays must still match, plus one
log-only replay per cheat). Decomp changes only as numbered `PORT_HOST`
patches with a row in [PATCHES.md](PATCHES.md) (the next free number); the
`getenv` hooks keep working for the replays. Persistence: **the options file,
not the save.** The flat PS1 save image has no spare room (found in P7.9) and
a cheat toggle must not edit P5.3 saves; `cheat_*` keys in
`port_options.cfg` fit the current design, per region like every key. Toggles
(infinite HP, walk speed) persist; one-shot actions (give items, max money)
do not and run from the menu. A one-shot still changes the live game and the
game's own save would then store it: say so on the screen and log it. No
cheat may write a save file by itself.
**PR 1 as built (2026-10-24):**
- *Entry:* on the Options screen, **Square** (PC **Z**, Switch **Y**) opens the
  Cheats screen and Triangle goes back. The hint "Square/Y Cheats" is drawn
  **only while the master switch is on**: any always-visible entry (a ninth
  row, paging, a hint) would change the Options screen's pixels and the
  existing `*-options-persist`, `*-volume-persist` and
  `*-battle-cursor-persist` references would fail; they are not refreshed.
  With the master off the Options screen is pixel-identical and the eight
  rows and the dirty-rectangle cache are untouched.
- *Cheats screen:* five rows in the Options window: Cheats (master, persisted
  as `cheat_enabled`), Fill bag (one-shot), Instant win (persisted as
  `cheat_instant_win`), Battle now (one-shot) and Cheat saves (read-only
  count). Rows 2-4 are greyed and inert while the master is off; turning the
  master off disarms the one-shots.
- *Cheats:* **Fill bag** puts 99 of every usable item, weapon, armour and
  accessory (ids 43..359) in the bag through patch `0070`'s hook; **Instant
  win** ends each battle as won on its first turn; **Battle now** makes the
  next footstep in a random-battle area start a battle, even at Encounter
  rate OFF. The `DW3_FORCE_*` variables keep working and win over the screen.
- *Save flag:* a sidecar `save<N>.cheats` next to the memory card file, never
  a byte in the save
  ([PORT_OPTIONS.md](PORT_OPTIONS.md#the-cheat-used-flag-on-saves)). Shown as
  the count on the Cheats screen and in the boot log; the in-game memory card
  screen does not show it yet (follow-up).
- *Code:* `port/options/cheats.c`, `cheat_saves.c`, `port/include/port_cheats.h`,
  patch `0077-PORT_HOST-p620-cheats-hooks.patch` (every line in
  `#ifdef DW3_CHEATS`), the Square/Z/Y hook in `port_options.c`, the lettering
  (`CHEAT_TEXT` in `tools/generate_pc_title_lettering.py`).
- *Tests:* `tools/tests/test_cheats.py`, the cache test now covers the Cheats
  page and the hint, seven log-only replay pairs (`<v>-cheats-screen`,
  `cheat-fill-bag[-off]`, `cheat-battle-now[-off]`, `cheat-instant-win[-off]`),
  the full replay suite on the cheats-ON build against unchanged references,
  and a section and symbol comparison of the cheats-OFF build with `main`.
**PR 2 as built (2026-10-25, patches `0118` to `0120`):**
- *Layout:* the Cheats screen got a second page instead of a ninth row on the
  Options screen (the eight Options rows are untouched; page 1 keeps its five
  rows). Down from the last row of page 1 continues on page 2, the title reads
  "1/2" and "2/2". Reason: five more rows do not fit the window with page 1's.
- *Cheats:* Money (+10,000, +100,000, MAX), Warp (13 areas of the debug stage
  select), Item id, Item count, Give item. All applied by the field at its first
  idle frame, used up and logged; Give item needs no decomp patch beyond the
  same hook place because it adds to `GAME.items` itself (patch `0119`).
- *Not done:* infinite HP / MP, max stats, digivolution unlocks, walk speed
  (their decomp fields are not located); item names on the picker (needs the
  game's text archive in the overlay); P9.7 (Options in game).
- *Defaults chosen without the owner (the owner can change them):* see
  [Cheats, page 2](#cheats-p620) and ROADMAP P6.20.
