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

- [ ] **P1.1 Build environments in Docker.** `tools/docker/` with two
  images: the decomp's (`vendor/dw3_decomp/Dockerfile`, reused) and
  `devkitpro/devkita64` for Switch. Wrapper `tools/dk.sh <cmd>` runs a
  command in the devkitA64 image with the repo mounted.
  Done when: `tools/dk.sh make -C platforms/switch` works on a clean
  machine with only Docker.
- [ ] **P1.2 Vendor PSY-Z.** Add `vendor/psyz` as a submodule, pin a
  commit, note the license. Decide `-DPSYZ_RENDERER` default for PC
  (`sdl3_gl` vs software) in `docs/`.
  Done when: PSY-Z's `samples/gpu_hello` builds and runs on this PC from
  our tree.
- [ ] **P1.3 PC host skeleton.** `platforms/pc/` with a CMake project
  that links PSY-Z and a stub `main` (clear screen, read a pad, print to
  stdout). This is the bring-up target for Phases 2–3.
  Done when: `cmake -B build/pc && cmake --build build/pc` yields a
  window that closes on input.
- [ ] **P1.4 Switch host skeleton (`.nro`).** `platforms/switch/` Makefile
  on libnx: console/framebuffer, hid pad read, nxlink stdout, romfs mount,
  clean exit to hbmenu. No PSY-Z yet.
  Done when: the `.nro` boots on hardware or in an emulator, shows a
  status screen and logs over nxlink.
- [ ] **P1.5 Disc extraction + file-id map.** Document the owner-side
  steps (`tools/extract_disc.py` from the decomp into `disks/<ver>/`) and
  add `tools/build_file_map.py` that reads the ISO directory records and
  `FILE_SECTORS` to emit `files_<ver>.json` (`id → path, sectors, form`).
  Done when: every id in `FILE_SECTORS` resolves to exactly one path for
  the version, with a `--check` mode that reports gaps.
- [ ] **P1.6 Runtime data layout.** Define and document the on-SD layout
  (`sdmc:/switch/dw3/<ver>/AAA/...` + `files_<ver>.json` + exe tail boot
  image) and the equivalent PC directory. Add `tools/pack_runtime.py` to
  produce it from `disks/<ver>/`.
  Done when: `pack_runtime.py` output passes `build_file_map.py --check`
  and `.gitignore` covers it.
- [ ] **P1.7 CI.** GitHub Actions building P1.3 and P1.4 in Docker on
  every push (no game data; PSY-Z sample + skeleton only).
  Done when: a green run on `master`.

Gate: PC window + Switch `.nro` skeleton + `files_us.json` for the owner's
disc.

## Phase 2 — Port glue and engine bring-up on PC

Objective: compile the decomp's **executable** modules (`src/engine/`) for
the host, replace MIPS-only pieces, and reach the title screen on PC.

- [ ] **P2.1 Source selection and flags.** `port/CMakeLists.txt` (or
  `port/sources.mk`) that lists the decomp's C through the version's
  `C_SRC` rule, builds with the GCC 2.8.1-semantics flags from
  `mk/toolchain/gcc.mk` (`-std=gnu89 -fsigned-char -fno-builtin
  -fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks
  -fno-aggressive-loop-optimizations -fno-zero-initialized-in-bss
  -fno-common -fno-toplevel-reorder`), `VERSION_US/EU`, PSY-Z include
  path first.
  Done when: `src/engine/**/*.c` compiles to objects on x86_64 with
  `-Werror=implicit-function-declaration -Werror=int-conversion`.
- [ ] **P2.2 Patch mechanism for the submodule.** `port/patches/*.patch`
  applied by `tools/apply_patches.sh` into a build copy (never in-place),
  with a `--check` that fails if the submodule moved. Policy in
  `docs/PATCHES.md`: every patch states whether it is upstream-bound
  (match-neutral) or port-only.
  Done when: an empty patch series round-trips and CI runs the check.
- [ ] **P2.3 Replace MIPS-only code.** `SetSpadStack`/`ResetSpadStack`
  → no-ops under `PORT_HOST`; `allocMemHigh` returns the pointer;
  `OVERLAY_ADDRESS`/`SUB_OVERLAY_ADDRESS` become host symbols; boot TIM
  read from the packed exe tail; GTE macros in `include/engine/gte.h`
  mapped to PSY-Z's software GTE under `PORT_HOST`.
  Done when: no `__asm__` remains in the host build and `grep -r mtc2`
  over the preprocessed output is empty.
