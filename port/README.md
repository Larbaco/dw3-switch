# Port glue

Platform-independent pieces that sit between the Switch host and the game C
from `vendor/dw3_decomp`:

- CD / file table → host filesystem (`port/cdfs/` — Wave 1 file-id reads)
- Overlay and stage loading
- Memory card virtualization (`port/memcard/` — 128 KiB `.mcr` per slot)
- Build flags that drop matching-only PS1 constraints

See `port/cdfs/README.md` for the file-id API and fixture tests.
See `port/memcard/README.md` for the `.mcr` host backend and DuckStation note.
