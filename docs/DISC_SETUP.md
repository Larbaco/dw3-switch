# US disc extract and file-id map

Owner-side steps for Phase 1.5: turn a personal Digimon World 3 (US) MODE2
disc image into a local `disks/us/` tree and `files_us.json`. Game data stays
on disk only — `/disks/` is gitignored; never commit disc contents.

## Prerequisites

- Own US disc image (MODE2/2352 `.bin`), not redistributed by this repo.
- `vendor/dw3_decomp` initialized (`git submodule update --init vendor/dw3_decomp`).
- Optional: activate `.venv` for a local Python 3.

## Cue filename mismatch

Redump-style cues often name the track `Digimon World 3 (USA).bin`. On this
machine the image is `Digimon World 3.bin` (same SHA-1 as the US dump). Always
pass the `.bin` path directly to the tools; do not rely on the `.cue` FILE line.

| Field | Value |
|---|---|
| Owner image | `/mnt/windows/Jogos/PS1/Digimon World 3/Digimon World 3.bin` |
| Disc image SHA-1 | `f0b022f9be53cbce14640abd8f01beaadcb35208` |
| Expected `SLUS_014.36` SHA-1 | `444653259f78ddb483fd22af72cce9276f42f214` |

## Extract

From the repo root (extract of a ~648 MB MODE2 image can take several minutes):

```bash
# from repo root, with venv optional
python3 vendor/dw3_decomp/tools/extract_disc.py \
  "/mnt/windows/Jogos/PS1/Digimon World 3/Digimon World 3.bin" \
  disks/us

sha1sum disks/us/SLUS_014.36
# expect: 444653259f78ddb483fd22af72cce9276f42f214
```

### NTFS / Windows mounts

The ISO has a root file named `DUMMY.` (trailing dot). NTFS and many
Windows-backed mounts reject that name (`OSError: [Errno 22] Invalid
argument`). If the repo lives under `/mnt/windows/...`, point `disks/us` at
a Linux filesystem before extracting:

```bash
mkdir -p "$HOME/.cache/dw3-switch/disks/us"
mkdir -p disks
ln -sfn "$HOME/.cache/dw3-switch/disks/us" disks/us
```

Then run `extract_disc.py` with `disks/us` as usual. `/disks/` remains
gitignored either way.

## Build file-id map

Maps `FILE_SECTORS` LBAs from the decomp to ISO9660 paths:

```bash
python3 tools/build_file_map.py \
  --bin "/mnt/windows/Jogos/PS1/Digimon World 3/Digimon World 3.bin" \
  --version us \
  --file-table vendor/dw3_decomp/src/engine/file/file_table.c \
  --out disks/us/files_us.json

python3 tools/build_file_map.py \
  --check disks/us/files_us.json \
  --file-table vendor/dw3_decomp/src/engine/file/file_table.c
```

`--check` exits 0 when every non-zero `FILE_SECTORS` id has a path. If holes
remain after a correct LBA join, re-check with `--allow-missing ID` (repeatable)
and document each id and reason below.

### Allow-missing (if any)

None for the owner US disc when extract + map succeed. Add rows here only if
`--check` reports holes that are intentional (e.g. unused table slots with no
ISO file).

| File id | Reason |
|---|---|
| — | — |

## Layout notes

- Output tree: `disks/us/` (exe, `AAA/`, overlays, etc.) plus `files_us.json`.
- Runtime packing for SD/PC paths: `tools/pack_runtime.py` → see `docs/RUNTIME_LAYOUT.md`.

## Formal CDFS suite (PC or SD tree)

After `pack_runtime.py` (or after the card already has `switch/dw3/us/`),
run the P2.5 / P4.5 host suite against that root — no game assets in git:

```bash
gcc -std=c11 -Wall -Wextra -O2 \
  -o build/cdfs_suite tools/cdfs_suite.c port/cdfs/cdfs.c -Iport/cdfs
./build/cdfs_suite --root runtime/us
# Mounted or copied SD layout (same relative files):
./build/cdfs_suite --root /path/to/switch/dw3/us
DW3_SD_RUNTIME=/path/to/switch/dw3/us python3 -m pytest \
  tools/tests/test_cdfs_suite.py -k packed -v
```

See `docs/P4.5_STATUS.md`.

## EU (Digimon World 2003) extract

Same tools, different executable and map. Prefer an ext4 cache symlink for
`disks/eu` (NTFS rejects `DUMMY.` the same way as US).

```bash
mkdir -p "$HOME/.cache/dw3-switch/disks/eu"
mkdir -p disks
ln -sfn "$HOME/.cache/dw3-switch/disks/eu" disks/eu

# Owner MODE2/2352 .bin (example Redump name; pass the .bin path directly)
python3 vendor/dw3_decomp/tools/extract_disc.py \
  "/path/to/Digimon World 2003 (Europe)....bin" \
  disks/eu

python3 tools/build_file_map.py \
  --bin "/path/to/Digimon World 2003 (Europe)....bin" \
  --version eu \
  --file-table vendor/dw3_decomp/src/engine/file/file_table.c \
  --out disks/eu/files_eu.json

python3 tools/build_file_map.py --check disks/eu/files_eu.json \
  --file-table vendor/dw3_decomp/src/engine/file/file_table.c

python3 tools/pack_runtime.py --version eu --disks disks/eu --out runtime/eu --force
mkdir -p runtime/eu/memcard
```

| Field | Value |
|---|---|
| Product / EXE | `SLES_039.36` (`BESLES-03936`) |
| Map | `files_eu.json` |
| Boot TIM | extracted from EXE by `pack_runtime.py` when missing |
| FIGHTSTG file id | `0x167` (US `0x159`) |

EU `dw3-pc` / Switch: `docs/P3_EU_BUILD.md`. Never commit `disks/` or `runtime/`.
