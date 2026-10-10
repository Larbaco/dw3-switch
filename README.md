# Digimon World 3 — Nintendo Switch port

Native Switch homebrew port based on the matching PS1 decompilation
[ReGame-Labs/dw3_decomp](https://github.com/ReGame-Labs/dw3_decomp).

This repository does **not** ship game assets. You need your own legally
obtained copy of *Digimon World 3* (USA, `SLUS-01436`) or *Digimon World 2003*
(Europe, `SLES-03936`).

## Layout

```
dw3-switch/
├── docs/                 # design notes and milestones
├── platforms/pc/         # PC bring-up host (CMake, PSY-Z, SDL3)
├── platforms/switch/     # libnx / homebrew host (EGL, audio, input, FS)
├── port/                 # platform-independent glue (loaders, shims)
├── tools/                # asset extract / pack helpers
├── vendor/dw3_decomp/    # upstream decomp (git submodule)
└── vendor/psyz/          # PSY-Z Psy-Q shim (git submodule, Phase 1+)
```

Phase 1 desktop bring-up uses **CMake** to build `platforms/pc/` against the
**PSY-Z** submodule (`vendor/psyz`) and SDL3 before tightening the Switch host.

## Status

**Phase 1 complete:** PC host (`dw3-pc` + PSY-Z), Switch `.nro` skeleton
(CI), disc → `files_us.json` → `runtime/us/` pack tools, and Docker/CI
toolchains. Survey: [docs/SURVEY.md](docs/SURVEY.md). Next is Phase 2
(port glue + engine bring-up on PC) per [docs/ROADMAP.md](docs/ROADMAP.md);
layering in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

Localization (PT-BR first, then other languages) is a Phase 6 goal after
the playable Switch port (Phase 5 gate); see P6.6–P6.8 in the roadmap.

Optional compatibility with the Digimon World 2003 community QoL patch
ecosystem (and companion app, if a protocol exists) is Phase 7; see
[docs/ROADMAP.md](docs/ROADMAP.md) and
[docs/COMMUNITY_QOL.md](docs/COMMUNITY_QOL.md).

## Setup

```bash
git clone --recursive <this-repo-url> dw3-switch
cd dw3-switch

# If you already cloned without submodules:
git submodule update --init --recursive
```

Upstream decomp lives at `vendor/dw3_decomp`. Follow its README to extract your
disc into a local `disks/` tree when you are ready to build game code paths
(that directory is gitignored).

## Requirements (planned)

- [devkitPro](https://devkitpro.org/) with `switch-dev` (or Docker:
  `devkitpro/devkita64`)
- libnx, mesa/EGL (or deko3d path TBD)
- [PSY-Z](https://github.com/Xeeynamo/psyz) as the Psy-Q shim (submodule, Phase 1)
- SDL3 + CMake for the PC bring-up host
- Own disc image for assets

## License

Port scaffolding and original glue code: see [LICENSE](LICENSE).

Game code under `vendor/dw3_decomp` remains under that project's license.
Original Digimon World 3 assets remain property of their respective owners
and are not redistributed here.
