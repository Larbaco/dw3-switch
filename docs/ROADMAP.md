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
pad state connected; GPU polish open; merged via PR #3).

## Phase 3 — Game bring-up on PC

Status: **complete** (gate met on `phase3-pc-bringup` @ `a5ae93a`;
plan:
[`docs/superpowers/plans/2026-10-10-phase3-pc-game-loop.md`](superpowers/plans/2026-10-10-phase3-pc-game-loop.md)).

Tracking: Phase 3 on `main` / `phase3-pc-bringup` (P3.5 merged from
`phase3-cardgame` @ `fcff119`). P3.1 done. **P3.2 Done-when met** (+
WSTAG790 follow-up). **P3.3 Done-when met:** forced encounter →
`STFGTREP` (XP/levels) → return to `FIELDSTG` (`docs/P3.3_BATTLE.md`).
**P3.4 Done-when met** (`READY_FOR_MAIN_ROADMAP_SYNC: P3.4`,
`docs/P3.4_MENUS.md`): menus open/return from field; **STSTATUS Triangle
UI cancel** (no `AUTO_MENU_RETURN`); **STGMCARD save + load** via P2.7
`.mcr`; pad START→status; FORCE_NPC shop/lab/naming/gym; **WSTAG245
`talkToActorAt` → shop** (warped proximity). Follow-up: free pad walk-up.
**P3.5 Done-when met**
(`READY_FOR_MAIN_ROADMAP_SYNC: P3.5`, `docs/P3.5_CARDGAME.md`): force-win
→ prize item → `FIELDSTG`; album `cardsSeen` 29→33 via vanilla
`STCRDSHP_openPack` `addCards` (host win `addCards` gated off). **P3.7 Done-when met** (`READY_FOR_MAIN_ROADMAP_SYNC: P3.7`,
`docs/P3.7_MOVIES.md`): sustained STR play + Start-skip → `MODE_TITLE` +
XA decode/mix + **owner audible DAC listen** (Pulse rebuild). Follow-up:
video presentation quality (frames/decode/skip already evidenced). Default
skip remains `MODE_TITLE` (`0010`).
**P3.8 Done-when met** (`READY_FOR_MAIN_ROADMAP_SYNC: P3.8`,
`docs/P3.8_STAGES.md`): `STAGSLCT` + batches 1–6 link 238/239 US stage
objs; live PASS 237/237; only `WSTAG460` left stubbed (host
`updateWanderer` SEGV). **Phase 3 gate met** (`docs/P3_GATE.md`): US
continuous start→movie→Lobby→Tamer→Genji→field @ `a5ae93a` (Lobby
`0x9004@0x203` / patch `0036`); `DW3_FORCE_WIN` accepted for gate (P3.3
class; pad KO follow-up); EU playthrough not required. **EU `dw3-pc` link
scaffold** (`docs/P3_EU_BUILD.md`): `-DDW3_VERSION=eu` +
`overlay_table_eu.c` configure/link; `runtime/eu` / playthrough still open.
P3.9 harness (`tools/compare_harness/`, `docs/P3.9_REFERENCE.md`):
**done** — title, field, and Kunemon vs Monmon Fight-menu battle pairs
(`DW3_FORCE_ENCOUNTER=34` + `DW3_FORCE_PARTY=1` on PC).

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
  Done when: title music, field music change on stage switch, battle SFX.
- [x] **P3.7 Movies.** Title STR playback through PSY-Z `libpress` +
  streaming (`CdRead2`, `StSetRing`) from raw-sector `.STR` files packed
  in P1.6. **Done-when evidence** (`docs/P3.7_MOVIES.md`,
  `READY_FOR_MAIN_ROADMAP_SYNC: P3.7`): Wave 1–4 + present + Task 5 —
  ring backpressure; sustained `DecDCTout` #330+; FB dump nonzero; XA
  file-bridge mix; `DW3_AUTO_PAD=start` + `0034` → Start-skip →
  `mode=0xE00`; **owner audible XA** on real Pulse/PipeWire DAC after
  SDL rebuild (`SDL_PULSEAUDIO=ON` + `-DSDL_X11_XSCRNSAVER=OFF`).
  Follow-up: video presentation — half-width + mid-grey on
  `phase3-pc-bringup`; **recognizable MOVIEOPN frames** on
  `phase3-gpu-glitch` (LP64-safe MDEC quant upload + sync slice order;
  see `docs/P3_GPU_GLITCHES.md`). **R1 closed (2026-10-10):** title
  shred was LP64 `TimBlock.pixels` alignment (patch `0038`); owner
  confirmed the window after #29. Field sprite polish still open.
  Done when: the opening movies play with audio and can be skipped.
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
- [ ] **P4.2 Video.** Present PSY-Z's VRAM/framebuffer via EGL + OpenGL
  (mesa/nouveau on libnx) or deko3d; 60 Hz vsync; docked/handheld
  resolution switch.
  Done when: `gpu_hello`-level sample renders at 60 fps on hardware.
  **Status (2026-10-10):** EGL + GLES2 path + `gpu_hello` sample landed in
  `platforms/switch/psyz_backend/nx_gpu.c` (docked 1080p / handheld 720p,
  `eglSwapInterval(1)`). `.nro` builds via Docker (`DW3_DK_TMPBUILD` on
  NTFS). **Not ticked:** no Switch hardware in session to measure 60 fps.
  Owner machine has local Ryubing/Eden installs.
  **Ryubing smoke (2026-10-10):** the `.nro` loads and its clear/present
  loop animates in Ryubing 1.3.339-canary — plumbing confirmed; emulator fps
  is **not** the done-when. Gate: OLED + hbmenu / nxlink. See
  `docs/P4.2_VIDEO.md`, `docs/SWITCH_BACKEND.md`.
