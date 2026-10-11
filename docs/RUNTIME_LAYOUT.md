# Runtime data layout

Shared relative layout for the Phase 1+ hosts. PC and Switch use the same
paths under a version root; only the mount prefix differs.

## Paths

| Host | Root |
|---|---|
| PC (dev) | `runtime/<ver>/` (repo-local, gitignored) |
| Switch SD | `sdmc:/switch/dw3/<ver>/` |

`<ver>` is `us` or `eu`. Example US trees:

```text
runtime/us/                    # PC
sdmc:/switch/dw3/us/           # Switch (same relative layout)
  files_us.json
  SLUS_014.36
  boot_tim.bin                 # extracted by packer when available
  # BOOT_IMAGE.txt             # fallback if the executable tail is unavailable
  AAA/
    PRO/
    DAT/
    STR/
    ...
  memcard/                     # required on Switch; can start empty
```

EU uses `files_eu.json` and `SLES_039.36` instead of the US names.

## Packing

From a completed disc extract + file map (`docs/DISC_SETUP.md`):

```bash
python3 tools/pack_runtime.py --version us --disks disks/us --out runtime/us
python3 tools/build_file_map.py --check runtime/us/files_us.json \
  --file-table vendor/dw3_decomp/src/engine/file/file_table.c

# EU (owner disc; same layout, SLES_039.36 + files_eu.json):
python3 tools/pack_runtime.py --version eu --disks disks/eu --out runtime/eu
python3 tools/build_file_map.py --check runtime/eu/files_eu.json \
  --file-table vendor/dw3_decomp/src/engine/file/file_table.c
```

`pack_runtime.py` refuses to run if `files_<ver>.json` is missing. If
`--out` already exists and is non-empty, pass `--force` to wipe it first.

Form-2 `.STR` files in the extract are 2048 bytes per sector. That drops
the last two XA ADPCM blocks and the opening soundtrack runs slow. On
boot, the game rewrites any such file to raw Mode2/2352 when it can see
the owner's disc image. It looks for `DW3_DISC_BIN`, `DW3_DISC_CUE`, then
`disc.cue` / `disc.bin` / a single `*.cue` or `*.bin` in the runtime root
and in the parent directory. `boot_tim.bin` is never a disc image. A CUE
whose `FILE` name is not on the card still selects the only other `.bin`
in that directory when the file is large enough to cover the STR extents.
Put the pair here on the card:

```text
sdmc:/switch/dw3/Digimon World 3.cue
sdmc:/switch/dw3/Digimon World 3.bin
sdmc:/switch/dw3/us/AAA/STR/    # rewritten in place on first launch
```

The first launch copies about 260 MB. On Switch the screen keeps the
dark blue clear and draws a progress bar while the log says `restore`
(see `docs/P3.7_MOVIES.md`). The next launch only checks file sizes.
The same rewrite can be done on the PC before copying:

```bash
python3 tools/restore_form2_str.py \
  --runtime runtime/us \
  --bin "/path/to/Digimon World 3.bin"
```

A tree left at 2048 still boots; the movie picture is paced, and the
sound stays slow until a disc image is visible at startup.

EU `dw3-pc` link uses `-DDW3_VERSION=eu` (`docs/P3_EU_BUILD.md`); run still
needs a local `runtime/eu` tree (gitignored).

### Memory-card directory

The Switch host expects `<runtime-root>/memcard/` to exist. On first boot,
`memcard_host_ensure()` creates formatted `memcard0.mcr` / `memcard1.mcr` files
when they are absent, but it does not create their parent directory. The
runtime packer does not preserve empty directories, so create the directory
before copying a freshly packed runtime to the SD card:

```bash
mkdir -p runtime/us/memcard
```

For the OLED layout this is `sdmc:/switch/dw3/us/memcard/`. An empty directory
is enough; existing memory-card images should be preserved rather than
replaced with blank ones. The 2026-10-10 hardware smoke initially stopped at
`memcard_host_ensure()` until the empty directory was created.

## Boot image

On PS1 the boot TIM is the executable's tail, resident at `OVERLAY_VRAM`
before overlays overwrite that region (`BOOT_IMAGE_RECT` in
`vendor/dw3_decomp/src/engine/system/main.c`). The packer writes
`BOOT_IMAGE.txt` explaining the gap; extract the tail into
`runtime/us/boot_tim.bin` when assets are available. `pack_runtime.py`
extracts the TIM from the version executable (STAGE_VRAM) when
`disks/<ver>/boot_tim.bin` is missing. The PC host loads it into
`STAGE_VRAM` before `dw3_game_main` (see `docs/P2.8_TITLE_BOOT.md`).

## Git

`/runtime/` is gitignored. Never commit game bytes from `disks/` or `runtime/`.
