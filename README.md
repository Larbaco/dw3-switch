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

Details and checkboxes: [docs/ROADMAP.md](docs/ROADMAP.md). Gate honesty:
[docs/P3_GATE.md](docs/P3_GATE.md).

| Phase | Summary |
|---|---|
| **0–2** | **Done** — toolchains, disc→runtime pipeline, US title boot from decomp C ([P2.8](docs/P2.8_TITLE_BOOT.md)) |
| **3** | **Gate met (US)** — field, battle, menus, card game, movies, SEP audio, stage sweep on PC; start→first boss (Genji) on a continuous US path with documented bring-up aids ([P3 gate](docs/P3_GATE.md)). **Open:** P3.9 DuckStation compare harness (title/field captured; battle oracle partial), EU playthrough, pad-driven boss KO, [GPU/sprite polish](docs/P3_GPU_GLITCHES.md), `WSTAG460` stub |
| **4** | **In progress** — native libnx PSY-Z backend ([P4.1](docs/SWITCH_BACKEND.md)); Switch `.nro` runs **EGL present + audren** smoke ([P4.2](docs/P4.2_VIDEO.md), [P4.3](docs/P4.3_AUDIO.md)); **game not linked on Switch yet** |
| **5–8** | Playable Switch, packaging, optional QoL / texture packs / port options (after Phase 4–5 gates) |

**Can you play on PC today?** Yes, with your **own US disc** packed to
`runtime/us`: title through early story is reachable in `dw3-pc`. Expect
visual rough edges and developer env vars for some automated paths; it is not
a finished retail experience. **Switch:** homebrew sample only (no game loop).

Layering: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

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

### PC build (`dw3-pc`, US default)

CMake, Ninja, SDL3 — [platforms/pc/README.md](platforms/pc/README.md)
(Pulse/PipeWire optional for audible audio/movies).

```bash
tools/apply_patches.sh
cmake -S platforms/pc -B build/pc -G Ninja -DPSYZ_RENDERER=sdl3_gpu
ninja -C build/pc dw3-pc
# equivalent: cmake --build build/pc -j"$(nproc)"
```

EU link scaffold (separate build tree, needs `runtime/eu`):
[docs/P3_EU_BUILD.md](docs/P3_EU_BUILD.md).

### Disc → runtime layout (required to run the game)

[docs/DISC_SETUP.md](docs/DISC_SETUP.md),
[docs/RUNTIME_LAYOUT.md](docs/RUNTIME_LAYOUT.md).

```bash
python3 tools/pack_runtime.py --version us --disks disks/us --out runtime/us --force
```

### Run (US)

```bash
./build/pc/dw3-pc --runtime runtime/us
```

Keyboard: Enter = Start, arrows = D-pad. Bring-up env vars (`DW3_START_MODE`,
`DW3_AUTO_PAD`, …): [docs/P3_GATE.md](docs/P3_GATE.md) and `docs/P3.*.md`.

### Tooling self-check (no disc)

```bash
python3 -m pytest tools/tests -q
```

### Switch `.nro` (Phase 4 host sample)

Docker + [devkitPro](https://devkitpro.org/) via `tools/dk.sh`
([tools/docker/README.md](tools/docker/README.md) — WSL/NTFS: use
`DW3_DK_TMPBUILD`):

```bash
./tools/dk.sh make -C platforms/switch
# → platforms/switch/dw3-switch.nro
```

This is the **combined EGL + audren** bring-up binary (not the full game).
Backend: [platforms/switch/psyz_backend/](platforms/switch/psyz_backend/).

## Repository layout

```
dw3-switch/
├── docs/                      # roadmap, gates, phase evidence
├── platforms/pc/              # PSY-Z + SDL3 → dw3-pc
├── platforms/switch/          # libnx + psyz_backend/ (Phase 4)
├── port/                      # overlays, cdfs, memcard, movies, patches
├── tools/                     # pack/extract, stage_sweep, compare_harness, …
├── vendor/dw3_decomp/         # upstream decomp (submodule)
└── vendor/psyz/               # PSY-Z (submodule)
```

## Documentation

| Doc | When to open it |
|---|---|
| [docs/ROADMAP.md](docs/ROADMAP.md) | Phases, gates, what’s next |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Host / PSY-Z / game layering |
| [docs/P3_GATE.md](docs/P3_GATE.md) | Phase 3 gate (US start → first boss) — **met** |
| [docs/P3.9_REFERENCE.md](docs/P3.9_REFERENCE.md) | DuckStation vs PC screenshot harness |
| [docs/P3_EU_BUILD.md](docs/P3_EU_BUILD.md) | EU `-DDW3_VERSION=eu` scaffold |
| [docs/P3_GPU_GLITCHES.md](docs/P3_GPU_GLITCHES.md) | PC present / sprite follow-ups |
| [docs/SURVEY.md](docs/SURVEY.md) | Decomp facts that drive the port |
| [docs/PATCHES.md](docs/PATCHES.md) | Decomp patch policy and series |
| [docs/DISC_SETUP.md](docs/DISC_SETUP.md) / [RUNTIME_LAYOUT](docs/RUNTIME_LAYOUT.md) | Owner disc → runtime tree |
| [docs/PSYZ.md](docs/PSYZ.md) | PSY-Z pin and renderer notes |
| [docs/SWITCH_BACKEND.md](docs/SWITCH_BACKEND.md) | Switch backend decision + smoke loop |
| [docs/P4.2_VIDEO.md](docs/P4.2_VIDEO.md) / [P4.3_AUDIO.md](docs/P4.3_AUDIO.md) | Switch EGL / audren bring-up |
| [docs/P2.8_TITLE_BOOT.md](docs/P2.8_TITLE_BOOT.md) | Title screen evidence |
| [docs/P3.2_FIELD.md](docs/P3.2_FIELD.md) … [P3.8](docs/P3.8_STAGES.md) | Phase 3 subsystem notes |
| [docs/COMMUNITY_QOL.md](docs/COMMUNITY_QOL.md) / [TEXTURE_PACKS](docs/TEXTURE_PACKS.md) / [PORT_OPTIONS](docs/PORT_OPTIONS.md) | Later optional features |
| [docs/SWITCH_FEATURES.md](docs/SWITCH_FEATURES.md) | Native Switch residual features (Phase 10) |

Hosts: [platforms/pc/README.md](platforms/pc/README.md),
[platforms/switch/README.md](platforms/switch/README.md).
Port glue: [port/README.md](port/README.md).
Compare harness: [tools/compare_harness/README.md](tools/compare_harness/README.md).

## Requirements

- **PC:** CMake, Ninja, SDL3 — [platforms/pc/README.md](platforms/pc/README.md)
- **Switch:** devkitPro `switch-dev`, or `devkitpro/devkita64` via `./tools/dk.sh`
- **PSY-Z** submodule (pin: [docs/PSYZ.md](docs/PSYZ.md))
- **Own disc** for in-game runtime data

## License

Port scaffolding and original glue code: see [LICENSE](LICENSE).

Game code under `vendor/dw3_decomp` remains under that project’s license.
Original Digimon World 3 / 2003 assets remain property of their respective
owners and are not redistributed here.
