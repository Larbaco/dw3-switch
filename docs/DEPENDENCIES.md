# Dependencies: pins, upstream changes, bump plan

## Decisions (owner, 2026-10-31)

* **Done in this PR (safe part):** SDL 3.4.16 to 3.4.18 (own commit; the pin
  lives in the fork, Larbaco/psyz#2), GitHub Actions majors (own commit), and
  the devkitA64 Docker image pinned by digest `sha256:1fc388c3...` (= tag
  `20260219`, devkitA64 r29.2, gcc 15.2; **not upgraded**).
* **Deferred: decomp + PSY-Z merge.** They go in **after D1 to D5 have landed
  and before v0.9.0**, each with the full replay tier on both regions
  (`tools/replay_tests.sh --jobs 2`) plus `--bin-both --both-subset`. Plan and
  patch list: sections 2 and 3 below.
* **Not now:** nugget, devkitA64 r30 / gcc 16.
* **SPU choice for the PSY-Z merge:** keep OUR mixer buses (P6.36) and the
  #188 voice-leak fix. Do **not** adopt upstream #203 (hardware-accurate SPU)
  wholesale. Take only the parts of #203 that fix a concrete bug, ported on
  top of our code, each with an OLED audio check.

Survey of 2026-10-31 (owner request: what changed upstream since about
2026-10-01, does it affect us, then bump). **Phase A only: nothing was bumped,
built or replayed.** "Impact" below comes from reading commit titles, file
lists, a trial rebase of our patches and a trial merge of our psyz fork; the
trial results are mechanical (they say what conflicts), not behavioural (nobody
has run the game on the new code).

## 1. Inventory

| Dependency | Pin (source) | Latest | Behind | Impact |
|---|---|---|---|---|
| `vendor/dw3_decomp` (ReGame-Labs) | `791932b`, 2026-10-15 (submodule + `port/patches/DECOMP_PIN`); **bumped in B2, 2026-11-01** (was `b9caa6c`) | `791932b` | 0 | Done, see section 7 |
| `vendor/psyz` (our fork Larbaco/psyz) | `6b0993e`, merge of upstream `f8780dc` (**B2, 2026-11-01**; was `192c006`, then `5328a18`); see section 7.2 | upstream Xeeynamo/psyz `f8780dc`, 2026-10-25 | merge-base `6d01339` (2026-10-11): 32 upstream commits not in the fork, 26 fork commits not upstream | **Affects us, risky.** 7 files / ~18 hunks conflict; overlaps our widescreen gutters, SPU buses, pacing; see 2.2 |
| psyz `external/SDL` | `fa2c02b` = `release-3.4.16`, 2026-09-02 | `release-3.4.18`, 2026-10-02 | 73 commits, 1 release | Neutral (bugfixes, nothing we use). Safe |
| psyz `nugget` | `020793e`, 2026-09-27 | `eb6cac5` (main) | 599 commits | We build only `nugget/bsdec` (movies). bsdec changed (`463ad72`, 2026-10-14, new lookup tables). **Risky for movies**, no benefit besides speed |
| psyz `external/cimgui` | `d3f0c2f`, 2026-04-06 | `2a6a93f` | 48 commits | Debug UI only. Not reviewed; skip |
| psyz `decomp/tools/maspsx`, `asm-differ` | psyz pins | not checked | n/a | PS1 matching toolchain. Not used by PC/Switch builds |
| devkitPro image `devkitpro/devkita64:latest` (`tools/dk.sh`, `build_switch_game.sh`, CI) | Local image created 2026-02-19: devkitA64 r29.2-1, gcc 15.2.0-7, binutils 2.45.1-2, libnx 4.12.0-1, switch-cmake 1.5.2-1 | Docker tag unchanged (2026-02-19); **pinned by digest in this PR**; pacman repo has devkitA64 r30-1, **gcc 16.1.0-1**, binutils 2.46.0-1; libnx has no update | one toolchain release | **Risky**: first gcc major bump since the port started; not pinned (`:latest`), so CI/a pulled image can move under us. Defer |
| CMake minimum | 3.21 in 4 files; local 3.30.5 | n/a | n/a | None. Leave |
| Python (CI) | `pip install pytest pytest-xdist pillow` unpinned (ci.yml:199); local venv pytest 9.2.0, Pillow 13.0.0 | same (pip index) | 0 | None |
| GitHub Actions | `checkout@v4`, `cache/{save,restore}@v4`, `upload-artifact@v4`, `softprops/action-gh-release@v2` | checkout v7.0.1, cache v6.1.0, upload-artifact v7.0.2, action-gh-release v3.0.3 | 1 to 3 majors | CI only. Safe in a docs/CI-only PR; v4 still works. Runners are `ubuntu-24.04` |
| Ryubing | local build at `/mnt/windows/Jogos/emuladores/Ryubing/build/Ryujinx` (`tools/replay/ryubing.py`) | not checked (release endpoints not found by `gh`) | ? | Test tool only; owner-managed |
| realesrgan-ncnn-vulkan (mods tooling) | not pinned in the repo files I read | v0.2.0 (2022-04-24) | n/a | Nothing to bump |
| ccache | 4.14.2 (`~/.cache/dw3-switch/bin`, `tools/get_ccache.sh`) | not checked | ? | None |

Not covered: `docs/BUILD*.md` pins other than the above, Python packages used
only by individual tools, apt packages in CI (`libstdc++-14-dev`, Mesa).

## 2. What changed, and what it means for us

### 2.1 dw3_decomp (9 commits, 837 files, most under `config/jp`)

* #40, #41: Japanese version added and built from the shared C. 408 files touched.
  `VERSION_JP` joins `VERSION_US/EU` in `include/version.h`; `PAL_VIDEO` is
  defined as `VERSION_EU`. Our CMake passes `VERSION_US=1` or `VERSION_EU=1`,
  which `version.h` still accepts (read, not compiled). Many `#if VERSION_EU`
  blocks became `#if PAL_VIDEO` or `VERSION_EU || VERSION_JP`.
* #43 "reduce the hacks", #45/#46/#47: new names (`include/engine/story.h`, new,
  1984 lines). `FIELD_PROGRESS_MOVIE_BATTLES` and `FIELD_PROGRESS_EXTRA` became
  `STORY_*`. We reference the old names in `port/walkthrough/walkthrough_steps.c`
  (comment), `tools/tests/test_walkthrough.py` (greps `include/field/field_map.h`
  and `field.c` text) and `tools/replay/manifest.py`. Those need an update with the bump.
