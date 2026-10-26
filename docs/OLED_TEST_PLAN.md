# OLED test plan for v0.6.5-preview.36

One ordered checklist for the owner's next Switch OLED session, built from the
[ROADMAP "Pending OLED checks"](ROADMAP.md#pending-oled-checks), the per-item
docs and the [release README](../platforms/switch/release/README.txt). Nothing
in preview.31 to preview.36 has been checked on the OLED unless a source says
so. Each row links to the section that holds the full steps. If a doc and this
page disagree, the linked section wins.

Markers: **[GATE]** closes the Phase 5 gate. **[P5.1]** the 2 hour run.
**[LAUNCHER]** P6.24, not in any release, needs a manual NRO. Checks are for US
and EU unless marked; on EU, use at least two languages where the source says
so.

## Before you start

**NROs** (release assets, see the
[README bundle](../platforms/switch/release/README.txt)):

| File | Use |
|---|---|
| `dw3-switch.nro` | US ordinary build |
| `dw3-switch-eu.nro` | EU ordinary build (attached by hand) |
| `dw3-switch-nxlink.nro` | US nxlink logging build |
| EU nxlink build | **Not published in preview.35 or preview.36** (check the release assets). Build it: `./tools/build_switch_game.sh --nxlink --eu` ([P5.1](P5.1_MEMORY.md#owner-checklist-2-hour-oled--nxlink-session-p51-stays-unticked-until-done)) |
| `dw3-switch-elf.zip` | US unstripped ELFs, to symbolize a crash report |
| Launcher `dw3.nro` | **Not in a release.** CI artifact `dw3-switch-launcher-nro`, or `tools/build_switch_launcher.sh` ([SINGLE_BUILD](SINGLE_BUILD.md#oled-checklist-launcher-owner)) |

Use the ordinary NROs for feel checks. Use an nxlink build where a row says
"nxlink" (FPS and memory lines). The nxlink build also has the debug HUD
(Minus + L + R, [REPORTING_ISSUES](REPORTING_ISSUES.md)).

**SD layout** ([README](../platforms/switch/release/README.txt),
[CROSS_REGION_SAVES](CROSS_REGION_SAVES.md#as-built-slice-1)):

```
sdmc:/switch/dw3/us/            US runtime; memcard/ holds save0.bin, save1.bin
sdmc:/switch/dw3/eu/            EU runtime; memcard/ the same
sdmc:/switch/dw3/dw3.log        log
sdmc:/switch/dw3/crash_report.txt, crash_report_prev.txt
sdmc:/switch/dw3/port_options.cfg
sdmc:/switch/dw3/dw3-us.nro, dw3-eu.nro, dw3.nro   only for the launcher check
```

The US disc BIN/CUE sits beside `us/`, the EU one inside `eu/`. **Back up the
`us/` and `eu/` memcard folders before session A.** Never delete saves,
`*.pre-auto` or `.mcr.bak` files.

**Capture:**

- `dw3.log`: copy `sdmc:/switch/dw3/dw3.log` off the SD card **before
  relaunching**, and name it per session. Lines to grep are named in the rows.
- nxlink log: in hbmenu press **Y** (NetLoader) first, do not probe TCP 28280,
  then `nxlink -a <Switch-IP> -s dw3-switch-nxlink.nro 2>&1 | tee p-<session>-<date>.log`
  ([README](../platforms/switch/release/README.txt),
  [REPORTING_ISSUES](REPORTING_ISSUES.md)).
- After any crash keep `crash_report.txt` and `dw3.log` before relaunching (the
  next boot keeps one previous report).
- Always say the region, EU language, handheld or docked.

## Order at a glance

| Session | Build | Ticks or decides |
|---|---|---|
| A. Boot and title | US + EU ordinary | P6.28, P6.31 row, language fit |
| B. Field and 16:9 tour | US + EU ordinary | P6.1, P7.7 water, P9.7 field Options |
| C. Turbo speed | US + EU nxlink | P6.22 pass 2 |
| D. Battles | US + EU ordinary | P5.2, P6.18, P6.20 battle rows, P7.7, P9.7 battle Options |
| E. Saves | US + EU ordinary | P6.27, P6.9, P6.25, P6.31, P6.20 save mark, title textures after Return to title |
| F. 2 h memory run | US then EU nxlink | **[P5.1]**, **[GATE]**, P5.4 notice |
| G. Everything else | mixed | P5.4 forced crash, P10.2, P4.3, **[LAUNCHER]** |

**[GATE]** (New Game to the first boss, US and EU) is normal play, not a
single check. Do it in session F, where you play for hours anyway. Do the
session E cheat rows on a separate save, or after the gate play, because cheat
saves are marked ([PORT_OPTIONS](PORT_OPTIONS.md#the-cheat-used-flag-on-saves)).

---

## Session A: quick boot and title

Fresh start from hbmenu, US then EU. Main source:
[PORT_OPTIONS](PORT_OPTIONS.md#oled-checklist-quit-and-return-to-title-owner).

| # | Action | Expect | If it fails, send | Ticks |
|---|---|---|---|---|
| A1 | Launch the NRO (first run: wait out the extraction bar) | Title shows. `dw3.log` has `dw3: version=` for preview.36 and `[init_paths] cdfs root=` with the right region | `dw3.log`, photo | boot ([README](../platforms/switch/release/README.txt)) |
| A2 | EU: pick a language at the select; repeat later for a second language | Title labels in that language | photo, `dw3.log` | language fit |
| A3 | Title: Down through the choices | New Game, Options, Load Game, then **Quit game** last, a little lower; Down again does nothing | photo | P6.28 |
| A4 | Cross on Quit game | Window opens on **No**; Cross or Triangle closes it; hints show Switch letters (B, X) | photo | P6.28 |
| A5 | Options from the title: move over all rows, handheld and docked | **Nine** rows, the ninth is Auto-save, OFF; rows a little tighter but fit; no clipping top or bottom. EU: labels in the disc's language | photo per language | P6.31 row ([item 1](P6.31_AUTOSAVE.md#oled-checklist-owner-us-and-eu-builds-from-this-pr)), [P6.18 item 8](P6.18_PACING.md#oled-check-for-the-owner) |
| A6 | Set Encounter rate OFF and Text speed Instant, close from the HOME menu, relaunch, open Options | Values kept | `port_options.cfg`, photo | P6.18 ([item 7](P6.18_PACING.md#oled-check-for-the-owner)) |
| A7 | Title: Quit game, move to Yes, Cross | NRO ends, back in hbmenu. `dw3.log` ends with `dw3-port-quit: saves flushed; asking the host to quit`. `save0.bin`/`save1.bin` intact; next launch Continues from an earlier save | `dw3.log` tail, memcard folder listing | P6.28 |
| A8 | Relaunch, HOME, close the software | Exits as before; `dw3.log` has the `applet exit` line | `dw3.log` | P6.28 |

## Session B: field and 16:9 screens tour

US then EU, widescreen ON (default). Encounter rate stays OFF from A6 so walks
are not interrupted. Look for black strips, shifted parts, wrong textures.
Sources: the ROADMAP [P6.1 line](ROADMAP.md#pending-oled-checks) and
[P6.1_WIDESCREEN](P6.1_WIDESCREEN.md). Known and expected: the Lab's chart, entry and skill screens keep their
left panels at the 4:3 edge; Open Booster was not captured.

| # | Action | Expect | If it fails, send | Patch / item |
|---|---|---|---|---|
| B1 | Title menu, intro news caption bar, registration screen (New Game) | Fill the 16:9 screen; caption bar spans the width | photo | 0085-0087 / P6.1 |
| B2 | Field menu (Plus): the dissolve, then Items, Sort, Techniques, Status, Card folders | 16:9 | photo | 0074-0076 / P6.1 |
| B3 | Map screen | 16:9 (release notes: black side strips, #240) | photo | 0080-0082 / P6.1 |
| B4 | Save and load screens (save point, Continue) | 16:9 | photo | P6.1 |
| B5 | Area banner on entering an area, and a technique backdrop | Names whole, backdrop right. **EU still to do** (US was done) | photo, region | P6.1 ([open transitions](P6.1_WIDESCREEN.md#open-transitions-2026-10-23)) |
| B6 | Card folders: card album and deck editor (all panels, card details, list of cards owned) | 16:9 | photo per panel | 0100-0104 / P6.1 |
| B7 | Card shop, Buy One Card | 16:9 | photo | 0100-0104 / P6.1 |
| B8 | Gym; Digimon Lab (menu, party, digivolve) | 16:9; Lab chart, entry and skill screens keep left panels at 4:3 edge (known) | photo | 0100-0104 / P6.1 |
| B9 | Other shops and the Forest Inn | No shifted or cut panel; note any display-wide band layer that now draws in the side strips | photo | P6.1 (ROADMAP: "a look for other display-wide band layers") |
| B10 | **Door iris:** walk through several doors and area exits | View closes from the full 16:9 picture; grid cover covers the whole picture; no map shifted back after it closes | photo, `dw3.log` | 0115 / P6.1 |
| B11 | **Underground Path water:** leave the Underground Path to the Cargo Tower streets | Canal water texture right at once, not wrong until you scroll down | photo, `dw3.log` | 0111 / P7.7, P6.1 ([FIELD_TILE_SPRITES](FIELD_TILE_SPRITES.md)) |
| B12 | **Encounter spiral (patch 0175).** Set Encounter rate back to 1x, enter a random battle; try it at turbo off and at 2x, 3x and 4x (Options) | The swirl covers the whole 16:9 picture: the gutters do not go black on its first frame and the tiles slide off across the full width (a tile column hangs off each side of the spiral), in the same time as before. The picture then goes black as before, the battle appears | photo of the first spiral frames at each turbo speed, `dw3.log`, region | 0175 / P6.1 (PSY-Z `dw3-gutter-capture`) |
| B13 | In a battle, a technique's magic circle | Say whether a black hole shows in its middle (also on PC and Ryubing) | photo | P6.1 (ROADMAP open item) |
| B14 | nxlink build: a door and then a battle | Say if the transitions feel slow; send the log | nxlink log across both | P6.1 "Slow transitions" (a shorter banner hold is the owner's call, P6.18) |
| B15 | **Options from the field (#270, P9.7).** In the field press Plus; the list ends with OPTIONS (EU: OPTIONS, OPZIONI, OPTIONEN, OPCIONES) and then TITLE SCREEN. Cross on OPTIONS. Left x3 on Music volume. Triangle, then Triangle again, and walk | The Options window opens over the dimmed field; **Widescreen is grey** with the "Locked in game" line and Cross on it does nothing; the field music gets quieter at once and keeps playing; Triangle returns to the menu with the cursor still on OPTIONS, the second Triangle to the field and the character walks (nothing was pressed twice). The 16:9 dim covers the side strips and no part of the window stays after closing | photo, `dw3.log` (`dw3-port-options: open in game` and `close in game`) | P9.7 slice 1 ([checklist](PORT_OPTIONS.md#oled-checklist-options-in-game-owner)) |

## Session C: turbo speed

nxlink builds (US published, EU local). Set turbo to **4x** in Options, toggle
with R3. Sources:
[P6.22 pass 2](P6.22_FIELD_CPU.md#pass-2-2026-10-25-frames-turbo-does-not-show-are-not-drawn),
the README OLED checklist.

| # | Action | Expect | If it fails, send | Ticks |
|---|---|---|---|---|
| C1 | New Game opening at 4x. Compare with preview.34 at 4x if you remember | Much closer to the cap (4x cap is 240 fps US, 200 EU; the planning range is 160-240; an estimate, not measured). `frame dt us=` about 250000 per 60 pumps for 240 fps | nxlink log, FPS you saw | P6.22 |
| C2 | Forest Inn, top and bottom, at 4x | Same | log | P6.22 |
| C3 | Read the `defer dbg: queued= dropped= drawn=` lines | Dropped about 80 % of queued | the lines | P6.22 |
| C4 | Only if C1 looks wrong: A/B with `DW3_NX_DEFER=0` in `DW3_NX_TEST_ENV` (nxlink build); `DW3_NX_PRESENT_HASH=1` logs a hash per presented frame | Same picture both ways | both logs | P6.22 |
| C5 | Field-to-battle transition at **2x, 3x and 4x** | No hang, no crash, picture intact; battle itself is unchanged | crash report, `dw3.log`, photo | P6.22 |
| C6 | Watch the field picture at 4x | Nothing missing or flickering against 1x | photo | P6.22 |

## Session D: battles

Ordinary builds. Sources: [P5.2](P5.2_BATTLE_GPU.md),
[P6.18](P6.18_PACING.md#oled-check-for-the-owner),
[PORT_OPTIONS cheats](PORT_OPTIONS.md#oled-checklist-owner),
[P7.7](P7.7_BUG_FIXES.md#oled-checklist-owner).

| # | Action | Expect | If it fails, send | Ticks |
|---|---|---|---|---|
| D1 | First Kuwagamon fight with Kotemon, then later battles: partner portrait top right of the HUD (`gpu-tex`, the default) | No transparent holes; the stage does not show through | photo, render mode | P5.2 (#229) |
| D2 | Win a battle with **R3 turbo on**; watch the results screen | Runs at the battle's speed | `dw3.log`, photo | P6.18 (#223) |
| D3 | Text speed **Instant**: read the post-battle report and the area banner after it | Last lines (item, level up, skill) whole about 0.2 s apart; banner names whole at once | `dw3.log`: `slow Instant page with no scroll hold`, `text scroll` lines. A slow page with no `text scroll` line before it is a text that bypasses Instant | P6.18 |
| D4 | Instant at a door: the area banner | Names whole | `dw3.log` | P6.18 |
| D5 | Encounter rate OFF: walk about 400 steps; then a story or boss fight | No random battle; the story fight still starts. If time allows 0.5x, 1.5x, 2x | step counts | P6.18 |
| D6 | Cheats (Y on Options, master ON): **Battle now** Ready, walk | A battle starts within a few steps even with Encounter rate OFF | photo | P6.20 ([item 6](PORT_OPTIONS.md#oled-checklist-owner)) |
| D7 | Cheats **Instant win** ON, enter a random battle | Won on the first turn | photo, `dw3.log` | P6.20 (item 5) |
| D8 | Partner at Champion or higher (Digivolve in the field menu first): the HUD name. Try a long name (MetalGreymon, MetalGarurumon) | Champion's name, readable, does not touch the HP number | photo | P7.7 items 1-4 |
| D9 | Digivolve command in battle; a Nickname Shop name; a default-name Rookie | Name changes with the form; nickname kept; Rookie as before | photo | P7.7 items 2-4 |
| D10 | Growlmon or WarGrowlmon with a Picking or Snapping Claw (needs the item) | "Stole <item>!" before "Defeated!" at least sometimes (low chance); item in the bag | photo, `dw3.log` | P7.7 item 5 |
| D11 | After a won battle: is there field music? | Music plays. Known: silent map after a battle was reported on rc8 US, cause unknown | nxlink log | known issue ([README](../platforms/switch/release/README.txt)) |
| D12 | **Options in battle (P9.7 slice 2).** In a battle, when the command menu (Fight, Tech, ...) is waiting, press **Minus** (no L, R, ZL or ZR held). Move to Music volume, Left x3, Triangle. Do it again on a later turn. Also press Minus while an attack plays: nothing should happen | The Options window opens over the battle, which stays on screen frozen (3D models, HUD, partner portrait, no flicker or black frame); nothing moves for as long as you wait (try 30 s: no enemy attack, no gauge change); the battle music keeps playing and gets quieter; Widescreen is grey. Triangle: the command menu is back as it was, the cursor where it was, and the battle goes on and ends normally; no button press reached the menu | photo of the frozen picture, `dw3.log` (`dw3-battle-options: held` and `resume` lines show equal `events=`, `event_time=`, `hp=`), say docked or handheld | P9.7 slice 2 ([checklist](PORT_OPTIONS.md#oled-checklist-options-in-battle-owner)) |

## Session E: saves

Ordinary builds, memcard folders backed up. Sources:
[P6.31](P6.31_AUTOSAVE.md#oled-checklist-owner-us-and-eu-builds-from-this-pr),
[CROSS_REGION_SAVES](CROSS_REGION_SAVES.md#as-built-slice-1),
[PORT_OPTIONS](PORT_OPTIONS.md#oled-checklist-quit-and-return-to-title-owner),
[P6.9](P6.9_RESTART.md#validation).

### E1. Return to title and the restart chord

| # | Action | Expect | If it fails, send | Ticks |
|---|---|---|---|---|
| E1.1 | In the field press Plus: the last row (TITLE SCREEN; EU: RETOUR TITRE and so on). With Card folders open the panel has seven rows and the last is inside it | Row present with Restart hotkey OFF and ON; text fits in the other EU languages (plaque and both windows) | photo per language | P6.27 |
| E1.2 | Cross on it | Window on **No**; No leaves the menu as before | photo | P6.27 |
| E1.3 | Open again, Yes | Back to the title (US) or the language select (EU). `dw3.log` has `dw3-port-restart: requested from the field menu` then `saves flushed`. A save written before still Continues | `dw3.log` | P6.27 |
| E1.4 | Options: Restart hotkey ON. Hold **L + R + Plus + Minus** in the field, inside a battle, after a won battle, and right after saving at a save point (release between tries) | Each reaches the title (EU: the language select) with no overlay left over. Continue loads the save written before the restart | `dw3.log`, photo of any leftover overlay | **P6.9** (tick only after this; PC replays already pass) |
| E1.5 | Note: the chord contains Minus + L + R, so it can also toggle the nxlink HUD | Expected | | [README](../platforms/switch/release/README.txt) |
| E1.6 | After the Yes in E1.3, at the title (US) or the language select and title (EU): look at the logo, the menu rows and the backdrop for a few seconds, open Options and Quit game's window, then start Continue | Textures rebuild with no glitch (#268 releases the title overlay's textures outside the title): no blank, garbled or half-drawn logo, menu or Options window; the rows are where they were on the first boot | photo, `dw3.log` (`dw3-title-overlay:` lines) | #268 / P9.7 |

### E2. Auto-save

| # | Action | Expect | If it fails, send | Ticks |
|---|---|---|---|---|
| E2.1 | Options: Auto-save to **Each area**. Continue a save, walk through a door, wait | `dw3.log`: `target is card ... (loaded)`, then `area change ...`, then `wrote card ...` | `dw3.log` (`dw3-autosave` lines) | P6.31 |
| E2.2 | Close, relaunch, Continue the same slot | You arrive in the new area, not where you saved; the list entry shows that area | photo | P6.31 |
| E2.3 | Look at the card folder | `save0.bin.pre-auto` (and `save1.bin.pre-auto` for card 2) exists and holds your earlier version | folder listing | P6.31 |
| E2.4 | New Game with the option on | Nothing written until you save once by hand | `dw3.log` | P6.31 |
| E2.5 | Option OFF, play | No `dw3-autosave` line in `dw3.log` | `dw3.log` | P6.31 |
| E2.6 | Feel: does the write stall a frame at an area change? | A feel report is enough (no timing line yet). Not covered by PC replays: a real SD write failure, HOME during the write | note | P6.31 ([item 6](P6.31_AUTOSAVE.md#oled-checklist-owner-us-and-eu-builds-from-this-pr)) |

### E3. Cross-region import

Both builds on one SD card.

| # | Action | Expect | If it fails, send | Ticks |
|---|---|---|---|---|
| E3.1 | A US save in `us/memcard/`, none in `eu/memcard/` (move the EU ones aside). Start EU | `dw3.log` has `memcard cross-import slot=0 from=us to=eu`; `save0.import` appears in the EU folder; US `save0.bin` unchanged (size and time stamp) | `dw3.log`, the `.import` file | P6.25 |
| E3.2 | Title, Continue, slot 1 | US save listed (name, area, money, play time); it loads at the saved place. Save over it, leave with HOME, start again, Continue | photo, `dw3.log` | P6.25 |
| E3.3 | Reverse: an EU save, no US save, start US | Same | same | P6.25 |
| E3.4 | A save in both folders | Nothing imported | `dw3.log` | P6.25 |
| E3.5 | If you have one, an EU extra-chapter save going to US | No save listed (or error 8 if the card also has an importable one); no crash | `dw3.log` | P6.25 |
| E3.6 | Start the same build again | The import does not run a second time | `dw3.log` | P6.25 ([README](../platforms/switch/release/README.txt)) |

### E4. Cheats screen and the cheat mark

| # | Action | Expect | If it fails, send | Ticks |
|---|---|---|---|---|
| E4.1 | Options: Y with the master OFF | Nothing visible, no hint | photo | P6.20 ([page 1](PORT_OPTIONS.md#oled-checklist-owner) item 1) |
| E4.2 | Y opens the Cheats screen: title and rows in the game language (EU: all five); X returns. Master ON: rows 2-4 active, hint shows on Options. Close, relaunch | Cheats and Instant win keep state; Fill bag and Battle now are OFF again | photo | page 1 items 2-3 |
| E4.3 | Fill bag Ready, then field menu Items | 99 of each, no key items | photo | page 1 item 4 |
| E4.4 | Down from "Cheat saves" to page 2 ("CHEATS 2/2"); with master OFF the rows are grey and inert | As written | photo | P6.20 ([page 2](PORT_OPTIONS.md#oled-checklist-page-2-owner) item 1) |
| E4.5 | **Money**: arm +10,000 (Cross twice steps +10,000, +100,000), enter a field, open the field menu | Amount added; MAX is 9,999,999; the row is OFF the next time you open the screen | photo | page 2 item 2 |
| E4.6 | **Warp** to Digimon Lab, load a game | At the first idle moment the screen fades and you arrive in the Lab. Save, reload: still in the Lab. Try Central Park or the Bulk Swamp and walk: random battles work | photo, `dw3.log` | page 2 item 3 |
| E4.7 | **Give item** (Left/Right, L/R by ten, a count), Ready, load | Item listed with the count, not a key item; a second arm adds, up to 99 | photo | page 2 item 4 |
| E4.8 | Save after a cheat, relaunch | "Cheat saves" shows 1; `dw3.log` has `dw3-cheats: save... was written with cheats` (page 2: `dw3-cheats: ... changed the game`). Save a clean game over that slot, relaunch: None | `dw3.log` | page 1 item 7, page 2 item 5 |
| E4.9 | Note: Warp can leave a story-flagged map looking unfinished | Known | | [README](../platforms/switch/release/README.txt) |

## Session F: the 2 hour memory run (nxlink)

**[P5.1]** US first, then EU, each as its own session, on the nxlink build (EU
built locally). Also the slot for the **[GATE]** play and the P5.4 notice. Full
source:
[P5.1_MEMORY](P5.1_MEMORY.md#owner-checklist-2-hour-oled--nxlink-session-p51-stays-unticked-until-done).

Setup: HOME menu off, auto-sleep off (except the planned sleep and wake),
handheld on the charger, `nxlink -s` captured to a file, keep
`sdmc:/switch/dw3/dw3.log`.

| # | Action | Expect | If it fails, send | Ticks |
|---|---|---|---|---|
| F1 | Play 2 h of wall time without restarting the NRO, at 1x (or note how long turbo was on). Include 8 or more random battles (win, take the report), 10 or more area changes, field menu with Items, Status, Options a few times, Cards/Gym/Lab if reachable, three manual saves with Auto-save ON for the area changes, one Continue from the title, one sleep and wake. Cheats Battle now and Warp may help (they mark the save) | No crash | crash report, logs | **[P5.1]** |
| F2 | Every 15 min note the HUD line or take it from the log: `heap=`/`hw=`, `fcache=`, `libc=`/`hw=` | | | **[P5.1]** |
| F3 | Pass test | After the first 30 min `heap hw` and `libc hw` stop climbing (a rise under 80 K over the last hour is noise; a new area or battle type may raise them once). No monotonic climb of `libc` used in the last hour. No crash report. No `dw3-file-cache: all 64 slots taken` followed by `FATAL`. Keep the `dw3-file-cache: ... evicted` lines (#235) | full nxlink log | **[P5.1]** |
| F4 | Report | `mem dbg:` lines at minutes 30, 60, 90, 120 (hw and used), count of `dw3-file-cache` lines, total battles and area changes, EU language, whether the console slept, log paths, what was not covered | | **[P5.1]**; then tick P5.1 |
| F5 | Reference for F3 | rc7 sample after 30 min: 144435K newlib hw, 1685K game heap hw, 1259K file cache | | context |
| F6 | **[GATE]** New Game to the first boss, US and EU (in this run or a separate one) | Reaches the first boss with no crash or blocker | crash report, `dw3.log`, where it stopped | Phase 5 gate (owner, 2026-10-24: not met; [Phase 5](ROADMAP.md#phase-5--playable-on-switch)) |
| F7 | If the boot shows "The last run crashed." | Panel stays up to 60 s, A dismisses it; an untouched EU panel logs `crash notice: timed out after 60000 ms` | photo, `dw3.log` | P5.4 (#231) |

## Session G: anything else

| # | Action | Expect | If it fails, send | Ticks |
|---|---|---|---|---|
| G1 | **P5.4 PR 2 forced crash.** Two special `--force-crash` nxlink builds (US forced assert right after boot, handheld; EU forced abort on the title after about 25 s, docked), each followed by the normal NRO. Follow [P5.4_CRASH](P5.4_CRASH.md#pr-2-owner-oled-check) steps 1-5 as written | Panel readable; US: A ends it early; EU: full 60 s; report symbolizes with the copied ELF; step 5 boots with no panel | the files listed in the source (step 2, 4, 5 logs, Atmosphere log, photo) | P5.4 (tick after) |
| G2 | **P10.2:** HOME during a save write. Use the nxlink build (US, then EU), handheld. Back up the memcard folder first. Steps: **(a)** Continue a save, open the field menu, Save, pick the slot, confirm; the instant the "saving" bar shows, press HOME and choose Close Software (X on the HOME menu tile). Repeat **8 times**, pressing HOME at different moments (before the bar, on the bar, right after it). **(b)** Options: Auto-save **Each area**; walk through a door and press HOME at once, close the game; 3 times. **(c)** After every close: relaunch from hbmenu, Continue the slot you saved to. **(d)** Look at `sdmc:/switch/dw3/<us or eu>/memcard/` | Every relaunch: Continue loads (the new save or the one before, never a corrupt one). The folder has `save0.bin` of exactly 32768 bytes, **no** `save0.bin.bad`, and no lasting `save0.bin.tmp` / `save0.name.tmp` (one left by the close is removed by the next boot). `dw3.log` shows one of: `applet exit: flat saves flushed` (normal), `applet exit: a save is being written; waiting for it` followed by `flat saves flushed` (the new deferral worked), `dw3: memcard: recovered ... from an interrupted write` or `removed stale ...` (a close landed inside the write; the file is still valid). Not acceptable: `bad flat`, `quarantined`, an empty slot where a save was | `dw3.log` copied before each relaunch (name them g2-1 ... g2-8), the nxlink log, folder listing with sizes after each try, which moment HOME was pressed | P10.2 (stays unticked until you report; [audit](P5.3_HOME_QUIT.md#home-during-a-save-write-the-audit-p102-2026-10-25)) |
| G3 | **P10.2:** HOME (not the power button) pause mid-game, nxlink run | `applet focus: ... state=3` line, audio cut, game continues, no time jump. Known: these lines did not appear on rc7 | nxlink log | P10.2 |
| G4 | **P4.3:** nxlink session of 10 min or more: title 1 min, opening movie 2 min, field walk 4 min, battle 3 min, one save | `nx_audio dbg:` lines show `underruns=0`, `seq` advancing, no `audrvUpdate failed`. Listen: movie XA mix, field BGM, battle SFX, no gap | log | P4.3 ([recipe](P4.3_AUDIO_HW.md#hardware-session-recipe)) |
| G5 | P7.7 HUD name and Claw steal | Done in D8-D10 | | P7.7 |
| G6 | Rumble (optional, still unverified on hardware) | Report the feel | note | [P10.4](P10.4_RUMBLE.md) |

### G7. [LAUNCHER] P6.24 slice 2 (not in the release)

Needs a manual NRO: `dw3.nro` from the CI artifact `dw3-switch-launcher-nro`
(or `tools/build_switch_launcher.sh`) in `sdmc:/switch/dw3/`, with the US and EU
builds beside it as `dw3-us.nro` and `dw3-eu.nro`. Start `dw3` from hbmenu in
applet mode. Source:
[SINGLE_BUILD](SINGLE_BUILD.md#oled-checklist-launcher-owner). Check the
picker text is readable at arm's length, handheld.

| # | Action | Expect | If it fails, send |
|---|---|---|---|
| G7.1 | Both `us/` and `eu/` present | Picker shows `> US <`; after 3 s the US game starts. Next start the picker begins on the last region. Right, then A starts EU | photo |
| G7.2 | Y until "always" is ON, A. Next start | No picker. Hold **Minus** while pressing A in hbmenu: the picker comes back. D-pad or stick cancels the countdown; `+` quits to hbmenu | photo |
| G7.3 | One region only (move `eu/` away, then `us/`) | No picker, starts at once (about 0.2 s pause is expected) | photo |
| G7.4 | No region | Setup message names both folders; `+` returns to hbmenu | photo |
| G7.5 | Remove `dw3-eu.nro`, choose EU | Message names `sdmc:/switch/dw3/dw3-eu.nro`; `+` quits | photo |
| G7.6 | Start `dw3.nro` as a title takeover (hold R on a game) | "cannot start another program" message, no crash | photo |
| G7.7 | Saves and options of both regions after going through the launcher; handheld Joy-Cons and a Pro Controller | Unchanged (it writes only `port_options.cfg`); same results on both controllers | folder listing |

## What to send back

Per session: the letter, region, language, handheld or docked, `dw3.log` copied
before relaunch, the nxlink log if used, photos of anything wrong, and the
crash report after a crash (format in [REPORTING_ISSUES](REPORTING_ISSUES.md)).
When a row passes, tick the item in the [ROADMAP](ROADMAP.md) table and remove
it from Pending OLED checks.
