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
[ARCHITECTURE.md](ARCHITECTURE.md).

## Status at a glance

Statuses: **done** = ticked; **in progress** = not ticked, with merged work
or hardware evidence; **open** = not ticked, no work yet; **future** = goal
note only (see [Backlog / future goals](#backlog--future-goals)). Update
this table in the same PR that changes an item. The history of an item
lives in its doc, not in the entry below.

| ID | Item | Status | Doc |
|---|---|---|---|
| P1.1–P1.7 | Toolchains, data pipeline, host skeletons | done | [DISC_SETUP](DISC_SETUP.md), [RUNTIME_LAYOUT](RUNTIME_LAYOUT.md), [CI_SUBMODULES](CI_SUBMODULES.md) |
| P2.1–P2.8 | Port glue and engine bring-up on PC | done | [PATCHES](PATCHES.md), [CDFS_NOTES](CDFS_NOTES.md), [P2.8](P2.8_TITLE_BOOT.md) |
| P3.1–P3.9 | Game bring-up on PC | done | [P3_GATE](P3_GATE.md), [P3_EU_BUILD](P3_EU_BUILD.md), P3.x docs |
| P4.1 | Backend spike | done | [SWITCH_BACKEND](SWITCH_BACKEND.md) |
| P4.2 | Video | done | [P4.2](P4.2_VIDEO.md) |
| P4.3 | Audio | in progress | [P4.3](P4.3_AUDIO.md) |
| P4.4 | Input | done | [P4.4](P4.4_INPUT_STATUS.md) |
| P4.5 | Filesystem + log + time | done | [P4.5](P4.5_STATUS.md) |
| P4.6 | Static modules on Switch | in progress | [P4.6](P4.6_MODULES_STATUS.md) |
| P4.7 | Boot the game | done | [P4.7](P4.7_BOOT.md) |
| P5.1 | Memory budget | in progress | [P5.1](P5.1_MEMORY.md) |
| P5.2 | Performance pass | done | [P5.2](P5.2_BATTLE_GPU.md) |
| P5.3 | Save handling | done | [P5.3](P5.3_FLAT_SAVE.md) |
| P5.4 | Error reporting | in progress | [P5.4](P5.4_CRASH.md) |
| P5.5 | Stage sweep on hardware | open | — |
| P6.1 | Scaling / widescreen | in progress | [P6.1](P6.1_WIDESCREEN.md) |
| P6.2 | PAL/NTSC, EU/US, language at boot | in progress | [P3_EU_BUILD](P3_EU_BUILD.md) |
| P6.3 | PGXP-style vertex precision | open | — |
| P6.4 | Fast-forward | in progress | [P6.4](P6.4_FAST_FORWARD.md) |
| P6.5 | Release packaging | done | [P6.5](P6.5_PACKAGING.md), [RELEASES](RELEASES.md) |
| P6.6 | Localization goal | open | — |
| P6.7 | Language packs (PT-BR first) | open | — |
| P6.8 | Extra locales | open | — |
| P6.9 | Restart-game hotkey | in progress | [P6.9](P6.9_RESTART.md) |
| P6.10 | Hardware render outside battle | future | [P5.2](P5.2_BATTLE_GPU.md) |
| P6.11 | Internal resolution scaling | future | [PC README](../platforms/pc/README.md) |
| P6.12–P6.14, P6.16–P6.21 | Backlog goal notes | future | [backlog](#backlog--future-goals) |
| P6.15 | Regression replay tests | in progress | [P6.15](P6.15_REPLAY_TESTS.md) |
| P7.1 | Inventory and boundaries | done | [COMMUNITY_QOL](COMMUNITY_QOL.md) |
| P7.2 | Patched EU disc as data source | open | [COMMUNITY_QOL](COMMUNITY_QOL.md) |
| P7.3 | Binary-patch limits | in progress | [COMMUNITY_QOL](COMMUNITY_QOL.md) |
| P7.4 | Feature parity (native) | open | [COMMUNITY_QOL](COMMUNITY_QOL.md) |
| P7.5 | Companion (optional) | open | [COMMUNITY_QOL](COMMUNITY_QOL.md) |
| P7.6–P7.14 | Backlog goal notes | future | [backlog](#backlog--future-goals) |
| P8.1 | Texture pack goal note | done | [TEXTURE_PACKS](TEXTURE_PACKS.md) |
| P8.2 | Pack layout | open | [TEXTURE_PACKS](TEXTURE_PACKS.md) |
| P8.3 | Formats and scaling | open | [TEXTURE_PACKS](TEXTURE_PACKS.md) |
| P8.4 | Tooling (optional) | open | — |
| P9.1 | Port options goal note | done | [PORT_OPTIONS](PORT_OPTIONS.md) |
| P9.2 | Menu entry and shell | done | [P9.2](P9.2_MENU.md) |
| P9.3 | Toggle registry | done | [PORT_OPTIONS](PORT_OPTIONS.md) |
| P9.4 | Persistence (blocks many goals) | open | [PORT_OPTIONS](PORT_OPTIONS.md) |
| P9.5 | Wire real features | in progress | [PORT_OPTIONS](PORT_OPTIONS.md) |
| P9.6 | External save converter | open | — |
| P10.1 | Switch features goal note | done | [SWITCH_FEATURES](SWITCH_FEATURES.md) |
| P10.2 | Lifecycle hardening (has P5.3's leftover) | open | [SWITCH_FEATURES](SWITCH_FEATURES.md) |
| P10.3 | Presentation modes | open | — |
| P10.4 | Rumble polish (optional) | open | — |
| P10.5 | Extra applets (optional) | open | — |

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
  **Status (2026-10-11):** the CMake game image links PSY-Z's SPU.
  Opening-movie XA played in Ryubing. The Makefile `host_hello` sample
  still uses the weak 440 Hz stub in `nx_audio.c`. **Not ticked:** no
  hardware listen, and no underrun count on device. See
  `docs/P4.3_AUDIO.md`; the listen + underrun recipe (dry-pass count,
  `Dw3NxAudioStats` exports, ≥10 min nxlink session) is planned out in
  `docs/P4.3_AUDIO_HW.md` (2026-10-16, not run yet).
  **Ready to close? Needs:** an underrun count from a hardware log. Owner
  listens already exist: game audio "sounding perfect" on preview.4 (P4.7)
  and correct XA audio on preview.21 (P3.7). Owner to decide whether a
  listen is enough, or to set a number (for example 0 underruns over 10 min
  of field and battle in the nxlink log).
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
  **Options follow-up (2026-10-15):** owner tested the EU #170 candidate
  with the HUD: navigation improved but remains somewhat slow. This is a
  partial gain, not completion. Cached component/GPU composition is proposed
  in [MENU_COMPOSITION_INVESTIGATION](MENU_COMPOSITION_INVESTIGATION.md).
  **PC prototypes (2026-10-22, PC host only, opt-in `DW3_MENU_COMPOSITION`,
  default = reference):** the CPU dirty-region cache (`cpu`) is pixel-identical
  to reference on 284/284 US+EU replay checkpoints and cuts the host cost of a
  cursor move from ~29.5 ms to ~1.0 ms (value change ~29.6 ms to ~0.34 ms).
  The GPU atlas (`gpu`) fails the visual gate (3,564 edge pixels, 90/284
  checkpoints) and is not the candidate. **The Switch host
  (`nx_title_overlay.c`, GLES) is not changed yet, so the OLED behaves exactly
  as on preview.28; OLED acceptance needs the Switch adapter port (open).** Stock
  clocks only (no overclock). History (preview.7–.26, mesh approach A,
  flat-blue root cause): [P5.2_BATTLE_GPU.md](P5.2_BATTLE_GPU.md#history-moved-from-roadmapmd).

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
  **Status (2026-10-15):** in progress; the owner is investigating the
  crash-report path on hardware. Not ticked.
  **Status (2026-10-16):** in progress; report path researched and small
  code landed. `crash_report.txt` now rotates the previous run to
  `crash_report_prev.txt`, and startup logs a "previous run crashed?"
  latch. The report gains frame counters (`vsync_pumps`, `last_dt_us`
  via `dw3_nx_crash_note_vsync`). `tools/build_switch_game.sh` gained a
  `--force-crash` flag that passes `-DDW3_NX_FORCE_CRASH=ON` through the
  cache (warns when used without `--nxlink`). Kernel-path analysis
  (libnx user handler vs Atmosphere creport vs `fatal`) and the captured
  field checklist are in
  [P5.4_ERROR_REPORT.md](P5.4_ERROR_REPORT.md). Not ticked.
  **SD crash report (2026-10-12):** `__libnx_exception_handler` writes
  `sdmc:/switch/dw3/crash_report.txt` (version, reason, mode/stage file
  ids, PC/LR/FAR/ESR). Install from `boot_main` after SD+log open; does
  not touch P5.3 memcard atexit. Force-fault: `-DDW3_NX_FORCE_CRASH=ON`
  (soft default OFF). See [P5.4_CRASH.md](P5.4_CRASH.md).
  Owner crash reports from preview.26 are in
  [PREVIEW26_CRASH_RECORDS.md](PREVIEW26_CRASH_RECORDS.md) (#156).
  **Still open:** the forced-fault hardware run
  (`build_switch_game.sh --nxlink --force-crash` producing a readable
  `crash_report.txt`) and the on-screen assert overlay (HUD hook in
  [DEBUG_OVERLAY.md](DEBUG_OVERLAY.md) remains triage-only).
  Note: the P5.2 nxlink on-screen HUD (`DW3_NX_DEBUG_OVERLAY`) is not a
  substitute for this task.
- [ ] **P5.5 Stage sweep on hardware.** P3.8 again on the console.
  Done when (proposed, from P3.8): the P3.8 loadable stage set loads and
  returns on hardware, US and EU. P6.20's warp menu would help.

Gate: start → first boss on hardware, both versions.

## Phase 6 — Polish and options

Starts only after the Phase 5 gate (playable port on hardware). Localization
is an explicit project goal in this phase, not during bring-up. P6.5 is
the exception: packaging was done early because the owner asked. That
tick does not start Phase 6.
In practice P6.1, P6.2, P6.4 and P6.9 also have early work at the owner's
request (marked "in progress" in the
[status table](#status-at-a-glance)). That does not change the Phase 5
gate, which stays open.

- [ ] **P6.1** Integer scaling / aspect options; optional widescreen
  where the GTE projection allows.
  **Widescreen first pass (2026-10-14, owner asked early; Phase 6 not
  started):** the Port options Widescreen toggle (or `DW3_WIDESCREEN=1`)
  draws side gutters outside VRAM, 16:9 in battle and 16:10 in the field,
  on PC `sdl3_gpu` and the Switch renderer. Host tests and a synthetic PC
  render pass; PC and Ryubing validated with the owner's disc (PR #123),
  shipped in v0.6.5-preview.22. Full 16:9 in the field needs more tile
  slots. Integer scaling is untouched. See
  [P6.1_WIDESCREEN.md](P6.1_WIDESCREEN.md).
  **OLED feedback (2026-10-14) applied:** field 16:9 (fifth tile column
  in 3 host tile slots), battle cold-boots in `gpu-tex` with the partner
  view fixed, Widescreen ON by default, the field menu / shop / memory
  card / partner name screens fill 16:9, full-screen fades (the Gatomon
  inn night) cover the gutters, and EU no longer shows 16:10. Title is in
  PRs #126 / #127. Validated on PC and Ryubing with US and EU; OLED pass
  pending.
  **16:9 field crash (2026-10-15):** preview.26 EU crash record 2 (field
  map) is fixed by patch `0057` in #158 (cause inferred from code, not
  reproduced); shipped in preview.27, hardware acceptance pending. Crash
  record 1 (battle) is separate and open. Menu and transition coverage:
  [P6.1_WIDESCREEN_MENU_AUDIT.md](P6.1_WIDESCREEN_MENU_AUDIT.md) (#161).
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
- [ ] **P6.4** Fast-forward / frame-skip toggles for grinding (optional).
  **Status (2026-10-14, first pass):** R3 toggles 2x/3x/4x fast-forward
  in field and battle; speed (or OFF) in the Port options row. Switch skips
  presents; PC paces at speed × base rate. Done early on the owner's
  request without opening Phase 6. See
  [P6.4_FAST_FORWARD.md](P6.4_FAST_FORWARD.md).
  **OLED feedback (preview.25):** R3 works; field ~70–80 fps and battle
  ~180 fps reported at 2x. Display-buffer raster skipping is deferred:
  encounter transitions read those buffers through MoveImage.
  **Owner OLED feedback (2026-10-14, EU preview.26):** turbo works; reported
  HUD readings are approximately 90 FPS in the Inn, 70 FPS in the city and
  field, and 130 FPS in battle. These are observed counters, not measured
  logical-speed multipliers, display/present rates, or P5.2 benchmark proof.
  P6.4 remains open; no additional measurement or optimization was performed.
  **Ready to close? Needs:** an OLED turbo check on the US disc, including
  the return to normal speed. PC and Ryujinx already cover US and EU,
  including R3 off back to 1x (P6.4_FAST_FORWARD.md, commit `0b94858`);
  the OLED reports (preview.25, preview.26) are EU only.
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
  **PC implementation / evidence (2026-10-13):** title Port options has an
  opt-in restart setting (OFF by default, session-only). The PS1 logical
  chord L1+R1+Start+Select runs shared `port/` policy, flushes P5.3 dirty
  saves, and cancels if flushing fails. It resets `GAME` state and the active
  mode/overlay inside the existing `dw3_game_main()` loop; host, PSY-Z, audio,
  heap, pad, and graphics initialization do not run again. Private Xvfb PC run
  returned to title after the successful flush. See
  [P6.9_RESTART.md](P6.9_RESTART.md).
  **Owner OLED feedback (2026-10-14, EU preview.26):** the restart hotkey
  works. However, choosing a language, reaching the main menu, then restarting
  leaves the title menu above the language-selection screen. Cause: the
  native title overlay state was only cleared by title-task draws, which a
  restart discards; EU re-enters `CNTY_SEL`, so the overlay (menu plaques,
  logo, fade) stayed up. Fix: restart now resets that overlay and, as on a
  real PS1 reset, EU returns to a clean language selection. Ryubing US and EU
  validation in [P6.9_RESTART.md](P6.9_RESTART.md). Continue reload of a save
  written before restart is not newly confirmed; P6.9 stays open.
  **Status (2026-10-15):** the EU restart overlay bug is fixed in #162
  (Ryubing US and EU). Still open: restart before/after a battle and after
  an in-game save on both discs, and an OLED retest.
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

- **P6.12–P6.21** are goal notes, kept with their ids in
  [Backlog / future goals](#backlog--future-goals).

Gate: polish options from P6.1–P6.5 land as needed; localization P6.6–P6.8
may still be in progress when Phase 7 starts only if the Phase 5 playable
gate and the Phase 6 packaging baseline (P6.5) are already met. Prefer
finishing P6.2 (EU language / NTSC) before QoL parity work that assumes PAL
language slots.

**Proposed Phase 6 gate (proposal for the owner, not adopted):** the
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

Gate: P8.2 works with an owner-supplied minimal pack. P8.4 is best-effort.

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
  still slow; PC `cpu` prototype validated, Switch port open, see
  [composition proposal](MENU_COMPOSITION_INVESTIGATION.md)),
  persistence (P9.4). History:
  [P9.2_MENU.md](P9.2_MENU.md).
  Done when: the screen is reachable from the main menu and returns
  cleanly to it (PC + Switch; TTB print on PC).
- [x] **P9.3 Toggle registry.** Maintain a registry of non-native features
  (Phase 6–8 items and later), each with a default equivalent to stock
  behaviour (or documented exception).
  Done when: flipping a stub or real flag from the menu has a documented
  effect (safe no-op allowed for not-yet-wired features).
  **Status (2026-10-13):** shared registry has Widescreen and Fast-forward
  safe no-op stubs plus the real session-only Restart hotkey, OFF by default.
  Fast-forward became a real session-only row on 2026-10-14 (P6.4).
  Widescreen became a real session-only toggle on 2026-10-14 (P6.1).
  Its chord and flush behavior are documented in
  [PORT_OPTIONS.md](PORT_OPTIONS.md) and [P6.9_RESTART.md](P6.9_RESTART.md).
  P9.4 persistence and P9.5 wiring of remaining real features remain open.
- [ ] **P9.4 Persistence.** Load/save the option file under the owner
  runtime layout (PC directory / `sdmc:/switch/dw3/…`); values survive
  process exit.
  Done when: a toggled value remains after restarting the host or
  returning from hbmenu.
  Blocks: P6.12, P6.17, P6.18, P7.6–P7.10 and a persisted P6.9 restart
  setting (see [Backlog](#backlog--future-goals)).
- [ ] **P9.5 Wire real features.** Connect registry entries to real port
  flags as Phases 6–8 land; mark unavailable items N/A in
  [PORT_OPTIONS.md](PORT_OPTIONS.md) rather than hiding the menu.
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
- [ ] **P10.3 Presentation modes.** Docked vs handheld already in P4.2;
  verify operation-mode switches do not break the frame loop once the
  game is linked, and that P6.1 scale/widescreen options apply on both.
  Done when: dock/undock during title or field keeps a stable present path.
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
- [ ] **P10.5 Extra applets — optional.** `swkbd` name entry, system
  language hint, CRT/sharp filters, multi-card / user-folder saves;
  Capture/album best-effort. Skip amiibo/gyro/touch-in-game.
  Done when: each item is either wired + toggled or explicitly N/A in
  [SWITCH_FEATURES.md](SWITCH_FEATURES.md). Does **not** block the Phase
  10 gate.

Gate: P10.2 (+ P10.3 once present is live). P10.4–P10.5 are best-effort.

## Backlog / future goals

Goal notes recorded early at the owner's request. They are not part of any
phase gate and nobody owns them yet. Their ids stay stable (other docs and
PRs cite them); an item moves back into its phase when someone takes it.
Statuses are in the [table](#status-at-a-glance).

**Dependencies not stated in the entries:**

- **P9.4 (option persistence) blocks** P6.12 (remap survives a restart),
  P6.17, P6.18, P7.6–P7.10, and a persisted P6.9 restart setting (today
  session-only).
- **Future P6.15 gameplay scenarios** may need P6.9 (restart to a known
  state) and P5.3 (a fixed save per case). The current title/language replay
  uses a fresh process per scenario; the game RNG is a fixed table seeded to
  0, so no injectable RNG seed is needed.
- **P6.21** comes after P6.10 (field on the GPU) and P6.11 (internal
  resolution).
- P6.20's map warp would help P5.5 (stage sweep on hardware).
- Until P6.19 (JP disc) lands, "both discs" in these done-whens means US
  and EU.
- P7.12 waits for a frozen save format; P6.14 and P6.16 touch saves too.

**Overlaps to decide (owner; nothing merged or deleted here):**

- P6.12 / P6.13: P6.13 is the UI of P6.12. Possible merge.
- P6.18 vs P7.6 / P7.10: P6.18's pacing options overlap the progression
  and training options; P6.18 already says it may land as P7.4 items.
- P6.14 vs P6.16: two "resume" models (in-game save slot vs full state).
  P6.16 also touches P10.2 (sleep/resume).
- P6.7 / P6.8 (Phase 6, not backlog): same pack mechanism; PT-BR is the
  first case of P6.8.

### Phase 6 goal notes

- [ ] **P6.12 Button mapping (future goal).** Let the owner remap the
  PS1 pad buttons on the Switch and PC, in `port/` policy with the host
  input mapping kept in `platforms/`. Goal note only, recorded at the
  owner's request (2026-10-15); it does not start Phase 6 and is not part
  of the Phase 5 gate. Done when a remap survives a restart and works on
  the US and EU discs.
- [ ] **P6.13 Mapping submenu (future goal).** A submenu in Port options
  that holds the button mapping (P6.12), with its own screen in the game's
  window style. Goal note only (2026-10-15); not part of the Phase 5 gate.
  Done when the submenu opens from Port options, lists each mapped button,
  and returns with the mapping saved.
- [ ] **P6.14 In-game load / new save menu (future goal).** A load or
  new-save menu reachable during play, so the owner can switch save slots
  without returning to the title. It must flush P5.3 saves before a load
  and keep the save rules of P5.3 intact. Goal note only (2026-10-15); not
  part of the Phase 5 gate. Done when a load or new save from inside the
  game works without a restart, on the US and EU discs.
- [~] **P6.15 Regression replay tests.** Record pad input, replay it and
  compare screenshots at fixed checkpoints, on the US disc and the EU disc
  in all 5 languages. First version landed (2026-10-15):
  `tools/replay_tests.sh` replays frame-keyed pad recordings on the PC host
  under Xvfb (US boot to title menu; EU language select to localized title
  menu in en/fr/de/it/es) and diffs window and PS1-frame captures against
  references kept outside git. `DW3_REPLAY` / `DW3_REPLAY_RECORD` in
  `psyz_pad_host.c` (off by default). Runs are byte-identical on one
  machine; the game RNG is a fixed table seeded to 0, so no injectable seed
  is needed. See `docs/P6.15_REPLAY_TESTS.md`. Still open: Switch/Ryubing
  replay, scenarios past the title (New Game, field, battle, save, restart),
  movies, cross-machine tolerance, CI. The full goal remains open until the
  replay set covers gameplay and Switch.
- [ ] **P6.16 Suspend / resume (future goal).** Save the whole port state
  (RAM, VRAM, SPU, overlay state) on exit or sleep and resume at the same
  point. Separate from P5.3 game saves. Goal note only (2026-10-15). Done
  when exit and relaunch resume mid-play on US and EU.
- [ ] **P6.17 Music / SFX volume (future goal).** Separate music and
  sound-effect volume rows in Port options, persisted with P9.4. Goal note
  only (2026-10-15). Done when both sliders work on US and EU.
- [ ] **P6.18 Pacing options (future goal).** Text speed, encounter rate
  (0.5x, 1x, off) and faster battle animations as Port options toggles; may
  land as concrete P7.4 items. Goal note only (2026-10-15). Done when each
  toggle works on US and EU and is off by default.
- [ ] **P6.19 Japanese disc support (future goal).** File map, executable
  differences and boot for the JP disc, next to US and EU. Goal note only
  (2026-10-15). Done when the JP disc boots to the field on PC and Switch.
- [ ] **P6.20 Port debug / cheat menu (future goal).** Port-side menu for
  money, items and map warp, to speed up testing. Off by default. Goal
  note only (2026-10-15). Done when it works on US and EU.
- [ ] **P6.21 60 fps interpolation (far future).** Interpolate camera and
  field movement in 30 fps sections. After P6.10. Goal note only
  (2026-10-15).
  Done when (proposed): camera and field movement in 30 fps sections
  present at 60 fps on PC and Switch with the US and EU discs, and the
  original 30 fps stays selectable.

### Phase 7 goal notes

- [ ] **P7.6 Progression options (future goal).** Port options for EXP
  and DV EXP rate (1x default, Harmony uses 1.3x / 1.2x), uncapped DV EXP
  and HP/MP restore on level up. Credit markisha64 / rsigrist. Done when
  each option works on US and EU and is off by default.
- [ ] **P7.7 Bug and text fixes (future goal).** Picking / Snapping Claw on
  a KO'd enemy, active Digivolution name in battle, battle reward
  "1IT"/"1EXP" text, save label showing Asuka Inn, Square map glitch.
  Check each on the US disc too. Done when each fix is a `port/patches`
  entry tested on US and EU.
- [ ] **P7.8 Battle info HUD (future goal).** Optional enemy HP, always-on
  partner MP, technique attribute / power / MP cost colours, DV stats and
  EXP to next level after battle. Done when the toggle works on US and EU
  in all 5 EU languages.
- [ ] **P7.9 Battle menu memory (future goal).** Remember the last battle
  command (Techniques stays on Techniques) and allow reordering techniques
  per Digimon. Done when both persist in the P5.3 save on US and EU.
- [ ] **P7.10 Training without RNG (future goal).** Guaranteed base gains,
  optional timing bonus, Fast Training skip, as an option off by default.
  Done when it works on US and EU.
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
