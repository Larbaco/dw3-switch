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
tile goes through `cdfs_read_field_tile`: the bytes are copied immediately,
then the reader stays busy until `cdfs_poll`. `vsyncCallback` calls
`cdfs_poll` once per VSync, so `FIELDSTG_requestTiles` starts one tile
per frame. Full-file and offset-0 reads are not held.

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

## Owner data (skipped in CI)

When `runtime/us/` exists locally (own disc, never committed), spot-check a
few ids by comparing `cdfs_cat` output to the packed files. CI must not
require `disks/` or `runtime/`.
