# Host overlays (Phase 2)

## E3 / P2.3 — address symbols

`host_vram.c` provides `OVERLAY_VRAM`, `STAGE_VRAM`, `OVERLAY_ADDRESS`, and
`SUB_OVERLAY_ADDRESS` for the host engine build. Sizes are padded US regions
(256 KiB each).

## E5 / P2.6 — static overlay model

On PS1, `loadModeOverlay` / `loadSubOverlay` `memcpy` a `.PRO` into fixed
VRAM **only when the tracked id changes**. libnx has no `dlopen`, and the
port links game C into one image, so “load” becomes:

| Event | Host action |
|---|---|
| Mode / sub file id **changes** | Restore that overlay’s initial `.data`, **zero** its `.bss` |
| Same id again | No-op — overlay globals **preserved** |

`overlay_host.c` implements `dw3_overlay_restore`. Patch
`0008-PORT_HOST-static-overlay-restore.patch` wires the decomp loaders to
it. Descriptor table: `{file_id → entry, data, data_init, data_size, bss,
bss_size}`.

Host zeros `.bss` on id change even though the PS1 CD image does not clear
it after a `memcpy`; that keeps the static model deterministic.

### Symbol-prefix strategy: **generated headers** (not objcopy)

**Choice for this port: generated `#define` / wrap headers per overlay or
stage TU**, not `objcopy --prefix-symbols`.

Reasons:

1. Overlays and stages are compiled from **C sources** (decomp), not from
   prebuilt MIPS objects. Prefixing at the preprocessor / compile unit
   boundary fits the patch-copy build.
2. Engine tables (`MODE_ENTRY_POINTS`) must keep **stable unprefixed
   entry names** (`STDWTITL_start`, …). Generated headers can rename
   colliding globals (`setupStage`, `stageObjects`, …) while leaving the
   exported `*_start` symbols alone. `objcopy --prefix-symbols` would
   rename everything and force a second redefine/alias pass.
3. Stage files share identical global names across hundreds of TUs; a
   per-stage generated header (or compile definition) is the
   straightforward way to uniquify them in one linked image.

`objcopy --prefix-symbols` remains a fallback if we ever ingest opaque
`.o` blobs; it is not the Phase 2 default.

E6 will add the generator (or hand-maintained wrap headers) when more
than the title overlay is linked. Until then the restore API and US
title table stub are in place.

### Title-path overlay list (US)

| Overlay | File id (US) | Needed for title? |
|---|---|---|
| `STDWTITL` | `0x1C1` | **yes** — title mode |
| `CNTY_SEL` | `0x157` | **no** for US (country select is EU-oriented; skip unless a path requires it) |

`overlay_table_us.c` registers `STDWTITL`. Full `.data` / `.bss` snapshots
for that overlay land when it is linked in E6 / P2.8.

### Host test

```bash
# from repo root
python3 -m pytest tools/tests/test_overlay_host.py -v

# or directly:
mkdir -p build
gcc -std=c11 -Wall -Wextra -Werror -O2 -DPORT_HOST=1 \
  -o build/overlay_host_test tools/overlay_host_test.c \
  port/overlays/overlay_host.c -Iport/overlays
./build/overlay_host_test
```

Fixtures prove: two different overlays reset; the same overlay twice
preserves mutated `.data` / `.bss`.

## Boot TIM

On PS1 the boot TIM is the packed exe tail at `OVERLAY_VRAM`, read via
`SUB_OVERLAY_ADDRESS`. Host `main` stubs that upload until
`runtime/us/boot_tim.bin` is loaded (see `docs/RUNTIME_LAYOUT.md`; wire in E6).
