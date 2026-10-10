# port/memcard — 128 KiB PS1 `.mcr` host backend

Persists each memory-card **slot** as a raw 128 KiB PlayStation card image
on disk so saves stay DuckStation-importable.

## On-disk layout

| Slot | Default filename |
|------|------------------|
| 0 (port A / `bu00`) | `memcard0.mcr` |
| 1 (port B / `bu10`) | `memcard1.mcr` |

- Size: **131072** bytes (1024 × 128-byte frames).
- Format: raw PS1 card image (same payload DuckStation accepts as `.mcr` /
  `.mcd` — no DexDrive / VGS wrapper header).
- Fresh images use the DuckStation-style empty format: `"MC"` header frame,
  free directory frames, broken-sector list, write-test frame; data area
  filled with `0xFF`.

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
known sector offset against `tools/testdata/memcard/`.

## Owner DuckStation round-trip (manual)

CI does **not** run DuckStation. Owner follow-up when convenient:

1. Copy a `memcard0.mcr` written by this host into DuckStation’s memory-card
   path (or use Import).
2. Confirm Digimon World 3 sees the save (or at least a valid empty card).
3. Export / copy a card from DuckStation back over `memcard0.mcr` and confirm
   the PC host still reads the expected bytes at the same offsets.
