# P3.7 Opening movies — design

Date: 2026-10-10  
Status: draft for owner review  
Branch target: `phase3-pc-bringup` (PSY-Z submodule bump intentional when needed)  
Related: `docs/P3.7_MOVIES.md`, `docs/ROADMAP.md` § P3.7, PSY-Z pin `docs/PSYZ.md`

## Goal

Opening STR movies on the PC host play with XA audio and can be skipped
with Start (or end naturally), through PSY-Z libpress + CD stream paths,
fed from packed runtime `.STR` files (P1.6).

ROADMAP done-when (all required together — no partial tick):

1. **Play** — frames from the opening movie decode and present.
2. **XA** — movie audio is audible on the host audio path.
3. **Skip** — Start during playback (or last frame) leaves the player and
   advances to the mode the game requests (typically `MODE_TITLE`).

Default bring-up without `DW3_START_MODE` remains `MODE_TITLE` (patch
`0010`) until this done-when is met; do not remove that skip as a
substitute for real playback.

## Non-goals

- Fake host story-skip past `0xE01`/`0xE02` to close the Phase 3 gate
  (gate may use movie path once this works; gate itself is separate).
- P3.6 SEP / `_SsSeqPlay` title-field music (XA is CD stream, not SEP).
- DuckStation oracle (P3.9).
- Switch / Phase 4 backends.
- Requiring cue/bin mounted for the primary bring-up path (file-backed
  STR is the port bridge; cue/bin may remain optional later).

## Context (current blockers)

Documented in `docs/P3.7_MOVIES.md`:

| Gap | Where |
|---|---|
| Soft MDEC under `MDEC_*` | **Done (Wave 1)** — `libpress.c` → `Psyz_Mdec*`; pin `docs/PSYZ.md` |
| `DecDCTvlc` stub | `libpress.c` still `NOT_IMPLEMENTED`; `DecDCTvlc2` / `Build` port no-ops |
| `StCdInterrupt` stub | `vendor/psyz/psyz/src/psyz/libcd.c` |
| No track at movie LBA | packed `runtime/.../AAA/STR/*.STR` is 2048-byte extracts, not a cue track |
| Audio device | `Psyz_AudioInit` must succeed for XA validation |

US opening: `STDWTITL_movies[0]` → `AAA/STR/MOVIEOPN.STR` (file id / LBA via
`FILE_TABLE`), mode `0xE01`. Second opening (`0xE02`) uses movie index 1
then field `0x207` — same decode/stream stack.

## Architecture

```
STDWTITL_startMoviePlayerTask
  FILE_TABLE.getPos → CdlLOC
  DecDCTReset / StSetRing / StSetStream / CdRead2
       │                    │
       ▼                    ▼
  MDEC_* ──► Psyz_Mdec*   StCdInterrupt + ring
       │                    │
       │                    ▼
       │              port: open STR by file id / path
       │                    feed Mode2/Form2 (or mapped) sectors
       ▼
  DecDCTin/out → framebuffer present (existing GPU path)
  CdlModeRT → XA → SPU / SDL audio (existing host mix)
  PAD_START / endFrame → pause CD, StUnSetRing, requestMode(next)
```

### Layer ownership

| Piece | Owner | Notes |
|---|---|---|
| Wire `MDEC_*` → soft MDEC; real `DecDCTvlc` if available | `vendor/psyz` | Upstream-bound; call out submodule commit |
| `StCdInterrupt` / stream pump that reads sectors into the ring | `vendor/psyz` (+ host tick if needed) | Align with `samples/movie` interrupt cadence |
| Map LBA / file id → `runtime/.../AAA/STR/*.STR` and sector layout | `port/` | Prefer glue here over baking paths into PSY-Z |
| PC present + audio device + any vsync/CD interrupt inject | `platforms/pc` | Thin; no game logic |
| Evidence + ROADMAP sync | `docs/` | Same commit wave as code when status changes |

### Sector / pack format

P1.6 copies 2048-byte user sectors. Prefer a **port adapter** that maps
those payloads into whatever sector view the PSY-Z ring/MDEC path needs
(including synthesizing Mode2/Form2 headers if required). Fall back to
documenting a raw 2352-byte STR requirement only if the adapter cannot
feed a working decode. Record the chosen layout in `docs/P3.7_MOVIES.md`.
Do not invent a second asset pack in git.

## Implementation waves

### Wave 1 — Decode surface

- Replace `MDEC_*` stubs with calls into `Psyz_Mdec*`.
- Unblock `DecDCTvlc` (and related entry points the title movie uses) so
  the decode path is not a hard `NOT_IMPLEMENTED` abort.
- Unit/smoke: existing PSY-Z `test_mdec` if green; optional tiny decode
  fixture without full game link.

### Wave 2 — File-backed stream

- Implement `StCdInterrupt` (or host-driven equivalent) that fills the
  ring from the open stream.
- Port bridge: given movie file id / `CdlLOC`, open the matching `.STR`
  under the runtime root and supply sectors to PSY-Z CD stream APIs.
- Fix host `CdlSetmode` / track-open failures that currently block
  `CdRead2` for Size1/Stream flags when using the file bridge.
- Prove: `DW3_START_MODE=0xE01` advances past INIT into frame delivery
  (even if silent).

### Wave 3 — XA + skip + evidence

- Ensure CD RT / XA path mixes into the SDL audio device; validation
  requires a working audio sink (not “silent success” under dummy-only
  unless XA decode is separately logged and owner confirms hearable
  run).
- Exercise Start-skip and natural end → next mode.
- Update `docs/P3.7_MOVIES.md` with commands, logs, and pass/fail.
- Sync `docs/ROADMAP.md` on `main` for the wave; tick P3.7 **only** when
  play + XA + skip are all evidenced.

## Validation

| Check | How |
|---|---|
| Play | `DW3_START_MODE=0xE01 DW3_RUNTIME=runtime/us ./build/pc/dw3-pc --runtime runtime/us` — visible frames (or framebuffer dump / log proving decoded frames presented) |
| XA | Same run with audio device; hear opening XA (owner listen) + log that RT/XA path is active |
| Skip | Start during play → leave movie task → `MODE_TITLE` (or documented next mode) without hang |
| Default | unset `DW3_START_MODE` → still `MODE_TITLE` bring-up |

Honesty: do not tick ROADMAP P3.7 until all three pass. Partial waves keep
status **blocked** or **in progress** with explicit missing criteria.

## Risks

| Risk | Mitigation |
|---|---|
| Soft MDEC insufficient for real STR bitstreams | Spike early; fall back to stronger decoder or upstream PSY-Z fix |
| 2048 vs 2352 sector mismatch | Adapter in `port/`; document; optional raw track later |
| XA tied to broken CD mode flags | Fix file bridge + `CdlSetmode` together in Wave 2/3 |
| Headless CI cannot “hear” XA | Evidence doc + owner listen; CI may only check decode/stream logs |
| Submodule churn | Small intentional `vendor/psyz` commits; note in commit messages |

## Out of band

Phase 3 gate (start → first boss) and P3.6 remain separate tracks. Once
movies work, the automated intro past `0xE02` becomes unblocked for gate
work without fake skips.
