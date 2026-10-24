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
| P4.3 | Audio | in progress (BGM loop fixed, #209, confirmed on the OLED in rc8; new bug: music stops after a won battle, fix in a separate PR; no underrun count yet) | [P4.3](P4.3_AUDIO.md) |
| P4.4 | Input | done | [P4.4](P4.4_INPUT_STATUS.md) |
| P4.5 | Filesystem + log + time | done | [P4.5](P4.5_STATUS.md) |
| P4.6 | Static modules on Switch | in progress | [P4.6](P4.6_MODULES_STATUS.md) |
| P4.7 | Boot the game | done | [P4.7](P4.7_BOOT.md) |
| P5.1 | Memory budget | in progress | [P5.1](P5.1_MEMORY.md) |
| P5.2 | Performance pass | done | [P5.2](P5.2_BATTLE_GPU.md) |
| P5.3 | Save handling | done | [P5.3](P5.3_FLAT_SAVE.md) |
| P5.4 | Error reporting | in progress (done-when met on the OLED, US + EU; PR 2 boot notice + assert/abort landed, emulator-tested; owner OLED check open) | [P5.4](P5.4_CRASH.md) |
| P5.5 | Stage sweep on hardware | open | — |
| P6.1 | Scaling / widescreen | in progress (rc8 OLED: banner, technique backdrops and encounter spiral reach 16:9; results screen 16:9 with patch 0074 in PR #213, pending OLED; Items menu dissolve 16:9 with patch 0075 and the field menu screens re-anchored to the 16:9 edges with patch 0076, pending OLED; Map screen (0080), results header (0081) and memory card slot picker (0082) fill 16:9 in PR for branch fix/p6.1-ws-map-results-save, pending OLED; open: shop / partner name panels) | [P6.1](P6.1_WIDESCREEN.md) |
| P6.2 | PAL/NTSC, EU/US, language at boot | in progress | [P3_EU_BUILD](P3_EU_BUILD.md) |
| P6.3 | PGXP-style vertex precision | open | — |
| P6.4 | Fast-forward | in progress (OLED turbo works in battle on the US disc, rc7; tick is the owner's call) | [P6.4](P6.4_FAST_FORWARD.md) |
| P6.5 | Release packaging | done | [P6.5](P6.5_PACKAGING.md), [RELEASES](RELEASES.md) |
| P6.6 | Localization goal | open | — |
| P6.7 | Language packs (PT-BR first) | open | — |
| P6.8 | Extra locales | open | — |
| P6.9 | Restart-game hotkey | in progress | [P6.9](P6.9_RESTART.md) |
| P6.10 | Hardware render outside battle | future | [P5.2](P5.2_BATTLE_GPU.md) |
| P6.11 | Internal resolution scaling | future | [PC README](../platforms/pc/README.md) |
| P6.12–P6.14, P6.16, P6.19, P6.21 | Backlog goal notes | future | [backlog](#backlog--future-goals) |
| P6.15 | Regression replay tests | in progress | [P6.15](P6.15_REPLAY_TESTS.md) |
| P6.17 | Music / SFX volume | in progress (owner: volume options work, rc6 EU / rc7 US; relaunch check open) | [P6.17](P6.17_VOLUME.md) |
| P6.18 | Pacing options | in progress (Instant text confirmed on the US OLED, rc7; rest of the OLED check open) | [P6.18](P6.18_PACING.md) |
| P6.20 | Port debug / cheat menu in the Options screen (future goal) | future | [roadmap](#phase-6-goal-notes), [PORT_OPTIONS](PORT_OPTIONS.md) |
| P6.22 | Turbo speed in scripted scenes (future goal) | in progress (PC profile: the CPU rasterizer is ~97 % of a field frame; pixel-identical PSY-Z rasterizer 4-5x faster, OLED check open) | [P6.22](P6.22_FIELD_CPU.md), [roadmap](#phase-6-goal-notes), [P6.4](P6.4_FAST_FORWARD.md) |
| P6.23 | Documentation: player guide, docs index, contributor and modder docs (future goal) | future | [roadmap](#phase-6-goal-notes) |
| P6.24 | One build for US and EU: one `.nro` and one PC binary, region found at boot (future goal) | future | [SINGLE_BUILD](SINGLE_BUILD.md), [roadmap](#phase-6-goal-notes) |
| P7.1 | Inventory and boundaries | done | [COMMUNITY_QOL](COMMUNITY_QOL.md) |
| P7.2 | Patched EU disc as data source | open | [COMMUNITY_QOL](COMMUNITY_QOL.md) |
| P7.3 | Binary-patch limits | in progress | [COMMUNITY_QOL](COMMUNITY_QOL.md) |
| P7.4 | Feature parity (native) | open | [COMMUNITY_QOL](COMMUNITY_QOL.md) |
| P7.5 | Companion (optional) | open | [COMMUNITY_QOL](COMMUNITY_QOL.md) |
| P7.6, P7.8–P7.14 | Backlog goal notes | future | [backlog](#backlog--future-goals) |
| P7.7 | Bug and text fixes | in progress (2 of 5 fixed; Claw steal and HUD name not yet checked on the OLED) | [P7.7](P7.7_BUG_FIXES.md) |
| P7.9 | Battle menu memory | in progress (part A confirmed on the US OLED, rc7; part B deferred) | [P7.9](P7.9_BATTLE_MENU_MEMORY.md) |
| P8.1 | Texture pack goal note | done | [TEXTURE_PACKS](TEXTURE_PACKS.md) |
| P8.2 | Pack layout | open | [TEXTURE_PACKS](TEXTURE_PACKS.md) |
| P8.3 | Formats and scaling | open | [TEXTURE_PACKS](TEXTURE_PACKS.md) |
| P8.4 | Tooling (optional) | open | — |
| P8.5 | Modding API: mod folder, asset and data-table overrides (future goal) | future | [roadmap](#phase-8--texture-pack-compatibility), [TEXTURE_PACKS](TEXTURE_PACKS.md) |
| P9.1 | Port options goal note | done | [PORT_OPTIONS](PORT_OPTIONS.md) |
| P9.2 | Menu entry and shell | done | [P9.2](P9.2_MENU.md) |
| P9.3 | Toggle registry | done | [PORT_OPTIONS](PORT_OPTIONS.md) |
| P9.4 | Persistence (blocks many goals) | open | [PORT_OPTIONS](PORT_OPTIONS.md) |
| P9.5 | Wire real features | in progress | [PORT_OPTIONS](PORT_OPTIONS.md) |
| P9.6 | External save converter | open | — |
| P9.7 | Port options reachable in game (future goal) | future | [roadmap](#phase-9--port-options-menu) |
| P9.8 | Confirm / cancel layout option (PS positional or Nintendo A/B) (future goal) | future | [roadmap](#phase-9--port-options-menu), [HOTKEYS](HOTKEYS.md) |
| P10.1 | Switch features goal note | done | [SWITCH_FEATURES](SWITCH_FEATURES.md) |
| P10.2 | Lifecycle hardening (has P5.3's leftover) | in progress (sleep and resume confirmed on the OLED, rc7; HOME during a save write and the focus log open) | [SWITCH_FEATURES](SWITCH_FEATURES.md) |
| P10.3 | Presentation modes | open | — |
| P10.4 | Rumble polish (optional) | open | — |
| P10.5 | Extra applets (optional) | open | — |
| P10.6 | Switch-style button glyphs (future goal) | future | [roadmap](#phase-10--native-switch-features-residual), [TEXTURE_PACKS](TEXTURE_PACKS.md) |

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
  **Owner OLED (2026-10-23, preview.30-rc8, US, no log):** the BGM loop
  fix (#209, SEQ/SEP NRPN loop count from Data Entry) works: the music loops
  in the field, the inn and battle. **New bug, open, fix in progress in a
  separate PR:** after winning a battle the music stops; it is silent on the
  results screen and back on the map, while the SFX for exp and level up
  still play. Not reproduced or diagnosed here (no log was provided); the
  rc7 loop bug may have hidden it. The underrun listen still has to be done
  on a build with both fixes.
  **New bug, in progress (2026-10-23, owner OLED, US, preview.30-rc7):**
  the **background music plays only once and then stops**, in the field, in
  the inn and in battle. The suspected cause is the loop count of the SEQ/SEP
  sequences (the NRPN loop controller, still listed as thin in
  [P3.6_AUDIO.md](P3.6_AUDIO.md)). Being fixed in a separate PR; this entry
  only records it, nothing was changed or verified here. It also matters for
  P6.17 (a long listen on the OLED). Still no underrun count.
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
  **Options follow-up (2026-10-15):** owner tested the EU #170 candidate
  with the HUD: navigation improved but remains somewhat slow. This is a
  partial gain, not completion. Cached component/GPU composition is proposed
  in [MENU_COMPOSITION_INVESTIGATION](MENU_COMPOSITION_INVESTIGATION.md).
  **Owner OLED (2026-10-23, handheld, US, preview.30-rc7):** the default
  battle renderer is `gpu-tex` and the performance is good. The rc7 `dw3.log`
  is consistent with that: `soft=0` in all 1707 `frame dt` windows, 211
  `mesh dbg` windows with mesh frames, and the present path `mesh+ui` for the
  battle (the literal text `gpu-tex` is not printed in that log). This
  supports the tick above; it adds no per-mode frame-time numbers, so the
  numeric benchmark subcriterion stays a follow-up.
  **CPU dirty-region cache (2026-10-22, PC and Switch, PR #185 draft):** the
  GPU atlas experiment was dropped (it failed the pixel gate and was no
  faster); the retained-canvas cache is shared code
  (`platforms/pc/src/title_options_cache.c`) and the default on PC
  (`DW3_MENU_COMPOSITION=reference` keeps the old path). It is pixel-identical
  to the reference on 284/284 US+EU replay checkpoints and an exhaustive unit
  test, and cuts a cursor move on PC from ~30-37 ms to ~1.0-1.4 ms and a value
  change to ~0.4-0.5 ms, uploading 434 KB / 30 KB instead of 2.76 MB. The
  Switch host (`nx_title_overlay.c`) now uploads only dirty rectangles with
  `glTexSubImage2D` and logs per-update times in nxlink/HUD builds; Ryubing
  smoke on US and EU is recorded in
  [MENU_COMPOSITION_INVESTIGATION](MENU_COMPOSITION_INVESTIGATION.md#decision-and-switch-port-2026-10-22).
  **Owner OLED (2026-10-22, handheld, preview.29 candidate = main `aa470d3`
  + this cache):** the owner reports the menu "much more responsive". The EU
  `--nxlink` log (about 22 min, turbo used) shows per update: cursor move
  compose ~4.5 ms + upload ~1.4 ms (434 KB), value change ~1.3 ms + ~0.3 ms
  (30 KB), and one ~151 ms rebuild each time the title menu appears (3 in
  the session). This closes the P5.2 Options follow-up on owner feel. Not
  covered by that run: a numeric A/B against preview.28, sleep/wake and
  dock/undock with Options open, the restart-hotkey step, a rim/fade pixel
  check, and the US disc on the OLED (checklist in
  [MENU_COMPOSITION_PLAN](MENU_COMPOSITION_PLAN.md)). Stock
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
  **Status (2026-10-23, report PR 2 = plan steps 6–7):** in progress,
  waiting for the owner's OLED check. **6)** When the boot latch rotates a
  report from the previous run, every build (release, nxlink) shows a
  short English panel after the EGL surface is up: "The last run
  crashed.", version, reason, mode / stage, where the report is
  (`crash_report_prev.txt`), for 8 s or until A / B / + (after 1 s), then
  boots normally. Once per crash (the rotation consumes the report); no
  previous report = nothing drawn, boot unchanged. Drawn with the
  STR-restore bar's scissor clears and the HUD 8×8 font
  (`nx_crash_notice.c`), integer scale 2 handheld / 3 docked. **7)** The
  game image wraps `abort`, `__assert_func`, `exit`, `diagAbortWithResult`
  and `fatalThrow` (`-Wl,--wrap`): an assembly stub saves the caller's
  registers and the PR 1 writer produces the same report (`reason: assert
  file:line func: expr` / `abort` / `fatal 0x… (2xxx-yyyy, …)` / `exit N`,
  pc = the call, backtrace from the caller), the `CRASH` line and
  `svcBreak`; the notice fires for these too. Covers newlib asserts,
  libstdc++ `std::terminate`, mesa's fatal paths and libnx's
  `diagAbortWithResult` callers; not aborts before `main` (no SD yet).
  The survey and the force modes (`DW3_FORCE_CRASH=assert|abort|fatal|exit`
  in `--force-crash` builds) are in [P5.4_CRASH.md](P5.4_CRASH.md#report-pr-2-2026-10-23-steps-67).
  Tested in Ryubing (Xvfb), US + EU; evidence
  `tmp/evidence/2026-10-23-p54-crash-notice/`; re-run on the tree merged
  with `main` at `ea46b6b` (assert US, exit US, abort EU, notice once):
  [PR 2 test results](P5.4_CRASH.md#pr-2-test-results-ryubing),
  `tmp/evidence/2026-10-24-p54-pr2-merge-check/`.
  **Remaining (owner, OLED, US and EU):** the PR 2 hardware check in
  [P5.4_CRASH.md](P5.4_CRASH.md#pr-2-owner-oled-check): a forced assert
  build, then the normal build twice (notice once, then none). P5.4 is
  ticked after that. Still deferred: previews keep or publish each `.elf`
  (release packaging, after rc7).
  **Status (2026-10-23, report PR 1 = plan steps 1–5):** in progress.
  `crash_report.txt` now carries `module_base` and `base+0x…` offsets, a
  validated frame-pointer backtrace, `git` + GNU `build_id`, overlay
  names for `mode_file` / `stage_file`, disc and EU language, decoded
  `esr`, x0–x28 and the last 64 log lines; the handler appends a `CRASH`
  line to `dw3.log` (now a raw `FsFile`). `tools/symbolize_crash.py`
  resolves a report with the matching `.elf` and warns on a build-id
  mismatch. It re-read the preview.30-rc6 OLED crash with the right
  (EU `-nxlink`) `.elf`: `getItemCategory` under
  `STSTATUS_drawItemScreen`, the bug patch 0069 fixed; the earlier
  `spriteDrawerDraw` reading used the wrong `.elf`. Tested in Ryubing
  (US + EU) through `DW3_FORCE_CRASH=sim`, because Ryubing does not deliver
  a real guest fault to the handler.
  **Owner OLED (2026-10-23, handheld, `a2683c3` `--nxlink --force-crash`,
  US and EU):** the done-when is met. On both discs the real null write
  produced a full report (`esr` data abort write, `far 0x0`, matching
  `build_id`, `module_base` equal to the Atmosphere creport base, the last
  log lines) and the `CRASH` line at the end of `dw3.log`;
  `symbolize_crash.py` resolves pc `crash_force_null_write`, lr
  `crash_force_now` and frames `dw3_nx_crash_force_fault` ← `main`.
  Evidence: `tmp/evidence/2026-10-23-p54-oled-forcecrash-{us,eu}/`.
  Not ticked only because the objective also names the assert overlay
  (PR 2 above).
  **Still open:** the PR 2 OLED check (see above). Done after preview.30:
  releases publish `dw3-switch-elf.zip` (see
  [RELEASES](RELEASES.md#publishing-a-release)). See
  [P5.4_CRASH.md](P5.4_CRASH.md).
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
  **Transitions and backdrops (2026-10-23, #202 and #205, merged; not in
  preview.30-rc7, so the rc7 OLED feedback says nothing about them; in
  rc8, see the OLED paragraph below):** the
  technique backdrops (blue band, magic circle) and the area name banner
  now span 16:9 (patches `0072` and `0071`; PSY-Z `dw3-gutter-width`: a
  display-wide band of rows takes the gutters). PC and Ryubing, US and EU,
  captured; widescreen off unchanged.
  **OLED (2026-10-23, preview.30-rc8, owner, US, no log; handheld or docked
  not stated):** the area name banner (#205, patch `0071`) and the technique
  backdrops (#202, patch `0072`) extend to 16:9 correctly: verified. The
  owner also saw the **battle-start circle/spiral effect** extend to 16:9
  correctly, which the list below had as 4:3 and needing a decision; that is
  the owner's observation (no capture), so the spiral item is rewritten as a
  confirm item. The area-change iris was not mentioned, so it stays open.
  **Post-battle results screen with black side bars (not a code
  regression, see below; fixed by patch `0074` in PR #213, pending OLED).**
  **Future:** the
  Items menu, see the last item below. **Open, in this order**
  (details and evidence in [P6.1_WIDESCREEN.md](P6.1_WIDESCREEN.md#open-transitions-2026-10-23)):
  - [ ] **Area-change iris and grid cover** (`FIELDSTG_closeField`): start the
    clip at 320+2*gutter centred on the player and widen the blue grid; a
    small patch with the next free number.
  - [ ] **Encounter spiral** (`FIELDSTG_playBattleTransition`): the owner
    saw the battle-start circle/spiral extend to 16:9 correctly on the OLED
    (rc8). Before, the choice was a black 16:9 with the 4:3 swirl (no work)
    or gutter-aware tiles (PSY-Z: copy the gutters into the transition image,
    tiles to 426 px). Confirm with a capture (PC or OLED) which of the two
    it is, and whether the swirl itself or only the cover reaches the edges;
    if it is right, tick this and drop the decision.
  - [ ] **Post-battle results screen: black side bars** (rc8 OLED report,
    called a regression). It is a long-standing 4:3 results screen, not a
    code regression: PC captures rc7 `b0aed93` vs `a7860bf` are identical
    through battle, report and field fade, and it stood out once the battle,
    banner and technique backdrops went 16:9. Fixed by patch `0074` in
    PR #213 (16:9, replay scenarios `us/eu-battle-report`); tick when verified
    on the OLED (US and EU).
  - [ ] **PC `sdl3_gpu` only:** an empty drawing area (the 0x0 clip
    `closeField` leaves on the map layer) must draw nothing; today
    `UpdateScissor` keeps the previous scissor, so the map shows again after
    the iris closes (also in 4:3). PSY-Z fix; the Switch raster is right.
  - [ ] **"Slow" transitions:** the PC runs the game's own frame counts
    (banner about 126 frames, spiral about 98 plus 30 black). Whether the
    Switch is slower needs an OLED nxlink log (`frame dt us=`, field tile
    reads) across a door and into a battle; a shorter banner hold is the
    P6.18 owner's-call item.
  - [ ] **OLED pass** for the banner and the technique backdrop: **US done
    (rc8, owner)**; EU still to do. Plus a look for other display-wide band
    layers that now draw into the gutters.
  - [ ] Ryubing and the PC show a black hole in the middle of the technique
    magic circle (rotated sprite); it is the same on main, so it is not from
    these patches. Look at it on the OLED first (the rc8 owner report says
    the backdrops look right on the OLED; nothing was said about that hole).
  - [ ] **FUTURE: redesign the Items menu (and its open transition) for
    16:9.** Owner, rc8 OLED: the transition effect when opening the Items
    menu is still 4:3, and the menu itself stays 4:3 while the background
    behind it is 16:9. Goal: draw the Items menu layout and its open
    transition across the 16:9 width, like the other field menus, shop and
    memory-card screens that already fill 16:9. Not started; start from
    [P6.1_WIDESCREEN_MENU_AUDIT.md](P6.1_WIDESCREEN_MENU_AUDIT.md).
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
  **Owner OLED (2026-10-23, handheld, US, preview.30-rc7):** turbo
  (fast-forward) works in battle. The `dw3.log` shows R3 on at 4x and off back
  to 1x three times (`dw3-turbo: speed 4x` / `speed 1x`). This is the US OLED
  check the "Ready to close?" note above asked for, with the return to normal
  speed; the field and the exact FPS were not measured in this round. Left
  unticked: the owner decides whether this closes P6.4.
  **Future goal (2026-10-23):** the owner also finds the New Game opening
  scenes very slow with turbo at 4x (about 70 FPS); recorded as P6.22, nothing
  started (first numbers in P6.22 below: field 68 fps at 4x, battle 190).
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
  opt-in restart setting (OFF by default; saved since P9.4). The PS1 logical
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
- [ ] **P8.5 Modding API (future goal).** Dated 2026-10-24, owner request:
  "modding API". Goal note only, **nothing started, no code**. Generalizes the
  texture pack idea of P8.2 to every kind of owner-supplied replacement, so
  that P8.2 (pack layout), P6.7 / P6.8 (language packs) and P7.2 (patched EU
  disc as data) share one layout and one loader instead of three.
  **What exists today (cited):** the game reads disc data by **numeric file
  id**; `port/cdfs/cdfs.c` maps an id to a path under the runtime root
  (`cdfs_set_root`, `cdfs_load_map` reads `files_us.json` / `files_eu.json`,
  `cdfs_file_size`, `cdfs_read`, `cdfs_read_at`, `cdfs_read_field_tile`;
  relative paths only, `.` and `..` rejected at map load, the on-disk size
  wins over the JSON `bytes`; see `port/cdfs/README.md`). The runtime is
  `sdmc:/switch/dw3/<us|eu>/` ([RUNTIME_LAYOUT.md](RUNTIME_LAYOUT.md)), next
  to `port_options.cfg`. So a **file replacement is one lookup away**: swap the
  path for an id, which is the plan in [TEXTURE_PACKS.md](TEXTURE_PACKS.md)
  (native TIM first, fall back to the vanilla file when the pack has no entry).
  **Hard limit found by the survey:** the overlays and the executable are
  **linked into the NRO** (one image, `.data` restored on a mode change;
  [ARCHITECTURE.md](ARCHITECTURE.md)), so code and tables compiled into them
  (Digimon stats, items, techniques, if they live in C data) do not come from
  the SD card, and replacing an overlay file changes nothing. File-id
  replacement covers what the game really loads by id (textures, maps, message
  files, sound banks, `.STR` movies); **data-table overrides need a port
  hook** (a `port/` step that patches the table after the overlay restore,
  keyed by table and index), and each table has to be located in
  `vendor/dw3_decomp` first. [COMMUNITY_QOL.md](COMMUNITY_QOL.md) says the
  same about MIPS patches: they do not run in the NRO.
  **Scope ideas, in the order of cost:**
  1. *Mod folder:* `sdmc:/switch/dw3/<us|eu>/mods/<mod-name>/` with a small
     manifest (`mod.json`: name, version, region `us` / `eu` / both, API
     version, priority, author, licence) and a tree that mirrors the runtime
     paths or lists file ids.
  2. *Asset overrides* by file id with fallback: textures / sprites (TIM first,
     P8.3), text and strings (the EU `LANGUAGE` slots, P6.7), audio (XA and
     SEP / VAB banks; P6.17 already has Music and SFX buses).
  3. *Data-table overrides* (Digimon stats, items, techniques) through
     declarative files (JSON or CSV) validated against the table shape.
  4. *Load order and versioning:* an order or priority in a `mods.cfg` beside
     `port_options.cfg`, a conflict line in the log (two mods replacing one
     id), an API version a mod must match, and a safe mode that boots with
     mods off.
  5. *US against EU:* ids and files differ (`files_us.json` against
     `files_eu.json`, five EU languages); a mod declares its region and the
     loader refuses a mismatch, as the extract does for a wrong disc.
  6. *Later, script hooks* (events, battle or field scripts): only after 1 to
     4 are stable, with an allow-list; not scoped here.
  **Constraints:** **never redistribute game data.** A mod ships as a diff or
  patch (for example xdelta against the owner's own file) or as wholly new
  content, never as copies of original files; a `tools/` helper applies the
  diff at install time, and the repo, the releases and this port never host
  mods. Off by default: with no mod folder the game is bit-identical to today
  (the P6.15 replays still match). The loader reads the SD card only, a mod
  cannot reach the save files, and every replaced id is logged.
  **Dependencies and risks:** needs a **stable id-to-path lookup in
  `port/cdfs`**: today the game goes through several entry points
  (`cdfs_read`, `cdfs_read_at` for map headers and `FILE_CACHE`,
  `cdfs_read_field_tile`, and the STR sector path), so the override must be in
  one resolve step they all use, or be added to each; P9.4 persistence for a
  mods on / off row; the field-tile reader and the file cache expect the size
  of the vanilla file, so a larger replacement needs a size check; large packs
  on the SD (FAT) cost read time. P8.4 `--check` extends to validate a mod
  against `files_*.json`.
  Open questions (owner): keep P8.2 as its own item or fold it in as "asset
  overrides"; how far to go with data tables before script hooks; plain
  folders or an archive (zip) on the SD.
  Done when (proposed): a documented `mods/` layout and manifest, one example
  mod per kind (a texture, a string, a table row) loads on the PC and the
  Switch build with US and EU, the loader reports conflicts, the default is
  bit-identical, and the modder docs (P6.23) describe it.
  Related: P8.2 to P8.4, P6.7, P6.8, P7.2, P6.23, P9.4.

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
- [ ] **P9.4 Persistence.** Load/save the option file under the owner
  runtime layout (PC directory / `sdmc:/switch/dw3/…`); values survive
  process exit.
  Done when: a toggled value remains after restarting the host or
  returning from hbmenu.
  **Status (2026-10-22): implemented, PC and Ryubing validated on both
  discs; waiting for the OLED check.** `port_options.cfg` (key=value,
  `version=1`) in each region runtime root (`runtime/<ver>/`,
  `sdmc:/switch/dw3/<ver>/`), loaded in `init_paths` before any flag is
  read, saved on every change on the Options screen, FAT-safe atomic write.
  Format, rules and rationale: [PORT_OPTIONS.md](PORT_OPTIONS.md#persistence).
  Evidence: unit tests `tools/tests/test_options_persist.py`; two-phase
  replay scenarios `us/eu-options-persist` on the PC build (identical
  captures on two runs) and in Ryubing (Widescreen OFF, Fast-forward 3x,
  Restart hotkey ON written by one process, read back by the next; the file
  on the emulated SD has the same content).
  Ready to close? needs OLED: toggle, HOME->close, relaunch from hbmenu.
  Blocks: P6.12, P7.6–P7.10 and a persisted P6.9 restart
  setting (see [Backlog](#backlog--future-goals)); P6.17 (volume) and P6.18
  (pacing) are rows added on top of it (`music_volume`, `sfx_volume`,
  `encounter_rate`, `text_speed`).
  **Partial OLED evidence (2026-10-22, EU, preview.30-rc4, owner):** with no
  `port_options.cfg` on the SD card the first change created it and every
  later change saved it (56 `dw3-options: saved` lines, no failure logged).
  Not covered: relaunch from hbmenu and reading the values back, and US.
  Evidence: `tmp/evidence/2026-10-22-preview30-rc4-oled/dw3.log`. Stays open.
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

- [ ] **P9.7 Port options reachable in game from the pause / field menu
  (future goal).** Dated 2026-10-22, owner request after the rc4 OLED test:
  today the Options screen opens only from the title, so a setting cannot
  be changed while playing (a volume in the field, the Encounter rate before a
  walk, Widescreen on a battle). Goal note only, nothing started. It needs:
  1. **Pausing game logic while the screen is open**, in the field and in
     battle (the title screen can run its own task; a field or a battle must
     stop its task list, timers, the random countdown and the sound script
     but keep the music, and resume with the same state and without a frame of
     input leaking into the game).
  2. **Drawing over the field and over a battle**: the options canvas is drawn
     by the native title overlay today; it must also draw in the field and
     fight overlays (and over the widescreen gutters), on PC and Switch, and
     deal with the draw-order and VRAM use of those modes.
  3. **The entry point**, one of: (a) a **hotkey** (a chord, like the P6.9
     restart chord), which is port-only, needs no text and no decomp change
     beyond a poll, but is invisible to the player; or (b) a **new entry in the
     game's own pause / field menu**, which is discoverable but needs a decomp
     patch (`PORT_HOST`, numbered patch) for the menu list, its cursor and
     layout, and **its name in the five EU languages** (EN, FR, DE, IT, ES; on
     US one string) with the game's own font and width limits. Pick (a) first
     if only one fits, (b) as the finished form.
  4. **Live application of every option**: Widescreen (the gutter and the
     present size change at runtime, and a battle in progress), Fast-forward,
     the volumes and the pacing rows already read their value when used, but
     Widescreen, Restart hotkey and the audio bus gains have to be re-applied
     safely in the middle of a frame; each row needs a documented answer.
  Done when: Port options opens and closes in the field and in battle on PC and
  Switch (US and EU), the game resumes exactly where it stopped, every row
  changes the game at once, and the choice of entry point is written down.
  Related: P6.17 / P6.18 rows, P9.2 (the title entry), P6.9 (the chord).

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

Gate: P10.2 (+ P10.3 once present is live). P10.4–P10.6 are best-effort.

## Backlog / future goals

Goal notes recorded early at the owner's request. They are not part of any
phase gate and nobody owns them yet. Their ids stay stable (other docs and
PRs cite them); an item moves back into its phase when someone takes it.
Statuses are in the [table](#status-at-a-glance).

**Dependencies not stated in the entries:**

- **P9.4 (option persistence) blocks** P6.12 (remap survives a restart),
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
- [ ] **P6.17 Music / SFX volume.** Separate music and sound-effect volume
  rows in Port options, persisted with P9.4. Done when both sliders work on
  US and EU.
  **Status (2026-10-22): implemented, PC and Ryubing checked on both discs;
  needs OLED check** (left unticked until the owner confirms by ear, steps in
  [P6.17_VOLUME.md](P6.17_VOLUME.md#oled-check-for-the-owner)). Rows
  "Music volume" and "SFX volume", 0..10, default 10 = the original mix,
  bit-identical. Music and effects are separated inside PSY-Z's SPU mixer
  (voice buses, gain per bus, `Larbaco/psyz` `dw3-spu-bus-volume`), because
  both hosts get one mixed stream; patch 0060 tells it which SEP tracks are
  music (the exclusive bit of the sound id). Movie (XA) audio follows Music
  volume. Left/Right step the volume, Confirm steps up and wraps. The
  options screen got taller (five rows). **Sound feedback (2026-10-22, owner
  request after rc4):** the Options screen plays the game's own cursor, choice
  and cancel sounds on the SFX bus, after the new gain is applied, so a change
  of SFX volume is heard at the new level ([P6.17_VOLUME.md](P6.17_VOLUME.md#sound-feedback-on-the-options-screen)). Measured per-bus attenuation and the
  default-unchanged proof are in the doc; not covered: Switch audio output
  (no hardware or capture), a hearing test, and every sound id of every bank.
  **Partial OLED evidence (2026-10-22, EU, preview.30-rc4, owner):** Music
  volume stepped 0 to 10 and SFX volume down to 2 on the Options screen, with
  the expected Q15 gains logged (music 0, 1, 2, 3, 4, 5, 7 = 0x0000, 0x0148,
  0x051f, 0x0b85, 0x147b, 0x2000, 0x3eb8; SFX 3 and 2 = 0x0b85, 0x051f) and
  each step saved. Not covered: a hearing test, US, relaunch persistence.
  Stays unticked.
  **Owner OLED (2026-10-23, preview.30-rc6 EU and rc7 US):** the owner reports
  that the sound volume options work. The rc7 US log shows Music volume
  stepped between 10 and 2 and SFX volume between 10 and 6 (final cfg: music 10,
  SFX 7), each step saved. That is the hearing test and the US check; **still open:** relaunch
  persistence (the rc7 log is a single run) and the in-game and movie steps of
  the checklist. Keep it unticked until the owner closes it. Note the new BGM
  bug in P4.3 (music stopped after one loop, fixed in rc8; music now stops after a won battle, open) when listening in game.
- [ ] **P6.18 Pacing options.** Encounter rate (1x, 0.5x, off), text speed
  and faster battle animations as Port options rows, off by default, saved
  with P9.4. Done when each toggle works on US and EU and is off by default.
  **Status (2026-10-22): encounter rate and text speed implemented, PC
  checked on both discs; faster battle animations deferred; needs OLED check**
  (left unticked until the owner confirms, steps in
  [P6.18_PACING.md](P6.18_PACING.md#oled-check-for-the-owner)). Rows
  "Encounter rate" (OFF / 0.5x / 1x / 1.5x / 2x; stored 2 / 1 / 0 / 3 / 4,
  1.5x and 2x added 2026-10-22 after the rc4 OLED test, nothing renumbered) and
  "Text speed" (Normal / Fast / Instant), default 1x and Normal, keys
  `encounter_rate` and `text_speed`.
  Encounter rate scales or skips the per-footstep countdown of the random
  battles (`FIELDSTG_countBattleSteps`, patch 0062); scripted, story and boss
  battles never go through it, and neither do map-forced areas. Text speed
  maps the dialogue type delay (patch 0063); Instant shows each page whole,
  as Cross does. The Options screen got a compact seven-row layout. Measured:
  a random battle after 102 footsteps at 1x and 203 at 0.5x, none in 3300
  frames when Off, a scripted battle still starts when Off; the first
  dialogue page types in 308 frames Normal, 131 Fast, 1 Instant; default
  captures are pixel-identical. **Faster battle animations are not done:** the
  battle has no single time knob (reasons in the doc), and R3 fast-forward
  (P6.4) already speeds a battle up. May still land as concrete P7.4 items.
  **Instant and scrolled lines (2026-10-22, patch 0068):** a text that
  scrolls (a third line) typed the new line at the fast delay; Instant now holds the
  full page 12 frames and shows the new line whole (reproduced in the opening's
  dialogue, 53/50/32 frames before). **The 103/73/67/127-frame lines of the rc4 and
  rc6 OLED logs were not scrolls and not the battle report** (2026-10-23): they are the
  area name banner that opens in the field after each battle (delay 5, `6n + 1`
  frames), timed as a dialogue page because the game reused a freed report
  window's address; port fix in `pacing.c` and scenarios `*-banner-instant`,
  details in [P6.18_PACING.md](P6.18_PACING.md#the-103-frame-pages-the-area-name-banner-rc4-and-rc6-oled-logs).
  **Open (owner's call):** the banner still types its two names at the game's own
  speed, about 2.3 s with the hero frozen after every battle; Text speed could end it
  0.3 to 0.7 s earlier, a shorter hold would need its own option.
  **Partial OLED evidence (2026-10-22, EU, preview.30-rc4, owner):** Encounter
  rate was cycled (1x, 0.5x, Off) and returned to 1x; the log shows 5 random
  battles, all at 1x, after 104, 175, 103, 128 and 93 footsteps (the owner
  counted the first four). Text speed Instant: 19 of 27 logged pages typed in
  1 to 2 frames. **Open:** 8 pages took 103 (5 times), 67 (2) and 127 (1)
  frames: **the area name banner, not text** (above, fixed in the port 2026-10-23); real
  scrolls are patch 0068, not yet rechecked on the OLED. Not covered: 0.5x and Off in
  play, Fast, US, relaunch persistence. Stays unticked.
  **Owner OLED (2026-10-23, US, preview.30-rc7):** Text speed Instant works as
  desired. The rc7 log has no `slow Instant page` and no `text scroll` line;
  its 26 area name banners are logged as `not a dialogue page (area name
  banner)`, which is the #203 fix working on hardware. The banner itself still
  types at the game's own speed (the open owner call above). **Still open:**
  Fast, 0.5x and Off in play, EU on rc7, a dialogue that scrolls, relaunch
  persistence. Stays unticked.
- [ ] **P6.19 Japanese disc support (future goal).** File map, executable
  differences and boot for the JP disc, next to US and EU. Goal note only
  (2026-10-15). Done when the JP disc boots to the field on PC and Switch.
- [ ] **P6.20 Port debug / cheat menu in the Options screen (future goal).**
  First noted 2026-10-15 (money, items, map warp, off by default); widened
  2026-10-24 on an owner request: "a cheats submenu inside the options, to
  make testing easier". Goal note only, **nothing started: no code, no
  patch**. The purpose is faster hardware testing (reach a battle, a map or a
  full bag without replaying hours of the game), not a player feature.
  AGENTS.md lists cheats as out of scope unless requested; this is the owner's
  explicit request, so the item is in scope under the constraints below.
  **What exists (cited, to be reused, not rebuilt):**
  - *The Options screen* ([PORT_OPTIONS.md](PORT_OPTIONS.md)): registry in
    `port/options/port_options.c` (`DW3_PORT_OPT_COUNT` = 8, the Battle cursor
    row of P7.9 is the latest), keys in `port_options.cfg`, saved on every
    change, atomic write, unknown keys kept (so an older build does not erase a
    newer build's cheat keys). The eight rows fill the window: 38 px rows on a
    42 px pitch, the last ends at y 482, 26 px above the message rule at y 508
    ([PORT_OPTIONS.md](PORT_OPTIONS.md#options-screen-2026-10-14), "Eight
    rows"), so a ninth row does not fit without changing the layout. Labels are
    pre-rendered lettering (`tools/generate_pc_title_lettering.py`).
  - *Dev hooks, today environment variables read by `PORT_HOST` patches*,
    PC only and silent when unset: `DW3_START_MODE` / `DW3_PROGRESS` (boot into
    a field or menu mode, patches `0020`, `0023`), `DW3_FORCE_ENCOUNTER` /
    `DW3_FORCE_PARTY` (a forced battle, `0024`, `0040`), `DW3_FORCE_WIN` (jump
    to the report, `0025`), `DW3_FORCE_ITEMS` (fills the bag with an item id
    range, `0070`), `DW3_FORCE_MENU` / `DW3_FORCE_STAGE` (`0026`, `0030`), and
    the P7.7 / P7.9 set (`DW3_FORCE_ENEMY_HP`, `FORM`, `TECH`, `HIT`, `STEAL`,
    `EXTRA_TECHS`; `0064`, `0067`). The Switch NRO has no environment; only a
    build-time `DW3_NX_TEST_ENV` bakes some in
    (`platforms/switch/source/boot_main.c`). A cheat menu is therefore the first
    way to set them at runtime on an ordinary NRO: the hooks would read the
    port's flag store instead of `getenv`.
  - *Options and aids that already do a cheat's job*, no new work needed:
    **Encounter rate OFF** is "no random encounters" (P6.18); **Text speed
    Instant** and **Fast-forward** (P6.4) are the time savers;
    `DW3_FORCE_CRASH` exercises the crash path
    ([P5.4_CRASH.md](P5.4_CRASH.md)); the nxlink HUD
    ([DEBUG_OVERLAY.md](DEBUG_OVERLAY.md)) shows the mode and the fps; the P6.15
    replay scenarios ([P6.15_REPLAY_TESTS.md](P6.15_REPLAY_TESTS.md)) drive the
    hooks on PC.
  **Candidate cheats** (cheap = a port flag or an existing hook; decomp =
  needs the field layout of `GameState` / party / battle structs from
  `vendor/dw3_decomp`, to be located first): give items (cheap, hook `0070`
  becomes a runtime action); instant battle win (cheap, hook `0025`); start a
  chosen battle (cheap, `0024` / `0040`); max money / bits (decomp, one field
  once found); infinite HP / MP and max stats (decomp, party and battle
  structs, re-applied every frame); unlock digivolutions (decomp, the
  requirement flags per form); walk speed (decomp, a field movement constant;
  it changes the walk length of the replays, so only when on); warp to map
  (decomp and the hardest: the hooks above act only at boot, an in-game warp
  must change mode safely, see P3.8 `DW3_FORCE_STAGE`; it would also help P5.5).
  **First slice:** (1) a Cheats sub-screen entered from the Options screen
  (layout decision first: a scrolling list, a second page or a ninth row), with
  the master switch off; (2) a runtime flag store in `port/options/` with its
  `cheat_*` keys; (3) three cheap entries wired to the existing hooks (give
  items, instant win, start a battle); (4) a log line per use. The decomp
  cheats follow one at a time after the field is located.
  **Constraints:** off by default, and a build or a config with every cheat
  off is bit-identical to today (the P6.15 replays must still match, plus one
  log-only replay per cheat). Decomp changes only as numbered `PORT_HOST`
  patches with a row in [PATCHES.md](PATCHES.md) (the next free number); the
  `getenv` hooks keep working for the replays. Persistence: **the options file,
  not the save.** The flat PS1 save image has no spare room (found in P7.9) and
  a cheat toggle must not edit P5.3 saves; `cheat_*` keys in
  `port_options.cfg` fit the current design, per region like every key. Toggles
  (infinite HP, walk speed) persist; one-shot actions (give items, max money)
  do not and run from the menu. A one-shot still changes the live game and the
  game's own save would then store it: say so on the screen and log it. No
  cheat may write a save file by itself.
  **Open questions (owner):** (a) *visibility*: nxlink / dev builds only (the
  code compiles out of the release `.nro`, like `DW3_NX_DEBUG_OVERLAY`) or in
  every build behind a master switch? Every preview ships an nxlink `.nro`
  anyway, so dev-only still serves hardware testing; (b) the Options screen
  opens only from the title today, so give-item or instant-win inside a battle
  needs P9.7 (Options reachable in game); the first slice works from the title
  only; (c) English-only labels for a test menu, or the five EU languages;
  (d) whether to flag a save as touched by a cheat (a log line only, never a
  change to the save).
  Done when: the sub-screen works on the PC and the Switch build with US and
  EU, every cheat in the slice works and is logged, the default is
  bit-identical to a build without it, the visibility choice is recorded, and
  PORT_OPTIONS.md and PATCHES.md describe it.
  Related: P9.7, P9.4, P6.15, P6.18, P7.9, P5.5, P3.8.
- [ ] **P6.21 60 fps interpolation (far future).** Interpolate camera and
  field movement in 30 fps sections. After P6.10. Goal note only
  (2026-10-15).
  Done when (proposed): camera and field movement in 30 fps sections
  present at 60 fps on PC and Switch with the US and EU discs, and the
  original 30 fps stays selectable.
- [ ] **P6.22 Turbo speed in scripted scenes (future goal).** Dated
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
  **Already known (cited, not repeated as new findings):**
  [P6.4_FAST_FORWARD.md](P6.4_FAST_FORWARD.md) says the Switch host skips
  only presents, "the CPU rasterizer still draws every frame", so the real
  speed is "up to" the setting. The same doc records the field/event
  readings of earlier OLED rounds (about 70-80 fps in the field against
  about 180 fps in battle at 2x on preview.25; about 90 FPS in the Inn and
  70 FPS in the city and field against 130 in battle on EU preview.26,
  HUD counters, not benchmarks) and defers display-buffer raster skipping
  because the field encounter transition copies VRAM with `MoveImage`
  (`FIELDSTG_playBattleTransition`). P6.10 above (field and menus on the
  GPU path) already carries the "field turbo has no headroom" reasoning, and
  [P5.2_BATTLE_GPU.md](P5.2_BATTLE_GPU.md) shows why battle is faster
  (the mesh path takes the triangles off the CPU). So this item does not
  restart those: it adds the **scripted-scene** case and a measure-first
  step that decides between them. [P6.18_PACING.md](P6.18_PACING.md) and
  [P6.15_REPLAY_TESTS.md](P6.15_REPLAY_TESTS.md) already show that
  dialogue typing is counted in game frames (`text page typed in N frame(s)`),
  so text waits should scale with turbo; that is a reading of the docs, not
  a test of the opening.
  **First measurement (2026-10-23, rc7 log of the owner's New Game run, US
  OLED handheld, 4998 windows of 60 game frames at turbo 4x, 73 minutes):**
  the field and scripted scenes run at **68 game frames per second** (p10 to
  p90 63 to 74, best window 85) against 240 asked for, while the one battle in
  the same run reached **190** at the same setting. `present_us` is about
  42 ms per window (15 presents, 2.8 ms each, under 5 % of the window), so
  the present is not the bound: the field spends about **13.7 ms of CPU per
  game frame** (battle about 4.2 ms) and that caps it near 70 fps. This
  favours candidate (1) below and weakens (3). The log cannot split the
  opening from the rest of the session, nor logic from raster; (a) and (b)
  below still apply.
  **Candidates, none confirmed:** (1) CPU cost per emulated frame (game
  logic plus the software rasterizer; scripted scenes draw many sprites,
  text and a full map on the soft path, and the map still fills in one
  tile per frame per [STATIC_SD_LOAD.md](STATIC_SD_LOAD.md)); (2) the
  turbo gate: the port speeds up only the field and battle mode files
  (`port/options/turbo.c`, US 0x158/0x159, EU 0x166/0x167), so a scene that
  runs in another mode or overlay would stay near 1x; the opening goes
  through several overlays (P6.15: Kusanagi, name entry, Login Room, lobby);
  (3) present/vsync pacing in `Psyz_VideoVSync` (`nx_gpu.c`) and the
  per-frame fixed costs that turbo repeats (audio pump, input poll,
  mesh/batch reset); (4) CD or stream reads, which look unlikely (no disc
  seek on the port, map preload is one `fread`) but are unmeasured in
  these scenes; (5) a wait that runs in real time instead of in game
  frames (not audited); (6) audio sync (the sequencer ticks once per game
  frame, so this should follow turbo, but the Switch audio buffer is not
  checked at 4x); (7) the movie inside the opening (about frames
  14400-14900) is always 1x by design and is not part of this item.
  **First step, measure before optimizing:** (a) on the OLED, play the
  opening with turbo at 4x on a `--nxlink` build and read the existing
  ~1 Hz lines per scene: `frame dt us=` (60 pumps, so the real rate),
  `mesh dbg:` (`flush_us`, `upload_us`, `present_us`, `frames_blit`,
  `soft`), the `mem dbg:` line and the `dw3: mode=` / `overlay restore`
  lines to tell which mode each scene is in (the HUD of
  [DEBUG_OVERLAY.md](DEBUG_OVERLAY.md) shows `fps` and `mode file=`);
  (b) on PC replay the scripted opening with turbo forced
  (`DW3_TURBO=4`, `tools/replay_tests.sh`, the `*-newgame-field`
  recordings of P6.15; the replay is keyed by game frame, so captures do
  not change) and time the game frames per scene against the same stretch
  at 1x, with a per-frame split (game logic, raster, present) added if the
  existing counters are not enough; (c) compare with the battle numbers to
  see whether the limit is CPU raster, a pacing wait or the mode gate.
  Only then pick the optimization (for example the deferred raster
  skipping in the P6.4 doc, P6.10, or a fix to the gate or a wait). The
  rule that the port does not overclock (AGENTS.md, directive 7) stays:
  the work is on the present path, the field-tile pace and the raster.
  Done when (proposed): a short doc records the measured bottleneck per
  scene, and with turbo at 4x the New Game opening scenes, a long field
  walk and a text-heavy scene reach the target rate (or the doc shows why
  not) on the Switch OLED with the US and EU discs, with no change in game
  state (the P6.15 replays still match) and a clean return to 1x.
  **New evidence (2026-10-23, preview.30-rc8, owner, Forest inn, OLED, US,
  no log; HUD reading, not a benchmark):** in the Forest inn the FPS reads
  120+ when walking to the bottom corner of the room and drops to about 80 at
  the top; the owner expects the same in other places. Same room, same game
  logic, different camera position: so the field cost depends on what is on
  screen (camera and visible geometry, so raster and draw cost), not only on
  a fixed per-frame cost. That fits the rc7-log numbers (the field at turbo
  4x runs at about 68 game fps, about 13.7 ms of CPU per frame, with
  `present_us` under 5 % of the window; table in
  [P6.4](P6.4_FAST_FORWARD.md#new-game-opening-measured-from-the-rc7-log-2026-10-23))
  and points at candidate (1) and the P6.10 reasoning rather than the gate or
  a pacing wait, and it adds that the per-frame cost is not fixed but moves
  with the camera. The rc8 reading itself has no counters or log.
  **Suggested A/B for the measurement step:** use the same room at both
  positions (Forest inn, bottom corner against top), same turbo setting and
  disc, standing still for about 10 s at each so the `frame dt us=`, `mesh
  dbg:` (`flush_us`, `upload_us`, `present_us`, `frames_blit`, `soft`) and
  `mem dbg:` lines cover each position; only the camera changes, so any
  difference is the on-screen cost. Repeat in one more room (city or field
  map) to check the owner's expectation. See
  [P6.4](P6.4_FAST_FORWARD.md#field-fps-depends-on-what-is-on-screen-rc8-oled).
  Related: P6.4, P6.10, P6.15, P6.18, P5.2.
- [ ] **P6.23 Documentation (future goal).** Dated 2026-10-24, owner request:
  "documentation". Goal note only, nothing written yet. **Gaps found by a
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
- [ ] **P6.24 One build for US and EU (future goal).** Dated 2026-10-24, owner
  request: "Build única pra US e EU". One `.nro`, and one PC binary, that runs
  either disc, instead of the separate US and EU builds (`--eu`, two `.nro`
  files, EU attached to releases by hand). Goal note only, **nothing started,
  no code**. Survey, sizes and the options in detail:
  [SINGLE_BUILD.md](SINGLE_BUILD.md).
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
  which `runtime_extract.c` already checks); saves and options stay per region
  (the save formats differ), so no player data moves.
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
  Open questions (owner): auto-detect with one region present and a picker
  only with both, or always ask; remember the last choice; the picker's look
  and languages; keep the separate `--eu` builds for development only or
  also in releases for a while; is a 12 MB `.nro` fine; the PC too; switching
  region without quitting (proposed: out of scope).
  Done when (proposed): one `.nro` and one `dw3-pc` from one configure, built
  in CI; with only US or only EU data it boots that disc, with both it does
  what the owner chose; the US and EU P6.15 scenarios pass on it with the
  existing references; the crash report names the region and
  `tools/symbolize_crash.py` resolves both from the one `.elf`; the release
  ships one `.nro` (plus nxlink); checked on the OLED with each disc and with
  both on the card.
  Related: P6.2, P6.15, P6.19, P5.4, P6.5, P3 EU build
  ([P3_EU_BUILD.md](P3_EU_BUILD.md)).

### Phase 7 goal notes

- [ ] **P7.6 Progression options (future goal).** Port options for EXP
  and DV EXP rate (1x default, Harmony uses 1.3x / 1.2x), uncapped DV EXP
  and HP/MP restore on level up. Credit markisha64 / rsigrist. Done when
  each option works on US and EU and is off by default.
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
