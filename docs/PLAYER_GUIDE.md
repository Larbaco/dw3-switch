# Player guide

How to set up and play DW3NX, the Switch port of Digimon World 3 (Digimon
World 2003 in Europe). This is the first slice of ROADMAP P6.23. Every section
names the doc it comes from, so you can read the details there.

The port is an early preview series. Expect bugs. Many features are checked on
PC and in an emulator but not yet on a Switch OLED; the release notes say which.

Contents: [Setup](#setup), [Controls and hotkeys](#controls-and-hotkeys),
[Title menu](#title-menu), [Options screen](#options-screen),
[Help](#help), [Walkthrough](#walkthrough), [Cheats screen](#cheats-screen), [Options in the field](#options-in-the-field),
[Options in battle](#options-in-battle),
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
- `dw3-switch.nro`, the one file for US and EU.

No game files ship with the port. A wrong-region disc is rejected.

| Region | Disc | Build | Runtime folder |
|---|---|---|---|
| US | Digimon World 3 (`SLUS-01436`) | `dw3-switch.nro` | `sdmc:/switch/dw3/us/` |
| EU | Digimon World 2003 (`SLES-03936`) | `dw3-switch.nro` | `sdmc:/switch/dw3/eu/` |

Disc dump: the docs describe a Mode2/2352 BIN with its CUE. They do not
describe a plain ISO as supported, so use a BIN and CUE. If the CUE names a
different BIN than the one you have, the port picks the only other `.bin` in
that folder, if it is large enough ([RUNTIME_LAYOUT](RUNTIME_LAYOUT.md)).

**One file for both regions (P6.24).** `dw3-switch.nro` holds both games and
chooses at launch. From the release after preview.53 this is the only `.nro` a
release carries (owner decision 2026-10-29):

- Only one region's data is found: that game starts at once.
- Both are found: a picker shows for 3 seconds and starts the last choice (US
  the first time). D-pad or left stick moves the choice and stops the countdown;
  A starts the highlighted region; Y turns on "always start this one" (no picker
  next time); + quits.
- Hold Minus while you press A on the `.nro` in hbmenu to see the picker again.
- Every decision is a `dw3-region:` line in `dw3.log`.

**Coming from an older preview?** Earlier releases also had `dw3-switch-eu.nro`,
`dw3-switch-both.nro` and the `-nxlink` files. Delete those from `sdmc:/switch/`
(and any Sphaira or hbmenu forwarder that points at them), put the new
`dw3-switch.nro` there and recreate the forwarder if you use one. Your saves and
the `us/` and `eu/` folders do not change.

**Launcher.** A separate launcher `dw3.nro` (a stop-gap, built by CI only) is
**not in releases**. Use the `.nro` files above.

Steps:

1. Download `dw3-switch.nro` from the
   [latest release](https://github.com/Larbaco/dw3-switch/releases/latest).
2. Copy it to `sdmc:/switch/`.
3. Copy your disc's BIN and CUE files:
   Put them into `sdmc:/switch/dw3/` or into `sdmc:/switch/dw3/us/`
   (or `eu/`). It finds a disc in either place and tells the region from the
     disc itself (its executable: `SLUS_014.36` is US, `SLES_039.36` is EU), not
     from the file name. You can leave a US and an EU disc side by side in
     `sdmc:/switch/dw3/`: both regions count as present and the picker appears.
4. Launch from hbmenu. The first boot extracts the game data from the disc and
   draws a progress bar. Leave it running.

The card then looks like this for a US disc (EU uses `eu/`):

```text
sdmc:/switch/dw3-switch.nro
sdmc:/switch/dw3/Digimon World 3.cue
sdmc:/switch/dw3/Digimon World 3.bin
sdmc:/switch/dw3/us/            created on first boot
```

The disc can also sit inside the region folder (`us/` or `eu/`); either works.

Both regions from discs in the parent folder (the picker then asks which to
start, and extracts the one you choose; the other is extracted the first time you
choose it):

```text
sdmc:/switch/dw3/Digimon World 3.bin        US disc
sdmc:/switch/dw3/Digimon World 2003.bin     EU disc
```

After the first boot:

- The next boot checks the extracted files and does not read the BIN again.
- Do not pre-extract files by hand when you want the port to extract them.
- Keep `us/`, `eu/` and `saves/`. Never delete or move `save0.bin`,
  `save1.bin` or `.mcr.bak` to force an extraction. Back up `saves/` (and the old
  `memcard/` folders, if you have them) before deleting a region folder.
- US and EU keep separate folders and settings.
- The port writes `port_options.cfg` (settings) in the region folder and the
  saves under `saves/` (see Saves below). Logs and crash reports go one level up, in
  `sdmc:/switch/dw3/`.

The same file streams its log to a PC when you start it with `nxlink -a <ip> -s
dw3-switch.nro` (development; the port checks at run time whether nxlink started
it). Started from hbmenu or Sphaira it only writes `sdmc:/switch/dw3/dw3.log`.
You do not need nxlink to play. The port does not overclock.

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
| Open Options in a battle | Minus | Only while the battle's command menu waits for you and no shoulder button is held. The battle stops until you close it. |
| Skip an opening movie | Plus | Press or hold. |

Diagnostic chords (not for normal play):

- Minus + ZL + ZR cycles the battle renderer: `soft`, `soft+bbox`, `gpu-solid`,
  `gpu-tex`. The game starts in `gpu-tex`. Field rendering is not affected.
- Minus + L + R toggles the debug HUD in development builds with the HUD
  (`tools/build_switch_game.sh --nxlink`; not in the released file).
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

Options holds the features the port adds. It is a list of groups, drawn like
the title menu: **Display**, **Sound**, **Gameplay**, **Saves**, **Help**, **Walkthrough** (and **Cheats**,
before Help, once you have turned the master switch on, see [Cheats screen](#cheats-screen)).
Up and Down choose a group, B or A opens it. Inside, Up and Down choose a row and
B or A changes it. X goes back: from a group to the list (the cursor stays on that
group), from the list to the title, or to the field menu when opened there, see
[Options in the field](#options-in-the-field). A line under the list explains the
focused item. The words are in the language of the disc. Changes are saved at once in `port_options.cfg`
([PORT_OPTIONS](PORT_OPTIONS.md#persistence)). Defaults keep the original
game, except Widescreen.

| Group | Row | Values | Default | What it does |
|---|---|---|---|---|
| Display | Widescreen | ON / OFF | ON | 16:9 in the field, the 3D battle and most menus: the title menu, the intro's news caption bar, the EU language select, the player name screen, the map, results and save screens, the Items menu, and the card album, deck editor, card shop, gym and Digimon Lab (a few Lab panels are still 4:3). Movies, the still screen and the card battle stay 4:3. Which screens are confirmed on the OLED: [P6.1](P6.1_WIDESCREEN.md). |
| Gameplay | Fast-forward | 2x / 3x / 4x / OFF | 2x | The speed R3 switches to. The game still starts at 1x. OFF disables R3. Music and sound effects speed up too. [P6.4](P6.4_FAST_FORWARD.md) |
| Gameplay | Restart hotkey | ON / OFF | OFF | Enables the L + R + Plus + Minus restart chord. [P6.9](P6.9_RESTART.md) |
| Sound | Music volume | OFF, 1 to 10 | 10 | 10 is the original mix. Left and Right step; B steps up and wraps 10 to 0. [P6.17](P6.17_VOLUME.md) |
| Sound | SFX volume | OFF, 1 to 10 | 10 | Same keys. The screen plays the game's menu sounds, so you hear the change. |
| Gameplay | Encounter rate | OFF / 0.5x / 1x / 1.5x / 2x | 1x | Random battles only. Story battles stay. [P6.18](P6.18_PACING.md) |
| Gameplay | Text speed | Normal / Fast / Instant | Normal | How fast dialogue is typed out. Normal is the original. |
| Gameplay | Battle cursor | OFF / Command / Command+Tech | OFF | The battle command menu (and with Command+Tech the technique list) opens on your last pick. [P7.9](P7.9_BATTLE_MENU_MEMORY.md) |
| Saves | Auto-save | OFF / Each area | OFF | With Each area: at each new field area the game saves to its own file, `autosave.bin`, never over your slots. On the title, **Continue** loads it. See [Saves](#saves). [P6.31](P6.31_AUTOSAVE.md) |

The groups, rows, values and lines on the screen are in the language of the
disc (English on the US disc; English, French, German, Spanish or Italian on the
EU disc). Widescreen is locked while a game is running; change it at the title.

## Help

<!-- Sources: P6.30_HELP.md, port/help/text/en.txt, HOTKEYS.md -->

**Help** is the last group of the Options screen, so it opens from the title, from the
field menu's OPTIONS row and in a battle. It is a short book of ten pages that you can read
without leaving the game: Setup, Controls, Port hotkeys, Turbo, Options explained, Saves,
Widescreen, Map view and mods, Logs and bug reports, and a FAQ. It needs no network and
changes nothing in the game.

- Up and Down choose a page, B or A opens it, X goes back.
- On a page, Up and Down scroll one line, Left and Right a screenful. Amber arrows above or
  below the text show that there is more. **L** and **R** go to the previous and next page. X
  returns to the page list; X on the list returns to the Options list.
- Button names are written in brackets, for example [B] or [ZR], and follow your console
  (the keys on PC).
- A line that is not yet checked on a Switch OLED ends with **(unverified)**. The page title
  says **(draft)** while the owner has not yet signed off the text.
- The pages are in English for now. The EU disc has the screen's own words in French, German,
  Italian and Spanish; the page text for those languages is still to come, so the game shows
  the English page and writes a line in `dw3.log`. Brazilian Portuguese arrives with the
  game's own pt-BR language.
- The Cheats screen is not described in the help.

## Walkthrough

<!-- Sources: P6.35_WALKTHROUGH.md, port/walkthrough/text/en.txt -->

**Walkthrough** is the group after Help. It tells you where to go next and what to do, for
the whole main story (Kusanagi City up to the last battle) and, on the EU disc, the extra
chapter that opens after the ending. It only reads your progress: nothing is written to the game or the
save, and it needs no network.

- From the field menu (or a battle) it opens on the page of **your current step**. The field
  menu itself only opens once you have reached Seiryu City (story progress 4), so earlier
  steps can be read from the title.
- X goes up one level: step page, the steps of the chapter, the chapter list, out. On the
  chapter list the arrow marks your chapter. **L** and **R** on a page go to the previous and next
  entry, also into the next chapter.
- A step page shows a short goal. The answer is hidden: **Answer**, then **Yes** (the cursor starts on
  **No**). Nothing you have not reached yet is shown unless you ask: its title says **Not reached
  yet**, and asking warns that it may spoil the story. An **Area** entry is the whole-area answer.
- At the title there is no game to read, so only the chapter list is shown. If your progress is
  not one the walkthrough knows, it opens the chapter list and says **Step not recognised**.
- The side content (optional quests, recruits, items) is not in it yet.
- The text is a draft written from the game's scripts, not from play: tell us if a step sends you
  to the wrong place. On the EU disc it is in all five languages; French, German, Italian and
  Spanish are machine drafts that still need native review.

## Cheats screen

<!-- Source: PORT_OPTIONS.md#cheats-p620 -->

The Cheats screen is a test aid, not a game mode. On the Options screen press
Y (from the list or from any group). X goes back. The **Cheats** group in the
list shows only once the master switch is on, so you will not see it before you
know the key. Builds made with
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
title). The line under the rows says "Locked in game".

## Options in battle

<!-- Sources: PORT_OPTIONS.md#options-in-battle-p97-slice-2, HOTKEYS.md -->

In a battle, when the command menu (Fight, Tech, DV, Tag, Item, Run) is waiting
for you, press **Minus**. The Options screen opens over the battle, which stops
completely: no enemy attack, no gauge, no animation, for as long as you want. The
music keeps playing. Change what you like (the same rows and the same greyed
Widescreen and Cheats as in the field), then press X: the command menu is back
exactly as it was and the battle goes on. Minus does nothing while an attack
plays or in the technique and item lists, and it does not open the screen while L,
R, ZL or ZR is held (those buttons are part of other shortcuts). On the PC use
Backspace. Battle cursor changes take effect from the next turn's menu.

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

- **Where:** one folder per Switch profile and region,
  `sdmc:/switch/dw3/saves/<uid>/us/memcard/` or `.../saves/<uid>/eu/memcard/`
  (`<uid>` is the profile key, 32 hex digits; `shared` or `default` when there is no profile). The first time a
  profile starts a region, the saves of the old per-region folder
  (`sdmc:/switch/dw3/us/memcard/` or `eu/memcard/`, where earlier versions kept
  them) are **copied** into it; the old folder is left as it was.
  Slots are `save0.bin` and `save1.bin`. Old `memcardN.mcr` files convert once
  and are kept as `.mcr.bak`. Details: [P5.3_FLAT_SAVE](P5.3_FLAT_SAVE.md).
- **In-game saving is manual,** as in the original game. The port flushes
  changed slots at safe points and on HOME or sleep. It never saves for you,
  unless Auto-save is on (see below; it writes its own file).
- **Auto-save (Options, OFF by default).** With **Each area**, the first idle
  frame of each new field area writes the game to its **own file,
  `autosave.bin`**, in the card folder. It never touches your save slots.
  Only an area change writes it: not HOME, sleep or quitting.
  Loading a save is not an area change: the first idle frame after a load only
  sets the starting point.
  - **Continue and Load.** When `autosave.bin` exists the title shows five
    plaques: New Game, Options, **Continue** (loads the auto-save at once),
    **Load Game** (the slot list, as the old Continue) and Quit game. Without the
    file the title has four plaques and the third opens the slot list. Continue
    loads the file even if the option has since been turned OFF.
  - There is one auto-save. A new game replaces it at its first area change.
  - It does **not** run when: you are in an event, a menu, a talk, a climb, an
    area banner or fade, or a battle is starting; the story is not far enough
    for the field menu to open (progress 4 or more); or the map is not one the
    save list knows. A battle that returns you to the same area does not count.
  - If a cheat changed the game, the auto-save is still made and marked
    (`autosave.cheats`), and Continue keeps the game marked (see Cheat marker).
  - A failed write is logged and the game carries on (the old auto-save stays).
    There is no on-screen notice yet. Log lines starting `dw3-autosave` are in
    `dw3.log`.
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
   `crash_report_prev.txt` and the `sdmc:/switch/dw3/dw3*.log` files off the SD
   card. Only one previous report is kept; the logs keep the last 5 runs
   (`dw3.log` is the newest).
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
