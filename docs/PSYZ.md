# PSY-Z (vendor/psyz)

PC Psy-Q compatible shim used for Phase 1+ bring-up before a Switch
libnx backend exists.

## Pin

| Field | Value |
|---|---|
| Upstream | https://github.com/Xeeynamo/psyz.git |
| Working fork (submodule URL) | https://github.com/Larbaco/psyz.git |
| Commit | `166e46a` (`dw3-widescreen-gutters`: P6.1 side gutters, `Psyz_VideoSetGutter`; parent `eb0393b` `dw3-str-pace` STR pump at 150 sectors/s) |
| Base upstream | `6d01339aadf16c5c9d6e44f44c0d66514752e472` |
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

There is still no **pacman** SDL3 package for Switch homebrew (portlibs
ship SDL2 only). `devkitPro/SDL` branches `switch-sdl-3.2` /
`switch-sdl-3.4` build from source under `devkita64`, but they expose
GLES renderers without SDL_GPU — so PSY-Z's PC default `sdl3_gpu` does
not apply. P4.1 chose a **native libnx** backend instead; see
[SWITCH_BACKEND.md](SWITCH_BACKEND.md).
