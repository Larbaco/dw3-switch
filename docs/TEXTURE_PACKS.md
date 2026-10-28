# Texture pack compatibility

Phase 8 goal note. See [ROADMAP.md](ROADMAP.md) P8.1–P8.4.

This port does **not** ship game textures. Owners keep original disc data
(and optional replacement packs) outside git. Texture packs are an overlay
on the P1.6 runtime layout, resolved through `port/cdfs` (or equivalent),
not a parallel asset pipeline inside `platforms/switch/`.

## What a pack is

- A directory (or archive expanded on SD/PC) that replaces one or more
  files the game already loads by **numeric file id** / path map.
- First target format: **native TIM** (and whatever archives the decomp
  already unpacks) so the engine and PSY-Z path stay unchanged.
- Missing pack entries **fall back** to the owner's vanilla (or QoL-patched)
  runtime tree — a pack must never be required to boot.

## Layering

| Layer | Responsibility |
|---|---|
| Owner data (`runtime/…`, SD layout) | Vanilla extract + optional `texture_packs/<name>/` overlay |
| `port/` | Resolve file id → pack path if present, else vanilla; document remap rules |
| `tools/` | Optional validate/pack helpers against `files_*.json` (P8.4) |
| `platforms/*` | Present whatever GPU/TIM path PSY-Z already has; no pack logic |

## Formats and scaling (P8.3 seed)

| Support tier | Format | Notes |
|---|---|---|
| Required for gate | Drop-in TIM (same id, compatible dimensions/CLUT assumptions) | Least friction with existing loaders |
| Later | Higher-res or PNG/etc. with a convert or decode step | Needs VRAM/emulation budget rules; not the Phase 8 gate |

Exact size/CLUT constraints are filled in when the first pack is validated
against a real title/field asset.

## Beyond textures: modding API (P8.5)

A future goal (2026-10-24) widens this overlay idea to every owner-supplied
replacement (text, audio, data tables) with a `mods/` folder, a manifest, load
order and versioning. Textures stay the first case and P8.2's layout should be
chosen with it in mind. Limits found there: overlays and the executable are
linked into the NRO, so only data loaded by file id can be replaced as a file;
tables compiled into the overlays need a port hook. Mods never carry original
game data. See ROADMAP P8.5. The first concrete mod is the AI-upscaled map:
see [MOD_MAP_UPSCALE.md](MOD_MAP_UPSCALE.md) (pack folder `mods/<pack>/map/`,
manifest, loader plan).

## Out of scope

- Shipping or hosting texture packs or original art in this repository
- Requiring a specific community pack as the default experience
- Rewriting the game into a modern material/shader pipeline “for packs”

## History moved from ROADMAP.md (2026-10-24)

Moved from the ROADMAP.md entries on 2026-10-24, word for word (two leading
spaces removed). "Above" and "below" refer to the old entry. The roadmap
keeps the current state and links here.

**What exists today (cited):** the game reads disc data by **numeric file
id**; `port/cdfs/cdfs.c` maps an id to a path under the runtime root
(`cdfs_set_root`, `cdfs_load_map` reads `files_us.json` / `files_eu.json`,
`cdfs_file_size`, `cdfs_read`, `cdfs_read_at`, `cdfs_read_field_tile`;
relative paths only, `.` and `..` rejected at map load, the on-disk size
wins over the JSON `bytes`; see `port/cdfs/README.md`). The runtime is
`sdmc:/switch/dw3/<us|eu>/` ([RUNTIME_LAYOUT.md](RUNTIME_LAYOUT.md)), next
to `port_options.cfg`. So a **file replacement is one lookup away**: swap the
path for an id, which is the plan in [TEXTURE_PACKS.md](TEXTURE_PACKS.md)
(native TIM first, fall back to the vanilla file when the pack has no entry).
**Hard limit found by the survey:** the overlays and the executable are
**linked into the NRO** (one image, `.data` restored on a mode change;
[ARCHITECTURE.md](ARCHITECTURE.md)), so code and tables compiled into them
(Digimon stats, items, techniques, if they live in C data) do not come from
the SD card, and replacing an overlay file changes nothing. File-id
replacement covers what the game really loads by id (textures, maps, message
files, sound banks, `.STR` movies); **data-table overrides need a port
hook** (a `port/` step that patches the table after the overlay restore,
keyed by table and index), and each table has to be located in
`vendor/dw3_decomp` first. [COMMUNITY_QOL.md](COMMUNITY_QOL.md) says the
same about MIPS patches: they do not run in the NRO.
**Scope ideas, in the order of cost:**
1. *Mod folder:* `sdmc:/switch/dw3/<us|eu>/mods/<mod-name>/` with a small
   manifest (`mod.json`: name, version, region `us` / `eu` / both, API
   version, priority, author, licence) and a tree that mirrors the runtime
   paths or lists file ids.
2. *Asset overrides* by file id with fallback: textures / sprites (TIM first,
   P8.3), text and strings (the EU `LANGUAGE` slots, P6.7), audio (XA and
   SEP / VAB banks; P6.17 already has Music and SFX buses).
3. *Data-table overrides* (Digimon stats, items, techniques) through
   declarative files (JSON or CSV) validated against the table shape.
4. *Load order and versioning:* an order or priority in a `mods.cfg` beside
   `port_options.cfg`, a conflict line in the log (two mods replacing one
   id), an API version a mod must match, and a safe mode that boots with
   mods off.
5. *US against EU:* ids and files differ (`files_us.json` against
   `files_eu.json`, five EU languages); a mod declares its region and the
   loader refuses a mismatch, as the extract does for a wrong disc.
6. *Later, script hooks* (events, battle or field scripts): only after 1 to
   4 are stable, with an allow-list; not scoped here.
**Dependencies and risks:** needs a **stable id-to-path lookup in
`port/cdfs`**: today the game goes through several entry points
(`cdfs_read`, `cdfs_read_at` for map headers and `FILE_CACHE`,
`cdfs_read_field_tile`, and the STR sector path), so the override must be in
one resolve step they all use, or be added to each; P9.4 persistence for a
mods on / off row; the field-tile reader and the file cache expect the size
of the vanilla file, so a larger replacement needs a size check; large packs
on the SD (FAT) cost read time. P8.4 `--check` extends to validate a mod
against `files_*.json`.
