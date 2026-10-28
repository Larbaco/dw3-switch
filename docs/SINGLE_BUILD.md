# P6.24 One build for US and EU (feasibility survey)

Owner request (2026-10-24): "Build única pra US e EU". One `.nro`, and one PC
binary, that runs the US and the EU disc, instead of the separate builds of
today. This note is the survey behind the
[ROADMAP P6.24 entry](ROADMAP.md#phase-6-goal-notes). The owner has since
decided how saves and the boot choice work (next section). **Slice 1 is built
(2026-10-25): the run-time region choice, the picker logic and a PC launcher,
see [As built](#as-built-slice-1) below.** Slice 2
(2026-10-25) adds the Switch launcher `.nro`, see
[As built, slice 2](#as-built-slice-2-the-switch-launcher). **Slice 3
(2026-10-28, owner priority) is the dual-image link itself: one `dw3-pc` with
both game copies, see [As built, slice 3](#as-built-slice-3-the-dual-image-link-pc);
the Switch half (one `.nro`, the boot picker in the image) is
[slice 4](#as-built-slice-4-the-combined-switch-nro).** Numbers
come from `origin/main` at `3e2a9b7`, the decomp at
`b9caa6c` (`vendor/dw3_decomp`), the PC objects of a US build, and the
preview.31-rc1 Switch builds (US and EU, `.nro` and `.elf`).

## Owner decisions (2026-10-24)

Final; they replace the matching open questions at the end of this note.

1. **Saves are unified between US and EU.** A save made in one region loads in
   the other. The save images are not the same format, so this needs a
   converter; it is its own item, **P6.25**, with the field-by-field survey and
   the design in [CROSS_REGION_SAVES.md](CROSS_REGION_SAVES.md). This
   supersedes the earlier assumption here that "saves and options stay per
   region".
2. **With both game images on the console, ask at boot**, with a short timeout
   (default selection = the last choice, else a sensible default) and a "don't
   ask again" option that is stored and can be changed later. Design in
   [Boot picker](#boot-picker) below.
3. **Saves are bound to the Switch user profile that is playing.** Its own item,
   **P6.26**, in [PER_USER_SAVES.md](PER_USER_SAVES.md). The user choice is the
   Switch profile selector (`pselShowUserSelector`), decided 2026-10-24.

## Owner priority (2026-10-28) and orchestrator defaults

On 2026-10-28 the owner made **one `.nro` that plays both US and EU the top
priority** (ROADMAP P6.24, Tier 0 of the Priority section). The work is
option (a) below, in two PRs: PR A is the PC (`DW3_VERSION=both`, the dual-image
link, the P6.15 replays of both regions on the one binary), PR B is the Switch
(`tools/build_switch_game.sh --both`, auto-detection on the SD card, the boot
picker, CI and the release asset).

The questions that were still open on 2026-10-28 got these answers from the
orchestrator, so work could start without waiting. **Orchestrator defaults,
owner may change any of them:**

| Question | Default taken |
|---|---|
| Look and languages of the picker | A simple host screen in English. Title-menu style comes later, with the P9.9 work |
| Hold-to-ask button | **Minus**, held while the `.nro` starts |
| Size | About 12 MB for the combined `.nro` is fine |
| Separate US / EU `.nro` files | Keep shipping them next to the combined one until the owner has confirmed the combined one on the OLED |
| Switching region without quitting | Out of scope; changing region means quitting |

**Roadmap shape.** P6.25 (cross-region saves) and P6.26 (per-user saves) are
separate items and **P6.24 depends on both**: each can be built, tested and
shipped alone, P6.25 is useful before the single build (separate US and EU
builds can already share one save store), and they touch the save host
(`port/memcard`) and the boot code, not the game-side link that is P6.24's
hard part. The picker and the build change stay in P6.24.

**Implementation order.**

1. P6.25 slice 1: the converter and its tests on synthetic fixtures (PC only).
2. P6.25 slice 2: the store behind a switch, legacy import, preview for the
   separate builds sharing `saves/shared/`.
3. P6.26: user resolution (PC `default` profile first, then the Switch), the user
   folder, the import prompt, the global options file.
4. P6.24 slice 1 (`DW3_VERSION=both` on the PC, no visible change) can start in
   parallel with 1 to 3; it only needs the save work to be done before the
   **Switch** one-`.nro` release.
5. P6.24 on the Switch: auto-detect, the boot picker, CI on `both`, an OLED check.

## How the region is chosen today

The region is fixed **at compile time**, everywhere:

- **CMake.** `DW3_VERSION` (`us` | `eu`, default `us`) is a cache variable in
  `port/CMakeLists.txt`, `platforms/pc/CMakeLists.txt` and
  `platforms/switch/CMakeLists.txt`. It sets the macro `VERSION_US=1` or
  `VERSION_EU=1`, picks `port/overlays/overlay_table_us.c` or
  `overlay_table_eu.c`, and sets `DW3_DEFAULT_RUNTIME` (`runtime/<ver>` on PC,
  `sdmc:/switch/dw3/<ver>` on the Switch).
- **Scripts.** `tools/build_switch_game.sh --eu` (or `DW3_VERSION=eu`) builds
  into `build/switch-eu` / `build/switch-eu-nxlink`; both regions produce a file
  named `dw3-switch.nro`, renamed by hand for a release.
- **Decomp.** `include/version.h` requires **exactly one** of `-DVERSION_US`
  and `-DVERSION_EU` and turns them into 0 / 1 for `#if`. Upstream chose
  compile-time versions on purpose (matching builds of each disc).
- **CI.** `ci.yml` builds the PC host and the Switch game for **US only**;
  `switch-release.yml` publishes US only. The EU `.nro` is built and attached
  by hand ([RELEASES.md](RELEASES.md), "EU is a manual step"). AGENTS.md asks
  for every change to be tested on both discs, so every check runs twice.

### Region sites (counted with a small script, comments excluded)

| Where | Region `#if` lines | Blocks | Files |
|---|---|---|---|
| `vendor/dw3_decomp/src` + `include` | 1,310 (`#if VERSION_US` 618, `#elif VERSION_EU` 572, `#if VERSION_EU` 113, ...) | 731 | 352 |
| of which `src/field/stages` | 746 | 390 | 238 |
| `port/` | 6 | | 5 |
| `platforms/` | 8 | | 3 (`boot_main.c`, `nx_crash.c`, PC `main.c`) |
| `port/patches/*.patch` (added lines) | 13 | | 6 of 76 (`0010`, `0043`, `0050`, `0054`, `0055`, `0064`) |

What the 731 decomp blocks contain:

- **291 hold only `#define`s** (1,811 lines). Mostly file ids: each stage has
  `STAGE_TEXT` / `STAGE_FILE` / `EVENT_TEXT_FILE` per region (251 of the stage
  blocks), the 19 mode overlays in `include/engine/overlay.h`
  (`FILE_FIELDSTG` 0x158 US / 0x166 EU, `FILE_STSTATUS` 0x1F9 / 0x207), mode ids
  in `include/engine/game_state.h` (`MODE_BATTLE_MOVIE` 0xE09 / 0xE0A; EU adds
  `MODE_OPENING_2` 0xE02).
- **436 hold code or data** (11,277 lines). The largest are data tables:
  `field_mode/data/fieldstg.c` (1,729 lines), `engine/file/file_table.c`
  (1,728, the per-region sector table), `engine/text/text_tools.c` (702),
  `deck_editor/data/stcrddek.c` (698), `player_name/data/stplnmet.c` (698),
  `battle/report/data/stfgtrep.c` (675), `debug/stage_select/stage_select.c`
  (651), `partner_name/data/stdgname.c` (462).
- **Struct layouts differ.** `GameState` (`game_state.h`): the flag arrays from
  offset 0x2644 have other sizes, so every later field moves, and
  `GAME_SAVE_SIZE` is 0x26BC (US) against 0x26C4 (EU). `BattleFuncs`
  (`battle_mode/battle.h`): EU has `rollCounter` at 0xD0 and every later
  member is 4 bytes further. `include/menus/title.h` and `field_menu.h` have
  region members too. A US and an EU save are not the same format.
- **EU-only state.** `src/engine/data/eu_settings.c` defines `LANGUAGE` (2-6,
  set by `CNTY_SEL`), `NTSC_MODE` (0) and `SHIFT_PAL_SCREEN` (1) under
  `#if VERSION_EU`; `TEXT_FILE(file)` is `LANGUAGE + file - 1`
  (`include/engine/file.h`), so the EU text of the 5 languages is chosen at
  run time already, inside the EU build.
- The other 4 blocks mix `#define`s and code, or pick an `#include`.
- **EU-only stages.** EU has 55 more stages (WSTAG920-974,
  `config/eu/stages.txt`); the port weak-stubs them today.

### PAL and NTSC

The EU game calls `SetVideoMode(MODE_PAL)` unless `NTSC_MODE`
(`src/engine/system/main.c`) and branches on `NTSC_MODE` in `display.c`,
`sound.c`, `actor.c` and `actor_control.c`. PSY-Z paces off the mode the game
sets (59.94 Hz NTSC, 50 Hz PAL, `Psyz_VideoStats()`,
[P6.4](P6.4_FAST_FORWARD.md)); the Switch backend records `DisplayMode.pal`
but keeps the 240-line picture (`switch_gpu.c`, `nx_gpu.c`). So the **host is
already region-neutral for timing**: it follows the running game, not a build
flag. Only the game code knows PAL.

### Data layout in the image

The PS1 addresses, splat configs and linker scripts in
`vendor/dw3_decomp/config/{us,eu}` serve the matching PS1 build only. The port
compiles the C natively and **links every overlay into the image**
([ARCHITECTURE.md](ARCHITECTURE.md)): 255 `dw3data_*` sections (one per
overlay or stage, restored on a mode change) and 4 `dw3bss_*` sections, plus 21
`-Wl,--undefined=__start_dw3data_*` lines in each platform CMakeLists. Stage
objects already get their globals prefixed per stage
(`tools/prefix_stage_obj.py`, `WSTAG780_...`) and their writable sections
merged (`tools/rename_ovl_sections.py`, `merge_ovl_sections.ld`): the build
already renames symbols and sections per object, which is the tool a two-region
link needs. The one PS1 address the host still uses is `STAGE_VRAM` for
`boot_tim.bin` (`port/cdfs/runtime_extract.c`: 0x800A4CA4 US, 0x800A5DE0 EU).

### The host-game boundary

From `nm` over the PC objects of a US build (game = `dw3_engine`, `dw3_title`
and the 255 `host_ovl/*.host.o`; host = everything else but SDL):

- the host uses **25 symbols** the game defines: 17 in `overlay_table_us.c`
  (16 mode entry points such as `FIELDSTG_start` and `STSTATUS_start`, plus
  `STGDGLAB_createScene`), `GAME` (the three `port/options/title_*` files),
  `PAD` (`port_options.c`, `restart.c`, `turbo.c`), `SOUND` and
  `findSoundBank` (`port_options.c`), `FILE_COUNT` / `FILE_SECTORS` /
  `FILE_SECTOR_COUNTS` (`runtime_extract.c`) and `dw3_game_main` (PC
  `main.c`);
- the game uses **181 host symbols** (PSY-Z API, port hooks), which stay
  shared.

`dw3_port_options` (14 objects) and `dw3_host_overlays` are compiled with the
game's region macro and headers today (they read `GAME` and region file ids:
`widescreen.c`, `turbo.c`, `title_logo_capture.c`), so they sit on the game
side of the line or need a split.

### Sizes (preview.31-rc1)

| | US | EU |
|---|---|---|
| `.nro` | 9,285,804 B | 9,310,380 B |
| `.nro` nxlink | 9,306,284 B | 9,334,956 B |
| `.text` (ELF) | 5.59 MB | 5.62 MB |
| `dw3data_*` | 1.19 MB | 1.19 MB |
| `.bss` | 15.72 MB | 15.72 MB |

Matching the game's symbol names in the US ELF: about **1.34 MB of `.text`**,
**1.12 MB of `dw3data_*`** and 0.09 MB of `.data` are game code and data;
of the 15.72 MB `.bss` only about 0.13 MB is game, the rest is host buffers
(the 8 MiB heap arena, VRAM emulation; [P5.1](P5.1_MEMORY.md)) that both
regions can share. A second game copy therefore adds about **2.6-3.0 MB** (with
its share of `.rodata` and `.eh_frame`): one `.nro` of about **12 MB**, against
18.6 MB for the two `.nro` files today. RAM grows by the same amount; the heap
arena does not double.

## How data is found at run time

- Root per region: `sdmc:/switch/dw3/<us|eu>/` (PC `runtime/<ver>/`), with
  `files_us.json` + `SLUS_014.36` or `files_eu.json` + `SLES_039.36`, `AAA/`,
  `boot_tim.bin`, `memcard/` and `port_options.cfg`
  ([RUNTIME_LAYOUT.md](RUNTIME_LAYOUT.md)).
- Shared, one level up: `dw3.log`, `crash_report.txt` (and `_prev`),
  `replay.rec`. A disc image may sit in the parent folder (`sdmc:/switch/dw3/`)
  for the separate builds, which search the region folder and then its parent
  (`dw3_str_find_disc`). The combined build does not search the parent: its
  disc must be in `us/` or `eu/` ([issue #322](https://github.com/Larbaco/dw3-switch/issues/322)).
- `port/cdfs/runtime_extract.c` already **identifies a disc image by its
  executable**: it extracts only when `RUNTIME_EXECUTABLE` is in the image and
  rejects an image that holds `RUNTIME_OTHER_EXECUTABLE` (the other region).
- Saves and options are per region today (the save formats differ, see
  `GAME_SAVE_SIZE`). **Decided 2026-10-24: they stop being per region.** The
  saves go to a shared, per-user store with a canonical format
  ([P6.25](CROSS_REGION_SAVES.md), [P6.26](PER_USER_SAVES.md)) and
  `port_options.cfg` becomes one global file; the old per-region folders are
  only read once, copied, and never changed or deleted.

So the region **can be detected from the SD card without asking**: a complete
`us/` tree (`files_us.json` + `SLUS_014.36`), a complete `eu/` tree, or a disc
image whose executable names the region. A picker is needed only when both are
present.

## Candidate architectures

Costs are relative (the roadmap has no time estimates).

### (a) Two game copies in one image, region dispatch at boot (recommended)

Compile the game side twice from the one patched tree (`build/decomp-src`),
once with `VERSION_US=1` and once with `VERSION_EU=1`. For each region:
`ld -r` the region's game objects into one relocatable object, keep global only
the exports the host uses (about 25, renamed `us_` / `eu_`, e.g.
`us_dw3_game_main`) and localize the rest (`objcopy --keep-global-symbols`,
`--redefine-syms`), and rename its sections `dw3data_<x>` to
`dw3data_<region>_<x>` so GNU ld does not merge the two regions' restore
sections. The host (PSY-Z, platform backend, cdfs, memcard, movies, options
store) is linked once. At boot the host picks the region and calls the
region's entry through a small table (`main`, title registration, `GAME`,
`FILE_SECTORS`, ...).

- **Decomp:** untouched. The 731 blocks and the 76 patches stay compile-time.
- **Port / platforms:** the 14 `#if` sites become a runtime `dw3_region()`
  (paths, executable names, `STAGE_VRAM`, crash report disc and language
  line), or move to the per-region side (overlay table, the game-reading parts
  of `port/options`). The `__start_dw3data_*` lists and `overlay_sections.c`
  become per region (generated).
- **Size:** about +2.7 MB, one `.nro` of about 12 MB. Build time: the game part
  compiles twice.
- **Risk:** medium, and all in the build: a symbol both sides define by
  accident (a duplicate-symbol link error, which is loud), PSY-Z overrides that
  the game expects to win by link order (`psyz_host_overrides.c`,
  `psyz_pad_host.c`) must still win once, and the overlay restore must cover
  only the running region. Only one region runs per process, so PSY-Z's global
  state is not shared at the same time. Changing region means quitting.
- **Scales** to a third region (P6.19 JP): one more copy.

### (b) Runtime region variables instead of `#if`

Turn every `VERSION_*` test into a run-time check. Rejected: the
`GameState` and `BattleFuncs` layouts differ, so every field after 0x2644
would need an accessor or a superset struct (and a superset breaks the save
image, which is `GAME` up to `GAME_SAVE_SIZE`); file ids used in static
initializers, `case` labels and array sizes cannot become variables; the
change touches 352 decomp files, which as `PORT_HOST` patches means hundreds of
new patches to rebase on every `DECOMP_PIN` bump, against an upstream that
chose compile-time versions. Very high cost and risk; no benefit over (a).

### (c) Launcher `.nro` that chains to the region's `.nro`

(Built as P6.24 slice 2, see [As built, slice 2](#as-built-slice-2-the-switch-launcher).)

A small `.nro` detects the region (or shows a picker) and starts
`dw3-switch-us.nro` or `dw3-switch-eu.nro` with libnx `envSetNextLoad()` (works
under hbmenu / hbloader). Low cost and no change to the game build, but **not
one build**: still two game builds, CI and replays per region, three files to
ship, and nothing for the PC. Useful only as a stop-gap for the boot UX.

### (d) Other ideas

- **Shared libraries per region (PC only).** `dlopen("libdw3_us.so")` is easy
  on the PC, but libnx homebrew has no general dynamic loader (the `ro`
  service wants registered NRR files), so the Switch would still need (a);
  two mechanisms for one goal. Not recommended.
- **Hybrid of (a) and (b):** (a) for the decomp, run-time checks only for the
  14 host sites. This is what (a) above already proposes.

## Effects on the rest of the project (with (a))

- **Replay tests (P6.15).** 71 recordings (34 `us-*`, 37 `eu-*`) in
  `tools/replay/scenarios`; `manifest.py` picks the build by the scenario's
  `version`. With one binary the runner passes the region instead (an argument
  or environment variable). The references stay valid only if each region's
  output is **bit-identical** to the separate build and the boot adds no frame:
  a forced region must skip any picker, or all 71 recordings need
  re-recording. `tools/replay_ryubing.sh` needs a way to force the region on
  the virtual SD (one region present, or a forcing file).
- **Crash symbolization (P5.4).** One `.elf` per `.nro`, one build id; the
  region shows in every frame name (`us_` / `eu_`). `nx_crash.c`'s `disc:` and
  EU `language:` lines become run-time. `tools/symbolize_crash.py` needs no
  change beyond the names.
- **Releases.** `tools/pack_release_elfs.py` gets two items (release and
  nxlink) instead of four; the "EU is a manual step" section of
  [RELEASES.md](RELEASES.md) goes away because CI builds both regions by
  construction. The NACP name is `Digimon World 3 NX` (set in `platforms/switch/CMakeLists.txt`).
- **CI.** EU gets built and unit-tested on every PR for free, which it is not
  today.
- **Per-region tools.** `pack_runtime.py --version` and `build_file_map.py`
  stay per region (the data is per region). `build_switch_game.sh --eu` and
  `DW3_VERSION=us|eu` can stay as developer options (smaller, faster builds)
  next to a new default `both`.

## Boot picker

Decision 2 above. Proposed behaviour, for the Switch (the PC uses the same logic
with `--region` / `DW3_REGION`).

**When it appears.** Only when the region detection above finds **both** a
complete `us/` and a complete `eu/` tree (or disc images for both), and nothing
forces the region.

| Case | Behaviour |
|---|---|
| No image | The existing "no game data" failure, now naming both folders it looked in. No picker. |
| One image | Boot it, no picker, no question. |
| Both, `region_default=us` or `eu` | Boot it, no picker (the "don't ask again" state). |
| Both, `region_default=ask` (default) | The picker. |
| Forced: `--region`, `DW3_REGION`, the replay builds (`DW3_NX_REPLAY`, `DW3_REPLAY`), `DW3_MEMCARD_DIR` set by a runner | Boot the forced region; **no picker and no extra frame**, so the 71 P6.15 recordings stay valid. |

**The screen.** Host-drawn, before `dw3_game_main`, with the host debug-overlay
font that the Switch host already has (`platforms/switch/source/debug_overlay_font.h`),
English only in the first version. Two entries ("US", "EU") with the disc's
language or title if the executable gives it, a line "Starting <default> in 3...",
and a hint line.

- **Default selection**: `last_region` (written every time a region boots), else
  the region whose save was modified last, else US.
- **Timeout**: **3 seconds**, counting down on screen. Any D-pad press cancels the
  countdown (the player is choosing), so it never boots under the finger.
- **Input**: D-pad left/right (or the stick) to choose, **A** to boot now, **Y**
  to toggle "Always start this one" before confirming, **+** to quit to hbmenu.
  The picker reads the pad itself; it waits until all buttons are released
  before the game starts, so the confirming press does not reach the title
  screen.
- **Don't ask again**: stored in the **global** `port_options.cfg` as
  `region_default=ask|us|eu` (a file-only key as in
  [PORT_OPTIONS.md](PORT_OPTIONS.md#persistence); not part of any save).
  `last_region=us|eu` sits next to it.
- **Getting the picker back**: (a) **hold - (Minus) while the `.nro` starts**:
  shows the picker once even if a region is remembered (not R: hbmenu uses a
  held R to pick applet mode); (b) an **Options row** "Region at boot: Ask / US /
  EU" (global, applies on the next launch; the same patch seam as the other port
  rows); (c) edit `region_default=ask` by hand.

**Interaction with the rest.** The region is chosen first, then the Switch user
([PER_USER_SAVES.md](PER_USER_SAVES.md)) if one cannot be resolved without UI.
Switching region needs a restart, as stated below. The PC picks `last_region`
when both runtimes exist and no flag is given, and prints the choice; a window
picker is not part of the first slice.

## Recommendation and first slice

Approach **(a)**. First slice, **PC only and no visible change**:

1. `DW3_VERSION=both` in the three CMakeLists: the game side built twice,
   linked into one `dw3-pc` with `us_` / `eu_` exports and per-region
   `dw3data_*` names; `us` and `eu` keep building exactly as today.
2. A `dw3_region` chosen by `--region us|eu` / `DW3_REGION`, else from the
   runtime that is present (`runtime/us` or `runtime/eu`).
3. The 14 host `#if` sites made run-time.
4. Done when the P6.15 quick and full scenarios of **both** regions pass on the
   one `dw3-pc` with the existing references.

Then: the Switch `.nro` with auto-detection on the SD; the picker; CI and the
release workflow on `both`; an OLED check with each disc and with both on the
card.

## Open questions for the owner

Answered on 2026-10-24: saves ("Both regions on the SD": unified, decision 1;
per user, decision 3) and the picker behaviour (decision 2). Still open:

1. **The picker's look and languages**: the plain host screen above in English
   only, or the title-menu style, or the 5 EU languages?
2. **Hold-to-ask button**: is **- (Minus)** at launch right? (R is taken by hbmenu.)
3. **EU extra-chapter save in the US build** (hard case 1 in
   [CROSS_REGION_SAVES.md](CROSS_REGION_SAVES.md#hard-cases)): the US build cannot
   play it. Proposed: the slot shows the game's error 8 and stays untouched. OK?
4. **Legacy saves**: copy (never move) into the first user's folder with a prompt,
   first-fit into the 6 slots when both regions have saves. OK, or should a
   conflict ask the player slot by slot?
5. **Downgrade**: no dual write to the old `memcard/` folders (an old build then
   sees stale saves). Acceptable, with a release note?
6. **Profile selector** (decided 2026-10-24: "Quero usar o seletor de perfil do
   Switch."): used when several profiles exist and none is preselected. The
   selector has no timeout we control, and a wrong pick uses someone else's
   folder. If it cannot launch from applet mode, the fallback is open: see
   [PER_USER_SAVES.md](PER_USER_SAVES.md#open-questions-for-the-owner).
7. **Options**: one global `port_options.cfg` (recommended) or per user?
8. **Separate builds**: keep `--eu` / `DW3_VERSION=us|eu` for development only
   and ship only the combined `.nro` once it passes, or ship both for a while?
9. **Size**: is an `.nro` of about 12 MB acceptable (about 9.3 MB today)?
10. **PC**: one `dw3-pc` for both regions too (the same mechanism), or Switch
    only?
11. **Switching region in game** (without quitting): out of scope?

## As built (slice 1)

**Choice of slice.** The recommended first slice (the game side built twice and
linked into one binary, with `us_` / `eu_` exports and per-region `dw3data_*`
section names) is the one piece with real risk: 255 sections, the
`__start_dw3data_*` lists, PSY-Z overrides that must still win by link order,
and the proof that both regions stay bit-identical. It did not fit one
reviewable PR next to the picker. The smallest step that delivers value and
keeps the separate builds untouched is **everything around the dual image**:
region detection, the decision table, the picker logic with the owner's
decisions (3 s, D-pad cancels, Y = always, Minus to get it back, stored
`region_default` / `last_region`), and a PC front end that applies them and
starts the region's own binary. The dual-image link replaces "start the
region's binary" with "call the region's entry" later and reuses all of it.
(Default chosen, owner can change: this order, instead of the dual image first.)

**What exists.**

- `port/region/dw3_region.{c,h}`: platform independent (stdio / stat only).
  `dw3_region_probe` (a region is present when `<base>/<us|eu>/` holds
  `files_<r>.json` and its executable `SLUS_014.36` / `SLES_039.36`),
  `dw3_region_decide` (the table in [Boot picker](#boot-picker): forced, one
  tree, none, both + Minus, both + `region_default`, both + ask),
  `Dw3Picker` (pure state machine: 3 s countdown that any D-pad or Y press
  cancels, A boots, Y toggles always, + quits; the host feeds button edges and
  a millisecond delta), and `dw3_region_cfg_read/write` for `region_default`
  (`ask|us|eu`) and `last_region` in `port_options.cfg` (other keys are kept;
  an unreadable file is never overwritten).
- `platforms/pc/src/launcher_main.c`, built by the standalone project
  `platforms/pc/launcher/` (it links no game code and no PSY-Z; the existing
  CMake and CI do not know it). `tools/build_pc_single.sh [OUT]` assembles
  `OUT/dw3-pc` (the launcher) next to `dw3-pc-us` and `dw3-pc-eu` (the unchanged
  per-region binaries). The launcher runs
  `dw3-pc [--region us|eu] [--pick] [--runtime-base DIR] [game args]`, looks in
  `DW3_RUNTIME_BASE` (default `runtime`) for `us/` and `eu/`, and starts
  `dw3-pc-<region>` with `DW3_RUNTIME=<base>/<region>` (not overriding a
  `DW3_RUNTIME` or `--runtime` that is given).
- Fail-safe: one tree present boots it with no picker; none prints the setup
  message (both folders it looked in) and exits 2; `--region` / `DW3_REGION`
  boots that region, no picker, **no file written** (the replay runs).
- Picker on the PC: in a terminal (`l`/`1` US, `r`/`2` EU, Enter/`a` start, `y`
  always, `q` quit), or `DW3_REGION_PICK_KEYS` for tests (one character per
  100 ms step, `.` idles). With no terminal and no keys it starts the default
  entry at once instead of waiting (default chosen, owner can change). `--pick`
  / `DW3_REGION_PICK=1` is the PC stand-in for holding Minus.
- Stored choice: `<DW3_CONFIG_DIR or base>/port_options.cfg`. Until P6.26 makes
  the options file global this is the launcher's own file in the base folder;
  the per-region `runtime/<r>/port_options.cfg` files are not touched (default
  chosen, owner can change). The picker's default entry is `last_region`, else
  US (the "save modified last" tie-break of the design is not done).

**Tests.** `tools/tests/test_region.py` (C test of the table, the picker and the
stored choice, plus the launcher with stand-in binaries: one tree, none, both,
countdown, scripted keys, always, `--pick`, forced). Replay tier `single`
(`tools/replay_tests.sh --single --launcher PATH`): seven scenarios start the
`us-title` and `eu-lang-en` recordings through the launcher with only US, only
EU, and both trees (region forced, picker keys, countdown). They compare with
the references of the plain scenarios (`Scenario.ref_from`), so a pass means
the captures are bit-identical to the separate builds: 0 px differ on all 7,
US and EU. The existing `us` / `eu` scenarios and builds are unchanged and pass
the full suite (183 full + 6 quick scenarios, 1613 checks, 0 failed, on the
plain PC builds); `pytest tools/tests` 361 passed, 2 skipped; the US and EU
Switch `.nro` (10,219,692 and 10,252,460 B) still build. Evidence:
`tmp/evidence/20261025-p624-single-build/` (not in git).

## As built (slice 2: the Switch launcher)

Slice 2 is the same launcher approach on the Switch, still **not** the dual-image
link: option (c) above, built after all as the Switch front end of the slice 1
logic. The game `.nro` files are unchanged.

**Files.** `platforms/switch/launcher/` (a standalone libnx Makefile project and
`source/main.c`), `port/region/dw3_launcher.{c,h}` (pure: NRO lookup and the
text of every screen, host-testable), `tools/build_switch_launcher.sh` (same
`devkitpro/devkita64` image as `tools/build_switch_game.sh`; it copies the few
files it needs to `<build dir>/src` and builds there, so an NTFS checkout is
fine; `DW3_LAUNCHER_BUILD_DIR`, default `build/switch-launcher`; output
`<build dir>/out/dw3.nro`, 241,720 B, no game code and no PSY-Z). CI job
`switch-launcher` builds it and uploads the artifact `dw3-switch-launcher-nro`;
it is **not** a release asset and the release workflow does not know it.

**SD layout (default chosen, owner can change).** Everything stays in the
usual folder:

```
sdmc:/switch/dw3/dw3.nro        the launcher (what hbmenu starts)
sdmc:/switch/dw3/dw3-us.nro     the US game build (dw3-switch.nro, renamed)
sdmc:/switch/dw3/dw3-eu.nro     the EU game build (dw3-switch.nro from --eu, renamed)
sdmc:/switch/dw3/us/  eu/       the runtime trees, as in RUNTIME_LAYOUT.md
sdmc:/switch/dw3/port_options.cfg   region_default / last_region
```

`dw3-switch-us.nro` / `dw3-switch-eu.nro` are accepted as a second name. hbmenu
lists the two game NROs as well; that is harmless (starting one directly still
works and skips the launcher). **The data path does not depend on where the NRO
sits**: the game NRO opens `sdmc:/switch/dw3/<ver>/` (`DW3_DEFAULT_RUNTIME`
baked in at build time, `boot_main.c` ignores `argv`), so chain-loading from
`dw3-us.nro` finds the same data as hbmenu starting `dw3-switch.nro`. Nothing
in the game was changed to make this so.

**Flow** (the slice 1 decision table, `dw3_region_decide`):

1. Sample the pad for about 200 ms (12 frames) for **Minus held** (the pad is
   late to report on the first frames).
2. Probe `sdmc:/switch/dw3/us` and `/eu` (`files_<r>.json` + the executable),
   read `region_default` / `last_region` from `sdmc:/switch/dw3/port_options.cfg`.
3. One region: chain-load it at once. None: the setup message, **+** quits.
   Both: the picker unless `region_default` is us/eu and Minus is not held.
4. Picker on libnx's console (plain text, 80 columns of 16 px at 720p, white on
   black): `> US <` / `> EU <`, "Starting US in 3...", "Always start this one:
   off/ON". Left/Right (D-pad or left stick) choose and cancel the countdown, A
   starts, Y toggles "always", + quits. The default entry is `last_region`,
   else US.
5. `chain_load`: finds the NRO (`dw3_launcher_nro_path`), checks
   `envHasNextLoad()`, calls `envSetNextLoad(path, path)` (argv[0] = the NRO
   path, as hbmenu passes it; the game ignores it), stores `last_region` (and
   `region_default` when Y was on), and returns from `main`, so hbloader starts
   the queued NRO.

**Fail-safe screens** (all wait for **+**, none crashes): no data; the region's
NRO missing (names the file to copy); `envHasNextLoad()` false, for example not
started from hbmenu / hbloader or a title takeover (says so and names the NRO
to start by hand); `envSetNextLoad` returned an error (shows the code).

**Tests.** `tools/tests/test_region.py::test_switch_launcher_helpers` (C test
`tools/tests/launcher_main.c`: NRO lookup order and name per region, every
screen's text, picker marks, countdown and "always" line, tiny buffers); the
slice 1 tests cover the decision table and the picker. The libnx glue itself
(`main.c`) is only built, then run in Ryubing (below). **Ryubing smoke**
(headless on its own Xvfb, own data root, `Ryujinx -r <root> <nro>`):
- no data: the setup screen is drawn (evidence `none.png`);
- EU tree only, no game NRO: "The EU game file is missing" (`one-nonro.png`);
- both trees and both NROs: the picker countdown ran out (it is 3 s, shorter
  than Ryubing's window start, so the picker screen itself was **not** captured
  in the emulator; its text is covered by the host test), US was chosen, and
  the **no-loader screen** came up (`both.png`, `both-t*.png`).
**Ryubing cannot chain-load**: it runs the `.nro` directly as a program, with no
hbloader, so `envHasNextLoad()` is false and the real `envSetNextLoad` hand-over
is **not** tested; that is the first OLED check. The Minus-held path was not
driven in the emulator either (the decision is the tested slice 1 table).
Evidence: `tmp/evidence/20261025-p624-launcher/` (not in git).

**Findings.** Ryubing hangs while loading an NRO that embeds the 256x256
`icon.jpg` through this Makefile (the same file in the CMake game NRO loads
fine); the launcher therefore ships with the stock libnx icon (default chosen,
owner can change; not investigated further). The container runs as root and a
user namespace maps it to an id the host does not own, so the build script
leaves the tree world-writable (`chmod -R a+rwX`) instead of `chown`.

**Not done / open for the launcher.** The launcher does not know the discs in
the folder (only extracted trees, as in slice 1); its own icon; a real
hbloader run (OLED, see [Pending OLED checks](ROADMAP.md#pending-oled-checks));
the release workflow and README do not ship it yet.

### OLED checklist (launcher, owner)

Setup: `dw3.nro` (the launcher, CI artifact `dw3-switch-launcher-nro` or
`tools/build_switch_launcher.sh`) in `sdmc:/switch/dw3/`, the US build as
`dw3-us.nro` and the EU build (`--eu`) as `dw3-eu.nro` next to it. Start `dw3`
from hbmenu in applet mode (hold R on a game to get a title takeover for the
negative check at the end). Check the picker text is readable on the OLED at
arm's length, handheld.

1. **Both regions** (`us/` and `eu/` trees present): the picker appears with
   `> US <`; do nothing: after 3 s the US game starts and is the same as
   starting `dw3-switch.nro` (title, a save loads). Quit the game, start `dw3`
   again: the picker now starts on the last region. Press Right, A: EU starts.
2. **Y "always"**: Y until it says ON, A. Next start: no picker, that region
   boots at once. **Hold Minus** while pressing A on `dw3` in hbmenu: the picker
   comes back although "always" is stored. D-pad/stick cancel the countdown.
   `+` quits to hbmenu without starting anything.
3. **One region** (move `eu/` away): no picker, the US game starts at once (a
   short pause of about 0.2 s for the Minus sample is expected). Then only EU.
4. **No region** (move both away): the setup message names both folders;
   `+` returns to hbmenu.
5. **Missing game NRO** (remove `dw3-eu.nro`, choose EU): the message names
   `sdmc:/switch/dw3/dw3-eu.nro`; `+` quits.
6. **No next-load** (start `dw3.nro` as a title takeover, or from a loader
   without next-load support): the "cannot start another program" message,
   no crash.
7. **Saves and options** of both regions are unchanged after going through the
   launcher (it writes only `sdmc:/switch/dw3/port_options.cfg`).
8. **Controllers**: the same with handheld Joy-Cons and a Pro Controller.

**What remains for P6.24.**

1. The dual-image link: `DW3_VERSION=both` in the three CMakeLists, the game
   side built twice, `us_` / `eu_` exports, per-region `dw3data_*` names and
   `__start_` lists, the 14 host `#if` sites made run-time, then
   `dw3-pc-us` / `dw3-pc-eu` and the launcher become one binary.
2. The Switch: auto-detection on the SD card, a host-drawn picker in
   `boot_main.c` (debug-overlay font, Minus read at start), reading the
   same `dw3_region.c`; CI and the release workflow on one `.nro`; an OLED check
   with each disc and with both. The stop-gap launcher `.nro` (option (c)) was
   built in slice 2 (above); it is a third file to ship, which is why it is not
   a release asset yet. This item is the in-image replacement of it.
3. Disc images as a detection source (today only extracted trees count); the
   Options row "Region at boot"; the per-region language/title line in the
   picker; the open questions below.

## As built (slice 3: the dual-image link, PC)

Slice 3 is option (a): the game side compiled twice, linked once. PR A of the
2026-10-28 priority, PC only; the Switch `.nro` follows in PR B. The plain
`DW3_VERSION=us` and `eu` builds are unchanged (same flags, same objects, same
link line; the only additions are `dw3_region.c` in the executable, so the host
can name the region, and a log line when `--region` / `DW3_REGION` names the
other one).

```bash
cmake -S platforms/pc -B build/pc-both -G Ninja -DDW3_VERSION=both \
  -DPSYZ_RENDERER=sdl3_gpu
ninja -C build/pc-both dw3-pc
build/pc-both/dw3-pc --region eu      # or DW3_REGION=eu, else the runtime present
```

**How it is built.**

- `port/CMakeLists.txt` has three parts, chosen by `DW3_PORT_PART`: `all` (a
  plain build, as before), `host` and `game`. `port/dual_region.cmake`
  (`dw3_add_both_game`) adds the directory three times: the host part once
  (cdfs, memcard, movies, `psyz_missing`, the run-time region code, one archive
  `dw3_host_objs`) and the game part twice with `DW3_REGION_BUILD=us|eu`, which
  gives every game target the suffix `_us` / `_eu` (`dw3_engine_eu`,
  `dw3_p32_field_us`, ...). Each game copy compiles from the same
  `build/decomp-src`, with its own `VERSION_US=1` / `VERSION_EU=1`, its own
  overlay table and its own `host_ovl/` folder. The patched tree is applied
  once. The P3.1 audit target is not built twice.
- `tools/localize_region_obj.py` packs one copy into `game_<r>.o`: `ld -r` over
  the copy's archives, pulling members from the root symbol (so an object that
  nothing used stays out, as in a plain link; the six overlay archives are taken
  whole), then `objcopy`: every global becomes local except `dw3_game_api`, which
  becomes `us_dw3_game_api` / `eu_dw3_game_api`; the restore sections
  `dw3data_<x>` / `dw3bss_<x>` become `dw3data_<r>_<x>` / `dw3bss_<r>_<x>` (510
  per copy) and the undefined `__start_` / `__stop_` references of
  `overlay_sections.c` (1,020) follow, so GNU ld synthesizes them per region and
  each region's overlay restore covers only its own sections. The host symbols a
  copy needs (PSY-Z, the platform backend, cdfs, memcard, movies ...) stay
  undefined and global, so the final link resolves them once with the same rules
  as a plain build: the executable's own objects come first, then the host
  archive, then PSY-Z, with the same `--wrap` options. `psyz_pad_host.c`,
  `psyz_host_overrides.c` and the `--wrap` redirections therefore still win
  exactly as before. The tool refuses to finish if a global is left over, an
  export is missing, or a `__start_` has no section.
- **The door.** `port/region/dw3_game_api.h` is a table of what the host calls
  in the game: `dw3_game_main`, the boot TIM loader, the title registration, the
  map view calls, the port-options calls the title overlay makes
  (`is_open`, `level`, `locked_mask_for`, ...), the turbo and screen-fade
  levels, the cheats soak hook, and the pointers to the arrays the host reads
  (`FILE_SECTORS`, `FILE_SECTOR_COUNTS`, `FILE_COUNT`, `HEAP`, `DW3_HOST_HEAP`).
  `port/region/game_api.c` fills it inside each copy (it is the one symbol that
  copy exports). That is 24 entries; they are the same 24 symbols a count with
  `nm` over the US objects found in the survey, with the 17 overlay-table
  entries staying inside the copy. In host code compiled with `DW3_BOTH=1` the
  calls are redirected by macros in that header (`dw3_port_options_level(i)`
  becomes `dw3_game()->port_options_level(i)`); in a plain build the header does
  nothing and the calls are direct.
- **`dw3_region()`** (`port/region/dw3_region_rt.h`) replaces the host's
  `#if VERSION_EU` tests: inline and constant in a plain build, the region chosen
  at boot in a `both` build. The sites that became run-time: all of
  `port/cdfs/runtime_extract.c` (version string, executable names, map name,
  `STAGE_VRAM`, the EU extract branch, the file tables through the table) and in
  `platforms/pc/src/main.c` (the log tag, the US-only runtime extract, the
  cross-import region, the title registration, `DW3_DEFAULT_RUNTIME`). The game
  copies keep their compile-time `#if`s: they are built once per region.
- One weak fallback had to go: `map_view.c` defines a weak
  `dw3_map_view_async_start` for tests, and a weak definition inside a copy
  would shadow the host's real one once the symbols are local. A copy compiles
  with `DW3_REGION_IMAGE=1`, which leaves it out.

**Choosing the region** (`dw3_region_resolve`, tested on its own): `--region`,
then `DW3_REGION`, then the runtime folder that is given (`DW3_RUNTIME` /
`--runtime`: by the files it holds, then by a trailing `/us` or `/eu`), then the
folders present under `DW3_RUNTIME_BASE` (default `runtime`): one present boots
it, both boot `region_default`, else `last_region` (read from `port_options.cfg`
in `DW3_CONFIG_DIR`, else the base), else US. Nothing present: US, so the
disc-extract path of the US build still works. A value that is not `us` or `eu`
stops with status 1 before anything is opened. No picker, no extra frame and
nothing written on the PC; the picker is the Switch's (PR B). The log says what
was chosen: `dw3-region: running <r> (<source>), one image with both game
copies` and `dw3-pc: region=<r>`.

**Findings.**

- The boundary is as small as the survey said: only 24 host-to-game references,
  and none from PSY-Z. There is one duplicate (the weak fallback above) and no
  game definition of a name PSY-Z also defines.
- `ld -r` with `-u dw3_game_api` and archives keeps archive semantics; whole
  objects were not forced in, so no new undefined reference appeared.
- Size on the PC (debug-less default build): `dw3-pc` 13.4 MB (one region) to
  18.4 MB (both), +5.0 MB. RSS in the replays is the same as a plain build (about
  190-200 MiB at the title). The configure-and-build of the second copy is the
  cost: the game side compiles twice.
- The window title of a `both` build is plain `dw3-pc` (the plain builds say
  `dw3-pc Phase 2` / `dw3-pc EU`).

**Tests.** `tools/tests/test_localize_region_obj.py` (two copies of one C file,
same globals, same restore section: a raw link fails, the localized objects link
and keep separate state, separate section spans and a shared host function),
`tools/tests/test_region.py::test_region_resolve` (`region_rt_main.c`), the
existing `runtime_extract` tests (now through `dw3_region()`), and CI: the `pc`
job also builds `DW3_VERSION=both` and checks that each copy exports exactly
`<r>_dw3_game_api` and that `--region xx` stops with status 1.
**P6.15 replays on the one binary** (`tools/replay_tests.sh --bin-both PATH`; the
runner sets `DW3_REGION` per phase and compares with the plain builds'
references, private copy of the refs):

- quick tier, US and EU: 68 checks, 0 failed, every capture 0 px different;
- full tier, a curated subset of both regions, chosen to cover every kind of
  scenario: the 25 that finished before the run was cut (new game, field walk,
  bridge gate, load, the four autosave scenarios, title continue/load, field
  Options, banners and the four cross-region import scenarios `xr-*`), then 31
  names for each region (load menu, options/volume persistence, two battles,
  battle report, file-cache-full, items, map view, battle results, save screen,
  card album/shop/deck, gym, lab, cheats screen, restarts, title quit, return to
  title, turbo menu, battle options, door iris, underground exit, encounter,
  text speed, HUD/claw, menu screens): 768 checks, 0 failed, 0 skipped, every
  capture 0 px different from the plain builds' references. The other full-tier
  scenarios (about 233 in all, about 3 h at 2 jobs) were not run on the `both`
  binary; CI and the owner can run `tools/replay_tests.sh --bin-both PATH`

**Not done / open.** The Switch image and picker (PR B). The EU language and
title lines of `nx_crash.c` still use the compile-time region on the Switch
(PR B). Disc images as a detection source, the Options row "Region at boot", and
the per-region title in the picker (below). The `tools/build_pc_single.sh`
launcher is no longer needed on the PC but stays until the Switch path is
confirmed on the OLED.

## As built (slice 4: the combined Switch `.nro`)

PR B of the 2026-10-28 priority. The dual-image link of slice 3 on the Switch,
plus what only the console has: finding the data, the picker, the build, CI and
the release asset.

```bash
tools/build_switch_game.sh --both            # build/switch-both/dw3-switch.nro
tools/build_switch_game.sh --both --nxlink   # build/switch-both-nxlink/dw3-switch-nxlink.nro
```

NACP name "Digimon World 3 NX" (the nxlink variant "Digimon World 3 NX
(nxlink)"), as for the separate builds. Sizes (this tree): US 11.10 MB, **combined
14.21 MB** (+3.1 MB over one region; the survey estimate of about 12 MB was low,
the ordinary build had grown since; still inside the "about 12 MB is fine"
default, record it and tell me if it is not).

**What it does at boot** (`platforms/switch/source/boot_main.c`,
`psyz_backend/nx_region_boot.c`, `port/region/dw3_region_boot.c`):

1. Log, pad and SD come up as always; then, before the profile selector (which
   needs the region's save folder), the region step runs.
2. **Forced** (`DW3_REGION` in the environment, as a replay NRO's baked test
   environment, or `DW3_RUNTIME`): no probe, no picker, no extra frame, nothing
   written (the P6.15 rule).
3. Else it looks under `sdmc:/switch/dw3/` for `us/` and `eu/` (a complete tree:
   `files_<r>.json` plus the executable; a region folder that only holds a disc
   image, `*.cue` / `*.bin` / `*.iso` directly inside it, also counts, so a first
   run from a disc still extracts). Only `<base>/us` and `<base>/eu` are read: a
   disc image placed directly in `sdmc:/switch/dw3/` is not found here, unlike
   with the separate builds ([issue #322](https://github.com/Larbaco/dw3-switch/issues/322)). It reads `region_default` and `last_region`
   from `sdmc:/switch/dw3/port_options.cfg`.
4. Only US or only EU: that one runs, no picker. None: the setup screen (names
   both folders) and `+` quits. Both: the picker, unless `region_default` is `us`
   or `eu`; **holding Minus at launch** (sampled for about 200 ms, and only when it
   can matter: both present and a region stored) asks again.
5. The picker is drawn on the EGL surface with the 8x8 HUD font (scissor clears,
   like the crash notice; the text is the stop-gap launcher's): `> US <  > EU <`,
   "Starting US in 3...", "Always start this one: off/ON". Default entry
   `last_region`, else US; 3 s countdown that Left/Right/Y cancel; A starts; Y
   toggles always; `+` quits to hbmenu; HOME ends it. After A it waits (up to
   0.6 s) for the buttons to be released so the press cannot reach the title.
6. `last_region` is stored every time a region boots (not when forced);
   `region_default` only when Y was on. Then `dw3_region_select()` fixes the
   region and the boot continues as the separate build does (user step,
   `sdmc:/switch/dw3/<r>` as the runtime root, extraction, memcard, title).
   Saves are the per-user, cross-region store of P6.25 / P6.26, unchanged.

**Log proof (AGENTS.md rule 12).** Every decision is one line in `dw3.log`:
`dw3-region: data found us=<0|1> eu=<0|1> under <base>`, `stored
region_default=<ask|us|eu> last_region=<us|eu|none> minus_held=<0|1|n/a>`,
`decision <boot|pick|no-data> region=<r>: <why>`, `picker shown (default entry
<r>, timeout 3000 ms)`, `picker result <r> by <countdown|button A> after N ms
always=<0|1>` or `picker quit by + after N ms`, `saved last_region=<r>
[region_default=<r>]`, `forced <r> by <source>: ...`, and then `running <r>
(<source>), one image with both game copies`. The OLED plan rows G9.* name them.

**Other host changes.** The crash report's `disc:` and EU `language:` lines are
run time (`none yet` before a region is chosen, so a crash in the picker still
writes a report); the backend files that call the game (`nx_gpu.c`,
`nx_crash.c`, `nx_title_overlay.c`, `switch_gpu.c`, ...) include
`nx_game_api.h` last, which redirects the calls in a `both` image. The table
grew by the overlay host (mode, stage and file name), `port_options_in_game`,
`port_battle_options_holding`, `port_turbo_present_due`, `overlay_set_strict` and
the EU `LANGUAGE` pointer, and before a region is chosen it is a harmless
"no game" table, so the HUD or the crash handler cannot call through nothing.

**CI and release.** CI job `switch-game-both` builds the ordinary and the nxlink
combined `.nro` and uploads the artifact `dw3-switch-both-nro`. The release
workflow also builds them and publishes `dw3-switch-both.nro`,
`dw3-switch-both-nxlink.nro` and `dw3-switch-both-elf.zip` as extra assets;
nothing it published before is removed or renamed ([RELEASES](RELEASES.md)).

**Tests.** `tools/tests/region_boot_main.c` (via `test_region.py`): the flow with a
fake console, 11 cases: no data, only US, only EU, both with the countdown, the
last choice as default, Right + A, Y stored, `region_default` with and without the
Minus sample, `+`, forced, a disc image counting as present, a picker that cannot
run. The pure parts of the picker and the decision table are the slice 1 tests.
Ryubing (below). The plain US and EU Switch builds still build.

**Ryubing runs** (headless on its own Xvfb, own data root with the real US and
EU runtimes, `Ryujinx -r <root> <nro>`, evidence in
`tmp/evidence/20261028-p624-both/`, not in git):

| Case | Result (from `dw3.log`) |
|---|---|
| only `us/` | `data found us=1 eu=0`, `decision boot region=us: only the US data is present`, `saved last_region=us`, `running us`, `runtime version=us ... SLUS_014.36`, `entering dw3_game_main()`; the US opening movie plays |
| only `eu/` | the same for EU: `running eu`, `executable=SLES_039.36`, `entering dw3_game_main()` |
| both, nothing stored | `decision pick region=us: both present: picker`, `picker shown (default entry us, timeout 3000 ms)`, `picker result us by countdown after 3046 ms always=0`, `saved last_region=us`, then the US game |
| none | `data found us=0 eu=0`, `decision no-data`, the setup screen is drawn (the first overlay draw of a Ryubing session is invisible while it compiles the shader, as noted for the title overlay; the text is the launcher's tested one) |
| both, picker kept up (a throw-away build with a 40 s countdown, not committed) | the picker is drawn on the EGL surface, readable at 1280x960: title, `> US <   EU`, "Always start this one: off", "Starting US in 33...", the key hints and the Minus line; it counts down and boots US at 40 s |

**Not covered by the emulator:** pad input. Ryubing's window does not get
the injected keys while the guest starts, so Left / Right / A / Y / + and
hold-Minus were not driven (the same limit as the launcher's smoke); they are
host-tested through the flow and the picker state machine and are the first OLED
check (G9.3 to G9.7).

**Orchestrator defaults in this slice (owner may change):** the picker is a plain
English screen; Minus is hold-to-ask; the separate US / EU `.nro` files keep
shipping next to the combined one until you confirm it on the OLED; no region
switching in game; a disc image has to sit in `us/` or `eu/` for a first-run
extraction to be found (an extracted tree works wherever the disc was). This was
written as a default for both, but the separate builds also find a disc in
`sdmc:/switch/dw3/` (their extraction searches the parent folder); the combined
build does not ([issue #322](https://github.com/Larbaco/dw3-switch/issues/322)).
So for the combined build the rule is: disc in `us/` or `eu/`.

**Not done / open.** Nothing was run on a console: the picker's look at arm's
length, the Minus hold and the profile selector coming up after an EGL surface
exists (it normally comes before it) are OLED checks
([OLED_TEST_PLAN G9](OLED_TEST_PLAN.md#g9-combined-p624-one-nro-for-both-regions-pr-b)).
The Options row "Region at boot" and the per-region title in the picker are still
open. The stop-gap launcher stays until the OLED check passes.

## History moved from ROADMAP.md (2026-10-24)

Moved from the ROADMAP.md entries on 2026-10-24, word for word (two leading
spaces removed). "Above" and "below" refer to the old entry. The roadmap
keeps the current state and links here.

**What the survey found (cited there):** the region is a compile-time
choice everywhere: `DW3_VERSION` in the three CMakeLists sets
`VERSION_US=1` / `VERSION_EU=1`, the overlay table and the default runtime,
and the decomp's `include/version.h` allows exactly one. The decomp has
**731 region blocks** (1,310 `#if` / `#elif` lines in 352 files; 390 blocks in
the stages), of which 291 only `#define` file ids and 436 hold code or data
(the file and stage tables, text tables). **Struct layouts differ**:
`GameState` from offset 0x2644 (`GAME_SAVE_SIZE` 0x26BC US, 0x26C4 EU) and
`BattleFuncs` (EU `rollCounter` at 0xD0), so a run-time region variable in
the decomp is not practical. The port side is small: 14 region `#if` sites in
`port/` and `platforms/`, 13 in 6 of the 76 patches, and the host uses only
**25 game symbols** (mode entry points, `GAME`, `PAD`, `SOUND`, the file
sector table, `dw3_game_main`). PAL / NTSC timing already follows the mode
the game sets, not a build flag. The region can be found on the SD card
without asking (`us/` or `eu/` tree, or the executable inside a disc image,
which `runtime_extract.c` already checks). The saves are **not** the same
format in the two regions; the owner decided they get unified (P6.25) and
bound to the Switch user (P6.26), see the decisions below.
**Recommended approach:** compile the game side twice into one image (one
copy per region, exports renamed `us_` / `eu_`, the rest local, the
`dw3data_*` restore sections renamed per region, the existing
`tools/prefix_stage_obj.py` technique), link the host once, and pick the
region at boot. The decomp and the patches stay untouched. Cost: medium,
all in the build; about **+2.7 MB**, one `.nro` of about 12 MB (US 9.29 MB
and EU 9.31 MB today, 18.6 MB for both). Rejected: run-time region variables
in the decomp (352 files, struct layouts); a launcher `.nro` that chains to
the region's `.nro` (cheap, but still two builds; a stop-gap at most).
**First slice (PC, no visible change):** `DW3_VERSION=both` builds one
`dw3-pc` holding both regions (`us` and `eu` stay as they are), a
`--region` / `DW3_REGION` override else the runtime present, the 14 host
sites made run-time; it passes when the P6.15 scenarios of both regions match
the existing references on that one binary.
**Effects:** P6.15 runs one binary per platform (a forced region must skip
any picker, or the 71 recordings need re-recording); one `.elf` and build
id per `.nro`, with the region in each symbol name (P5.4); two release items
instead of four in `tools/pack_release_elfs.py` and CI builds EU on every PR
([RELEASES.md](RELEASES.md)). Overlaps P6.2, whose "EU/US selection at
boot" this item would carry (P6.2 keeps PAL / NTSC and the language). A third
region (P6.19, JP) is one more copy.
