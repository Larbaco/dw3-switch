# Community QoL and companion projects

Phase 7 goal note. See [ROADMAP.md](ROADMAP.md) P7.1–P7.5.

This port stays a **native** build of the decomp C over a Psy-Q shim. It does
**not** ship discs, BIOS, or third-party patches. Owners apply patches to
**their** Digimon World 2003 (Europe / `SLES-03936`) copy, then feed the
result through the same extract / file-map / pack pipeline as vanilla.

## Projects in scope

Announced together in the Digimon World 2003 QoL Reddit thread
([r/DigimonWorld](https://www.reddit.com/r/DigimonWorld/comments/1wzc6e0/digimon_world_2003_qol_new_systems_mod_for_psx/)):

| Project | Role | Notes |
|---|---|---|
| Combined **Quality of Life Patch** (beta) | Integrates community mods + new battle, exploration, progression, and menu systems | Owner downloads from that project's GitHub; needs original EU disc |
| **Flawe** Fast Travel / walkthrough / faster battle text | Upstream of several exploration and battle-text changes | Also on romhacking.net (Fast Travel / QoL suite) |
| **guiomatos** Initial Pack Customizer (+ Pack D in the combined build) | Starter team selection UI | Expanded in the combined patch |
| **markisha64** / Marko Grizelj (NTSC / definitive-style fixes) | NTSC 60 Hz timing, field-move / HP Proxy fixes in some builds | See e.g. `dmw2003NTSC`, `dmw_2003_definitive` |
| **DW3-DS-Android** (companion) | Second-screen / DS-style companion intended to complement the QoL patch | Protocol for a native Switch/PC host is TBD (P7.5) |

Walkthrough credit called out upstream: EmeraldPhoenix (underlying Flawe guide).

Exact repository URLs for the combined patch and the Android companion may
move; treat the Reddit post and each project's own README as authoritative
for download and install steps.

## What “compatibility” means here

1. **Patched disc as data source (P7.2)** — owner applies xdelta/PPF (or
   equivalent) to their EU image, extracts into `disks/eu/` (or a clearly
   named variant), regenerates `files_*.json`, packs the runtime layout.
   Asset-only and file-id-stable changes can ride through `port/cdfs`.
2. **Native feature parity (P7.4)** — PS1 EXE / overlay binary patches do
   **not** run inside the `.nro`. Behaviour that lives in MIPS code must be
   reimplemented against `vendor/dw3_decomp` via `port/` glue or
   `port/patches/`, with credit to the original authors.
3. **Companion (P7.5)** — optional. Only if the Android (or other) companion
   documents a port-friendly interface (save export, IPC, etc.). Until then
   it is tracked as blocked, not as a Phase 7 gate dependency.

## Binary patch vs native code (checklist seed)

| Kind | Examples from the QoL announcement | Likely path |
|---|---|---|
| Disc / archive data | Multilingual walkthrough strings, UI text, some menu assets | Patched EU tree → pack pipeline |
| Game logic in EXE/overlays | Share EXP, NPC bubbles, quest log HUD, Fast Start, battle DV/EXP UI, L1/R1 leader swap | Native reimplementation (P7.4) |
| Video timing flags | NTSC 60 Hz / native timing (not every scene at 60 fps) | Overlaps Phase 6 `CNTY_SEL` / NTSC options; verify parity on host |
| External app | DW3-DS-Android companion | P7.5 if protocol exists |

Refine this table when a specific patch version is validated (P7.3).

## Layering reminder

- Runtime packs and patched trees: owner disk / SD layout — **not** git.
- Remap, loaders, parity patches: `port/`.
- Switch/PC hosts stay thin; no QoL game logic in `platforms/switch/`.
- Do not vendor another team's patch files into this repository.

## Out of scope until asked

- Shipping or hosting any patch or ISO
- Claiming endorsement by the mod authors
- Matching every beta feature on day one of Phase 7 (parity is a checklist,
  not a single drop)
