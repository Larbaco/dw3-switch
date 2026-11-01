# port/cdfs — file-id → host filesystem

Maps Digimon World 3 **numeric file ids** to files under a packed runtime
root (`docs/RUNTIME_LAYOUT.md`), using the same JSON shape as
`files_us.json` / `files_eu.json` from `tools/build_file_map.py`.

## Mapping

1. Host calls `cdfs_set_root("runtime/us")` (PC) or the SD equivalent.
2. Host calls `cdfs_load_map(NULL)` to load `<root>/files_us.json`
   (falls back to `files_eu.json`), or passes an explicit map path.
3. Each JSON entry under `"files"` is `"<id>": { "path": "...", "bytes": N, ... }`.
   Relative `path` is joined to the root (`AAA/PRO/...`, etc.). Absolute paths
   and any `.` / `..` (or empty) path components are rejected at map load.
4. `cdfs_file_size(id)` / `cdfs_read(id, buf, buflen)` open that host file
   and return **payload bytes as stored on disk** (byte-identical to the
   packed tree). The JSON `bytes` field is advisory; on-disk size wins.

`cdfs_read` and `cdfs_read_at` finish before they return. A field-map
tile goes through `cdfs_read_field_tile`. The first tile of a file id
reads that whole file into one resident buffer. A different id replaces
it. Later tiles copy from the buffer, and the reader is idle when the
call returns, so a second tile in the same frame succeeds. `cdfs_shutdown`
frees the buffer. `cdfs_poll` is a no-op. Map headers and `FILE_CACHE`
stay on `cdfs_read_at`. Preview.7 held one tile per frame until
`cdfs_poll`; that hold is gone.

## Overrides (P6.7)

`cdfs_override_set(id, "l10n/ptbr/108.bin")` makes `cdfs_file_size`, `cdfs_read`, `cdfs_read_at` and
`cdfs_read_field_tile` open that file (a path under the root, at most 47 bytes) instead of the map's, until
`cdfs_override_clear(id)`. The map's LBA and sector fields are kept; `cdfs_lookup_lba` still answers with the disc
layout. The text pack loader (`port/l10n/`) is the only user: on the host the engine reads a text file by id (patch
0009), so this and the two file-table arrays are all a pack needs. See [LOCALIZATION](../../docs/LOCALIZATION.md).

## Out of scope

- PSY-Z `libcd` / `St*` **STR movie streaming** (keep that on the sector
  path).
- Re-encoding inside `cdfs_read`. That call serves files already stored
  under the runtime root. `runtime_extract.c` is the US boot path that
  fills the root from a Mode2/2352 BIN (Form-1 user data, raw Form-2
  `.STR`). See `docs/RUNTIME_LAYOUT.md`.

## Build (tiny host CLI)

From the repo root:

```bash
mkdir -p build
gcc -std=c11 -Wall -Wextra -O2 \
  -o build/cdfs_cat tools/cdfs_cat.c port/cdfs/cdfs.c -Iport/cdfs
./build/cdfs_cat --root tools/testdata/cdfs_mini \
  --map tools/testdata/cdfs_mini/files_mini.json 1
```

`tools/tests/test_cdfs.py` builds this CLI and checks byte-identical reads
against `tools/testdata/cdfs_mini/` (no real disc).

## Formal suite (P2.5 / P4.5)

```bash
gcc -std=c11 -Wall -Wextra -O2 \
  -o build/cdfs_suite tools/cdfs_suite.c port/cdfs/cdfs.c -Iport/cdfs
./build/cdfs_suite --self-test /tmp/cdfs-self
# Packed PC or SD tree (same relative layout):
./build/cdfs_suite --root runtime/us
./build/cdfs_suite --root /media/…/switch/dw3/us
```

`tools/tests/test_cdfs_suite.py` always runs `--self-test` (map load, path
reject, read / `read_at` / LBA). It also runs `--root` when `runtime/us`,
`runtime/eu`, `DW3_CDFS_ROOT`, or `DW3_SD_RUNTIME` points at a packed tree.
Details: `docs/P4.5_STATUS.md`.

## Owner data (skipped in CI)

When `runtime/us/` exists locally (own disc, never committed), spot-check a
few ids by comparing `cdfs_cat` output to the packed files, or run
`cdfs_suite --root runtime/us`. CI must not require `disks/` or `runtime/`.
