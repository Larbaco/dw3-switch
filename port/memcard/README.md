# port/memcard — 128 KiB PS1 `.mcr` host backend

Persists each memory-card **slot** as a raw 128 KiB image because the
decomp still speaks the card API. The running port does not import or
export emulator cards. Interchange with DuckStation or another emulator
is an external tool, roadmap P9.6, and it is not linked into the game.

## On-disk layout

| Slot | Default filename |
|------|------------------|
| 0 (port A / `bu00`) | `memcard0.mcr` |
| 1 (port B / `bu10`) | `memcard1.mcr` |

- Size: **131072** bytes (1024 × 128-byte frames).
- Format: raw 131072-byte card image, no DexDrive / VGS wrapper. That is
  the file this library writes today. Opening it in an emulator is P9.6,
  a separate tool, not a job of this library.
- Fresh images use the DuckStation-style empty format: `"MC"` header frame,
  free directory frames, broken-sector list, write-test frame; data area
  filled with `0xFF`.
- Directory bytes 8–9 are the **next block number minus 1** (0..14 means
  blocks 1..15), or `0xFFFF` on the last block. Data for block N starts at
  byte `N * 8192`. Reads and writes both use that BIOS encoding.

A US `BASLUS-01436DMW3-USA` save is 4 blocks with 3 icon frames. Its data
section starts at file offset `0x300` and is `0x26BC` bytes, so it crosses
into block 2. Treating the link halfword as an absolute block number re-reads
block 1 for that tail. `STGMCARD_loadSave` then reports
`checksum fail version=3 got=67 calc=129` on a save whose real XOR is 67
(`Failed to access.`). The intact card loads once the link is decoded as
minus 1.

Byte `0x7F` is a frame XOR only on block 0 (header, directory, broken-sector
list). Data frames keep all 128 bytes. Overwriting that byte on a data frame
replaces the high byte of any field that ends on the sector boundary. The
second slot's partner index is one of those fields; the slot list then
indexes `STGMCARD_partnerIcons` with a huge value and the process faults
back to the launcher. `MemCardWriteFile` leaves data-frame byte `0x7F` as
the game wrote it.

Set the directory with `memcard_host_set_dir(...)`; the host then
reads/writes `<dir>/memcard0.mcr` and `<dir>/memcard1.mcr`.

## API surface

| Layer | Role |
|---|---|
| `memcard_host_*` | Byte/sector I/O on the 128 KiB image |
| `psyz_card_wrap.c` | `-Wl,--wrap=_card_read/_card_write` → `memcard_host_*` + PSY-Z completion events |
| `memcard_libmcrd.c` | Host `MemCard*` (libmcrd) filesystem on the same `.mcr` via `_card_*` |

Game code (`vendor/dw3_decomp` `src/engine/memcard/memcard.c`) talks
PSY-Q **`MemCard*`**. PSY-Z stubs raw `_card_*` without sector payloads;
this port supplies both the wrap and a minimal directory/file layer so
STGMCARD save/load hits real `.mcr` bytes (P3.4 evidence in
`docs/P3.4_MENUS.md`).

LP64 note: patch `0028` keeps `MemCard` + `MemCardFuncs` in one
`g_dw3_memcard` object so `MEMCARD_SYSTEM.funcs` is not misaligned.

## Build (tiny host CLI)

```bash
mkdir -p build
gcc -std=c11 -Wall -Wextra -O2 \
  -o build/memcard_rw tools/memcard_rw.c port/memcard/memcard_host.c \
  -Iport/memcard
```

`tools/tests/test_memcard.py` builds that CLI and checks read/write at a
known sector offset against `tools/testdata/memcard/`. It also builds
`tools/memcard_chain_test.c` and checks a BIOS-style link (`1` means block
2) plus a host write of the same encoding.

## Owner DuckStation round-trip (manual)

CI does **not** run DuckStation. Owner follow-up when convenient:

1. Copy a `memcard0.mcr` written by this host into DuckStation’s memory-card
   path (or use Import).
2. Confirm Digimon World 3 sees the save (or at least a valid empty card).
3. Export / copy a card from DuckStation back over `memcard0.mcr` and confirm
   the PC host still reads the expected bytes at the same offsets.
