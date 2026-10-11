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
| **3** | **Gate met (US)** — full PC loop; **P3.9** compare harness **done** ([P3.9](docs/P3.9_REFERENCE.md)). **Open:** EU playthrough, pad-driven boss KO, PC [GPU/battle polish](docs/P3_GPU_GLITCHES.md), `WSTAG460` stub |
| **4** | **In progress** — native libnx backend ([P4.1](docs/SWITCH_BACKEND.md)). **P4.7 done:** CMake game `.nro` links; **OLED** reached main menu + new game (owner smoke). Host wiring for **input / SD / overlays** landed ([P4.4](docs/P4.4_INPUT_STATUS.md), [P4.5](docs/P4.5_STATUS.md), [P4.6](docs/P4.6_MODULES_STATUS.md)) — hardware validation still open. **Phase 4 gate** (field + battle on Switch) **not met**. CI: job `switch` = host sample; `switch-game` = game `.nro` artifact |
| **5–10** | Playable Switch segment on hardware, polish, port-options, QoL / texture packs ([SWITCH_FEATURES](docs/SWITCH_FEATURES.md)). **P6.5** preview packaging: [v0.6.5-preview.1](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.1) (`.nro` + owner setup README; **no** game data). Phase 6 gate otherwise not started |

**PC:** Own US disc → `runtime/us` → `dw3-pc`. Early story is playable; some visuals still rough.

**Switch:** Own US tree on SD under `sdmc:/switch/dw3/us/` (see below). **Not** a finished handheld port — menu/new-game on OLED is proven; field/battle gate and full pad validation on hardware are still Phase 4–5. Build from **current `main`** for title/movie present fixes; the first GitHub preview predates some of that.

Layering: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Upstream

