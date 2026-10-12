# port/memcard — flat save host backend

Each memory-card **port** persists as one flat file under the memcard
directory. The file holds the bytes `MemCardReadFile` / `MemCardWriteFile`
already address for Digimon World 3 (4 blocks = 32 KiB: header, icons,
info, three slots). That image stays **resident in RAM** after open; sector
calls become `memcpy`. The running port does **not** import or export
emulator cards. Interchange with DuckStation or another emulator is an
external tool, roadmap **P9.6**, and is not linked into the game.

## On-disk layout

| Slot | Flat save | Name sidecar (optional) | Legacy (migration only) |
|------|-----------|-------------------------|-------------------------|
| 0 (port A / `bu00`) | `save0.bin` | `save0.name` | `memcard0.mcr` → `memcard0.mcr.bak` |
| 1 (port B / `bu10`) | `save1.bin` | `save1.name` | `memcard1.mcr` → `memcard1.mcr.bak` |

- Flat size: **32768** bytes (`0x8000`).
- `saveN.name` is a short text sidecar with the product file name
  (e.g. `BASLUS-01436DMW3-USA`) written on create / migration.
- Fresh ports create **no** file until `MemCardCreateFile` /
  `MemCardWriteFile` auto-create. An empty directory is a valid empty card.
- Set the directory with `memcard_host_set_dir(...)`.

### One-time migration from `.mcr`

If `saveN.bin` is missing (or present but the wrong size — quarantined as
`saveN.bin.bad`) and `memcardN.mcr` exists, `memcard_host_ensure` scans
all directory frames for a Digimon World 3 product name
(`BASLUS-01436DMW3-USA`, `BESLES-03936DMW3-EUR`, or `BISLPS-03446DMW3-JPN`),
extracts that file via the BIOS next-block-minus-1 link, writes
`saveN.bin` (+ sidecar), and only then renames the image to
`memcardN.mcr.bak`. Other games' first-block entries are ignored. If no
DW3 name matches, the `.mcr` is left untouched and no flat save is
created.

Do **not** put a frame XOR on data-frame byte `0x7F`. That byte is game
data (e.g. partner index high byte). Flat saves never apply frame XOR.

### Flush policy

- `MemCardWriteFile` only dirties RAM.
- The first non-write `MemCard*` call after a write burst (Exist / Accept /
  Read / GetDirentry / Create / Format), or `memcard_host_shutdown`,
  performs **one** rename-atomic write of the whole `saveN.bin`
  (`saveN.bin.tmp` → remove dest → rename; required on FAT/exFAT).
- Hosts register `atexit(memcard_host_shutdown)` and also call shutdown if
  `dw3_game_main` returns. Abrupt HOME kill still skips that path; see
  [docs/P5.3_FLAT_SAVE.md](../../docs/P5.3_FLAT_SAVE.md).

### Synthetic directory

`MemCardGetDirentry` returns one entry with `size = 0x8000` when a flat
save exists so `STGMCARD_askError` space checks stay valid.
`MemCardExist` / `MemCardAccept` do not scan directory frames.

## API surface

| Layer | Role |
|---|---|
| `memcard_host_*` | Flat RAM image, flush, migration; legacy `.mcr` sector helpers for fixtures |
| `psyz_card_wrap.c` | `-Wl,--wrap=_card_read/_card_write` → legacy `.mcr` only (STGMCARD uses `MemCard*`) |
| `memcard_libmcrd.c` | Host `MemCard*` on the flat RAM image |

Game code (`vendor/dw3_decomp` `src/engine/memcard/memcard.c`) talks
PSY-Q **`MemCard*`**. This port keeps that API and no longer walks a live
128 KiB card for save/load.

LP64 note: patch `0028` keeps `MemCard` + `MemCardFuncs` in one
`g_dw3_memcard` object so `MEMCARD_SYSTEM.funcs` is not misaligned.

STGMCARD panel: patch `0043` sets host `MEMCARD_LOAD_FRAMES` /
`MEMCARD_SAVE_FRAMES` to **100** (PS1 US stays 1239/1266) so the bar
tracks flat sector pacing; see [docs/P5.3_FLAT_SAVE.md](../../docs/P5.3_FLAT_SAVE.md).

## Build (host CLIs)

```bash
mkdir -p build
gcc -std=c11 -Wall -Wextra -O2 \
  -o build/memcard_rw tools/memcard_rw.c port/memcard/memcard_host.c \
  -Iport/memcard

gcc -std=c11 -Wall -Wextra -O2 \
  -o build/memcard_flat_test tools/memcard_flat_test.c \
  port/memcard/memcard_libmcrd.c port/memcard/memcard_host.c \
  -Iport/memcard
```

`tools/tests/test_memcard.py` builds those CLIs and checks flat
create/write/read, deferred flush, `.mcr` migration, plus legacy sector
I/O against `tools/testdata/memcard/`.

`tools/memcard_chain_test.c` remains as a legacy directory-link helper for
optional offline `.mcr` inspection; the pytest suite uses
`memcard_flat_test` for the runtime path.
