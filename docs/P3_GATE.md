# Phase 3 gate — start → first boss (PC / US)

## Status (2026-10-10)

**Gate not met.** Do not tick Phase 3 complete.

Automated bring-up on `phase3-pc-bringup` proves early loop pieces and a
forced battle round-trip, then the vanilla intro leaves into
`MODE_OPENING_2` (`0xE02`) and stalls on PSY-Z movie stubs. There is no
honest automated path to the first story boss yet.

Branch tip at evidence time: see git (`phase3-pc-bringup`). Local logs under
`build/p3-gate/` (gitignored).

## Gate criteria (ROADMAP)

> a full playthrough segment (start → first boss) on PC without crashes;
> both versions build.

| Criterion | Status |
|---|---|
| US `dw3-pc` builds / links Phase 3 overlays | **yes** (255 registered overlays) |
| Start → field playable without crash | **partial** (see Proven) |
| Reach / defeat first story boss | **no** |
| EU build claimed | **no** (US only) |
| Audio / movies on the segment | **partial** (P3.7 Done-when met — play/skip + audible XA; video presentation follow-up; P3.6 SEP still open; gate segment not end-to-end) |

## Proven (automated, headless `SDL_VIDEODRIVER=dummy`)

### Title land then movie attempt

Unset `DW3_START_MODE`: boots `MODE_TITLE` (`0xE00`), then title flow
requests `MODE_OPENING` (`0xE01`) → movie path (VLC / stream / XA still
open; `MDEC_*` wired in Wave 1 — see `docs/P3.7_MOVIES.md`).

```bash
SDL_VIDEODRIVER=dummy ./build/pc/dw3-pc --runtime runtime/us
# dw3: mode=0xe00 …
# dw3: mode=0xe01 …  + MDEC_reset/in/out not implemented
```

### New-game intro → name → Chamber → movie wall

```bash
SDL_VIDEODRIVER=dummy \
  DW3_START_MODE=0x2D7 DW3_FAST_SCRIPT=1 DW3_AUTO_PAD=confirm \
  ./build/pc/dw3-pc --runtime runtime/us
```

Observed sequence (`build/p3-gate/newgame_field.log`):

| Step | Mode | Stage / note |
|---|---|---|
| Field start | `0x2D7` | `WSTAG780` file 474 |
| Script door | `0x2D8` | `WSTAG785` file 475 |
| Name entry | `0x500` | `STPLNMET` |
| Back to Online Center | `0x2D8` | 475 |
| Chamber Room | `0x2D9` | `WSTAG790` file 476 |
| **Hard stop** | `0xE02` | `MODE_OPENING_2` → VLC / stream / XA (MDEC wired) |

No further story progress without a movie skip/bridge or owner pad past
this cutscene on a path that does not need decode.

US `STDWTITL_movies[1]` (mode `0xE02` → movie index 1) would next
request field mode `0x207` (`WSTAG225`) after a successful decode. That
landing is proven only via host boot aids below — not through the movie.

### Post-movie Asuka (host boot past `0xE02`)

```bash
# Free walk on the post-movie landing stage (skip event 5)
SDL_VIDEODRIVER=dummy \
  DW3_START_MODE=0x207 DW3_PROGRESS=2 DW3_AUTO_PAD=walk \
  ./build/pc/dw3-pc --runtime runtime/us
# FIELDSTG_pickStage mode=0x207 stageFile=455; moveByPad Δy observed

# Natural progress=1 + FAST event 5 → Asuka City Server (WSTAG210)
SDL_VIDEODRIVER=dummy \
  DW3_START_MODE=0x207 DW3_PROGRESS=1 DW3_FAST_SCRIPT=1 \
  DW3_AUTO_PAD=confirm ./build/pc/dw3-pc --runtime runtime/us
# SCRIPT_LEAVE mode=0x203 → FIELDSTG_pickStage stageFile=453 (WSTAG210)
```

This advances **toward** early Asuka story, not to the first boss. No
automated story flags / wilderness walk / boss fight beyond this.

### Free pad walk (skip intro)

```bash
SDL_VIDEODRIVER=dummy \
  DW3_START_MODE=0x2D7 DW3_PROGRESS=2 DW3_AUTO_PAD=walk \
  ./build/pc/dw3-pc --runtime runtime/us
# moveByPad Δy on WSTAG780 — see docs/P3.2_FIELD.md
```

### Forced encounter → report → field

```bash
SDL_VIDEODRIVER=dummy \
  DW3_START_MODE=0x600 DW3_FORCE_ENCOUNTER=1 DW3_FORCE_WIN=1 \
  DW3_AUTO_PAD=confirm ./build/pc/dw3-pc --runtime runtime/us
# FORCE_WIN → STFGTREP (+XP) → FIELDSTG 0x2d7 / WSTAG780
```

This is **not** the first story boss; it is the P3.3 Done-when aid path
(`docs/P3.3_BATTLE.md`).

### Other Phase 3 Done-whens (not re-run for this doc)

Documented separately and already ticked on main ROADMAP:

| Task | Evidence |
|---|---|
| P3.1–P3.5 | `docs/P3.1_64BIT_AUDIT.md` … `docs/P3.5_CARDGAME.md` |
| P3.8 | 237/237 live PASS; `WSTAG460` stubbed (`docs/P3.8_STAGES.md`) |

## Blocked / not proven

| Item | Why |
|---|---|
| **Start → first boss** | Intro leaves to `0xE02` movie; post-movie Asuka boot aids stop short of any boss |
| **P3.6 Audio** | PSY-Z `_SsSeqPlay` ASM gap (`docs/P3.6_AUDIO.md`) |
| **P3.7 Movies** | **Done-when met** (`docs/P3.7_MOVIES.md`) — play + Start-skip + owner audible XA; video presentation quality still a follow-up |
| **P3.9 Harness** | Needs owner DuckStation oracle + capture fill |
| **WSTAG460** | Host `updateWanderer` SEGV; weak stub only |
| **EU playthrough** | Not in gate claim |

## Hard blockers needing owner / PSY-Z

Autonomy should pause on these; port glue alone cannot close the gate:

1. **PSY-Z `_SsSeqPlay`** (and SEP note-on path) — P3.6 title/field/battle music.
2. **Post-movie intro / story progress** past `0xE01`/`0xE02` toward the
   first boss (P3.7 Done-when met; video presentation quality still a
   follow-up; automated intro wall may remain). Soft `MDEC_*` is wired.
3. **Owner DuckStation** — P3.9 visual oracle / screenshot harness.
4. **WSTAG460** — optional for “all loadable stages”; required if gate
   or P3.8 is reinterpreted as 239/239 including Reliability Spot.
5. **First-boss segment** — either owner pad playthrough once movies/audio
   allow, or a new story-progress aid that reaches the first boss without
   lying about the gate.

## Honesty rule

Do **not** mark Phase 3 gate passed until a start→first-boss segment runs
without crashes on PC (US at minimum). Forced battle / menu / stage-sweep
evidence does not substitute for that segment.
