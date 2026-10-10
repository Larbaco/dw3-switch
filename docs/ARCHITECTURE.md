# Architecture

## Sources of truth

| Layer | Location | Role |
|---|---|---|
| Game logic (matching C) | `vendor/dw3_decomp/src` | PS1 game code; keep as upstream, patch via port glue |
| PsyQ headers (reference) | `vendor/dw3_decomp/external/psyq_headers` | API surface the game expects |
| Host (Switch) | `platforms/switch` | libnx entry, present, audio, input, FS |
| Port glue | `port` | Overlay loader, CD→FS, memcard, build integration |

## Runtime model (target)

```
+-----------------------------------------+
|  Switch host (libnx)                    |
|  present / audio / pad / fs / time      |
+-------------------+---------------------+
                    |
+-------------------v---------------------+
|  Psy-Q compatible shim                  |
|  libgpu / libgte / libspu / libcd / ... |
+-------------------+---------------------+
                    |
+-------------------v---------------------+
|  Digimon World 3 game C                 |
|  engine modules + overlays + stages     |
+-----------------------------------------+
```

## Hard constraints from the decomp

- PsyQ libraries in the decomp are still MIPS disassembly; they are **not**
  linked into the Switch binary. The shim replaces them.
- Overlays and stages originally load at fixed PS1 addresses. The port must
  provide an equivalent loader or a single linked image with relocatable
  modules.
- Disc I/O is sector-oriented (`cd_reader`). Unpack once with the decomp's
  `tools/extract_disc.py`, then serve files from sdmc/romfs.

## Non-goals (for now)

- Byte-identical matching builds on Switch
- Shipping any original assets in git
- Official eShop packaging