- [ ] **P4.3 Audio.** `audren` output thread fed by PSY-Z's SPU mixer at
  44.1 kHz.
  Done when: libsnd sample plays without underruns.
  **Status (2026-10-10):** audren thread + `Psyz_Audio*` in
  `platforms/switch/psyz_backend/nx_audio.c` (44.1 kHz voice, weak SPU
  tone stub; combined `host_hello` with P4.2 EGL after rebase onto
  `675f1cf`). `.nro` builds via Docker
  (`DW3_DK_TMPBUILD`). **Ryubing smoke (2026-10-10):** the guest reaches
  `audren` — session acquired, `Initialize` (REV4), renderer started.
  **Not ticked:** no Switch hardware for underrun/listen check; full PSY-Z
  libsnd not linked yet (P4.7). See `docs/P4.3_AUDIO.md`.
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
  **Status (2026-10-10):** `platforms/switch/CMakeLists.txt` compiles the Phase 3
  image for aarch64 (reuses `port/CMakeLists.txt`; overlay packing uses target
  binutils). `vendor/psyz` is pinned to `dw3-switch-backend` @ `15afac1`
  (NintendoSwitch, no SDL). `psyz_backend/` supplies `psyz_log`, `Psyz_Os*`,
  pad, `Psyz_VideoVSync`, and the `__wrap_*` hooks — that closed the earlier
  15 undefined platform symbols. `source/boot_main.c` is the CMake `main()` and
  calls `dw3_game_main`. The devkitPro Makefile still builds `source/main.c`
  (`host_hello`: EGL + audren sample); that is the `.nro` CI produces.
  **Link (rebuilt after rebase onto current main):** `dw3_game_main` pulls
  `libgpu.o` in, and the link stops on **20** undefined `Draw_*` symbols.
  `libgpu.o` also references `Draw_SetHorizontalGrid` (21 undefined in the
  object); the linker did not demand that one. No game `.elf` / `.nro` from
  this CMake link. An earlier “links, ~8.5 MiB `.nro`” note was the image
  *before* the boot path pulled `libgpu.o`. **Not ticked:** no title on
  hardware, and the game image does not link yet. See `docs/SWITCH_BACKEND.md`.

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

## Phase 9 — Port options menu

Starts after the Phase 8 gate in the planned order (or earlier once at
least one non-vanilla port feature exists to toggle). Goal: a title / main
menu entry that exposes toggles for **every port-added feature** that is
not part of the original PS1 game, with vanilla-safe defaults and
persistence on owner storage. Details: [PORT_OPTIONS.md](PORT_OPTIONS.md).

- [x] **P9.1 Goal note.** Document scope (port-only toggles), layering
  (`port/` registry + owner config file; hosts stay thin), and that
  defaults preserve original PS1 behaviour.
  Done when: [PORT_OPTIONS.md](PORT_OPTIONS.md) exists and the README
  points at Phase 9 / that note.
- [ ] **P9.2 Menu entry and shell.** Add a port options entry on the
  title / main menu that opens and closes a dedicated screen without
  breaking the vanilla title flow (PC and Switch).
  Done when: the screen is reachable from the main menu and returns
  cleanly to it.
- [ ] **P9.3 Toggle registry.** Maintain a registry of non-native features
  (Phase 6–8 items and later), each with a default equivalent to stock
  behaviour (or documented exception).
  Done when: flipping a stub or real flag from the menu has a documented
  effect (safe no-op allowed for not-yet-wired features).
- [ ] **P9.4 Persistence.** Load/save the option file under the owner
  runtime layout (PC directory / `sdmc:/switch/dw3/…`); values survive
  process exit.
  Done when: a toggled value remains after restarting the host or
  returning from hbmenu.
- [ ] **P9.5 Wire real features.** Connect registry entries to real port
  flags as Phases 6–8 land; mark unavailable items N/A in
  [PORT_OPTIONS.md](PORT_OPTIONS.md) rather than hiding the menu.
  Done when: every shipped non-native feature is either togglable here or
  explicitly documented as N/A. Evolves with the roadmap; does **not**
  require all of P6–P8 complete for the Phase 9 gate.

Gate: P9.2 + P9.4 with at least one persistent toggle (stub or real).
P9.5 stays best-effort as features ship.

## Cross-cutting rules

- Record every non-obvious decision as a short note in `docs/`.
- Changes to the decomp go through `port/patches/` first; upstream PRs for
  anything match-neutral; submodule bumps are explicit commits.
- CI builds PC and Switch skeletons on every push; game-data tests run
  locally only.
- Keep `platforms/switch/` thin: platform backend only, no game logic.
