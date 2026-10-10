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

Related PC recomp catalog (not a dependency of this port): the public
[Xive080/D-W-3-Recomp](https://github.com/Xive080/D-W-3-Recomp) repo hosts
the OpenWorld2003 launcher mod index under `source/*.json`. That index is a
convenient inventory of the same community work; their patches apply to a
statically recompiled MIPS image and **do not** load into our `.nro`.

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

## Path legend (our port)

| Path | Meaning |
|---|---|
| **data (P7.2)** | Owner-supplied file / archive change survives extract→pack; no EXE logic |
| **native (P7.4)** | Needs C / `port/patches/` against the decomp; binary IPS-style patches are useless here |
| **host** | PC/Switch chrome only (`platforms/*`): import tools, overlays UI, not game logic |
| **Phase 6** | Timing / language / polish already on the roadmap (esp. P6.2) |
| **skip / later** | Nice-to-have, Archipelago-only, or Recomp-launcher-only; not Phase 7 gate |

Evidence column: OpenWorld2003 `patcher.json` `target` (when present), or the
loose file they ship under `source/<author>/`.

## OpenWorld2003 catalog → Phase 7 mapping

Snapshot of
[Xive080/D-W-3-Recomp `source/`](https://github.com/Xive080/D-W-3-Recomp/tree/main/source)
(mod ids as published). Re-check after they add sources. This feeds P7.3 but
does **not** finish P7.3: the combined QoL patch beta still needs its own
pass.

### Flawe

| Mod | What it does | Their delivery | Our path | Notes |
|---|---|---|---|---|
| FastTravel | Warp from map to visited areas | Full `STSTATUS.PRO` replace + `disc_map` | **native (P7.4)** | Overlay logic + UI; credit Flawe |
| BattleSpeedUp | Shorter battle filler text / auto-close | Full `FIGHTST2.PRO` replace | **native (P7.4)** | Battle overlay; may overlap combined-patch battle text |
| Walkthrough | Next-step guide in main menu | `ESSTATUS.BIN` replace | **data (P7.2)** first | Strings/table asset; if menu hook is code-only, add a thin **native** glue |

### markisha64 (game patches)

| Mod | What it does | Targets (patcher) | Our path | Notes |
|---|---|---|---|---|
| NTSC | PAL build at 60 Hz | `SLES_039.36` | **Phase 6** (P6.2) + verify | Not every scene is 60 fps; pair with host timing |
| FastText | Hold-X turbo for textboxes | `STPLNMET.PRO`, `SLES_039.36` | **native (P7.4)** | Small; high UX value |
| FastStart | Skip intro / startup sequences | `STDWTITL.PRO`, `STPLNMET.PRO`, stage PROs | **native (P7.4)** | Called out in P7.4 scope |
| FixedFields | Field moves via Thunder Field fix | `FIGHTSTG.PRO` | **native (P7.4)** | Bugfix class; consider upstream-bound if match-neutral |
| ImprovedHPProxy | %-based HP Proxy damage cut | `FIGHTSTG.PRO`, `SLES_039.36` | **native (P7.4)** | |
| UncappedDigivolutionExp | Remove digivolution EXP cap | `STFGTREP.PRO` | **native (P7.4)** or **data** if only table constants | Confirm in decomp before choosing |
| CardBattleDisable | Disable card battles | `SLES_039.36` | **native (P7.4)** / **skip** | Challenge flag; low priority for gate |
| DisableScriptItems | No script item/BITS pickups | `SLES_039.36` | **native** / **skip** | Randomizer/Archipelago adjacent |
| FishingKickingDisable | No fishing/kicking | `FIELDSTG.PRO` | **native** / **skip** | |
| ForcedEncountersDisable | No forced encounters | `FIELDSTG.PRO` | **native** / **skip** | |
| RandomEncountersDisable | No random encounters | `SLES_039.36` | **native** / **skip** | |
| NoRunningAway | Escape always fails | `FIGHTSTG.PRO` | **native** / **skip** | |
| NoCounterCrest | Remove Tuskmon Counter Crest drop | `SDIGIEDT.PRO` | **data** or **native** | Likely drop table |
| PostGameUnlock | Post-game fights without legendary weapons | `SLES_039.36` | **native (P7.4)** | |
| FastAdminCenter | Skip Staff Pass quest | `WSTAG325/620.PRO` | **native** / **skip** | Stage script skip |
| FastBaronmon | Skip Phoenix Bay TNT Chip trip | `WSTAG375.PRO` | **native** / **skip** | |
| FastSepikmon | Skip Sepikmon Jungle Grave quest | `WSTAG480.PRO` | **native** / **skip** | |
| FolderBagCutsceneSkip | Skip folder-bag cutscene | `WSTAG270.PRO` | **native** / **skip** | |
| APSepikMask | Sepik Mask via Gatomon (Archipelago) | `WSTAG475.PRO` | **skip / later** | Archipelago-specific |
| DRIUnlock | DRI unlock | `WSTAG330.PRO` + `objects.json` | **native** / **skip** | |

### markisha64 / others (launcher tabs, not game EXE)

| Mod | What it does | Our path | Notes |
|---|---|---|---|
| DigiRandom | Randomizer → writes a mod package | **skip / later** | Algorithm exists upstream (MIT); native port is a project of its own |
| DigiLab (JulioGawl) | Edit enemy DVEXP / EXP / BITS | **data (P7.2)** + optional **host** tool | Offsets documented against loose `STFGTREP.PRO`; owner-edited runtime file, or a small editor in `tools/` |
| Digivice (Jeanheck) | Companion overlay (party/quests) | **host** / **P7.5** | On Recomp it is UI over game memory; for us, closer to companion bridge than disc patch |
| Port .mcd (Xive080) | Import emulator/real card saves | **host** (overlaps P2.7 / P5.3) | We already want `.mcr` import/export; no game-logic patch |
| Time Stranger UI (Xive080) | 8-bit UI reskin | **data (P7.2)** / **skip** | Asset/UI pack; Recomp marks it unstable |

### RichardPacco / MestreBD

| Mod | What it does | Our path | Notes |
|---|---|---|---|
| Exp Share | Pause-menu Exp Share modes; digivolution EXP split | **native (P7.4)** | Explicitly in P7.4 scope; credit RichardPacco |
| ReColor - Amaterasu | Recolour cast / Digimon textures | **data (P7.2)** | Large texture `patcher.json`; owner runtime assets only |

### Combined QoL patch / guiomatos (from announcement; not in Recomp index)

| Feature (announcement) | Likely path | Status |
|---|---|---|
| Share EXP / battle–progression systems | **native (P7.4)** | Overlaps Exp Share; confirm against a pinned beta |
| NPC bubbles / quest log HUD | **native (P7.4)** | |
| Fast Start / starter pack UI (guiomatos) | **native (P7.4)** | Starter UI not in OpenWorld2003 `source/` as a separate mod |
| Multilingual walkthrough strings | **data (P7.2)** | Aligns with Flawe Walkthrough |
| NTSC / timing | **Phase 6** | Aligns with markisha64 NTSC |
| DW3-DS-Android companion | **P7.5** (blocked until protocol) | Distinct from Digivice-on-Recomp |

## Suggested Phase 7.4 subset (first pass)

Agree and pin before coding. Proposed gate subset (high value, already
credited upstream, not Archipelago-only):

1. NTSC / EU timing parity (via P6.2, verified on host)
2. FastStart
3. FastText
4. Exp Share (RichardPacco)
5. FastTravel (Flawe)
6. BattleSpeedUp or equivalent battle-text pacing (Flawe / combined patch)
7. FixedFields + ImprovedHPProxy (markisha64 bugfix class)

Everything else in the tables stays checklist backlog until that subset
ships. DigiLab-as-owner-data and Walkthrough-as-data can ride P7.2 without
blocking P7.4.

## Binary patch vs native code (summary)

| Kind | Examples | Likely path |
|---|---|---|
| Disc / archive data | Walkthrough `ESSTATUS.BIN`, ReColor textures, DigiLab table edits | Patched EU tree or owner runtime files → P7.2 |
| Full overlay replace | Flawe `STSTATUS.PRO`, `FIGHTST2.PRO` | Native reimplementation (P7.4); cannot `memcpy` their PRO into `.nro` logic |
| IPS-style EXE/PRO edits | Almost all markisha64 `patcher.json` `changes` | Native (P7.4) |
| Video timing flags | NTSC | Phase 6 / P6.2 |
| Launcher-only / companion UI | Port .mcd, Digivice, DigiRandom tab | Host and/or P7.5; not disc patches |
| External app | DW3-DS-Android | P7.5 if protocol exists |

## Layering reminder

- Runtime packs and patched trees: owner disk / SD layout — **not** git.
- Remap, loaders, parity patches: `port/`.
- Switch/PC hosts stay thin; no QoL game logic in `platforms/switch/`.
- Do not vendor another team's patch files into this repository.
- OpenWorld2003 / psxrecomp remain a reference for behaviour and UX, not a
  submodule or code source for the Switch image.

## Out of scope until asked

- Shipping or hosting any patch or ISO
- Claiming endorsement by the mod authors
- Matching every beta feature on day one of Phase 7 (parity is a checklist,
  not a single drop)
- Vendoring or linking the OpenWorld2003 / psxrecomp engine
