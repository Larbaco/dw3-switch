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
game data. See ROADMAP P8.5.

## Out of scope

- Shipping or hosting texture packs or original art in this repository
- Requiring a specific community pack as the default experience
- Rewriting the game into a modern material/shader pipeline “for packs”
