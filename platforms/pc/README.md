# platforms/pc — PSY-Z host skeleton (Phase 1)

Desktop bring-up host using vendored PSY-Z (`vendor/psyz`) with the
`sdl3_gpu` renderer. Switch-specific code stays under `platforms/switch/`.

## Smoke build (acceptance)

From the repository root:

```bash
cmake -S platforms/pc -B build/pc -G Ninja -DPSYZ_RENDERER=sdl3_gpu
cmake --build build/pc -j"$(nproc)"
test -x build/pc/dw3-pc
```

Binary path: `build/pc/dw3-pc`.

If system `cmake` is missing, this host has used:

`/home/larbaco/Documents/Pessoal/llamacpp/.deps/cmake/bin/cmake`

Ninja may be at `~/.local/bin/ninja`.

## Dependencies

Prefer installing SDL3 Linux build deps via apt rather than disabling
backends permanently:

```bash
sudo apt-get install -y build-essential pkg-config ninja-build \
  libgl1-mesa-dev libxss-dev \
  libasound2-dev libpulse-dev
```

See [SDL3 Linux build dependencies](https://wiki.libsdl.org/SDL3/README-linux#build-dependencies)
and `docs/PSYZ.md`.

### Configure fallback (missing optional packages)

If configure fails on `XSCRNSAVER` / audio backends (no `libxss-dev` or
ALSA/Pulse), the flags that worked for Task 4 on this host are:

```bash
cmake -S platforms/pc -B build/pc -G Ninja \
  -DPSYZ_RENDERER=sdl3_gpu \
  -DSDL_X11_XSCRNSAVER=OFF \
  -DSDL_ALSA=OFF -DSDL_JACK=OFF -DSDL_PIPEWIRE=OFF \
  -DSDL_PULSEAUDIO=OFF -DSDL_SNDIO=OFF
```

Install the apt packages above when possible so these OFF flags are
unnecessary.

## Run

```bash
./build/pc/dw3-pc
```

Close the window to quit (`Psyz_QuitRequested`).
