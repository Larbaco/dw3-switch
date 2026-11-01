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

# Both regions in ONE binary (P6.24; the game side is built twice, the region
# is chosen at run time):
cmake -S platforms/pc -B build/pc-both -G Ninja \
  -DDW3_VERSION=both -DPSYZ_RENDERER=sdl3_gpu -DSDL_X11_XSCRNSAVER=OFF
ninja -C build/pc-both dw3-pc
build/pc-both/dw3-pc --region eu          # or DW3_REGION=eu
```

`dw3-pc` from a `both` build takes the region from `--region us|eu`, then
`DW3_REGION`, then the runtime folder it is given (`DW3_RUNTIME` /
`--runtime`: judged by `files_<r>.json` and the executable in it, then by a
trailing `/us` or `/eu`), then the folders that are present under
`DW3_RUNTIME_BASE` (default `runtime`): one present boots it, both boot the
stored `region_default` / `last_region` (`port_options.cfg` in
`DW3_CONFIG_DIR`, else the base folder), else US. It logs
`dw3-region: running <r> (<source>)` and `dw3-pc: region=<r>`. A wrong value
(`--region xx`) stops with status 1 before anything is opened. See
[docs/SINGLE_BUILD.md](../../docs/SINGLE_BUILD.md#as-built-slice-3-the-dual-image-link-pc).

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

### Field CPU profiler (P6.22)

`-DDW3_PC_FIELD_PROF=ON` (separate tree, off by default) runs a shadow copy of
the Switch's CPU rasterizer next to the PC renderer and prints a `dw3-prof:`
line every 60 game frames (rasterizer, game logic, PC-only renderer, SPU
mixer). `DW3_PROF_SAMPLE`, `DW3_SOFT_TRACE` and `DW3_SOFT_HASH` add a sampler,
a `Draw_*` trace for `tools/soft_raster_bench.sh` and per-frame VRAM hashes.
See [docs/P6.22_FIELD_CPU.md](../../docs/P6.22_FIELD_CPU.md).

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

### Internal resolution (P6.11)

Options > Display > **Internal resolution** (1x, 2x or 3x, default 1x) draws the whole picture
at that multiple of the PS1 pixels through PSY-Z's scaled render target; the vertices stay
integer PS1 coordinates, so the polygon wobble stays and the 2D layers become N x N blocks.
**Native res while turbo** (default ON) drops to 1x while fast-forward runs and returns to
your setting afterwards (PSY-Z keeps the scaled target allocated, so the switch is one
VRAM copy). Both are saved in `port_options.cfg` (`internal_res=0|1|2`, `internal_res_turbo`).
`DW3_INTERNAL_RES=1|2|3` sets it for one run (and `DW3_INTERNAL_RES_TURBO=0|1`), never saved.
The old experiment `DW3_PC_INTERNAL_RES=3` is an alias of `DW3_INTERNAL_RES=3` for one release.

```bash
DW3_INTERNAL_RES=3 ./build/pc/dw3-pc --runtime runtime/us
```

At 3x with a 320×240 game display mode the image is 960×720. The SDL3 GPU host sets the
presentation window to 1280×720 pixels when the window is smaller than that (a window you made
larger stays), the 4:3 image is centered with 160-pixel side bars, and the US title's
background is widened to 16:9. The menu atlas is sampled linearly from exact pixel-edge UVs,
so the content maps pixel-for-pixel at that size and scales smoothly if you resize the window.
Details and the Switch design: [P6.11_INTERNAL_RES](../../docs/P6.11_INTERNAL_RES.md).

Widescreen (P6.1) is on by default for field and battle; start in 4:3 with:

```bash
DW3_WIDESCREEN=0 ./build/pc/dw3-pc --runtime runtime/us
```

The log shows `dw3-widescreen: gutter 53 px per side` in the field, in
battle and in the menus that take gutters. See [docs/P6.1_WIDESCREEN.md](../../docs/P6.1_WIDESCREEN.md).

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

## Region launcher (P6.24 slice 1)

`tools/build_pc_single.sh [OUT]` builds `dw3-pc-us`, `dw3-pc-eu` and a small
`dw3-pc` launcher that finds `runtime/us` and `runtime/eu`, asks (3 s, with a
default) when both exist, and starts the right binary. See
[docs/SINGLE_BUILD.md](../../docs/SINGLE_BUILD.md#as-built-slice-1). The plain
`DW3_VERSION=us|eu` builds are unchanged. With `DW3_VERSION=both` (above) the
launcher is not needed on the PC: one binary holds both regions.
