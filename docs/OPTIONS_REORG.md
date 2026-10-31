# Options reorganization (D1 design)

Status: **design only, written 2026-10-31, waiting for the owner's answers**
([questions](#7-questions-for-the-owner)). No game code, no registry change and no
replay reference changes in the PR that adds this file. The build is a separate PR
after the owner approves.

Source: the owner's decisions of 2026-10-31, item 2 (A and A cont.): *every
improvement or change versus the original must be switchable on/off from the
Options menu (not file-only)*, and *the Options menu should be reorganized to be
more coherent*. The rule itself is now in [AGENTS.md](../AGENTS.md) (technical
directive 8) and in [PORT_OPTIONS.md](PORT_OPTIONS.md#port-wide-rule-every-change-is-switchable).
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
| Lettering limits: group name 330 px, row label 290 px, value 236 px, message line 690 px; the generator fails when a text is wider | `tools/generate_pc_title_lettering.py` |
| The Options layer is a 960x720 RGBA canvas (the compose logs say `bytes=2764800`); 16:9 hosts add flat side strips | [Menu background](PORT_OPTIONS.md#menu-background-p51-item-6) |

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
| (action row, no key) | Controls, 12 Reset to default | n/a | all | Puts the default layout back |
| `switch_glyphs` | Controls, 14 | 0 (OFF) (**D2: ON**) | **Switch only** (`DW3_OPT_GLYPHS_ROW`) | The game's button icons become the Switch's buttons |
| `cheat_enabled`, `cheat_instant_win` | Cheats group (its ten rows) | 0 / 0 | build with `DW3_CHEATS` (default); title only | Test aid: master switch, Fill bag, Instant win, Battle now, Money, Warp, Give item. Off = no effect |

The Cheats screen's other rows (Fill bag, Battle now, Cheat saves, Money, Warp, Item id,
Item count, Give item) are one-shot actions, not stored keys.

### 1.2 File-only keys (no row): the gap the new rule closes

| Key | Default | Region / platform | What it changes versus the original | Verdict |
|---|---|---|---|---|
| `items_equip_from_items` | 1 | all | Cross on a weapon / armor / accessory in the Items menu equips it (patch 0188) | **Owner ordered a row** |
| `items_equip_stays` | 1 | all | Equipping returns to the slot list in about 9 frames, cursor kept (patch 0190) | **Owner ordered a row** |
| `items_bag_view` | 1 | all | Square cycles the whole bag in the equip list; move an item between Digimon (patch 0192) | **Owner ordered a row** |
| `items_use_stays` | 1 | all | Using a consumable keeps the partner step open for the next copy (patch 0193) | **Owner ordered a row** |
| `items_partner_switch` | 1 | all | ZL / ZR change the partner on the equip panel (patch 0128) | **Owner ordered a row** |
| `map_mod` | 1 | all; needs a pack in the mods folder | Uses an installed map-upscale pack for the full-screen map ([MOD_MAP_UPSCALE](MOD_MAP_UPSCALE.md)) | **Add a row** (a change versus the original picture; only visible with a pack, and the map scaler row of D5 sits next to it) |
| `map_view_open`, `_zoom_out`, `_zoom_in`, `_centre`, `_scaler_prev`, `_scaler_next`, `_field_open` | square, l1, r1, select, l2, r2, select | all | Which PS button does each action of the full-screen map | **Flag** (bindings of an optional feature, not a change versus the original). Recommend: stay file-only, documented as advanced. A Customize-style screen is a later item |
| `show_version` | 1 | all | The title's bottom-right corner shows `dw3-switch v... (sha) US`. The Options footer always shows it | **Flag** (owner said so: a test label) |
| `save_store` | 0 | all; needs a profile folder; read before the cards open, so a restart | US and EU share one per-profile store (`card0.dw3s`) | **Flag** (experimental) |
| `stage_sweep`, `stage_sweep_frames` | 0 / 90 | build with `DW3_CHEATS`; PC + Switch | The opt-in stage sweep walks every stage | **Flag** (dev). Recommend: stays file-only |

Internal state, not options and never a row: `last_region` (top-level file), `button_map`
(companion of `custom`), `version`, `end`, and the future `options_global` migration
marker of D3. The `DW3_*` environment variables (about 70 in the source: `DW3_AUTO_PAD`,
`DW3_FORCE_*`, `DW3_REPLAY*`, `DW3_STAGE_SWEEP*`, `DW3_MAP_*`, `DW3_FB_DUMP`, ...) are
test aids; the few that pin an option for one run (`DW3_WIDESCREEN`, `DW3_TURBO`,
`DW3_MUSIC_VOLUME`, `DW3_ITEMS_*`, ...) are not options either and never gain a row.

### 1.3 Changes versus the original with no key at all (found by the audit)

The rule says *every* change. These have no key and no row; some are plain portability
work the original never needed. They are listed so the owner can say which class each is
in; **nothing here is added without an answer** ([Q3](#7-questions-for-the-owner)).

| Class | Items (patch / doc) | Proposed handling |
|---|---|---|
| **A. Crash and 64-bit fixes** (the PS1 read or wrote kernel RAM, or the 8 MiB host heap behaves differently) | `0057` stream busy guard, `0058` / `0059` NULL guards, `0069` items-screen icon, `0096` file-cache evict, `0111` tile sprite count, `0197` WSTAG460 wanderer, `0198` BUG-6 wireframe tag | Exempt. No original behaviour to switch back to: it crashes or reads garbage |
| **B. Host plumbing with no visible game change** | focus-loss pause, delta clamp, rotating `.mcr` backups, write-through, docked/handheld re-present, vsync wait ([SWITCH_FEATURES](SWITCH_FEATURES.md)); `0043` flat save/load panel pace (100 frames), `0041`/`0042` field-tile preload; per-profile saves (P6.26 slice 1) and the cross-region import (P6.25 slice 1) | Exempt, as the docs already say "always-on". `0043` is the one that changes a visible pace; flagged for the owner |
| **C. Port UI that has no original equivalent** | the Options entry (title row, field-menu row, battle Select), Return to title (P6.27), Quit game (P6.28), Help and Walkthrough screens, the version footer, the Options menu background | Exempt: they are the menu the rule lives in. The version *label on the title* is `show_version` ([section 1.2](#12-file-only-keys-no-row-the-gap-the-new-rule-closes)) |
| **D. Original bugs fixed, always on** | `0065` HUD shows the Digimon the partner fights as, `0066` Claw steal on the killing blow, the area-name banner fix, `0187` EU language select accepts Cross, `0129` title logo scale | These change what the player sees or gets. [Q3](#7-questions-for-the-owner) proposes **one** row "Original bug fixes" (default ON) in Gameplay; `0187` and `0129` stay exempt as UI polish of port screens |
| **E. Sub-features already behind a row** | the 16:9 patches `0047`-`0115`, `0175`, `0177`, `0179` (behind Widescreen); the menus at 1.5x (`0176`, behind Fast-forward); the instant-scroll hold (`0068`, Text speed); the jingle hold (`0194`) and tick gate (`0191`, Turbo music); `0140`-`0142` Continue row for the auto-save (Auto-save) | Already covered |

## 2. The reorganized tree

### 2.1 Principles

1. **First screen = settings the player changes while playing**; test aids and the two
   help screens sit below the fold.
2. **A group is one idea**: the picture, the sound, the rules of play, the menus, the
   buttons, the files. No group exists for a single row unless the row is a family
   that is growing (Saves).
3. **Registry indexes and cfg keys never change.** Only `dw3_optgroups[]` and a list-order
   table move ([section 5](#5-migration-and-implementation-notes)).
4. **Nothing new needs more than six plaques**, so no sub-screen except Cheats scrolls
   and no new scrolled reference is needed.

### 2.2 The tree

Order of the group list (nine plaques; six fit, so it scrolls by three as it does
today with eight):

| # | Group | EN | FR | DE | ES | IT | Rows (registry index) |
|---|---|---|---|---|---|---|---|
| 1 | Display | Display | Affichage | Anzeige | Pantalla | Schermo | Widescreen (0), Full-screen map (15), **Map scaler** (new), **Map mod pack** (new, `map_mod`), *Version label (new, pending Q2: `show_version`)* |
| 2 | Sound | Sound | Son | Ton | Sonido | Audio | Music volume (3), SFX volume (4), Turbo music (13) |
| 3 | Gameplay | Gameplay | Jouabilité | Gameplay | Jugabilidad | Gameplay | Fast-forward (1), Encounter rate (5), Text speed (6), Battle cursor (7), *Original bug fixes (new, pending Q3)* |
| 4 | **Menus** (new) | Menus | Menus | Menüs | Menús | Menu | **Equip from Items**, **Equip stays**, **Whole bag**, **Use item stays**, **Partner switch** (all new, the five `items_*` keys) |
| 5 | Controls | Controls | Commandes | Steuerung | Controles | Comandi | Button layout (9), Customize buttons (11), Reset to default (12), Switch glyphs (14, Switch), **Restart hotkey (2, moved from Gameplay)** |
| 6 | Saves | Saves | Sauvegardes | Speichern | Partidas | Salvataggi | Auto-save (8), **Region at start (10, moved from Gameplay; combined image only)**, *Shared saves (new, pending Q2: `save_store`)* |
| 7 | Cheats | Cheats | Triches | Cheats | Trucos | Trucchi | the ten Cheats rows (unchanged; title only, greyed in game) |
| 8 | Help | Help | Aide | Hilfe | Ayuda | Aiuto | a screen of its own (unchanged) |
| 9 | Walkthrough | Walkthrough | Pas à pas | Komplettlösung | Guía | Soluzione | a screen of its own (unchanged) |

The existing group names in FR/DE/ES/IT are the P9.9 lettering already in
`GROUP_TEXT`. The only new name is **Menus**; its drafts (`Menus` / `Menüs` / `Menús` /
`Menu`) and its message line are machine drafts like the other FR/DE/IT/ES texts and
need native review (the same P6.30 rule).

What moved and why:

| Move | From | To | Reason |
|---|---|---|---|
| Restart hotkey | Gameplay | Controls (last row) | It is a chord. P9.9 planned this "when P6.12 exists"; P6.12 exists. It goes last so Switch glyphs keeps its place |
| Region at start | Gameplay | Saves | It chooses which disc and which saves open; it pairs with Shared saves. Combined image only |
| Controls | 6th (after Cheats) | 5th (before Saves) | Settings first |
| Saves | 4th | 6th | After Controls |
| Cheats | 5th | 7th | A test aid belongs beside the help screens |
| Items keys | file-only | Menus group | The owner's order, and P6.34 phase 5's planned group |
| Gameplay | six rows (combined) | four (five with bug fixes) | No more scrolling inside Gameplay |

Rows with **no cfg key change at all** are every row that moves. The `[cheats listed,
master off, ...]` and `group open <name>` log lines keep their form; the new group logs
as `menus`.

### 2.3 Availability of each new or moved row

| Row | Where it works | Switch only? | Combined only? | Applies |
|---|---|---|---|---|
| Map scaler, Map mod pack | title, field menu, battle | no | no | at the next open of the full-screen map (the map cache keys on the picture source). The scaler key is read at each open; the L2 / R2 change in the view stays for the session (not written) |
| Version label | title, in game | no | no | at once on the title; the Options footer stays |
| Original bug fixes | all | no | no | from the next battle |
| Menus: the five rows | all (the hooks read the key at each press) | no | no | at once |
| Restart hotkey | all | no | no | at once |
| Region at start | all (not locked) | no | **yes** | next start, or the Yes in the P6.38 restart question |
| Shared saves | **title only** (greyed in game: the cards are opened before it is read) | no | no | next start |
| Switch glyphs | all | **yes** | no | at once |
| Widescreen, Customize buttons, Cheats | **title only** (as today) | no | no | unchanged |

The Help and Walkthrough entries are unchanged: Confirm opens the screen on its own full
canvas layer, from the title, the field menu and a battle. Help gains the **Options
explained** text for the new tree and the "Items and equipment" page already planned by
P6.34 phase 5. The Walkthrough keeps its place at the end of the list.

### 2.4 Rows per page against the limit (6 visible, 10 maximum)

| Page | Rows, PC US | Rows, Switch combined, all pending rows accepted | Fits in 6? |
|---|---|---|---|
| Group list | 9 | 9 | no, scrolls (offsets 0 to 3; today 0 to 2) |
| Display | 4 | 5 | yes |
| Sound | 3 | 3 | yes |
| Gameplay | 4 | 5 | yes |
| Menus | 5 | 5 | yes |
| Controls | 4 | 5 | yes |
| Saves | 1 | 3 | yes |
| Cheats | 10 | 10 | scrolls, as today |

Gameplay shrinks from five (six combined) to four, Controls grows from three (four on
the Switch) to four (five). No page needs a seventh plaque, so the plaque size, the
690 px message strip and the 960x720 canvas stay as they are, and the 16:9 and 4:3
presentations differ only in the flat side strips (which D4 replaces by the tiled
scrolling background).

## 3. ASCII mock of each page

Plaque = label left, value right; `>` is the focused plaque (the white pointer and lit
halo in the game). The footer hints are drawn from the button map in force
(Switch with D2: `A Change`, `B Back`).

### 3.1 Group list (scroll offset 0, then 3)

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

scrolled to the end (offset 3):

|  ^                                                           |
|    Saves                                                     |
|    Cheats                                                    |
|    Help                                                      |
|  > Walkthrough                                               |
```

(The list shows six of nine plaques, so the last three screens are one scroll apart: a
Down press from Saves reveals Cheats, Help, Walkthrough.)

### 3.2 Display

```
+--------------------------------------------------------------+
| DISPLAY                                                      |
+--------------------------------------------------------------+
|  > Widescreen                                         ON     |   locked in game
|    Full-screen map                                    OFF    |
|    Map scaler                                         FSR 1  |   D5: values FSR 1 / Nearest /
|    Map mod pack                                       ON     |       Sharp bilinear / Lanczos2 / Bilinear
|    Version label                                      ON     |   pending Q2
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
|    Original bug fixes                                 ON     |   pending Q3
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
|    Customize buttons                                         |   locked in game
|    Reset to default                                          |
|    Switch glyphs                                      ON     |   Switch only; D2 default ON
|    Restart hotkey                                     OFF    |   moved from Gameplay
+--------------------------------------------------------------+
| Nintendo: A confirms, B cancels. Applies now.                |
+--------------------------------------------------------------+
|                                  A Change      B Back        |
+--------------------------------------------------------------+
```

Customize buttons keeps its own sub-screen (`DW3_OPTGRP_CUSTOM`, ten rows, never on the
list), unchanged.

### 3.7 Saves

```
+--------------------------------------------------------------+
| SAVES                                                        |
+--------------------------------------------------------------+
|  > Auto-save                                          OFF    |
|    Region at start                                    Ask    |   combined image only
|    Shared saves                                       OFF    |   pending Q2; title only
+--------------------------------------------------------------+
| How and when the game saves.                                 |
+--------------------------------------------------------------+
|                                  A Change      B Back        |
+--------------------------------------------------------------+
```

### 3.8 Cheats, Help, Walkthrough

Cheats keeps its ten-row scrolling sub-screen exactly as built (P9.9 slice 2), greyed in
game. Help and Walkthrough have no rows; their plaques open the full-canvas screens. The
only change to these three is their place in the list (7, 8, 9).

## 4. Help and text consequences

- `port/help/text/<lang>.txt` page **options** ("Options explained"): rewrite the group
  paragraph and the per-group bullets for the new tree (Menus, the moved rows, the new
  Display rows). Five languages, the usual machine-draft tag for FR/DE/IT/ES.
- P6.34 phase 5's "Items and equipment" Help page ships with the Menus group.
- `GROUP_TEXT` gains `menus`; `ROW_TEXT` gains the new labels; the generator's width
  check runs on all of them (row label 290 px: "Equip stays on list", "Original bug
  fixes" and "Shared saves" are the widest candidates and fit by eye, the generator is
  the judge).
- `PLAYER_GUIDE.md`, `HOTKEYS.md` (the P9.9 paragraph lists the groups) and the OLED
  test plan rows that name a plaque by position need the new positions.

## 5. Migration and implementation notes

**Config files: nothing to migrate.** Every cfg key keeps its name, range and default.
The five `items_*` keys, `map_mod`, `show_version` and `save_store` already live in the
file; they are only *read by a row as well*. A file written by any earlier build loads as
before. New keys, if the owner accepts them: `map_scaler` (words `fsr1`, `nearest`,
`sharp`, `lanczos2`, `bilinear`; default `fsr1` per the owner's 7a), and the bug-fix
switch (name chosen in the build PR). A missing or invalid value falls back to the
default with the usual `is not valid` log line.

**Registry.** New rows are appended as indexes 16, 17, ... (`DW3_PORT_OPT_COUNT` grows;
the `_Static_assert` and the comment in `port_options_groups.h` are updated). Rows 0 to 15
keep their numbers. The `items_*` env pins (`DW3_ITEMS_*=0`) keep working: a pinned row
keeps the file's value when another row saves, as for the volumes.

**Groups table.** The list order is currently the enum order. To reorder without
renumbering the page codes' meaning, add a small `dw3_optgroup_order[]` and have
`dw3_optgroups_visible` walk it; add `DW3_OPTGRP_MENUS` after `DW3_OPTGRP_CUSTOM`
(`DW3_OPTGRP_COUNT` 9 to 10). The overlay page code (`dw3_optlist_page`) is internal to
the cache, not stored anywhere, so changing it only changes cache keys. `g_group_names`
gets `menus`. A group whose rows are all hidden in a given build is skipped by the
same visible-list pass.

**Replay references that change** (internal count to be measured by the build PR with
`tools/replay_tests.sh --changed`; the recipes in `tools/replay/gen_scenarios.py` walk the
list by plaque position, e.g. "Help is the sixth plaque", "the Walkthrough group is the
eighth", so the **recordings** change as well as the pictures):

| Family (US and EU, and the language variants) | Why |
|---|---|
| `options-persist`, `volume-persist`, `battle-cursor-persist`, `pacing-persist`, `options-sound`, `options-cheats-listed`, `options-noversion`, `options-region`, `options-region-relaunch-*` | the group list changes (nine plaques, new order); Gameplay loses Restart hotkey and Region; Saves gains Region |
| `field-options` (+ `-fr -de -it -es`, `-cheats`, `-nx`, `-release-nx`), `battle-options` (+ `-ref`, `-nx`) | the group list and the Display page |
| `cheats-screen`, `cheats-screen2` | the Cheats plaque moves from 5th to 7th (the path to it changes); the sub-screen itself is unchanged |
| `controls-layout`, `controls-custom`, `controls-custom-invalid` | Controls moves from 6th to 5th and gains Restart hotkey |
| `help-title`, `help-field`, `help-battle`, `walkthrough-screen-*` | the path through the list; the screens themselves are unchanged |
| `turbo-menu`, `restart-*`, `map-rebind-relaunch` and any recipe that turns a row on through the screen | navigation counts |
| new | `*-options-menus` (Menus page, a toggle of each row persists), Display with the new rows, Saves with Region and Shared saves |

The family also changes on D2 (the Controls values), D4 (the background) and D5 (the
Display values). To keep the churn down, **sequence the build PRs** as in
[Q5](#7-questions-for-the-owner): reorganization plus new rows first, then D2 with D5,
then D4 last. D4 should pin the scroll offset to 0 in the replays so its refs are stable.

## 6. Where D2 to D5 land

| Item | Lands in | What changes in this tree |
|---|---|---|
| **D2** `button_layout` default `nintendo`, `switch_glyphs` default ON | Controls (rows 1 and 4) | no placement change. Values on the page and the Help text change. **"Reset to default"** goes back to the new default (Nintendo), so its message and the Help text must say so, and `DW3_BUTTON_MAP=default` stays the PlayStation pin for tests. Because `po_save()` writes every key, any file written by a build since P9.8 already holds `button_layout=playstation` and `switch_glyphs=0` even if nobody chose them: the new default reaches only a missing key ([Q4](#7-questions-for-the-owner)). Every replay that assumes the PS layout with no key must pin `button_layout=playstation` |
| **D3** one global options file | no page change | `sdmc:/switch/dw3/port_options.cfg` holds every key, including `save_store` and `stage_sweep`. The rows read and write the global file. Region at start already lives there. The Shared saves row is no longer "set it in both regions". Migration log lines are D3's |
| **D4** A18 scrolling background | the look of all pages | the scroll is under the retained canvas, so no row or group is affected; the side strips of 16:9 are tiled by the quad. Plaques, strip and hints do not move. All Options refs change again, so D4 should follow this reorganization |
| **D5** FSR 1 map scaler, default always | Display, row 3 **Map scaler** | the row set by 7a. FSR 1 is the default, the map cache builds off the game thread as today; the GPU FSR form (P6.32 7.8 item 2) removes the first-open build |

## 7. Questions for the owner

Five questions. Each has my recommendation; the first one gates the build.

1. **The tree** ([section 2.2](#22-the-tree)): nine groups, order Display, Sound,
   Gameplay, **Menus**, Controls, Saves, Cheats, Help, Walkthrough; Restart hotkey moves to
   Controls; Region at start moves to Saves; the five `items_*` rows go in Menus, all ON.
   **Recommendation: approve.** If you would rather keep Region at start in Gameplay, say
   so; nothing else depends on it.
2. **The three keys I flagged**:
   - `show_version`: **recommend a row** ("Version label", Display, default ON). It changes
     the title screen, one plaque costs nothing and the rule is easiest to keep without
     exceptions. The alternative is to leave it file-only as a test label.
   - `save_store`: **recommend no row yet.** Keep it file-only while it is experimental;
     when you promote it, it becomes "Shared saves" in Saves (title only, takes effect on
     the next start).
   - `stage_sweep` / `stage_sweep_frames` (and the seven `map_view_*` bindings): **recommend
     they stay file-only**, documented as dev / advanced. They are not a change versus the
     original game.
3. **Always-on changes with no switch** ([section 1.3](#13-changes-versus-the-original-with-no-key-at-all-found-by-the-audit)):
   **recommend** class A (crash and 64-bit fixes), B (host plumbing) and C (the port's own
   Options / Quit / Return / Help screens) are exempt, and class D (patches `0065` HUD name
   and `0066` Claw steal, the original bugs fixed for display and loot) get **one** row
   "Original bug fixes", default ON, read from the next battle. It needs a small hook in
   those two patches, so it is a separate PR from the reorganization. Yes / no?
4. **D2 and the old files**: the new Nintendo and glyph defaults reach only a missing key,
   and files written since P9.8 contain `button_layout=playstation` / `switch_glyphs=0`
   without anyone choosing them (including yours). **Recommend** we do not guess: you switch
   the two rows once on your console (or delete the two lines), and the release notes say
   so. The alternative (treat an explicit `playstation` written before D2 as "not chosen")
   cannot be told from a deliberate choice.
5. **Order of the build PRs.** **Recommend**: (1) the reorganization and the new rows
   (Menus, Map mod pack, the Map scaler row), (2) D2 and D5 together (both change only
   the values on the Controls and Display pages), (3) D3 (no pictures change), (4) D4 (the
   background, last). The Options references are then regenerated three times instead of
   five. Say if D3 or D4 should jump the queue.
