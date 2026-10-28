# Roadmap

Goal: a native Switch homebrew build of Digimon World 3 / 2003 whose game
logic is the C from `vendor/dw3_decomp`, running on a Psy-Q compatible shim.

Approach, in order:

1. Build the game C natively on **PC first** (Linux x86_64) over **PSY-Z**,
   where iteration is cheap and debuggers exist.
2. Give PSY-Z a **libnx backend** in `platforms/switch/` and run the same
   game image on the console.
3. Only then polish for the handheld.

Each task has an id (`P<phase>.<n>`), an objective and a "done when". Tasks
inside a phase are roughly ordered. The previous phase's gate is the bar
for starting the next phase's real work. A goal note written early
(P7.1, P8.1, P9.1, P10.1) or a packaging task the owner asked for early
(P6.5) does not start that phase. No time estimates on purpose. Facts
behind the tasks are in [SURVEY.md](SURVEY.md); layering is in
[ARCHITECTURE.md](ARCHITECTURE.md). The phase order is the dependency order;
the [Priority](#priority) section is the work order, and agents pick work from
the highest non-empty tier.

## Priority

Work order, approved by the owner on 2026-10-28. Agents pick work from the
highest non-empty tier.

### Tier 0 — Now

- Fixes from the owner's latest preview feedback (see
  [Pending OLED checks](#pending-oled-checks)).
- Bookkeeping (2026-10-28): the owner's OLED reports for preview.41 to
  preview.45 are recorded in the status table, under
  [Pending OLED checks](#pending-oled-checks), in the goal notes and in
  [OLED_TEST_PLAN](OLED_TEST_PLAN.md). Nothing is ticked on that basis alone:
  each open goal note still has a part the owner has not reported. P6.4, P6.17
  and P9.4 are ticked. Still open: the in-game Options lag, the EU language
  select at 16:9 and the map balloon, cursor and name box (fixes in preview.47,
  waiting for the owner).
- [P6.24](SINGLE_BUILD.md): **one `.nro` that plays both US and EU** (owner
  priority 2026-10-28). Option (a), two PRs. PR A, PC: `DW3_VERSION=both`, the
  game side built twice and linked once, the P6.15 replays of both regions on
  the one `dw3-pc` ([slice 3](SINGLE_BUILD.md#as-built-slice-3-the-dual-image-link-pc)).
  PR B, Switch: `tools/build_switch_game.sh --both`, detection of the game data
  on the SD card, the boot picker, a CI job and the release asset. Separate US /
  EU `.nro` files keep shipping until the owner confirms the combined one on the
  OLED. Defaults taken for the open questions are in
  [SINGLE_BUILD](SINGLE_BUILD.md#owner-priority-2026-10-28-and-orchestrator-defaults).

### Tier 1 — Stability

- [P5.5](#phase-5--playable-on-switch): stage sweep on hardware.
- [#188](https://github.com/Larbaco/dw3-switch/issues/188) and
  [P4.3](P4.3_AUDIO.md): audio, underrun count.
- [P10.2](SWITCH_FEATURES.md): HOME during a save write.
- [P6.1](P6.1_WIDESCREEN.md): 16:9 batch 2. Stopped by the owner; resumes only
  when the owner allows.

### Tier 2 — UI base (in this order)

1. [P9.9](PORT_OPTIONS.md): finish the Cheats screen restyle and the remaining
   sub-screens.
2. Controls bundle, done together because they touch the same menus:
   [P6.12](#phase-6-goal-notes) remapping, [P9.8](HOTKEYS.md) A/B layout and
   [P10.6](TEXTURE_PACKS.md) Switch glyphs.
3. [P6.30](#phase-6-goal-notes): technical help.
4. [P6.34](#phase-6-goal-notes): menu-flow redesign. It absorbs
   [P6.33](P6.33_ITEMS_MENU.md) phases 2–4, so the items menu is redesigned once.
5. [P6.32](P6.32_MAP_ZOOM.md): map phase 1.

### Tier 3 — Content and QoL

- [P6.35](PLAYER_GUIDE.md): walkthrough. It lives in the new Options sub-screens,
  so it comes after Tier 2.
- [P6.20](PORT_OPTIONS.md#cheats-p620): cheats PR 2
  ([#248](https://github.com/Larbaco/dw3-switch/pull/248)).
- [P6.18](P6.18_PACING.md): pacing.
- [P6.22](P6.22_FIELD_CPU.md): turbo in scripted scenes.
- [P6.25](CROSS_REGION_SAVES.md): cross-region saves (P6.24, the single build,
  moved to Tier 0 on 2026-10-28).

### Tier 4 — Long term

- [P6.7](#phase-6--polish-and-options): PT-BR and localization.
- [P8](TEXTURE_PACKS.md): texture packs.
- [P6.3](#phase-6--polish-and-options): PGXP vertex precision.
- [P6.10](P5.2_BATTLE_GPU.md): hardware render outside battle.
- [P7.5](COMMUNITY_QOL.md): companion.
- Exception: the first mod, the map upscale ([P8.5](MOD_MAP_UPSCALE.md)), runs
  now at the owner's request.

## Status at a glance

Statuses: **done** = ticked; **in progress** = not ticked, with merged work
or hardware evidence; **open** = not ticked, no work yet; **future** = goal
note only (see [Backlog / future goals](#backlog--future-goals)); **dropped**
= the owner decided not to do it (the entry stays for its id and research); **merged**
= folded into another id (the entry stays and names it). Update
this table in the same PR that changes an item. The history of an item
lives in its doc, not in the entry below. OLED checks waiting on the owner are
listed once, in [Pending OLED checks](#pending-oled-checks).

| ID | Item | Status | Doc |
|---|---|---|---|
| P1.1–P1.7 | Toolchains, data pipeline, host skeletons | done | [DISC_SETUP](DISC_SETUP.md), [RUNTIME_LAYOUT](RUNTIME_LAYOUT.md), [CI_SUBMODULES](CI_SUBMODULES.md) |
| P2.1–P2.8 | Port glue and engine bring-up on PC | done | [PATCHES](PATCHES.md), [CDFS_NOTES](CDFS_NOTES.md), [P2.8](P2.8_TITLE_BOOT.md) |
| P3.1–P3.9 | Game bring-up on PC | done | [P3_GATE](P3_GATE.md), [P3_EU_BUILD](P3_EU_BUILD.md), P3.x docs |
| P4.1 | Backend spike | done | [SWITCH_BACKEND](SWITCH_BACKEND.md) |
| P4.2 | Video | done | [P4.2](P4.2_VIDEO.md) |
| P4.3 | Audio | in progress (BGM loop and win jingle fixed; no underrun count yet) | [P4.3](P4.3_AUDIO.md) |
| P4.4 | Input | done | [P4.4](P4.4_INPUT_STATUS.md) |
| P4.5 | Filesystem + log + time | done (log rotation keeps `dw3.log` to `dw3.4.log`, rc=0x0: owner OLED, 2026-10-28, preview.45) | [P4.5](P4.5_STATUS.md) |
| P4.6 | Static modules on Switch | in progress | [P4.6](P4.6_MODULES_STATUS.md) |
| P4.7 | Boot the game | done | [P4.7](P4.7_BOOT.md) |
| P5.1 | Memory budget | in progress (instrumentation; file cache full now evicts, #235) | [P5.1](P5.1_MEMORY.md) |
| P5.2 | Performance pass | done (partner portrait fix #229, OLED pending) | [P5.2](P5.2_BATTLE_GPU.md) |
| P5.3 | Save handling | done | [P5.3](P5.3_FLAT_SAVE.md) |
| P5.4 | Error reporting | in progress (done-when met on the OLED; boot notice 60 s, #231; PR 2 OLED check open) | [P5.4](P5.4_CRASH.md) |
| P5.5 | Stage sweep on hardware | open | — |
| P6.1 | Scaling / widescreen | in progress (map, results, save, title, caption, registration, Items menu at 16:9 in preview.31; card album, deck editor, card shop, gym and Lab at 16:9 in the next preview (0100-0104, OLED pending); iris and grid cover at 16:9 in the next preview (0115, OLED pending); encounter swirl at 16:9 in the next preview (0175, OLED pending); card battle still 4:3; intro news caption bar at 16:9 confirmed on the OLED 2026-10-27, preview.41; EU language select at 16:9 (0177) pending preview.47) | [P6.1](P6.1_WIDESCREEN.md) |
| P6.2 | PAL/NTSC, EU/US, language at boot | in progress | [P3_EU_BUILD](P3_EU_BUILD.md) |
| P6.3 | PGXP-style vertex precision | open | — |
| P6.4 | Fast-forward | done (owner confirmed on the OLED, 2026-10-24; 1.5x in menus, patch 0176, confirmed 2026-10-27, preview.41) | [P6.4](P6.4_FAST_FORWARD.md) |
| P6.5 | Release packaging | done | [P6.5](P6.5_PACKAGING.md), [RELEASES](RELEASES.md) |
| P6.6 | Localization goal | open | — |
| P6.7 | Language packs (PT-BR first) | open | — |
| P6.8 | Extra locales | open | — |
| P6.9 | Restart-game hotkey | in progress (hotkey works on the OLED; PC replays pass for battle, save and Continue on US and EU; OLED retest open) | [P6.9](P6.9_RESTART.md) |
| P6.10 | Hardware render outside battle | future | [P5.2](P5.2_BATTLE_GPU.md) |
| P6.11 | Internal resolution scaling | future | [PC README](../platforms/pc/README.md) |
| P6.12, P6.14, P6.16, P6.19, P6.21 | Backlog goal notes | future | [backlog](#backlog--future-goals) |
| P6.13 | Mapping submenu, merged into P6.12 (owner, 2026-10-24) | merged | [backlog](#backlog--future-goals) |
| P6.15 | Regression replay tests | in progress | [P6.15](P6.15_REPLAY_TESTS.md) |
| P6.17 | Music / SFX volume | done (owner confirmed on the OLED, 2026-10-24) | [P6.17](P6.17_VOLUME.md) |
| P6.18 | Pacing options | in progress (Instant text works; Instant banner and report speed #223; rest of the check open) | [P6.18](P6.18_PACING.md) |
| P6.20 | Port debug / cheat menu in the Options screen | in progress (PR 1 merged, #236: Cheats screen; PR 2, #248: money, give item and warp on page 2, in review; OLED checks open) | [roadmap](#phase-6-goal-notes), [PORT_OPTIONS](PORT_OPTIONS.md) |
| P6.22 | Turbo speed in scripted scenes | in progress (CPU rasterizer is ~97 % of a field frame; pixel-identical rasterizer 4-5x faster, #220; turbo in the intro confirmed on the OLED 2026-10-27, preview.41; Forest Inn and the frame lines still open) | [P6.22](P6.22_FIELD_CPU.md), [roadmap](#phase-6-goal-notes), [P6.4](P6.4_FAST_FORWARD.md) |
| P6.23 | Documentation: player guide, docs index, contributor and modder docs (future goal) | in progress (slice 1 done: player guide and docs index; slice 2: `CONTRIBUTING.md` and a pytest for all doc links, PR open; modder docs open) | [PLAYER_GUIDE](PLAYER_GUIDE.md), [docs index](README.md), [roadmap](#phase-6-goal-notes) |
| P6.24 | One build for US and EU: one `.nro` and one PC binary, region found at boot, a boot picker with both discs (**owner priority 2026-10-28, Tier 0**; P6.25 and P6.26 are built) | in progress (slice 1 built: run-time region choice, picker logic and a PC launcher; slice 2 built: Switch launcher `.nro`, CI artifact only, OLED check open; **slice 3 built (PR A): `DW3_VERSION=both`, one `dw3-pc` with both game copies, P6.15 replays of both regions pass on it**; the Switch `.nro` with the in-image picker, CI and the release asset are PR B) | [SINGLE_BUILD](SINGLE_BUILD.md#as-built-slice-1), [slice 2](SINGLE_BUILD.md#as-built-slice-2-the-switch-launcher), [slice 3](SINGLE_BUILD.md#as-built-slice-3-the-dual-image-link-pc), [roadmap](#phase-6-goal-notes) |
| P6.25 | Cross-region saves: a US save loads in EU and the reverse, one canonical save container (future goal) | in progress (slice 1 built: converter and one-way import on load, PC replays US to EU and EU to US; container and store open; OLED check open) | [CROSS_REGION_SAVES](CROSS_REGION_SAVES.md), [roadmap](#phase-6-goal-notes) |
| P6.26 | Per-user saves: saves bound to the Switch user profile chosen in the Switch profile selector, global options (future goal) | in progress (slice 1 built: user-resolution seam, Switch account + profile selector, saves per profile with a safe legacy copy; owner OLED, preview.43/44 (2026-10-27/28): each profile has its own Continue, cancelling the profile selector returns to the Switch HOME; E5.1, E5.2 and the logs not reported; import prompt, canonical store and global options open) | [PER_USER_SAVES](PER_USER_SAVES.md), [roadmap](#phase-6-goal-notes) |
| P6.27 | Return to title from the pause (field) menu | in progress (built, PC US and EU replays; owner OLED, 2026-10-27, preview.41: Return to title works and Continue after it works; the Yes / No window, fade and card checks not reported) | [PORT_OPTIONS](PORT_OPTIONS.md#quit-and-return-to-title-p627-p628), [roadmap](#phase-6-goal-notes) |
| P6.28 | Quit game on the title menu, back to hbmenu | in progress (built, PC US and EU replays; owner OLED, 2026-10-27, preview.41: Quit through the Sphaira forwarder works and returns to hbmenu; the full-app Sphaira launch shows errors, the forwarder is fine; card files, flush line and the US / EU split not reported) | [PORT_OPTIONS](PORT_OPTIONS.md#quit-and-return-to-title-p627-p628), [roadmap](#phase-6-goal-notes) |
| P6.29 | Save editor: external tool on the owner's own saves, builds on P6.25 (future goal) | future | [roadmap](#phase-6-goal-notes), [CROSS_REGION_SAVES](CROSS_REGION_SAVES.md) |
| P6.30 | Help, FAQ and tips from the pause menu and the title menu (future goal; owner answers recorded 2026-10-27; after P9.9) | future | [roadmap](#phase-6-goal-notes) |
| P6.31 | Auto-save, off by default | in progress (built: Options row, area-change write to its own file `autosave.bin`, title Continue loads it and the old Continue is Load; PC US and EU replays; owner OLED, 2026-10-27, preview.41: writes on an area change and on HOME close, title plaque spacing (#287); the save-file, Continue and Load Game checks not reported) | [P6.31_AUTOSAVE](P6.31_AUTOSAVE.md), [roadmap](#phase-6-goal-notes), [P5.3](P5.3_FLAT_SAVE.md) |
| P6.32 | Map zoom and full-screen map (future goal) | phase 0 PoC built (PR `feat/p6.32-map-poc`); owner OLED, preview.45 (2026-10-28): the map shows and Nearest is the preferred scaler; balloon, cursor and name box pending preview.47 | [roadmap](#phase-6-goal-notes), [P6.32](P6.32_MAP_ZOOM.md), [P6.1](P6.1_WIDESCREEN.md), [map upscale mod](MOD_MAP_UPSCALE.md) |
| P6.33 | Items menu usability redesign | in progress (owner decisions recorded 2026-10-26; phase 1 partner cycle built: patch 0128, ZL / ZR, on by default; phases 2, 3 and 4 open; phase 1 cycle confirmed on the OLED 2026-10-27, preview.41; the `items_partner_switch=0` check open; phases 2–4 folded into P6.34) | [roadmap](#phase-6-goal-notes), [P6.33](P6.33_ITEMS_MENU.md), [P6.1](P6.1_WIDESCREEN.md) |
| P6.34 | Redesign of the items / status / equipment menu flow (future goal) | future | [roadmap](#phase-6-goal-notes), [P6.33](P6.33_ITEMS_MENU.md) |
| P6.35 | In-game walkthrough help: where to go next and what to do, by story progress (future goal) | future (design written 2026-10-28, owner decisions recorded) | [roadmap](#phase-6-goal-notes), [design](P6.35_WALKTHROUGH.md), [PLAYER_GUIDE](PLAYER_GUIDE.md) |
| P7.1 | Inventory and boundaries | done | [COMMUNITY_QOL](COMMUNITY_QOL.md) |
| P7.2 | Patched EU disc as data source | open | [COMMUNITY_QOL](COMMUNITY_QOL.md) |
| P7.3 | Binary-patch limits | in progress | [COMMUNITY_QOL](COMMUNITY_QOL.md) |
| P7.4 | Feature parity (native) | open | [COMMUNITY_QOL](COMMUNITY_QOL.md) |
| P7.5 | Companion (optional) | open | [COMMUNITY_QOL](COMMUNITY_QOL.md) |
| P7.6, P7.8–P7.14 | Backlog goal notes | future | [backlog](#backlog--future-goals) |
| P7.7 | Bug and text fixes | in progress (2 of 5 fixed; Claw steal and HUD name not yet checked on the OLED; Underground Path water texture bug fixed, owner confirmed on the OLED 2026-10-27, preview.41) | [P7.7](P7.7_BUG_FIXES.md) |
| P7.9 | Battle menu memory | in progress (part A confirmed on the US OLED, rc7; part B deferred) | [P7.9](P7.9_BATTLE_MENU_MEMORY.md) |
| P8.1 | Texture pack goal note | done | [TEXTURE_PACKS](TEXTURE_PACKS.md) |
| P8.2 | Pack layout | open | [TEXTURE_PACKS](TEXTURE_PACKS.md), [MOD_MAP_UPSCALE](MOD_MAP_UPSCALE.md) |
| P8.3 | Formats and scaling | open | [TEXTURE_PACKS](TEXTURE_PACKS.md) |
| P8.4 | Tooling (optional) | open | — |
| P8.5 | Modding API: mod folder, asset and data-table overrides (future goal) | future (first mod: map upscale, design note) | [roadmap](#phase-8--texture-pack-compatibility), [TEXTURE_PACKS](TEXTURE_PACKS.md), [MOD_MAP_UPSCALE](MOD_MAP_UPSCALE.md) |
| P9.1 | Port options goal note | done | [PORT_OPTIONS](PORT_OPTIONS.md) |
| P9.2 | Menu entry and shell | done | [P9.2](P9.2_MENU.md) |
| P9.3 | Toggle registry | done | [PORT_OPTIONS](PORT_OPTIONS.md) |
| P9.4 | Persistence (blocks many goals) | done (owner confirmed on the OLED, 2026-10-24) | [PORT_OPTIONS](PORT_OPTIONS.md) |
| P9.5 | Wire real features | in progress | [PORT_OPTIONS](PORT_OPTIONS.md) |
| P9.6 | External save converter | open | — |
| P9.7 | Port options reachable in game | in progress (slice 1, the field menu, and slice 2, battle: built, PC US and EU replays, Switch Ryubing nx scenarios; live Widescreen open; owner OLED checks open) | [PORT_OPTIONS](PORT_OPTIONS.md#options-in-game-p97), [roadmap](#phase-9--port-options-menu) |
| P9.8 | Confirm / cancel layout option (PS positional or Nintendo A/B) (future goal) | future | [roadmap](#phase-9--port-options-menu), [HOTKEYS](HOTKEYS.md) |
| P9.9 | Options screen redesign in the title menu's style, room for more rows (slice 1 done 2026-10-27: groups and sub-screens; OLED pending; Cheats screen restyle, Controls, Help, Walkthrough follow) | slice 1 in review | [roadmap](#phase-9--port-options-menu), [PORT_OPTIONS](PORT_OPTIONS.md) |
| P10.1 | Switch features goal note | done | [SWITCH_FEATURES](SWITCH_FEATURES.md) |
| P10.2 | Lifecycle hardening (has P5.3's leftover) | in progress (sleep and resume: owner confirmed on the OLED, 2026-10-24; HOME during a save write open) | [SWITCH_FEATURES](SWITCH_FEATURES.md) |
| P10.3 | Presentation modes | open | — |
| P10.4 | Rumble polish (optional) | open | — |
| P10.5 | Extra applets (optional) | open | — |
| P10.6 | Switch-style button glyphs (future goal) | future | [roadmap](#phase-10--native-switch-features-residual), [TEXTURE_PACKS](TEXTURE_PACKS.md) |
| P10.7 | NSP packaging: forwarder, full NSP or stay NRO | dropped | [NSP_PACKAGING](NSP_PACKAGING.md), [roadmap](#phase-10--native-switch-features-residual) |

## Pending OLED checks

Test on the latest preview. The owner's reports for **preview.41 to preview.45**
(2026-10-27/28) are recorded in the table and in the bullets below, which now
list only what is still open. One line per item; the entry holds the steps and
the history. Tick the item in the table once the owner confirms every part of
its done-when. The ordered session plan (written for preview.36) is
[OLED_TEST_PLAN](OLED_TEST_PLAN.md).

- **P5 gate:** start → first boss on hardware, US and EU. The Phase 5 gate is
  not met yet (owner, 2026-10-24); see [Phase 5](#phase-5--playable-on-switch).
- **P6.1:** 16:9 for the map screen, results header, save slot picker, title
  menu, news caption bar, registration screen (patches 0080-0082, 0085-0087),
  and the Items menu and field menu screens (0075, 0076, 0074). US and EU. The
  banner and technique backdrops: EU still to do. The intro news caption bar at 16:9 is confirmed (owner, 2026-10-27, preview.41). **Pending preview.47:** the EU language select at 16:9 (patch 0177, E1.6b). **New (0115):** the area-change iris and grid cover (walk through a door or exit: the view should close from the full 16:9 picture, the grid cover covering the whole picture, no map shifted back after it closes); **New (0175):** the encounter spiral swirl now covers the 16:9 picture (enter a random battle at turbo off and at 2x, 3x and 4x: the gutters must not go black on the swirl's first frame and its tiles must slide off across the full width; B12 of the [OLED test plan](OLED_TEST_PLAN.md)). **New (0100-0104):** the
  card album and deck editor (from Card folders: all their panels, the card
  details, the list of cards owned), the card shop (Buy One Card; Open Booster
  was not captured), the gym and the Digimon Lab (menu, party, digivolve). US
  and EU; in the Lab the chart, entry and skill screens still keep their left
  panels at the 4:3 edge.
- **P6.18:** a won battle with R3 on, the results screen at the
  battle's speed (#223); Instant text, the area banner's names whole, after a
  battle and at a door.
- **P6.20:** the Cheats screen checklist in
  [PORT_OPTIONS.md](PORT_OPTIONS.md#cheats-p620) (#236), and the page 2
  checklist (Money, Warp, Give item) in
  [PORT_OPTIONS.md](PORT_OPTIONS.md#oled-checklist-page-2-owner) (PR 2).
- **P6.24 (slice 2, Switch launcher):** copy the launcher `dw3.nro` (CI
  artifact `dw3-switch-launcher-nro`) to `sdmc:/switch/dw3/` and the US and EU
  builds there as `dw3-us.nro` and `dw3-eu.nro`; start `dw3` from hbmenu
  (applet mode, not a title takeover). Steps and what to expect:
  [SINGLE_BUILD.md](SINGLE_BUILD.md#oled-checklist-launcher-owner). Cases:
  both regions (picker, 3 s countdown boots the last region, A, Y "always",
  + quits), one region only (starts at once, no picker), no region (setup
  message, + quits), and Minus held at start with a stored "always" (picker
  comes back).
- **P6.25 (slice 1):** a US save in the US folder and none in the EU folder:
  the EU build lists it under Continue and loads it; and the reverse; with a
  save in both folders nothing is imported. Checklist:
  [CROSS_REGION_SAVES.md](CROSS_REGION_SAVES.md#as-built-slice-1).
- **P6.9:** restart on the OLED, US and EU, with Restart hotkey ON: the chord
  in the field, inside a battle, after a won battle and right after saving at
  a save point; each must reach the title (EU: the language select) with no
  overlay left over, and Continue must load the save written before the
  restart. PC replays pass ([P6.9_RESTART.md](P6.9_RESTART.md)); tick P6.9
  only after this.
- **P5.1:** the 2 hour session of the checklist in
  [P5.1_MEMORY.md](P5.1_MEMORY.md#owner-checklist-2-hour-oled--nxlink-session-p51-stays-unticked-until-done)
  (US and EU): `heap hw` and `libc hw` flat after the first 30 minutes. The PC
  soak never filled the file cache, so also look for the
  `dw3-file-cache: ... evicted` lines in `dw3.log` (#235); tick P5.1 after.
- **P5.4:** the notice stays up to 60 s and A dismisses it (#231); the PR 2
  forced-assert check in [P5.4_CRASH.md](P5.4_CRASH.md#pr-2-owner-oled-check).
- **P5.2:** partner view portrait in `gpu-tex` (#229): Kotemon, the first
  Kuwagamon fight, later battles.
- **P6.22:** field turbo with the faster pixel-identical rasterizer (#220) and pass 2 (frames turbo does not show are not drawn, cheaper present): US and EU, 4x, the Forest Inn top and bottom (the opening: turbo confirmed on the OLED 2026-10-27, preview.41, frame lines not sent); read `frame dt us=` and `defer dbg:` in `dw3.log` ([P6.22_FIELD_CPU.md](P6.22_FIELD_CPU.md#pass-2-2026-10-25-frames-turbo-does-not-show-are-not-drawn)).
- **P6.27 / P6.28:** Quit game on the title menu (the window, No, Yes: back to
  hbmenu with the flush line in `dw3.log` and the card files intact; HOME still
  exits) and Return to title in the field menu (six and seven
  rows; No, Yes: back at the title, or the language select on EU). US and EU, at
  least two EU languages. Checklist:
  [PORT_OPTIONS.md](PORT_OPTIONS.md#oled-checklist-quit-and-return-to-title-owner).
  Confirmed on the OLED 2026-10-27 (preview.41): Quit through the Sphaira forwarder works and returns to hbmenu (the full-app Sphaira launch shows errors; the forwarder is fine), and Return to title works, with Continue after it. Not reported yet: the Quit window, the card files and flush line, and the US / EU split.
- **P9.7 (slice 1):** the Options row of the field menu, US and EU, at least two
  EU languages: the window over the field, Widescreen and the Cheats hint grey,
  Music volume, Encounter rate, Text speed and the Restart hotkey apply at once,
  B returns to the menu, the values persist. Checklist:
  [PORT_OPTIONS.md](PORT_OPTIONS.md#oled-checklist-options-in-game-owner).
  Still lagged on preview.45 (B15b); the fix is in preview.47, pending the owner.
- **P9.7 (slice 2):** Options in battle, US and EU, at least two EU languages:
  Minus at the command menu opens the window over the frozen battle (3D picture
  kept, no flicker), nothing moves while it is open (30 s), music keeps playing
  and follows the volume, Triangle returns to the same command menu and the
  battle ends normally; Minus does nothing during attacks or with L / R held.
  Checklist:
  [PORT_OPTIONS.md](PORT_OPTIONS.md#oled-checklist-options-in-battle-owner)
  (OLED_TEST_PLAN D12).
- **P6.31:** Auto-save row (the ninth Options row, OFF by default) on US and
  EU, at least two EU languages: on, Continue a save, walk through a door,
  `dw3.log` shows `wrote autosave.bin`, `save0.bin` / `save1.bin` unchanged;
  relaunch: the title has five plaques, **Continue** loads the auto-save (new
  area), **Load Game** opens the slot list with your own save; with no
  `autosave.bin` the title is as before. Checklist:
  [P6.31_AUTOSAVE.md](P6.31_AUTOSAVE.md#oled-checklist-owner-us-and-eu-builds-from-this-pr).
  Confirmed 2026-10-27 (preview.41): the auto-save writes on an area change and on HOME close; title plaque spacing (#287). Still open: the save-file checks, Continue loading the auto-save, Load Game, and the title with no file.
- **P6.26 (per-user saves, slice 1):** confirmed on the OLED 2026-10-27/28
  (preview.43/44): each Switch profile has its own Continue, and cancelling the
  profile selector returns to the Switch HOME (E5.3 to E5.5; logs not sent).
  Still open: E5.1, E5.2 and E5.6 to E5.9 ([OLED_TEST_PLAN E5](OLED_TEST_PLAN.md#e5-per-user-saves-p626)).
- **P6.32 (map view, phase 0):** the map shows and Nearest is the preferred
  scaler (owner, 2026-10-28, preview.45). Pending preview.47: the balloon, the
  cursor and the name box (OLED_TEST_PLAN B16).
- **P6.33 (phase 1, partner cycle):** the ZL / ZR cycle in Equipment works
  (owner, 2026-10-27, preview.41, G8). Still open: the `items_partner_switch=0`
  check.
- **Older, still open:** P6.9 (OLED retest of restart after a battle and a save; PC replays pass), P4.3
  (underrun count), P7.7 (Claw steal, HUD Digivolution name), P7.9 (rest of the
  checklist), P10.2 (HOME during a save write).

**Language matrix.** The EU disc has 7 `LANGUAGE` slots: 0 JPN, 1 USA,
2 ENG, 3 FRA, 4 ITA, 5 GER, 6 SPN (`STAGSLCT_regionNames`). The EU country
select maps its options through `CNTY_SEL_languages = {2,3,5,4,6,1,0}`, and
the pad only reaches options 0–4. So **5 EU languages are playable**
(English, French, German, Italian, Spanish); slots 1 and 0 are unreachable
on the disc. The US disc is English only and has no `LANGUAGE`. "All
languages" in a done-when means those 5 EU languages plus US English.
Source: `vendor/dw3_decomp` @ `b9caa6c`,
`src/menus/country_select/country_select.c` and
`src/debug/stage_select/stage_select.c`.

## Phase 0 — Repository (done)

- [x] Dedicated project folder, git repo, `vendor/dw3_decomp` submodule
- [x] Scaffold layout, ignore rules for discs/assets, `AGENTS.md`
- [x] Survey of the decomp for porting (`docs/SURVEY.md`)

## Phase 1 — Toolchains, data pipeline, host skeletons

Objective: every build target exists as a reproducible command, and the
owner's disc can be turned into the layout the port reads, without any
game code yet.

- [x] **P1.1 Build environments in Docker.** `tools/dk.sh` +
  `devkitpro/devkita64` (drvfs/NTFS note in `tools/docker/README.md`).
- [x] **P1.2 Vendor PSY-Z.** Submodule `vendor/psyz` @ `fac9bd58`;
  renderer default `sdl3_gpu` in `docs/PSYZ.md`.
- [x] **P1.3 PC host skeleton.** `platforms/pc/` links PSY-Z; stub
  `dw3-pc` builds with CMake/Ninja.
- [x] **P1.4 Switch host skeleton (`.nro`).** `platforms/switch/` libnx
  console/pad skeleton; build via `./tools/dk.sh make -C platforms/switch`
  (on Linux/ext4; see Docker README for WSL mounts).
- [x] **P1.5 Disc extraction + file-id map.** `tools/build_file_map.py` +
  tests; owner US extract documented in `docs/DISC_SETUP.md`; local
  `disks/us/files_us.json` maps 2194 ids and `--check` passes.
- [x] **P1.6 Runtime data layout.** Define and document the on-SD layout
  (`sdmc:/switch/dw3/<ver>/AAA/...` + `files_<ver>.json` + exe tail boot
  image) and the equivalent PC directory. Add `tools/pack_runtime.py` to
  produce it from `disks/<ver>/`.
  Done when: `pack_runtime.py` output passes `build_file_map.py --check`
  and `.gitignore` covers it. Documented in `docs/RUNTIME_LAYOUT.md`;
  local `runtime/us/` packs with `BOOT_IMAGE.txt` stub (boot TIM Phase 2).
- [x] **P1.7 CI.** GitHub Actions building P1.3 and P1.4 in Docker on
  every push (no game data; PSY-Z sample + skeleton only).
  Done when: a green run on `main` / `phase1-toolchains-hosts`
  (verified: run `38028432267`).
  Current host CI initializes only required submodule paths, including
  `nugget/bsdec` for PC and Switch movies; it skips the unused PS1 matching
  toolchain. See [CI_SUBMODULES.md](CI_SUBMODULES.md).

Gate: PC window + Switch `.nro` skeleton + `files_us.json` for the owner's
disc.

**Gate passed** (2026-10-10): P1.1–P1.7 complete. Local: `pytest tools/tests`
(4 passed), `files_us.json` `--check` on `disks/us/` and `runtime/us/`,
`build/pc/dw3-pc` present. Switch `.nro`: CI run
[`38028432267`](https://github.com/Larbaco/dw3-switch/actions/runs/38028432267)
(NTFS/Docker host cannot write `platforms/switch/build/` — see
`tools/docker/README.md`).

## Phase 2 — Port glue and engine bring-up on PC

Status: **complete** (gate met on `phase2-engine` @ `1f34110`;
title from decomp C; pad Critical re-reviewed APPROVE.
Plan:
[`docs/superpowers/plans/2026-10-10-phase2-pc-bringup.md`](superpowers/plans/2026-10-10-phase2-pc-bringup.md)).

Tracking: Phase 2 complete on `phase2-engine` @ `1f34110` (pad
`PadInitDirect`→`InitPAD` / wrap `PadGetState`; headless Start bit
proven). Merged to `main` via PR #3 (`e1f048b`).

Objective: compile the decomp's **executable** modules (`src/engine/`) for
the host, replace MIPS-only pieces, and reach the title screen on PC.

- [x] **P2.1 Source selection and flags.** `port/CMakeLists.txt` builds
  US `src/engine/**/*.c` from `build/decomp-src` (via
  `tools/apply_patches.sh`) with the GCC 2.8.1-semantics flags from
  `mk/toolchain/gcc.mk` (`-std=gnu89 -fsigned-char -fno-builtin
  -fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks
  -fno-aggressive-loop-optimizations -fno-zero-initialized-in-bss
  -fno-common -fno-toplevel-reorder`), `VERSION_US`, `PORT_HOST`,
  port/PSY-Z includes first. Target: `dw3_engine_objs`.
  Done when: `src/engine/**/*.c` compiles to objects on x86_64 with
  `-Werror=implicit-function-declaration -Werror=int-conversion`.
- [x] **P2.2 Patch mechanism for the submodule.** `port/patches/*.patch`
  applied by `tools/apply_patches.sh` into a build copy (never in-place),
  with a `--check` that fails if the submodule moved. Policy in
  `docs/PATCHES.md`: every patch states whether it is upstream-bound
  (match-neutral) or port-only.
  Done when: an empty patch series round-trips and CI runs the check.
- [x] **P2.3 Replace MIPS-only code.** `SetSpadStack`/`ResetSpadStack`
  → no-ops under `PORT_HOST`; `allocMemHigh` returns the pointer;
  `OVERLAY_ADDRESS`/`SUB_OVERLAY_ADDRESS` become host symbols
  (`port/overlays/host_vram.c`); boot TIM stubbed pending
  `runtime/us/boot_tim.bin` (E6); GTE macros in `include/engine/gte.h`
  mapped to PSY-Z's software GTE under `PORT_HOST`.
  Done when: no `__asm__` remains in the host build and `grep -r mtc2`
  over the preprocessed output is empty.
- [x] **P2.4 64-bit audit, part 1 (engine).** Replace literal sizes with
  `sizeof`/`offsetof` in `src/engine` (`HEAP.zero(&GFX.prim, …)`,
  `PAD_DATA_SIZE`, `MemBlock` split constant, `createInn`), fix
  `freeFilesFrom` to take `void *`, remove pointer↔`u32`/`s32` casts in
  engine code. Match-neutral patches `0006`/`0007` in `port/patches/`
  (see `docs/PATCHES.md`).
  Done when: `-Wpointer-to-int-cast -Wint-to-pointer-cast` are clean for
  `src/engine` (enforced as `-Werror=` on `dw3_engine`). Upstream
  `make compare` not run here — no MIPS toolchain on this host (blocker
  noted in `docs/PATCHES.md`).
- [x] **P2.5 CD → filesystem.** `port/cdfs/`: Wave 1 file-id read path
  over `files_<ver>.json` and packed host files (`cdfs_set_root` /
  `cdfs_load_map` / `cdfs_read`); path traversal rejected; `cdfs_poll`
  reserved for later `cdReadyCallback`-style async. PSY-Z `libcd` kept
  for STR streaming only. Sector/EOF notes:
  [CDFS_NOTES.md](CDFS_NOTES.md). Host CLI + tests:
  `tools/cdfs_cat.c`, `tools/cdfs_suite.c`, `tools/tests/test_cdfs.py`,
  `tools/tests/test_cdfs_suite.py` (mini fixture always; packed
  `runtime/us` / `DW3_SD_RUNTIME` when present; skipped in CI without
  owner data).
  Done when: fixture reads are byte-identical — verified. Full
  `FILE_CACHE.load` wiring is later (title boot / engine integrate).
  P4.5 reuses this suite against the SD layout.
- [x] **P2.6 Static overlay model.** `port/overlays/`: descriptor table
  `{file id → entry, data, data_init, bss}`, `loadModeOverlay` /
  `loadSubOverlay` host path (`0008`) restores initial `.data` **only
  when the id changes** and zeros `.bss`; same id preserves. Symbol
  prefixes: **generated headers** (documented in
  `port/overlays/README.md`; not objcopy). US title table registers
  `STDWTITL` (`CNTY_SEL` skipped). Host test:
  `tools/tests/test_overlay_host.py`.
  Done when: two different overlays and the same overlay twice behave as
  on PS1 (globals reset vs. preserved) — verified by the host fixture
  test. Full title link/boot is P2.8 / E6.
  Follow-up (2026-10-22, patch `0061`): a failed restore (file id not in
  the host table) used to print only the `kind=unknown UNKNOWN` marker
  while the loader still recorded the id as loaded. It now adds a
  `dw3: ERROR` line, leaves the latch at `-1` (the next load restores
  again), and aborts in strict mode (every replay run, the Switch replay
  build, `DW3_OVERLAY_STRICT=1`). Checked: the same-sub skip across
  battles matches the PS1 (every battle exit is a mode change, which
  resets the sub latch), so `WFIGHTMN` is restored per battle; regression
  scenarios `us/eu-two-battles` (PC). See `port/overlays/README.md`.
- [x] **P2.7 Memory card → file.** `port/memcard/`: 128 KiB PS1-format
  `.mcr` per slot (`memcard_host_*` sector API matching libcard).
  Engine path: `memcard_libmcrd.c` (`MemCard*`) + `--wrap=_card_read/
  _card_write` → `memcard_host_*` (P3.4 STGMCARD save evidence). Host CLI
  + tests: `tools/memcard_rw.c`, `tools/tests/test_memcard.py`.
  DuckStation round-trip is owner manual (see `port/memcard/README.md`).
  Done when: host `.mcr` images are 128 KiB formatted/readable — verified
  by fixture tests.
- [x] **P2.8 Boot to title on PC.** (E6 / 2026-10-10; pad fix 2026-10-10):
  `dw3-pc` links engine + `STDWTITL` + cdfs + memcard + PSY-Z;
  `DW3_RUNTIME`/`--runtime` (default `runtime/us`), memcard dir,
  `boot_tim.bin`, `dw3_game_main()`. Movies skipped (`MODE_TITLE`).
  Title loader completes; `STDWTITL_tickTitle` runs (menu / PRESS START).
  Host libpad: `PadInitDirect` → `InitPAD`, `PadGetState` returns
  connected (`PadStateFindCTP1`) when PSY-Z/SDL has a pad/keyboard frame
  (PSY-Z's stub always returned Discon). Proven headless:
  `pytest tools/tests/test_pad_host.py` (injected Start bit in RX buffer).
  Screenshot: recognizable title (circuit BG + logo fragments;
  sprite/VRAM glitches remain — follow-up). `vendor/` clean.
  Details: `docs/P2.8_TITLE_BOOT.md`.
  Done when: the boot image shows, then the title screen renders and
  reacts to the pad on PC (movies may be skipped at this point).

Gate: title screen on PC from the decomp's C, no local edits inside
`vendor/`. **Passed** on `phase2-engine` @ `1f34110` (title draws +
pad state connected; merged via PR #3). Field sprite polish is a
Phase 3 follow-up (`docs/P3_GPU_GLITCHES.md`), not this gate.

## Phase 3 — Game bring-up on PC

Status: **complete** (gate met on `phase3-pc-bringup` @ `a5ae93a`;
plan:
[`docs/superpowers/plans/2026-10-10-phase3-pc-game-loop.md`](superpowers/plans/2026-10-10-phase3-pc-game-loop.md)).

Tracking: P3.1–P3.9 all met their done-when (evidence in the P3.x docs).
Still open after the gate: the full EU playthrough and EU movie acceptance.
The tracking log, the EU/Switch extraction notes and the PC 3× render
experiment (now P6.11) are in
[P3_GATE.md](P3_GATE.md#history-moved-from-roadmapmd).

Objective: the full game loop runs on PC: field, stages, battle, menus,
audio, movies.

- [x] **P3.1 64-bit audit, part 2 (overlays + stages).** Same rules as
  P2.4 across `src/field`, `src/battle`, `src/cardgame`, `src/menus`,
  `src/debug`. Match-neutral `0018` + PORT_HOST `0019` (`port/patches/`):
  literal `createTask` sizes → `sizeof`, children as `N * sizeof(Task *)`,
  `STAGE_SELECT_MODE_ARG_SAVE`, field script/`GFX.vsync` as `void*`,
  pointer+offset for map/fighter/menu casts, field `freeFrom` heap
  watermark. Evidence: `docs/P3.1_64BIT_AUDIT.md` — `apply_patches.sh
  --check`; `ninja -C build/pc dw3_p31_audit` (432 objs under
  `-Werror=pointer-to-int-cast -Werror=int-to-pointer-cast`);
  `dw3_engine_objs` still builds. Excluded with compiler evidence: EU-only
  WSTAG920–974 (`LANGUAGE`); battle `draw`/`lights`/`model_mesh` (PSY-Z
  gaps). Full overlay link lands with P3.2+. Upstream `make compare`
  blocker unchanged (`docs/PATCHES.md`).
- [x] **P3.2 Field + first stage.** `FIELDSTG` + `WSTAG260` scripts +
  starting town `WSTAG780` (door → `WSTAG785`) + `STPLNMET`. Patches
  `0020`–`0023` + `DW3_AUTO_PAD` host inject. **Done-when evidence**
  (`docs/P3.2_FIELD.md`): pad `moveByPad` moves actor (Δy on 780/785);
  real `SCRIPT_LEAVE`/`leaveField` 474→475 (not `AUTO_DOOR`); name
  `0x500` then back to 475. Follow-up: **WSTAG790** (`0x2D9` / 476)
  linked — FAST path `SCRIPT_LEAVE`→`pickStage` 476 without SEGV
  (`docs/P3.2_FIELD.md`).
  Done when: new game → name entry → first field area is walkable and a
  door changes stage.
- [x] **P3.3 Battle.** `FIGHTSTG` + `WFIGHTMN` + `STFGTREP` linked
  (`dw3_p33_battle`); patches `0024`/`0025` + `DW3_FORCE_ENCOUNTER` /
  `DW3_FORCE_WIN` / `DW3_AUTO_PAD=confirm`. **Done-when evidence**
  (`docs/P3.3_BATTLE.md`): win → report applies XP/levels → return to
  `FIELDSTG` (`0x2d7` / stage 474) with walk + door. Follow-up: real
  wilderness encounter + pad-driven KO (`WFIGHTTS` optional).
  Done when: a random encounter completes and returns to the field with
  stats applied.
- [x] **P3.4 Menus and shops.** `STSTATUS`, `STITSHOP`, `STGTRAIN`,
  `STGDGLAB`, `STGMCARD`, `STDGNAME`, `STPLNMET` linked (`dw3_p34_menus`);
  patches `0026`/`0027`/`0028`/`0031`/`0033` + `DW3_FORCE_MENU` /
  `DW3_AUTO_MENU_RETURN` / `DW3_FORCE_MCARD_SAVE` /
  `DW3_FORCE_MCARD_LOAD` / `DW3_FORCE_NPC_ACTION`. **Done-when evidence**
  (`docs/P3.4_MENUS.md`, `READY_FOR_MAIN_ROADMAP_SYNC: P3.4`): each opens
  from field via `leaveField` and returns to `FIELDSTG`; **STSTATUS
  Triangle UI cancel** (no `AUTO_MENU_RETURN`); **STGMCARD force-save +
  force-load** via P2.7 `.mcr`; pad `AUTO_PAD=status`; NPC applyAction
  shop/lab/naming/gym; WSTAG245 warped `talkToActorAt` → shop.
  Follow-up: free pad walk-up. Load checksum on a DuckStation US card
  matches (`port/memcard` next-block-minus-1 links; see that README).
  Done when: each opens from the field, works end-to-end, and saving via
  `STGMCARD` uses P2.7.
- [x] **P3.5 Card game.** Linked (`dw3_p35_cardgame`); patches `0029`/
  `0032` + `DW3_FORCE_CARD_WIN` / `DW3_FORCE_PACK_OPEN` /
  `DW3_AFTER_CARD_ALBUM` / reuse `DW3_FORCE_MENU`. **Done-when
  evidence** (`docs/P3.5_CARDGAME.md`,
  `READY_FOR_MAIN_ROADMAP_SYNC: P3.5`): force-win prize → `FIELDSTG`;
  `STCRDSHP_openPack` real `addCards` `cardsSeen` 29→33 →
  `STCRDABM` 33 → field. Host win `addCards` default off
  (`DW3_FORCE_CARD_WIN_ADDCARDS`). Follow-up: pad battle, NPC/script
  entry.
  Done when: a card battle can be won and the album updates.
- [x] **P3.6 Audio.** libsnd path through PSY-Z: VAB banks, SEP music,
  SFX, reverb settings the game sets. Verify `SsSeqCalledTbyT` timing
  from the vsync callback.
  **Done-when evidence** (`docs/P3.6_AUDIO.md`,
  `READY_FOR_MAIN_ROADMAP_SYNC: P3.6`): tick pump; psyz `dw3-seq-sep`
  @ `9e6c124` host `_SsSeqPlay` + note-on (sotn-decomp seqread port,
  not matching ASM); title bank `0x47` SEP + non-silent raw; field
  WSTAG780→785 banks `0x29`→`0x1B` (`SsSepPlay` / `SEP note-on`
  seq 2→3); battle `DW3_FORCE_ENCOUNTER`/`FORCE_WIN` → battle SEP
  `0x6004000a` + `SsUtKeyOn` (`SOUND_COUNT` / PLAYER00) + non-silent
  raw. Patch `0035` evidence logs (PR #13). Follow-up: pitch bend /
  matching midiread / owner Pulse listen.
  #188 (tech SFX silenced all audio until map change): psyz `2caa505`
  frees voices that end without repeat (ENVX 0); unit test + battle smoke
  on PC, in-game tech and HW still to confirm.
  Music loop fix (2026-10-23, preview.30-rc7 OLED: music played one pass,
  then silence): the host SEQ/SEP reader dropped the CC#6 Data Entry loop
  count, so the NRPN loop (CC#99 20 / 30) never jumped back and the song hit
  end-of-track. psyz `a79bc8c` (`dw3-seq-nrpn-loop`) follows the retail
  handlers; unit group `snd_loop` and replay scenarios `us/eu-music-loop`
  and `us/eu-battle-music-loop` (field music loops 3-4 times in 3000 frames,
  the battle theme loops, no end-of-track; the old build logs one). Open:
  OLED check of the fix, NRPN VAB attributes (reverb type / depth) still
  not applied. See
  [P3.6_AUDIO.md](P3.6_AUDIO.md#fix--music-plays-once-and-goes-silent-2026-10-23-preview30-rc7).
  Done when: title music, field music change on stage switch, battle SFX.
- [x] **P3.7 Movies.** Title STR playback through PSY-Z `libpress` +
  streaming (`CdRead2`, `StSetRing`) from raw-sector `.STR` files packed
  in P1.6. **Done-when evidence** (`docs/P3.7_MOVIES.md`,
  `READY_FOR_MAIN_ROADMAP_SYNC: P3.7`): Wave 1–4 + present + Task 5 —
  ring backpressure; sustained `DecDCTout` #330+; FB dump nonzero; XA
  file-bridge mix; `DW3_AUTO_PAD=start` + `0034` → Start-skip →
  `mode=0xE00`; **owner audible XA** on real Pulse/PipeWire DAC after
  SDL rebuild (`SDL_PULSEAUDIO=ON` + `-DSDL_X11_XSCRNSAVER=OFF`).
  OLED preview.21 (2026-10-13): ~15 fps, no skipped frames, correct XA
  audio after the #111 bridge fix. Still open: start-to-finish playback
  and image quality on hardware. History:
  [P3.7_MOVIES.md](P3.7_MOVIES.md#history-moved-from-roadmapmd).
  Done when: the opening movies play with audio and can be skipped. The
  OLED meets the audio and skip part; see
  [pacing diagnostics](P3.7_MOVIE_PACING_DIAGNOSTICS.md).
- [x] **P3.8 Stage sweep.** `STAGSLCT` linked (`dw3_p38_stagslct`); patch
  `0030` + `tools/stage_sweep.py`. **Done-when evidence**
  (`docs/P3.8_STAGES.md`, `READY_FOR_MAIN_ROADMAP_SYNC: P3.8`): batches
  1–6 link 238/239 US stage objs; live STAGSLCT round-trip PASS 237/237
  in `build/p38-sweep/results.tsv`. **`WSTAG460` intentionally excluded**
  (weak stub; host `updateWanderer` SEGV — see `docs/P3.8_STAGES.md`).
  Done when: all **loadable** stages load and return to the select
  screen (`WSTAG460` known-bad, not in the 237 PASS set).
- [x] **P3.9 Reference comparison harness.** Script that boots the same
  save in DuckStation and the PC build and captures screenshots for
  side-by-side review (PS1 output is the oracle).
  **Done** (2026-10-10, `EmulationSpeed=2`): title = MODE_TITLE; field =
  Asuka Inn both sides (`STGMCARD_loadSave ok`, `mode=0x20a`,
  `stageFile=457`); battle = Kunemon vs Monmon Fight menu on both
  (Duck `owner.state` Monmon 20/140; PC `DW3_FORCE_ENCOUNTER=34` +
  `DW3_FORCE_PARTY=1`, fighter0=32, partner=2, full HP, stage drawn).
  Accepted on harness logs + capture contract; pixel match not required.
  Follow-ups (not P3.9 blockers): Fight-camera framing, face-inset layer,
  savestate load into `dw3-pc`. See `docs/P3.9_REFERENCE.md`.
  Done when: a doc page with the comparison for title, field, battle.

Gate: a full playthrough segment (start → first boss) on PC without
crashes; both versions build. **Met** (US-minimum) — see
`docs/P3_GATE.md` (continuous path C @ `a5ae93a`; `FORCE_WIN` accepted;
EU `dw3-pc` link deferred with clear blocker; `WSTAG460` / pad Genji KO
are follow-ups, not gate blockers).

## Phase 4 — Switch host for PSY-Z

Objective: a libnx backend so the Phase 3 image runs unchanged on the
console.

- [x] **P4.1 Backend spike.** Two short experiments, documented in
  `docs/SWITCH_BACKEND.md`: (a) build devkitPro's SDL3 branch from source
  and assess PSY-Z's `sdl3_*` backend; (b) sketch a native libnx backend
  following PSY-Z's PSP layout (`gpu`, `input`, `audio`, `os`, `file`,
  `log`, `module`). Pick one.
  Done when: the decision and its reasons are written down.
  **Decision (2026-10-10):** native libnx (`platforms/switch/psyz_backend/`).
  dkp `switch-sdl-3.4` builds (`libSDL3.a`) but has no SDL_GPU and is not
  in pacman; unsuitable for PSY-Z's PC `sdl3_gpu` default. See
  `docs/SWITCH_BACKEND.md`.
- [x] **P4.2 Video.** Present PSY-Z's VRAM via EGL + GLES2, with vsync
  and docked 1920×1080 / handheld 1280×720 framebuffer sizes.
  Done when: the game image presents the title framebuffer on hardware.
  **Status (2026-10-11):** `nx_gpu.c` presents
  `SoftwareRasterizer_GetVram()` (`eglSwapInterval(1)`). The OLED showed
  the title from hbmenu. Measuring frame time on handheld clocks moved to
  P5.2. Dock/undock while the game is running stays P10.3. The
  `gpu_hello` clear loop is the Makefile `host_hello` sample, not this
  done-when. See `docs/P4.2_VIDEO.md`.
- [ ] **P4.3 Audio.** `audren` output thread fed by PSY-Z's SPU mixer at
  44.1 kHz.
  Done when: libsnd sample plays without underruns on hardware.
  **State (2026-10-24):** in progress, not ticked. The game's audio plays on
  hardware. The BGM loop bug is fixed (#209, confirmed by the owner on the
  OLED in rc8). The "music stops after a won battle" bug is fixed by #215
  (SEP tracks 16..31 play on the host, so the win jingle sounds); owner on
  preview.31-rc1: "fanfare OK". Still open: an underrun count from a hardware
  log. The owner decides whether a listen is enough, or sets a number (for
  example 0 underruns over 10 min of field and battle in the nxlink log). The
  listen + underrun recipe is in [P4.3_AUDIO_HW.md](P4.3_AUDIO_HW.md) (not run
  yet). Dated history: [P4.3_AUDIO.md](P4.3_AUDIO.md#history-moved-from-roadmapmd-2026-10-24).
- [x] **P4.4 Input.** `hid` → PS1 pad bits, including the digital/analog
  modes `PadInfoMode`/`PadSetMainMode` expect; rumble via `PadSetAct` →
  HD rumble (optional).
  Done when: title and field controls work with Joy-Con and Pro
  Controller.
  **Status (2026-10-11):** the Switch host publishes `PSYZ_CTRL_ANALOG_PAD`
  (`0x73`) and maps the left stick through a centre dead zone of 8192.
  D-pad, buttons, Start and Select stay in the button word. **OLED:** the
  owner confirmed the left stick on handheld Joy-Con (`style=0x3`,
  `handheld=1` on the preview.5 boot) and on a Pro Controller. Rumble
  (2026-10-16): the adapter reports the DualShock actuator table, hands
  `PadGetState` to `PadStateStable` after 4 connected polls, and forwards
  `PadSetAct` to the new `Psyz_PadsSetAct` hook; the Switch backend
  (`nx_rumble.c`) maps it to hid HD rumble — **unverified on hardware**,
  still P10.4. See `docs/P4.4_INPUT_STATUS.md` and
  `docs/P10.4_RUMBLE.md`.
- [x] **P4.5 Filesystem + log + time.** `romfs:`/`sdmc:` for the runtime
  layout from P1.6, nxlink/stdout logging, `VSync`/`RCnt` timing.
  Done when: formal P2.5 CDFS suite passes against the packed P1.6 tree
  (PC `runtime/<ver>` or Switch SD `…/switch/dw3/<ver>`): map load, path
  reject, file-id read, `cdfs_read_at`, `cdfs_lookup_lba`.
  **Status:** `tools/cdfs_suite` + `tools/tests/test_cdfs_suite.py`;
  host evidence against owner `runtime/us`. SD validation = same suite with
  `--root` on a mounted/copied card tree (`docs/P4.5_STATUS.md`). OLED
  `sdmc:` I/O already evidenced by preview.4 boot. `nx_file` / `nx_os`
  mount and thread fixes covered by `test_switch_backend.py`.
- [ ] **P4.6 Static modules on Switch.** Confirm P2.6 needs no `dlopen`;
  if PSY-Z's module API is required, provide a static `Psyz_ModuleOpen`
  that dispatches to the linked table.
  Done when: all overlays switch on hardware.
  **Status (2026-10-12):** no-`dlopen` decision stands (`nx_module.c` empty
  TU; modes/stages already linked). OLED owner checklist + nxlink restore
  markers (`kind=mode|sub`) + `tools/parse_overlay_nxlink.py` are in
  `docs/P4.6_MODULES_STATUS.md`. Gate stays open until that checklist is
  filled from a hardware capture — preview field/battle smoke is not enough.
- [x] **P4.7 Boot the game.** Link the Phase 3 image against the Switch
  backend and run from hbmenu.
  Done when: title screen on hardware.
  Title on hardware since the 2026-10-10/11 OLED runs; preview.4 also
  reached field and battle (the Phase 4 gate below). Link story:
  [SWITCH_BACKEND.md](SWITCH_BACKEND.md). P4.6's overlay checklist is a
  separate open item. History (preview.1–.5): [P4.7_BOOT.md](P4.7_BOOT.md).

  **Gate:** title + field + battle on a Switch. **Met (2026-10-11)** from the
  OLED preview.4 run: main menu/new game, multiple map transitions, and battle
  were owner-confirmed. This does not close the individual P4.6 criterion.

## Phase 5 — Playable on Switch

Objective: the Phase 3 playthrough segment works on the console at full
speed.

- [ ] **P5.1 Memory budget.** Heap arena, file cache and VRAM emulation
  sized for the console; no allocations after init in the frame loop.
  Done when: a 2-hour session shows flat memory in a debug overlay.
  **Instrumentation (in progress, not done).** `--nxlink` builds sample
  game-heap used/free/high-water, `MEM_FILE_CACHE`, fixed VRAM-emulation
  footprint, and newlib `mallinfo` at the ~1 Hz `frame dt` cadence
  (`mem dbg:` line + HUD lines). See [P5.1_MEMORY.md](P5.1_MEMORY.md).
  This is measurement only — not a proven 2h flat session.
  **PC audit and soak (2026-10-25, test/p5.1-memory-soak):** no port or engine
  allocation per frame after init on PC (all steady-state host mallocs are the
  PC-only SDL GPU renderer; per-file `fopen` churn and the title movie's
  per-slice buffer are listed, the latter fixed); US 2 h at 1x (431568 frames)
  and EU 1 h at 1x on one process each: no crash, game heap hw 1929K / 1854K of
  8192K, file cache max 1.4M, glibc arena flat at 94008K for the last 45 min of
  the US run. Details, tools and the 2 h OLED checklist in
  [P5.1_MEMORY.md](P5.1_MEMORY.md). Stays unticked until the owner's OLED run.
  **rc7 OLED log (2026-10-23, US, handheld):** 1707 `mem dbg` samples (about
  30 minutes of play plus one sleep). The game heap high-water stays at 1685K
  of 8192K, the file cache at 1259K and newlib at 144435K; after the opening
  the heap sits at 1334K to 1398K. Flat, but far short of 2 hours, so the
  done-when is not met.
- [x] **P5.2 Performance pass.** Profile GTE/GPU emulation and the
  drawing path on handheld clocks. Decide software vs. a GL renderer.
  The frame-time check that used to be the P4.2 done-when lives here.
  **Owner-accepted milestone (2026-10-15, #160/#168):** with `gpu-tex`,
  the owner reports the battle sky fixed and battle running above 60 fps on
  both US and EU discs; the field was already about 60 Hz. This owner check
  is the basis for the P5.2 tick. See
  [P5.2_BATTLE_GPU.md](P5.2_BATTLE_GPU.md).
  **Original numeric benchmark subcriterion — follow-up, not measured:**
  no frame over 16.7 ms in field and battle benchmarks, docked and handheld.
  The owner report does not split the per-mode numbers, and this entry does
  not claim that the exact benchmark was met.
  Open follow-ups (not blocking the tick): MonMon's eye artifact, geometry
  in front of the camera during the idle orbit, FPS drop on Port options
  selection change, and the PSY-Z `KernelInit` sync-object warning.
  **Menu follow-ups (2026-10-15 to 2026-10-23):** the Port options screen got
  a CPU dirty-region cache (PR #185), the owner finds the menu "much more
  responsive" (2026-10-22), and the rc7 OLED run (US, 2026-10-23) shows
  `gpu-tex` as the default battle renderer with good performance (`soft=0` in
  all 1707 windows). No per-mode frame-time numbers, so the benchmark
  subcriterion above stays a follow-up. Details:
  [P5.2_BATTLE_GPU.md](P5.2_BATTLE_GPU.md#history-moved-from-roadmapmd-2026-10-24). Older history
  (preview.7-.26, mesh approach A, flat-blue root cause):
  [P5.2_BATTLE_GPU.md](P5.2_BATTLE_GPU.md#history-moved-from-roadmapmd).

- P5.2 follow-up, partner-view portrait transparency in `gpu-tex` (#229,
  merged, in preview.31): fixed in the Switch backend (mask bit on while the
  partner model draws). Root cause:
  [P5.2_BATTLE_GPU.md](P5.2_BATTLE_GPU.md#partner-view-portrait-transparent-in-gpu-tex--root-cause-and-fix-2026-10-24).
  Ryubing US and EU before/after done. OLED check pending (Kotemon, first
  Kuwagamon fight, later battles).

- [x] **P5.3 Save handling.** The port's own save on owner storage, plus
  sleep/exit safety. The running game does not import or export emulator
  memory cards. That interchange is an external tool, P9.6.
  Done when: a save this port wrote loads after a clean exit (including
  HOME) and a relaunch on hardware.
  **Done (owner OLED, 2026-10-15, #171):** the owner saved, closed the app
  from the Switch HOME menu with X, reopened it from hbmenu, and loaded the
  save with Continue; the owner confirmed it works on both the US and EU
  discs.
  **Remaining work moved to P10.2:** sleep/focus-loss flush and the
  time-delta clamp. HOME pressed in the middle of a write section was not
  tested on its own; P10.2's smoke covers it.
  History (flat save host, preview.11–.20 OLED passes, HOME quit flush,
  bar pace): [P5.3_FLAT_SAVE.md](P5.3_FLAT_SAVE.md#history-moved-from-roadmapmd).
- [ ] **P5.4 Error reporting.** Crash handler that writes a log + last
  mode/stage id to SD; assert overlay.
  Done when: a forced fault produces a readable report.
  **State (2026-10-24):** in progress, not ticked. The done-when is met on
  the OLED (US and EU, `--nxlink --force-crash` build, 2026-10-23): a real null
  write gives a full `crash_report.txt` and a `CRASH` line in `dw3.log`, and
  `tools/symbolize_crash.py` resolves it. Not ticked only because the
  objective also names the assert overlay. Report PR 2 is merged and tested in
  Ryubing (US and EU): a next-boot crash notice, and `abort`, assert,
  `exit`, `diagAbortWithResult` and `fatalThrow` routed into the same report.
  #231 (merged, in preview.31): the notice stays up to 60 s with a countdown,
  A dismisses it, and it ends on HOME.
  **Remaining (owner, OLED, US and EU):** the PR 2 hardware check in
  [P5.4_CRASH.md](P5.4_CRASH.md#pr-2-owner-oled-check): a forced assert
  build, then the normal build twice (notice once, then none). P5.4 is ticked
  after that. Releases publish `dw3-switch-elf.zip` (see
  [RELEASES](RELEASES.md#publishing-a-release)). Owner crash reports from
  preview.26: [PREVIEW26_CRASH_RECORDS.md](PREVIEW26_CRASH_RECORDS.md).
  Kernel-path analysis: [P5.4_ERROR_REPORT.md](P5.4_ERROR_REPORT.md). The P5.2
  nxlink HUD (`DW3_NX_DEBUG_OVERLAY`) is not a substitute for this task.
  Dated history: [P5.4_CRASH.md](P5.4_CRASH.md#history-moved-from-roadmapmd-2026-10-24).
- [ ] **P5.5 Stage sweep on hardware.** P3.8 again on the console.
  Done when (proposed, from P3.8): the P3.8 loadable stage set loads and
  returns on hardware, US and EU. P6.20's warp menu would help.
  **Known crash fixed (2026-10-22):** the new-game walk to the Digital
  World bridge gate ("Oh yeah! I need my partner", WSTAG210 event 0x1E)
  faulted in `FIELDSTG_drawBalloon` (preview.28 and .29; reproduced on the
  PC host US and EU and on Ryubing US and EU); patch `0058`, regression scenarios
  `us/eu-bridge-partner-gate`. See
  [BALLOON_NULL_ACTOR.md](BALLOON_NULL_ACTOR.md), which also holds the audit
  of the other places where a NULL from the PS1's readable low RAM could
  fault a 64-bit host. Patch `0059` guards the two that script data can in
  principle reach (actor mark draw, camera follow target) and adds a
  once-per-site `null-guard hit` log in dev builds; none of them fires in
  the shipped-data scan, the rest were left (dead code, player/singleton
  lookups, or a call through NULL that would crash the PS1 too).
  **Second NULL read fixed (2026-10-23, owner's preview.30-rc6 OLED crash
  on opening Items):** the field menu's item list drew the item panel's icon
  for `getItem(0)` (NULL) for two frames on every first open of a non-empty
  list; patch `0069`, hook `0070`, scenarios `us/eu-items-screen[-normal]`,
  details in [BALLOON_NULL_ACTOR.md](BALLOON_NULL_ACTOR.md#patch-0069-items-screen-null-item-preview30-rc6-oled-crash).
  Present since at least preview.29. Needs an OLED check of the Items screen
  in the next release candidate. **Confirmed on the OLED (2026-10-23,
  preview.30-rc7, owner, US):** the Items screen no longer crashes and items
  work (#201, patch `0069`). The 103-frame Instant page after each battle is
  the area name banner and is explained in P6.18 ([RELEASES](RELEASES.md)).
  **Bridge gate confirmed on US hardware (2026-10-23, preview.30-rc7, OLED
  handheld, owner; patch `0058`):** the New Game walk to the bridge gate no
  longer crashes. EU had passed it on the preview.29 OLED run, so both discs
  are now checked on the console
  ([BALLOON_NULL_ACTOR.md](BALLOON_NULL_ACTOR.md)).

Gate: start → first boss on hardware, both versions.

**Gate status (owner, 2026-10-24): not met.** The owner has not yet tested
start → first boss on hardware, so the gate stays open. The check is in
[Pending OLED checks](#pending-oled-checks).

## Phase 6 — Polish and options

Starts only after the Phase 5 gate (playable port on hardware). Localization
is an explicit project goal in this phase, not during bring-up. P6.5 is
the exception: packaging was done early because the owner asked. That
tick does not start Phase 6.
In practice P6.1, P6.2 and P6.9 also have early work at the owner's
request (marked "in progress" in the
[status table](#status-at-a-glance)). That does not change the Phase 5
gate, which stays open.

- [ ] **P6.1** Integer scaling / aspect options; optional widescreen
  where the GTE projection allows.
  **State (2026-10-24):** in progress. The Port options Widescreen toggle
  (default ON, or `DW3_WIDESCREEN=1`) draws side gutters outside VRAM on PC
  `sdl3_gpu` and the Switch renderer: 16:9 in battle and in the field (a fifth
  tile column in 3 host tile slots). Integer scaling is untouched. Dated
  history, patch by patch: [P6.1_WIDESCREEN.md](P6.1_WIDESCREEN.md#history-moved-from-roadmapmd-2026-10-24).
  **16:9 on main:**
  - Field, battle scene and fades: patches `0047`-`0053`; the 16:9 field
    crash of preview.26 EU is fixed by `0057` (#158).
  - Area name banner `0071` and technique backdrops `0072`: owner-verified on
    the OLED, US, rc8 (EU still to do). The encounter spiral's black 16:9 around a
    4:3 swirl was what the owner saw on the OLED (rc8); the swirl itself spans
    16:9 since `0175`.
  - Results screen `0074` (#213); Items menu dissolve `0075` (#218); field
    menu screens re-anchored to the 16:9 edges `0076` (#219).
  - Title menu `0085`, news caption bar `0086`, Digimon Online registration
    screen `0087` (#224).
  - Map screen `0080`, results header `0081`, memory card slot picker `0082`
    (#240).
  - Card album and deck editor `0101` (hook `0100`), card shop `0102`, gym
    `0103`, Digimon Lab `0104` (branch `fix/p6.1-ws-cards`, PR open).
  - Everything from `0074` on is in preview.31 and pending the OLED check (US
    and EU); see [Pending OLED checks](#pending-oled-checks).
  **Still 4:3, in order** (source: the 2026-10-24 screen audit, summarised in
  [P6.1_WIDESCREEN.md](P6.1_WIDESCREEN.md#remaining-43-screens-audit-2026-10-24)):
  - [x] **Card album** (`STCRDABM`) and **deck editor** (`STCRDDEK`): patches
    `0100` (test hook) and `0101`; OLED pending.
  - [x] **Gym** (`STGTRAIN`, `0103`), **Digimon Lab** (`STGDGLAB`, `0104`),
    **card packs shop** (`STCRDSHP`, `0102`, entry point unconfirmed): done on
    the replays, US and EU ids in `port/options/widescreen.c`; OLED pending.
    Open: the Lab's chart, entry and skill screens' left panels, the Open
    Booster screen (not captured).
  - [x] **Area-change iris and grid cover** (`FIELDSTG_closeField`): patch
    `0115`; the view closes from 320+2*gutter columns centred on the player
    and the grid covers the whole picture. PC US and EU replays
    `us/eu-ws-door-iris`; OLED pending.
  - [x] **Encounter spiral** (`FIELDSTG_playBattleTransition`): patch `0175` +
    PSY-Z `dw3-gutter-capture` (`3208f31`, `Psyz_VideoCopyGutters`). The gutters
    are not in VRAM, so PSY-Z copies them beside the swirl's image (a 426-px
    image) and each tile row gets gutter-width tiles that move with its first and
    last tile; the spiral keeps its order and frame count. PC US and EU replays
    `us/eu-ws-battle-spiral` (turbo 4) and `-1x` (turbo off, same pictures);
    widescreen off unchanged; OLED pending (B12, at turbo off, 2x, 3x and 4x).
  - [ ] Low priority: card battle, country select (EU only), and the cosmetic
    cut ends (item shop and memory card panels, inn menu right panels, field
    dialogue windows).
  **Other open sub-items:**
  - [ ] **Items menu (16:9):** layout and open transition are `0075`/`0076`,
    pending OLED. A usability redesign is a separate item: **P6.33**
    ([design proposal](P6.33_ITEMS_MENU.md)).
  - [x] **PC `sdl3_gpu` only:** an empty drawing area (the 0x0 clip
    `closeField` leaves on the map layer) draws nothing. Fixed in PSY-Z
    `dw3-empty-scissor` (`e3a7e83`, [PSYZ.md](PSYZ.md#empty-drawing-area-sdl3_gpu));
    4:3 replays `us/eu-door-iris43`. The Switch raster was already right.
  - [ ] **"Slow" transitions:** needs an OLED nxlink log across a door and
    into a battle; a shorter banner hold is the P6.18 owner's call.
  - [ ] **EU OLED pass** for the banner and backdrops, and a look for other
    display-wide band layers that now draw into the gutters.
  - [ ] The black hole in the middle of the technique magic circle (Ryubing
    and PC, same on main): look at it on the OLED first.
  - [ ] **preview.31-rc1 OLED report (owner, EU): "leaving the Underground
    Path the water texture is wrong until the screen scrolls down",
    fixed, pending OLED (US and EU).** Not a renderer or VRAM / CLUT
    problem: a map tile of the streets by the Cargo Tower (WSTAG202) counts 6
    sprites in a depth and patch `0057`'s clamp made the next two depths read
    garbage, which drew huge sprites over the canal while that tile was in
    view (patch `0111`; test hook `0110`). Replay scenarios
    `us/eu-underground-exit`. Details: [FIELD_TILE_SPRITES.md](FIELD_TILE_SPRITES.md).
  Details and evidence for each item:
  [P6.1_WIDESCREEN.md](P6.1_WIDESCREEN.md#open-transitions-2026-10-23) and
  [P6.1_WIDESCREEN_MENU_AUDIT.md](P6.1_WIDESCREEN_MENU_AUDIT.md) (partly
  stale, see the note in the doc).
- [ ] **P6.2** PAL/NTSC and EU/US selection at boot; language from EU
  `CNTY_SEL`.
  **Status (2026-10-14, language part):** `CNTY_SEL` linked as
  `dw3_p62_cnty_sel` (US and EU tables); EU host boot starts there (patch
  `0055`) and the choice sets `LANGUAGE` before the opening and title.
  **Owner OLED feedback (2026-10-14, EU preview.26):** language selection
  works. Restart from the main menu after choosing a language leaves the
  title menu drawn above the language-selection screen (see P6.9; fixed in
  #162, restart now returns to a clean `CNTY_SEL`). This
  validates the tested language path only; PAL/NTSC and disc selection remain
  open, so P6.2 is not complete.
- [ ] **P6.3** PGXP-style vertex precision from PSY-Z if it stays stable.
  Done when: TBD (owner).
- [x] **P6.4** Fast-forward / frame-skip toggles for grinding (optional).
  **State (2026-10-24):** done. Owner confirmed on the OLED, 2026-10-24.
  R3 toggles 2x/3x/4x fast-forward in field and battle; the speed (or OFF) is a
  Port options row. The Switch skips presents; PC paces at speed x base rate.
  The field menu screens still run at 1x. Scripted scenes are P6.22. The battle
  report (`STFGTREP`) is the turbo gate's third mode file since #223; its check
  is under P6.18 in [Pending OLED checks](#pending-oled-checks). Dated history
  and numbers:
  [P6.4_FAST_FORWARD.md](P6.4_FAST_FORWARD.md#history-moved-from-roadmapmd-2026-10-24).
  Done when: PC and Switch (Ryujinx, then hardware) show it on both the US
  and EU discs, and it returns cleanly to normal speed.
- [x] **P6.5** Release packaging: `.nro` + `README` for own-disc setup;
  never ship data.
  Done when: a deliberate GitHub Release (`workflow_dispatch` or a `v*`
  tag) attaches the CMake game `.nro` (`build/switch/dw3-switch.nro`,
  `source/boot_main.c` calling `dw3_game_main`) and
  `platforms/switch/release/README.txt`, and those assets contain no
  disc, BIOS, or extracted game files.
  Ticked on `v0.6.5-preview.1` (2026-10-11). No release ships game data;
  EU builds are attached from preview.25 on, per tag. Current list and
  do-not-install tags: [RELEASES.md](RELEASES.md). History, workflow notes
  and branding: [P6.5_PACKAGING.md](P6.5_PACKAGING.md).
- [ ] **P6.6 Localization goal.** Record that translation / localization
  comes after the playable port (Phase 5): language packs live in the
  owner runtime data layout; remap / charset glue in `port/`; host
  (`platforms/*`) only for homebrew chrome. No gettext-style layer; no
  game assets in git. Short note in `docs/` when the approach is chosen.
  Done when: the note exists and this section is the roadmap source of
  truth for i18n scope.
- [ ] **P6.7 Language packs (PT-BR first).** Tooling and/or pack steps so
  the owner can replace (or remap) message tables and, if needed, font
  glyphs for Brazilian Portuguese on top of the P1.6 layout, preferably
  against the EU build that already indexes text by `LANGUAGE`.
  Done when: a short playthrough (title → field dialogue) shows PT-BR
  with the owner's pack and no shipped assets in the repo.
- [ ] **P6.8 Extra locales.** Same pack mechanism for other languages;
  selection via existing EU `LANGUAGE` / `CNTY_SEL` slots or a
  `port/cdfs` id remap without rebuilding the `.nro` for each locale.
  Done when: a second owner-supplied language switches at boot or
  country select and renders in-game text correctly.
  Overlap: P6.7 is the first case of this mechanism (see Backlog notes).
- [ ] **P6.9 Restart-game hotkey.** Host button combo that resets the
  running game back to its boot/title flow **in-process**, without
  quitting the `.nro` to Sphaira/hbmenu or relaunching the console.
  Re-enter the existing boot path (`dw3_game_main`) rather than a second
  init sequence, and flush the P5.3 saves first: a restart must not
  silently delete or modify save data, and is not a card format. Route
  the combo through the Phase 9 option registry like P6.4; keep the
  policy in `port/`, not in `platforms/switch/` (deliberately not
  implemented during bring-up).
  Done when: the hotkey returns to the boot/title flow without
  relaunching Sphaira (PC and hardware), and a save written before the
  restart still loads afterwards.
  **State (2026-10-24):** in progress. Implemented: an opt-in Port options
  setting (OFF by default, saved since P9.4); the PS1 logical chord
  L1+R1+Start+Select runs shared `port/` policy, flushes P5.3 dirty saves
  (cancels if the flush fails) and resets `GAME` state and the active
  mode/overlay inside `dw3_game_main()`. Owner OLED (EU, preview.26): the
  hotkey works; the EU language-select overlay bug it showed is fixed in #162
  (Ryubing US and EU). **PC replay validation (2026-10-25, US and EU, 14
  scenarios, [P6.9_RESTART.md](P6.9_RESTART.md#2026-10-25-pc-replay-validation-us-and-eu)):**
  restart before a battle, after a won battle and inside a battle (the chord
  is allowed in every mode), after an in-game save, and a save written before
  the restart loads with Load Game / Continue afterwards; a second New Game
  after a restart reproduces the first byte for byte. No bug found; the
  restart does not reseed the RNG (documented, not a leak). New test hook:
  patch `0127` (`DW3_START_MODE_EVERY`, on top of 0124). Still open: the OLED retest (chord,
  battle, save, Continue on hardware, US and EU) Ryubing passed for the
  New Game restart (US and EU); its battle and save cases need a baked-env
  replay NRO. Dated history:
  [P6.9_RESTART.md](P6.9_RESTART.md#history-moved-from-roadmapmd-2026-10-24).
- [ ] **P6.10 Hardware render outside battle (goal).** Move the field
  (`FIELDSTG` map tiles, actors) and the menus (field menu, Status, shops,
  memory card) from the software rasterizer to the GPU path, the way
  battle already draws in `gpu-tex` (P5.2,
  [P5.2_BATTLE_GPU.md](P5.2_BATTLE_GPU.md)). Goal note only, recorded
  early at the owner's request (2026-10-14); it does not start Phase 6
  and is not part of the Phase 5 gate.
  Why: the field already holds 60 Hz at 1x, but turbo (P6.4) has no
  headroom there. On `v0.6.5-preview.25` the owner saw about 70–80 fps
  with turbo outside battle against about 180 fps in battle. On Ryujinx
  turbo is CPU-bound and the CPU rasterizer dominates
  ([P6.4_FAST_FORWARD.md](P6.4_FAST_FORWARD.md)).
  Known risks: preview.9 sent field OT triangles to the GLES mesh and
  showed severe field artifacts (blue stripe column, rug blobs, missing
  walls, neon outlines) and lower FPS, which is why the mesh is gated to
  `FIGHTSTG`; the opaque black sky on the cliff/sky battle stage (CLUT
  index 0) is reported fixed under `gpu-tex` by the owner on the OLED, US
  and EU (2026-10-15, #160, #168), but no root cause or fix commit is
  recorded, so field and menu textures may still hit it; the P6.1
  widescreen patches (0047–0049, 0051–0053, the
  three extra host tile slots in VRAM, gutter fades) and layered panels
  drawn into small drawing areas (like the battle partner view) must keep
  working; and soft rendering must stay selectable for comparison.
  Done when: field and menus render on the GPU path on PC and Switch with
  US and EU discs, match soft on the P3.8 stage sweep and the P6.1
  widescreen checks, and field turbo gains measurably on the OLED.

- [ ] **P6.11 Internal resolution scaling (future goal).** Render the 3D
  geometry above the native resolution, the way the PC `DW3_PC_INTERNAL_RES`
  experiment does on PC ([platforms/pc/README.md](../platforms/pc/README.md)),
  with the Switch and EU/US discs both covered. Goal note only, recorded at
  the owner's request (2026-10-15); it does not start Phase 6 and is not part
  of the Phase 5 gate. Done when a scale setting is in the Port options, the
  GTE/3D path renders above native on PC and Switch with US and EU discs, and
  the 2D/UI layers and widescreen patches still match soft rendering.

- **P6.12–P6.35** (except P6.15, P6.17, P6.18, P6.20 and P6.22, which have their own entries) are goal notes, kept with their ids in
  [Backlog / future goals](#backlog--future-goals).

Gate: polish options from P6.1–P6.5 land as needed; localization P6.6–P6.8
may still be in progress when Phase 7 starts only if the Phase 5 playable
gate and the Phase 6 packaging baseline (P6.5) are already met. Prefer
finishing P6.2 (EU language / NTSC) before QoL parity work that assumes PAL
language slots.

**Proposed Phase 6 gate (owner, 2026-10-24: not adopted):** the
current gate says "land as needed", which is not a criterion. Proposal:

1. Phase 5 gate met (start → first boss on hardware, US and EU).
2. P6.1 widescreen passes on the OLED with US and EU (field, battle, and the
   menus in [P6.1_WIDESCREEN_MENU_AUDIT.md](P6.1_WIDESCREEN_MENU_AUDIT.md)),
   with soft rendering still selectable.
3. P6.2 language selection checked on the OLED for each of the 5 playable
   EU languages; PAL/NTSC either done or split into its own item.
4. P6.9 restart passes on the OLED with US and EU, including after a battle
   and after an in-game save, and a save written before the restart loads.
5. P6.4 turbo checked on the OLED with US, returning cleanly to 1x.
6. P6.5 packaging baseline (done) and the P6.6 localization note exist.

Not in the proposed gate: P6.3, P6.7, P6.8, P6.10, P6.11 and the backlog.

## Phase 7 — Community QoL and companion compatibility

Starts after the playable port (Phase 5) and the Phase 6 packaging /
host-options baseline. P7.1 is an early note. The phase has not started.
Goal: optional compatibility with the Digimon World
2003 community QoL ecosystem (combined Quality of Life patch and its
upstream mods; companion app when a protocol exists). Details and project
list: [COMMUNITY_QOL.md](COMMUNITY_QOL.md).

- [x] **P7.1 Inventory and boundaries.** Document the projects named with
  the QoL announcement (combined patch, Flawe, guiomatos Initial Pack
  Customizer, markisha64 NTSC/definitive-style work, DW3-DS-Android
  companion), what is disc-patch vs code, and that this repo never ships
  patches or ISOs.
  Done when: [COMMUNITY_QOL.md](COMMUNITY_QOL.md) exists and the README
  points at Phase 7 / that note.
- [ ] **P7.2 Patched EU disc as data source.** Document and extend the
  P1.5–P1.6 pipeline so an owner-applied EU patch (xdelta/PPF or
  equivalent) can be extracted, mapped, and packed like vanilla.
  Done when: a patched EU tree produces a runtime layout and the port
  reaches title/field on PC or Switch without obvious asset regressions.
- [ ] **P7.3 Binary-patch limits.** Expand the checklist in
  [COMMUNITY_QOL.md](COMMUNITY_QOL.md): which QoL changes ride on patched
  data vs which require native reimplementation because EXE/overlay
  patches do not load into the `.nro`. Seed inventory from the
  OpenWorld2003 launcher catalog (`source/*.json`) is already in that
  doc; still need a pass against a **pinned combined QoL patch beta**.
  **2026-10-15:** pass done against Harmony Beta v1.0 (r31); table in
  COMMUNITY_QOL.md, new goals P7.6–P7.14 below. Left open until the owner
  agrees the P7.4 subset.
  Done when: the checklist covers the combined patch feature list at a
  chosen beta version.
- [ ] **P7.4 Feature parity (native).** Reimplement agreed QoL systems
  against the decomp via `port/` / `port/patches/` (battle/progression,
  Share EXP, exploration/menus, starter/Fast Start, timing parity with
  P6.2), crediting upstream authors. Scope is the checklist from P7.1/P7.3,
  not every beta experiment at once.
  Done when: a short native playthrough covers that subset without a
  patched PS1 EXE.
- [ ] **P7.5 Companion (DS-Android) — optional.** If the companion
  documents a port-friendly interface (save export, IPC, or similar), add
  a thin bridge on PC and/or Switch host; otherwise mark the blocker in
  [COMMUNITY_QOL.md](COMMUNITY_QOL.md).
  Done when: a smoke test with the companion works, or the note records an
  explicit blocker. Does **not** block the Phase 7 gate.

- **P7.6–P7.14** are goal notes, kept with their ids in
  [Backlog / future goals](#backlog--future-goals).

Gate: owner-patched EU data path works (P7.2) and the agreed native QoL
subset from P7.4 is playable. Companion (P7.5) is best-effort.

## Phase 8 — Texture pack compatibility

Starts after the Phase 7 gate in the planned order (or earlier only if the
P1.6 runtime layout and a stable TIM/draw path already exist to validate
against). P8.1 is an early note. The phase has not started.
Goal: owners can overlay replacement textures without forking
the port or committing assets. Details: [TEXTURE_PACKS.md](TEXTURE_PACKS.md).

- [x] **P8.1 Goal note.** Document that texture packs live in the owner
  runtime data layout, resolve through `port/` (file id / path overlay),
  keep hosts thin, and never ship textures in git.
  Done when: [TEXTURE_PACKS.md](TEXTURE_PACKS.md) exists and the README
  points at Phase 8 / that note.
- [ ] **P8.2 Pack layout.** Define the on-disk convention (e.g.
  `texture_packs/<name>/` keyed by file id or mirrored paths) aligned with
  P1.6 and `port/cdfs`, with fallback to the vanilla runtime file when a
  pack entry is absent.
  Done when: a minimal owner pack replaces at least one visible texture on
  title or field (PC or Switch).
- [ ] **P8.3 Formats and scaling.** Document accepted formats (native TIM
  first) and rules for upscaled or alternate formats, including VRAM /
  emulation limits.
  Done when: [TEXTURE_PACKS.md](TEXTURE_PACKS.md) states what is supported
  for the gate vs deferred.
- [ ] **P8.4 Tooling — optional.** `tools/` helper to validate or pack a
  texture pack against `files_*.json` (report missing/extra ids).
  Done when: a `--check` mode exists. Does **not** block the Phase 8 gate.
- [ ] **P8.5 Modding API (future goal).** Dated 2026-10-24, owner request:
  "modding API". Goal note only, **nothing started, no code**. Generalizes the
  texture pack idea of P8.2 to every kind of owner-supplied replacement, so
  that P8.2 (pack layout), P6.7 / P6.8 (language packs) and P7.2 (patched EU
  disc as data) share one layout and one loader instead of three.
  **What exists and the hard limit (summary):** the game reads disc data by
  numeric file id through `port/cdfs`, so a file replacement is one lookup
  away. The overlays and the executable are linked into the NRO, so code and
  tables compiled into them do not come from the SD card: file-id replacement
  covers textures, maps, message files, sound banks and `.STR` movies, while
  data-table overrides need a `port/` hook. Scope ideas in cost order (mod
  folder, asset overrides, data-table overrides, load order and versioning, US
  against EU, later script hooks):
  [TEXTURE_PACKS.md](TEXTURE_PACKS.md#history-moved-from-roadmapmd-2026-10-24).
  **Constraints:** **never redistribute game data.** A mod ships as a diff or
  patch (for example xdelta against the owner's own file) or as wholly new
  content, never as copies of original files; a `tools/` helper applies the
  diff at install time, and the repo, the releases and this port never host
  mods. Off by default: with no mod folder the game is bit-identical to today
  (the P6.15 replays still match). The loader reads the SD card only, a mod
  cannot reach the save files, and every replaced id is logged.
  **Dependencies and risks (summary):** needs one id-to-path resolve step in
  `port/cdfs` that all readers use; P9.4 for a mods on / off row; a size check
  for larger replacements. Full list:
  [TEXTURE_PACKS.md](TEXTURE_PACKS.md#history-moved-from-roadmapmd-2026-10-24).
  Open questions (owner): keep P8.2 as its own item or fold it in as "asset
  overrides"; how far to go with data tables before script hooks; plain
  folders or an archive (zip) on the SD.
  Done when (proposed): a documented `mods/` layout and manifest, one example
  mod per kind (a texture, a string, a table row) loads on the PC and the
  Switch build with US and EU, the loader reports conflicts, the default is
  bit-identical, and the modder docs (P6.23) describe it.
  Related: P8.2 to P8.4, P6.7, P6.8, P7.2, P6.23, P9.4. First mod (2026-10-28,
  design note): [MOD_MAP_UPSCALE.md](MOD_MAP_UPSCALE.md).

Gate: P8.2 works with an owner-supplied minimal pack. P8.4 is best-effort.
P8.5 is a future goal outside the gate.

## Phase 9 — Port options menu

Starts after the Phase 8 gate in the planned order (or earlier once at
least one non-vanilla port feature exists to toggle). P9.1 is done; P9.2
entry and return are validated on PC and OLED (see
[PORT_OPTIONS.md](PORT_OPTIONS.md)). Goal: a title / main menu entry that exposes
toggles for **every port-added feature** that is not part of the original
PS1 game, with vanilla-safe defaults and persistence on owner storage.

- [x] **P9.1 Goal note.** Document scope (port-only toggles), layering
  (`port/` registry + owner config file; hosts stay thin), and that
  defaults preserve original PS1 behaviour.
  Done when: [PORT_OPTIONS.md](PORT_OPTIONS.md) exists and the README
  points at Phase 9 / that note.
- [x] **P9.2 Menu entry and shell.** Add a port options entry on the
  title / main menu that opens and closes a dedicated screen without
  breaking the vanilla title flow (PC and Switch). Hook: `PORT_HOST`
  glue on `STDWTITL` `runOptions` + shell in `port/options/` (see
  [PORT_OPTIONS.md](PORT_OPTIONS.md) Entry investigation).
  Validated on PC (2026-10-12) and on the OLED (preview.20, 2026-10-12):
  the title row opens Port options and returns cleanly. Open follow-ups:
  Start/Continue text glow (owner wants the Options style), FPS drop when
  the Port options selection changes (P5.2; EU #170 owner test improved but
  still slow; dirty-region cache now on PC and Switch, OLED A/B open, see
  [composition plan](MENU_COMPOSITION_PLAN.md)),
  persistence (done in P9.4, OLED pending). History:
  [P9.2_MENU.md](P9.2_MENU.md).
  **Title credits (2026-10-23):** the EU title now shows the same native
  three-line credits as the US one, in all seven `LANGUAGE` banks (patch
  `0073`; the disc's own sprites stay as the fallback when the overlay
  cannot draw the footer). The credit sprites of the US disc and of every EU
  bank were composed from VRAM and are byte-identical, so no credit text is
  lost and EU needs no lines of its own; the third native line now keeps the
  original `Co.,Ltd` spelling. Transcription table:
  [TITLE_MENU_DESIGN.md](TITLE_MENU_DESIGN.md#title-credits). Validated on
  PC and in Ryubing (US and EU). **OLED (2026-10-23, preview.30-rc8,
  owner):** the credits appear as expected, verified on the OLED; the owner
  did not say handheld or docked, and which EU languages were looked at is
  not recorded.
  Done when: the screen is reachable from the main menu and returns
  cleanly to it (PC + Switch; TTB print on PC).
- [x] **P9.3 Toggle registry.** Maintain a registry of non-native features
  (Phase 6–8 items and later), each with a default equivalent to stock
  behaviour (or documented exception).
  Done when: flipping a stub or real flag from the menu has a documented
  effect (safe no-op allowed for not-yet-wired features).
  **Status (2026-10-13):** shared registry has Widescreen and Fast-forward
  safe no-op stubs plus the real Restart hotkey, OFF by default.
  Fast-forward became a real row on 2026-10-14 (P6.4), Widescreen a real
  toggle the same day (P6.1); both were session-only until P9.4.
  Its chord and flush behavior are documented in
  [PORT_OPTIONS.md](PORT_OPTIONS.md) and [P6.9_RESTART.md](P6.9_RESTART.md).
  P9.5 wiring of remaining real features remains open.
- [x] **P9.4 Persistence.** Load/save the option file under the owner
  runtime layout (PC directory / `sdmc:/switch/dw3/…`); values survive
  process exit.
  Done when: a toggled value remains after restarting the host or
  returning from hbmenu.
  **State (2026-10-24):** done. Owner confirmed on the OLED, 2026-10-24.
  Implemented: `port_options.cfg` (key=value, `version=1`) in each region runtime
  root, loaded before any flag is read and saved on every change. PC and
  Ryubing validated on both discs. Format and rules:
  [PORT_OPTIONS.md](PORT_OPTIONS.md#persistence). Dated history and evidence:
  [PORT_OPTIONS.md](PORT_OPTIONS.md#p94-history).
  Blocks: P6.12, P7.6-P7.10 and a persisted P6.9 setting (see
  [Backlog](#backlog--future-goals)); P6.17 and P6.18 add rows on top of it.
  Dated history and evidence: [PORT_OPTIONS.md](PORT_OPTIONS.md#p94-history).
- [ ] **P9.5 Wire real features.** Connect registry entries to real port
  flags as Phases 6–8 land; mark unavailable items N/A in
  [PORT_OPTIONS.md](PORT_OPTIONS.md) rather than hiding the menu.
  Real so far: Widescreen, Fast-forward, Restart hotkey and (2026-10-22,
  P6.17) Music volume and SFX volume, (P6.18) Encounter rate and Text speed.
  Done when: every shipped non-native feature is either togglable here or
  explicitly documented as N/A. Evolves with the roadmap; does **not**
  require all of P6–P8 complete for the Phase 9 gate.

- [ ] **P9.6 External save converter — later.** A tool under `tools/`, not
  linked into the game, converts between this port's save file and a raw
  PlayStation memory-card image. The port runtime does not do that
  conversion. This item does not block the Phase 9 gate and may land
  after Phase 9.
  Done when: the tool reads a port save and writes a 128 KiB card image,
  and the reverse, without the game calling it.

- [ ] **P9.7 Port options reachable in game from the pause / field menu.**
  Dated 2026-10-22, owner request after the rc4 OLED test: the Options screen
  opened only from the title, so a setting could not be changed while playing
  (a volume in the field, the Encounter rate before a walk, Widescreen on a
  battle). **State (2026-10-25): slice 1 (the field menu) and slice 2 (battle)
  built.** PC replays pass on US and EU; the owner's OLED checks are
  open ([Pending OLED checks](#pending-oled-checks)). The four needs of the
  note, as answered by slice 1 and slice 2 (design, tests and the OLED
  checklists: [PORT_OPTIONS.md](PORT_OPTIONS.md#options-in-game-p97) and
  [Options in battle](PORT_OPTIONS.md#options-in-battle-p97-slice-2)):
  1. **Pausing game logic** while the screen is open: in the field it is the field
     menu's own pause; the screen opens from inside the menu, the menu task hands
     it the pad and waits, and no input frame leaks on close. Music keeps playing.
     **Battle (slice 2, patch `0165`):** `main()` does not run the task list, the
     widescreen frame or `drawFrame` while the screen is open, so no task, timer,
     event, gauge or animation moves, nothing reads the pad, the display keeps
     the last picture, the random sequence is not advanced; the music (ticked by
     the vsync callback) keeps playing; the game's vsync clocks are put back on
     close. PC replays show the battle resuming pixel-identical to a battle that
     was never paused.
  2. **Drawing over the field**: the native overlay draws the Options window over
     the field menu on PC and Switch, in 4:3 and in 16:9 (the dim covers the
     gutters), the way patch `0123`'s Yes / No window already does. Over a
     battle: the same overlay over the held picture (PC SDL3 GPU, Switch; the
     battle is 4:3 on both); the Switch's mesh layer keeps its last picture
     while held (`nx_mesh.c`).
  3. **The entry point: (b), a new row in the game's own field menu**, **Options**,
     just above Return to title (patch `0160`, on `0123`'s approach), in the game's
     font in the five EU languages and English on US, with the six- to eight-row
     panel redrawn for the extra rows. The hotkey fallback (a) was not needed.
     **Battle (slice 2): (a), a button**: **Select / Minus** with no shoulder
     button held, polled by the command menu while it waits for the player (the
     battle has no menu the port controls). Documented in
     [HOTKEYS.md](HOTKEYS.md).
  4. **Live application**: every row except two applies at once; **Widescreen is
     greyed** ("Locked in game: change it at the title screen.", the map layout is
     set when a mode loads) and the **Cheats entry is greyed** (armed at the
     title by design). The Restart hotkey reads its value on every poll, so it
     applies at once. Persistence is the title's (`port_options.cfg`, written on
     every change).
  **Defaults chosen for the open questions (owner can change):** the entry is a
  field-menu row, not a hotkey; field menu only for now; the pause is the menu's
  own; Widescreen and Cheats stay title-only; changes save as today; the Restart
  hotkey applies live. **Slice 2 defaults (owner can change):** the battle entry is
  Select / Minus (the title's own Options key; no chord with a shoulder button
  is touched) and only at the top command menu, not during attacks or sub-menus;
  no on-screen hint; Battle cursor applies from the next turn's menu;
  Widescreen and Cheats stay greyed; fast-forward runs at 1x while the screen is
  open.
  Patch `0160` (the row, the pad hand-over, the panel rows); port code
  `port/options/field_options.c`, the in-game mode of `port/options/port_options.c`
  and the locked rows of the overlay art; replay scenarios `us/eu-field-options`,
  `-cheats` and `eu-field-options-fr/de/it/es`; host tests `test_field_options.py`.
  Ryubing (Switch build, US and EU, after #268's texture release): the Options screen and the
  Return to title window draw with no blank or garbage frame on a reopen after the release
  (`us/eu-field-options-nx`, `-release-nx`).
  Slice 2: patch `0165`, `port/options/battle_options.c`, `nx_mesh.c`; replay
  scenarios `us/eu-battle-options-ref`, `us/eu-battle-options` (the excursion's
  pictures equal the plain battle's pixel for pixel, 300 frames held) and
  `us/eu-battle-options-nx` (Ryubing); host tests `test_battle_options.py`.
  Done when: Port options opens and closes in the field **and in battle** on PC and
  Switch (US and EU), the game resumes exactly where it stopped, every row
  changes the game at once (or says why not), and the choice of entry point is
  written down. Left: the owner's OLED checks of both slices and a live
  Widescreen switch.
  Related: P6.17 / P6.18 rows, P9.2 (the title entry), P6.9 (the chord), P6.27
  (the row it sits above).

- [ ] **P9.8 Confirm / cancel layout option (future goal).** Dated
  2026-10-23, owner request after the preview.30-rc7 OLED test. Goal note
  only, nothing started. On a Switch the physical **A** sits where the PS1
  **Circle** is and **B** where **Cross** is, and the port follows the
  PlayStation positions today (Cross = B confirms, Circle = A cancels; see
  [HOTKEYS.md](HOTKEYS.md)). Add an Options row, **Button layout**, with two
  values: **PlayStation** (positional, the current behaviour and the
  default, so nothing changes by itself) and **Nintendo** (A confirms, B
  cancels, which swaps what the game sees as Cross and Circle). Needs:
  1. **The swap in one place**: policy in `port/` (the option registry and
     the key, saved with P9.4 in `port_options.cfg`), the physical A/B read in
     `platforms/switch/psyz_backend/nx_input.c`
     ([P4.4_INPUT_STATUS.md](P4.4_INPUT_STATUS.md)); PC keyboard and gamepad
     get the same row. It overlaps P6.12 / P6.13 (general remap and its
     submenu), which may subsume it; decide before building (see the
     overlaps list in the Backlog).
  2. **Hotkeys must not conflict**: [HOTKEYS.md](HOTKEYS.md) is the table to
     re-audit. The port chords use other buttons (R3, Select/Minus, the
     L1+R1+Start+Select restart chord, the Minus+L+R and Minus+ZL+ZR
     diagnostics), but the Options screen itself confirms with Cross **or**
     Circle, and the help lines and docs name buttons; each must read right in
     both layouts, and a new chord may not use A or B.
  3. **The glyphs follow the mapping**: if the screens show Switch button
     glyphs (P10.6), the confirm icon is the physical button that confirms in
     the chosen layout (B in PlayStation, A in Nintendo), and the same for
     cancel. X and Y keep their positions in both layouts.
  Done when: the row works on the Switch (and PC) with US and EU, the choice
  survives a relaunch, the default is bit-identical to today, and HOTKEYS.md
  and P4.4_INPUT_STATUS.md describe both layouts.
  Related: P6.12 / P6.13, P10.6, [SWITCH_FEATURES.md](SWITCH_FEATURES.md)
  (the "Confirm/Cancel layout" row).
- [ ] **P9.9 Options screen redesign (slice 1 done 2026-10-27).** **Status:**
  slice 1 is the list of groups (Display, Sound, Gameplay, Saves, and Cheats once
  its master switch is on) with a sub-screen per group, in the title menu's
  plaques, focus cue and lettering, on PC and Switch, on the title, in the field
  menu and in battle; Back returns to the list with the cursor kept; the
  registry indexes and the `port_options.cfg` keys did not change; every new
  label (and, since the sub-screens show them, the row labels, values and message
  lines) is in en / fr / de / es / it; a list of more than six items scrolls
  (not used yet). Details: [PORT_OPTIONS](PORT_OPTIONS.md#groups-and-sub-screens-p99).
  **Still open:** OLED validation (OLED_TEST_PLAN A5, A9 to A12); the Cheats
  screen keeps the old window (restyle it with the owner's answer on the dark
  tone); Controls (P6.12), Help (P6.30) and Walkthrough (P6.35) are hidden
  placeholders in `port_options_groups.h`; the open questions below are answered
  by defaults (Cheats group listed only with the master switch on; the plain dim of
  the title backdrop, deeper in game; no fallback to the old window). Original
  goal note follows. Dated 2026-10-24, owner
  request (Portuguese, a new Options layout in the visual style of the title
  menu). Goal note only, nothing started. **What exists (cited):** the Options
  window is drawn in the game's window style by `port/options/port_options.c`
  and the PC overlay ([PORT_OPTIONS.md](PORT_OPTIONS.md), "Eight rows", where
  the window was full at eight rows); the P6.20 Cheats row needed a ninth
  row (it became a sub-screen), P6.31 (auto-save) took the ninth row by
  tightening the pitch to 34 px on 38, and P6.12 (button mapping) wants one too:
  the window is full again at nine. The
  title menu's look is set in [TITLE_MENU_DESIGN.md](TITLE_MENU_DESIGN.md), the
  P6.1 16:9 title layout (patches 0085-0087) and `platforms/pc/src/title_menu_overlay.c`.
  **Scope:** the Options screen gets the title menu's identity (its backdrop,
  its label style, its focus cue and its spacing) and a layout that has room
  for more rows: groups or pages, or a list that scrolls, with P6.13 (mapping
  submenu) as the first sub-screen. One redesign solves both the full window
  and the P6.20 Cheats row.
  **First slice:** a layout mock of the title-style Options screen on PC, with
  the existing eight rows moved into it and no behaviour change; the rows keep
  their registry indexes (`DW3_PORT_OPT_*` and the `port_options.cfg` keys stay
  the same, so old files still load).
  **Constraints:** the registry and the keys do not change; the screen is drawn
  in both regions and every new label is in the five EU languages (EN, FR, DE,
  IT, ES) and US English; the dirty-rectangle cache of the Options canvas keeps
  working; the widescreen gutters are filled (P6.1); docs in the same change
  ([PORT_OPTIONS.md](PORT_OPTIONS.md)); tested on the OLED for US and EU.
  **Owner decision (2026-10-27): submenus.** Quote: "esse menu de opções pode
  ter submenus" (this options menu can have submenus). The groups are
  submenus, for example Display / Sound / Gameplay / Saves / Cheats / Controls /
  Help (a first list, not final; it replaces the Audio / Display / Gameplay /
  Tools sketch). Help is the P6.30 submenu, and the in-game walkthrough is the
  P6.35 submenu, so both land after this redesign.
  **Open questions (owner):** should the Cheats group be hidden unless a switch is on;
  should the title-menu backdrop be used as is or in a darker tone for
  readability; keep the old window as a fallback option until the owner
  approves the new one.
  Done when (proposed): the Options screen matches the title menu's look on US
  and EU, every existing row works unchanged, a ninth row fits (P6.20), and the
  file written by the old build still loads.
  Related: P6.20, P6.13, P6.17, P6.18, P9.7, P6.1, P6.27, P6.30, P6.31, P6.35.

Gate: P9.2 + P9.4 with at least one persistent toggle (stub or real).
P9.5 stays best-effort as features ship. P9.6 does not block the gate.

## Phase 10 — Native Switch features (residual)

Starts after the Phase 9 gate in the planned order (earlier only to wire
applet behaviours already required by P4/P5). P10.1 is an early note.
The phase has not started. Goal: inventory and finish
Switch-native UX that is **not** already owned by P4–P6 / P9 — see
[SWITCH_FEATURES.md](SWITCH_FEATURES.md). Do not re-implement scaling,
rumble, saves, turbo, or widescreen here; those stay in their phases.

- [x] **P10.1 Goal note.** Document the ownership split (P4–P6/P9 vs
  residual Switch items), homebrew caveats, and skip list (amiibo, touch
  in-game, save states, NSO cloud).
  Done when: [SWITCH_FEATURES.md](SWITCH_FEATURES.md) exists and the
  README points at Phase 10 / that note.
- [ ] **P10.2 Lifecycle hardening.** Confirm P5.3 focus/sleep clamp and
  flush behaviour on hardware; fill any gaps in `nx_os` / applet hooks.
  This holds the only open part of P5.3 (ticked 2026-10-15): sleep/focus-loss
  flush, the time-delta clamp, and HOME during a save write section.
  **Wiring (2026-10-16):** the event map and the guarded focus-loss/flush
  plumbing are in [P5.3_HOME_QUIT.md](P5.3_HOME_QUIT.md); hardware-unverified
  until the OLED smoke there passes.
  Done when: HOME sleep/resume and clean exit smoke tests pass with
  saves intact.
  - [x] Sleep and resume: owner confirmed on the OLED, 2026-10-24.
  - [ ] HOME pressed during a save write section (open on hardware).
    **Audit and fixes (2026-10-25, PC-tested, not yet on hardware):** every
    way the process can stop was walked through in
    [P5.3_HOME_QUIT.md](P5.3_HOME_QUIT.md#home-during-a-save-write-the-audit-p102-2026-10-25).
    Gaps fixed: a kill between the `remove` and the `rename` of a save left no
    `saveN.bin` (now a complete `.tmp` is promoted at boot, other `.tmp` files
    are deleted); the `.name` sidecar was written in place (now atomic); no
    `fsync` before the rename (added); a HOME/Quit/restart landing in the
    middle of the game's own save burst flushed a half-updated image (the
    exit now waits for the burst, up to 8 s, and the focus flush and restart
    skip it); cheat marks now recover a complete `.tmp`. Host tests
    `tools/tests/test_save_interrupt.py` inject a kill at each step. The
    OLED steps are in [OLED_TEST_PLAN](OLED_TEST_PLAN.md) G2.
  - [ ] A HOME (not power button) pause, and the focus lines on an nxlink run
  (open). P10.2 stays in progress until these are closed.
  **Sleep part confirmed by the owner (2026-10-23, OLED handheld, US,
  preview.30-rc7):** the console went to sleep and resumed without problems,
  with saves made before and after the sleep, and the in-game time counted
  only play time. The clean exit already worked (`applet exit: flat saves
  flushed` ends the rc7 log). **Observation in that `dw3.log`, not a failure:**
  it has no `applet focus:` line and one `frame dt` window of 543746177 us
  (about 9 minutes, unclamped) where the sleep happened. So the sleep did not
  go through the logged focus-loss path or the dt clamp; the game was fine
  because it counts frames, not wall time. Whether the focus signal arrives
  at all for an hbmenu applet, and so whether that code is needed, is for the
  follow-up ([P5.3_HOME_QUIT.md](P5.3_HOME_QUIT.md)). **Still open:** HOME
  pressed during a save write, a HOME (not power button) pause, and the
  focus lines on an nxlink run. Stays unticked.
- [ ] **P10.3 Presentation modes.** Docked vs handheld already in P4.2;
  verify operation-mode switches do not break the frame loop once the
  game is linked, and that P6.1 scale/widescreen options apply on both.
  Done when: dock/undock during title or field keeps a stable present path.
  Not covered by the rc7 OLED run (2026-10-23): it was handheld only
  (`handheld=1` in the log); docked mode is untested.
- [ ] **P10.4 Rumble polish — optional.** Intensity slider and event
  tuning on top of P4.4 `PadSetAct` → HD rumble; Lite-safe no-op.
  Done when: toggleable from Phase 9 or marked N/A if hardware lacks
  actuators.
  **Status (2026-10-16):** the `PadSetAct` → `hidSendVibrationValue`
  chain now exists (adapter actuator table + Stable handoff in
  `psyz_pad_host.c`, backend `platforms/switch/psyz_backend/nx_rumble.c`
  with the handheld/player-1 routes, `DW3_NX_RUMBLE=0` kill-switch and a
  `-DDW3_NX_RUMBLE_SELFTEST` boot pulse; host `pad_host_test` covers the
  handshake). **Not ticked:** nothing has run on a console, the
  small-motor→high-band mapping is unverified, and the Phase 9 slider is
  not wired. Design + smoke plan: `docs/P10.4_RUMBLE.md`.
  **rc7 OLED (2026-10-23):** rumble was not tested; still unverified on
  hardware.
- [ ] **P10.5 Extra applets — optional.** `swkbd` name entry, system
  language hint, CRT/sharp filters, multi-card / user-folder saves;
  Capture/album best-effort. Skip amiibo/gyro/touch-in-game.
  Done when: each item is either wired + toggled or explicitly N/A in
  [SWITCH_FEATURES.md](SWITCH_FEATURES.md). Does **not** block the Phase
  10 gate.

- [ ] **P10.6 Switch-style button glyphs (future goal).** Dated 2026-10-23,
  owner request after the preview.30-rc7 OLED test. Goal note only, nothing
  started. The game draws PlayStation button icons (Cross, Circle, Square,
  Triangle and the shoulder buttons) in its text and menus. The owner finds
  the EU disc's icons nicer than the US ones, but neither matches the Switch.
  Replace them with Switch glyphs, **A / B / X / Y** and, where the game
  shows them, **L / R / ZL / ZR / + / -**, in **both regions**. Approach to
  pick (not decided): a **texture replacement** through the P8 pack mechanism
  ([TEXTURE_PACKS.md](TEXTURE_PACKS.md), P8.2/P8.3, a pack that is optional and
  falls back to the disc) or a host **overlay** drawn over the icon; the first
  keeps one path for US and EU, the second can follow the layout at runtime.
  Not yet investigated: which sprites or font glyphs hold the icons in each
  disc, whether they sit inside text strings (so a width change touches the
  five EU languages), and where the game shows each one. The glyph for an
  action must follow the confirm/cancel layout of P9.8 (B confirms in
  the PlayStation layout, A in the Nintendo layout), so P10.6 should land with or
  after P9.8; it needs a Port option to turn it off (the default is the
  original icons). Original art is never added to the repo; Switch glyphs must be
  drawn for the project or be under a licence that allows it.
  Done when: with the option on, the icons shown in the field, battle, menus
  and shops are Switch glyphs on the US and EU discs, they match the layout
  chosen in P9.8, and the option off restores the original icons.
  Related: P8.2/P8.3, P9.8, P6.12, [HOTKEYS.md](HOTKEYS.md).

- [ ] **P10.7 NSP packaging (dropped).** **Dropped 2026-10-24, owner
  decision: "vamos ignorar o NSP então".** The game stays an NRO. No forwarder
  note, no takeover OLED check, no AGENTS.md change; the open questions below
  are closed unasked. The research stays in
  [NSP_PACKAGING.md](NSP_PACKAGING.md) in case the owner reopens it.
  Original note, dated 2026-10-24, owner request:
  "Vamos investigar a possibilidade de empacotar o jogo como NSP para
  instalar no Switch" (show the game as its own title on HOME). Goal note
  only, nothing started, no code. Routes, requirements and sources:
  [NSP_PACKAGING.md](NSP_PACKAGING.md).
  The findings, the recommended approach (stay NRO), the impact checks, the
  first slice and the closed open questions are kept in
  [NSP_PACKAGING.md](NSP_PACKAGING.md#history-moved-from-roadmapmd-2026-10-24).
  **Done when (proposed):** the owner picks a route and the AGENTS.md wording;
  if route (c), the release README documents takeover and the US and EU OLED
  check has a report with the method stated. P10.7 is a future goal outside
  the gate (now dropped).
  Related: P6.5, P10.2, P5.1, P5.4, P6.24.

Gate: P10.2 (+ P10.3 once present is live). P10.4–P10.6 are best-effort.

## Backlog / future goals

Goal notes recorded early at the owner's request. They are not part of any
phase gate and nobody owns them yet. Their ids stay stable (other docs and
PRs cite them); an item moves back into its phase when someone takes it.
Statuses are in the [table](#status-at-a-glance).

**Dependencies not stated in the entries:**

- **P9.4 (option persistence, done 2026-10-24) is the base for** P6.12 (remap
  survives a restart),
  P7.6–P7.10 (P6.17 added its two keys, `music_volume` and
  `sfx_volume`, and P6.18 `encounter_rate` and `text_speed`, in the same file). The persisted P6.9 restart setting is done
  with P9.4; the option file is `port_options.cfg` (add a key per new row).
- **Future P6.15 gameplay scenarios** may need P6.9 (restart to a known
  state) and P5.3 (a fixed save per case). The current scenarios use a fresh
  process per scenario and replay the scripted opening from boot (~16400
  frames) instead of loading a save, because no save is reachable by pad input
  yet; the game RNG is a fixed table seeded to 0, so no injectable RNG seed is
  needed.
- **P6.21** comes after P6.10 (field on the GPU) and P6.11 (internal
  resolution).
- P6.20's map warp would help P5.5 (stage sweep on hardware).
- Until P6.19 (JP disc) lands, "both discs" in these done-whens means US
  and EU.
- P7.12 waits for a frozen save format; P6.14 and P6.16 touch saves too.

**Overlaps to decide (owner; nothing merged or deleted here):**

- P6.18 vs P7.6 / P7.10: owner undecided (2026-10-24). Recorded recommendation:
  no merge. P6.18 is presentation pacing (text speed, banners, encounter rate);
  P7.6 and P7.10 are progression and training balance. The owner can revisit.
- P6.7 / P6.8 (Phase 6, not backlog): same pack mechanism; PT-BR is the
  first case of P6.8.

**Decided (owner, 2026-10-24):**

- P6.12 / P6.13: merged into P6.12; the mapping submenu is part of P6.12.
- P6.14 vs P6.16: P6.16 (full state) is the resume model now. P6.14 (in-game
  save slot resume) stays as a future option, not dropped.

### Phase 6 goal notes

- [ ] **P6.12 Button mapping (future goal).** Let the owner remap the
  PS1 pad buttons on the Switch and PC, in `port/` policy with the host input
  mapping kept in `platforms/`. The mapping lives in a submenu in Port options,
  with its own screen in the game's window style (the former P6.13, merged by
  the owner's decision, 2026-10-24). Goal note only, recorded at the owner's
  request (2026-10-15); it does not start Phase 6 and is not part of the Phase 5
  gate. Done when the submenu opens from Port options, lists each mapped button,
  returns with the mapping saved, and a remap survives a restart and works on
  the US and EU discs.
- **P6.13 Mapping submenu.** Merged into P6.12 (owner, 2026-10-24). Its goal
  and done-when are in P6.12; the id stays for references.
- [ ] **P6.14 In-game load / new save menu (future goal).** A load or
  new-save menu reachable during play, so the owner can switch save slots
  without returning to the title. It must flush P5.3 saves before a load
  and keep the save rules of P5.3 intact. Goal note only (2026-10-15); not
  part of the Phase 5 gate. Done when a load or new save from inside the
  game works without a restart, on the US and EU discs.
  **Owner, 2026-10-24:** P6.16 is the resume model now. This item stays as a
  future option, not dropped; the owner wants in-game load and new save
  possible later.
- [~] **P6.15 Regression replay tests.** Record pad input, replay it and
  compare screenshots at fixed checkpoints, on the US disc and the EU disc
  in all 5 languages. First version landed (2026-10-15):
  `tools/replay_tests.sh` replays frame-keyed pad recordings on the PC host
  under Xvfb (US boot to title menu; EU language select to localized title
  menu in en/fr/de/it/es) and diffs window and PS1-frame captures against
  references kept outside git. `DW3_REPLAY` / `DW3_REPLAY_RECORD` in
  `psyz_pad_host.c` (off by default). Runs are byte-identical on one
  machine; the game RNG is a fixed table seeded to 0, so no injectable seed
  is needed. Second version (2026-10-22): gameplay scenarios on US and EU
  (English): New Game through the scripted opening to control in the first
  field, a field walk with 4 map transitions, Load Game on an empty card,
  and a 17-20 min soak; separate crash / hang / memory verdicts (RSS
  growth) and overlay-marker checks; `--quick` keeps the old title-only run;
  two-phase scenarios share a memory-card dir (runner support, unit-tested);
  `--twice` proves determinism. The recording parser moved to
  `port/replay/` and the Switch build has a compile-time replay hook
  (`tools/build_switch_game.sh --replay`, `sdmc:/switch/dw3/replay.rec`,
  `replay shot`/`replay end` log lines) driven by `tools/replay_ryubing.sh`
  in Ryubing (own data roots, log-only verdict). The walk tooling
  found a deterministic null dereference in `FIELDSTG_drawBalloon` (US PC
  and Ryubing); it is the bridge-gate balloon bug fixed by patch `0058`, and
  `us-repro-balloon` plus `us/eu-bridge-partner-gate` are now regression
  scenarios that pass. Patch `0061` (2026-10-22) makes replay runs abort on
  a failed overlay restore and adds `us/eu-two-battles` (PC only: two
  battles with a technique each, started with the P3.3 dev hooks through
  the manifest's new `env` field). See `docs/P6.15_REPLAY_TESTS.md`. Still
  open: a battle reached by pad input from a new game (needs the partner
  Digimon data from the lobby counter past the bridge gate), a save/load round trip (Start menu locked
  in the hub), the other
  four EU languages past the title, movies, cross-machine tolerance, CI (needs
  the owner's discs; a nightly recipe is in the doc). The full goal remains
  open until the replay set covers battle and save/load.
- [ ] **P6.16 Suspend / resume (future goal).** Save the whole port state
  (RAM, VRAM, SPU, overlay state) on exit or sleep and resume at the same
  point. Separate from P5.3 game saves. Goal note only (2026-10-15). Done
  when exit and relaunch resume mid-play on US and EU.
  **Owner, 2026-10-24:** this is the resume model now (see P6.14).
- [x] **P6.17 Music / SFX volume.** Separate music and sound-effect volume
  rows in Port options, persisted with P9.4. Done when both sliders work on
  US and EU.
  **State (2026-10-24):** done. Owner confirmed on the OLED, 2026-10-24.
  Rows "Music volume" and "SFX volume" (0..10, default 10 = the original mix,
  bit-identical), separated inside PSY-Z's SPU mixer; XA movie audio follows
  Music volume. The BGM bugs seen while listening (loop after one play, music
  silent after a won battle) are fixed; see P4.3. The OLED steps are kept in
  [P6.17_VOLUME.md](P6.17_VOLUME.md#oled-check-for-the-owner). Dated history:
  [P6.17_VOLUME.md](P6.17_VOLUME.md#history-moved-from-roadmapmd-2026-10-24).
- [ ] **P6.18 Pacing options.** Encounter rate (1x, 0.5x, off), text speed
  and faster battle animations as Port options rows, off by default, saved
  with P9.4. Done when each toggle works on US and EU and is off by default.
  **State (2026-10-24):** in progress. Implemented: "Encounter rate" (OFF /
  0.5x / 1x / 1.5x / 2x, keys `encounter_rate`, patch `0062`) and "Text speed"
  (Normal / Fast / Instant, `text_speed`, patch `0063`), default 1x and Normal;
  scripted, story and boss battles are never scaled. PC checked on both discs.
  Faster battle animations are not done (the battle has no single time knob;
  R3 fast-forward covers it; may land as P7.4 items). Owner OLED: Instant text
  works (US, rc7); the 103/73/67/127-frame pages of the rc4/rc6 logs were the
  area name banner, not text (port fix in `pacing.c`, #203). #223 (merged, in
  preview.31): Instant shows the banner's names whole (delay 5 -> 2, so the
  banner after a battle closes in 126 instead of 160 game ticks on US, 140
  instead of 180 on EU) and the battle report runs at turbo speed (P6.4). A
  shorter banner hold is not done (owner's call). Still open: Fast, 0.5x and
  Off in play, EU on rc7, a dialogue that scrolls (patch `0068`), relaunch
  persistence. Steps:
  [P6.18_PACING.md](P6.18_PACING.md#oled-check-for-the-owner). Dated history:
  [P6.18_PACING.md](P6.18_PACING.md#history-moved-from-roadmapmd-2026-10-24).
  **Boundary with P7.6 / P7.10 (owner undecided, 2026-10-24):** no merge is
  recorded. P6.18 is presentation pacing (text speed, banners, encounter rate);
  P7.6 and P7.10 are progression and training balance. The owner can revisit.
- [ ] **P6.19 Japanese disc support (future goal).** File map, executable
  differences and boot for the JP disc, next to US and EU. Goal note only
  (2026-10-15). Done when the JP disc boots to the field on PC and Switch.
- [ ] **P6.20 Port debug / cheat menu in the Options screen.**
  First noted 2026-10-15 (money, items, map warp, off by default); widened
  2026-10-24 on an owner request: "a cheats submenu inside the options, to
  make testing easier". **PR 1 (first slice) is built** (2026-10-24, patch
  `0077`, section "PR 1 as built" below); the rest is still a goal note. The purpose is faster hardware testing (reach a battle, a map or a
  full bag without replaying hours of the game), not a player feature.
  AGENTS.md lists cheats as out of scope unless requested; this is the owner's
  explicit request, so the item is in scope under the constraints below.
  **Design background** (what exists, candidate cheats, first slice,
  constraints): [PORT_OPTIONS.md](PORT_OPTIONS.md#p620-history). Constraints in
  short: off by default; a build or config with every cheat off is
  bit-identical to today; decomp changes only as numbered `PORT_HOST` patches
  with a row in [PATCHES.md](PATCHES.md); persistence is the options file, never
  the save; no cheat writes a save by itself.
  **Owner decisions (2026-10-24, final; they replace the open questions):**
  (1) the Cheats menu ships in **every** build, release and nxlink, US and EU,
  behind a master switch that is off by default; (2) the labels are in all five
  EU languages (English on the US disc), made the way the Options screen makes
  its labels (pre-rendered lettering, procedure in
  [PORT_OPTIONS.md](PORT_OPTIONS.md#cheats-p620)); (3) a save written while a
  cheat was used is flagged "cheat used", and a **build-time option** makes a
  build without any cheat: CMake `DW3_CHEATS` (default ON),
  `tools/build_switch_game.sh --no-cheats` on the Switch, `-DDW3_CHEATS=OFF` on
  the PC. OFF compiles the feature out (no entry, no hook, no flag code) and
  the game is bit-identical to a build before P6.20. Releases keep the default
  (cheats included) unless the owner says otherwise
  ([RELEASES.md](RELEASES.md#building-without-cheats)). The earlier question
  (b), the Options screen opening only from the title, is unchanged: the first
  slice works from the title and its effects show once the game is running
  (P9.7 would open Options in game).
  **PR 1 as built (2026-10-24, patch `0077`, #236 merged, in preview.31):** on
  the Options screen Square (PC Z, Switch Y) opens the Cheats screen, Triangle
  goes back; the hint is drawn only while the master switch is on, so the
  Options screen is pixel-identical when it is off. Five rows: Cheats (master,
  `cheat_enabled`), Fill bag (one-shot, 99 of every usable item, weapon, armour
  and accessory, ids 43..359), Instant win (`cheat_instant_win`), Battle now
  (one-shot) and Cheat saves (read-only count of saves flagged by the sidecar
  `save<N>.cheats`). Details, code list and tests:
  [PORT_OPTIONS.md](PORT_OPTIONS.md#p620-history).
  **PR 2 as built (2026-10-25, patches `0118` to `0120`, branch
  `feat/p6.20-cheats-2`):** page 2 of the Cheats screen (Down from "Cheat
  saves" goes on to it; the title reads "CHEATS 1/2" and "CHEATS 2/2"):
  **Money** (OFF, +10,000, +100,000, MAX = 9,999,999, `0118`), **Warp** (OFF or
  one of 13 areas of the debug stage select, `0120`), **Item id** (43..359),
  **Item count** (1..99) and **Give item** (OFF / Ready, `0119`). All are
  one-shots armed at the title and applied by the field at its first idle
  frame (the moment the field menu may open: no event, menu or fade), used up,
  logged, and they set the save cheat marker; with the master switch off they
  do nothing, and turning it off disarms them. Warp leaves the field the way an
  exit slot does, for the area's first start point (what the stage select's
  `requestMode(mode, 0)` gives), so the game keeps its normal state: the field
  menu opens there and a save made there is a normal save (with the cheat
  mark). Labels are in the five EU languages and English. **Defaults chosen,
  owner can change:** the area list (Asuka City, Digimon Lab, Digimon Arena,
  Central Park, Wire Forest, Tyranno Valley, Chinlong City, Bulk Swamp, Suzaku
  City, Noise Desert, Byakko City, Snow Mountain, Genbu City: the cities, the
  Lab and Arena, and one random-battle area per region); the money amounts; the
  item picker offers only ids 43..359 like Fill bag (no key item, no card
  pack), shown as numbers because item names are game text the port does not
  draw (ids are in [PORT_OPTIONS.md](PORT_OPTIONS.md#cheats-p620)); Give item
  adds (at most 99) where Fill bag sets; the picks are not kept between runs;
  the area names are plain labels, not the game's own text. Details, tests and
  the OLED checklist: [PORT_OPTIONS.md](PORT_OPTIONS.md#cheats-p620).
  **Follow-ups (out of PR 1 and 2):** infinite HP / MP, max stats,
  digivolution unlocks, walk speed (all need the decomp fields to be located
  first); a chosen encounter id (`DW3_FORCE_ENCOUNTER`) instead of the random
  one; the "cheat used" mark on the in-game memory card screen (STGMCARD); item
  names on the picker; P9.7 (Options reachable in game: warp and money could
  then apply at once); the warp could also offer a chosen start point.
  **Still open (hardware):** the OLED checklists in
  [PORT_OPTIONS.md](PORT_OPTIONS.md#cheats-p620) (PR 1) and
  [page 2](PORT_OPTIONS.md#oled-checklist-page-2-owner) (PR 2), so the item
  stays unticked.
  Done when: the sub-screen works on the PC and the Switch build with US and
  EU (PC done, Switch compiles; OLED open), every cheat in the slice works and
  is logged (done, replays), the default is bit-identical to a build without it
  (done), the visibility choice is recorded (done), and PORT_OPTIONS.md and
  PATCHES.md describe it (done).
  Related: P9.7, P9.4, P6.15, P6.18, P7.9, P5.5, P3.8.
- [ ] **P6.21 60 fps interpolation (far future).** Interpolate camera and
  field movement in 30 fps sections. After P6.10. Goal note only
  (2026-10-15).
  Done when (proposed): camera and field movement in 30 fps sections
  present at 60 fps on PC and Switch with the US and EU discs, and the
  original 30 fps stays selectable.
- [ ] **P6.22 Turbo speed in scripted scenes.** Dated
  2026-10-23, owner request after the preview.30-rc7 OLED test (US disc,
  Switch OLED). Goal note only, **nothing started**; first numbers from the
  rc7 log below (the full table is in
  [P6.4](P6.4_FAST_FORWARD.md#new-game-opening-measured-from-the-rc7-log-2026-10-23)).
  The owner reports that on New Game the opening scenes are very slow even
  with turbo on: the scripted scenes (meeting the friends, talking to the
  attendants and receptionists, and so on). Turbo at 4x only raises the
  frame rate to about 70 FPS there, while it works well in battle. Goal:
  find what bounds turbo in these scenes and raise it so turbo reaches its
  target (4x is 240 game frames per second on the US disc, 200 on EU) in
  scripted event scenes and in similar situations: other event scripts,
  long walks and text-heavy scenes.
  **State (2026-10-24):** in progress. First measurement (rc7 log, US OLED,
  turbo 4x): the field and scripted scenes run at 68 game frames per second
  against 240 asked, the battle at 190; the field costs about 13.7 ms of CPU per
  game frame (battle about 4.2 ms) and the present is under 5 % of the window.
  rc8 Forest inn (HUD reading): 120+ fps in the bottom corner, about 80 at the
  top, so the cost follows what is on screen. The PC profile puts the CPU
  rasterizer at about 97 % of a field frame, and a pixel-identical PSY-Z
  rasterizer is 4-5x faster (#220; OLED check open). **Pass 2 (2026-10-25):** the
  frames turbo does not present are queued and mostly never drawn (85 % of the
  opening's primitives), pixel-identical in every presented frame: the raster
  per game frame drops another 3.7-5x on the PC (PSY-Z `dw3-soft-defer`; OLED
  check open). The candidates, the
  measure-first steps (a)-(c) and the A/B for the inn are in
  [P6.22_FIELD_CPU.md](P6.22_FIELD_CPU.md#history-moved-from-roadmapmd-2026-10-24). The port does not
  overclock (AGENTS.md, directive 7): the work is on the present path, the
  field-tile pace and the raster.
  Done when (proposed): a short doc records the measured bottleneck per
  scene, and with turbo at 4x the New Game opening scenes, a long field
  walk and a text-heavy scene reach the target rate (or the doc shows why
  not) on the Switch OLED with the US and EU discs, with no change in game
  state (the P6.15 replays still match) and a clean return to 1x.
  Related: P6.4, P6.10, P6.15, P6.18, P5.2.
- [ ] **P6.23 Documentation (future goal).** Dated 2026-10-24, owner request:
  "documentation". **Slice 1 built 2026-10-25** (PR open, not merged): the
  player guide [PLAYER_GUIDE.md](PLAYER_GUIDE.md) and the docs index
  [docs/README.md](README.md), linked from `README.md` and from the release
  `README.txt`; `tools/check_doc_links.py` checks the links, the anchors and
  that the index lists every doc. **Slice 2 built 2026-10-25** (PR open, not
  merged): [CONTRIBUTING.md](../CONTRIBUTING.md) (layout, builds, patch
  workflow, replay tests, PSY-Z pin, PR rules; links AGENTS.md, does not
  repeat it) and `tools/tests/test_doc_links.py`, which runs the checker over
  every markdown file outside `vendor/` so CI catches a broken link. The
  checker now keeps underscores in anchors like GitHub does (the
  `PSYZ.md#empty-drawing-area-sdl3_gpu` link was right) and skips links into
  an empty submodule checkout. Open: the modder docs. **Defaults chosen while the owner was away (owner can change):** the
  guide ships in the repo as `docs/PLAYER_GUIDE.md` and is linked from the
  README and the release `README.txt` (one line with the GitHub URL), with no
  separate site and no copy of the guide in the release bundle; the index is
  written by hand and checked by the script, not generated from the ROADMAP
  table; the guide is named `PLAYER_GUIDE.md`, not `GUIDE.md`; it covers a
  BIN and CUE dump only, as the setup docs do; the FAQ is not written yet.
  The survey below is the original goal note. **Gaps found by a
  survey of `docs/` (about 80 files), `README.md`, `AGENTS.md` and
  `platforms/switch/release/README.txt`:**
  - *No player guide.* The player-facing text is spread over the README
    ("Play it on Switch", "Controls"), the release `README.txt` (install, the
    own-disc setup, the OLED checklist of the preview),
    [HOTKEYS.md](HOTKEYS.md), [DISC_SETUP.md](DISC_SETUP.md),
    [RUNTIME_LAYOUT.md](RUNTIME_LAYOUT.md) and
    [REPORTING_ISSUES.md](REPORTING_ISSUES.md). No single page covers install,
    US against EU, every Options row, controls, turbo, saves, crash reports and
    an FAQ. The release README is rewritten per preview and mixes a tester
    checklist with the install steps.
  - *No docs index.* `docs/` has about 80 files named by phase id
    (`P6.4_FAST_FORWARD.md`, `RELEASE_PREVIEW20.md`, ...) and the README table
    lists about ten of them. The ROADMAP status table is the only complete map,
    and it is organized by item, not by what a reader needs.
  - *Contributors:* the rules are in AGENTS.md (written for AI agents, with
    Cursor Cloud specifics in the middle), the README "Contributing" section
    is two bullets, and there is no `CONTRIBUTING.md`. How to add an Options
    row, a `PORT_HOST` patch or a replay scenario is described only inside
    [PORT_OPTIONS.md](PORT_OPTIONS.md), [PATCHES.md](PATCHES.md) and
    [P6.15_REPLAY_TESTS.md](P6.15_REPLAY_TESTS.md).
  - *Modders:* nothing yet; see P8.5.
  - *Stale-prone text:* the README "Available now" list and the release notes
    repeat statuses that live in the ROADMAP table, and AGENTS.md still names
    preview.22 as the latest tagged preview.
  **Proposed set (to decide with the owner):** (1) a **player guide**
  (`docs/GUIDE.md`, linked first from the README): install, own-disc setup for
  US and EU, the Options screen row by row, controls and hotkeys, turbo, saves
  and backups, crash reports and how to send them, an FAQ; the release
  `README.txt` then keeps "what is new" and the checklist and links to the
  guide; (2) a **docs index** (`docs/README.md`) grouped by reader (player,
  tester, contributor, modder) and by phase, replacing the short README table;
  (3) a **contributor guide** (`CONTRIBUTING.md`, a short entry that points to
  AGENTS.md for the hard rules): build on PC and Switch, tests, the squash
  merge flow, how to add an Options row, a patch, a replay scenario;
  (4) the **modder docs** of P8.5 once that API exists.
  **Constraints:** the guide states only what is confirmed (it links the
  ROADMAP or the release notes for "unverified on hardware"); one fact has one
  home and the rest link to it; no game data, logs or captures are added, and
  screenshots stay limited to the owner-approved set in
  `docs/assets/screenshots/` (AGENTS.md). Repo docs stay in English; a
  Portuguese guide is a separate decision, tied to P6.7.
  Open questions (owner): where the guide ships (repo only, or also in the
  release bundle as text), and whether the index is generated from the ROADMAP
  table so it cannot drift.
  Done when: the guide, the index and `CONTRIBUTING.md` exist, the README links
  them, the release `README.txt` points to the guide, and a new tester can
  install the US or EU build and report a crash using only those pages.
  Related: P6.5, P8.5, [REPORTING_ISSUES.md](REPORTING_ISSUES.md).
- [ ] **P6.24 One build for US and EU (in progress: slices 1 and 2 built 2026-10-25, slice 3 / PR A 2026-10-28).** Dated 2026-10-24, owner
  request: "Build única pra US e EU". One `.nro`, and one PC binary, that runs
  either disc, instead of the separate US and EU builds (`--eu`, two `.nro`
  files, EU attached to releases by hand). **Owner priority 2026-10-28: top of
  the work order (Tier 0).** Survey, sizes and the options in detail:
  [SINGLE_BUILD.md](SINGLE_BUILD.md).
  **Slice 3 (built, PR A, [As built](SINGLE_BUILD.md#as-built-slice-3-the-dual-image-link-pc)):**
  the dual-image link, option (a), PC. `DW3_VERSION=both` builds the game side
  twice (`VERSION_US` / `VERSION_EU`, from the one patched `build/decomp-src`),
  packs each copy with `ld -r` into one object whose only global is its
  `dw3_game_api` table (`us_` / `eu_`; `tools/localize_region_obj.py`), renames
  the `dw3data_*` / `dw3bss_*` restore sections and their `__start_` / `__stop_`
  references per region, and links the host (PSY-Z, platform, cdfs, memcard,
  movies) once. The host reaches the game only through the table (24 entries);
  the host `#if VERSION_*` sites became `dw3_region()`. Region from `--region`,
  `DW3_REGION`, the runtime folder given, or the runtime present. PSY-Z
  overrides and `--wrap` still win (same link order), the overlay restore covers
  only the running region, `us` and `eu` builds are unchanged. +5.0 MB on the
  PC binary. Tests: the localize tool on two copies of one C file, the region
  resolver, CI builds `both` on every PR, and the P6.15 replays of both regions
  on the one binary (`--bin-both`, private copy of the references). **Open
  (PR B, Switch):** `tools/build_switch_game.sh --both`, one `.nro` (NACP
  "Digimon World 3 NX", nxlink "(nxlink)"), detection of the game data on the
  SD card (only US: run it, only EU: run it, both: the picker), the host
  picker, CI job and release asset, OLED checks
  ([OLED_TEST_PLAN G9](OLED_TEST_PLAN.md#g9-combined-p624-one-nro-for-both-regions-pr-b)).
  **Slice 1 (built, [As built](SINGLE_BUILD.md#as-built-slice-1)):** the pieces
  around the dual image, chosen because the dual-image link is the risky part
  and the picker is useful on its own. `port/region/` (probe, the decision
  table, the picker as a state machine with the 3 s countdown / D-pad cancel /
  Y always / Minus-to-ask rules, `region_default` and `last_region` in
  `port_options.cfg`), and a PC launcher (`platforms/pc/launcher/`,
  `tools/build_pc_single.sh`) that picks the region and starts the unchanged
  `dw3-pc-us` / `dw3-pc-eu`. One tree: boots it, no picker; none: the setup
  message; `--region` / `DW3_REGION`: forced, nothing written. Tests:
  `tools/tests/test_region.py` and the replay tier `single` (7 scenarios, only
  US, only EU, both with env, picker keys and countdown; captures equal the
  plain runs, 0 px differ). No decomp patch (0135-0139 unused), the separate
  builds and CI are unchanged. **Open:** the dual-image link (one binary), the
  Switch picker and auto-detection, CI/release on one `.nro`, disc images as a
  detection source, the Options row, an OLED check.
  **Slice 2 (built, [As built](SINGLE_BUILD.md#as-built-slice-2-the-switch-launcher)):**
  the Switch side of the same launcher approach. A small libnx `.nro`
  (`platforms/switch/launcher/`, `tools/build_switch_launcher.sh`, 241 KB, no
  game code) probes `sdmc:/switch/dw3/us` and `/eu` with `dw3_region`, draws a
  console picker when both exist (3 s, A, Y always, + quit, Minus at start to
  force it), and chain-loads `sdmc:/switch/dw3/dw3-us.nro` or `dw3-eu.nro` with
  `envSetNextLoad()`. One region: chain-loads at once; none: the setup
  message; no next-load support, a missing NRO or an `envSetNextLoad` error: a
  message, no crash. The game `.nro` files still open `sdmc:/switch/dw3/<ver>/`,
  so the path they are started from does not matter. Pure parts tested in
  `tools/tests/test_region.py`; Ryubing runs the launcher (setup, missing
  NRO, picker countdown to the no-loader screen) but cannot chain-load, so the
  hand-over is an OLED check. CI builds it as an artifact (job
  `switch-launcher`); not a release asset yet. The separate builds and the
  release flow are unchanged.
  **Survey and approach (summary):** the region is a compile-time choice
  (`DW3_VERSION`); the decomp has 731 region blocks and the `GameState` and
  `BattleFuncs` layouts differ, so a run-time region variable is not
  practical. Recommended: compile the game side twice into one image (about
  +2.7 MB, one `.nro` of about 12 MB), link the host once, pick the region at
  boot; the decomp and the patches stay untouched. First slice (PC, no visible
  change): `DW3_VERSION=both` builds one `dw3-pc` with both regions; it passes
  when the P6.15 scenarios of both regions match on that binary. Full survey,
  the rejected options and the effects on P6.15, P5.4, releases and P6.2:
  [SINGLE_BUILD.md](SINGLE_BUILD.md#history-moved-from-roadmapmd-2026-10-24).
  **Owner decisions (2026-10-24, final):** (1) saves are unified between US and
  EU: a save made in one region loads in the other (**P6.25**); (2) with both
  game images on the console the game asks at boot, with a short timeout
  (default = the last choice) and a stored "don't ask again" that can be changed
  later (the picker, below); (3) saves are bound to the Switch user profile that
  is playing, chosen in the Switch profile selector (**P6.26**). P6.25 and P6.26 are separate items and **P6.24 depends
  on both** (see their entries).
  **Boot picker (design, [SINGLE_BUILD.md](SINGLE_BUILD.md#boot-picker)):** only
  when both regions are found and none is forced; no image or one image: no
  picker. Host-drawn screen, **3 s** countdown that any D-pad press cancels,
  default = `last_region`, then A to boot, Y to toggle "always this one". "Don't
  ask again" is `region_default=ask|us|eu` in the global `port_options.cfg`; get
  the picker back by holding **-** (Minus) at launch, from an Options row, or by
  editing the file. `--region` / `DW3_REGION`, the replay builds and
  `DW3_MEMCARD_DIR` skip it with no extra frame, so the P6.15 recordings stay
  valid.
  **Order:** P6.25 slice 1 (converter and tests, PC) -> P6.25 store with the
  separate builds sharing it -> P6.26 -> P6.24 on the Switch. The PC slice of
  P6.24 above can start in parallel.
  Open questions (owner), the rest of the original list is answered: the
  picker's look and languages (plain host screen, English only, proposed); the
  hold-to-ask button; keep the separate `--eu` builds for development only or
  also in releases for a while; is a 12 MB `.nro` fine; the PC too; switching
  region without quitting (proposed: out of scope). More in
  [SINGLE_BUILD.md](SINGLE_BUILD.md#open-questions-for-the-owner).
  Done when (proposed): one `.nro` and one `dw3-pc` from one configure, built
  in CI; with only US or only EU data it boots that disc, with both it shows the
  picker as decided above; saves are shared between the regions and per user
  (P6.25 and P6.26 done); the US and EU P6.15 scenarios pass on it with the
  existing references; the crash report names the region and
  `tools/symbolize_crash.py` resolves both from the one `.elf`; the release
  ships one `.nro` (plus nxlink); checked on the OLED with each disc and with
  both on the card.
  Related: P6.2, P6.15, P6.19, P5.4, P6.5, P6.25, P6.26, P3 EU build
  ([P3_EU_BUILD.md](P3_EU_BUILD.md)).
- [ ] **P6.25 Cross-region saves (future goal).** Dated 2026-10-24, owner
  decision: a save made in the US game loads in the EU game and the reverse.
  **Slice 1 built 2026-10-25** (PR open, not merged): the converter
  (`port/memcard/save_convert.c`) and a one-way import on load: a port with no
  save of its own takes the other region's `saveN.bin` as a converted copy
  (original read-only, `saveN.import` marker with the converter version);
  saving stays native; refusals and corrupt sources write nothing. Tests: 35
  unit/host tests and four PC replay scenarios (`xr-us-to-eu`, `xr-eu-to-us`,
  `xr-eu-extra-us`, `xr-us-badsum-eu`). Open: the `.dw3s` container and
  store, merge into an existing card, two-way flow, OLED check. Defaults and
  risks: [CROSS_REGION_SAVES](CROSS_REGION_SAVES.md#as-built-slice-1). No decomp patch.
  **Survey and design done.** Full note with every difference and the
  test plan: [CROSS_REGION_SAVES.md](CROSS_REGION_SAVES.md). Checked against the
  decomp (`b9caa6c`) and one real US save.
  **Findings and design (summary):** the card image and the first 9,796 bytes
  of `GameState` are identical in US and EU; the flag area from 0x2644, the
  version byte (3 US, 4 EU), the slot checksum and the header title differ,
  and the game rejects a foreign slot with error 8. Design: a port-side
  container (`.dw3s`) with the EU layout as canonical data, converters at the
  `memcard_host` boundary, legacy saves copied in (originals never touched).
  The EU extra-chapter save has no meaning in US (proposed: error 8, bytes
  kept). Every finding, hard case and the design in full:
  [CROSS_REGION_SAVES.md](CROSS_REGION_SAVES.md#history-moved-from-roadmapmd-2026-10-24).
  **Useful before P6.24:** yes, the separate US and EU builds can share one
  store.
  Order: converter + unit tests with synthetic fixtures (PC) -> store behind a
  switch + import + host tests -> a preview of the separate builds sharing it ->
  P6.26 and P6.24 build on it.
  Done when (proposed): US -> EU and EU -> US round trips are identity over the
  saved bytes on synthetic fixtures for all flag bits and fields; a real US save
  and a real EU save each load on the other region's build on the OLED; the EU
  extra-chapter save is refused on US and survives untouched; the legacy import
  keeps the originals; replays unchanged.
  Related: P6.24, P6.26, P5.3 ([P5.3_FLAT_SAVE.md](P5.3_FLAT_SAVE.md)), P9.6, P6.2.
- [ ] **P6.26 Per-user saves.** **State (2026-10-27):** slice 1 built (design
  Order steps 1 to 3 adapted), Switch US and EU build, PC replays and pytest
  green, **OLED check open**. Built: the seam `dw3_switch_user_resolve()` with
  unit tests; Switch resolution (applet type, preselect, one user, system
  profile selector; cancel = the game does not start, nothing written;
  `shared` if the account service fails); saves in
  `sdmc:/switch/dw3/saves/<uid>/<us|eu>/memcard/` in today's flat format
  (**not** `card0.dw3s`: P6.25 is not done); `profile.txt`; legacy saves
  **copied** into a new profile's region folder (`imported.txt`); PC profile
  `default` / `DW3_SAVE_PROFILE`; the uid hash in the log and the crash report.
  **Still open:** the interactive import prompt, the P6.25 canonical store,
  global options (`port_options.cfg` stays per region), the applet-mode
  fallback if `psel` cannot launch there. As built:
  [PER_USER_SAVES.md](PER_USER_SAVES.md#as-built-slice-1-2026-10-27); OLED
  items: [OLED_TEST_PLAN E5](OLED_TEST_PLAN.md#e5-per-user-saves-p626). **Owner OLED (preview.43/44, 2026-10-27/28):** each Switch profile has its own Continue, and cancelling the profile selector returns to the Switch HOME (E5.3 to E5.5; the logs were not sent).
  Original entry. Dated 2026-10-24, owner decision:
  saves are bound to the Switch user profile that is playing, chosen with the
  Switch profile selector ("Quero usar o seletor de perfil do Switch."). **Design
  done, no code.** Full note: [PER_USER_SAVES.md](PER_USER_SAVES.md).
  **Design (summary):** libnx account service; the user is the preselected
  user, else the only user, else the Switch profile selector, else the
  `shared` profile. Cancelling the selector exits the game cleanly, nothing
  written (owner decision, 2026-10-24). Saves go to
  `sdmc:/switch/dw3/saves/<uid>/`; legacy saves are copied to the user who
  accepts a prompt at first launch; options stay global. Full text (today's
  paths, the design, the new keys):
  [PER_USER_SAVES.md](PER_USER_SAVES.md#history-moved-from-roadmapmd-2026-10-24).
  Order: seam and tests with a fake account provider -> Switch resolution and
  OLED facts (applet type, preselect, user count) -> profile selector and import prompt ->
  global options.
  Done when (proposed): two profiles on the OLED keep separate saves in applet
  and in title-override mode; a one-user console boots with no extra screen; the
  import works and leaves the originals; options are one global file; replays
  unchanged.
  Open (owner): if the selector cannot launch in applet mode, an in-game picker
  or title takeover (see [PER_USER_SAVES.md](PER_USER_SAVES.md#open-questions-for-the-owner));
  global versus per-user options; the legacy-import rules (see
  [SINGLE_BUILD.md](SINGLE_BUILD.md#open-questions-for-the-owner)).
  Related: P6.24, P6.25, P5.3, P9.4 ([PORT_OPTIONS.md](PORT_OPTIONS.md)), P6.15.
- [ ] **P6.27 Return to title from the pause menu.** Dated 2026-10-24, owner
  request (Portuguese, "voltar ao título" in the pause menu). **State
  (2026-10-25):** built, PC US and EU replays pass, owner OLED check open.
  The port has no pause menu of its own; the closest screen is the game's field
  menu (START, `MODE_STATUS` 0x1000, [P3.4_MENUS.md](P3.4_MENUS.md)). **Built:**
  the field menu's options list gets a last row, **Return to title** (text in
  the game's font, five EU languages; one on US), always. Cross opens a Yes / No window (cursor on No, "Progress since your last
  save will be lost."); Yes runs the P6.9 reset unchanged (`restart_perform()`:
  dirty save slots flushed, a failed flush cancels). Patches 0123 (the row, the
  seven-row panel) and 0124 (a test hook); the window is the native overlay's
  (shared with P6.28). Design, tests and the OLED checklist:
  [PORT_OPTIONS.md](PORT_OPTIONS.md#quit-and-return-to-title-p627-p628);
  restart notes: [P6.9_RESTART.md](P6.9_RESTART.md#return-to-title-from-the-field-menu-p627).
  **Defaults chosen for the owner's open questions (owner can change):**
  confirmation Yes / No with the cursor on No; the field menu only, not in
  battle; the confirm text warns that progress since the last save is lost (the
  game's saves are manual, and the reset flushes only the dirty port save slots,
  as P6.9 does); it reuses the P6.9 reset exactly and a failed flush cancels
  it. The row is not tied to the Restart hotkey setting (orchestrator
  decision, 2026-10-25); the field menu screens' replay references were
  regenerated.
  **Owner OLED (preview.41, 2026-10-27):** Return to title works, and Continue after it works. Not reported yet: the Yes / No window, the fade and the US / EU split.
  Done when: from the field menu the player returns to the title on US and EU
  with saves flushed, and a failed flush keeps the game where it was (replays:
  yes on PC; the failed-flush case is covered by the unit test; OLED open).
  Related: P6.9, P6.14, P9.7, P5.3.
- [ ] **P6.28 Quit game on the title menu, back to hbmenu.** Dated 2026-10-24,
  owner request (Portuguese, "sair do jogo" on the title menu). **State
  (2026-10-25):** built, PC US and EU replays pass, owner OLED check open.
  **Built:** the native title menu (PC SDL3 GPU and the Switch GLES overlay) has a
  fourth plaque, **Quit game** (Quitter, Beenden, Salir, Esci; authored lettering
  like the Options plaque), the last row (first set a little apart from Load Game, on the same pitch as the
  others since the preview.38 OLED note). Cross opens the Yes / No window (cursor on No, the lost-progress
  warning); Yes flushes the dirty save slots (`dw3-port-quit: saves flushed`) and
  asks the host to end the session: the Switch sets the HOME exit flag
  (`dw3_nx_set_quit_requested`, so `dw3_game_main()` returns and the NRO goes back
  to hbmenu), the PC posts the window's close event. The HOME path
  (`dw3_nx_service_applet_quit`) is not changed. The title layout and the 16:9
  patches (0085-0087) are untouched: the plaques are 48 px high on a 52 px
  pitch (they were 54 on 60) so four fit above the credits. Patch 0123; the
  release README says Quit returns to hbmenu. Design, tests and the OLED
  checklist: [PORT_OPTIONS.md](PORT_OPTIONS.md#quit-and-return-to-title-p627-p628).
  **Defaults chosen for the owner's open questions (owner can change):**
  confirmation Yes / No with the cursor on No; on the title menu only (the pause
  menu has Return to title, P6.27); the last entry, away from Continue; the
  release README gets a line saying Quit returns to hbmenu; it flushes saves as
  HOME does and then exits (a failed flush cancels the quit, where HOME cannot);
  the window also warns that progress since the last save is lost. On the PC GL
  renderer (no native overlay) the title keeps three rows.
  **Owner OLED (preview.41, 2026-10-27):** Quit through the Sphaira forwarder works and returns to hbmenu. The full-app Sphaira launch shows errors; the forwarder is fine. Not reported yet: the card files, the flush line and the US / EU split.
  Done when: Quit on the title menu on US and EU returns to hbmenu with the card
  files intact and a flush log line, and the HOME path is unchanged (replays: the
  process ends by itself with status 0 and the flush line on PC US and EU; OLED
  open).
  Related: P9.2, P10.2, P5.4, P6.9.
- [ ] **P6.29 Save editor, external tool (future goal).** Dated 2026-10-24,
  owner request (Portuguese, an editor for saves). Goal note only, nothing
  started. A separate tool **outside the game**, for example `tools/save_editor`,
  that reads and writes the owner's own card files. It builds on the P6.25
  cross-region container (`.dw3s`, magic, format version, crc, origin region
  and language per slot, EU layout as the canonical data; see
  [CROSS_REGION_SAVES.md](CROSS_REGION_SAVES.md)). Today the saves are the raw
  PS1 card images in `sdmc:/switch/dw3/<us|eu>/memcard/`
  (`port/memcard/memcard_host.c`, [P5.3](P5.3_FLAT_SAVE.md)), and P9.6 (external
  save converter) is the open tool for PS1 card images. This goal is the editor
  itself: view and change what the game stores (money, items, cards, partners,
  progress) in a form a player can use.
  **Hard rule:** the tool **never ships game data**. It works only on the
  owner's own saves, which the user supplies; no sample save, no table copied
  from the disc, and no output that embeds disc content goes into git or into a
  release (AGENTS.md, "Own-disc only").
  **First slice:** a read-only viewer on PC that opens a `.dw3s` or a raw card
  file and prints the header, the origin region and the summary of each slot
  (name, money, play time), with the checksums verified. No write yet.
  **Constraints:** Python 3 standard library, in `tools/` next to the other
  helpers; tests use synthetic fixtures only; flags are edited only when their
  meaning is confirmed (a wrong flag can break a script); the EU extra-chapter
  rule of P6.25 is kept (refuse, do not guess).
  **Open questions (owner):** is it a CLI, a small GUI, or both; should it edit
  flags and progress or only items and money; should it also run on the Switch
  (no, unless the owner asks); should it live in `tools/` or in its own repo; is
  a read-only first release enough.
  Done when (proposed): the viewer reads the owner's own US and EU saves
  (checked on the owner's machine, not in CI), a write round trip keeps every
  byte it did not change, and the tests use no game data.
  Related: P6.25, P6.26, P9.6, P5.3.
- [ ] **P6.30 Help, FAQ and tips from the pause menu (future goal).** Dated
  2026-10-24, owner request (Portuguese, "ajuda / FAQ / dicas"). Goal note
  only, nothing started. **What exists (cited):** no in-game help text; the
  player guide is [PLAYER_GUIDE.md](PLAYER_GUIDE.md) (P6.23, slice 1), and the release
  `README.txt` (`platforms/switch/release/`) holds the install steps and the
  OLED checklist. **Scope:** an entry in the pause menu (P6.27) that opens a
  help screen: short pages for install and own-disc setup, controls and
  hotkeys ([HOTKEYS.md](HOTKEYS.md)), turbo, saves and backups, crash reports,
  and a short FAQ. The text comes from one source, the same one as the guide
  of P6.23, so the screen and the docs cannot drift.
  **First slice:** a single help page (controls and hotkeys) in the menu, with
  the strings in `port/ui/port_text.c`; the other pages follow.
  **Constraints:** port text, not game text, so no disc data is copied; every
  page in the five EU languages (EN, FR, DE, IT, ES) and US English, or the page
  says which languages it has; text stays within the window; only what is
  confirmed on hardware (unverified items link to the ROADMAP); screenshots only
  from the owner-approved set (AGENTS.md).
  **Owner answers (2026-10-27), they replace the open questions:** (1) it
  opens from the pause menu **and** from the title menu ("sim, pelo menu
  principal também", yes, from the title menu too); (2) no Brazilian Portuguese
  first: pt-BR comes when that language is added to the game (P6.7) ("ela deve
  ter português quando formos adicionar essa língua ao jogo", it should have
  Portuguese when we add that language to the game); (3) everything is offline,
  no links needed ("Tudo offline", everything offline); (4) a page may scroll
  ("pode ter rolagem", it can have scrolling).
  **Placement (owner, 2026-10-27):** the help is a submenu of the redesigned
  Options screen ([P9.9](#phase-9--port-options-menu)). Order: after P9.9 lands.
  Done when (proposed): a tester opens Help from the pause menu on US and EU,
  reads the controls page, and finds the answers for install and crash reports
  without leaving the console.
  Related: P6.23, P6.27, P6.7, P6.35, P9.2, P9.9.
- [ ] **P6.31 Auto-save (off by default).** Dated 2026-10-24, owner request
  (Portuguese, "salvamento automático"). First slice 2026-10-25 (patches `0140`,
  `0141`) wrote over the slot in use. **Reworked 2026-10-27 after the owner's
  decision of 2026-10-26 (branch `feat/p6.31-autosave-file`, patches `0140`,
  `0141`, `0142`), PC US and EU replays pass; owner OLED check open.** Design,
  defaults and checklist: [P6.31_AUTOSAVE.md](P6.31_AUTOSAVE.md).
  1. **Own file:** with Options > **Auto-save** on (OFF default), an area change
     (progress 4 or more, a known map, no event, menu, banner or battle) writes
     `autosave.bin` in the card folder (card format, rename-atomic). It never
     writes `save0.bin` / `save1.bin`; the `.pre-auto` copy is gone. A new game
     overwrites the previous auto-save (**default chosen, owner can change**).
  2. **Load path:** the title's **Continue** loads the auto-save whenever the
     file exists (regardless of the option); the old Continue (slot list) is the
     new **Load Game** row, and Quit game moves down. With no file the title is
     as before and Continue acts as Load (**default chosen, owner can change**:
     the alternative is a greyed or hidden Continue). Chosen over a load-screen
     entry because the title already has port plaques (patch 0123) and the
     load screen has a fixed three-slot layout.
  3. **P6.20 cheat marks:** the auto-save carries the mark in `autosave.cheats`;
     Continue keeps the game marked. **P6.26:** the file is in the card folder.
  Replays: `us/eu-autosave-area`, `-load`, `-off`, `-walk`, `-cheat`,
  `title-continue-load` ([P6.15](P6.15_REPLAY_TESTS.md)); unit tests
  `tools/tests/test_autosave.py`.
  **Still open:** an on-screen notice (success or failure); an after-battle
  trigger and a minimum interval; the cost of the SD write per area change on
  hardware; the title logo report of preview.38 (a separate fix; the spacing report is fixed by #287 and was confirmed on the OLED 2026-10-27).
  **Owner OLED (preview.41, 2026-10-27):** the auto-save writes on an area change and on HOME close; the title plaque spacing (#287) is confirmed. Not reported yet: the save-file checks, Continue, Load Game and the write time.
  Done when: with the option on, an area change leaves one recoverable save and
  the old save survives a forced write failure (both proven on the PC), and with
  the option off nothing changes (proven: the replay suite). The OLED check is in
  [Pending OLED checks](#pending-oled-checks).
  Related: P6.14, P6.16, P6.20, P6.25, P6.26, P9.4, P9.9, P5.3.

- [ ] **P6.32 Map zoom and full-screen map (future goal).** Dated 2026-10-24,
  owner request. Goal note only, nothing started, no code. Zoom in and out on
  the world map, and a full-screen map view that uses the whole 16:9 picture.
  Today the Map screen (field menu) shows the 392 px map centred in 16:9 with
  black sides and no scroll (P6.1, patch `0080`; see
  [P6.1_WIDESCREEN.md](P6.1_WIDESCREEN.md#map-results-and-save-screens-patches-0080-0081-0082)).
  The map logic (`moveMapCursor`, `showMapArea`) is the PS1's and must not
  change for a player who does not use the option.
  **Open questions (owner):** zoom steps and the buttons for them; whether the
  full-screen map replaces the field menu's Map entry or is a second view;
  whether the zoomed view needs a wider scroll clamp (a logic change, not
  drawing); where the Port option for it lives (P9.4 key, Options screen).
  **Design proposal (2026-10-25, no code):** [P6.32_MAP_ZOOM.md](P6.32_MAP_ZOOM.md)
  surveys the map screen code (`map_screen.c`, overlay `STSTATUS`), what
  zoom and full-screen can mean at 392 px source width, and compares four
  options. Recommendation: fill the picture first (option A), then integer
  zoom steps with a port-side pan (option B), each behind a Port option that
  is off by default, with no save change.
  **Owner decisions (2026-10-26/27):** build option D, a port-drawn full-screen
  map as a **new second screen** (the game's Map entry unchanged, no turbo),
  opened from the game's Map screen with Square (an optional field hotkey
  later), non-integer zoom steps with L/R, remappable, Switch and PC, US and
  EU, one Port option key, widescreen only; option B (integer zoom with pan) is
  the fallback. The picture may be the captured map, an original fan-art map
  committed under `assets/map/` (with LICENSE and PROVENANCE.md), or a
  user-generated local pack. Phase 0 feasibility, upscale survey and the plan
  (patches 0146-0159 proposed) are in [P6.32_MAP_ZOOM.md](P6.32_MAP_ZOOM.md).
  **Phase 0 (2026-10-27, PR `feat/p6.32-map-poc`):** the new screen exists as a
  PoC (patch `0146`, `port/map_view/`, a PC and a Switch overlay): Square opens
  it from the Map screen, pan, zoom fit / 1.0 / 1.25 / 1.5 / 2.0 / 3.0 on L1 / R1,
  **L2 / R2 cycle the scaler live** (nearest, sharp bilinear, Lanczos2, AMD FSR 1,
  CPU, timings in the log), no turbo, option key `map_screen` (PoC default on).
  The map is **392 x 352**; it can be **read from the disc files** (identical to
  the VRAM capture on US and EU, 0 bytes differ) and is **precomputed on a spare
  core / low-priority thread** and cached under `sdmc:/switch/dw3/cache/` (PC:
  the config dir). Verdict D: go; **open: the area-name boxes**. Free field
  buttons, memory (+7.8 MB while open) and the rest are in
  [P6.32 section 7](P6.32_MAP_ZOOM.md#7-phase-0-results-2026-10-27-spike-and-scaler-poc);
  the owner's OLED checklist is section 8. Tests: `test_map_view_core.py`,
  replay `us/eu-map-view` and `-43`, Ryubing US and EU.
  **HUD elements (2026-10-28, owner after preview.45, branch
  `fix/map-view-hud-elements`):** the view now shows the game's **balloon** on
  the current area, a **cursor** moved with the D-pad / left stick (the view
  follows it, the right stick zooms), and the **game's own name box** for the
  location under the cursor (screen-space, 16:9-aware, hidden by the HUD
  toggle); **Nearest is the default scaler**; Lanczos2 / FSR 1 build on a
  worker thread and no longer block input ([P6.32 section
  10](P6.32_MAP_ZOOM.md#10-the-balloon-the-cursor-and-the-name-box-owner-after-preview45)).
  **Owner OLED (preview.45, 2026-10-28):** the map shows and Nearest is the preferred scaler. Not confirmed, pending preview.47: the balloon, the cursor and the name box.
  Open: OLED look (left-stick feel, core-2 worker), the other EU languages.
  Done when (proposed): the new map screen opens from the Map screen on US and
  EU on PC and the Switch, zooms and pans over the full image, shows the
  game's area names, the default is bit-identical, and the P6.15 map replay
  scenarios still match.
- [ ] **P6.33 Items menu usability redesign (in progress).** Dated 2026-10-24,
  owner request. Owner decisions recorded 2026-10-26 in section 5 of the
  design note (ZL / ZR cycle, on by default, 4:3 and 16:9, side list dropped). Make the Items
  menu easier to use: switch the Digimon while equipping, and see the bag
  items while doing it. This is separate from the P6.1 sub-item "Items menu
  (16:9)", which is only the layout and the open transition (patches `0075`,
  `0076`); that work is done and pending OLED. P6.33 changes how the menu
  works, not how wide it is.
  **Design proposal (2026-10-24, no code):** [P6.33_ITEMS_MENU.md](P6.33_ITEMS_MENU.md)
  surveys the code, lists the pain points and compares three options.
  Equipping is in the Status screen (`equip_panel.c`), not in Items. The
  recommendation is option A, in phases: a partner cycle on the equip slot
  list (L2 / R2), a bag summary and items worn by other Digimon, all in the
  game's own widgets, behind a Port option that is off by default, with no
  save change.
  **Open questions (owner):** the exact flow (a partner switch row in the
  equip panel, or L1/R1 to change partner); which bag view is shown next to
  the equip panel; whether the change is a Port option or always on.
  **Owner OLED (preview.41, 2026-10-27):** the phase 1 partner cycle in Equipment works (G8). Not reported yet: the `items_partner_switch=0` check.
  Done when (proposed): the owner can change the partner and see the bag
  while equipping on US and EU, the menu still fits 16:9 and 4:3, and the
  P6.15 Items replay scenarios (`us/eu-items-screen[-normal]`) still match.
  Related: P6.1, P6.15.
  Phases 2–4 are folded into P6.34 (owner, 2026-10-28).
- [ ] **P6.34 Items / status / equipment menu flow redesign (future goal).**
  Dated 2026-10-27, owner: the options and their order in the items, status
  and equipment menus feel odd. A redesign of the whole flow, beyond the
  partner cycle of P6.33. No design yet; start from the P6.33 note. Phases 2–4
  of P6.33 are folded into this item (owner, 2026-10-28). Done when
  (proposed): the owner approves a flow and it works on US and EU, 4:3 and 16:9.

- [ ] **P6.35 In-game walkthrough help (future goal).** Dated 2026-10-27,
  owner request (Portuguese: "um menu de ajuda mais no sentido do walkthrough,
  pra ajudar o jogador no jogo", a help menu closer to a walkthrough, to help
  the player in the game). The owner also said "quero os 2" (I want both): this
  is player guidance, separate from the technical help of P6.30. Goal note
  only, nothing started.
  **What exists (cited):** the port gives no in-game guidance on where to go
  next. P6.30 covers only the technical help (install, controls, saves, crash
  reports), and the player guide ([PLAYER_GUIDE.md](PLAYER_GUIDE.md), P6.23)
  is for setup, not for play.
  **Scope:** a screen that tells the player where to go next and what to do,
  by story progress. Each step may have spoiler levels: a hint first, then the
  full answer, shown only when the player asks for it.
  **Proposal (no code):** drive the screen from the game's story progress
  flags, so it shows only the current step and never reveals later steps
  (spoiler-safe). The screen only reads the flags; it does not write them and
  the save format does not change. The flags are not mapped yet (to do: find
  them in the decomp).
  **Where it opens:** from the pause menu, as a submenu of the redesigned
  Options screen (P9.9). Maybe also from the title menu, as an open question
  for P6.30 (answered yes there).
  **Offline, scrolling, languages:** everything offline, no links; a page may
  scroll; no Brazilian Portuguese first, pt-BR comes with P6.7; the five EU
  languages and US English, as P6.30.
  **Constraints:** all text is original, written for this port. Never copied
  from existing walkthroughs or FAQs (GameFAQs and similar), and never from the
  game's disc script beyond short names (character, item and place names, so
  the player can find things). No game data in git; anything derived from the
  disc stays out of git (AGENTS.md). Screenshots only from the owner-approved
  set.
  **Open questions (owner):** (1) spoiler levels: is the hint-then-answer split
  wanted? (2) per-step hints tied to progress, or a full chapter list? (3)
  include side content, such as recruitable Digimon and card or item
  locations? (4) who writes the text: an agent drafts it from the decomp and the
  progress flags, and the owner reviews it?
  **Owner decisions (2026-10-28), they replace the open questions above:**
  (1) spoilers: each area has a spoiler sub-area that the player opens only
  if lost, hidden by default and shown after an explicit reveal; (2) the screen
  shows the current step, derived from the story progress, with an option to
  browse other steps and chapters; (3) side content (optional quests, recruits,
  items and the like) is included and marked as optional; (4) agents draft the
  text and the owner reviews it before it ships. Earlier P6.30 decisions still
  apply (offline, scrolling pages, Portuguese with P6.7).
  **Design (2026-10-28, no code):** [P6.35_WALKTHROUGH](P6.35_WALKTHROUGH.md):
  `GAME.progress` and the event flags as the step key (with file:line), a step
  table separate from per-language text tables, UI flow, the two-step spoiler
  reveal, side content, a phased plan (phase 1: main steps of the first area as
  a slice), the authoring and review workflow, the test plan with log proof
  lines, and seven open questions for the owner.
  Done when (proposed; the hint-then-answer wording is superseded by decision 1):
  at a given story point, a tester opens the help from the
  pause menu on US and EU and sees only the next step, the hint first and the
  full answer on request; every text is in the five EU languages and US
  English; a long page scrolls; nothing is copied from outside sources.
  Related: P6.30, P9.9, P6.23, P6.7.

### Phase 7 goal notes

- [ ] **P7.6 Progression options (future goal).** Port options for EXP
  and DV EXP rate (1x default, Harmony uses 1.3x / 1.2x), uncapped DV EXP
  and HP/MP restore on level up. Credit markisha64 / rsigrist. Done when
  each option works on US and EU and is off by default.
  **Boundary with P6.18 (owner undecided, 2026-10-24):** no merge is recorded.
  This is progression and training balance; P6.18 is presentation pacing (text
  speed, banners, encounter rate). The owner can revisit.
- [~] **P7.7 Bug and text fixes.** Picking / Snapping Claw on a KO'd enemy,
  active Digivolution name in battle, battle reward "1IT"/"1EXP" text, save
  label showing Asuka Inn, Square map glitch. Checked on the US and the EU
  disc (2026-10-22, PR for `fix/p77-original-bugs`; details and evidence in
  [P7.7_BUG_FIXES.md](P7.7_BUG_FIXES.md)):
  the Claw steal on the killing blow (patch `0066`) and the Digivolution name
  in the battle HUD (`0065`, a nickname is kept, long names are squeezed) are
  original-game bugs on both discs, fixed always-on (no Port option, decision
  in the doc) and covered by 20 log-only PC replay scenarios
  (`*-p77-*`, hooks in `0064`). The other three could not be reproduced on
  the stock discs and were left alone: the reward text renders "BIT" / "EXP"
  in the US disc and in all five EU languages; the stock game can only save at
  the 25 save points, whose labels are right (the "Asuka Inn" label is place 43,
  shared by the unused mode 0xC00, so it belongs to Harmony's Start-menu save,
  P6.14); the field reads no Square (the glitch is in Harmony's FastTravel,
  P7.4). **OLED status (2026-10-23, preview.30-rc7, owner, US):** neither fix
  is confirmed yet. The **Claw steal** is untested because the owner has no
  Claw (Picking / Snapping Claw, Growlmon line) yet. The **HUD Digivolution
  name** is not recorded as confirmed separately in the roadmap or in
  [P7.7_BUG_FIXES.md](P7.7_BUG_FIXES.md) (its OLED checklist is still all
  open), and the rc7 owner report does not mention it, so it stays open too.
  Both lines go only to the nxlink stderr, so the `dw3.log` cannot confirm them.
  Open: the OLED check of the two fixes (checklist in the doc), a
  digivolution played during a battle, and the three bugs above if a
  screenshot from an unmodified disc turns up. Done when each listed bug is
  fixed or shown not to exist on the stock discs, and the fixes are verified
  on the OLED.
  **New bug, open (2026-10-24, owner):** after leaving the Underground Path
  the water texture is wrong until the screen scrolls. Not diagnosed. Another
  agent is investigating it on branch `fix/underground-water-texture`; this
  entry claims no fix. Owner OLED, 2026-10-27 (preview.41): the water is right at once (test B11), so the fix is confirmed. Details:
  [P7.7_BUG_FIXES.md](P7.7_BUG_FIXES.md#underground-path-water-texture-2026-10-24).
- [ ] **P7.8 Battle info HUD (future goal).** Optional enemy HP, always-on
  partner MP, technique attribute / power / MP cost colours, DV stats and
  EXP to next level after battle. Done when the toggle works on US and EU
  in all 5 EU languages.
- [ ] **P7.9 Battle menu memory.** Remember the last battle command
  (Techniques stays on Techniques) and allow reordering techniques per
  Digimon. Done when both persist in the P5.3 save on US and EU.
  **OLED (2026-10-23, handheld, US, preview.30-rc7, owner): part A is
  confirmed.** The eight-row Options screen looks good (#206), Command+Tech
  works in battle, and a new battle opens on the technique used in the previous
  battle. The `dw3.log` agrees (`command menu opens on Techniques (remembered)`,
  `technique list of partner 3 opens on technique 56 (page 0, line 0,
  remembered)`, `battle_menu_memory=2` saved). Part B (reordering) is still
  deferred; "persist in the P5.3 save" is still not met, so the item stays open.
  Status (2026-10-23), part A built (OLED check above): the **Battle cursor** row
  of the Options screen (OFF / Command / Command+Tech, key `battle_menu_memory`
  in `port_options.cfg`: 1 = the command menu opens on the last command, 2 =
  also the technique list opens on that partner's last technique), off by
  default and bit-identical when off (patch `0067`, unchanged). It was a
  file-only key until the owner asked for a row after the rc6 OLED test; the
  Options screen now has **eight rows** (38 px rows on a 42 px pitch, same
  window; [PORT_OPTIONS.md](PORT_OPTIONS.md#options-screen-2026-10-14), "Eight
  rows"). Session memory, not saved. Covered by 12 log-only PC replay scenarios
  (`*-p79-*`), the new `*-battle-cursor-persist`, unit tests and the cache test. Open: the rest of the OLED checklist (docked,
  the value kept after a relaunch, Item/Run and OFF steps, EU), and part B: reordering techniques is investigated and deferred (the PS1 save
  image has no room and its size is checked; the design is a `saveN.order`
  sidecar), so "persist in the P5.3 save" is not met yet. Doc:
  [P7.9_BATTLE_MENU_MEMORY.md](P7.9_BATTLE_MENU_MEMORY.md).
- [ ] **P7.10 Training without RNG (future goal).** Guaranteed base gains,
  optional timing bonus, Fast Training skip, as an option off by default.
  Done when it works on US and EU.
  **Boundary with P6.18 (owner undecided, 2026-10-24):** no merge is recorded.
  This is training balance; P6.18 is presentation pacing. The owner can revisit.
- [ ] **P7.11 Guidance and exploration (future goal).** Quest log HUD,
  next-step walkthrough (Flawe, EU 5 languages + US English), NPC
  quest/battle bubbles, auction / DRI notices, Digivolution requirements in
  the DigiLab, shop stat bonuses, L1/R1 leader swap in the field. The
  community most often asks for "where do I go next" and Digivolution
  requirements. Done when each part toggles on US and EU.
- [ ] **P7.12 New items and name length (far future).** Harmony's encounter
  disks/rings, DV Adapter, Analysis Disk, MP recovery items; 10-character
  player name. Both change item tables or save layout, so only after the
  P5.3 save format is frozen with a migration. Done when old saves still
  load on US and EU.
- [ ] **P7.13 Transitions and visuals (future goal).** Shorter fades and
  location cards, skip redundant location screens after battle / save
  menu, transparent shadows (psyz). Measure save and menu time first; the
  port already has no disc seek. Done when the option works on US and EU.
- [ ] **P7.14 Community wishlist (note).** Requests from r/digimon /
  r/DigimonWorld threads (2026-10-15, search results were thin; mostly the
  Harmony announcement thread): lower encounter rate and less grinding
  (P6.18, P7.6), faster battles and text (P6.18), knowing where to go
  (P7.11), clearer Digivolution paths (P7.11), save anywhere / quick
  resume (P6.14, P6.16), widescreen and HD (P6.1, P6.11, Phase 8),
  60 fps (P6.21). Out of scope: new Digimon lines and new story content.
  Done when: TBD (owner). This is a note, not a task.

## Cross-cutting rules

- Record every non-obvious decision as a short note in `docs/`.
- Changes to the decomp go through `port/patches/` first; upstream PRs for
  anything match-neutral; submodule bumps are explicit commits.
- CI job `pc` builds the PC host. Job `switch` builds the Makefile
  `host_hello` sample. Job `switch-game` builds the CMake game `.nro` on
  pull requests and uploads an Actions artifact. A GitHub Release is
  `switch-release.yml` only. Game-data tests run locally.
- Keep `platforms/switch/` thin: platform backend only, no game logic.
