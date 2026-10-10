# Port options menu

Phase 9 goal note. See [ROADMAP.md](ROADMAP.md) P9.1–P9.5.

Vanilla Digimon World 3 / 2003 menus stay as the decomp implements them.
Anything this port adds that **did not exist on the original PS1 game**
(host polish, native QoL parity, texture packs, fast-forward, etc.) should be
reachable from a **port options** entry on the title / main menu, with
per-feature toggles and defaults that preserve original behaviour.

## Scope

| In scope | Out of scope |
|---|---|
| Toggle port-only features | Replacing or restyling every vanilla menu |
| Persist options on owner storage (PC dir / `sdmc:/switch/dw3/…`) | Shipping a default “modded” experience with no vanilla path |
| Same menu on PC and Switch hosts | Requiring a companion app to change settings |
| Clear labels: feature → which phase/doc owns it | Mixing port flags into the PS1 `.mcr` by default |

EU `CNTY_SEL` / `LANGUAGE` and US/EU disc selection are **original** (or
original-adjacent) flows — expose them where the game already does (see
P6.2). The port menu covers **port additions**, not a second language
select unless we add a port-only override.

## Layering

| Layer | Responsibility |
|---|---|
| Title / main menu hook | Entry point into port options (minimal glue; prefer `port/` over editing decomp in place) |
| `port/` | Option registry, defaults, apply-on-boot / apply-on-change |
| Owner config file | JSON or similar under the runtime layout — gitignored |
| `platforms/*` | Input/present only; no feature policy in the Switch tree |

## Toggle registry (seed)

Populate as features ship. Every non-native feature should land here or be
explicitly marked N/A in this note.

| Feature area | Examples | Roadmap |
|---|---|---|
| Display / timing | Integer scale, widescreen, NTSC host options beyond stock | Phase 6 |
| Convenience | Fast-forward / frame-skip | Phase 6 |
| Localization | Active language pack (if port-selected) | Phase 6 |
| Community QoL parity | Share EXP, quest log HUD, NPC bubbles, Fast Start, … | Phase 7 |
| Texture packs | Enable overlay / select pack name | Phase 8 |
| Upscaling research | FSR-class / other present upscale if a spike ships (default off; may stay N/A) | Phase 11 |

Defaults: **off** or equivalent to stock PS1 behaviour unless a feature is
purely host presentation with no gameplay impact (document exceptions).

## Persistence

- Path TBD with P1.6 layout (e.g. `config/port_options.json` next to
  runtime data).
- Load before game systems that read the flags; save on confirm or on
  toggle (pick one in P9.4 and stick to it).
- Do not write port options into the memcard image unless the user opts
  into an explicit “embed in save” experiment later.
