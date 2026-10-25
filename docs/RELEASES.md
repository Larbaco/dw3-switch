# Preview releases

All releases are GitHub prereleases. Each ships `.nro` files (and, from the
release after preview.30, `dw3-switch-elf.zip` with the matching `.elf`
files for crash reports), never game data. Tags are named `v0.6.5-preview.N`; hbmenu shows the short form
`0.6.5-p.N`. Pick the newest unless a note below says otherwise.

All releases are on the
[releases page](https://github.com/Larbaco/dw3-switch/releases). Install
steps are in the [README](../README.md#play-it-on-switch). Preview numbers
that have no tag (11 to 17) were never published.

## Do not install

| Tag | Why |
|---|---|
| [preview.1](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.1) | Crops the title and freezes the movie. |
| [preview.6](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.6) | Raised the CPU to 1785 MHz. Reverted afterwards. |
| [preview.9](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.9) | Enabled the battle mesh for every mode and broke the field picture. |
| [preview.19](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.19) | A GPU diagnostic cut, not a playable baseline. |

## Index

Newest first.

| Tag | What it was for |
|---|---|
| [preview.37](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.37) | Options in battle (P9.7 slice 2, #272): Minus at the command menu opens Options over the frozen battle; Triangle closes it. Save write safety (P10.2, #273): interrupted writes recover at boot, HOME and restart wait up to 8 s for the save burst. Options from the field menu (#270), title textures freed outside the title (#268) and memory work (#266, #267) carried from preview.36. EU attached by hand. Notes: [`platforms/switch/release/README.txt`](../platforms/switch/release/README.txt). |
| [preview.36](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.36) | Options from the field menu (P9.7 slice 1, #270): an OPTIONS row above TITLE SCREEN; Widescreen and Cheats stay title-only; battle not covered yet. Title screen textures freed outside the title on Switch and rebuilt on return (#268). PC soak and Ryubing memory check (#266, #267). OLED test plan (#269). Turbo (P6.22 pass 2, #263) and the player guide (#262) carried from preview.35. EU attached by hand. Notes: [`platforms/switch/release/README.txt`](../platforms/switch/release/README.txt). |
| [preview.35](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.35) | Faster turbo outside battle (P6.22 pass 2, #263): the Switch no longer draws frames it will not show, same picture; the field and opening at 4x should gain, not measured on Switch yet. Player guide covers Auto-save and the region launcher (#262). Auto-save (P6.31, #260), Quit game, Return to title and cross-region import carried from preview.33 and 34. EU attached by hand. Notes: [`platforms/switch/release/README.txt`](../platforms/switch/release/README.txt). |
| [preview.34](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.34) | Auto-save, off by default (P6.31, #260): a ninth Options row, OFF or Each area. When on, the first idle field frame of each new area saves to the last-used slot, overwriting it; the first time the card is kept as `save<N>.bin.pre-auto`. Player guide (#255), CONTRIBUTING.md and a doc-link test (#256), P6.32 map zoom proposal (#257). Region launcher in progress, not in this release (P6.24, #258, #259). Quit game, Return to title and cross-region import carried from preview.33. EU attached by hand. Notes: [`platforms/switch/release/README.txt`](../platforms/switch/release/README.txt). |
| [preview.33](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.33) | Quit game on the title menu (P6.28, flushes saves, back to hbmenu), Return to title in the field menu (P6.27, same reset as the restart hotkey). Title plaques 48 px so four fit. Cross-region save import, slice 1 (P6.25, #252): a region with no save of its own imports the other region's save once, one-way. Restart hotkey validated on PC, US and EU (P6.9, #251). PC door iris map-over fix in PSY-Z (#253). EU attached by hand. Notes: [`platforms/switch/release/README.txt`](../platforms/switch/release/README.txt). |
| [preview.32](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.32) | Canal water in the Cargo Tower streets after the Underground Path, fixed (patch 0111, #242). 16:9 for the card album, deck editor, card shop, gym and Digimon Lab (#245; Lab chart, entry and skill still 4:3 at the edges). 16:9 door iris and grid cover (#247). Cheats screen page 2: Money, Warp to 13 areas, Give item (#248). EU attached by hand. Notes: [`platforms/switch/release/README.txt`](../platforms/switch/release/README.txt). |
| [preview.31](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.31) | Field rasterizer 4 to 5 times faster (P6.22), 16:9 for the field menu, Items/Sort/Techniques/Status/Card folders, title menu, intro caption bar and registration screen. Post-battle report at turbo speed and the banner whole at Instant. Partner portrait in gpu-tex, file cache overflow fix (long sessions), crash notice for 60 s with software aborts, `dw3-switch-elf.zip` for crash symbolization. 16:9 for the map, battle results header and save screen (#240), Cheats screen in Options (P6.20, #236). EU attached by hand. Notes: [`platforms/switch/release/README.txt`](../platforms/switch/release/README.txt). |
| [preview.30](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.30) | Music loops again (#209), widescreen area banner, technique backdrops and results screen, EU native title credits, Items screen crash fix, Port options saved to `port_options.cfg`, Music/SFX volume, Encounter rate, Text speed and Battle cursor rows, crash-report backtrace. Win jingle and menu/battle sounds that never played now do (#215). EU attached by hand. Notes: [`platforms/switch/release/README.txt`](../platforms/switch/release/README.txt). |
| [preview.29](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.29) | Fixes the crash at the new-game bridge "need my partner" event (patch 0058), faster Port options (dirty-region cache), crash-report rotation, rumble, HOME/sleep save flush. EU attached by hand. Notes: [`platforms/switch/release/README.txt`](../platforms/switch/release/README.txt). |
| [preview.28](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.28) | EU restart returns to the language selection (#162); carries the preview.26/27 changes. EU attached by hand. |
| [preview.27](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.27) | Field streaming fix for the 16:9 crash (inferred from the preview.26 crash records, not yet reproduced). Adds the 4C DW3NX artwork. Hardware acceptance pending. |
| [preview.26](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.26) | EU language selection at boot, battle camera and portrait fixes, Port options on its own screen. Notes: [RELEASE_PREVIEW26](RELEASE_PREVIEW26.md). Crashes: [PREVIEW26_CRASH_RECORDS](PREVIEW26_CRASH_RECORDS.md). |
| [preview.25](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.25) | Widescreen on by default, battle defaults to `gpu-tex`, field and battle fast-forward, EU builds added. Notes: [RELEASE_PREVIEW25](RELEASE_PREVIEW25.md). |
| [preview.24](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.24) | Native title menu on Switch (New Game / Options / Load Game), EU runtime extraction. US only. |
| [preview.23](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.23) | Same play as preview.22 on Switch. PC title menu and docs only. |
| [preview.22](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.22) | Widescreen option, GPU battle background fix, opt-in restart hotkey. Notes: [RELEASE_PREVIEW22](RELEASE_PREVIEW22.md). |
| [preview.21](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.21) | Opening-movie read-race fix; movie pacing improved from 6.75 to about 15 fps. Notes: [RELEASE_PREVIEW21](RELEASE_PREVIEW21.md). |
| [preview.20](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.20) | Title Port options shell (switches were stubs), empty `StGetNext` scan cache. Notes: [RELEASE_PREVIEW20](RELEASE_PREVIEW20.md). |
| [preview.19](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.19) | GPU diagnostic cut. See the table above. |
| [preview.18](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.18) | Battle mesh gated to FIGHTSTG; `Minus + ZL + ZR` cycles battle render modes; software battle default. |
| [preview.10](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.10) | Battle mesh limited to FIGHTSTG, field back on the software blit, sampled `mesh dbg:` log lines. |
| [preview.9](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.9) | See the table above. |
| [preview.8](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.8) | Holds one field map in RAM so later tiles copy from that buffer. |
| [preview.7](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.7) | Uploads only the sampled VRAM rectangle and loads one field tile per frame. First build to reach field and battle on an OLED. |
| [preview.6](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.6) | See the table above. |
| [preview.5](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.5) | Hardware test of the copy progress bar, left-stick input and the US BIN extraction. |
| [preview.2](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.2) | Picture fix: display-mode title width and the full-height movie. |
| [preview.1](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.1) | See the table above. |

Previews 3 and 4 have no separate notes in this repository.

## Publishing a release

A `v*` tag (or `workflow_dispatch` with a tag) runs
`.github/workflows/switch-release.yml`, which builds the US and US nxlink
NROs and publishes `dw3-switch.nro`, `dw3-switch-nxlink.nro`,
`dw3-switch-elf.zip` and `README.txt`. The zip holds the unstripped
`dw3-switch-us.elf` and `dw3-switch-us-nxlink.elf` plus `BUILD_IDS.txt`
(GNU build id, `.elf` and `.nro` per line).
`tools/pack_release_elfs.py` makes it and fails the job when an `.elf`'s
build id is not the one in its `.nro` header. A local rebuild does not
reproduce a CI binary (the build ids differ), so the CI `.elf` is the only
one that symbolizes a CI `.nro`.

To test the packing without a release, run the script on local builds
(`tools/tests/test_pack_release_elfs.py` covers it in `pytest`). The
workflow itself has no dry-run path: `workflow_dispatch` always publishes
the tag it is given.

**EU is a manual step.** Build EU once, then upload the `.nro` and its
`.elf` zip from that same build (the `.elf` must come from the build that
made the `.nro`):

```bash
bash tools/build_switch_game.sh --eu
bash tools/build_switch_game.sh --eu --nxlink     # only if it ships
B=build/switch-eu
python3 tools/pack_release_elfs.py --out /mnt/cabal/tmp/dw3-switch-eu-elf.zip --tag v0.6.5-preview.N \
  --item $B/dw3-switch.nro $B/dw3-switch.elf dw3-switch-eu.elf
cp $B/dw3-switch.nro /mnt/cabal/tmp/dw3-switch-eu.nro
gh release upload v0.6.5-preview.N /mnt/cabal/tmp/dw3-switch-eu.nro /mnt/cabal/tmp/dw3-switch-eu-elf.zip
```

For an EU nxlink build add a second `--item
build/switch-eu-nxlink/dw3-switch-nxlink.nro build/switch-eu-nxlink/dw3-switch.elf
dw3-switch-eu-nxlink.elf` and upload that `.nro` too. The release README
lists only the files actually published.

Since preview.18, [`platforms/switch/release/README.txt`](../platforms/switch/release/README.txt)
holds the notes shipped with each release.

## Building without cheats

Since P6.20 every build, including the release and nxlink NROs, has a **Cheats**
screen in the Options screen (Y on the Switch; master switch off by default;
[PORT_OPTIONS.md](PORT_OPTIONS.md#cheats-p620)). The release workflow keeps this
default: the published `.nro` files include it. A build without it, which is the
game exactly as before the feature (byte-identical code), is one flag:

```bash
bash tools/build_switch_game.sh --no-cheats            # build/switch-nocheats/
bash tools/build_switch_game.sh --eu --no-cheats       # build/switch-eu-nocheats/
# PC: cmake -S platforms/pc -B <dir> ... -DDW3_CHEATS=OFF
```

The script passes `-DDW3_CHEATS=ON|OFF` every time and checks `CMakeCache.txt`.
Check a no-cheats `.nro` with `strings <nro> | grep -c dw3-cheats` (must be 0).
Say in the release notes which kind was published; a `--no-cheats` `.nro` is
published only if the owner asks for it.

## Hardware observations

Dated notes from the maintainer's Switch OLED. They record what was seen on
one console and are not acceptance gates; gates live in
[ROADMAP](ROADMAP.md).

- **preview.5.** Copy progress bar, left stick (handheld Joy-Con and Pro
  Controller) and US BIN extraction were tested. Rumble is not implemented.
- **preview.7.** Reached field and battle. Across 156 samples of 60 frames
  the mean was about 59 FPS, with some windows near 25 ms/frame. PSY-Z logged
  `KernelInit: cannot create the kernel synchronisation objects` once and
  continued; this error is still open. The load design that followed is in
  [STATIC_SD_LOAD](STATIC_SD_LOAD.md).
- **preview.8.** Field about 60 Hz, battle Fight-menu idle about 46 Hz.
- **preview.14.** Flat saves: loading MC1 (`save0.bin`), spending money and
  saving to MC2 (`save1.bin`) slot 2 passed (money 26). A later overwrite to
  MC2 slot 1 saved money 2. Details: [P5.3_FLAT_SAVE](P5.3_FLAT_SAVE.md).
- **After the 100-frame load bar change.** The memory-card load bar felt
  right; save uses the same constants.
- **preview.20.** The Port options title entry opened with both stubs off and
  returned to the middle row. Plus opens the title choices, B confirms, X
  closes. Applet exit and save flush were observed. Checkpoint:
  [RELEASE_PREVIEW20](RELEASE_PREVIEW20.md).
- **preview.21.** The opening movie ran at about 15 fps (preview.20: 6.75
  fps) with correct XA audio up to frame 1110, where Start was pressed.
  Image quality and the end of the movie are not validated. The GPU battle
  renderer and the STR video changes were reported to work well in game.
  Details: [RELEASE_PREVIEW21](RELEASE_PREVIEW21.md) and
  [P3.7_MOVIE_PACING_DIAGNOSTICS](P3.7_MOVIE_PACING_DIAGNOSTICS.md).
- **EU, 2026-10-14.** On a build before preview.25, extraction from only
  `eu/disc.bin` and `eu/disc.cue` wrote 2383 files and early gameplay was
  reached. Details: [P3_EU_BUILD](P3_EU_BUILD.md).
- **preview.26.** Two `User Break` crashes were reported on EU. Raw records:
  [PREVIEW26_CRASH_RECORDS](PREVIEW26_CRASH_RECORDS.md). Preview.27 carries a
  fix for the suspected cause.
- **preview.29 candidate, 2026-10-22 (OLED handheld, EU nxlink, about 22
  minutes, turbo used).** Passed the new-game bridge "need my partner" event
  that crashed preview.28; the Port options menu felt much more responsive
  (log: cursor move ~4.5 ms compose + ~1.4 ms upload, title-menu rebuild
  ~151 ms). Memory high-water stayed flat after the opening (heap 1949K,
  file cache 1471K). Not covered: US on the OLED, HOME/sleep, rumble, docked.
  Known issue in preview.29: the pump counter advanced twice per VSync
  (since #182), so the nxlink HUD `fps` and every 60-pump log window
  (`frame dt`, `mem dbg`, `movie dbg` cadence) cover 30 frames, and the HUD
  shows double the real rate (about 120 at a real 60 fps; the log above was
  60 fps real at 1x). `vsync_pumps` in `crash_report.txt` is doubled too.
  Gameplay pacing is not affected. Fixed on main after the release.
- **preview.30-rc4 candidate, 2026-10-22 (OLED handheld, EU nxlink).** Only
  the Port options and pacing rows were exercised. `port_options.cfg` was
  created on the first change and saved on every change (no failure logged).
  Music volume stepped 0 to 10 and SFX volume down to 2 with the expected
  gains logged. Encounter rate was toggled and returned to 1x (random battles
  at 1x after 104, 175, 103, 128 and 93 footsteps). Text speed Instant typed
  19 of 27 pages in 1 to 2 frames; 8 pages took 67 to 127 frames: those turned
  out (rc6 follow-up below) to be the area name banner, not text. The session ended with a normal HOME exit and
  `applet exit: flat saves flushed`. Not covered: US, relaunch persistence,
  the P7.7 HUD and claw (logged only to the nxlink stderr), widescreen.
  Log: `tmp/evidence/2026-10-22-preview30-rc4-oled/dw3.log`.
- **preview.30-rc6 candidate, 2026-10-22 (OLED handheld, EU, non-nxlink).**
  20 random battles logged at Encounter rate 2x with Text speed Instant and
  Fast-forward 4x / off, then the game **crashed on opening Items** in the
  field menu (`crash_report.txt`: `mode_file 0x207`, data abort `far=0xd`,
  `x0=0`). Root cause: a NULL `ItemInfo` read in `getItemCategory`, which
  the PS1 tolerates and a 64-bit host does not, on the first open of any
  non-empty item list; the same crash is in preview.29. Fixed by patch
  `0069` (not in rc6), covered by the `*-items-screen` replay scenarios; see
  [BALLOON_NULL_ACTOR.md](BALLOON_NULL_ACTOR.md#patch-0069-items-screen-null-item-preview30-rc6-oled-crash).
  Text speed is not related: Instant typed the report's pages in 1 to 2
  frames (49 lines). The other thing in that log, separate from the crash: after
  each battle (19 of 20), back in the field at 4x, one "page" of 103 (15 times)
  or 73 (4) frames and `slow Instant page with no scroll hold (103 frames): reveal
  missed it`, never a `text scroll`. Cause found 2026-10-23: **the area name
  banner**, not text and not the report (it types its two names at delay 5, 6
  frames per character: 17 and 12 characters, `6n + 1`), wrongly timed as a
  dialogue page by the port's own log on the address of a freed report window.
  Nothing was typed slowly that Text speed should have sped up. Fixed in the
  port (`pacing.c`, no patch); the banner itself is unchanged (it still takes
  about 2.3 s with the hero frozen), see
  [P6.18_PACING.md](P6.18_PACING.md#the-103-frame-pages-the-area-name-banner-rc4-and-rc6-oled-logs).
  Not covered: US, Fast, a dialogue that scrolls,
  the Items screen after the fix on the OLED. Evidence:
  `tmp/evidence/2026-10-22-preview30-rc6-oled/` (log, crash report,
  Atmosphere report).
- **preview.30 next candidate, 2026-10-23 (PC and Ryubing only, not on the
  OLED yet).** Owner request after rc6: the battle menu memory is the **Battle
  cursor** row of the Options screen (OFF / Command / Command+Tech, key
  `battle_menu_memory`), so the screen has eight rows (38 px rows on a 42 px
  pitch, same window). Off by default; no decomp patch changed. Included in this
  candidate with the rc6 follow-ups: the area name banner is no longer logged as
  a slow Instant page (#203) and the Items screen fix (#201). Check on the OLED:
  eight rows fit in handheld and docked, the row steps and keeps its value after
  a relaunch, and with Command+Tech the battle menu opens on the last pick
  ([P7.9 checklist](P7.9_BATTLE_MENU_MEMORY.md#oled-check-the-owner)).
- **preview.30-rc7 candidate, 2026-10-23 (OLED handheld, US, nxlink log; main
  `b0aed93`).** Owner feedback plus the log (`/mnt/cabal/tmp/dw3-preview30-rc7-out/dw3.log`,
  5702 lines, 1707 one-second windows, one run). Confirmed: the Items screen
  works (the rc6 crash, #201, patch `0069`); the eight-row Options screen looks
  good (#206); Battle cursor Command+Tech works and a new battle opens on the
  technique used in the last one (log: `technique list of partner 3 opens on
  technique 56 ... remembered`); Text speed Instant works as desired (no `slow
  Instant page`, no `text scroll`; 26 area name banners logged as non-dialogue,
  #203); the battle renderer is the default `gpu-tex` with good performance
  (`soft=0` in every window, mesh frames in 211, present path `mesh+ui`); turbo
  works in battle (`dw3-turbo: speed 4x` and `1x`, three times); the volume
  options work; sleep and resume worked with saves before and after, and the
  in-game time counted only play time. In the log that sleep is one unclamped
  `frame dt` window of 543746177 us and there is no `applet focus:` line (see
  ROADMAP P10.2). **New bug:** the background music plays only once and then
  stops, in the field, the inn and battle (SEQ/SEP NRPN loop count; fix in a
  separate PR, ROADMAP P4.3). Not covered: the Claw steal and the HUD
  Digivolution name (P7.7; the owner has no Claw yet), rumble, docked mode, an underrun
  count (P4.3), P5.1 (the 2 h memory session) and the P5.4 notice and assert
  (PR 2). The widescreen PRs #202 and #205 are not in rc7. The `dw3.log` heap
  high-water stayed at 1685K. The top of the log latches a crash of the
  previous run (`preview.29-19-ga2683c3`, the P5.4 `--force-crash` build), not
  an rc7 crash.
- **preview.30-rc7, 2026-10-23, second run (OLED handheld, US, New Game;
  log `dw3_ponte.log`, 15734 lines, about 5100 windows).** Owner report: the
  New Game bridge scene ("Oh yeah! I need my partner Digimon data!", patch
  `0058`) **no longer crashes on US hardware**; it was the last open
  hardware check of that patch (EU passed on preview.29). The log shows the
  build string `0.6.5-preview.30-rc7`, `runtime version=us`, no crash,
  abort or assert line, and ends with `applet exit: flat saves flushed`.
  Turbo (R3) went to 4x 7 times and back to 1x 6 times; the 4x windows are the P6.22
  numbers in [P6.4](P6.4_FAST_FORWARD.md#new-game-opening-measured-from-the-rc7-log-2026-10-23).
  One `frame dt` window is 9391087230 us (about 2.6 h): the console slept,
  again unclamped and with no `applet focus:` line (see P10.2).
- **preview.30-rc8 candidate, 2026-10-23 (OLED, US; main `a7860bf`, PSY-Z
  `ef67c4a`; folder `/mnt/cabal/tmp/dw3-preview30-rc8-out/`).** Owner feedback
  only: **no log was provided for this run**, and the owner did not say
  whether it was handheld or docked, so none of the items below is claimed for
  one mode. Fixed or verified:
  - **Music loop (#209):** the BGM now loops in the field, the inn and
    battle (the rc7 "plays once and stops" bug is fixed).
  - **EU title credits (#210, patch `0073`):** the three native credit lines
    appear as expected. Verified on the OLED (previously PC and Ryubing only).
  - **Widescreen (#205 patch `0071`, #202 patch `0072`):** the area name
    banner and the technique backdrops extend to 16:9 correctly. The
    battle-start circle/spiral effect also extends to 16:9 correctly, as the
    owner saw it. ROADMAP P6.1 listed it as still 4:3 and needing a decision;
    this is the owner's observation only, not yet explained or captured, so
    the roadmap item is adjusted, not closed.
  **Open (new in rc8):**
  - **Music stops after a won battle (P4.3, fix in progress in a separate
    PR):** after winning a battle the music is silent on the results screen
    and back on the map. The SFX for exp and level up still play. Not
    reported in rc7 (possibly hidden by the loop bug; not checked).
  - **Post-battle results screen shows black side bars** in 16:9. Not a code
    regression: the results screen was always 4:3 (PC captures of rc7
    `b0aed93` and `a7860bf` are identical through the whole battle, report
    and field fade), and it stood out once the battle, banner and technique
    backdrops went 16:9. Fixed by patch `0074` in PR #213, pending an OLED
    check (ROADMAP P6.1).
  - **Items menu is still 4:3:** the transition effect when opening it is 4:3
    and the menu itself stays 4:3 while the background is 16:9. Recorded as a
    future item (ROADMAP P6.1, "Items menu redesign for 16:9"), not a
    regression of this round.
  **Performance evidence for P6.22 (no log, owner HUD reading):** in the
  Forest inn the FPS reads 120+ walking to the bottom corner of the room and
  about 80 at the top. The owner expects the same elsewhere. The cost of a
  field frame depends on what is on screen
  ([P6.4](P6.4_FAST_FORWARD.md#field-fps-depends-on-what-is-on-screen-rc8-oled),
  ROADMAP P6.22). Not covered: the area-change iris (not looked at), US vs EU
  (US only), docked vs handheld (not stated), the EU credits on any language
  but the one shown, rumble, the underrun count (P4.3), P5.1.

Still open on hardware: audio underrun count, the full overlay sweep, battle
at 60 Hz, the sync-object error above, a fresh-save relaunch reload, and EU
movie and playthrough acceptance. See [ROADMAP](ROADMAP.md).
