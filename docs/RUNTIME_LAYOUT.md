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
  BOOT_IMAGE.txt               # Phase 1 stub; see below
  # boot_tim.bin               # Phase 2+ when exe-tail extract exists
  AAA/
    PRO/
    DAT/
    STR/
    ...
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
EU `dw3-pc` link uses `-DDW3_VERSION=eu` (`docs/P3_EU_BUILD.md`); run still
needs a local `runtime/eu` tree (gitignored).

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
