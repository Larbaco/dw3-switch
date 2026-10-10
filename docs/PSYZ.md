# PSY-Z (vendor/psyz)

PC Psy-Q compatible shim used for Phase 1+ bring-up before a Switch
libnx backend exists.

## Pin

| Field | Value |
|---|---|
| Upstream | https://github.com/Xeeynamo/psyz.git |
| Working fork (submodule URL) | https://github.com/Larbaco/psyz.git |
| Commit | `559865467dc2f8f6a32bafd87f57803e8aa51d8b` (`dw3-str-ring-wrap`: STR ring wrap id=1 sentinel; base `9e6c124` SEQ/SEP) |
| Base upstream | `fac9bd58ccd8a0c35e31c6354528f9d625e9bdd1` |
| License | MIT — see [`vendor/psyz/LICENSE`](../vendor/psyz/LICENSE) |

Update the pin intentionally; call out submodule SHA changes in the
commit message. Submodule URL is the Larbaco fork until the MDEC wire
lands on Xeeynamo/psyz (READ-only upstream).

## Recursive submodules

PSY-Z vendors SDL3 under `vendor/psyz/external/SDL`. After cloning or
updating this repo:

```bash
git submodule update --init --recursive vendor/psyz
```

Other nested deps (cimgui, nugget, decomp tools) come along with
`--recursive`; PC sample builds need at least `external/SDL`.

## PC renderer default

For host builds use:

```bash
-DPSYZ_RENDERER=sdl3_gpu
```

Smoke-test from the repo root (build dir is gitignored via `/build/`):

```bash
cmake -S vendor/psyz/samples/gpu_hello -B build/psyz-gpu-hello -G Ninja \
  -DPSYZ_RENDERER=sdl3_gpu
cmake --build build/psyz-gpu-hello -j"$(nproc)"
test -x build/psyz-gpu-hello/hello
```

On hosts missing optional SDL Linux packages, configure may need flags
such as `-DSDL_X11_XSCRNSAVER=OFF` (and audio backends OFF) until
`libxss-dev` / ALSA / Pulse are installed. See
[SDL3 Linux build dependencies](https://wiki.libsdl.org/SDL3/README-linux#build-dependencies).

## Switch

There is no public SDL3 package for Nintendo Switch homebrew. The
libnx present/audio/input backend for PSY-Z is Phase 4 work; do not
assume the PC `sdl3_gpu` path ports as-is.
