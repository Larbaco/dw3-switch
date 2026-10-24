# P6.24 One build for US and EU (feasibility survey)

Owner request (2026-10-24): "Build única pra US e EU". One `.nro`, and one PC
binary, that runs the US and the EU disc, instead of the separate builds of
today. This note is the survey behind the
[ROADMAP P6.24 entry](ROADMAP.md#phase-6-goal-notes). The owner has since
decided how saves and the boot choice work (next section). **Nothing is
implemented.** Numbers come from `origin/main` at `3e2a9b7`, the decomp at
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
  `replay.rec`, and a disc image may sit in the parent folder too.
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
  construction. The NACP name stays `dw3-switch`.
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
