# Player guide

How to set up and play DW3NX, the Switch port of Digimon World 3 (Digimon
World 2003 in Europe). This is the first slice of ROADMAP P6.23. Every section
names the doc it comes from, so you can read the details there.

The port is an early preview series. Expect bugs. Many features are checked on
PC and in an emulator but not yet on a Switch OLED; the release notes say which.

Contents: [Setup](#setup), [Controls and hotkeys](#controls-and-hotkeys),
[Title menu](#title-menu), [Options screen](#options-screen),
[Cheats screen](#cheats-screen), [Options in the field](#options-in-the-field),
[Return to title](#return-to-title),
[Saves](#saves), [Crashes and reports](#crashes-and-reports),
[Known issues](#known-issues).

## Setup

<!-- Sources: README.md (Play it on Switch), DISC_SETUP.md, RUNTIME_LAYOUT.md,
platforms/switch/release/README.txt (Own-disc setup and extraction),
SINGLE_BUILD.md#as-built-slice-1 (PC launcher),
SINGLE_BUILD.md#as-built-slice-2-the-switch-launcher (Switch launcher) -->

You need:

- A Switch running Atmosphere with hbmenu.
- Your own disc, dumped as a Mode2 BIN and CUE pair.
- The `.nro` that matches your disc.

No game files ship with the port. A wrong-region disc is rejected.

| Region | Disc | Build | Runtime folder |
|---|---|---|---|
| US | Digimon World 3 (`SLUS-01436`) | `dw3-switch.nro` | `sdmc:/switch/dw3/us/` |
| EU | Digimon World 2003 (`SLES-03936`) | `dw3-switch-eu.nro` | `sdmc:/switch/dw3/eu/` |

Disc dump: the docs describe a Mode2/2352 BIN with its CUE. They do not
describe a plain ISO as supported, so use a BIN and CUE. If the CUE names a
different BIN than the one you have, the port picks the only other `.bin` in
that folder, if it is large enough ([RUNTIME_LAYOUT](RUNTIME_LAYOUT.md)).

**Region launcher (P6.24).** The game is still two builds, one per region. A
launcher can start the right one:

- PC: the `dw3-pc` launcher starts the US or EU binary for the runtime folder it finds.
- Switch: a launcher `.nro` (`dw3.nro`, in `sdmc:/switch/dw3/`) is built by CI but
  **not shipped in releases yet**. Use the region `.nro` above until it is.

Steps:

1. Download the `.nro` for your region from the
   [latest release](https://github.com/Larbaco/dw3-switch/releases/latest).
   The EU build is attached to releases by hand.
2. Copy it to `sdmc:/switch/`.
3. Copy your disc's BIN and CUE files:
   - US: into `sdmc:/switch/dw3/`, next to the `us/` folder.
   - EU: into `sdmc:/switch/dw3/eu/`, for example `disc.bin` and `disc.cue`.
4. Launch from hbmenu. The first boot extracts the game data from the disc and
   draws a progress bar. Leave it running.

The US card then looks like this (EU uses `eu/`):

```text
sdmc:/switch/dw3-switch.nro
sdmc:/switch/dw3/Digimon World 3.cue
sdmc:/switch/dw3/Digimon World 3.bin
sdmc:/switch/dw3/us/            created on first boot
```

After the first boot:

- The next boot checks the extracted files and does not read the BIN again.
- Do not pre-extract files by hand when you want the port to extract them.
- Keep `us/`, `eu/` and their `memcard/` folders. Never delete or move
  `save0.bin`, `save1.bin` or `.mcr.bak` to force an extraction. Back up
  `memcard/` before deleting a region folder.
- US and EU keep separate folders and settings.
- The port writes `port_options.cfg` (settings) in the region folder and
  `memcard/` (saves). Logs and crash reports go one level up, in
  `sdmc:/switch/dw3/`.

The `-nxlink` build streams logs to a PC and is for development. You do not
need it to play. The port does not overclock.

## Controls and hotkeys

<!-- Sources: HOTKEYS.md, P4.4_INPUT_STATUS.md, P6.4_FAST_FORWARD.md,
P6.9_RESTART.md -->

Switch face buttons follow the PS1 positions, not the Nintendo layout. B is
Cross and confirms. A is Circle.

| PS1 | Switch |
|---|---|
| D-pad | D-pad or left stick |
| Cross | B |
| Circle | A |
| Triangle | X |
| Square | Y |
| Start | Plus |
| Select | Minus |
| L1 / R1 | L / R |
| L2 / R2 | ZL / ZR |
| L3 / R3 | Left / right stick click |

Port hotkeys (full table and PC keys: [HOTKEYS](HOTKEYS.md)):

| Action | Switch | Notes |
|---|---|---|
| Fast-forward on or off | R3 (right stick click) | Field, battle and the battle report only. The speed is set in Options. Release R3 before pressing again. |
| Restart the game | L + R + Plus + Minus | Off by default; turn on **Restart hotkey** in Options first. Saves are flushed first; unsaved progress is lost. Release before repeating. |
| Open Options from the title | Minus | Only while the title choices show and no shoulder button is held. |
| Skip an opening movie | Plus | Press or hold. |

Diagnostic chords (not for normal play):

- Minus + ZL + ZR cycles the battle renderer: `soft`, `soft+bbox`, `gpu-solid`,
  `gpu-tex`. The game starts in `gpu-tex`. Field rendering is not affected.
- Minus + L + R toggles the debug HUD in `-nxlink` builds.
- The restart chord contains the HUD chord, so in a build with the HUD it can
  also toggle the HUD.

## Title menu

<!-- Sources: HOTKEYS.md, PORT_OPTIONS.md (Quit and return to title),
platforms/switch/release/README.txt -->

Press Plus at the title to show the choices. Up and Down move, B confirms.

- **New Game**
- **Options** opens the [Options screen](#options-screen).
- **Load Game** (Continue) opens the memory card screen.
- **Quit game** is the last row, a little apart from the others. It asks Yes
  or No with the cursor on No. Yes flushes the save slots and returns to
  hbmenu. If the flush fails, the quit is cancelled. Progress since your last
  in-game save is lost. HOME still exits as before.

On the EU disc the plaques use the current language of the game. The Yes / No
window is in the five EU languages (the US disc is English).

## Options screen

<!-- Sources: PORT_OPTIONS.md (Options screen, Persistence), HOTKEYS.md,
P6.1_WIDESCREEN.md, P6.4_FAST_FORWARD.md, P6.9_RESTART.md, P6.17_VOLUME.md,
P6.18_PACING.md, P6.31_AUTOSAVE.md, P7.9_BATTLE_MENU_MEMORY.md; row names and
defaults from port/options/port_options.c -->

Options holds the features the port adds. Up and Down choose a row. B or A
changes it. X goes back (to the title, or to the field menu when opened there,
see [Options in the field](#options-in-the-field)). A line under the rows explains the
focused row. Changes are saved at once in `port_options.cfg`
([PORT_OPTIONS](PORT_OPTIONS.md#persistence)). Defaults keep the original
game, except Widescreen.

| Row | Values | Default | What it does |
|---|---|---|---|
| Widescreen | ON / OFF | ON | About 16:9 in the field and battle, and in several menus. The title and movies stay 4:3. [P6.1](P6.1_WIDESCREEN.md) |
| Fast-forward | 2x / 3x / 4x / OFF | 2x | The speed R3 switches to. The game still starts at 1x. OFF disables R3. Music and sound effects speed up too. [P6.4](P6.4_FAST_FORWARD.md) |
| Restart hotkey | ON / OFF | OFF | Enables the L + R + Plus + Minus restart chord. [P6.9](P6.9_RESTART.md) |
| Music volume | OFF, 1 to 10 | 10 | 10 is the original mix. Left and Right step; B steps up and wraps 10 to 0. [P6.17](P6.17_VOLUME.md) |
| SFX volume | OFF, 1 to 10 | 10 | Same keys. The screen plays the game's menu sounds, so you hear the change. |
| Encounter rate | OFF / 0.5x / 1x / 1.5x / 2x | 1x | Random battles only. Story battles stay. [P6.18](P6.18_PACING.md) |
| Text speed | Normal / Fast / Instant | Normal | How fast dialogue is typed out. Normal is the original. |
| Battle cursor | OFF / Command / Command+Tech | OFF | The battle command menu (and with Command+Tech the technique list) opens on your last pick. [P7.9](P7.9_BATTLE_MENU_MEMORY.md) |
| Auto-save | OFF / Each area | OFF | With Each area: at each new field area the game saves to the slot you last saved to or loaded from, and overwrites it. Nothing is saved until you have saved or loaded a slot in this run. See [Saves](#saves). [P6.31](P6.31_AUTOSAVE.md) |

The row lines on the screen are English on every disc, except the Auto-save
row, which is in the five EU languages on the EU disc.

## Cheats screen

<!-- Source: PORT_OPTIONS.md#cheats-p620 -->

The Cheats screen is a test aid, not a game mode. On the Options screen press
Y. X goes back. The "Y Cheats" hint shows only once the master switch is on,
so you will not see it before you know the key. Builds made with
`--no-cheats` do not have the screen.

The master row **Cheats** is OFF by default. While it is off, no cheat does
anything.

Page 1:

- **Fill bag**: at the next field menu, 99 of each usable item and equipment.
  Key items and card packs are left out.
- **Instant win**: each battle is won on its first turn.
- **Battle now**: the next step in a random-battle area starts a battle.
- **Cheat saves**: how many saves are marked "cheat used".

Page 2 (Down from the last row of page 1): **Money**, **Warp** (13 areas),
**Item id**, **Item count** and **Give item**. They apply in the field. A
Warp can leave a story-flagged map looking unfinished, like the debug stage
select.

Fill bag, Battle now and the page 2 actions are armed for this run only. A
cheat changes the game in progress, and the game's own save keeps that change.

## Options in the field

<!-- Sources: PORT_OPTIONS.md#options-in-game-p97, HOTKEYS.md -->

In the field, open the menu with Plus. The row above the last is **OPTIONS**
(on EU: OPTIONS, OPZIONI, OPTIONEN, OPCIONES). B opens the same Options screen as
the title, over the field, and X closes it back to the menu. The game waits
behind it, as it does in the menu, and the music keeps playing.

Every row changes the game at once: volumes, Fast-forward, the Restart hotkey,
Encounter rate, Text speed, Battle cursor and Auto-save, and the change is saved
in `port_options.cfg` as at the title. Two things are **greyed** and cannot
change in the field: **Widescreen** (the map is laid out when an area loads, so
change it at the title) and the **Y Cheats** hint (cheats are armed at the
title). The line under the rows says "Locked in game". There is no entry in
battle yet.

## Return to title

<!-- Sources: PORT_OPTIONS.md#quit-and-return-to-title-p627-p628, HOTKEYS.md -->

In the field, open the menu with Plus. The last row, under OPTIONS, is **TITLE SCREEN** (on EU:
RETOUR TITRE, TORNA AL TITOLO, TITELBILD, VOLVER AL INICIO). It asks Yes or No
with the cursor on No. Yes runs the same reset as the restart chord. The
save slots are flushed first and a failed flush cancels the reset. Unsaved
progress is lost. On the EU disc you land on the language select. The row is
always there; it does not depend on the Restart hotkey setting. It is in the
field menu only, not in battle.

## Saves

<!-- Sources: README.md, RUNTIME_LAYOUT.md, P5.3_FLAT_SAVE.md,
CROSS_REGION_SAVES.md#as-built-slice-1, PORT_OPTIONS.md#cheats-p620,
P6.31_AUTOSAVE.md (Auto-save, the ninth row) -->

- **Where:** `sdmc:/switch/dw3/us/memcard/` or `sdmc:/switch/dw3/eu/memcard/`.
  Slots are `save0.bin` and `save1.bin`. Old `memcardN.mcr` files convert once
  and are kept as `.mcr.bak`. Details: [P5.3_FLAT_SAVE](P5.3_FLAT_SAVE.md).
- **In-game saving is manual,** as in the original game. The port flushes
  changed slots at safe points and on HOME or sleep. It never saves for you,
  unless Auto-save is on (see below).
- **Auto-save (Options, OFF by default).** With **Each area**, the first idle
  frame of each new field area writes the game to the slot you last saved to or
  loaded from in this run. The write **overwrites that slot**. Loading a save
  is not an area change: the first idle frame after a load only sets the
  starting point, and the save happens at the next area change.
  - Before the first auto-save of a run, your version of the whole card is
    copied to `save<N>.bin.pre-auto` in the same folder (N is the card, 0 or 1).
    It is made again only after you save or load a slot again, never between
    two auto-saves. To undo an overwrite, close the game and put that file back
    as `save<N>.bin`.
  - It does **not** run when: no slot was saved to or loaded from in this run
    (a new game, or nothing used yet); you are in an event, a menu, a talk,
    a climb, an area banner or fade, or a battle is starting; the story is
    not far enough for the field menu to open (progress 4 or more); the map
    is not one the save list knows; or the slot is not the same character
    name, has a longer play time than the game now, is empty, or is on a card
    that is not a DW3 card. A refused auto-save changes nothing. A battle that
    returns you to the same area does not count as an area change.
  - A new game or Return to title clears the slot the auto-save would use.
  - If a cheat changed the game, the auto-save is still made and marked, the
    same as a manual save (see Cheat marker below).
  - A failed write is logged and the game carries on. There is no on-screen
    notice yet. Log lines starting `dw3-autosave` are in `dw3.log`.
  - Not yet checked on a Switch OLED. Details: [P6.31_AUTOSAVE](P6.31_AUTOSAVE.md).
- **US and EU saves are separate,** with one exception: the import below.
- **Cross-region import (slice 1).** If a region has no save of its own, the
  build copies the other region's save once, converted, into its own
  `saveN.bin`, and writes a `saveN.import` marker. Rules:
  - The original save is never changed.
  - It only runs when the region has no `saveN.bin` at all. If you already
    have saves in both regions, nothing is imported.
  - It runs once. A deleted save does not come back. Delete `saveN.import`
    (and the card) to import again.
  - Saves you make afterwards stay in their own region. They do not flow back.
  - An EU save from the extra chapter is not imported into US.
  - Corrupt or unusable sources are refused and nothing is written.
  - Details and the hardware checklist:
    [CROSS_REGION_SAVES](CROSS_REGION_SAVES.md#as-built-slice-1). Not yet
    checked on a Switch.
- **Cheat marker.** A save written after a cheat changed the game gets an
  entry in a `saveN.cheats` file beside `saveN.bin`. The save itself is
  unchanged. The Cheats screen counts marked saves. If you copy only
  `saveN.bin`, the mark stays behind
  ([PORT_OPTIONS](PORT_OPTIONS.md#cheats-p620)).

## Crashes and reports

<!-- Sources: P5.4_CRASH.md, REPORTING_ISSUES.md -->

If the port crashes, it writes `sdmc:/switch/dw3/crash_report.txt`. On the next
launch it shows a panel, "The last run crashed.", with the version, the
reason, and the path of the saved report, and moves the report to
`crash_report_prev.txt`. The panel waits up to 60 seconds, or until you press
A, B or Plus. It shows once per crash and never blocks the boot. The text is
English on both regions.

To report a crash:

1. Before relaunching more than once, copy `crash_report.txt` or
   `crash_report_prev.txt` and `sdmc:/switch/dw3/dw3.log` off the SD card. Only
   one previous report is kept.
2. Open an issue on the
   [tracker](https://github.com/Larbaco/dw3-switch/issues). Say the version,
   region, console and what you did, and attach both files.
3. Never attach disc images, BIOS files or extracted game data.

What to include and an example: [REPORTING_ISSUES](REPORTING_ISSUES.md). To
read a report yourself, see "How to symbolize a report" in
[P5.4_CRASH](P5.4_CRASH.md).

## Known issues

Known issues change with every preview. They are not copied here.

- The list for the current preview is in the release notes `README.txt`
  (the release's bundle, and
  [platforms/switch/release/README.txt](../platforms/switch/release/README.txt)).
- Release history and builds to avoid: [RELEASES](RELEASES.md).
- Open work and planned features: [ROADMAP](ROADMAP.md).
- Testing on a Switch and want a checklist: [OLED_TEST_PLAN](OLED_TEST_PLAN.md).
