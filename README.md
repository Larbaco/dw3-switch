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
├── platforms/switch/     # libnx / homebrew host (EGL, audio, input, FS)
├── port/                 # platform-independent glue (loaders, shims)
├── tools/                # asset extract / pack helpers
└── vendor/dw3_decomp/    # upstream decomp (git submodule)
```

## Status

Scaffold only. Next steps are documented in [docs/ROADMAP.md](docs/ROADMAP.md).

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

- [devkitPro](https://devkitpro.org/) with `switch-dev`
- libnx, mesa/EGL (or deko3d path TBD)
- Own disc image for assets

## License

Port scaffolding and original glue code: see [LICENSE](LICENSE).

Game code under `vendor/dw3_decomp` remains under that project's license.
Original Digimon World 3 assets remain property of their respective owners
and are not redistributed here.
