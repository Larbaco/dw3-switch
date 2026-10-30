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
  `dw3_game_main` returns. On Switch, HOME / `ExitRequested` also runs
  `memcard_host_flush_all` from the vsync applet drain before teardown;
  see [docs/P5.3_FLAT_SAVE.md](../../docs/P5.3_FLAT_SAVE.md).

### Synthetic directory

`MemCardGetDirentry` returns one entry with `size = 0x8000` when a flat
save exists so `STGMCARD_askError` space checks stay valid.
`MemCardExist` / `MemCardAccept` do not scan directory frames.

### Cross-region import (P6.25 slice 1)

`save_convert.c` converts a 32 KiB card image between the US and EU slot
layouts (pure functions, no I/O). When a port has no `saveN.bin`,
`memcard_host_ensure` imports the other region's `saveN.bin` from the folder
set with `memcard_host_set_cross_import` / `..._sibling` /
`memcard_host_setup_cross_import`: converted in memory, written as this
port's `saveN.bin` (no `.name` sidecar), `saveN.import` marks it done. The
source is only read. Saving stays native. See
[docs/CROSS_REGION_SAVES.md](../../docs/CROSS_REGION_SAVES.md#as-built-slice-1).
`tools/save_convert_tool.c` is the CLI used by the tests (`convert` never
overwrites its output or its input).

### Shared store (P6.25 slice 2, opt-in)

`save_store=1` in `port_options.cfg` (default 0; `DW3_SAVE_STORE=0|1` overrides
one run) serves ports 0 and 1 from `card0.dw3s` / `card1.dw3s` in the profile
folder (`saves/<uid>/` on the Switch, `runtime/saves/<profile>/` on the PC)
instead of `saveN.bin`. `save_store.c` is the pure part (container with CRC32 and
per-slot origin metadata, canonical EU layout, canonical <-> native per region,
first-fit import of the legacy cards); `memcard_host.c` does the I/O: open
(`save_store_to_native`), flush (`save_store_from_native`, only the slots the game
wrote), migration (`imported.txt`), a timestamped backup next to the card before
its first overwrite of a session, and `dw3-store:` log lines for every decision.
`saveN.bin`, `saveN.name`, `saveN.import` and `.mcr` files are only read.
`autosave.bin` is not part of it. Off (the default) is byte for byte slice 1. See
[docs/CROSS_REGION_SAVES.md](../../docs/CROSS_REGION_SAVES.md#as-built-slice-2).

## API surface

| Layer | Role |
|---|---|
| `memcard_host_*` | Flat RAM image, flush, migration; legacy `.mcr` sector helpers for fixtures; the opt-in shared store (`memcard_host_setup_store`) |
| `save_convert.c`, `save_store.c` | Pure functions: US <-> EU slot conversion; the `.dw3s` container, canonical <-> native, first-fit import |
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
  -o build/memcard_rw tools/memcard_rw.c port/memcard/memcard_host.c port/memcard/save_convert.c port/memcard/save_store.c \
  -Iport/memcard

gcc -std=c11 -Wall -Wextra -O2 \
  -o build/memcard_flat_test tools/memcard_flat_test.c \
  port/memcard/memcard_libmcrd.c port/memcard/memcard_host.c port/memcard/save_convert.c \
  port/memcard/save_store.c -Iport/memcard
```

`tools/tests/test_memcard.py` builds those CLIs and checks flat
create/write/read, deferred flush, `.mcr` migration, plus legacy sector
I/O against `tools/testdata/memcard/`.

`tools/memcard_chain_test.c` remains as a legacy directory-link helper for
optional offline `.mcr` inspection; the pytest suite uses
`memcard_flat_test` for the runtime path.
