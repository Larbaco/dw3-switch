# Digimon World 3: Nintendo Switch port

A native Nintendo Switch homebrew port of Digimon World 3 / Digimon World 2003.
The game logic is the C from the matching PlayStation decompilation. Console
APIs go through [PSY-Z](https://github.com/Xeeynamo/psyz), a Psy-Q compatible
shim. This is not an emulator wrapper and not a remake.

PC is the first host. A thin libnx host then runs the same image on the
console.

## What this is not

- An emulator wrapper. The `.nro` does not contain a PS1 core.
- A remake with a new engine as the game source.
- A place to download ROMs, disc images, BIOS, or extracted game trees.

## Status

Checkboxes: [docs/ROADMAP.md](docs/ROADMAP.md). Phase 3 gate:
[docs/P3_GATE.md](docs/P3_GATE.md). The latest tagged preview is
preview.21, which includes the STR file-bridge race fix (#111). The OLED
confirmed about 15 fps and correct XA audio up to frame 1110; the rest of
the opening and image quality remain unmeasured on hardware. See
[movie pacing diagnostics](docs/P3.7_MOVIE_PACING_DIAGNOSTICS.md).

| Phase | Summary |
|---|---|
| 0 to 2 | Done. Toolchains, disc to runtime, US title boot from decomp C ([P2.8](docs/P2.8_TITLE_BOOT.md)). |
| 3 | Gate met for US. Field, battle, menus, card game, movies, SEP, and the stage sweep run on PC. Start to Genji is documented, with aids. [P3.9](docs/P3.9_REFERENCE.md) is done. Still open: EU playthrough, a pad-driven boss KO, [PC GPU and battle polish](docs/P3_GPU_GLITCHES.md), and the `WSTAG460` stub. |
| 4 | Gate met on hardware for title, field, and battle. P4.2, P4.4, P4.5, and P4.7 are ticked. Still open: P4.3 underrun count and the P4.6 overlay sweep. |
| 5 to 10 | Phase 5 is open. P5.2 is the 16.7 ms bar (soft battle default ON after all-black mesh dumps; field mesh gated). P5.3 flat `saveN.bin` + deferred flush landed; OLED MC1→MC2 save PASS and STGMCARD 100-frame bar owner-confirmed on load ([P5.3_FLAT_SAVE](docs/P5.3_FLAT_SAVE.md)). HOME/`appletMainLoop` quit flush landed; OLED applet exit/flush was observed on preview.20; fresh-save/relaunch reload validation remains open. P6.5 packaging is done and does not start Phase 6. P6.9 has PC/Xvfb proof of in-process title restart after save flush; Switch hardware and post-restart Continue validation remain open ([P6.9_RESTART](docs/P6.9_RESTART.md)). P9.2 entry/shell behavior is validated on PC and OLED; the 2026-10-13 title-row visual pass has PC/Ryubing captures, while OLED handheld/docked label legibility still awaits owner confirmation ([PORT_OPTIONS](docs/PORT_OPTIONS.md)). The PC-only Digivice menu uses a native 960×720 RGBA overlay; opt-in 3× starts the SDL3 GPU window at 1280×720, presenting the 4:3 image with 160-pixel side bars. The US SDL3 GPU path resamples the original runtime logo to a 720×326 texture with Lanczos2 and a restrained local-contrast pass after its intro animation. A stable three-row Xvfb capture and the P9.2 shell return are recorded outside Git under `tmp/evidence/pr121-restored/`. New Game / Options shell notes are in [PORT_OPTIONS](docs/PORT_OPTIONS.md), [TITLE_MENU_DESIGN](docs/TITLE_MENU_DESIGN.md), and [PC_TITLE_LOGO_RESAMPLING](docs/PC_TITLE_LOGO_RESAMPLING.md). Automated Load Game selection remains open. The Switch draws the same menu and logo over its GLES present (Ryujinx/OLED validation pending). SDL3 GL and EU retain their existing logo path. P9.3's current registry includes the opt-in restart hotkey, while persistence P9.4 remains open. Other controller configurations and gameplay effects remain open. Phases 6 through 10 otherwise have not started. P7.1, P8.1, P9.1, and P10.1 are early notes. See [SWITCH_FEATURES](docs/SWITCH_FEATURES.md). |

Layering: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md). OLED network notes:
[docs/SWITCH_OLED_NETWORK.md](docs/SWITCH_OLED_NETWORK.md) (FTP and preview 7
NXLink transfer/launch/log stream verified).

### On an OLED, updated 2026-10-13 (preview.21 test: 2026-10-13)

Title, field, and battle run. The main menu picture looks right. The opening
video now runs at about 15 fps on preview.21 with correct XA audio up to frame
1110, where the owner pressed Start. Image quality and the final part of the
movie are not validated on hardware.

After #114 merged, the owner also reported that GPU battle rendering and the
STR video changes both work well in-game, though neither is fully there yet.
Specific remaining issues were not reported. This confirms in-game progress;
it does not close the P5.2 60 Hz/performance gate or the movie quality and
audio/synchronization questions.

Movie audio plays. STR files are restored from the Mode2 BIN in the parent
of `us/`.

The left stick walks on a handheld Joy-Con and on a Pro Controller. Rumble
is not implemented. That item is P10.4.

Runtime saves are flat `save0.bin` / `save1.bin` under `us/memcard/`
(P5.3). Older `memcardN.mcr` migrate once and become `.mcr.bak`. OLED
preview.14: load MC1 → spend → save MC2 slot2 PASS (money=26); a later
overwrite to MC2 slot1 saved money=2. Emulator `.mcr` interchange stays
P9.6 (external tool), not in-game. Data-frame byte `0x7F` is game data,
not a checksum — never put XOR on it.

If `sdmc:/switch/dw3/us/` is missing, US boot extracts the runtime from
that BIN. `v0.6.5-preview.5` is the hardware test of the copy progress
bar, the left stick, and this extract.

The CPU and GPU stay at the rate the console set. `v0.6.5-preview.6`
raised the CPU to 1785 MHz. That is reverted. Do not put that file on
the Switch.

`v0.6.5-preview.7` reached the field and a battle on the OLED. Across 156
samples of 60 frames the mean was about 59 FPS, with some 60-frame windows
near 25 ms/frame. PSY-Z logged `KernelInit: cannot create the kernel
synchronisation objects` once and continued. The load design that follows
from that session is [docs/STATIC_SD_LOAD.md](docs/STATIC_SD_LOAD.md):
the map file is read once per area; movies keep their sector pace.

STGMCARD load/save panel on host uses 100 frames (option B, #84). Owner
confirmed the load bar feels OK; save uses the same constants.

Still open on hardware: a P4.3 underrun count,
the full P4.6 overlay sweep, P5.2 battle 60 Hz (soft path for now), the
kernel sync-object error, and P5.3 fresh-save/HOME/relaunch reload validation.

On preview.20, the owner confirmed the Port options title entry opens with
both stubs OFF and closes back to the middle row: **Plus** opens the title
choices, D-pad selects, **B** confirms (PS1 Cross), **X** closes (Triangle).
This validates P9.2 on the tested OLED handheld controller configuration;
other controller models and option persistence remain unvalidated. See
[RELEASE_PREVIEW20](docs/RELEASE_PREVIEW20.md) for the post-release checkpoint.

**PC today.** Yes, with your own US disc packed to `runtime/us`. Expect
rough visuals in some scenes.

## Upstream

| Project | Role |
|---|---|
| [ReGame-Labs/dw3_decomp](https://github.com/ReGame-Labs/dw3_decomp) | Matching PS1 decomp. Game logic source (`vendor/dw3_decomp`). |
| [Xeeynamo/psyz](https://github.com/Xeeynamo/psyz) | Psy-Q compatible shim (`vendor/psyz`). Switch fork pin: [PSYZ.md](docs/PSYZ.md). |

This repository owns the hosts, the port glue, and the disc to runtime
tools. Prefer patches in `port/` and `platforms/` over edits inside the
decomp submodule, unless the fix belongs upstream.

## Acknowledgments

This port stands on other people's work. In particular:

### Foundations

| Project | Thanks for |
|---|---|
| [ReGame-Labs/dw3_decomp](https://github.com/ReGame-Labs/dw3_decomp) | The matching Digimon World 3 / 2003 decompilation. The game logic this port builds on. |
| [Xeeynamo/psyz](https://github.com/Xeeynamo/psyz) | PSY-Z, the Psy-Q compatible shim. PC uses the SDL3 backends. Switch uses the libnx backend in `platforms/switch/psyz_backend/`. |
| [libsdl-org/SDL](https://github.com/libsdl-org/SDL) | SDL3, used through PSY-Z's desktop backends. |

### Ideas and prior art

| Project | Thanks for |
|---|---|
| [Xive080/D-W-3-Recomp](https://github.com/Xive080/D-W-3-Recomp) (OpenWorld2003) | A public PC recomp of the same game. Useful as a reference for CD and filesystem pitfalls, and for launcher and mod catalog shape. Not a dependency of this port. |
| [markisha64/ddw3](https://github.com/markisha64/ddw3) | Parallel Digimon World 2003 decomp (Rust/asm). Reference for Digimon profiles, evolution labeling, TIM/map tooling, and language tables. Not a substitute for `vendor/dw3_decomp`. |
| [dorijantomic/dw2003-play-code](https://github.com/dorijantomic/dw2003-play-code) | Browser recreation built from ddw3 with ReGame cross-checks. Useful as a research oracle (battle formulas, GTE, SPU exclusive track, save-block size, field tick). Not a runtime dependency. |
| [rsigristc/dw2003-qol-patch](https://github.com/rsigristc/dw2003-qol-patch) (**Harmony**, with Flawe, guiomatos, markisha64, EmeraldPhoenix, and others) | EU-only (`SLES-03936`) BPS QoL suite that Phase 7 tracks for own-disc data (P7.2) and native reimplementation (P7.4). See [docs/COMMUNITY_QOL.md](docs/COMMUNITY_QOL.md). Binary patches do not load into the `.nro`. |

### Platform and tooling

| Project | Thanks for |
|---|---|
| [devkitPro](https://devkitpro.org/) / [libnx](https://github.com/switchbrew/libnx) | Switch homebrew toolchain and APIs for the `.nro` host. |
| The broader matching-decompilation and recomp communities | Patterns for shims, PC-first bring-up, and honest progress reporting. |

*Digimon* and *Digimon World* are trademarks of their respective owners. This is
an unofficial fan project and is not affiliated with Bandai Namco or Nintendo.

## Own disc only

You need a legally obtained copy of Digimon World 3 (USA, `SLUS-01436`) or
Digimon World 2003 (Europe, `SLES-03936`). Do not commit disc images, BIOS,
or extracted trees. `disks/`, `runtime/`, and disc-image extensions are
gitignored. Releases do not ship them.

## Quick start

### Clone

```bash
git clone <this-repo-url> dw3-switch
cd dw3-switch
git submodule update --init vendor/dw3_decomp vendor/psyz
git -C vendor/psyz submodule update --init nugget external/SDL
```

Both the PC host and Switch game image need `nugget/bsdec` for movies,
as well as SDL. Do not recursively initialize nugget: its
`third_party/xmake-psx` is a PS1 matching toolchain, unused by these hosts,
and fetching it can fail with Codeberg HTTP 429/503. CI uses the explicit
paths above; see [docs/CI_SUBMODULES.md](docs/CI_SUBMODULES.md).

### PC build

CMake runs `tools/apply_patches.sh` into `build/decomp-src`. Host notes:
[platforms/pc/README.md](platforms/pc/README.md). Pulse or PipeWire is
optional if you want audible movies.

```bash
cmake -S platforms/pc -B build/pc -G Ninja -DPSYZ_RENDERER=sdl3_gpu
cmake --build build/pc -j"$(nproc)"
./build/pc/dw3-pc
```

The default runtime directory is `runtime/us`. `--runtime` selects another
one. Enter is Start. Arrow keys are the D-pad. Close the window to quit.

The title offers Start / Port options / Continue. Use Up/Down to select
Port options and X (Cross) to open the shell; S (Triangle) closes it and
returns to the middle row. Backspace (Select) also opens it. Inside the
shell, Up/Down selects a stub and X toggles it. Private-display PC keyboard
validation at 60 Hz covered navigation, open/close, Start, and Continue.
Stub toggles have no gameplay effect and do not persist to disk. P9.2 is
complete with the later OLED entry/return checkpoint; P9.3/P9.4 and the
Phase 9 gate remain open. Details:
[docs/PORT_OPTIONS.md](docs/PORT_OPTIONS.md).

The 2026-10-13 title-row visual pass has Ryubing-on-Xvfb captures of all
three selections and shell open/close. The updated label's legibility on
OLED handheld and docked displays still needs owner confirmation.

EU is a separate build tree and needs `runtime/eu`:
[docs/P3_EU_BUILD.md](docs/P3_EU_BUILD.md).

### Disc to runtime

[docs/DISC_SETUP.md](docs/DISC_SETUP.md) and
[docs/RUNTIME_LAYOUT.md](docs/RUNTIME_LAYOUT.md).

```bash
python3 tools/pack_runtime.py --version us --disks disks/us --out runtime/us
```

If `runtime/us` already exists and is not empty, add `--force`.

On the Switch the same relative tree is `sdmc:/switch/dw3/us/`. Copy
`dw3-switch.nro` to `sdmc:/switch/dw3-switch.nro` and launch it from
hbmenu under Atmosphere. hbmenu shows the NACP display version for that
install (e.g. `0.6.5-p.18` for tag `v0.6.5-preview.18` — the field is only
15 characters). See [platforms/switch/README.md](platforms/switch/README.md).
At the title, Plus shows the choices, D-pad selects Port options, B opens
it (Cross), and X closes it (Triangle), returning to the middle row.

Put the Mode2 BIN and CUE next to `us/`, not inside it. `boot_tim.bin` is
not a disc image.

```text
sdmc:/switch/dw3/Digimon World 3.cue
sdmc:/switch/dw3/Digimon World 3.bin
sdmc:/switch/dw3/us/
```

US boot creates an empty `memcard/` when that root exists. Saves are
`save0.bin` / `save1.bin` (created on first in-game write). Legacy
`memcardN.mcr` migrate once to flat and become `.mcr.bak`. A packed
`.STR` that is still 2048 bytes per sector is rewritten from the parent
BIN on boot. The screen stays dark blue and draws a progress bar during
a long copy. EU does not extract. Pack `sdmc:/switch/dw3/eu/` yourself,
and create `eu/memcard/` before the first EU launch.

### Tooling self-check

No disc required.

```bash
python3 -m pytest tools/tests -q
```

### Switch `.nro`

GitHub Actions builds both Switch binaries in `devkitpro/devkita64`.

| Job | Output |
|---|---|
| `switch` | `make -C platforms/switch`. The `host_hello` sample at `platforms/switch/dw3-switch.nro`. No game loop. |
| `switch-game` | `tools/build_switch_game.sh`. The game image. `source/boot_main.c` calls `dw3_game_main`. Artifact `dw3-switch-game-nro` at `build/switch/dw3-switch.nro`. NACP/hbmenu version is the short `GITHUB_SHA`. This job does not open a Release. |

A Release is
[`.github/workflows/switch-release.yml`](.github/workflows/switch-release.yml),
on `workflow_dispatch` or a `v*` tag. It is a prerelease. Assets are
`dw3-switch.nro`, `dw3-switch-nxlink.nro`, and
[`platforms/switch/release/README.txt`](platforms/switch/release/README.txt).
The release tag becomes the NACP version (compressed for hbmenu) and the
`dw3: version=` boot line. No disc data.

The current standard preview is
[v0.6.5-preview.21](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.21),
cut from `b597c67` after #111 (STR file-bridge race fix) and #112 (docs). It
keeps the title Port options shell (#100) and the empty `StGetNext` scan
cache (#106). On the OLED the opening movie held about 15 fps (preview.20:
6.75 fps) with correct XA audio up to frame 1110. See
[preview.21 checkpoint](docs/RELEASE_PREVIEW21.md) and
[movie diagnostics](docs/P3.7_MOVIE_PACING_DIAGNOSTICS.md). Its switches are
stubs with no gameplay effects or persistence. GPU diagnostics stay
disabled and software battle stays the default. Preview.19 was a separate
PR #89 diagnostic cut, not the standard playable baseline.

An earlier tagged preview release is
[v0.6.5-preview.10](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.10).
Battle mesh is gated to FIGHTSTG (PR #71); field stays on the software
blit. PR #72 adds sampled `mesh dbg:` lines on the nxlink log. After
preview.10 all-black mesh FBO dumps, `main` defaults battle to software
`RasterTriangle` under that gate (`DW3_NX_MESH_SOFT_BATTLE=ON`; see
[P5.2_BATTLE_GPU.md](docs/P5.2_BATTLE_GPU.md)). Flat saves, debug HUD,
and the STGMCARD 100-frame bar also landed on `main` after that tag.
It does not change the CPU or GPU clock.
[v0.6.5-preview.9](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.9)
enabled mesh for every mode and broke the field picture on OLED. Do not
install that file for field play.
[v0.6.5-preview.8](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.8)
holds one field map in RAM so later tiles copy from that buffer. An OLED
log from that build: field about 60 Hz, battle Fight-menu idle about 46 Hz.
[v0.6.5-preview.7](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.7)
uploads only the sampled VRAM rectangle and loads one field tile per frame.
An OLED log from that build averaged about 59 FPS.
[v0.6.5-preview.6](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.6)
raised the CPU to 1785 MHz. Do not install that file.
[v0.6.5-preview.5](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.5)
is the hardware test of the progress bar, the left stick, and the US
BIN extract. The picture prerelease is
[v0.6.5-preview.2](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.2)
(`8273a91`, 8935621 bytes): display-mode title width and the full-height
movie.
[v0.6.5-preview.1](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.1)
is the earlier 8927429-byte file. It crops the title and freezes the movie.

A local game build is the same script and needs devkitPro. On a Windows
mount, set `DW3_DK_TMPBUILD=/tmp/dw3-dk-build`. Mount notes:
[tools/docker/README.md](tools/docker/README.md) and
[platforms/switch/README.md](platforms/switch/README.md).

```bash
bash tools/build_switch_game.sh
# build/switch/dw3-switch.nro
bash tools/build_switch_game.sh --nxlink
# build/switch-nxlink/dw3-switch-nxlink.nro
```

`--nxlink` is the development variant included in each preview release. Launch steps
are in [platforms/switch/README.md](platforms/switch/README.md).

Backend: [platforms/switch/psyz_backend/](platforms/switch/psyz_backend/).
Boot evidence: [docs/SWITCH_BACKEND.md](docs/SWITCH_BACKEND.md).

## Repository layout

```
dw3-switch/
├── docs/                      # roadmap, gates, phase evidence
├── platforms/pc/              # PSY-Z + SDL3 → dw3-pc
├── platforms/switch/          # Makefile sample + CMake game target, psyz_backend/
├── port/                      # overlays, cdfs, memcard, movies, patches
├── tools/                     # pack/extract, stage_sweep, compare_harness, …
├── vendor/dw3_decomp/         # upstream decomp (submodule)
└── vendor/psyz/               # PSY-Z (submodule)
```

## Documentation

| Doc | When to open it |
|---|---|
| [docs/ROADMAP.md](docs/ROADMAP.md) | Phases, gates, what is next |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Host, PSY-Z, and game layering |
| [docs/P3_GATE.md](docs/P3_GATE.md) | Phase 3 gate, US start to first boss. Met. |
| [docs/P3.9_REFERENCE.md](docs/P3.9_REFERENCE.md) | DuckStation vs PC screenshot harness. Done. |
| [docs/P3_EU_BUILD.md](docs/P3_EU_BUILD.md) | EU `-DDW3_VERSION=eu` scaffold |
| [docs/P3_GPU_GLITCHES.md](docs/P3_GPU_GLITCHES.md) | PC present and battle mesh follow-ups |
| [docs/SURVEY.md](docs/SURVEY.md) | Decomp facts that drive the port |
| [docs/PATCHES.md](docs/PATCHES.md) | Decomp patch policy and series |
| [docs/DISC_SETUP.md](docs/DISC_SETUP.md) / [RUNTIME_LAYOUT](docs/RUNTIME_LAYOUT.md) | Owner disc to runtime tree |
| [docs/PSYZ.md](docs/PSYZ.md) | PSY-Z pin and renderer notes |
| [docs/SWITCH_BACKEND.md](docs/SWITCH_BACKEND.md) | Switch backend, CMake game build, OLED title, field, and battle |
| [docs/SWITCH_FEATURES.md](docs/SWITCH_FEATURES.md) | Residual Switch UX inventory (Phase 10) |
| [docs/P4.2_VIDEO.md](docs/P4.2_VIDEO.md) / [P4.3_AUDIO.md](docs/P4.3_AUDIO.md) | Switch present is ticked. The audren underrun check is still open. |
| [docs/P4.4_INPUT_STATUS.md](docs/P4.4_INPUT_STATUS.md) / [P4.5](docs/P4.5_STATUS.md) / [P4.6](docs/P4.6_MODULES_STATUS.md) | Left stick and formal CDFS suite are ticked. The P4.6 overlay sweep is still open. |
| [docs/P2.8_TITLE_BOOT.md](docs/P2.8_TITLE_BOOT.md) | PC title screen evidence |
| [docs/P3.2_FIELD.md](docs/P3.2_FIELD.md) … [P3.8](docs/P3.8_STAGES.md) | Phase 3 subsystem notes |
| [docs/P5.2_BATTLE_GPU.md](docs/P5.2_BATTLE_GPU.md) / [P5.3_FLAT_SAVE](docs/P5.3_FLAT_SAVE.md) | Battle mesh / soft path; flat `saveN.bin` |
| [docs/DEBUG_OVERLAY.md](docs/DEBUG_OVERLAY.md) / [NXLINK_DEBUG](docs/NXLINK_DEBUG.md) | On-screen HUD + nxlink mesh/`frame dt` lines |
| [docs/COMMUNITY_QOL.md](docs/COMMUNITY_QOL.md) / [TEXTURE_PACKS](docs/TEXTURE_PACKS.md) / [PORT_OPTIONS](docs/PORT_OPTIONS.md) | Later optional features |

Hosts: [platforms/pc/README.md](platforms/pc/README.md),
[platforms/switch/README.md](platforms/switch/README.md).
Port glue: [port/README.md](port/README.md).
Compare harness: [tools/compare_harness/README.md](tools/compare_harness/README.md).

## Requirements

- PC: CMake, Ninja, and the PSY-Z submodule. SDL3 is vendored under `vendor/psyz/external/SDL`. See [platforms/pc/README.md](platforms/pc/README.md).
- Switch game `.nro`: GitHub Actions, image `devkitpro/devkita64`, job `switch-game`. A local build needs that toolchain. See [platforms/switch/README.md](platforms/switch/README.md).
- Your own disc, for `runtime/<ver>/` on PC or `sdmc:/switch/dw3/<ver>/` on the card.

## License

Port scaffolding and original glue code: see [LICENSE](LICENSE).

Game code under `vendor/dw3_decomp` remains under that project's license.
Original Digimon World 3 / 2003 assets remain property of their respective
owners and are not redistributed here.