| Project | Role |
|---|---|
| [ReGame-Labs/dw3_decomp](https://github.com/ReGame-Labs/dw3_decomp) | Matching PS1 decomp — game logic source (`vendor/dw3_decomp`) |
| [Xeeynamo/psyz](https://github.com/Xeeynamo/psyz) | Psy-Q compatible shim (GPU/GTE/CD/SPU surface) (`vendor/psyz`; Switch fork pin in [PSYZ.md](docs/PSYZ.md)) |

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

For the **Switch game** build only, a partial init is enough (see Switch section).

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

**Switch SD (US):** copy the packed tree to `sdmc:/switch/dw3/us/` (`files_us.json`,
`SLUS_014.36`, `boot_tim.bin`, `AAA/…`). Create an empty
`sdmc:/switch/dw3/us/memcard/` before first launch (boot stops in memcard init
otherwise). EU uses `sdmc:/switch/dw3/eu/` when an EU build is available.

### Run (US, PC)

```bash
./build/pc/dw3-pc --runtime runtime/us
```

Keyboard: Enter = Start, arrows = D-pad. Bring-up env vars (`DW3_START_MODE`,
`DW3_AUTO_PAD`, …): [docs/P3_GATE.md](docs/P3_GATE.md) and `docs/P3.*.md`.

### Tooling self-check (no disc)

```bash
python3 -m pytest tools/tests -q
```

Includes headless Switch-backend stubs (`test_switch_backend.py`) and pad host
checks shared with PC.

### Switch builds (Phase 4)

Docker + devkitPro via `tools/dk.sh` — [tools/docker/README.md](tools/docker/README.md).
On WSL/NTFS mounts use `DW3_DK_TMPBUILD=/tmp/dw3-dk-build`.

**Host sample** (CI job `switch` — no game data, not the game):

```bash
./tools/dk.sh make -C platforms/switch
# → platforms/switch/dw3-switch.nro  (EGL present + audren hello)
```

**Game `.nro`** (CI job `switch-game` — artifact on PRs; Release via workflow below):

```bash
git submodule update --init vendor/dw3_decomp vendor/psyz
git -C vendor/psyz submodule update --init nugget external/SDL
# do not --recursive nugget: codeberg xmake-psx returns HTTP 429
bash tools/build_switch_game.sh
# → build/switch/dw3-switch.nro
```

Copy to `sdmc:/switch/dw3-switch.nro` and install your runtime tree as above.
Full Docker/mount recipe: [platforms/switch/README.md](platforms/switch/README.md).

**GitHub Release (preview, no game bytes):**
[`.github/workflows/switch-release.yml`](.github/workflows/switch-release.yml)
— `workflow_dispatch` or push a `v*` tag. Ships `dw3-switch.nro` +
[`platforms/switch/release/README.txt`](platforms/switch/release/README.txt).
Example: [v0.6.5-preview.1](https://github.com/Larbaco/dw3-switch/releases/tag/v0.6.5-preview.1).

Backend: [platforms/switch/psyz_backend/](platforms/switch/psyz_backend/).
Evidence: [docs/SWITCH_BACKEND.md](docs/SWITCH_BACKEND.md).

## Repository layout

```
dw3-switch/
├── docs/                      # roadmap, gates, phase evidence
├── platforms/pc/              # PSY-Z + SDL3 → dw3-pc
├── platforms/switch/          # Makefile sample + CMake game target, psyz_backend/
├── port/                      # overlays, cdfs, memcard, movies, patches
├── tools/                     # pack, build_switch_game.sh, compare_harness, …
├── vendor/dw3_decomp/         # upstream decomp (submodule)
└── vendor/psyz/               # PSY-Z (submodule; Switch backend branch for NX game)
```

## Documentation

| Doc | When to open it |
|---|---|
| [docs/ROADMAP.md](docs/ROADMAP.md) | Phases, gates, what’s next |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Host / PSY-Z / game layering |
| [docs/P3_GATE.md](docs/P3_GATE.md) | Phase 3 gate (US start → first boss) — **met** |
| [docs/P3.9_REFERENCE.md](docs/P3.9_REFERENCE.md) | DuckStation vs PC harness — **done** |
| [docs/P3_EU_BUILD.md](docs/P3_EU_BUILD.md) | EU `-DDW3_VERSION=eu` scaffold |
| [docs/P3_GPU_GLITCHES.md](docs/P3_GPU_GLITCHES.md) | PC present / battle mesh follow-ups |
| [docs/SWITCH_BACKEND.md](docs/SWITCH_BACKEND.md) | Switch backend, boot traces, CMake game build |
| [docs/P4.4_INPUT_STATUS.md](docs/P4.4_INPUT_STATUS.md) | hid → PS1 pad wiring (hardware TBD) |
| [docs/P4.5_STATUS.md](docs/P4.5_STATUS.md) | SD mount, log, time / cdfs on hardware TBD |
| [docs/P4.6_MODULES_STATUS.md](docs/P4.6_MODULES_STATUS.md) | No `dlopen`; static overlays |
| [docs/P4.2_VIDEO.md](docs/P4.2_VIDEO.md) / [P4.3_AUDIO.md](docs/P4.3_AUDIO.md) | EGL / audren bring-up |
| [docs/SWITCH_FEATURES.md](docs/SWITCH_FEATURES.md) | Residual Switch UX (Phase 10) |
| [docs/SURVEY.md](docs/SURVEY.md) / [PATCHES.md](docs/PATCHES.md) | Decomp survey and patch policy |
| [docs/DISC_SETUP.md](docs/DISC_SETUP.md) / [RUNTIME_LAYOUT](docs/RUNTIME_LAYOUT.md) | Owner disc → runtime tree |
| [docs/PSYZ.md](docs/PSYZ.md) | PSY-Z pin and renderer notes |
| [docs/P2.8_TITLE_BOOT.md](docs/P2.8_TITLE_BOOT.md) | PC title evidence |
| [docs/P3.2_FIELD.md](docs/P3.2_FIELD.md) … [P3.8](docs/P3.8_STAGES.md) | Phase 3 subsystem notes |
| [docs/COMMUNITY_QOL.md](docs/COMMUNITY_QOL.md) / [TEXTURE_PACKS](docs/TEXTURE_PACKS.md) / [PORT_OPTIONS](docs/PORT_OPTIONS.md) | Later optional features |

Hosts: [platforms/pc/README.md](platforms/pc/README.md),
[platforms/switch/README.md](platforms/switch/README.md).
Port glue: [port/README.md](port/README.md).
Compare harness: [tools/compare_harness/README.md](tools/compare_harness/README.md).

## Requirements

- **PC:** CMake, Ninja, SDL3 — [platforms/pc/README.md](platforms/pc/README.md)
- **Switch:** devkitPro `switch-dev`, or `devkitpro/devkita64` via `./tools/dk.sh` / Docker
- **PSY-Z** submodule (pin: [docs/PSYZ.md](docs/PSYZ.md))
- **Own disc** for runtime data (PC directory or Switch SD layout)

## License

Port scaffolding and original glue code: see [LICENSE](LICENSE).

Game code under `vendor/dw3_decomp` remains under that project’s license.
Original Digimon World 3 / 2003 assets remain property of their respective
owners and are not redistributed here.
