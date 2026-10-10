# Architecture

## Sources of truth

| Layer | Location | Role |
|---|---|---|
| Game logic (matching C) | `vendor/dw3_decomp/src` | PS1 game code; keep as upstream, patch via port glue |
| PsyQ headers (reference) | `vendor/dw3_decomp/external/psyq_headers` | API surface the game expects |
| Psy-Q shim | `vendor/psyz` (pinned submodule) | PSY-Z: libgpu/libgte/libgs/libcd/libsnd/libpress/libmcrd in portable C; see `docs/PSYZ.md` |
| Host (PC) | `platforms/pc` | PSY-Z SDL3 bring-up skeleton (`CMakeLists.txt` + stub main); debugging target |
| Host (Switch) | `platforms/switch` | libnx backend for PSY-Z: present, audio, input, FS, log |
| Port glue | `port` | Static overlay table, CD→FS by file id, memcard image, patches to the decomp |
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
  (P2.6: restore `.data` / zero `.bss` on id change; generated-header
  symbol prefixes for colliding overlay/stage globals).

## Non-goals (for now)

- Byte-identical matching builds on Switch
- Shipping any original assets, BIOS, or third-party patches in git
- Official eShop packaging
- Running PS1 EXE/overlay binary patches inside the `.nro` (community QoL
  parity is a later native/`port/` effort; see [COMMUNITY_QOL.md](COMMUNITY_QOL.md))
