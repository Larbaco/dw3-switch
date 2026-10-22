# PSY-Z (vendor/psyz)

PC Psy-Q compatible shim used for Phase 1+ bring-up before a Switch
libnx backend exists.

## Pin

| Field | Value |
|---|---|
| Upstream | https://github.com/Xeeynamo/psyz.git |
| Working fork (submodule URL) | https://github.com/Larbaco/psyz.git |
| Commit | `fa0aee5` (`dw3-spu-bus-volume`: P6.17 host mixer buses with per-bus gain, libsnd track routing and bus stats, two commits on top of `2caa505`; see below. `2caa505` is `dw3-spu-envx-release`: backport of upstream `a7862ba` / #205, a voice that ends without repeat reads ENVX 0, fixes #188 voice starvation; its parent `fabc2bf` is `dw3-pal-aspect` PAL 240-line aspect) |
| Base upstream | `6d01339aadf16c5c9d6e44f44c0d66514752e472` |
| License | MIT — see [`vendor/psyz/LICENSE`](../vendor/psyz/LICENSE) |

Update the pin intentionally; call out submodule SHA changes in the
commit message. Submodule URL is the Larbaco fork until the MDEC wire
lands on Xeeynamo/psyz (READ-only upstream).

## Mixer buses (P6.17)

`psyz/spu.h` has a host extension that is not PS1 behaviour: voices belong to
one of four buses, each bus has a Q15 gain (`Psyz_SpuSetBusGain`), and the
CD/XA input has its own (`Psyz_SpuSetCdGain`). A voice takes its bus when it
is keyed on (`psyz_snd_voice_bus` reads `_svm_voice[v].seq_sep_no`, which
`SsUtKeyOn` sets to `0x21` and the SEP player sets to `(sep << 8) |
seq_access_num`), and `Psyz_SndSetSeqBus(seq, sep, bus)` is how the host
routes a track. Unity gains skip the multiply: the default mix is
bit-identical to the mixer without buses, which `spu_bus::
BusDefaultMixMatchesTheMixerWithoutBuses` pins with a hash rendered by
`2caa505`. `Psyz_SpuEnableBusStats` / `Psyz_SpuGetBusStats` count energy and
peak per bus before and after the gain (diagnostics only, off by default).
The PSP backend (`psp_sas.c`) does not implement these calls. Use in this
port: [P6.17_VOLUME.md](P6.17_VOLUME.md).

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