* #48: menus' panel stretch written as `STRETCH_SPRITE` macros. This is what our
  widescreen patches edit.
* Other identifiers removed from `include/` and `src/` that appear in `port/`,
  `platforms/` or `tools/`: none (heuristic: names of 6+ characters).

Patch check: the 131 patches were committed in order on the old pin, then
cherry-picked (3-way) onto `791932b`. **11 conflict**; the rest apply clean:

| Patch | Files in conflict | Upstream cause |
|---|---|---|
| 0010 game-entry-boot-tim-title | `engine/game/state.c` | #41 |
| 0018 overlay-64bit-audit | `field/field_mode.h`, `menus/gym/scene.c` | #41, #47 |
| 0026 p34-menu-evidence | `field/field_mode/field.c`, `menus/gym/screen.c` | #41, #43, #45, #48 |
| 0029 p35-cardgame-evidence | `cardgame/card_battle/battle.c` | #41, #43, #46 |
| 0035 p36-playsound-keyon-log | `engine/sound/sound.c` | #41 |
| 0041 field-tile-one-per-frame | `engine/gfx/display.c` | #41 (PAL_VIDEO) |
| 0050 p92-native-title-canvas | `menus/title/title.c` | #41 |
| 0076 p61-widescreen-menu-layout | `engine/menu/field_menu.c`, 7 `menus/field_menu/*.c` | #48 |
| 0102 p61-widescreen-card-shop | `cardgame/card_packs/shop.c` | #43 |
| 0104 p61-widescreen-digimon-lab | 6 `menus/digimon_lab/*.c` | #48 |
| 0179 p61-widescreen-item-shop | `menus/item_shop/{info,shop,trade}.c` | #48 |

