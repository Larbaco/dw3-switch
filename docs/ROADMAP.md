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
inside a phase are roughly ordered; phases do not start until the gate of
the previous one passes. No time estimates on purpose. Facts behind the
tasks are in [SURVEY.md](SURVEY.md); layering is in
[ARCHITECTURE.md](ARCHITECTURE.md).

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
title from decomp C; pad Critical re-reviewed APPROVE; GPU polish open.
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
  `tools/cdfs_cat.c`, `tools/tests/test_cdfs.py` (mini fixture; owner
  `runtime/us` spot-check skipped in CI).
  Done when: fixture reads are byte-identical — verified. Full
  `FILE_CACHE.load` wiring is later (title boot / engine integrate).
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
- [x] **P2.7 Memory card → file.** `port/memcard/`: 128 KiB PS1-format
  `.mcr` per slot (`memcard_host_*` sector API matching libcard). Engine
  wiring on `phase3-pc-bringup` @ `95fb20b`: `memcard_libmcrd.c` +
  `--wrap=_card_*` (STGMCARD save evidence). Host CLI + tests:
  `tools/memcard_rw.c`, `tools/tests/test_memcard.py`. DuckStation
  round-trip is owner manual (see `port/memcard/README.md`).
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
pad state connected; GPU polish open; merged via PR #3).

## Phase 3 — Game bring-up on PC

Status: **in progress** (branch `phase3-pc-bringup`; plan:
[`docs/superpowers/plans/2026-10-10-phase3-pc-game-loop.md`](superpowers/plans/2026-10-10-phase3-pc-game-loop.md)).

Tracking: Phase 3 on `phase3-pc-bringup` (+ sibling `phase3-harness`
for P3.9 design). P3.1 done. **P3.2 Done-when met** (`docs/P3.2_FIELD.md`).
WSTAG790 linked @ `481d4d0` (post-name chamber OK). **P3.3 Done-when
met** @ `a4d7e64`: `FIGHTSTG`+`WFIGHTMN`+`STFGTREP`; forced win →
report XP/levels → return to `FIELDSTG` (`docs/P3.3_BATTLE.md`).
**P3.4 partial** @ `95fb20b` (`READY_FOR_MAIN_ROADMAP_SYNC:
P3.4_MEMCARD`): menus linked + field open/return; **STGMCARD
force-save mutates `memcard0.mcr`** via P2.7 (`docs/P3.4_MENUS.md` on
branch). Pad-driven START→status and NPC script entries still open.
**P3.5 partial** on `phase3-pc-bringup` (merged from `phase3-cardgame`
@ `fcff119` / `2df364f`): force-win → prize item + return to
`FIELDSTG` proven; album `cardsSeen` 29→30 is host
`DW3_FORCE_CARD_WIN` `addCards` aid, not vanilla prize→album
(`docs/P3.5_CARDGAME.md`). Checkbox stays unticked until the real album
path. **P3.6 investigation** on main (`docs/P3.6_AUDIO.md` from
`phase3-audio-spike`): blocker PSY-Z `_SsSeqPlay` `NOT_IMPLEMENTED` —
Done-when not met. **P3.7 blocked** on `phase3-pc-bringup` @
`1f20556` (`READY_FOR_MAIN_ROADMAP_SYNC: P3.7_BLOCKED`,
`docs/P3.7_MOVIES.md`): `DW3_START_MODE=0xE01` reaches movie path;
PSY-Z `MDEC_*` / `StCdInterrupt` stubs + no CD track for
`MOVIEOPN.STR`; default skip `MODE_TITLE` retained. **P3.9 design** on
`phase3-harness` @ `3064486`: `docs/P3.9_COMPARE.md` + stub
`tools/compare_ref.py` (`--dry-run`); checkbox unticked until
title/field/battle captures.

Objective: the full game loop runs on PC: field, stages, battle, menus,
audio, movies.

- [x] **P3.1 64-bit audit, part 2 (overlays + stages).** Same rules as
  P2.4 across `src/field`, `src/battle`, `src/cardgame`, `src/menus`,
  `src/debug`. On `phase3-pc-bringup`: match-neutral `0018` + PORT_HOST
  `0019`; CMake `dw3_p31_audit` under
  `-Werror=pointer-to-int-cast -Werror=int-to-pointer-cast` (432 objs);
  `dw3_engine_objs` still builds. Evidence / exclusions (EU WSTAG920–974
  `LANGUAGE`; battle `draw`/`lights`/`model_mesh` PSY-Z gaps):
  `docs/P3.1_64BIT_AUDIT.md` on that branch. Full overlay link with P3.2+.
  Upstream `make compare` blocker unchanged (`docs/PATCHES.md`).
