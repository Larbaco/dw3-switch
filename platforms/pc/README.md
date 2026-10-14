# platforms/pc — PSY-Z host (Phase 2 title bring-up)

Desktop host using vendored PSY-Z (`vendor/psyz`, `sdl3_gpu`). Links the
engine + `STDWTITL` + `port/` glue (default **US**). Switch-specific code
stays under `platforms/switch/`. EU link: `-DDW3_VERSION=eu` in a separate
build tree (`docs/P3_EU_BUILD.md`).

## Build

From the repository root:

```bash
cmake -S platforms/pc -B build/pc -G Ninja -DPSYZ_RENDERER=sdl3_gpu
cmake --build build/pc -j"$(nproc)"
test -x build/pc/dw3-pc

# EU (separate tree; needs owner runtime/eu to run):
cmake -S platforms/pc -B build/pc-eu -G Ninja \
  -DDW3_VERSION=eu -DPSYZ_RENDERER=sdl3_gpu -DSDL_X11_XSCRNSAVER=OFF
ninja -C build/pc-eu dw3-pc
```

If system `cmake` is missing, this host has used:

`/home/larbaco/Documents/Pessoal/llamacpp/.deps/cmake/bin/cmake`

Ninja may be at `~/.local/bin/ninja`.

### Audible SDL audio (P3.7 owner listen)

Install headers (`libpulse-dev`, `libasound2-dev`, `libpipewire-0.3-dev`),
then configure **with** real backends. Without them, vendored SDL only
gets `dummy`/`disk` and `Psyz_AudioInit` falls back to silent dummy:

```bash
cmake -S platforms/pc -B build/pc -G Ninja \
  -DPSYZ_RENDERER=sdl3_gpu \
  -DSDL_X11_XSCRNSAVER=OFF \
  -DSDL_PULSEAUDIO=ON \
  -DSDL_PIPEWIRE=ON \
  -DSDL_ALSA=ON
```

### Configure fallback (missing optional packages / headless CI)

```bash
cmake -S platforms/pc -B build/pc -G Ninja \
  -DPSYZ_RENDERER=sdl3_gpu \
  -DSDL_X11_XSCRNSAVER=OFF \
  -DSDL_ALSA=OFF -DSDL_JACK=OFF -DSDL_PIPEWIRE=OFF \
  -DSDL_PULSEAUDIO=OFF -DSDL_SNDIO=OFF
```

## Runtime data (own disc)

```bash
# disks/us on a Linux FS (see docs/DISC_SETUP.md for NTFS DUMMY. note)
python3 tools/pack_runtime.py --version us --disks disks/us --out runtime/us --force
```

## Run

```bash
./build/pc/dw3-pc --runtime runtime/us
# env alternatives: DW3_RUNTIME, DW3_MEMCARD_DIR
```

### Experimental internal 720p render

The SDL3 GPU backend can render at a 3× internal scale. Opt in on PC only:

```bash
DW3_PC_INTERNAL_RES=3 ./build/pc/dw3-pc --runtime runtime/us
```

With a 320×240 game display mode, the game image occupies 960×720 pixels.
The SDL3 GPU host sets the initial 3× presentation window to 1280×720 pixels;
the 4:3 image is centered with 160-pixel side bars. The menu atlas is sampled
linearly from exact pixel-edge UVs, so the 960×720 content maps pixel-for-pixel
at this output size and scales smoothly if the user resizes the window. The
normal internal scale remains 1× when the variable is unset; this option is not
compiled into the Switch host.

Widescreen (P6.1) is on by default for field and battle; start in 4:3 with:

```bash
DW3_WIDESCREEN=0 ./build/pc/dw3-pc --runtime runtime/us
```

The log shows `dw3-widescreen: gutter 32 px per side` in the field and
`53` in battle. See [docs/P6.1_WIDESCREEN.md](../../docs/P6.1_WIDESCREEN.md).

Expect `dw3: mode=0xe00 title_entry=...`, `dw3-pad: port 0 connected`, then
the title window (logo may be glitched; see `docs/P2.8_TITLE_BOOT.md`).
Keyboard: Enter=Start, arrows=D-pad (PSY-Z SDL map). Close the window to
quit (`Psyz_QuitRequested`).

### Opening movie present smoke (P3.7)

```bash
ln -sfn "$HOME/.cache/dw3-switch/runtime/us" runtime/us
# Prefer offscreen or a real display — dummy has no SDL_GPU backend.
SDL_VIDEODRIVER=offscreen DW3_START_MODE=0xE01 DW3_RUNTIME=runtime/us \
  DW3_FB_DUMP=build/movie-evidence/opening.ppm \
  timeout -s KILL 45 ./build/pc/dw3-pc --runtime runtime/us
```

See `docs/P3.7_MOVIES.md` (Done-when met: play + Start-skip + owner
audible XA). Video presentation quality is a follow-up.

Pad unit check (no window): `gcc …` via `tools/tests/test_pad_host.py` or
the commands in that file.
