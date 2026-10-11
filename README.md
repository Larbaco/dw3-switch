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
[docs/P3_GATE.md](docs/P3_GATE.md). `main` includes `e85870c`, the P4.4 tick.

| Phase | Summary |
|---|---|
| 0 to 2 | Done. Toolchains, disc to runtime, US title boot from decomp C ([P2.8](docs/P2.8_TITLE_BOOT.md)). |
| 3 | Gate met for US. Field, battle, menus, card game, movies, SEP, and the stage sweep run on PC. Start to Genji is documented, with aids. [P3.9](docs/P3.9_REFERENCE.md) is done. Still open: EU playthrough, a pad-driven boss KO, [PC GPU and battle polish](docs/P3_GPU_GLITCHES.md), and the `WSTAG460` stub. |
| 4 | Gate met on hardware for title, field, and battle. P4.2, P4.4, and P4.7 are ticked. Still open: P4.3 underrun count, the P4.5 CDFS suite, and the P4.6 overlay sweep. |
| 5 to 10 | Phase 5 is open. P5.2 is the 16.7 ms bar. The owner says the game feels slow. P5.3 exit flush is open. P6.5 packaging is done and does not start Phase 6. Phases 6 through 10 otherwise have not started. P7.1, P8.1, P9.1, and P10.1 are early notes. See [SWITCH_FEATURES](docs/SWITCH_FEATURES.md). |

Layering: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

### On an OLED, 2026-10-11

Title, field, and battle run. The main menu picture looks right. The opening
video still has visual errors.

Movie audio plays. STR files are restored from the Mode2 BIN in the parent
of `us/`.

The left stick walks on a handheld Joy-Con and on a Pro Controller. Rumble
is not implemented. That item is P10.4.

A DuckStation memory card loads. A save this game creates in slot 2 loads
on a build that includes `994c42a`. Data-frame byte `0x7F` is game data,
not a checksum. That commit is on `main`, after the `v0.6.5-preview.5`
tag. The tagged preview.5 file does not contain it. On that file, a slot 2
save the game had just created returned to the launcher.

If `sdmc:/switch/dw3/us/` is missing, US boot extracts the runtime from
that BIN. `v0.6.5-preview.5` is the hardware test of the copy progress
bar, the left stick, and this extract.

The CPU and GPU stay at the rate the console set. `v0.6.5-preview.6`
raised the CPU to 1785 MHz. That is reverted. Do not put that file on
the Switch.

Still open on hardware: a P4.3 underrun count, the formal P4.5 CDFS suite,
the full P4.6 overlay sweep, P5.2, and the P5.3 flush on exit.

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
| Digimon World 2003 community QoL authors (Flawe, guiomatos, markisha64 / Marko Grizelj, EmeraldPhoenix, and others) | Patch and UX ideas tracked for optional later compatibility. See [docs/COMMUNITY_QOL.md](docs/COMMUNITY_QOL.md). |

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
git -C vendor/psyz submodule update --init external/SDL
```

The PC host needs those two checkouts. The Switch game image also needs
`nugget`, without a recursive init. A recursive init of nugget fetches
`third_party/xmake-psx` from codeberg.org and gets HTTP 429.

```bash
git -C vendor/psyz submodule update --init nugget
```

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
hbmenu under Atmosphere.

Put the Mode2 BIN and CUE next to `us/`, not inside it. `boot_tim.bin` is
not a disc image.

```text
sdmc:/switch/dw3/Digimon World 3.cue
sdmc:/switch/dw3/Digimon World 3.bin
sdmc:/switch/dw3/us/
```

US boot creates an empty `memcard/` when that root exists, and formats
`memcard0.mcr` and `memcard1.mcr` when those files are absent. A packed
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
| `switch-game` | `tools/build_switch_game.sh`. The game image. `source/boot_main.c` calls `dw3_game_main`. Artifact `dw3-switch-game-nro` at `build/switch/dw3-switch.nro`. This job does not open a Release. |

A Release is
[`.github/workflows/switch-release.yml`](.github/workflows/switch-release.yml),
on `workflow_dispatch` or a `v*` tag. It is a prerelease. Assets are
`dw3-switch.nro`, `dw3-switch-nxlink.nro`, and
[`platforms/switch/release/README.txt`](platforms/switch/release/README.txt).
No disc data.

The latest preview release is
[v0.6.5-preview.7](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.7).
It uploads only the sampled VRAM rectangle and loads one field tile per
frame. It does not change the CPU or GPU clock.
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

`--nxlink` is a development binary. It is not the release. Launch steps
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
| [docs/P4.4_INPUT_STATUS.md](docs/P4.4_INPUT_STATUS.md) / [P4.5](docs/P4.5_STATUS.md) / [P4.6](docs/P4.6_MODULES_STATUS.md) | Left stick is ticked. The CDFS suite and the overlay sweep are still open. |
| [docs/P2.8_TITLE_BOOT.md](docs/P2.8_TITLE_BOOT.md) | PC title screen evidence |
| [docs/P3.2_FIELD.md](docs/P3.2_FIELD.md) … [P3.8](docs/P3.8_STAGES.md) | Phase 3 subsystem notes |
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
