# Options reorganization (D1 design)

Status: **design approved by the owner on 2026-10-31** (answers to the five questions are
recorded in [section 7](#7-owner-decisions-2026-10-31)). The build is a series of
separate PRs ([order](#8-build-order)). **Step 1 (reorganization and new rows) is built**
(branch `feat/d1-options-reorg`, 2026-10-31; as built: [PORT_OPTIONS](PORT_OPTIONS.md#options-reorganization-d1)).
Steps 2 to 4 (D2 + D5, D3, D4) are not. Differences of step 1 from the text below: the Map
scaler row is not in it (it comes with D5, which owns the scaler default), so Display has four rows
(Widescreen, Full-screen map, Map mod pack, Version label); the label "Shared saves (exp.)"
and "Frames per stage" are shorter than the design's (the generator's 290 px limit); the message
line of Shared saves is "Experimental: US and EU share saves. Next start." (690 px).

Source: the owner's decisions of 2026-10-31, item 2 (A and A cont.): *every
improvement or change versus the original must be switchable on/off from the Options
menu (not file-only)*, and *the Options menu should be reorganized to be more
coherent*. The rule itself is in [AGENTS.md](../AGENTS.md) (technical directive 8) and
in [PORT_OPTIONS.md](PORT_OPTIONS.md#port-wide-rule-every-change-is-switchable).
Related: [P9.9 groups](PORT_OPTIONS.md#groups-and-sub-screens-p99),
[P6.34 phase 5](P6.34_MENU_FLOW.md#phase-5-menus-group-order-and-help),
[P6.26 global options](PER_USER_SAVES.md), [P6.32 map](P6.32_MAP_ZOOM.md).

Code facts this design rests on (checked on `main` at `95c9861`):

| Fact | Where |
|---|---|
| The groups are one table; the list order is the enum order | `port/include/port_options_groups.h` (`dw3_optgroups[]`, `dw3_optgroups_visible`) |
| A list shows **6** plaques (`DW3_OPTLIST_VISIBLE`), a longer one scrolls with amber arrows; a group holds at most **10** rows (`DW3_OPTGRP_MAX_ROWS`, `DW3_TITLE_OPTION_ROWS`) | same header |
| Registry rows are indexes 0 to 15 (`DW3_PORT_OPT_*`), never renumbered; the cfg keys hang off them | `port/options/port_options.c` |
| Every row change rewrites **all** registry keys, so a value that equals the default is still written to the file | `po_save()` |
| The cfg holds at most **48 keys** and 4096 bytes (`DW3_OPTCFG_MAX_ENTRIES`, `DW3_OPTCFG_TEXT_MAX`); a key is 32 characters at most, a value 48 | `port/include/port_options_cfg.h` |
| Lettering limits: group name 330 px, row label 290 px, value 236 px, message line 690 px; the generator fails when a text is wider | `tools/generate_pc_title_lettering.py` |
| The Options layer is a 960x720 RGBA canvas (the compose logs say `bytes=2764800`); 16:9 hosts add flat side strips | [Menu background](PORT_OPTIONS.md#menu-background-p51-item-6) |
| Sub-screens that are never on the group list already exist (`DW3_OPTGRP_CUSTOM`, ten rows, scrolls) | `port_options_groups.h` |

## 1. Audit of the port's behaviour keys

Sources read: `port_options.c` registry, `port_options_groups.h`, every
`dw3_port_options_file_int/str` call, the region files, `PORT_OPTIONS.md`,
`HOTKEYS.md`, `PATCHES.md`. "Row" = a row on the Options screen today. Defaults are
the values with the key missing. The cfg is per region today; D3 makes it one
global file (see [section 6](#6-where-d2-to-d5-land)).

### 1.1 Keys that already have a row

| Key | Row today (group, registry index) | Default | Region / platform | What it changes versus the original |
|---|---|---|---|---|
| `widescreen` | Display, 0 | 1 (ON) | US, EU, combined; PC + Switch; locked in game | 16:9 in the field, battles and several menus. Title and movies stay 4:3. Off = the PS1 picture |
| `map_screen` | Display, 15 | 0 (OFF) | all; PC + Switch; widescreen only | Square on the Map screen opens a full-screen map. Off = the PS1 Map screen |
| `fast_forward` | Gameplay, 1 | 2 (2x armed; the game starts at 1x) | all | R3 toggles 2x/3x/4x in the field, battle and results. OFF disables R3 |
| `restart_hotkey` | Gameplay, 2 | 0 (OFF) | all | L1+R1+Start+Select restarts the game in process |
| `encounter_rate` | Gameplay, 5 | 0 (= 1x) | all | Random battles OFF / 0.5x / 1x / 1.5x / 2x. Story battles untouched |
| `text_speed` | Gameplay, 6 | 0 (Normal) | all | Dialogue Normal / Fast / Instant (also the area banner for Instant) |
| `battle_menu_memory` | Gameplay, 7 | 0 (OFF) | all | Battle command menu and technique list open on the last pick |
| `region_default` (top-level file, **not** this cfg) | Gameplay, 10 | `ask` | **combined image only** | Which disc starts: Ask (picker) / US / EU |
| `music_volume`, `sfx_volume` | Sound, 3 and 4 | 10 (= the original mix) | all | Two mixer buses |
| `turbo_music` | Sound, 13 | 0 (1x) | all | Music keeps its tempo while fast-forward runs |
| `autosave` | Saves, 8 | 0 (OFF) | all | Writes `autosave.bin` at each new field area; the title shows Continue for it |
| `button_layout` (+ `button_map` while `custom`) | Controls, 9 | `playstation` (**D2: `nintendo`**) | all | Face-button layout: A confirms / B cancels with Nintendo |
| (action row, no key) | Controls, 11 Customize buttons | n/a | all; locked in game | Per-button remap, stored as `button_layout=custom` + `button_map` |
| (action row, no key) | Controls, 12 Reset to default | n/a | all | Puts the default layout back (**D2: Nintendo**) |
| `switch_glyphs` | Controls, 14 | 0 (OFF) (**D2: ON**) | **Switch only** (`DW3_OPT_GLYPHS_ROW`) | The game's button icons become the Switch's buttons |
| `cheat_enabled`, `cheat_instant_win` | Cheats group (its ten rows) | 0 / 0 | build with `DW3_CHEATS` (default); title only | Test aid: master switch, Fill bag, Instant win, Battle now, Money, Warp, Give item. Off = no effect |

The Cheats screen's other rows (Fill bag, Battle now, Cheat saves, Money, Warp, Item id,
Item count, Give item) are one-shot actions, not stored keys.

### 1.2 File-only keys (no row): all get a row (owner, 2026-10-31)

| Key | Default | Region / platform | What it changes versus the original | Row decided |
|---|---|---|---|---|
| `items_equip_from_items` | 1 | all | Cross on a weapon / armor / accessory in the Items menu equips it (patch 0188) | **Menus > Equip from Items** |
| `items_equip_stays` | 1 | all | Equipping returns to the slot list in about 9 frames, cursor kept (patch 0190) | **Menus > Equip stays on list** |
| `items_bag_view` | 1 | all | Square cycles the whole bag in the equip list; move an item between Digimon (patch 0192) | **Menus > Whole bag** |
| `items_use_stays` | 1 | all | Using a consumable keeps the partner step open for the next copy (patch 0193) | **Menus > Use item stays** |
| `items_partner_switch` | 1 | all | ZL / ZR change the partner on the equip panel (patch 0128) | **Menus > Partner switch** |
| `map_mod` | 1 | all; needs a pack in the mods folder | Uses an installed map-upscale pack for the full-screen map ([MOD_MAP_UPSCALE](MOD_MAP_UPSCALE.md)) | **Display > Map mod pack** |
| `show_version` | 1 | all | The title's bottom-right corner shows `dw3-switch v... (sha) US`. The Options footer always shows it (class C, exempt) | **Display > Version label** |
| `save_store` | 0 | all; needs a profile folder; read before the cards open, so a restart | US and EU share one per-profile store (`card0.dw3s`) | **Saves > Shared saves (experimental)**, title only, with a warning ([3.7](#37-saves)) |
| `stage_sweep`, `stage_sweep_frames` | 0 / 90 | build with `DW3_CHEATS`; PC + Switch; read at boot | The opt-in stage sweep walks every stage | **Advanced > Stage sweep, Sweep frames** (new group, [3.9](#39-advanced-new)) |
| `map_view_open`, `_zoom_out`, `_zoom_in`, `_centre`, `_scaler_prev`, `_scaler_next`, `_field_open` | square, l1, r1, select, l2, r2, select | all | Which PS button does each action of the full-screen map | **Controls > Map buttons** sub-page ([3.10](#310-map-buttons-sub-page-under-controls)) |
| (no key yet) `map_scaler` | `fsr1` (owner 7a) | all | The full-screen map's picture scaler | **Display > Map scaler** (D5) |

Internal state, not options and never a row: `last_region` (top-level file), `button_map`
(companion of `custom`), `version`, `end`, and the two migration markers
`defaults_gen` (D2) and `options_global` (D3). The `DW3_*` environment variables (about 70
in the source: `DW3_AUTO_PAD`, `DW3_FORCE_*`, `DW3_REPLAY*`, `DW3_STAGE_SWEEP*`,
`DW3_MAP_*`, `DW3_FB_DUMP`, ...) are test aids; the few that pin an option for one run
(`DW3_WIDESCREEN`, `DW3_TURBO`, `DW3_MUSIC_VOLUME`, `DW3_ITEMS_*`, ...) are not options
either and never gain a row.

**Key budget.** With every row above in one global file the cfg holds about 40 keys (13
registry keys, `button_map`, 2 cheat keys, 5 items, 6 new row keys, 7 map buttons,
`version`, `end`, `defaults_gen`, `options_global`, `region_default`, `last_region`):
under the 48-entry and 4096-byte caps, with 8 spare. The build PR adds a unit test that
writes a file with every key and reads it back, so the next row cannot overflow silently.

### 1.3 Changes versus the original with no key: exempt (owner, 2026-10-31)

The audit found these changes with no key and no row. The owner decided: **no row; all of
them are exempt from the rule**, and the list is part of the rule
([AGENTS.md](../AGENTS.md), [PORT_OPTIONS](PORT_OPTIONS.md#port-wide-rule-every-change-is-switchable)).
A change not in this list needs a row.

| Class | Items (patch / doc) |
|---|---|
| **A. Crash and 64-bit fixes** (the PS1 read or wrote kernel RAM, or the 8 MiB host heap behaves differently) | `0057` stream busy guard, `0058` / `0059` NULL guards, `0069` items-screen icon, `0096` file-cache evict, `0111` tile sprite count, `0197` WSTAG460 wanderer, `0198` BUG-6 wireframe tag |
| **B. Host plumbing** | focus-loss pause, delta clamp, rotating `.mcr` backups, write-through, docked/handheld re-present, vsync wait ([SWITCH_FEATURES](SWITCH_FEATURES.md)); `0043` flat save/load panel pace (100 frames), `0041`/`0042` field-tile preload; per-profile saves (P6.26 slice 1) and the cross-region import (P6.25 slice 1) |
| **C. The port's own screens** | the Options entry (title row, field-menu row, battle Select), Return to title (P6.27), Quit game (P6.28), the Help and Walkthrough screens, the version footer line of the Options screens, the Options menu background |
| **D. Original bugs fixed, always on** | `0065` HUD shows the Digimon the partner fights as, `0066` Claw steal on the killing blow ([P7.7](P7.7_BUG_FIXES.md)), the area-name banner fix, `0187` EU language select accepts Cross, `0129` title logo scale |
| **(not exempt) E. Sub-features behind a row** | the 16:9 patches `0047`-`0115`, `0175`, `0177`, `0179` (Widescreen); the menus at 1.5x (`0176`, Fast-forward); the instant-scroll hold (`0068`, Text speed); the jingle hold (`0194`) and tick gate (`0191`, Turbo music); `0140`-`0142` Continue row for the auto-save (Auto-save). They already have a switch |

## 2. The reorganized tree

### 2.1 Principles

1. **First screen = settings the player changes while playing**; test aids and the two
   help screens sit below the fold.
2. **A group is one idea**: the picture, the sound, the rules of play, the menus, the
   buttons, the files. No group exists for a single row unless the row is a family
   that is growing (Saves).
3. **Registry indexes and cfg keys never change.** Only `dw3_optgroups[]` and a list-order
   table move ([section 5](#5-migration-and-implementation-notes)).
4. **No page needs more than six plaques**, except the sub-screens that already scroll
   (Cheats, Customize) and the new Map buttons sub-page, which scrolls the same way.

### 2.2 The tree

Order of the group list (ten plaques; six fit, so it scrolls by up to four):

| # | Group | EN | FR | DE | ES | IT | Rows (registry index) |
|---|---|---|---|---|---|---|---|
| 1 | Display | Display | Affichage | Anzeige | Pantalla | Schermo | Widescreen (0), Full-screen map (15), **Map scaler**, **Map mod pack**, **Version label** (all new) |
| 2 | Sound | Sound | Son | Ton | Sonido | Audio | Music volume (3), SFX volume (4), Turbo music (13) |
| 3 | Gameplay | Gameplay | Jouabilité | Gameplay | Jugabilidad | Gameplay | Fast-forward (1), Encounter rate (5), Text speed (6), Battle cursor (7) |
| 4 | **Menus** (new) | Menus | Menus | Menüs | Menús | Menu | **Equip from Items**, **Equip stays on list**, **Whole bag**, **Use item stays**, **Partner switch** (the five `items_*` keys) |
| 5 | Controls | Controls | Commandes | Steuerung | Controles | Comandi | Button layout (9), Customize buttons (11), **Map buttons** (new sub-page), Reset to default (12), Switch glyphs (14, Switch), **Restart hotkey (2, moved from Gameplay)** |
| 6 | Saves | Saves | Sauvegardes | Speichern | Partidas | Salvataggi | Auto-save (8), **Region at start (10, moved from Gameplay; combined image only)**, **Shared saves (experimental)** (new) |
| 7 | Cheats | Cheats | Triches | Cheats | Trucos | Trucchi | the ten Cheats rows (unchanged; title only, greyed in game) |
| 8 | **Advanced** (new; builds with `DW3_CHEATS`) | Advanced | Avancé | Erweitert | Avanzado | Avanzate | **Stage sweep**, **Sweep frames per stage** |
| 9 | Help | Help | Aide | Hilfe | Ayuda | Aiuto | a screen of its own (unchanged) |
| 10 | Walkthrough | Walkthrough | Pas à pas | Komplettlösung | Guía | Soluzione | a screen of its own (unchanged) |

The existing group names in FR/DE/ES/IT are the P9.9 lettering already in
`GROUP_TEXT`. The two new names, **Menus** and **Advanced**, have machine drafts like the
other FR/DE/IT/ES texts and need native review (the same P6.30 rule).

**Where the stage sweep goes (proposal, answers the owner's "propose").** A new last-but-two
group **Advanced**, next to Cheats because both are test aids and the sweep exists only in
builds that have the Cheats feature (`DW3_CHEATS`, so it is listed exactly when Cheats is,
`DW3_OPTGRP_CHEATS_LISTED`). It is not Gameplay (it is not play) and it cannot join Cheats
(that sub-screen already holds the maximum of ten rows). Help and Walkthrough stay the
last two plaques as they are today. Advanced has room for later developer rows.

What moved and why:

| Move | From | To | Reason |
|---|---|---|---|
| Restart hotkey | Gameplay | Controls (last row) | It is a chord. P9.9 planned this "when P6.12 exists"; P6.12 exists. It goes last so Switch glyphs keeps its place |
| Region at start | Gameplay | Saves | It chooses which disc and which saves open; it pairs with Shared saves. Combined image only |
| Controls | 6th (after Cheats) | 5th (before Saves) | Settings first |
| Saves | 4th | 6th | After Controls |
| Cheats | 5th | 7th | A test aid belongs beside Advanced and the help screens |
| Items keys | file-only | Menus group | The owner's order, and P6.34 phase 5's planned group |
| Gameplay | six rows (combined) | four | No more scrolling inside Gameplay |

Rows with **no cfg key change at all** are every row that moves. The `[cheats listed,
master off, ...]` and `group open <name>` log lines keep their form; the new groups log
as `menus` and `advanced`.

### 2.3 Availability of each new or moved row

| Row | Where it works | Switch only? | Combined only? | Applies |
|---|---|---|---|---|
| Map scaler, Map mod pack | title, field menu, battle | no | no | at the next open of the full-screen map (the map cache keys on the picture source). The scaler key is read at each open; the L2 / R2 change in the view stays for the session (not written) |
| Version label | title, in game | no | no | at once on the title; the Options footer stays |
| Menus: the five rows | all (the hooks read the key at each press) | no | no | at once |
| Map buttons (sub-page) | all (the view reads the keys at each open) | no | no | at the next open of the map view |
| Restart hotkey | all | no | no | at once |
| Region at start | all (not locked) | no | **yes** | next start, or the Yes in the P6.38 restart question |
| Shared saves | **title only** (greyed in game: the cards are opened before it is read) | no | no | next start; confirm window when turned on |
| Stage sweep, Sweep frames | **title only** (greyed in game; the sweep reads them at boot) | no | no; needs `DW3_CHEATS` | next start |
| Switch glyphs | all | **yes** | no | at once |
| Widescreen, Customize buttons, Cheats | **title only** (as today) | no | no | unchanged |

The Help and Walkthrough entries are unchanged: Confirm opens the screen on its own full
canvas layer, from the title, the field menu and a battle. Help gains the **Options
explained** text for the new tree and the "Items and equipment" page already planned by
P6.34 phase 5. The Walkthrough keeps its place at the end of the list.

### 2.4 Rows per page against the limit (6 visible, 10 maximum)

| Page | Rows, PC US plain | Rows, Switch combined | Fits in 6? |
|---|---|---|---|
| Group list | 10 | 10 | no, scrolls (offsets 0 to 4; today 0 to 2) |
| Display | 5 | 5 | yes |
| Sound | 3 | 3 | yes |
| Gameplay | 4 | 4 | yes |
| Menus | 5 | 5 | yes |
| Controls | 5 (no Switch glyphs) | **6** (Layout, Customize, Map buttons, Reset, Glyphs, Restart) | yes, exactly full on the Switch |
| Saves | 2 | 3 | yes |
| Cheats | 10 | 10 | scrolls, as today |
| Advanced | 2 | 2 | yes |
| Customize (sub-screen) | 10 | 10 | scrolls, as today |
| Map buttons (sub-screen) | 8 (7 bindings + Reset map buttons) | 8 | scrolls like Customize (6 visible, offset 0 to 2) |

Controls on the Switch is full at six rows; a seventh would scroll. Anything else for
Controls goes in a sub-page. The plaque size, the 690 px message strip and the 960x720 canvas
stay as they are, and the 16:9 and 4:3 presentations differ only in the flat side strips
(which D4 replaces by the tiled scrolling background).

## 3. ASCII mock of each page

Plaque = label left, value right; `>` is the focused plaque (the white pointer and lit
halo in the game). The footer hints are drawn from the button map in force
(Switch with D2: `A Change`, `B Back`).

### 3.1 Group list (scroll offset 0, then 4)

```
+--------------------------------------------------------------+
| OPTIONS                                                      |
+--------------------------------------------------------------+
|  > Display                                                   |
|    Sound                                                     |
|    Gameplay                                                  |
|    Menus                                                     |
|    Controls                                                  |
|    Saves                                                  v  |   <- amber arrow: more below
+--------------------------------------------------------------+
| How the picture fills the screen.                            |
+--------------------------------------------------------------+
|                                  A Open        B Back        |
+--------------------------------------------------------------+

scrolled to the end (offset 4):

|  ^                                                           |
|    Controls                                                  |
|    Saves                                                     |
|    Cheats                                                    |
|    Advanced                                                  |
|    Help                                                      |
|  > Walkthrough                                               |
```

(Ten plaques, six visible: the window shows items 5 to 10 at offset 4. In a `--no-cheats`
build Cheats and Advanced are not listed and the list has eight plaques.)

### 3.2 Display

```
+--------------------------------------------------------------+
| DISPLAY                                                      |
+--------------------------------------------------------------+
|  > Widescreen                                         ON     |   locked in game
|    Full-screen map                                    OFF    |
|    Map scaler                                         FSR 1  |   D5: FSR 1 / Nearest /
|    Map mod pack                                       ON     |       Sharp bilinear / Lanczos2 / Bilinear
|    Version label                                      ON     |
+--------------------------------------------------------------+
| Uses the full 16:9 screen where the game allows it.          |
+--------------------------------------------------------------+
|                                  A Change      B Back        |
+--------------------------------------------------------------+
```

### 3.3 Sound

```
+--------------------------------------------------------------+
| SOUND                                                        |
+--------------------------------------------------------------+
|  > Music volume                                       10     |
|    SFX volume                                         10     |
|    Turbo music                                        1x     |
+--------------------------------------------------------------+
| Left / Right change the music volume (10 = original).        |
+--------------------------------------------------------------+
|                                  A Change      B Back        |
+--------------------------------------------------------------+
```

### 3.4 Gameplay

```
+--------------------------------------------------------------+
| GAMEPLAY                                                     |
+--------------------------------------------------------------+
|  > Fast-forward                                       2x     |
|    Encounter rate                                     1x     |
|    Text speed                                         Normal |
|    Battle cursor                                      OFF    |
+--------------------------------------------------------------+
| Press R3 in the field or in battle to switch speed.          |
+--------------------------------------------------------------+
|                                  A Change      B Back        |
+--------------------------------------------------------------+
```

### 3.5 Menus (new)

```
+--------------------------------------------------------------+
| MENUS                                                        |
+--------------------------------------------------------------+
|  > Equip from Items                                   ON     |
|    Equip stays on list                                ON     |
|    Whole bag                                          ON     |
|    Use item stays                                     ON     |
|    Partner switch                                     ON     |
+--------------------------------------------------------------+
| Cross on gear in the Items menu equips it. OFF = original.   |
+--------------------------------------------------------------+
|                                  A Change      B Back        |
+--------------------------------------------------------------+
```

Message lines (one per row, written in the build PR, each says what OFF does):
Equip from Items "Cross on gear in the Items menu equips it. OFF = original.", Equip
stays "Equipping returns to the slot list. OFF = the long fades.", Whole bag "Square
cycles the whole bag; move items between Digimon. OFF = original.", Use item stays "A use
keeps the partner step open. OFF = back to the list.", Partner switch "ZL / ZR change the
Digimon on the equip panel. OFF = original." These are switch rows: Confirm toggles
(Left / Right do nothing, as for Auto-save and Map screen). All five default ON (the
owner's list says default 1).

### 3.6 Controls

```
+--------------------------------------------------------------+
| CONTROLS                                                     |
+--------------------------------------------------------------+
|  > Button layout                                  Nintendo   |   D2 default
|    Customize buttons                                         |   sub-screen, locked in game
|    Map buttons                                               |   sub-screen (new)
|    Reset to default                                          |   goes to Nintendo (D2)
|    Switch glyphs                                      ON     |   Switch only; D2 default ON
|    Restart hotkey                                     OFF    |   moved from Gameplay
+--------------------------------------------------------------+
| Nintendo: A confirms, B cancels. Applies now.                |
+--------------------------------------------------------------+
|                                  A Change      B Back        |
+--------------------------------------------------------------+
```

Customize buttons and Map buttons are action rows (no value, Confirm opens the sub-screen),
listed next to each other. Customize keeps its ten-row sub-screen (`DW3_OPTGRP_CUSTOM`)
unchanged.

### 3.7 Saves

```
+--------------------------------------------------------------+
| SAVES                                                        |
+--------------------------------------------------------------+
|  > Auto-save                                          OFF    |
|    Region at start                                    Ask    |   combined image only
|    Shared saves (experimental)                        OFF    |   title only
+--------------------------------------------------------------+
| How and when the game saves.                                 |
+--------------------------------------------------------------+
|                                  A Change      B Back        |
+--------------------------------------------------------------+
```

**Shared saves (experimental)** is `save_store`, default OFF, title only (greyed in game
with the usual "Locked in game" line). Label: "Shared saves (experimental)"; if the
generator finds it wider than 290 px the label becomes "Shared saves (exp.)" and the
word "experimental" moves to the message line. Message line: "Experimental. US and EU
share one save store. Takes effect at the next start." Turning it **ON** opens the
port's Yes / No window (`port/options/confirm.c`, cursor on **No**, the pattern of Quit
game and the P6.38 restart question):

```
+----------------------------------------------+
| Shared saves (experimental)                  |
|                                              |
| The US and EU games will share one save      |
| store. Back up your saves first. This takes  |
| effect at the next start. Turn it on?        |
|                                              |
|                 Yes      > No                |
+----------------------------------------------+
```

Yes writes `save_store=1` and logs `dw3-options: save_store 0->1 (confirmed, next start)`;
No leaves it OFF (`... declined`). Turning it OFF needs no window. The window text and the
row label are in the five languages. The existing store design, backups and migration stay
as in [CROSS_REGION_SAVES](CROSS_REGION_SAVES.md#as-built-slice-2); with D3 the key lives
in the one global file, so it no longer has to be set in both regions.

### 3.8 Cheats, Help, Walkthrough

Cheats keeps its ten-row scrolling sub-screen exactly as built (P9.9 slice 2), greyed in
game. Help and Walkthrough have no rows; their plaques open the full-canvas screens. The
only change to these three is their place in the list (7, 9, 10).

### 3.9 Advanced (new)

```
+--------------------------------------------------------------+
| ADVANCED                                                     |
+--------------------------------------------------------------+
|  > Stage sweep                                        OFF    |   OFF / Run / Start over; title only
|    Sweep frames per stage                             90     |
+--------------------------------------------------------------+
| Test aid. Walks every stage after the next start.            |
+--------------------------------------------------------------+
|                                  A Change      B Back        |
+--------------------------------------------------------------+
```

- **Stage sweep** = `stage_sweep`: `OFF` (0, default), `Run` (1: run, or continue the run a
  crash cut short), `Start over` (2: forget the progress file). Confirm and Left / Right
  step. It reads at boot, so it is title only and takes effect at the next start.
  Choosing `Run` or `Start over` shows the Yes / No window (cursor on No): "The game will
  walk every stage by itself at the next start, with nobody at the pad. Turn it on?".
  P5.5 says to put the key back to 0 when done: the row makes that one press. **Build
  PR must check** (unverified): the sweep does not rewrite its own key, so after a
  `Start over` has run once the row must show `Run` (write 2 -> 1 when the progress file is
  cleared), otherwise the sweep restarts from zero at every boot.
- **Sweep frames per stage** = `stage_sweep_frames`: 30 to 3600, default 90. Left / Right
  step by 30, L1 / R1 by 300 (the Cheats screen's pattern for Item id), shown as a
  number in the title lettering. Greyed while Stage sweep is OFF.
- With D3 the key sits in the global file, so the old rule "put it in the region's file,
  the top-level file is ignored" goes away; the log line `dw3-sweep: ... region=us|eu`
  (D3 note) says which region ran.

### 3.10 Map buttons (sub-page under Controls)

A sub-screen like Customize (`DW3_OPTGRP_MAPBTN`, never on the group list, eight rows, six
visible, so it scrolls). Each row is one of the seven `map_view_*` keys; the value is the
console button that makes the chosen PS button under the layout and map in force (the
same words the full-screen map's help line uses; PS word on the PC).

```
+--------------------------------------------------------------+
| MAP BUTTONS                                                  |
+--------------------------------------------------------------+
|  > Open map (Map screen)                              Y      |
|    Zoom out                                           L      |
|    Zoom in                                            R      |
|    Centre on my area                                  Minus  |
|    Previous scaler                                    ZL     |
|    Next scaler                                        ZR   v |
+--------------------------------------------------------------+
| Used by the full-screen map. Left / Right pick a button.     |
+--------------------------------------------------------------+
|                                  A Change      B Back        |
+--------------------------------------------------------------+

scrolled (offset 2):
|    Previous scaler                                    ZL     |
|    Next scaler                                        ZR     |
|    Open map from the field                            Minus  |
|  > Reset map buttons                                         |
```

- Rows: the keys `map_view_open`, `_zoom_out`, `_zoom_in`, `_centre`, `_scaler_prev`,
  `_scaler_next`, `_field_open`, then the action row **Reset map buttons** (puts the seven
  defaults back, also in game). The cfg keys, their word values and defaults do not change.
- Values reachable from the screen: `cross square l1 r1 l2 r2 l3 select` for the six view
  actions; the field key takes the same list without `cross` (the field's talk button),
  exactly the rules in [PORT_OPTIONS](PORT_OPTIONS.md#full-screen-map-row-and-the-map-views-buttons-p632-phase-4).
- **Conflicts are resolved on the screen**: choosing a button that another of the six view
  actions already has **swaps** the two (the Customize pattern), so the six always differ
  and the `BINDING PROBLEM` reset-everything rule can only be reached by editing the file.
  The field key may equal the centre key (different place and time).
- Not locked in game (the view reads the keys at each open). The entry row **Map buttons**
  in Controls says "Which buttons drive the full-screen map. Needs Full-screen map ON." and
  is not greyed when that is OFF (the player can set buttons first).
- Logs: `dw3-port-options: group open mapbtn (8 row(s), title|in game)`, then the
  existing `dw3-map: bindings in effect ...` line at the next open.

## 4. Help and text consequences

- `port/help/text/<lang>.txt` page **options** ("Options explained"): rewrite the group
  paragraph and the per-group bullets for the new tree (Menus, Advanced, the moved rows,
  the new Display and Saves rows, Map buttons). Five languages, the usual machine-draft tag
  for FR/DE/IT/ES.
- P6.34 phase 5's "Items and equipment" Help page ships with the Menus group.
- `GROUP_TEXT` gains `menus`, `advanced` and `mapbtn`; `ROW_TEXT` gains the new labels and
  the Yes / No window texts; the generator's width check runs on all of them (row label
  290 px: "Shared saves (experimental)", "Equip stays on list", "Sweep frames per stage" and
  "Open map from the field" are the widest candidates, the generator is the judge).
- `PLAYER_GUIDE.md`, `HOTKEYS.md` (the P9.9 paragraph lists the groups and says the six
  map keys are file-only), `P5.5_STAGE_SWEEP.md` ("Turn it on" says the keys are file-only
  and goes in the region file) and the OLED test plan rows that name a plaque by position
  need the new positions.

## 5. Migration and implementation notes

**Config files: nothing to migrate for the reorganization itself.** Every cfg key keeps
its name, range and default. The five `items_*` keys, `map_mod`, `show_version`,
`save_store`, `stage_sweep`, `stage_sweep_frames` and the seven `map_view_*` keys already live
in the file; they are only *read by a row as well*. A file written by any earlier build
loads as before. The new key is `map_scaler` (words `fsr1`, `nearest`, `sharp`,
`lanczos2`, `bilinear`; default `fsr1` per the owner's 7a); a missing or invalid value
falls back to the default with the usual `is not valid` log line. The only migration is the
one-shot of D2 ([section 6](#6-where-d2-to-d5-land)).

**Registry.** New rows are appended after index 15 (`DW3_PORT_OPT_COUNT` grows; the
`_Static_assert` and the comment in `port_options_groups.h` are updated; exact numbers are
fixed in the build PR): the five Menus switches, Map scaler, Map mod pack, Version label,
Shared saves, Stage sweep, Sweep frames, and the action row Map buttons. Rows 0 to 15 keep
their numbers. The seven map buttons are not registry rows: like the Customize screen's ten
rows they come from their own module and the sub-screen. The `items_*` env pins
(`DW3_ITEMS_*=0`) keep working: a pinned row keeps the file's value when another row saves,
as for the volumes.

**Groups table.** The list order is currently the enum order. To reorder without
renumbering the page codes' meaning, add a small `dw3_optgroup_order[]` and have
`dw3_optgroups_visible` walk it; add `DW3_OPTGRP_MENUS`, `DW3_OPTGRP_ADVANCED` and
`DW3_OPTGRP_MAPBTN` (never listed, `built` 0) after `DW3_OPTGRP_CUSTOM`
(`DW3_OPTGRP_COUNT` 9 to 12); `DW3_OPTGRP_MAX_ROWS` stays 10. Advanced is listed under the
same `DW3_OPTGRP_CHEATS_LISTED` condition as Cheats. The overlay page code
(`dw3_optlist_page`) is internal to the cache, not stored anywhere, so changing it only
changes cache keys. `g_group_names` gets `menus`, `advanced`, `mapbtn`. A group whose rows
are all hidden in a given build is skipped by the same visible-list pass.

**Replay references that change** (internal count to be measured by the build PR with
`tools/replay_tests.sh --changed`; the recipes in `tools/replay/gen_scenarios.py` walk the
list by plaque position, e.g. "Help is the sixth plaque", "the Walkthrough group is the
eighth", so the **recordings** change as well as the pictures):

| Family (US and EU, and the language variants) | Why |
|---|---|
| `options-persist`, `volume-persist`, `battle-cursor-persist`, `pacing-persist`, `options-sound`, `options-cheats-listed`, `options-noversion`, `options-region`, `options-region-relaunch-*` | the group list changes (ten plaques, new order); Gameplay loses Restart hotkey and Region; Saves gains Region and Shared saves; Display gains three rows |
| `field-options` (+ `-fr -de -it -es`, `-cheats`, `-nx`, `-release-nx`), `battle-options` (+ `-ref`, `-nx`) | the group list and the Display page |
| `cheats-screen`, `cheats-screen2` | the Cheats plaque moves from 5th to 7th (the path to it changes); the sub-screen itself is unchanged |
| `controls-layout`, `controls-custom`, `controls-custom-invalid` | Controls moves from 6th to 5th and gains Map buttons and Restart hotkey |
| `help-title`, `help-field`, `help-battle`, `walkthrough-screen-*` | the path through the list (Advanced adds a plaque before Help); the screens themselves are unchanged |
| `turbo-menu`, `restart-*`, `map-rebind-relaunch` and any recipe that turns a row on through the screen | navigation counts |
| new | `*-options-menus` (Menus page, a toggle of each row persists), Display with the new rows, Saves with Region and Shared saves (and its Yes / No window), Advanced, Map buttons (both scroll offsets, a swap) |

The family also changes on D2 (the Controls values), D4 (the background) and D5 (the
Display values). The build order below keeps the regenerations to three.

## 6. Where D2 to D5 land

| Item | Lands in | What changes in this tree |
|---|---|---|
| **D2** `button_layout` default `nintendo`, `switch_glyphs` default ON, **one-shot migration of existing files** | Controls (rows 1 and 5) | no placement change. Values on the page and the Help text change. **"Reset to default" goes to Nintendo**, so its message and the Help text say so. Migration: the marker key below. `DW3_BUTTON_MAP=default` stays the PlayStation pin for tests (a test aid, not an option) |
| **D3** one global options file | no page change | `sdmc:/switch/dw3/port_options.cfg` holds every key, including `save_store` and `stage_sweep`. The rows read and write the global file. Region at start already lives there. The Shared saves row is no longer "set it in both regions" |
| **D4** A18 scrolling background | the look of all pages | the scroll is under the retained canvas, so no row or group is affected; the side strips of 16:9 are tiled by the quad. Plaques, strip and hints do not move. All Options refs change again, so D4 comes last |
| **D5** FSR 1 map scaler, default always | Display, row 3 **Map scaler** | the row set by 7a. FSR 1 is the default, the map cache builds off the game thread as today; the GPU FSR form (P6.32 7.8 item 2) removes the first-open build |

### 6.1 D2 migration marker (owner, 2026-10-31, question 4)

Facts: `po_save()` writes every key, so any file saved by a build since P9.8 holds
`button_layout=playstation` and `switch_glyphs=0` even when nobody chose them; the new
defaults would never reach it. The owner decided to **migrate once**.

- **Marker key: `defaults_gen`** (an integer; a missing key counts as 1, the generation
  before D2). It is a generic "defaults generation" so a later default change does not
  need a new mechanism. D2 sets it to **2**. It is internal (no row), kept like every
  unknown key, written by the registry on every save.
- **When:** at load (`persist_init`), only if a config file exists **and** `defaults_gen`
  is below 2. A fresh install (no file) is not migrated; its first save writes
  `defaults_gen=2` with the new defaults.
- **What it flips:** `button_layout=playstation` to `nintendo`, and `switch_glyphs=0` to
  `1`. **Not touched:** `nintendo` (already there), and `button_layout=custom` with its
  `button_map` (a deliberate choice that flipping would destroy; recommendation, the build
  PR keeps it unless the owner says otherwise), and a missing key (it simply takes the new
  default). `switch_glyphs` is set on the PC too but the PC has no row and the setting only
  acts on the Switch.
- **Write:** the migrated file is written at once (atomic write) with `defaults_gen=2`, so
  it runs once. If the write fails the migration stays in memory, is logged, and is retried
  at the next save and the next boot (nothing else in the file is harmed). Env pins
  (`DW3_BUTTON_MAP`, `DW3_SWITCH_GLYPHS`) are applied after it as always and never saved.
- **Log (once per boot that migrates, AGENTS.md rule 12):**
  `dw3-options: migrate defaults_gen 1->2 button_layout playstation->nintendo switch_glyphs 0->1 (kept: <list of untouched keys, e.g. custom>)`; when nothing needed flipping:
  `dw3-options: defaults_gen 1->2 nothing to flip`; on a failed write:
  `dw3-options: migrate defaults_gen NOT saved (<reason>), will retry`. Fresh install:
  `dw3-options: defaults_gen=2 (new file)`.
- **Release notes:** the preview that ships D2 says, in the notes and the README: "Existing
  settings files are migrated once: Button layout becomes Nintendo (A confirms) and Switch
  glyphs becomes ON. Change them back in Options > Controls; they stay." The OLED plan names
  the log line as the proof.
- **Replays:** a scenario cfg that holds any key but no `defaults_gen` would be migrated
  on load. The D2 PR makes the replay runner write `defaults_gen=2` into every scenario cfg
  (and the two-phase scenarios keep it between phases), and scenarios that need the PS layout
  pin `button_layout=playstation` **with** the marker. A new replay `*-defaults-migrate`
  feeds an old file (`button_layout=playstation`, `switch_glyphs=0`, no marker) and
  checks the flip, the log line and the second boot not flipping a value the player changed
  back.
- **Order with D3:** D2 ships first, so each region's file is migrated by itself. D3's merge
  then sees `defaults_gen=2` in both and applies its own rule (`last_region` wins per key).
  If a file still has no marker when D3 merges, D3 copies the key into the global file and
  the migration runs on the merged file.

## 7. Owner decisions (2026-10-31)

The five questions of the first version of this document and the owner's answers (reply
relayed by the orchestrator, 2026-10-31). Each question is kept as asked, with my original
recommendation, then the decision.

### Q1. The tree

*Asked:* nine groups, order Display, Sound, Gameplay, Menus, Controls, Saves, Cheats, Help,
Walkthrough; Restart hotkey to Controls; Region at start to Saves; the five `items_*` rows in
Menus, all ON. *Recommendation:* approve.

**Decision, 2026-10-31: APPROVED as proposed (Menus is a new group).** The decisions of Q2
add one group (Advanced, for the stage sweep) and one sub-screen (Map buttons), so the list
is now ten plaques; the rest of the tree is as approved.

### Q2. The flagged keys

*Asked:* `show_version` row or file-only; `save_store` no row yet; `stage_sweep` and the
`map_view_*` bindings file-only. *Recommendation:* a row for `show_version`, the others
file-only.

**Decision, 2026-10-31: EVERYTHING gets a row.**
- `show_version`: **Version label** row in Display.
- `map_mod`: **Map mod pack** row in Display.
- `save_store`: a row now, in Saves, **title only**, labelled experimental ("Shared saves
  (experimental)"), default OFF, with a confirm window when it is turned on
  ([3.7](#37-saves)).
- `stage_sweep` and `stage_sweep_frames`: rows, default OFF. *Proposal (the owner asked me to
  propose where):* a new **Advanced** group next to Cheats ([3.9](#39-advanced-new)).
- the seven `map_view_*` bindings: rows in a **Map buttons** sub-page under Controls, next to
  Customize, so the six-row page limit holds ([3.10](#310-map-buttons-sub-page-under-controls)).

Row counts per page were re-checked in [2.4](#24-rows-per-page-against-the-limit-6-visible-10-maximum):
every listed page fits in six; Controls on the Switch is exactly six; Map buttons has eight
rows and scrolls like Customize.

### Q3. Always-on changes with no switch

*Asked:* exempt crash / 64-bit fixes, host plumbing and the port's own screens; one
"Original bug fixes" row for P7.7 `0065` / `0066`. *Recommendation:* as asked.

**Decision, 2026-10-31: NO row.** The P7.7 fixes `0065` and `0066` stay always on and are
exempt, like the crash and 64-bit fixes, the host plumbing and the port's own screens. The
exemption list is part of the rule ([1.3](#13-changes-versus-the-original-with-no-key-exempt-owner-2026-10-31),
[AGENTS.md](../AGENTS.md), [PORT_OPTIONS](PORT_OPTIONS.md#port-wide-rule-every-change-is-switchable)).
The "Original bug fixes" row of the first version is removed from the tree.

### Q4. D2 and the old config files

*Asked:* the new defaults reach only a missing key and files since P9.8 already hold
`button_layout=playstation` / `switch_glyphs=0`. *Recommendation:* do not guess, the owner
flips the two rows once.

**Decision, 2026-10-31: MIGRATE ONCE.** A one-shot marker key flips `button_layout` to
`nintendo` and `switch_glyphs` to on in files written before the change; it is logged and
recorded in the release notes; "Reset to default" goes to Nintendo. Marker `defaults_gen`,
the log lines and the edge cases are in [6.1](#61-d2-migration-marker-owner-2026-10-31-question-4).

### Q5. Order of the build PRs

*Asked:* reorganization and new rows, then D2 with D5, then D3, then D4 last.
*Recommendation:* as asked.

**Decision, 2026-10-31: as recommended** ([section 8](#8-build-order)).

## 8. Build order

1. **Reorganization and new rows**: the groups table and order, Menus, Advanced, Map buttons,
   the Display and Saves rows (Map mod pack, Version label, Shared saves with its warning,
   Map scaler as a row over the current scaler code), Help text, five-language lettering,
   unit tests (key budget, every row persists), replays regenerated once.
2. **D2 + D5 together**: the Controls defaults and the `defaults_gen` migration (6.1), the
   FSR 1 default and the GPU FSR form; only the values on the Controls and Display pages
   change.
3. **D3**: one global options file (no picture changes).
4. **D4**: the scrolling background, last; the replays pin the scroll offset to 0 so its
   references are stable.

The Options references are regenerated three times (after 1, 2 and 4) instead of five.
