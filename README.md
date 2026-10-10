# Digimon World 3 — Nintendo Switch port

A **native Switch homebrew** port of *Digimon World 3* / *Digimon World 2003*,
built from the matching PlayStation decompilation rather than an emulator or a
from-scratch remake.

Matching decomps already recover the game as readable C. This project takes
that C, runs it on a modern Psy-Q–compatible shim
([PSY-Z](https://github.com/Xeeynamo/psyz)), brings it up on **PC first**
(where debugging is cheap), then keeps a thin **libnx** host for the console.

## What this is not

- An emulator wrapper (no PS1 core packed inside the `.nro`)
- A remake or “spiritual successor” with new engine code as the source of truth
- A place to download ROMs, disc images, or game assets

## Status

**Phase 2 (PC title) reached:** `dw3-pc` links engine + `STDWTITL` + cdfs/
memcard + PSY-Z and boots to the US title screen from decomp C (GPU polish
open). See [docs/P2.8_TITLE_BOOT.md](docs/P2.8_TITLE_BOOT.md) and
[docs/ROADMAP.md](docs/ROADMAP.md). Phase 1 host/CI/pack tools remain;
layering in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

**Not playable yet.** Title boots on PC; field/battle and Switch polish are
later phases. See [docs/ROADMAP.md](docs/ROADMAP.md).

Later phases cover a playable Switch build, localization, optional community
QoL compatibility, texture-pack overlays, and a title-menu port options
screen (toggles for non-vanilla features) — only after the port gates land.
Details live in the roadmap and linked design notes, not here.

## Upstream

| Project | Role |
|---|---|
| [ReGame-Labs/dw3_decomp](https://github.com/ReGame-Labs/dw3_decomp) | Matching PS1 decomp — game logic source (`vendor/dw3_decomp`) |
| [Xeeynamo/psyz](https://github.com/Xeeynamo/psyz) | Psy-Q compatible shim (GPU/GTE/CD/SPU surface) (`vendor/psyz`) |

This repository owns the **hosts**, **port glue**, and **disc → runtime tools**.
Prefer patches here (`port/`, `platforms/`) over editing the decomp submodule
unless the fix clearly belongs upstream.

## Acknowledgments

This port stands on other people’s work. In particular:

### Foundations

| Project | Thanks for |
|---|---|
| [ReGame-Labs/dw3_decomp](https://github.com/ReGame-Labs/dw3_decomp) | The matching Digimon World 3 / 2003 decompilation — the game logic this port builds on |
| [Xeeynamo/psyz](https://github.com/Xeeynamo/psyz) | PSY-Z, the Psy-Q compatible shim used for PC bring-up (and the intended Switch path) |
| [libsdl-org/SDL](https://github.com/libsdl-org/SDL) | SDL3, used through PSY-Z’s desktop backends |

### Ideas and prior art

| Project | Thanks for |
|---|---|
| [Xive080/D-W-3-Recomp](https://github.com/Xive080/D-W-3-Recomp) (OpenWorld2003) | A public PC recomp of the same game; useful reference for CD/filesystem pitfalls, launcher/mod catalog shape, and “what players already expect” — not a dependency of this native port |
| Digimon World 2003 community QoL authors (Flawe, guiomatos, markisha64 / Marko Grizelj, EmeraldPhoenix, and others) | Patch and UX ideas tracked for optional later compatibility; see [docs/COMMUNITY_QOL.md](docs/COMMUNITY_QOL.md) |

### Platform and tooling

| Project | Thanks for |
|---|---|
| [devkitPro](https://devkitpro.org/) / [libnx](https://github.com/switchbrew/libnx) | Switch homebrew toolchain and APIs for the `.nro` host |
| The broader matching-decompilation and recomp communities | Patterns for shims, PC-first bring-up, and honest progress reporting |

*Digimon* and *Digimon World* are trademarks of their respective owners. This is
an unofficial fan project and is not affiliated with Bandai Namco or Nintendo.

## Own disc only

You need a legally obtained copy of *Digimon World 3* (USA, `SLUS-01436`) or
*Digimon World 2003* (Europe, `SLES-03936`). Disc images and extracted trees
are gitignored on purpose and are never shipped with this repo.

## Quick start

```bash
git clone --recursive <this-repo-url> dw3-switch
cd dw3-switch

# If you already cloned without submodules:
git submodule update --init --recursive
```

### PC host smoke build

Needs CMake, Ninja, and SDL3 build dependencies (see
[platforms/pc/README.md](platforms/pc/README.md)):

```bash
cmake -S platforms/pc -B build/pc -G Ninja -DPSYZ_RENDERER=sdl3_gpu
cmake --build build/pc -j"$(nproc)"
# → build/pc/dw3-pc
```

### Disc → runtime layout (optional until Phase 2)

Extract your disc and build the file-id map, then pack a runtime tree. Full
steps: [docs/DISC_SETUP.md](docs/DISC_SETUP.md) and
[docs/RUNTIME_LAYOUT.md](docs/RUNTIME_LAYOUT.md).

```bash
python3 tools/pack_runtime.py --version us --disks disks/us --out runtime/us
```

### Switch `.nro` skeleton

Uses Docker + [devkitPro](https://devkitpro.org/) via `tools/dk.sh` (see
[tools/docker/README.md](tools/docker/README.md) for WSL/NTFS mount notes):

```bash
./tools/dk.sh make -C platforms/switch
# → platforms/switch/dw3-switch.nro
```

No game data is required for the Phase 1 skeleton.

## Repository layout

```
dw3-switch/
├── docs/                 # design notes and milestones for this port
├── platforms/pc/         # PC bring-up host (CMake, PSY-Z, SDL3)
├── platforms/switch/     # libnx / homebrew host
├── port/                 # platform-independent glue (loaders, shims)
├── tools/                # disc extract / pack helpers
├── vendor/dw3_decomp/    # upstream decomp (git submodule)
└── vendor/psyz/          # PSY-Z (git submodule)
```

## Documentation

| Doc | When to open it |
|---|---|
| [docs/ROADMAP.md](docs/ROADMAP.md) | Phases, gates, what’s next |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Host / PSY-Z / game layering |
| [docs/SURVEY.md](docs/SURVEY.md) | Decomp facts that drive the port |
| [docs/DISC_SETUP.md](docs/DISC_SETUP.md) | Owner disc → `disks/<ver>/` |
| [docs/RUNTIME_LAYOUT.md](docs/RUNTIME_LAYOUT.md) | PC / SD runtime tree |
| [docs/PSYZ.md](docs/PSYZ.md) | PSY-Z pin and renderer notes |
| [docs/COMMUNITY_QOL.md](docs/COMMUNITY_QOL.md) | Optional QoL compatibility (later) |
| [docs/TEXTURE_PACKS.md](docs/TEXTURE_PACKS.md) | Texture pack overlays (later) |
| [docs/PORT_OPTIONS.md](docs/PORT_OPTIONS.md) | Title-menu toggles for port-only features (later) |

## Requirements

- **PC:** CMake, Ninja, SDL3 deps — [platforms/pc/README.md](platforms/pc/README.md)
- **Switch:** [devkitPro](https://devkitpro.org/) `switch-dev`, or Docker image
  `devkitpro/devkita64` via `./tools/dk.sh`
- **PSY-Z** submodule (already vendored for Phase 1)
- **Own disc** for any game-data path

## License

Port scaffolding and original glue code: see [LICENSE](LICENSE).

Game code under `vendor/dw3_decomp` remains under that project’s license.
Original Digimon World 3 / 2003 assets remain property of their respective
owners and are not redistributed here.
