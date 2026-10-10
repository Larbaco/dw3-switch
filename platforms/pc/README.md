# platforms/pc — PSY-Z host (Phase 2 title bring-up)

Desktop host using vendored PSY-Z (`vendor/psyz`, `sdl3_gpu`). Links the
US engine + `STDWTITL` + `port/` glue. Switch-specific code stays under
`platforms/switch/`.

## Build

From the repository root:

```bash
cmake -S platforms/pc -B build/pc -G Ninja -DPSYZ_RENDERER=sdl3_gpu
cmake --build build/pc -j"$(nproc)"
test -x build/pc/dw3-pc
```

If system `cmake` is missing, this host has used:

`/home/larbaco/Documents/Pessoal/llamacpp/.deps/cmake/bin/cmake`

Ninja may be at `~/.local/bin/ninja`.

### Configure fallback (missing optional packages)

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

Expect `dw3: mode=0xe00 title_entry=...`, `dw3-pad: port 0 connected`, then
the title window (logo may be glitched; see `docs/P2.8_TITLE_BOOT.md`).
Keyboard: Enter=Start, arrows=D-pad (PSY-Z SDL map). Close the window to
quit (`Psyz_QuitRequested`).

Pad unit check (no window): `gcc …` via `tools/tests/test_pad_host.py` or
the commands in that file.
