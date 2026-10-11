# Architecture

## Sources of truth

| Layer | Location | Role |
|---|---|---|
| Game logic (matching C) | `vendor/dw3_decomp/src` | PS1 game code; keep as upstream, patch via port glue |
| PsyQ headers (reference) | `vendor/dw3_decomp/external/psyq_headers` | API surface the game expects |
| Psy-Q shim | `vendor/psyz` (pinned submodule) | PSY-Z: libgpu/libgte/libgs/libcd/libsnd/libpress/libmcrd in portable C; see `docs/PSYZ.md` |
| Host (PC) | `platforms/pc` | PSY-Z SDL3 host: links engine + `STDWTITL` + cdfs/memcard; debugging target |
| Host (Switch) | `platforms/switch` | Native libnx PSY-Z backend (`psyz_backend/`); P4.2 EGL+GLES2 present (`nx_gpu.c` / `gpu_hello`); letterbox uses the PC display-range aspect (4:3 at full framebuffer height, side bars on 16:9); audio/input/FS later |
| Port glue | `port` | Overlays, cdfs, memcard, PSY-Z gap stubs, decomp patches (`build/decomp-src`) |
| Disc → runtime | `tools/` + `docs/DISC_SETUP.md` / `docs/RUNTIME_LAYOUT.md` | `build_file_map.py` / `pack_runtime.py`: owner disc → `runtime/<ver>/` (PC) or SD layout |

## Runtime model (target)

```
+------------------------------+   +------------------------------+
|  PC host (SDL3, PSY-Z's own) |   |  Switch host (libnx backend) |
|  present / audio / pad / fs  |   |  present / audio / pad / fs  |
+--------------+---------------+   +--------------+---------------+
               |                                  |
+--------------v----------------------------------v---------------+
|  PSY-Z (Psy-Q compatible shim)                                  |
|  libgpu / libgte / libgs / libcd+St / libsnd / libpress / libmcrd |
+--------------------------------+--------------------------------+
                                 |
+--------------------------------v--------------------------------+
|  port/ glue: overlay table, cdfs (file id -> path), memcard file |
+--------------------------------+--------------------------------+
                                 |
+--------------------------------v--------------------------------+
|  Digimon World 3 game C (vendor/dw3_decomp, patched at build)   |
|  engine modules + 21 overlays + stages, one linked image        |
+-----------------------------------------------------------------+
```

## Hard constraints from the decomp

See [SURVEY.md](SURVEY.md) for the evidence behind each point.

- PsyQ libraries in the decomp are MIPS disassembly; they are **not**
  linked into the port. The shim replaces them. The surface the game uses
  is small (~120 functions) but includes libsnd (SEQ/SEP/VAB) and MDEC.
- Overlays and stages load at fixed PS1 addresses by `memcpy`, only when
  the id changes. The port links everything into **one image** with
  per-binary symbol prefixes and emulates the load by restoring the
  overlay's initial `.data`. libnx has no `dlopen`, so this is also the
  Switch-compatible choice.
- The game is 32-bit code: literal struct sizes, `u32` holding pointers and
  heap-address comparisons must be fixed for AArch64 (`port/patches/`,
  upstream when match-neutral).
- Disc I/O is by **numeric file id** over a sector table, not by path.
  The port serves files from an unpacked tree through an id → path map
  generated from the owner's disc.
- The saved part of `GameState` has no pointers, so PS1 save format
  compatibility (`.mcr`) is a goal, not a stretch.
- MIPS-only constructs (P2.3 done for engine host): scratchpad stack
  no-ops, `allocMemHigh` returns the pointer, GTE macros → PSY-Z software
  GTE, overlay address symbols + static restore in `port/overlays/`
  (P2.6: restore `.data` / zero `.bss` on id change).
- P2.8: PC host calls `dw3_game_main` after cdfs/memcard/boot TIM init;
  `CD_READER` via cdfs; US `STDWTITL` linked; title mode draws. Later
  picture work is in `docs/P3_GPU_GLITCHES.md`. See
  `docs/P2.8_TITLE_BOOT.md`.
- P3.2 (in progress): field/stage bring-up uses **objcopy / `ld -r`** to
  prefix stage globals and rename `.data`/`.bss` into per-overlay sections
  (`dw3data_*` / `dw3bss_*`); `overlay_sections.c` snapshots them for
  restore. Weak mode stubs omit linked overlays (`FIELDSTG`, `STPLNMET`,
  battle P3.3, menus P3.4).

## Non-goals (for now)

- Byte-identical matching builds on Switch
- Shipping any original assets, BIOS, or third-party patches in git
- Official eShop packaging
- Running PS1 EXE/overlay binary patches inside the `.nro` (community QoL
  parity is a later native/`port/` effort; see [COMMUNITY_QOL.md](COMMUNITY_QOL.md))
- Shipping texture packs or original art (owner overlays only; see
  [TEXTURE_PACKS.md](TEXTURE_PACKS.md))
- Shipping a forced “modded” default: port-added features are opt-in via
  the Phase 9 options menu ([PORT_OPTIONS.md](PORT_OPTIONS.md))
- Duplicating Switch host work in Phase 10 that already belongs to P4–P6
  (present, pad/rumble, saves, scale/widescreen, turbo); see
  [SWITCH_FEATURES.md](SWITCH_FEATURES.md)