Applying the series naively (no rebase) fails 70 patches, but that is the
cascade from these 11. Rebase them in this order; patches that stack on a
conflicting one (many `p61-widescreen-*`, `p620-cheats-*`) must be re-checked
after each. The next free patch number is 0199.

Nothing in the new decomp helps the port right now except naming and the JP
version (out of scope). Value of bumping: keeping the next upstream sync small.

### 2.2 psyz (our fork vs Xeeynamo, 32 new upstream commits)

Trial merge of `upstream/main` into `192c006`: conflicts in
`platform/sdl3_gpu.c` (11 hunks), `psyz_spu.c` (3), `sdl3_common.h`,
`spu_voice.h`, `tests/CMakeLists.txt`, `tests/test_libspu.c`, and
`psyz/src/psyz/libpress.c` (deleted upstream, modified by our fork).

Affects us (titles and files, **diffs not yet read**):

* #219 widescreen support / resolution override and #216 custom-resolution API:
  overlaps our `Psyz_VideoSetGutter`, side gutters and `Psyz_VideoCopyGutters`
  (we render widescreen ourselves). Biggest merge risk; may let us drop code.
* #203 hardware-accurate SPU, #205: `a7862ba` is the upstream fix we backported
  by hand as `2caa505`; #203 collides with our mixer buses (`fb187f5`) and the
  default-mix test. Audio change: needs an OLED/audio check, not only replay.
* #211 framerate pacing, #206 `VSync(n)` waits n blanks, #217 VSync hook before
  InitPAD, `ReadPads` input-replay fix: can change frame timing and input
  timing, which the replay suite keys on. Expect drift to investigate.
* #213 asynchronous `StoreImage`, #222 prims/drawenv order, #209 negative
  offsets, #204 poly corner sampling: pixel-level effects, so refs may drift.
* #202/#210 native `DecDCTvlc` / MDEC wiring: our fork has a weak
  `DecDCTvlc` stub so `port/movies/dct_vlc.c` + nugget `bsdec` overrides it.
  Check that the override still wins and movies still decode.
* #224 async memory card, #223, #220, #194: API additions; neutral.
* Switch note: psyz still builds for Switch without SDL (`8dbed91`); upstream
  `psp` changes do not touch it.

### 2.3 SDL 3.4.16 to 3.4.18

Release notes of 3.4.18: crash fixes at startup/shutdown (Windows EZFRD64, X11
IME), KMSDRM, PS2, joystick/gamepad ids, pen events, pre-1970 time. Nothing
we use is listed. 3.4.17 does not exist as a release.

### 2.4 devkitA64

`tools/dk.sh` and `build_switch_game.sh` use `devkitpro/devkita64:latest`; the
Docker Hub tag was last updated 2026-02-19, so a plain pull changes nothing.
`dkp-pacman -Syu` inside the image would move gcc 15 to 16 and binutils to
2.46. libnx 4.12.0 has no newer package. A new gcc is a real risk for the
AArch64 build (new warnings as errors, UB-exploiting optimisations in 1990s C);
it is not a security fix and nothing in the port needs it.

## 3. Recommended bump set and order

0. Order below is the plan; items 1 and 2 (and the digest pin) are done in
   this PR, items 3 and 4 wait for D1 to D5 and land before v0.9.0.
1. **SDL 3.4.18** (psyz submodule `external/SDL`): trivial, own commit; PC
   build only, no engine impact. Needs the fork PR because the pin lives in the
   fork.
2. **GitHub Actions** majors (`checkout`, `cache`, `upload-artifact`,
   `action-gh-release`): CI only, own PR, check that `switch-game` and the
   release workflow still pass (`softprops/action-gh-release@v3` is the one
   used for publishing: try it on a PR-only path first, or leave it for last).
