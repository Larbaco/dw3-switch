# Releases

Each release ships ONE `.nro`, `dw3-switch.nro` (US + EU in one image), plus
`dw3-switch-elf.zip` (the matching `.elf` for crash reports) and `README.txt`
(the release notes). Never game data. Earlier previews shipped more files; each
release page lists what it carries. **Pick the newest unless it is in "Do not
install" below.** Every release so far is a GitHub prerelease; the first normal
release will be `v0.9.0` (the public beta, see
[Release criteria](ROADMAP.md#release-criteria)).

## Naming

Tags follow SemVer (owner decision 2026-10-31, #394). New previews are `v0.9.0-preview.N`, not `v0.6.5-preview.N`:

| Tag | What it is | GitHub | hbmenu shows (15 characters at most) |
|---|---|---|---|
| `v0.9.0-preview.N` (N from 1) | A preview on the way to the 0.9 public beta | prerelease | `0.9.0-p.N` (`0.9.0-preview.N` while N is 1 to 9) |
| `v1.0.0-rc.N` (N from 1) | A release candidate for 1.0, after the beta | prerelease | `1.0.0-rc.N` |
| `v0.9.0` | The public beta | release, marked latest | `0.9.0` |
| `v1.0.0` | The first full release | release, marked latest | `1.0.0` |
| `v0.6.5-preview.N` (1 to 57) | The old series, kept as published | prerelease | `0.6.5-p.N` |

**Why the change.** `0.6.5` was the roadmap id of the packaging task (P6.5)
when the first release workflow was written on 2026-10-10 (commit `6a1a9fd`).
It was never bumped, so all 57 previews carry it. The old tags, release
pages and index rows below stay as they are.

**Where the new series starts.** Previews from here on are
`v0.9.0-preview.1`, `v0.9.0-preview.2` and so on; preview numbers restart at 1.
The index rows below keep the short names `preview.N` for the old series; a row
for the new series is written `0.9.0-preview.N`. The next tag is
`v0.9.0-preview.1`. A build past a tag shows the `git describe` form
(`0.9.0-p.12+a0e4`, see [P6.37](P6.37_VERSION.md)).

**Gates** (the full text is in [Release criteria](ROADMAP.md#release-criteria)):

- `v0.9.0`, the public beta: plan p57 passes on the OLED with no blockers, and
  the options decisions D1 to D5 ([OPTIONS_REORG](OPTIONS_REORG.md)) are on `main`.
- `v1.0.0`: a full playthrough to the ending on the OLED (one region at least)
  with no known crash (BUG-1, the Sphaira exit crash, included) and the Phase 5
  gate ticked in the roadmap. `v1.0.0-rc.N` tags lead up to it.

All releases are on the
[releases page](https://github.com/Larbaco/dw3-switch/releases). Install
steps are in the [README](../README.md#play-it-on-switch) and the
[player guide](PLAYER_GUIDE.md).

## Do not install

| Tag | Why |
|---|---|
| [preview.1](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.1) | Crops the title and freezes the movie. |
| [preview.6](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.6) | Raised the CPU to 1785 MHz. Reverted afterwards. |
| [preview.9](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.9) | Enabled the battle mesh for every mode and broke the field picture. |
| [preview.19](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.19) | A GPU diagnostic cut, not a playable baseline. |

## Index

Newest first. One short line per release; the full text of each is on its release page. The old series
is `preview.N` (tags `v0.6.5-preview.N`); rows for the new series (`0.9.0-preview.N`) go above `preview.57`.
Preview numbers 11 to 17 were never published.

| Release | What it was for |
|---|---|
| [0.9.0-preview.1](https://github.com/Larbaco/dw3-switch/releases/tag/v0.9.0-preview.1) | First of the 0.9.0 series: regrouped Options, instant pause-menu exit, PRESS START only when ready, condensed walkthrough, SemVer numbering. |
| [preview.57](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.57) | Digivolve in battle no longer crashes; restart in the chosen region; Minus (Select) opens the map from the field; more walkthrough. |
| [preview.56](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.56) | Fixes the Reliability Spot crash; map view area names and zoom; walkthrough chapters 2 to 10; experimental cross-region saves. |
| [preview.55](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.55) | Walkthrough screen in Options and Switch button glyphs; the full-screen map is off by default. |
| [preview.54](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.54) | One release file, dw3-switch.nro, for US and EU. Using an item stays on the partner step. |
| [preview.53](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.53) | Custom button remap, turbo music, a bigger equipment screen, and FR/DE/IT/ES help text (machine-drafted). |
| [preview.52](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.52) | PlayStation or Nintendo button layout; equip from the Items menu; battle list selection bar fixed. |
| [preview.51](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.51) | Options > Help with ten pages; faster builds for contributors. |
| [preview.50](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.50) | EU language select takes the confirm button; map sprites keep their size; faster turbo in scenes. |
| [preview.49](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.49) | Item shop in 16:9, scrolling Cheats screen, stage sweep, and a combined .nro that finds your disc image. |
| [preview.48](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.48) | One .nro for US and EU with a region picker; first map mod loader. |
| [preview.47](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.47) | Map view gets the game's cursor and location name; EU language select in 16:9. |
| [preview.46](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.46) | Options becomes a list of groups (Display, Sound, Gameplay, Saves) in the title menu's style. |
| [preview.45](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.45) | Fixes dw3.log rotation on the Switch file service. |
| [preview.44](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.44) | Options opens without the stutter; map view shows revealed areas; log rotation fix. |
| [preview.43](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.43) | Saves per Switch user profile; dw3.log keeps the last five runs. |
| [preview.42](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.42) | App name is now Digimon World 3 NX in hbmenu and Sphaira. |
| [preview.41](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.41) | Turbo menus at 1.5x; evenly spaced title plaques. |
| [preview.40](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.40) | Change partner in the equipment screen, auto-save in its own file, map view prototype. |
| [preview.39](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.39) | Quit game no longer ends with a system error; field menu OPTIONS no longer shows a black square. |
| [preview.38](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.38) | The battle-start swirl covers the whole 16:9 picture. |
| [preview.37](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.37) | Options in battle (Minus at the command menu); safer save writes. |
| [preview.36](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.36) | Options reachable from the field menu; title textures freed outside the title. |
| [preview.35](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.35) | Faster turbo outside battle; player guide covers Auto-save. |
| [preview.34](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.34) | Auto-save, off by default. |
| [preview.33](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.33) | Quit game, Return to title, and a one-time cross-region save import. |
| [preview.32](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.32) | Canal water fix; 16:9 for card game, Digimon Lab and door transitions; Cheats page 2. |
| [preview.31](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.31) | Field drawing 4 to 5 times faster; many menus in 16:9; Cheats screen; ELF zip for crash reports. |
| [preview.30](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.30) | Music loops again; Options saved to a file; volume, encounter rate and text speed rows; many widescreen and crash fixes. |
| [preview.29](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.29) | Fixes the new-game "need my partner" crash; much quicker Options screen; HOME/sleep save flush. |
| [preview.28](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.28) | EU restart returns to a clean language select; carries the preview.26 and 27 changes. |
| [preview.27](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.27) | Field streaming fix for the 16:9 crash seen in preview.26; DW3NX artwork. US only. |
| [preview.26](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.26) | EU language select at boot, battle camera and portrait fixes, Options on its own screen. |
| [preview.25](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.25) | Widescreen on by default, fast-forward on R3, battle defaults to gpu-tex, first EU builds. |
| [preview.24](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.24) | Native title menu on Switch; EU runtime extraction. US only. |
| [preview.23](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.23) | Same play as preview.22 on Switch; PC title menu and docs only. |
| [preview.22](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.22) | Widescreen option, restart hotkey, GPU battle background fix. |
| [preview.21](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.21) | Opening-movie read-race fix; movie pace from 6.75 to about 15 fps. |
| [preview.20](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.20) | Port options entry on the title menu (switches were stubs); cheaper movie polling. |
| [preview.19](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.19) | GPU diagnostic cut for the OLED battle investigation. Not playable; do not install. |
| [preview.18](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.18) | Selectable battle render modes, flat saves, debug HUD. Preview numbers 11 to 17 were never published. |
| [preview.10](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.10) | Battle mesh limited to the fight stage; field back on the software blit. |
| [preview.9](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.9) | Battle mesh triangles on GLES3. It broke the field picture; do not install. |
| [preview.8](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.8) | Holds one field map in RAM so later tiles copy from that buffer. |
| [preview.7](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.7) | Uploads only the sampled VRAM rectangle, one field tile per frame. First build to reach field and battle on an OLED. |
| [preview.6](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.6) | Present-path and tile-loading speedups, plus a 1785 MHz CPU boost removed in preview.7. Do not install. |
| [preview.5](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.5) | Copy progress bar, left-stick input and US extraction from the disc image. |
| [preview.4](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.4) | Movie test with the disc-image lookup fixed. |
| [preview.3](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.3) | Hardware test of the opening movie (paced CD, restored STR files). |
| [preview.2](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.2) | Picture fix: display-mode title width and the full-height movie. |
| [preview.1](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.1) | First preview: boots to the title in Ryubing. Crops the title and freezes the movie; do not install. |

## Release notes style

Owner decision 2026-10-31: the notes were far too long (the preview.57 body ran
to 1100 lines). Every release now follows the usual changelog style (Keep a
Changelog, GitHub release notes, well-run homebrew projects). The same text is
the GitHub release body and the `README.txt` asset
(`platforms/switch/release/README.txt`, the template), so it stays plain text of
**about 60 lines at most**. A release body has these parts, in this order:

1. **Title line and a 1 to 2 line summary.** If the preview must not be used,
   a `WARNING - Do not install this preview: ...` line comes first.
2. **Highlights**: 3 to 6 bullets, player-facing, one line each.
3. **Fixed**: one line each.
4. **Changed**: defaults and options that moved, if any.
5. **Known issues**: short, only what a player would notice.
6. **Install / update**: two or three lines and a link to the
   [player guide](PLAYER_GUIDE.md). No setup walkthrough.
7. **Downloads**: the files actually published, one line each.
8. **Full changes**: the compare link `vPREV...vNEW` and the PR numbers, then
   the short own-disc / not-affiliated line.

Leave out of the notes (they live in the PRs, `ROADMAP.md`, `OLED_TEST_PLAN.md`
and the other docs): patch numbers, OLED row ids and checklists, replay and
test counts, log lines, "built by the workflow" lines, test-tooling-only PRs,
and the history of earlier previews ("From preview.N" blocks). One bullet per
user-visible change, not per PR.

Skeleton (copy it, fill it, keep each bullet to one line):

```
Digimon World 3 / 2003 for Switch (homebrew) - v0.9.0-preview.N

<One or two lines: what this preview is for.>

## Highlights
- <player-facing change>

## Fixed
- <what was broken, now works>

## Changed
- <default or option that moved>

## Known issues
- <what a player may hit>

## Install / update
Copy dw3-switch.nro to sdmc:/switch/ and start it from your launcher. ...
Setup, controls, saves and crash reports: <player guide link>

## Downloads
- dw3-switch.nro        US + EU in one file; the region is chosen at boot
- dw3-switch-elf.zip    symbols to read crash reports (matches this .nro only)
- README.txt            these notes

## Full changes
https://github.com/Larbaco/dw3-switch/compare/<prev tag>...<this tag>
PRs: #a #b #c

No game files are included: bring your own disc. Unofficial fan project, not
affiliated with Bandai Namco or Nintendo. Digimon is a trademark of its owners.
```

Past releases (preview.1 to preview.57) were rewritten to this style on
2026-10-31; the long original bodies are kept in
`tmp/release-bodies-backup-2026-10-31/` on the maintainer's machine, and each
release's `README.txt` asset still holds the original long-form text.

## Publishing a release

A tag `vX.Y.Z`, `vX.Y.Z-preview.N` or `vX.Y.Z-rc.N` (or `workflow_dispatch` with
such a tag) runs `.github/workflows/switch-release.yml`. It rejects any other
shape, and `tools/dw3_nro_version.py --release-kind <tag>` is the one place that
decides what the tag is: a `-preview.N` or `-rc.N` tag is published as a
GitHub prerelease and is not marked latest; a plain `vX.Y.Z` is a normal
release, marked latest (title `dw3-switch v0.9.0 public beta` for 0.x.y). The
workflow builds the combined US + EU image
(`tools/build_switch_game.sh --both`) and publishes exactly three files:
`dw3-switch.nro`, `dw3-switch-elf.zip` and `README.txt`. The zip holds the
unstripped `dw3-switch.elf` plus `BUILD_IDS.txt` (GNU build id, `.elf` and
`.nro` per line). `tools/pack_release_elfs.py` makes it and fails the job when
the `.elf`'s build id is not the one in the `.nro` header. A local rebuild does
not reproduce a CI binary (the build ids differ), so the CI `.elf` is the only
one that symbolizes a CI `.nro`.

**One .nro only (owner decision 2026-10-29: "pq tanto nro? eu quero um
unico").** Releases no longer carry `dw3-switch-eu.nro`, `dw3-switch-both.nro`
or any `-nxlink` file. The per-region builds (`--eu`, default US) and the
dev-only nxlink build (`--nxlink`, with the HUD and the sampled debug lines)
still build locally and in CI, but are not published. The single file detects
an nxlink launch at run time (`__nxlink_host`), see
[SINGLE_BUILD](SINGLE_BUILD.md#one-release-file-2026-10-29). With one region's
data on the SD it starts that game; with both it shows the picker. Crash
reports are symbolized with `dw3-switch-elf.zip`; the report's `disc:` line
says the region. Upgrade note for the release notes: delete the old
`dw3-switch-eu.nro` / `-both` / `-nxlink` files, recreate any forwarder; saves
and region folders are unchanged.

**Cutting a tag.** From an up-to-date `main` whose notes PR is merged:

```
git fetch origin && git checkout main && git pull --ff-only
git tag -a v0.9.0-preview.1 -m "dw3-switch v0.9.0-preview.1" <sha>
git push origin v0.9.0-preview.1
```

Without a local tag, `gh workflow run switch-release.yml --ref main -f
tag=v0.9.0-preview.1` does the same from the GitHub side. Watch the run and do
not cancel it.

To test the packing without a release, run the script on local builds
(`tools/tests/test_pack_release_elfs.py` covers it in `pytest`). The
workflow itself has no dry-run path: `workflow_dispatch` always publishes
the tag it is given. A hand-built release (as preview.53) follows the same
rule: the three files above, nothing else.

**Release notes PR checklist.** The notes PR (merge it first, then tag the
notes commit; the workflow publishes `README.txt` from the tag) does three
things:

1. Rewrites [`platforms/switch/release/README.txt`](../platforms/switch/release/README.txt)
   for the new tag, following "Release notes style" below.
2. Adds one row to the index above (at most 25 words, a link to the release).
3. Updates the [README](../README.md) "Latest preview" link and its Status
   rows, so the front page does not go stale.

Before writing "Known issues", prune the list against the PASSED marks in
[OLED_TEST_PLAN.md](OLED_TEST_PLAN.md) and the owner's latest logs: list only
items that are really still open, and only the ones a player would notice. Run
`python3 tools/oled_open_rows.py` for the plan rows that have no clean PASSED
mark. Row ids, the full OLED checklist and test-method detail belong in the
plan and in the PRs, not in the notes.

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