- [x] **P3.2 Field + first stage.** `FIELDSTG` + `WSTAG260` scripts +
  starting town `WSTAG780` (door → `WSTAG785`) + `STPLNMET`. Patches
  `0020`–`0023` + `DW3_AUTO_PAD` host inject. **Done-when evidence**
  (`docs/P3.2_FIELD.md`): pad `moveByPad` moves actor (Δy on 780/785);
  real `SCRIPT_LEAVE`/`leaveField` 474→475 (not `AUTO_DOOR`); name
  `0x500` then back to 475. Follow-up: WSTAG790 (`0x2D9`) linked @
  `481d4d0` (post-name chamber OK; see `docs/P3.2_FIELD.md`).
  Done when: new game → name entry → first field area is walkable and a
  door changes stage.
- [x] **P3.3 Battle.** `FIGHTSTG` + `WFIGHTMN` + `STFGTREP` linked on
  `phase3-pc-bringup` @ `a4d7e64`; patches `0024`/`0025` +
  `DW3_FORCE_ENCOUNTER` / `DW3_FORCE_WIN` / `DW3_AUTO_PAD=confirm`.
  **Done-when evidence** (`docs/P3.3_BATTLE.md`): win → report applies
  XP/levels → return to `FIELDSTG` (`0x2d7` / stage 474) with walk +
  door. Follow-up: real wilderness encounter + pad-driven KO.
  Done when: a random encounter completes and returns to the field with
  stats applied.
- [ ] **P3.4 Menus and shops.** Linked on `phase3-pc-bringup` @ `95fb20b`
  (`dw3_p34_menus`); patches `0026`/`0027`/`0028` + `DW3_FORCE_MENU` /
  `DW3_AUTO_MENU_RETURN` / `DW3_FORCE_MCARD_SAVE`. **Partial evidence**
  (`docs/P3.4_MENUS.md` on branch): each opens from field via `leaveField`
  and returns to `FIELDSTG`; **STGMCARD force-save mutates `memcard0.mcr`**
  (`MemCard*` + `_card_write` wrap → P2.7). Still open: pad-driven
  START→status, NPC script entries, load e2e. Checkbox stays unticked
  until full Done-when.
  Done when: each opens from the field, works end-to-end, and saving via
  `STGMCARD` uses P2.7.
- [ ] **P3.5 Card game.** Linked on `phase3-pc-bringup` (from
  `phase3-cardgame` @ `2df364f`, `dw3_p35_cardgame`); patch `0029`
  (renumbered from cardgame `0028` after P3.4 took `0028`) +
  `DW3_FORCE_CARD_WIN` / `DW3_AFTER_CARD_ALBUM` / reuse
  `DW3_FORCE_MENU`. **Partial evidence** (`docs/P3.5_CARDGAME.md`):
  force-win applies prize item → `CARDGAME return` → `FIELDSTG` (no
  SEGV); optional `DW3_AFTER_CARD_ALBUM` → `STCRDABM`. Album
  `cardsSeen` 29→30 is host `addCards` on `FORCE_CARD_WIN`, not vanilla
  prize→album. Checkbox stays unticked until real album path.
  Done when: a card battle can be won and the album updates.
- [ ] **P3.6 Audio.** libsnd path through PSY-Z: VAB banks, SEP music,
  SFX, reverb settings the game sets. Verify `SsSeqCalledTbyT` timing
  from the vsync callback.
  Investigation (Task S1): `docs/P3.6_AUDIO.md` (from `phase3-audio-spike`)
  — blocker PSY-Z `_SsSeqPlay` `NOT_IMPLEMENTED`; no Core CMake.
  Implementation = Task A6.
  Done when: title music, field music change on stage switch, battle SFX.
- [ ] **P3.7 Movies.** Title STR playback through PSY-Z `libpress` +
  streaming (`CdRead2`, `StSetRing`) from raw-sector `.STR` files packed
  in P1.6. **Blocked** on `phase3-pc-bringup` @ `1f20556`
  (`docs/P3.7_MOVIES.md`, `READY_FOR_MAIN_ROADMAP_SYNC: P3.7_BLOCKED`):
  boot attempt `DW3_START_MODE=0xE01` hits `MDEC_*` / stream stubs;
  default skip `MODE_TITLE` retained. Checkbox unticked until play +
  audio + Start-skip.
  Done when: the opening movies play with audio and can be skipped.
- [ ] **P3.8 Stage sweep.** Use `STAGSLCT` (debug stage select) to load
  every stage of the version and log crashes/asserts.
  Done when: all stages load and return to the select screen.
- [ ] **P3.9 Reference comparison harness.** Script that boots the same
  save in DuckStation and the PC build and captures screenshots for
  side-by-side review (PS1 output is the oracle).
  Done when: a doc page with the comparison for title, field, battle.

Gate: a full playthrough segment (start → first boss) on PC without
crashes; both versions build.

## Phase 4 — Switch host for PSY-Z

Objective: a libnx backend so the Phase 3 image runs unchanged on the
console.

