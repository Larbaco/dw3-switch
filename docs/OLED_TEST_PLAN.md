# OLED test plan for v0.6.5-preview.36

One ordered checklist for the owner's next Switch OLED session, built from the
[ROADMAP "Pending OLED checks"](ROADMAP.md#pending-oled-checks), the per-item
docs and the [release README](../platforms/switch/release/README.txt). Nothing
has been checked on the OLED unless a row says so. The owner's reports for
preview.41 to preview.45 (2026-10-27/28) are in the rows marked **PASSED** or
**PARTIAL**. Each row links to the section that holds the full steps. If a doc and this
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

- `dw3.log`: every launch rotates the log (`dw3.log` newest, then `dw3.1.log`
  ... `dw3.4.log`; 5 runs kept). Send the whole `sdmc:/switch/dw3/*.log` set.
  The first line of each run (`dw3-run:`) has its date. Lines to grep are named
  in the rows.
- nxlink log: in hbmenu press **Y** (NetLoader) first, do not probe TCP 28280,
  then `nxlink -a <Switch-IP> -s dw3-switch-nxlink.nro 2>&1 | tee p-<session>-<date>.log`
  ([README](../platforms/switch/release/README.txt),
  [REPORTING_ISSUES](REPORTING_ISSUES.md)).
- After any crash keep `crash_report.txt` and `dw3.log` before relaunching (the
  next boot keeps one previous report).
- Always say the region, EU language, handheld or docked.

**Owner results (2026-10-27/28).** **PASSED** means the owner's report covers
what the row's Expect column says the owner can see. **PARTIAL** means part of
the row is still unreported. The reports did not include `dw3.log`, so rows
whose Expect names log lines are passed on the owner's observation; the log
lines are still wanted as proof.

## Order at a glance

| Session | Build | Ticks or decides |
|---|---|---|
| A. Boot and title | US + EU ordinary | P6.28, P6.31 row, language fit, P6.30 help (H1 to H8), P9.8 Button layout (L1 to L8) |
| B. Field and 16:9 tour | US + EU ordinary | P6.1, P7.7 water, P9.7 field Options |
| C. Turbo speed | US + EU nxlink | P6.22 pass 2 |
| D. Battles | US + EU ordinary | P5.2, P6.18, P6.20 battle rows, P7.7, P9.7 battle Options |
| E. Saves | US + EU ordinary | P6.27, P6.9, P6.25, P6.31, P6.20 save mark, title textures after Return to title |
| F. 2 h memory run | US then EU nxlink | **[P5.1]**, **[GATE]**, P5.4 notice |
| G. Everything else | mixed | P5.4 forced crash, P10.2, P4.3, **P5.5 stage sweep (G10)**, **[LAUNCHER]**, **[COMBINED]** (G9) |

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
| A3 | Title: Down through the choices | New Game, Options, Load Game, then **Quit game** last, a little lower; Down again does nothing | photo | P6.28. **PARTIAL**: owner, 2026-10-27, preview.41: plaque spacing (#287) only |
| A4 | Cross on Quit game | Window opens on **No**; Cross or Triangle closes it; hints show Switch letters (B, X) | photo | P6.28 |
| A5 | Options from the title: the list of groups, then open each group and move over its rows, handheld and docked | The list shows **Display, Sound, Gameplay, Saves, Help** as title-style plaques (no Cheats yet; Help opens the technical help, see H1); each group shows its rows (1, 2, 5 and 1 of them: all nine settings are there, Auto-save in Saves, OFF); no clipping top or bottom; the message strip explains the focused item. EU: every label in the disc's language | photo per language | P9.9 slice 1 (A9 to A12), P6.31 row ([item 1](P6.31_AUTOSAVE.md#oled-checklist-owner-us-and-eu-builds-from-this-pr)), [P6.18 item 8](P6.18_PACING.md#oled-check-for-the-owner) **PASSED**: owner log, 2026-10-28, preview.47 US (group open lines, title and in game) |
| A6 | Set Encounter rate OFF and Text speed Instant, close from the HOME menu, relaunch, open Options | Values kept | `port_options.cfg`, photo | P6.18 ([item 7](P6.18_PACING.md#oled-check-for-the-owner)) |
| A7 | Title: Quit game, move to Yes, Cross (title path = `dw3-port-quit:` lines, HOME path = `applet exit`) | NRO ends, back in hbmenu. `dw3.log` ends with `dw3-port-quit: saves flushed; asking the host to quit`, `dw3_game_main returned 0`, `audio stopped`, `gpu closed; leaving main` (P38: preview.38 ended in a Horizon error here). `save0.bin`/`save1.bin` intact; next launch Continues from an earlier save | `dw3.log` tail, memcard folder listing | P6.28. **PARTIAL**: owner, 2026-10-27, preview.41: Quit through the Sphaira forwarder works and returns to hbmenu; saves intact and the log tail not reported. The full-app Sphaira launch shows errors; the forwarder is fine **PASSED**: owner log, 2026-10-28, preview.47 US (dw3-port-quit saves flushed, dw3_game_main returned 0 (quit path: title Quit game), leaving main) |
| A8 | Relaunch, HOME, close the software | Exits as before; `dw3.log` has the `applet exit` line | `dw3.log` | P6.28 |
| A9 | Options from the title, B on Sound, X (back), B on Gameplay, X, X | B opens a group, X returns to the list with the cursor on the group you left, X on the list returns to the title menu. The choice sound plays on open, the cancel sound on back. Feel: the plaques look like the title menu's; the cursor moves without a stutter | `dw3.log` lines in the proof table below, photo | P9.9 **PASSED**: owner log, 2026-10-28, preview.47 US (by log: group open/close lines) |
| A10 | Open each group for the first time (Display, Sound, Gameplay, Saves, then Cheats if on), at the title and in game, then again | **No stutter on any group open.** The first open of a group is made over the prewarmed shared layer: `group-over-base`, **compose under 10000 us** (it was `rebuild`, 48000 to 81000 us, in preview.47); the second visit is `cached` (about 3000 us) | `dw3.log`: `dw3-port-options: prewarm sub-screen title ready in N ms (worker)` and `prewarm sub-screen in-game ready in N ms (worker)` a few seconds after boot, `dw3-port-options: options layer slot N allocated (K of 6, X MB held)` (memory: 15.8 MB held at most, 5.3 MB more than before), `dw3-port-options: group open <name> (N row(s), title\|in game)` then `dw3-port-options: group layer page N built over the shared layer in X ms` and `dw3-title-overlay: options update #N group-over-base rects=1 ... compose=N us` (a `rebuild` line here is the old stall; one is allowed if the open comes before the prewarm line) | P9.9 **PASSED** (by log: owner log, 2026-10-28, preview.47 US) for the old behaviour; slice 3 re-check pending |
| A11 | Options in game (field menu OPTIONS), Display group | Widescreen greyed with the lock note; the near-opaque dim keeps the field menu behind from being legible; Cheats group (if the master switch is on) greyed; X, X back to the field menu | photo, `dw3.log` | P9.9, P9.7 **PASSED**: owner log, 2026-10-28, preview.47 US (by log: group open gameplay (5 rows, in game)) |
| A12 | Options in battle (Minus at the command menu), Sound, change Music, X, X | The battle stays held; closes after two X; the battle resumes as before | `dw3.log` | P9.9, P9.7 slice 2 **PASSED**: owner log, 2026-10-28, preview.47 US (by log: group open/close lines) |
| A13 | **Cheats sub-screen (P9.9 slice 2).** With the master ON (Y on Options, or the Cheats group), open Cheats: Down through all ten rows, Cross on the first row (master OFF then ON), Cross on Fill bag, Left/Right on Money and Warp, Left/Right/L/R on Item id | The Cheats screen looks like the other groups (plaques, header CHEATS, hints Change / Back), not the old white window; the list scrolls with amber arrows; rows 2 to 10 are grey while the master is OFF; Item id and count are numbers; X returns to the group list on Cheats. EU: all five languages. **A value or master change must not stutter** | `dw3.log`: `dw3-port-options: group open cheats (group)` (or `(Square / Y shortcut)`), `dw3-cheats: open (master ...)`, `dw3-title-overlay: options update #N dirty rects=N bytes=N compose=N us` for the master, a scroll step and a value change (**`dirty`, never `rebuild`, compose well under 10000 us**; only the first open of the sub-screen says `rebuild` or `cached`), `dw3-cheats: master on\|off`, `dw3-cheats: close (back to Options)` | P9.9 slice 2 ([checklist](PORT_OPTIONS.md#cheats-sub-screen-p99-slice-2)) |

### A14. Technical help (P6.30, Session A or B; the H items)

Use an ordinary US and EU NRO of the preview that carries P6.30. The help is a group of
Options, so the same path opens it at the title, in the field and in a battle. The English
text is a **draft** (the page title says `(draft)`); report wording problems as notes, they
are not failures.

| # | Action | Expect | If it fails, send | Ticks |
|---|---|---|---|---|
| H1 | Title, Options, Down to **Help**, B. Move over the page list, B on **Controls**, X, X | Help is the last plaque of the Options list; the page list has the title menu's look; the message strip explains the focused page; Open and Back hints with the B and X caps. **No stutter opening it** (compose well under 16 ms; the Options open itself is unchanged: `group-over-base`, compose under 10000 us) | `dw3.log`, photo | P6.30. Proof: `dw3-port-options: group open help (title)`, `dw3-help: open (from title, region US\|EU, platform switch, language en, 10 pages)`, `dw3-help: first frame composed N ms after open`, `dw3-help: panel composed in N us (list, compose 1)` |
| H2 | Field menu: OPTIONS row, B, Help, B on a page; then a battle: Minus at the command menu, Help | The game waits behind it, the music keeps playing; Back out and the menu or the battle resumes as after any Options visit | `dw3.log`, photo | P6.30. Proof: `dw3-help: open (from field, ...)` and `open (from battle, ...)`, `dw3-help: closed after N ms (pages opened N)`, then the usual `close in game (back to ...)` |
| H3 | Open **Controls** and **FAQ**: Down / Up one line at a time, hold Down, Left / Right, L and R. Read a page at arm's length, handheld then docked | Text is readable on the OLED; amber arrows show when there is more above or below and disappear at the ends; hold-Down scrolls evenly (no stutter on the repeat); Left / Right move a screenful; L / R change the page (Switch **L** and **R**); X goes back to the page list, X again to Options | `dw3.log`, photo, say which scroll felt slow | P6.30. Proof: `dw3-help: page controls (from list, N lines, panel 13 lines, unverified blocks N)`, `end reached (line N of N)`, `closed after N ms (read to line N of N)`, `layout controls N lines in N us`, and `dw3-help: panel composed in N us` only when over 20 ms (**a line here means a scroll step took over 20 ms: send it**) |
| H4 | **Controls** and **Port hotkeys** pages | The names in brackets are the console's: Cross [B], Circle [A], Triangle [X], Square [Y], Start [+], Select [-], L1 [L], R1 [R], L2 [ZL], R2 [ZR], L3 [L3], R3 [R3], the restart chord `[L] + [R] + [+] + [-]` | photo | P6.30. Proof: `dw3-pages: token {btn:r3} -> "[R3]"` and the other `token {...} -> "..."` lines (one per token per page opened) |
| H5 | **Setup** page, US then EU | Only the lines of this disc: US shows SLUS-01436 and `dw3-switch.nro`, EU shows SLES-03936 and `dw3-switch-eu.nro`; the paths are `sdmc:/switch/dw3/...` | photo | P6.30. Proof: `dw3-pages: text help en loaded (N keys, N qualified, N filtered out for US\|EU/switch, N bytes)` |
| H6 | **Saves**, **Map view and mods** and **FAQ** (last answers) | Lines not yet checked on the OLED end with `(unverified)` (the per-profile saves, the cross-region import, the map pack) | photo | P6.30. Proof: `dw3-help: page saves (... unverified blocks N)` with N above 0, `dw3-help: text en status "draft, ...", N unverified block(s)` |
| H7 | Open every page of the list once | No `[?...]` text anywhere, no cut-off line, no overlap with the arrows or the strip | photo of any problem | P6.30. Proof: **no** `dw3-pages: token ... unresolved` line, no `dw3-help: page ... cannot be laid out` line |
| H8 | EU disc: set each of the five languages (language select) and open Help | The group name and the screen words (Open, Back) in the language's lettering where authored; the pages are **English** for FR, DE, IT and ES (the translations are still to come) | `dw3.log` | P6.30 launch gap. Proof: `dw3-help: open (from title, region EU, platform switch, language fr\|de\|es\|it, 10 pages)` and `dw3-pages: text fallback help * (fr): the language file has no keys yet (stub), English shown` |

### A15. Button layout (P9.8, Session A or B; the L items)

Use a preview that carries P9.8, US and EU ordinary NROs. The layout lives in
`port_options.cfg` of **each region**, so set it once per disc. Every row names
the `dw3.log` line that proves it (AGENTS.md rule 12); the owner only reports
how it feels. Known gap: the Options hint row still shows the letters **B** and
**X** in both layouts (the glyphs are phase 3, P10.6); with Nintendo the real
confirm is A and the back button is B. Report it as a note, not a failure.

| # | Action | Expect | If it fails, send | Ticks |
|---|---|---|---|---|
| L1 | Fresh launch (no `button_layout` key), US then EU. Title: Plus, Down, **B** on Options, **B** on Controls, then **X**, **X** | The default is the game as before: B confirms, X backs out, A is the secondary button. Options list has **Controls** between Saves and Help; its one row says **PlayStation**. Proof: `dw3-controls: map loaded layout=playstation source=default`, `dw3-controls: map cross<-B circle<-A triangle<-X square<-Y l1<-L r1<-R l2<-ZL r2<-ZR l3<-StickL r3<-StickR`, `dw3-controls: hotkeys fast-forward=R3 (Right stick click) restart=L1+R1+Start+Select (L+R+Plus+Minus)`, then `dw3-controls: first press source=B -> logical cross` | `dw3.log` (all `dw3-controls:` lines), photo | P9.8 |
| L2 | Options, Controls, **Button layout**: press **B** (it says **Nintendo**), **X** twice back to the title, then **A** on New Game's row | The row changes at once, with the menu sound, and **the screen stays on the Controls group** (the B you pressed does not also back out; the held button is ignored until you let go). The map changes before the next press: A confirms, B backs out. Proof: `dw3-controls: layout changed playstation -> nintendo (screen)`, the new map line `cross<-A circle<-X triangle<-B square<-Y ...`, `dw3-options: saved ... port_options.cfg`, then `dw3-controls: first press source=A -> logical cross` | `dw3.log`, photo | P9.8 |
| L3 | Nintendo, in the field and in a battle: open the field menu with Plus, **A** on a row, **B** to leave it; in a shop try the secondary action; in a battle open Tech with **A** and leave with **B**; Minus opens Options in a battle | A confirms, B cancels a menu, the shop's secondary action (the PS1 Circle: trade, card picks) is on **X**; Plus and Minus unchanged. Proof: the map line shows `triangle<-B circle<-X`; no `dw3-controls: map invalid` | `dw3.log`, say which screen felt wrong | P9.8 |
| L4 | Leave with the title's Quit game (or HOME), relaunch, US and EU (set Nintendo on each disc) | The layout is kept: the Controls row says Nintendo and A confirms from the first screen, including the **EU language select** (A picks the language, as does Plus; patch 0187). Proof: `dw3-controls: map loaded layout=nintendo source=cfg` on both discs, `dw3-lang-select: language N chosen by Cross (confirm)` on EU | `dw3.log` of both runs (`dw3.1.log` for the first), `port_options.cfg` (`button_layout=nintendo`) | P9.8 |
| L5 | Nintendo: the full-screen map (Plus, Map, **Y**, then **B**) and the restart chord (L + R + Plus + Minus, with Restart hotkey on) and fast-forward (right stick click) | The map view closes with **B** (the cancel button) and Y inside it does nothing; the chord and fast-forward work exactly as before. Proof: `dw3-map: close requested by the cancel button (PS-side bit 14, ...)`, the restart lines of E1 and the `dw3-turbo:` toggle line of C7 | `dw3.log` | P9.8, P6.32 |
| L6 | Nintendo: Help (Options, Help), the **Controls** and **Port hotkeys** pages | Cross [A], Circle [X], Triangle [B], Square [Y], Start [+], and the restart chord `[L] + [R] + [+] + [-]`. With PlayStation they read [B], [A], [X], [Y]. Proof: `dw3-pages: token {btn:cross} -> "[A]"` (Nintendo) and `"[B]"` (PlayStation) | photo, `dw3.log` | P9.8, P6.30 |
| L7 | nxlink build, both layouts: **Minus + L + R** (debug HUD) and **Minus + ZL + ZR** (renderer mode) | The chords are unchanged by the layout (they read the physical buttons). The region picker (A, Y, Plus), the launcher and the crash notice also keep their physical buttons | nxlink log | P9.8 (CONTROLS_BUNDLE section 1.4) |
| L8 | Developer: nxlink build with `DW3_BUTTON_MAP=default` in `DW3_NX_TEST_ENV`, cfg set to Nintendo | The run plays with the PlayStation buttons and the file is not changed by an unrelated option change. Proof: `dw3-controls: map loaded layout=playstation source=env`; `port_options.cfg` still `button_layout=nintendo` | `dw3.log`, `port_options.cfg` | P9.8 |

## Session B: field and 16:9 screens tour

US then EU, widescreen ON (default). Encounter rate stays OFF from A6 so walks
are not interrupted. Look for black strips, shifted parts, wrong textures.
Sources: the ROADMAP [P6.1 line](ROADMAP.md#pending-oled-checks) and
[P6.1_WIDESCREEN](P6.1_WIDESCREEN.md). Known and expected: the Lab's chart, entry and skill screens keep their
left panels at the 4:3 edge; Open Booster was not captured.

| # | Action | Expect | If it fails, send | Patch / item |
|---|---|---|---|---|
| B1 | Title menu, intro news caption bar, registration screen (New Game) | Fill the 16:9 screen; caption bar spans the width | photo | 0085-0087 / P6.1. **PARTIAL**: owner, 2026-10-27, preview.41: the intro caption bar at 16:9; title menu and registration not reported |
| B2 | Field menu (Plus): the dissolve, then Items, Sort, Techniques, Status, Card folders | 16:9 | photo | 0074-0076 / P6.1 |
| B3 | Map screen | 16:9 (release notes: black side strips, #240) | photo | 0080-0082 / P6.1 |
| B4 | Save and load screens (save point, Continue) | 16:9 | photo | P6.1 |
| B5 | Area banner on entering an area, and a technique backdrop | Names whole, backdrop right. **EU still to do** (US was done) | photo, region | P6.1 ([open transitions](P6.1_WIDESCREEN.md#open-transitions-2026-10-23)) |
| B6 | Card folders: card album and deck editor (all panels, card details, list of cards owned) | 16:9 | photo per panel | 0100-0104 / P6.1 |
| B7 | Card shop, Buy One Card | 16:9 | photo | 0100-0104 / P6.1 |
| B8 | Gym; Digimon Lab (menu, party, digivolve) | 16:9; Lab chart, entry and skill screens keep left panels at 4:3 edge (known) | photo | 0100-0104 / P6.1 |
| B9 | Other shops and the Forest Inn | No shifted or cut panel; note any display-wide band layer that now draws in the side strips | photo | P6.1 (ROADMAP: "a look for other display-wide band layers") |
| B10 | **Door iris:** walk through several doors and area exits | View closes from the full 16:9 picture; grid cover covers the whole picture; no map shifted back after it closes | photo, `dw3.log` | 0115 / P6.1 |
| B11 | **Underground Path water:** leave the Underground Path to the Cargo Tower streets | Canal water texture right at once, not wrong until you scroll down | photo, `dw3.log` | 0111 / P7.7, P6.1 ([FIELD_TILE_SPRITES](FIELD_TILE_SPRITES.md)). **PASSED**: owner OLED, 2026-10-27, preview.41 (water texture fixed; P7.7) |
| B12 | **Encounter spiral (patch 0175).** Set Encounter rate back to 1x, enter a random battle; try it at turbo off and at 2x, 3x and 4x (Options) | The swirl covers the whole 16:9 picture: the gutters do not go black on its first frame and the tiles slide off across the full width (a tile column hangs off each side of the spiral), in the same time as before. The picture then goes black as before, the battle appears | photo of the first spiral frames at each turbo speed, `dw3.log`, region | 0175 / P6.1 (PSY-Z `dw3-gutter-capture`) |
| B13 | In a battle, a technique's magic circle | Say whether a black hole shows in its middle (also on PC and Ryubing) | photo | P6.1 (ROADMAP open item) |
| B14 | nxlink build: a door and then a battle | Say if the transitions feel slow; send the log | nxlink log across both | P6.1 "Slow transitions" (a shorter banner hold is the owner's call, P6.18) |
| B15 | **Options from the field (#270, P9.7).** In the field press Plus; the list ends with OPTIONS (EU: OPTIONS, OPZIONI, OPTIONEN, OPCIONES) and then TITLE SCREEN. Cross on OPTIONS. Left x3 on Music volume. Triangle, then Triangle again, and walk | The Options window opens over the dimmed field; **Widescreen is grey** with the "Locked in game" line and Cross on it does nothing; the field music gets quieter at once and keeps playing; Triangle returns to the menu with the cursor still on OPTIONS, the second Triangle to the field and the character walks (nothing was pressed twice). The 16:9 dim covers the side strips and no part of the window stays after closing | photo, `dw3.log` (`dw3-port-options: prewarm title ready in N ms (worker)` and `prewarm in-game ready in N ms (worker)` a few seconds after boot (N around 150 on the Switch); no frame drops in the field while they run (turbo 4x included); then on every open `dw3-port-options: open in game`, `dw3-title-overlay: options update #1 cached rects=1 ... compose=` **a few ms, not 166000 us**, `first frame shown N ms after the open request` **under 20 ms**, `longest frame gap N ms` **under 20 ms** (it was one frame of ~170 ms in preview.41), `close in game`, `open for N ms`). Open it a second and third time and once from the title: every open says `cached`, never `rebuild` | P9.7 slice 1 ([checklist](PORT_OPTIONS.md#oled-checklist-options-in-game-owner)) |
| B15b | **Options opens in game without a hitch (preview.45 feedback).** EU or US: in the field, wait about 3 seconds (the title textures are released), Plus, Cross on OPTIONS. Do it three times, with 3 s of field between. Then open Options at the title once | The first frame of the in-game open is as fast as the title's: `dw3.log` has `dw3-title-overlay: 960x720 RGBA options texture (GLES), create N us` (no `960x5040` / `title atlas` line when you open from the field) and `dw3-port-options: first frame shown N ms after the open request` under 20 ms, `longest frame gap N ms (frame 1)` under 20 ms (was 92.6 / 95.6 ms in preview.45: the open created the 960x5040 title atlas). At the title the atlas line `960x4320 RGBA title atlas, 6 states (GLES), create N us` appears once per visit | `dw3.log`, say whether you still feel the open | P9.9 / #303 follow-up (2026-10-28). **PASSED**: owner, 2026-10-28, preview.47 US ("Não sinto o engasgo no menu mais"). Log: first frame shown 27-28 ms in game (was 92.6 ms on preview.45), title 24.7 ms, options texture create 1.4-2.1 ms, no 960x5040 atlas on the in-game open. The 20 ms target was not met numerically; the owner accepts the feel |
| B16 | **Full-screen map view (P6.32 PoC, #284).** Widescreen ON. Field menu (Plus), Map, then **Y** (Square). Move the cursor with the D-pad or left stick (it starts on your current area; the view follows it), zoom with the **right stick** (up in, down out, smooth) or **L / R** (steps), **ZL / ZR** to cycle the scaler (its name shows on screen), Circle to hide the text, the **cancel button** (Switch **X**, PS1 Triangle; **not** B or Y: Y only opens the view) to return. Try it at fit, 1.0, 1.25, 1.5 and 2.0 while moving the cursor slowly; on a location the game's **name box** ("Bulk Swamp / Bulk Bridge") appears; Circle hides it. The scaler starts on **Nearest**; press **ZR** to Lanczos2 / FSR 1 and keep playing the cursor while it builds (the old picture stays, the HUD says BUILDING). Turbo: press **R3** in the view | Map fills the 16:9 picture **and shows the areas the game has revealed** (the lit towns and the visited-area icons, the same as the game's own Map screen; only the dark fogged map would be the bug of preview.43); the **balloon** (your partner's face bubble) sits over the current area and the **cursor** (arrow, a hexagon on a location) moves with the pad, both scaling and following the map at every zoom, the animation running; the name box matches the game's Map screen at the same save and goes away with Circle; Lanczos2 / FSR 1 do not freeze the buttons while they build; the right stick zooms smoothly with no jump at the steps; no pan past the edge, no seams or flicker; the scaler name changes; R3 does not speed the game up there; X (the cancel button, PS1 Triangle) returns to the Map screen as it was, and X again closes the Map as before; Y inside the view does nothing (it only opens the view; owner decision 2026-10-28). Say which scaler you prefer and how long a first-time Lanczos2 or FSR 1 took | photo per scaler, `dw3.log` (`dw3-map:` lines: `reveal layer: N pieces from the game (T towns + A visited areas drawn), D drawn, hash H` (D must be above 0 and equal N), `open half=`, sheet/image file sizes, `compose from vram rc=`, `scaler X built in N ms` / `cache hit|miss|stale` (a Lanczos2 / FSR 1 file is only reused for the same reveal hash H), `zoom source=stick (axis N) zoom Z` and `zoom source=button step`, `texture WxH queued`, `sprite bank: 12 of 12 frames built` (EU; 9 of 12 on US), `cursor on location id=I (name id=I ...)`, `cursor left location`, `cursor at rest at X,Y, location id=I`, `balloon at sprite X,Y ... frame 0x35|0x3b`, `name box shown for location id=I ... at the top|bottom`, `name box hidden (...)`, `scaler X requested (worker thread, input stays live)` then `built in N ms`, `switch sprite bank uploaded`, `switch texture upload ok|FAILED (GL error)`, `switch map not drawn: no texture yet`, `zoom step .. draw rect`, `close requested by the cancel button (PS-side bit 14, ...)` then `close after N frames (zoom: S stick frames, B button presses ...)`; a Y press inside the view only logs `Square ignored in the view`), say whether `sdmc:/switch/dw3/cache/` filled | P6.32 ([section 8](P6.32_MAP_ZOOM.md#8-oled-checklist-for-the-owner-poc)), patch 0146. **PARTIAL**: owner, 2026-10-28, preview.45: the map shows and Nearest is preferred. Balloon, cursor and name box pending preview.47. **Owner OLED 2026-10-28 (preview.47/48): PASSED** (icons, balloon, cursor) **except** the name box on a location jumped between positions while zooming with the right stick; L/R was fine. Cause: the port rewrote the game's `hoverY` every frame, so the above/below choice followed the cursor's screen row, which the zoom moves; fixed by latching it when the cursor gets on the location (the game's own rule). **Retest:** put the cursor on a location near the middle of the picture and zoom with the stick in and out: the box stays put; the log has no `name box placement changed` line Owner preview.48 (2026-10-28): the name-box jitter with the right stick **was not reproduced** on preview.48 (fix #326 lands in preview.49 anyway; retest it there). **New (P6.32 section 11):** the balloon and the cursor keep one on-screen size at every zoom (the size at zoom 1.0), their positions follow the map; the lit towns and visited areas scale with the map. Retest at 1.0 and at the top zoom (3.0): the cursor and balloon are the same size as at 1.0, the balloon tip stays on its area, no giants. Log: `dw3-map: sprites at fixed size (scale 1/Z, ...)` once per open |
| B17 | **Map upscale mod (P8.5 first mod, P6.32).** Needs the mod folder on the SD card: build it on the PC with `tools/mods/extract_map_assets.py` (see [MOD_MAP_UPSCALE section 7](MOD_MAP_UPSCALE.md#7-as-built-phase-2)) and copy its `switch` folder to the SD root so that `sdmc:/switch/dw3/mods/map-upscale-animevideov3/mod.json` exists. Widescreen ON. Plus, Map, **Y**. Look at fit, 1.0 and 2.0 zoom; move the cursor to a town and a location; press **ZR** once and back. Then close the game, rename the folder to `mods-off` (or put `map_mod=0` in `port_options.cfg`), start again and open the same view | With the pack: the map is the sharper upscaled picture (clean lettering, no halo round the lit towns and the area icons), same position and size as the original, the lit towns and visited icons are upscaled too; the HUD says `MOD 4X`; the balloon, cursor and name box are the game's and sit where they did; **ZR** only changes the filter (sharp / smooth), no freeze; opening costs no visible pause. Without the pack the original map returns, nothing else changed. Say whether the look is better than Nearest, and whether a 1568 x 1408 texture costs any frame (turbo off) | `dw3.log`: `dw3-map: mods folder sdmc:/switch/dw3/mods` at boot; on each open `dw3-map: mod: pack "map-upscale-animevideov3" 1.0.0 loaded: map_us_h0@4x.png 1568x1408, reveal N pieces from the pack, 0 at 1x nearest, 0 skipped, M ms` (N above 0; `at 1x nearest` and `skipped` should be 0 with a full pack; M is the build time) then `dw3-map: texture 1568x1408 queued for the GPU`, `switch texture upload ok`. Without it: `dw3-map: mod: original map (no packs in the mods folder; folder sdmc:/switch/dw3/mods)` or `(map_mod=0 in port_options.cfg)`. A bad file says `pack "x": map_us_h0@4x.png is WxH, expected 1568x1408` | P8.5 first mod, P6.32 ([MOD_MAP_UPSCALE](MOD_MAP_UPSCALE.md)) **Not tested** (owner, preview.48, 2026-10-28): there was no mods folder on the SD card |
| B18 | **Item shop in 16:9 (preview.47 feedback: "the item shop is still 4:3").** Widescreen ON. In a town open an item shop (for example Asuka Armory). Look at: the shop (money bar, BUY / SELL panel, Back plate), BUY (the list, the details with Circle for the partners' stats, quantity, total with Buy / No), then SELL (the four kinds, the list, quantity). Leave with the cancel button | The panels reach the 16:9 edges (the money bar, BUY / SELL panel and Back plate hang from the right edge, the list frame and description band run edge to edge, the stats page's left plate from the left edge); nothing is cut at the old 4:3 edge; text and cursors sit on their panels; the three partner boxes stay in the middle | photo of the shop, of the BUY list and of the quantity screen | patch 0179 / P6.1. Replay `us/eu-item-shop`. **Not confirmed** |
| B19 | **Equipment in the Items menu (preview.47 feedback: "selecting a weapon does nothing").** Field menu, Items, open the second option (the weapons list), press Cross on a weapon. Then Status > Equipment on the same Digimon and equip it there | In the Items menu Cross on a weapon, armor or accessory does nothing (silent), as in the original game; Status > Equipment equips it. No change from before | `dw3.log` | patch 0180 / P6.33 ([finding](P6.33_ITEMS_MENU.md#6-selecting-equipment-in-the-items-menu-finding-2026-10-28)); decision on "equip from the Items menu" is the P6.34 proposal. **Not confirmed** |
| B20 | **Map view closes with the cancel button (owner decision 2026-10-28).** As B16: open the view with **Y** (Square), then press **Y** again, then press **X** | Y inside the view does nothing (the view stays); X closes it and shows the game's Map screen as it was; X again leaves the Map | `dw3.log` | patch 0146 / P6.32. **Not confirmed** |

## Session C: turbo speed

nxlink builds (US published, EU local). Set turbo to **4x** in Options, toggle
with R3. Sources:
[P6.22 pass 2](P6.22_FIELD_CPU.md#pass-2-2026-10-25-frames-turbo-does-not-show-are-not-drawn),
the README OLED checklist.

| # | Action | Expect | If it fails, send | Ticks |
|---|---|---|---|---|
| C1 | New Game opening at 4x. Compare with preview.34 at 4x if you remember | Much closer to the cap (4x cap is 240 fps US, 200 EU; the planning range is 160-240; an estimate, not measured). `frame dt us=` about 250000 per 60 pumps for 240 fps. **Proof line:** `pace dbg: speed=4x game_fps=` about 240 (200 EU) with `presented=15 skipped=45`; `swap_us` in the hundreds of thousands = the display is the limit, `busy_pct` near 100 with `game_fps` lower = CPU bound and `sync_us` / `present_us` / `other_us` say where (the first line after boot also counts the frames before the first window and reads high) | nxlink log, FPS you saw | P6.22. **PARTIAL**: owner, 2026-10-27, preview.41: turbo works in the intro; frame lines not sent |
| C2 | Forest Inn, top and bottom, at 4x | Same. `pace dbg:` `sync_us` is larger at the top (more of the map on screen) than at the bottom; both near the cap | log | P6.22 |
| C3 | Read the `defer dbg: queued= dropped= drawn=` lines | Dropped about 75-80 % of queued (Ryubing, US opening: 75 %) | the lines | P6.22 |
| C4 | Only if C1 looks wrong: A/B with `DW3_NX_DEFER=0` in `DW3_NX_TEST_ENV` (nxlink build); `DW3_NX_PRESENT_HASH=1` logs a hash per presented frame | Same picture both ways | both logs | P6.22 |
| C10 | A text-heavy scene at 4x (the opening's dialogue, a long NPC talk), then the same with the mixer back on the game thread's core: `DW3_NX_AUDIO_CORE=-2` in `DW3_NX_TEST_ENV` on an nxlink build | `dw3.log` starts with `nx_audio: mixer thread on core 1 (prio 0x20)` (core -2 in the A/B run). `pace dbg:` `mix_us` (the mixer thread's CPU in the window) is about the same either way, 4-8 % of the window; with the mixer on core 1 `game_fps` is equal or higher and `busy_pct` equal or lower than with -2; `dw3-audio: health` `underruns` does not grow; no crackle | both logs, FPS you saw | P6.22 |
| C5 | Field-to-battle transition at **2x, 3x and 4x** | No hang, no crash, picture intact; battle itself is unchanged | crash report, `dw3.log`, photo | P6.22 |
| C6 | Watch the field picture at 4x | Nothing missing or flickering against 1x | photo | P6.22 |
| C7 | **Menus at 1.5x (patch 0176, P6.4).** Turbo on at 4x. In the field press Plus (field menu), hold Down on the list, open OPTIONS and hold Down / Left on a row, close it, open TITLE SCREEN (the Yes / No question) and cancel | The menu and its cursor are visibly quicker than at 1x but clearly slower than field walking at 4x; a held direction is easy to stop on a row. `dw3.log`: `dw3-turbo: menu 1.5x (speed 4x) in the field menus, mode file 0x..` on opening and `menu off (frames=N) after the ...` on leaving, no `speed 1x` in between | `dw3.log`, region | P6.4. **PASSED**: owner, 2026-10-27, preview.41 (feel: 1.5x in the menus works; log lines not sent) **PASSED**: owner log, 2026-10-28, preview.47 US (98 menu 1.5x lines with matching menu off) |
| C8 | Same turbo, battle: at the command menu move the cursor, open Tech and move in the list, pick one | Cursor and list at 1.5x; the technique animation, messages and enemy turn at the full 4x | `dw3.log` (`menu 1.5x` / `menu off` pairs around the turn) | P6.4 |
| C9 | Turbo on: open the field menu Map, Items and a shop, then walk | These stay at 1x (no `menu 1.5x` line from them); walking is back at the full speed as soon as the field menu closes | `dw3.log` | P6.4 |

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
| D13 | **Audio after many techs (#188).** EU build. Use 10 or more techs in a row across battles (the same or different battles, the field in between). Then play on: a battle, the field, the menus | Music and sound effects keep playing in the battle, after it and in the field; no silence that does not come back | `dw3.log`: `dw3-battle: technique picked id=.. N techs this session`, `dw3-audio: health` every 30 s (output peak, voices, music requests), **`dw3-audio: underruns N in last 60 s ... voices in use min/avg/max .../24 ... key-on K ok F failed`** every minute and the `dw3-audio: exit:` line (F above 0 or `WARNING SsUtKeyOn found no free voice` = the #188 voice leak; voices in use min that keeps rising with nothing playing = leak), `dw3-audio: WARNING output silent for N s while music is expected`; say when it died, region, language, handheld or docked | #188 **NOT tested yet** (owner, preview.48 EU log, 2026-10-28: only one Techniques command in that session). The audio health lines looked healthy: 0 underruns, 0 key-on failed, voices in use max 23 of 24. Still open: 10 or more techs in a row |
| D14 | **Audio underruns (P4.3).** Any build, handheld, nxlink or the log rotation. Play 10 minutes: field walking, 3 or more battles, a menu visit, one movie. Quit with the title Quit game or HOME then Close Software | No crackle or dropout you can hear | `dw3.log`: about ten `dw3-audio: underruns N in last 60 s (total T), queue depth min/avg/max a/b/c of 4` lines, then `dw3-audio: exit: underruns ... (total T)`. Goal: total `T` = 0 over the 10 minutes and queue depth min 3 or more; a higher `T` with the time of the window says where. Say handheld or docked | P4.3 |

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
| E1.3 | Open again, Yes | The field fades to black (about a third of a second, still 16:9), stays black while the title loads, then the title (US) or the language select (EU) fades in. **No 4:3 picture is ever visible**: no squeezed field frame, no small picture with black sides before the title. `dw3.log` has `dw3-port-restart: requested from the field menu`, `fading to black before the reset (field menu, 20 frames)`, `screen is black after 20 frames; resetting now`, `saves flushed`, `new screen ready after N black frames; fading in (15 frames)`, `fade finished, N frames from the request`. A save written before still Continues | `dw3.log`, a phone video if you can (the old flash was about 6 frames) | P6.27 / return fade. **PARTIAL**: owner, 2026-10-27, preview.41: Return to title, and Continue after it, work; fade and log lines not reported **PASSED**: owner log, 2026-10-28, preview.47 US (fading to black 20 frames, fade finished 42 frames from the request (field menu); owner saw no 4:3 flash) |
| E1.4 | Options: Restart hotkey ON. Hold **L + R + Plus + Minus** in the field, inside a battle, after a won battle, and right after saving at a save point (release between tries) | Each fades to black, then reaches the title (EU: the language select) with no overlay left over and no 4:3 flash (`dw3-port-restart: fading to black before the reset (restart chord, 20 frames)`). Continue loads the save written before the restart | `dw3.log`, photo of any leftover overlay | **P6.9** (tick only after this; PC replays already pass) |
| E1.5 | Note: the chord contains Minus + L + R, so it can also toggle the nxlink HUD | Expected | | [README](../platforms/switch/release/README.txt) |
| E1.6 | After the Yes in E1.3, at the title (US) or the language select and title (EU): look at the logo, the menu rows and the backdrop for a few seconds, open Options and Quit game's window, then start Continue | Textures rebuild with no glitch (#268 releases the title overlay's textures outside the title): no blank, garbled or half-drawn logo, menu or Options window; the rows are where they were on the first boot | photo, `dw3.log` (`dw3-title-overlay:` lines; `dw3-title-logo: drawn at canvas 120,47-840,373` identical on every boot; `dw3-widescreen: gutter N px per side (was M; ...) mode file 0x..`: on US the `gutter 0 px ... 4:3 picture` line falls inside the black part of the fade, between `screen is black after N frames` and `new screen ready`; on EU there is no such line before the language select any more (it is 16:9, patch 0177, E1.6b); if you still see a 4:3 frame, send the log) | #268 / P9.7 |
| E1.6b | **EU language select in 16:9 (patch 0177).** Widescreen ON, first boot or after Return to title / the restart chord | The language select fills the screen: the tile pattern continues into both side bands, the header band and the logo band run to the edges, the flags' panel (black) runs to the right edge, cursor and flags unchanged. The pattern keeps scrolling with no seam at the old 4:3 edges. Pick a language: the fade-out reaches the edges and no 4:3 flash appears before the title | `dw3.log`: `dw3-widescreen: gutter 53 px per side (was -1 or 0; 16:9) mode file 0x165`; the title follows with `gutter 0 px ... mode file 0x1d0` then `gutter 53 px` a few frames later (the title's own start, unchanged) | photo, P6.1 (2026-10-28). **PASSED by log** (owner, preview.48, EU via the combined `.nro`, 2026-10-28, OLED): `dw3-widescreen: gutter 53 px per side (was -1; 16:9) mode file 0x165 frame 1`; the owner saw no 4:3 frame. Also new (patch 0187): Cross (the confirm button, Switch A) picks the language as well as Start; see E1.6c |
| E1.6c | **EU language select confirms with the confirm button (patch 0187).** EU, first boot or after Return to title. Move to a language with Up / Down, press the confirm button (Switch **B**, PS1 Cross), then on another boot press **Plus** (Start) | Both choose the language: the cursor flashes, the panels close and the title follows, in the chosen language. Plus keeps working | `dw3.log`: `dw3-lang-select: language N chosen by Cross (confirm)` for B and `... chosen by Start` for Plus (N is the game's `LANGUAGE` value, 1 to 6; check the title text matches the language you picked) | owner preview.48 feedback (only Start worked), P6.1 / P9.8; the button follows the future P6.12 remap ([CONTROLS_BUNDLE](CONTROLS_BUNDLE.md)). Pending OLED |

### E2. Auto-save

| # | Action | Expect | If it fails, send | Ticks |
|---|---|---|---|---|
| E2.0 | No `autosave.bin` in the card folder: look at the title (US and each EU language) | Four plaques: New Game (EU: Start / Commencer / Start / Inizia / Comenzar), Options, **Load Game** (EU: Load Game / Charger / Laden / Carica / Cargar; never "Continue"), Quit game. Cross on the third opens the slot list. All four are in the same lettering (no plaque with a white bubble outline or extra shading) | photo per language, `dw3.log`: `dw3-title-menu: no autosave.bin: plaques New Game, Options, Load Game (slot list), Quit game` | P6.31, title lettering (2026-10-28) |
| E2.1 | Options: Auto-save to **Each area**. Load a save (progress 4 or more), walk through a door, wait | `dw3.log`: `area change ...`, then `wrote autosave.bin`; `autosave.bin` appears; `save0.bin` / `save1.bin` unchanged, no `.pre-auto` | `dw3.log` (`dw3-autosave` lines), folder listing | P6.31. **PARTIAL**: owner, 2026-10-27, preview.41: writes on an area change and on HOME close; save files and log lines not reported **PASSED**: owner log, 2026-10-28, preview.47 US (40 wrote autosave.bin lines) |
| E2.2 | Close, relaunch, look at the title | Five evenly spaced plaques: New Game, Options, **Continue**, **Load Game**, Quit game (EU: in the chosen language), all in the same lettering | photo per language, `dw3.log`: `dw3-title-menu: plaques New Game, Options, Continue (autosave.bin), Load Game (slot list), Quit game` and `dw3-autosave: autosave.bin found: Continue loads it` | P6.31. **PARTIAL**: owner, 2026-10-27, preview.41: plaque spacing (#287) confirmed; the auto-save Continue not reported **PASSED**: owner log, 2026-10-28, preview.47 US (autosave.bin found: Continue; plaques New Game, Options, Continue (autosave.bin), Load Game (slot list), Quit game) |
| E2.3 | Continue | You arrive in the new area at once, no card screen; `dw3.log` `Continue loads autosave.bin` | photo, `dw3.log` | P6.31 |
| E2.4 | Load Game | The slot list shows your own saves, unchanged (the old area) | photo | P6.31 |
| E2.4b | New Game with the option on, reach progress 4, change area | `autosave.bin` replaced by the new game | `dw3.log` | P6.31 |
| E2.5 | Option OFF with the file still there: Continue, play, change area | Continue still loads it; no `wrote autosave.bin` | `dw3.log` | P6.31 |
| E2.6 | Feel: does the write stall a frame at an area change? | `wrote autosave.bin ... in N ms (SD write M ms ...); option Auto-save=1` is the stall, measured. Not covered by PC replays: a real SD write failure, HOME during the write | note | P6.31 ([item 6](P6.31_AUTOSAVE.md#oled-checklist-owner-us-and-eu-builds-from-this-pr)) |

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

### E5. Per-user saves (P6.26)

Needs a console with at least two user profiles. Read `dw3.log` after each row:
every row names the `user:` lines that prove it. Profile folders are under
`sdmc:/switch/dw3/saves/`; the folder name is 32 hex digits; the log shows the
short id (8 hex digits, a hash of the uid, not a prefix). Two profiles must
show different short ids; if they match, write it down, that is a bug.

| # | Action | Expect | If it fails, send | Ticks |
|---|---|---|---|---|
| E5.1 | Create a forwarder in sphaira with the **profile selector** option, pick profile A, launch | Starts without another selector. Log: `user: applet type=` (note which), `user: accountGetPreselectedUser rc=0x0 valid=1`, `user: profile=<8hex> reason=preselected`, `user: save dir=.../saves/<uid>/<region>/memcard` | `dw3.log` | P6.26 |
| E5.2 | First launch of A with old saves in `<region>/memcard/` | Continue and Load show the old saves. Log: `migration: new region folder, copied N legacy file(s)`. Folder `saves/<uid>/` has `profile.txt`, `<region>/imported.txt`, `<region>/memcard/save0.bin`. The old `<region>/memcard/` files are still there, same size and time | `dw3.log`, folder listing | P6.26 |
| E5.3 | As A: save in slot 2 (or play to an auto-save), quit. Launch a forwarder for profile B | B has its own folder (different `profile=<8hex>`); B's saves are the copy of the old ones, **not** A's new save. Save something different as B | `dw3.log` | P6.26. **PARTIAL**: owner, 2026-10-27/28, preview.43/44: each profile has its own Continue; folder and log lines not reported |
| E5.4 | Back as A: Continue / Load | A sees A's save and auto-save, not B's. B likewise | `dw3.log` | P6.26. **PARTIAL**: owner, preview.43/44: own Continue per profile; the save check and log lines not reported |
| E5.5 | Forwarder with the selector: at the system selector press **B/cancel** | The game does not start, back to the launcher, no black screen hang. Log: `user: result=cancelled` (or `error`: send the `rc=` and `module/desc` of `pselShowUserSelector`), `no user chosen: leaving without starting the game`. No new folder under `saves/` | `dw3.log`, folder listing | P6.26. **PASSED**: owner, 2026-10-27/28, preview.43/44: cancelling the profile selector returns to the Switch HOME (log lines not sent) |
| E5.6 | Start from hbmenu in **applet mode** (hold nothing, launch from the Album) with several users | Either the system selector appears (log: `showing the system profile selector`, `reason=selector`) or it does not launch (log `pselShowUserSelector rc=` non-zero, `reason=error`, the game does not start). **Both outcomes are information: write which one happened** | `dw3.log` | P6.26 (open question) |
| E5.7 | Hbmenu in title-override mode (hold R on a game), several users | Note whether `accountGetPreselectedUser` is valid (`reason=preselected`) or the selector appears | `dw3.log` | P6.26 |
| E5.8 | A console with one profile | No extra screen. Log: `reason=single-user`, `users=1` | `dw3.log` | P6.26 |
| E5.9 | Cause a crash if safe (nxlink force-crash build) or read an old report | `crash_report.txt` has `profile: <8hex>` after `disc:`, no nickname | crash report | P6.26 |

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
| G2 | **P10.2:** HOME during a save write. Use the nxlink build (US, then EU), handheld. Back up the memcard folder first. Steps: **(a)** Continue a save, open the field menu, Save, pick the slot, confirm; the instant the "saving" bar shows, press HOME and choose Close Software (X on the HOME menu tile). Repeat **8 times**, pressing HOME at different moments (before the bar, on the bar, right after it). **(b)** Options: Auto-save **Each area**; walk through a door and press HOME at once, close the game; 3 times. **(c)** After every close: relaunch from hbmenu, Continue the slot you saved to. **(d)** Look at `sdmc:/switch/dw3/<us or eu>/memcard/` | Every relaunch: Continue loads (the new save or the one before, never a corrupt one). The folder has `save0.bin` of exactly 32768 bytes, **no** `save0.bin.bad`, and no lasting `save0.bin.tmp` / `save0.name.tmp` (one left by the close is removed by the next boot). `dw3.log` shows `dw3-save: write save0.bin committed (32768 bytes ...)` for each save that finished, and after a close that landed inside a write on the next boot `dw3-save: write save0.bin interrupted, previous file kept` (or `... interrupted after the old file was removed; the complete new file was recovered`), plus one of: `applet exit: flat saves flushed` (normal), `applet exit: a save is being written; waiting for it` followed by `flat saves flushed` (the new deferral worked), `dw3: memcard: recovered ... from an interrupted write` or `removed stale ...` (a close landed inside the write; the file is still valid). Not acceptable: `bad flat`, `quarantined`, an empty slot where a save was | the `dw3*.log` set (each try is one run), the nxlink log, folder listing with sizes after each try, which moment HOME was pressed | P10.2 (stays unticked until you report; [audit](P5.3_HOME_QUIT.md#home-during-a-save-write-the-audit-p102-2026-10-25)) **Not tested** (owner, preview.48, 2026-10-28): no `dw3-save` line in that log |
| G3 | **P10.2:** HOME (not the power button) pause mid-game, nxlink run | `applet focus: ... state=3` line, audio cut, game continues, no time jump. Known: these lines did not appear on rc7 | nxlink log | P10.2 |
| G4 | **P4.3:** nxlink session of 10 min or more: title 1 min, opening movie 2 min, field walk 4 min, battle 3 min, one save | `nx_audio dbg:` lines show `underruns=0`, `seq` advancing, no `audrvUpdate failed`. Listen: movie XA mix, field BGM, battle SFX, no gap | log | P4.3 ([recipe](P4.3_AUDIO_HW.md#hardware-session-recipe)) |
| G5 | P7.7 HUD name and Claw steal | Done in D8-D10 | | P7.7 |
| G6 | Rumble (optional, still unverified on hardware) | Report the feel | note | [P10.4](P10.4_RUMBLE.md) |
| G8 | **Equipment partner cycle (P6.33 phase 1, #282).** Field menu, Status, Equipment: the slot list waits. Press **ZL** and **ZR** to change the party member (two or more partners needed; the slot cursor stays). Then quit to hbmenu, set the file key `items_partner_switch=0` in the region folder (`sdmc:/switch/dw3/us/port_options.cfg` or `.../eu/...`; file-only, not a row, see [PORT_OPTIONS](PORT_OPTIONS.md#persistence)): relaunch, and ZL / ZR must do nothing there. Default is ON | Partner changes on each press, wraps at both ends, the panel shows that partner's gear; no leave to the menu and no item list opened by ZL / ZR; with the key 0, nothing changes | photo, `dw3.log` if the panel did not change, say which key value was on, how many partners, region | P6.33 ([phase 1](P6.33_ITEMS_MENU.md#phase-1-status-2026-10-26)), patch 0128. **PARTIAL**: owner, 2026-10-27, preview.41: the partner cycle works; the key-0 check not reported **PASSED**: owner log, 2026-10-28, preview.47 US (log partner switch member 1 -> 0; the items_partner_switch=0 sub-check was not done) |
| G10 | **P5.5 stage sweep (US, then EU).** Ordinary NRO of a preview (it has the Cheats screen; a `--no-cheats` build has no sweep). Quit to hbmenu. In `sdmc:/switch/dw3/us/port_options.cfg` add the line `stage_sweep=1` above `end=1` (EU: the same in `eu/port_options.cfg`). Start the game and leave the pad alone: the screen flicks through the stages one by one (a stage, then the debug stage list, about a second each; 5 to 10 minutes in all) and ends on the stage list, still. If the game closes or crashes in between, **keep `crash_report.txt`**, start the NRO again: it carries on with the next stage. When the list has been still for a minute, quit with HOME. **Then set the line to `stage_sweep=0`** (or delete it). Details: [P5.5_STAGE_SWEEP](P5.5_STAGE_SWEEP.md) | Nothing is written to the saves (`dw3-autosave:` never appears). The last run's `dw3.log` ends with `dw3-sweep: done 237/237 ok, failed: none; skipped: WSTAG460`, and has a `dw3-sweep: stage WSTAGxxx ok in N ms (frames 90, mem free M, load L ms)` line per stage | all `sdmc:/switch/dw3/dw3*.log`, `crash_report*.txt`, `us/stage_sweep.txt` (or `eu/`); nothing else is needed, the `done` line and the `FAILED` lines name every problem stage | P5.5 (ticked once US and EU both load and return the set; send the failures, they become findings) |

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

### G9. [COMBINED] P6.24 one `.nro` for both regions (PR B)

**Status: built in PR B; first run on the OLED with preview.48 (2026-10-28, EU chosen): the rows below say which parts the log proves.** The combined `.nro` comes from
`tools/build_switch_game.sh --both`, the CI artifact `dw3-switch-both-nro` or the
release asset `dw3-switch-both.nro`. The
separate US and EU `.nro` files keep shipping next to it until you have confirmed
this one. Put `dw3-switch-both.nro` (or the nxlink variant, whose NACP name ends
in "(nxlink)") in `sdmc:/switch/dw3/` and start it from hbmenu in applet mode.
Data: `sdmc:/switch/dw3/us/` and `/eu/` as always, or a disc image directly in
`sdmc:/switch/dw3/` (G9.11 to G9.13; see
[RUNTIME_LAYOUT](RUNTIME_LAYOUT.md)); the stored choice is
`sdmc:/switch/dw3/port_options.cfg` (`region_default`, `last_region`). Design and
the defaults taken (picker in English, Minus to ask again, no region switching
in game): [SINGLE_BUILD](SINGLE_BUILD.md#owner-priority-2026-10-28-and-orchestrator-defaults).
Every row has its proof in `dw3.log`, so you only report feel and looks (is the
picker readable at arm's length, handheld; does the countdown feel right).

| # | Action | Expect on screen | Proof lines in `dw3.log` (all begin `dw3-region:`) |
|---|---|---|---|
| G9.1 | **Only US data**: move `eu/` away. Start the combined `.nro` | No picker and no pause; the US game starts at once. Same game as the US-only `.nro` | `data found us=1 eu=0`, `stored ... minus_held=n/a`, `decision boot region=us: only the US data is present`, no `picker shown` line, `running us (only the US data is present)` |
| G9.2 | **Only EU data**: put `eu/` back, move `us/` away | No picker; the EU game starts at once (language select first, as always) | `data found us=0 eu=1`, `decision boot region=eu: only the EU data is present`, no `picker shown` line, `running eu (...)` |
| G9.3 | **Both, picker and timeout**: both folders present, `region_default` not stored (delete `port_options.cfg`). Start and press nothing | The picker: `> US <` highlighted and "Starting US in 3..." counting down; after 3 s the US game starts | `data found us=1 eu=1`, `decision pick region=us: both present: picker`, `picker shown (default entry us, timeout 3000 ms)`, `picker result us by countdown after 3000 ms always=0`, `saved last_region=us`, `running us (picker)` **PARTIAL** (owner, preview.48 log, 2026-10-28): the both-present picker **PASSED by log** (`data found us=1 eu=1`, `picker shown (default entry us, timeout 3000 ms)`). The timeout is **not tested**: the owner moved the cursor first, which cancels the countdown by design (`dw3_picker_step`, `port/region/dw3_region.c`: Left, Right or Y sets `cancelled`; `dw3_picker_seconds_left` then returns -1, no countdown), so the 19.5 s wait was expected and the countdown did not fire. To test it, press nothing |
| G9.4 | **Default = last choice**: start again, press Right to EU (the countdown stops), A | The picker starts on US (last was US); Right moves to EU and stops the countdown; A starts EU. Start once more and press nothing: the picker now starts on EU and the countdown boots EU | first run: `picker result eu by button A after N ms always=0`, `saved last_region=eu`; next run: `picker shown (default entry eu, ...)` and `picker result eu by countdown after 3000 ms` **PARTIAL** (owner, preview.48 log): the button choice **PASSED by log** (`picker result eu by button A after 19473 ms always=1`; the countdown was cancelled by the cursor move, by design). The default-from-last-choice part (`picker shown (default entry eu, ...)` on a later start with `region_default` cleared) is not tested |
| G9.5 | **Don't ask again**: at the picker press **Y** until "Always start this one: ON", then A. Quit to hbmenu and start again | The next start has no picker and boots that region at once | `picker result <r> by button A after N ms always=1`, `saved last_region=<r> region_default=<r>`; next run `decision boot region=<r>: region_default`, no `picker shown` line **PARTIAL** (owner, preview.48 log): the choice with "always" **PASSED by log** (`picker result eu by button A after 19473 ms always=1`, `saved last_region=eu region_default=eu`, `running eu`). The next start without a picker (`decision boot region=eu: region_default`) is not in the log yet |
| G9.6 | **Minus asks again**: with "always" stored, hold **Minus** while pressing A on the `.nro` in hbmenu, and keep holding until the picker appears | The picker comes back although a region is stored; choose either | `minus_held=1`, `decision pick region=<last>: Minus held: picker`, `picker shown (...)`. Without Minus on the same card: `minus_held=0` and no picker |
| G9.7 | **Quit from the picker and no data**: at the picker press **+**; then move both folders away and start | **+** returns to hbmenu and nothing is started; with no data a setup message names both folders and **+** returns | `picker quit by +`, no `running` line; `data found us=0 eu=0`, `decision no-data` |
| G9.8 | **Both regions play and save**: from the combined `.nro`, start US, load or make a save, quit; start EU, same. Then check the folders | Each region keeps its own saves ([PER_USER_SAVES](PER_USER_SAVES.md)); the other region imports a converted copy once ([CROSS_REGION_SAVES](CROSS_REGION_SAVES.md)); options and the game behave as in the separate `.nro` of that region | `running us/eu`, `runtime version=<r> root=sdmc:/switch/dw3/<r>`, `cross-import ...` as in E3; folder listing of `saves/<uid>/us|eu/memcard/` |
| G9.9 | **Crash report names the region** (optional, nxlink build, forced crash is not needed): after any crash, the first lines of `crash_report.txt` | `disc: us` or `disc: eu` (and the EU `language:` line) | `crash_report.txt` header |
| G9.10 | **Handheld Joy-Cons and a Pro Controller** for G9.3 to G9.6 | Same picker behaviour with both; D-pad or left stick moves the choice | the log is identical apart from times |
| G9.11 | **Disc in the parent folder, one region** ([#322](https://github.com/Larbaco/dw3-switch/issues/322)): move `us/` and `eu/` away (keep `saves/`), put only your US BIN/CUE directly in `sdmc:/switch/dw3/` (not in `us/`). Start the combined `.nro`. Repeat with only the EU disc | The progress bar of the first extraction appears and the game of that region starts, no picker and no setup message. Second start: no extraction | `data found us=1 eu=0 under sdmc:/switch/dw3 (us: disc image sdmc:/switch/dw3/<name>.bin, needs extraction)`, `decision boot region=us: only the US data is present`, `us will be extracted from the disc image sdmc:/switch/dw3/<name>.bin`, `runtime extract wrote=N`, `running us (...)`; second run: `data found us=1 eu=0 under sdmc:/switch/dw3` without `disc image`. For EU the same with `eu` |
| G9.12 | **Both discs in the parent folder**: US and EU BIN/CUE both directly in `sdmc:/switch/dw3/` (names that do not say the region; `us/` and `eu/` away). Start; press A on EU, then on a second start A on US | The picker appears (both regions present); the chosen region is extracted from its own disc (not "disc region mismatch") and starts | `data found us=1 eu=1 under ... (us: disc image <path>; eu: disc image <path>, needs extraction)`, `decision pick region=us: both present: picker`, `picker result eu by button A ...`, `eu will be extracted from the disc image <the EU file>`, `runtime extract wrote=N`; no `disc region mismatch` line |
| G9.13 | **One disc in the parent, the other region already extracted**: keep an extracted `eu/` tree, US disc directly in `sdmc:/switch/dw3/`, no `us/`. Start; pick US | Picker; US extracts from the parent disc and starts. Next start: the disc is not read again | `data found us=1 eu=1 under ... (us: disc image ...)`, no `eu: disc image`, `us will be extracted from the disc image ...`; next run `data found us=1 eu=1 under sdmc:/switch/dw3` with no `disc image` text |

Also tell me: is the picker text readable handheld at arm's length; is 3 s the
right countdown; is Minus a good key to hold; is a 12 MB `.nro` acceptable (it is
about 9.3 MB for one region today).

## What to send back

Send the whole `sdmc:/switch/dw3/*.log` set (the last 5 runs are kept, so no
copying between launches). Logs answer the "did it work" part of each row; the
owner reports feel, looks and preferences. Per session: the letter, region,
language, handheld or docked, the nxlink log if used, photos of anything wrong, and the
crash report after a crash (format in [REPORTING_ISSUES](REPORTING_ISSUES.md)).
When a row passes, tick the item in the [ROADMAP](ROADMAP.md) table and remove
it from Pending OLED checks.

## Proof lines by item (added with the rotating log)

| Item | Lines in `dw3*.log` |
|---|---|
| A1 | `dw3-run: started <date>`, `dw3: version=`, `crash report ids:` (build id), `runtime version=<region>` |
| Log rotation | Owner check: after two launches `sdmc:/switch/dw3/` has `dw3.log` and `dw3.1.log` with different `dw3-run: started` times. Line `dw3-log: rotated (...)` in the new `dw3.log` shows each step (`r` renamed, `c` copied, `-` nothing/failed) with its rc and `kept N previous run(s)`; if dw3.log could not be moved aside the new run is appended, never truncated. **PASSED**: owner OLED, 2026-10-28, preview.45: keeps `dw3.log` to `dw3.4.log`, rc=0x0 |
| A7, A8 | `dw3_game_main returned 0; shutting down (quit path: title Quit game or HOME / applet exit)` |
| H1 to H8 (A14) | `dw3-port-options: group open help (title\|in game)`, `dw3-help: open (from title\|field\|battle, region US\|EU, platform switch, language L, 10 pages)`, `dw3-pages: atlas N glyphs in 3 faces, N bytes of coverage in the image, index built in N us`, `dw3-pages: text help en loaded (...)`, `dw3-help: panel canvas allocated, 2764800 bytes`, `first frame composed N ms after open`, `panel composed in N us` (first two composes, and any over 20 ms), `page <id> (from list\|prev\|next, N lines, panel 13 lines, unverified blocks N)`, `end reached`, `closed after N ms`, `dw3-help: closed after N ms (pages opened N)`, `panel canvas released`, `dw3-port-options: group close help (back to the group list)`; problems: `token ... unresolved`, `text fallback ...`, `cannot be laid out` |
| A5, A9 to A13 | `dw3-title-overlay: options update #N dirty rects=N` for a value or master change (A13: a `rebuild` there is the old 48-80 ms stutter); `dw3-port-options: group open <sound|gameplay|display|saves> (N row(s), title|in game)`, `group close <name> (back to the group list)` then `close (resume runOptions)` or `close in game (back to ...)`, `sound select|cancel|cursor (sfx volume N, voice N)` in the same order as the keys, `dw3-title-overlay: options update rebuild|cached|full rects=N bytes=N compose=N us upload=N us` for each open and group change; Cheats: `group open cheats (group)` or, in game, `cheats are locked in game` |
| B15 | `dw3-port-options: prewarm title|in-game ready in N ms (worker)`, `options update #1 cached ... compose=N us` (a `rebuild` line is the old stutter), `dw3-port-options: first frame shown N ms after the open request`, `longest frame gap N ms`, `open for N ms` |
| B16 | `dw3-map:` lines (see the row); a missing picture shows as `switch map not drawn: no texture yet`, `scaler ... FAILED` or `upload FAILED (GL error 0x..)`, `dw3-map: sprites at fixed size (scale 1/Z, ...)` once per open |
| E1.6c | `dw3-lang-select: language N chosen by Cross (confirm)` or `... by Start` |
| B17 | `dw3-map: mods folder <dir>` (boot), `dw3-map: mod: pack "<id>" <version> loaded: map_<region>_h<half>@Nx.png WxH, reveal N pieces from the pack, M at 1x nearest, K skipped, T ms` or `dw3-map: mod: original map (<reason>; folder <dir>)`, `dw3-map: texture WxH queued for the GPU` (the mod map is `1568x1408`, the original `392x352` or `1176x1056`) |
| B18 | `dw3-widescreen: screen STITSHOP (arg N) opened: gutter 53 px, 16:9 layout, mode file 0x1f7 (STITSHOP)` (US; EU 0x205) when the shop opens; before it `dw3-widescreen: mode file 0x1f7 (STITSHOP): gutter stays 53 px (16:9)` (the field and the shop both use 53 px, so there is no "gutter changed" line) and, on leaving, `mode file 0x158 (FIELDSTG): gutter stays 53 px` (EU 0x166). `gutter 0 px` with `STITSHOP` means the shop ran in 4:3 |
| B19 | `dw3-items-menu: Cross on weapon N (list L): the original game does nothing here (the Items menu only uses consumables; equip from Status > Equipment ...). Not a port regression (items_partner_switch=V)`, one line per press (armor and accessory likewise); no `dw3-items-menu: partner switch` line in the Items menu |
| B20 | `dw3-map: Square ignored in the view (it opens the view; close with the cancel button, PS-side bit 14)` for Y inside the view, then `dw3-map: close requested by the cancel button (PS-side bit 14, ...)` and `dw3-map: close after N frames` for X |
| C7-C9 | `dw3-turbo: menu 1.5x (speed Nx) in the field menus or battle menu, mode file 0x..`, `menu off (frames=N) after the ...` |
| D13 | `dw3-battle: technique picked id=`, `dw3-audio: health`, `dw3-audio: WARNING output silent`, `dw3-audio: underruns ... key-on K ok F failed`, `dw3-audio: WARNING SsUtKeyOn found no free voice` |
| D14 | `dw3-audio: underruns N in last 60 s (total T), queue depth min/avg/max ...`, `dw3-audio: exit: underruns ...` |
| E1 | `dw3-port-restart: fading to black before the reset`, `screen is black after N frames; resetting now`, `new screen ready after N black frames; fading in`, `fade finished, N frames from the request`; `dw3-title-overlay: screen fade to black on`; `dw3-widescreen: gutter 0 px per side (was 53; 4:3 picture) mode file 0x..` must come **between** `screen is black` and `fading in` |
| E2 | `dw3-autosave: wrote autosave.bin ... in N ms (SD write M ms ...); option Auto-save=N` |
| G8 | `dw3-options: file keys items_partner_switch=N map_screen=N` at boot, `dw3-items-menu: partner switch` |
| G9.11 to G9.13 | `dw3-region: data found ... (us: disc image <path>, needs extraction)`, `dw3-region: <r> will be extracted from the disc image <path>`, `dw3-region: disc image ignored: <path> is neither a US nor an EU disc` (a stray BIN), then the usual `decision`, `picker`, `running` lines |
| G10 | `dw3-sweep: stage sweep on (key stage_sweep=1, 238 stages ...`, per stage `dw3-sweep: stage WSTAGxxx (mode 0x..) begin` then `... ok in N ms (frames F, mem free M, load L ms)` or `... FAILED: <reason>`; after a crash the next run says `FAILED: crash or hang in the previous run`; the end is `dw3-sweep: done X/Y ok, failed: ...`. A finished sweep relaunched logs `the previous sweep is complete; not running again`. `dw3-autosave:` must not appear. Reasons: [P5.5_STAGE_SWEEP](P5.5_STAGE_SWEEP.md#log-lines) |

Game-side `fprintf(stderr)` lines (`dw3-port-options`, `dw3-map`, `dw3: memcard`,
`dw3-port-restart`, `dw3-battle-options`) now reach `dw3.log` too: the Switch
build routes stderr into the log when nxlink is not attached. Before this they
did not, on an ordinary build.