3. **dw3_decomp** to `791932b`: rebase the 11 patches (0199 free for anything
   new), rename `FIELD_PROGRESS_*` users, update the walkthrough tests. Rebuild
   PC US/EU/both and Switch combined. Engine-wide: FULL replay on both regions
   plus `--bin-both --both-subset`.
4. **psyz** merge of upstream into Larbaco/psyz (branch + PR on the fork, no
   force-push), then move the pin. Do it **after** the decomp (separate
   commits, so a replay drift can be bisected to one of them), full replay on
   both regions, an audio check of the SPU mixer on OLED, and a movie check.

Decomp and psyz can be separate PRs or one PR with separate commits; one
commit per dependency either way.

## 4. Do NOT bump yet

* **nugget** (599 commits; we only need bsdec): the one bsdec change is a
  speed-up, not a fix. Take it only with a dedicated movie-frame comparison
  (all movies, both regions). Revisit after item 4.
* **devkitA64 r30 / gcc 16**: no benefit, real build risk. Do it in its own
  change, after the preview/beta settles, and pin the image digest when you do.
* **cimgui**: debug UI, no reason.
* **maspsx / asm-differ**: unused by the port.
* **CMake minimum, Python packages**: nothing to gain.

## 5. Questions answered (owner, 2026-10-31)

* psyz #203: keep our mixer buses and the #188 voice-leak fix; port only
  concrete bug fixes from #203 on top, with an OLED audio check. The psyz
  merge conflicts in `psyz_spu.c`, `spu_voice.h` and `test_libspu.c` are
  therefore resolved in favour of our side first.
* The devkitpro image is pinned by digest (`tools/dk.sh`,
  `tools/build_switch_game.sh`, `tools/build_switch_launcher.sh`,
  `tools/symbolize_crash.py`, CI job `switch`). `DW3_DK_IMAGE` overrides it.
  To upgrade later: pull, test, replace the digest in those five places.

## 6. Method and limits

* Fetched `origin/main` of the decomp and `upstream/main` of psyz in this
  worktree; scratch clones for the trial rebase/merge in
  `/mnt/cabal/tmp/dw3-deps-survey` (not committed).
* Not done: reading the psyz diffs listed in 2.2, checking Ryubing/ccache
  versions, `docs/BUILD*.md` pins.
* SDL bump check (this PR): PC US, EU and both built; Switch combined `.nro`
  built with the digest-pinned image; `tools/replay_tests.sh --changed
  --jobs 1 --fail-fast` (47 scenarios, `vendor/psyz` has no rule so it ran the
  engine-wide area sweep plus canary): **459 checks, 0 failed, 0 skipped** (2026-10-31, EU default plus US canary; logs in `/mnt/cabal/tmp/dw3-deps-bump/replay-out`).
* Actions bumps are only exercised by this PR's CI run; `switch-release.yml`
  (action-gh-release v3) can only be proven by a real tag, inputs were checked
  against v3's `action.yml`.

## 7. Phase B2 results (2026-11-01)

### 7.1 dw3_decomp b9caa6c to 791932b (this PR)

* **Patches.** 28 of the 132 patches needed a rebase, not only the 11 the survey
  listed: 0010 0018 0026 0029 0030 0031 0035 0041 0042 0043 0044 0050 0051 0055
  0060 0076 0080 0102 0103 0104 0123 0140 0160 0165 0175 0179 0185 0191. The
  other 17 stack on the 11 (they touch the same lines). Numbers and intent are
  unchanged; the next free number is still 0201. The rebase was done in a scratch
  clone (one commit per patch, `git rebase --onto 791932b`), and the patch files
  were regenerated from the rebased commits. Check: the patched tree equals the
  rebased branch tip tree for tree; `tools/apply_patches.sh --check` passes.
* **STRETCH_SPRITE (#48).** The widescreen menu patches (0076, 0103, 0104,
  0179) now pass the shifted x through upstream's `STRETCH_SPRITE` /
  `ZOOM_SPRITE` macros instead of the expanded `setScale` / `setPivot` blocks.
  `DRAW_PAGE_PANELS` has a `DRAW_PAGE_PANELS_AT(sprite, screen, i, left)` form
  (`left` is `DW3_MENU_LEFT` for the widened screens); `DRAW_PAGE_PANELS` is the
  same macro with no shift, for the demo screen, which 0076 never widened.
  0175 (battle spiral gutters) follows upstream's `PrimPtr` union.