- [ ] **P2.4 64-bit audit, part 1 (engine).** Replace literal sizes with
  `sizeof` in `src/engine` (`HEAP.zero(&GFX.prim, 0x18)`,
  `PAD_DATA_SIZE`, `MemBlock` split constant), fix `freeFilesFrom` to
  take an offset/symbol, remove pointer↔`u32` casts in engine code.
  Match-neutral changes go upstream.
  Done when: `-Wpointer-to-int-cast -Wint-to-pointer-cast` are clean for
  `src/engine` and `make compare` upstream still says `OK`.
- [ ] **P2.5 CD → filesystem.** `port/cdfs/`: implement the file-id read
  path the engine uses (`CD_READER.readFile` semantics, `FILE_TABLE`
  sector counts, `cdReadyCallback`-style completion) over
  `files_<ver>.json` and host files; keep PSY-Z's `libcd` only for STR
  streaming.
  Done when: `FILE_CACHE.load(id)` returns byte-identical buffers to the
  disc for a sample of ids (unit test with the owner's data, skipped in
  CI).
- [ ] **P2.6 Static overlay model.** `port/overlays/`: generate per-binary
  symbol prefixes for the 21 overlays + stages (objcopy
  `--prefix-symbols` or a generated `#define` header), a table
  `{file id → entry, data start/end}` and `loadModeOverlay` /
  `loadSubOverlay` replacements that restore the overlay's initial
  `.data` **only when the id changes**, and zero its `.bss`.
  Done when: two different stages and the same stage twice behave as on
  PS1 (globals reset vs. preserved), checked by a host test.
- [ ] **P2.7 Memory card → file.** `port/memcard/`: back `MemCard*`
  calls with a 128 KiB PS1-format card image per slot on disk, so saves
  stay importable/exportable (`.mcr`).
  Done when: a save written on PC loads in DuckStation and vice versa.
- [ ] **P2.8 Boot to title on PC.** Link engine + `STDWTITL` + `CNTY_SEL`
  (EU) and run `main()` under PSY-Z.
  Done when: the boot image shows, then the title screen renders and
  reacts to the pad on PC (movies may be skipped at this point).

Gate: title screen on PC from the decomp's C, no local edits inside
`vendor/`.

## Phase 3 — Game bring-up on PC

Objective: the full game loop runs on PC: field, stages, battle, menus,
audio, movies.

- [ ] **P3.1 64-bit audit, part 2 (overlays + stages).** Same rules as
  P2.4 across `src/field`, `src/battle`, `src/cardgame`, `src/menus`,
  `src/debug` (11 literal `createTask` sizes, `(s32)0x80000000` mode arg
  in `stage_select.c`, `(Task *)` function-pointer casts).
  Done when: warnings clean tree-wide; upstream PRs opened for the
  neutral ones.
- [ ] **P3.2 Field + first stage.** `FIELDSTG` + `WSTAG260` scripts + the
  starting town stage, with map, actors, text windows and triggers.
  Done when: new game → name entry → first field area is walkable and a
  door changes stage.
- [ ] **P3.3 Battle.** `FIGHTSTG` + `WFIGHTMN` (+ `WFIGHTTS` from the debug
  menu), models, camera, effects, `STFGTREP` report.
  Done when: a random encounter completes and returns to the field with
  stats applied.
- [ ] **P3.4 Menus and shops.** `STSTATUS`, `STITSHOP`, `STGTRAIN`,
  `STGDGLAB`, `STGMCARD`, `STDGNAME`, `STPLNMET`.
  Done when: each opens from the field, works end-to-end, and saving via
  `STGMCARD` uses P2.7.
- [ ] **P3.5 Card game.** `CARDGAME`, `STCRDDEK`, `STCRDSHP`, `STCRDABM`.
  Done when: a card battle can be won and the album updates.
- [ ] **P3.6 Audio.** libsnd path through PSY-Z: VAB banks, SEP music,
  SFX, reverb settings the game sets. Verify `SsSeqCalledTbyT` timing
  from the vsync callback.
  Done when: title music, field music change on stage switch, battle SFX.
- [ ] **P3.7 Movies.** Title STR playback through PSY-Z `libpress` +
  streaming (`CdRead2`, `StSetRing`) from raw-sector `.STR` files packed
  in P1.6.
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

## Cross-cutting rules

- Record every non-obvious decision as a short note in `docs/`.
- Changes to the decomp go through `port/patches/` first; upstream PRs for
  anything match-neutral; submodule bumps are explicit commits.
- CI builds PC and Switch skeletons on every push; game-data tests run
  locally only.
- Keep `platforms/switch/` thin: platform backend only, no game logic.