- [ ] **P4.1 Backend spike.** Two short experiments, documented in
  `docs/SWITCH_BACKEND.md`: (a) build devkitPro's SDL3 branch from source
  and run PSY-Z's `sdl3_*` backend; (b) sketch a native libnx backend
  following PSY-Z's PSP layout (`gpu`, `input`, `audio`, `os`, `file`,
  `log`, `module`). Pick one.
  Done when: the decision and its reasons are written down.
- [ ] **P4.2 Video.** Present PSY-Z's VRAM/framebuffer via EGL + OpenGL
  (mesa/nouveau on libnx) or deko3d; 60 Hz vsync; docked/handheld
  resolution switch.
  Done when: `gpu_hello`-level sample renders at 60 fps on hardware.
- [ ] **P4.3 Audio.** `audren` output thread fed by PSY-Z's SPU mixer at
  44.1 kHz.
  Done when: libsnd sample plays without underruns.
- [ ] **P4.4 Input.** `hid` → PS1 pad bits, including the digital/analog
  modes `PadInfoMode`/`PadSetMainMode` expect; rumble via `PadSetAct` →
  HD rumble (optional).
  Done when: title and field controls work with Joy-Con and Pro
  Controller.
- [ ] **P4.5 Filesystem + log + time.** `romfs:`/`sdmc:` for the runtime
  layout from P1.6, nxlink/stdout logging, `VSync`/`RCnt` timing.
  Done when: P2.5's cdfs tests pass on hardware against the SD layout.
- [ ] **P4.6 Static modules on Switch.** Confirm P2.6 needs no `dlopen`;
  if PSY-Z's module API is required, provide a static `Psyz_ModuleOpen`
  that dispatches to the linked table.
  Done when: all overlays switch on hardware.
- [ ] **P4.7 Boot the game.** Link the Phase 3 image against the Switch
  backend and run from hbmenu.
  Done when: title screen on hardware.

Gate: title + field + battle on a Switch.

## Phase 5 — Playable on Switch

Objective: the Phase 3 playthrough segment works on the console at full
speed.

- [ ] **P5.1 Memory budget.** Heap arena, file cache and VRAM emulation
  sized for the console; no allocations after init in the frame loop.
  Done when: a 2-hour session shows flat memory in a debug overlay.
- [ ] **P5.2 Performance pass.** Profile GTE/GPU emulation and the
  drawing path on handheld clocks; software vs. GL renderer decision.
  Done when: no frame over 16.7 ms in field and battle benchmarks.
- [ ] **P5.3 Save handling.** Card images under `sdmc:/switch/dw3/saves/`,
  import/export of `.mcr`, sleep/exit safety (flush on `appletMainLoop`
  exit).
  Done when: a PS1 save imported on Switch continues the game.
- [ ] **P5.4 Error reporting.** Crash handler that writes a log + last
  mode/stage id to SD; assert overlay.
  Done when: a forced fault produces a readable report.
- [ ] **P5.5 Stage sweep on hardware.** P3.8 again on the console.

Gate: start → first boss on hardware, both versions.

## Phase 6 — Polish and options

Starts only after the Phase 5 gate (playable port on hardware). Localization
is an explicit project goal in this phase, not during bring-up.

- [ ] **P6.1** Integer scaling / aspect options; optional widescreen
  where the GTE projection allows.
- [ ] **P6.2** PAL/NTSC and EU/US selection at boot; language from EU
  `CNTY_SEL`.
- [ ] **P6.3** PGXP-style vertex precision from PSY-Z if it stays stable.
- [ ] **P6.4** Fast-forward / frame-skip toggles for grinding (optional).
- [ ] **P6.5** Release packaging: `.nro` + `README` for own-disc setup;
  never ship data.
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

Gate: polish options from P6.1–P6.5 land as needed; localization P6.6–P6.8
may still be in progress when Phase 7 starts only if the Phase 5 playable
gate and the Phase 6 packaging baseline (P6.5) are already met. Prefer
finishing P6.2 (EU language / NTSC) before QoL parity work that assumes PAL
language slots.

## Phase 7 — Community QoL and companion compatibility

Starts after the playable port (Phase 5) and the Phase 6 packaging /
host-options baseline. Goal: optional compatibility with the Digimon World
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

Gate: owner-patched EU data path works (P7.2) and the agreed native QoL
subset from P7.4 is playable. Companion (P7.5) is best-effort.

## Phase 8 — Texture pack compatibility

Starts after the Phase 7 gate in the planned order (or earlier only if the
P1.6 runtime layout and a stable TIM/draw path already exist to validate
against). Goal: owners can overlay replacement textures without forking
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

Gate: P8.2 works with an owner-supplied minimal pack. P8.4 is best-effort.

## Cross-cutting rules

- Record every non-obvious decision as a short note in `docs/`.
- Changes to the decomp go through `port/patches/` first; upstream PRs for
  anything match-neutral; submodule bumps are explicit commits.
- CI builds PC and Switch skeletons on every push; game-data tests run
  locally only.
- Keep `platforms/switch/` thin: platform backend only, no game logic.