* **Renames.** `FIELD_PROGRESS_*` is `STORY_*` (`include/engine/story.h`). Updated:
  `port/walkthrough/walkthrough_steps.c`, `tools/replay/manifest.py`,
  `docs/P6.35_WALKTHROUGH.md`, `tools/tests/test_walkthrough.py`. The walkthrough
  tests read the decomp's text: they now read the `STORY_*` values from
  `story.h`, expand the named flags, items and special conditions back to
  `FLAG(g, i)` / `ITEM(k, i)` / `SPECIAL(n)`, put the `u16` code lists on one
  line, and use `PLACE` / `PLACE_ARG` for what was `WARP_ARG`.
  `test_overlay_table_stages.py` and `test_save_convert.py` ignore the JP
  entries and accept `#elif VERSION_EU || VERSION_JP`.
* **JP.** Not built, no support added. `VERSION_US=1` / `VERSION_EU=1` still
  select the build; the JP blocks are never compiled.
* **Builds.** PC US, EU and combined; the Switch combined `.nro` (digest-pinned
  image). Same warnings as before (55 in the US PC build; the ScriptCommand
  `create` pointer type, `OVL_NAME`).
* **Tests.** `pytest tools/tests`: all pass.
* **Replay (full tier, both regions, `--jobs 1`, `nice`, private refs copy).**
  US 240, EU 296, cross-region 12, and `--bin-both --both-subset` with the
  combined binary. Every picture drift and every failed check is also present,
  with pixel-identical captures, on an unmodified `main` (`aed7647`) build of the
  same scenarios, so there is **no regression from the bump** (0 px of
  difference between the decomp bump and `main`). What fails on `main` and here
  alike (stale expectations, not touched by this PR):
  * `us|eu-glyphs-*` log lines expect `map=playstation`; the default is
    `nintendo` since D2 (#408);
  * controls and help pictures (`us|eu-controls-custom`, `us-help-field`,
    `eu-help-field`, `eu-help-title`, `us|eu-options-mapbtn`);
  * 112 to 125 px drifts in the field menu family (`eu-field-options*`,
    `us|eu-nintendo-menu`, `eu-return-title*`, `eu-items-use-stays*`,
    `eu-map-view-43|off`, `eu-restart-save-continue`,
    `eu-options-region-relaunch-game`).
    The combined-binary subset (`--bin-both --both-subset`, 65 scenarios, 865 checks)
  gives the same 31 failed checks on this branch and on `main`, with identical captures.
  References were not refreshed. A reference refresh for these is a separate
  change.
* **Evidence:** logs and the drift table in `tmp/evidence/2026-11-01-deps-b2/` of the main
  checkout (not committed). Not covered: nothing ran on the Switch; the new `.nro` was only built.

### 7.2 psyz: merge of Xeeynamo/psyz f8780dc into Larbaco/psyz (PR 2)

* **Fork PR:** Larbaco/psyz#3, branch `deps/merge-upstream-f8780dc`, head `6b0993e`.
  Merge it with a **merge commit** (no squash) so the pinned SHA stays reachable.
  This repo's `vendor/psyz` points at that head.
* **Conflicts (7 files), resolved on the fork's side where the two overlap:**
  gutters (`Psyz_VideoSetGutter`, `Psyz_VideoCopyGutters`) kept on top of upstream's
  native-canvas draw path in `sdl3_gpu.c`; SPU: our mixer buses (P6.36) and the
  #188 voice-end fix kept, upstream #203 **not** taken; `libpress.c` deleted
  upstream (MDEC wiring and `DecDCTvlc` moved to `mdec.c` / `vlc.c`), upstream's
  `DecDCTvlc` is `weak` so `port/movies/dct_vlc.c` (bsdec) still wins; the PAL 240-line
  aspect fix is moot (upstream dropped `set_disp_vert`).
* **Widescreen #219 versus our P6.1 gutters:** ours stay. The game draws its own
  wide picture and PSY-Z only holds the side columns. Upstream's resolution
  override and canvas are in the merge but idle: `layout.native` is
  `request.w != 0`, and the port never calls `Psyz_VideoSetResolution`.
* **SPU audit (#203):** it is one accuracy rewrite (ADPCM shift/filter rounding,
  signed volume registers, envelope clock phase and key-off wait, voice end at
  sample 20, a measured gauss table). None of it fixes a defect this port has
  seen, each part changes the sound a little and needs an OLED listen, so none
  is ported. Candidates, if a defect turns up: `spu_volume()` (sign bit of
  volume registers), ADPCM shift 13 to 15 and filters 5 to 7, the envelope
  phase. Our bus tests (`test_libspu_bus`, `test_libsnd_loop`, `test_libspu`)
  keep passing: psyz unit tests 567 passed, 0 failed.
* **Port changes the merge needed:** `StoreImage` is asynchronous (upstream
  #213), so the four direct `Draw_StoreImage` calls (title logo and menu-label
  capture, title background capture, map view VRAM grab) became
  `StoreImage` + `DrawSync(0)` (without it the title logo capture read an empty
  buffer and the original PS1 logo stayed); `horizontal_resolution_368` is
  `_384` upstream (Switch GPU backend, field profiler).
* **Builds:** PC US, EU, combined, and the Switch combined `.nro`.
* **Replay (full tier on both regions + cross-region + `--bin-both --both-subset`,
  `--jobs 1`):** no process, log, marker or crash failure beyond what `main`
  already fails. The pixel drift against the decomp-only PR (same scenarios,
  same refs): 81 of 548 scenarios have at least one capture that differs (17 + 35 + 29 below).
  Explained by the upstream shader/raster commits, not by the port:
  * 17 scenarios, whole-frame differences of at most 8 levels per channel
    (door iris 4:3, menu instant exit, ...): untextured polygons are now
    truncated to 15 bit (`psx.frag`), as on the PS1.
  * 35 scenarios, differences of 3.8 thousand up to 68 thousand px of the 76800 px
    PS1 frame, large amplitude in places: textured polygon sampling (#204 corner sampling,
    #209 offsets; `resolveTexel` no longer steps back by the screen derivative):
    battle backgrounds and models (`*-battle-*`, `*-two-battles`, `*-restart-in-battle*`,
    `*-help-battle`), the news monitors (`*-field-walk`, `*-newgame-field`,
    `*-repro-balloon`), door iris and spiral transitions. The pictures look the
    same; a different texel is picked at polygon edges.
  * 29 title-family scenarios (`*-title*`, `*-lang-*`, `*-load-menu`,
    `*-title-quit*`): the PS1 frame is identical (0 px), the window picture
    differs in the innermost gutter column (three window columns beside the
    picture, dark noisy backdrop), same cause.
  References are not refreshed. If these differences are accepted, one refresh
  from this build replaces them.
* **Audio:** the turbo-music scenarios (`us|eu-turbo-music-field|battle|menu|hold|nohold|hold-stopped`),
  `*-music-loop`, `*-battle-music-loop`, `*-victory-music`: process, log and
  marker checks pass (only the battle pictures of `hold-stopped` drift). The
  music-tick windows are 15 ticks per 60 game frames as before (1233 window lines in both
  runs). Over the whole tier no `WARNING SsUtKeyOn found no free voice`, no
  `voices ran out`, no `output silent`, and every minute line has 0 failed key-ons. There is no tech-spam replay scenario; the
  #188 coverage here is the voice-end fix staying unchanged in `psyz_spu.c` plus
  those logs. OLED: item D16 of `docs/OLED_TEST_PLAN.md`.
* **Not covered:** nothing ran on the Switch (a Ryubing or OLED run of the new `.nro`
  is open); the Switch software raster and `nx_gpu.c` follow the same PSY-Z core, so
  the polygon sampling change applies there too and has only been compiled.
