# Phase 3 gate — start → first boss (PC / US)

## Status (2026-10-10)

**Gate not met.** Do not tick Phase 3 complete.

Movies + SEP unblocked the intro wall: an honest new-game path reaches
`MODE_OPENING_2` (`0xE02`), Start-skips the STR (real movie path, not a
host story fake-skip), lands post-movie Asuka Server (`0x207` → `0x203`).
The first story boss is identified as **Tamer Genji** event battle on
`WSTAG200` (`MODE` `0x200`, progress `3`); with progress aids + empty-party
`setParty(0)` it enters `FIGHTSTG`, can `DW3_FORCE_WIN` through `STFGTREP`,
and returns to field.

Still missing for the gate: one continuous start→Genji segment without
mode-warping past Asuka Lobby / Tamer Service pack selection, and a
non-`FORCE_WIN` (or clearly scoped) boss completion claim. EU not claimed.

Branch tip at evidence time: see git (`phase3-pc-bringup`). Local logs under
`build/p3-gate/` (gitignored).

## Gate criteria (ROADMAP)

> a full playthrough segment (start → first boss) on PC without crashes;
> both versions build.

| Criterion | Status |
|---|---|
| US `dw3-pc` builds / links Phase 3 overlays | **yes** (255 registered overlays) |
| Start → field playable without crash | **partial** (intro→movie→Asuka Lobby proven; continuous to Genji not) |
| Reach / defeat first story boss | **partial** (Genji event battle enter + `FORCE_WIN` → report → field via aids; not continuous from title) |
| EU build claimed | **no** (US only) |
| Audio / movies on the segment | **yes for intro movies + SEP** (P3.6/P3.7 Done-when; Start-skip on `0xE02`) |

## First story boss (US)

| Item | Value |
|---|---|
| Stage | `WSTAG200` — Asuka Central Sector streets (file 451) |
| Mode | `0x200` |
| Story step | `GAME.progress == 3` → events 50/52 (`endEvent50` sets `EVENT_BATTLE(1)`; `endEvent52` → progress 4) |
| Opponent | Tamer Genji (event battle) |
| Prerequisites | Starters via Tamer Service (`WSTAG212` / `0x204`, `SPECIAL(0..2)` + `setParty`) — skipped by host empty-party `setParty(0)` in `FIGHTSTG` when booting past that |

## Proven (automated, `SDL_VIDEODRIVER=offscreen`)

Disc for Form2 XA / STR: `DW3_DISC_BIN` → owner
`/mnt/windows/Jogos/PS1/Digimon World 3/Digimon World 3.bin`.

### A — Honest new-game → `0xE02` movie → Asuka Lobby

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  DW3_START_MODE=0x2D7 DW3_FAST_SCRIPT=1 DW3_AUTO_PAD=confirm,start \
  DW3_DISC_BIN="$DISC" DW3_RUNTIME=runtime/us \
  ./build/pc/dw3-pc --runtime runtime/us
```

Observed (`build/p3-gate/newgame_e02_skip.log`):

| Step | Mode | Note |
|---|---|---|
| Field start | `0x2D7` | `WSTAG780` |
| Online Center / name | `0x2D8` → `0x500` → `0x2D8` | |
| Chamber | `0x2D9` | `WSTAG790` |
| Opening 2 | `0xE02` | STR open + `DecDCTout frame done #1` |
| Start-skip | — | `dw3-movie: Start-skip -> TASK_KILL` (pad hold; not fake mode skip) |
| Post-movie | `0x207` | `WSTAG225` |
| Event 5 → Lobby | `0x203` | `WSTAG210` — **stall** (needs Tamer Service / doors) |

### B — Genji first boss via progress aids

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  DW3_START_MODE=0x200 DW3_PROGRESS=3 DW3_FAST_SCRIPT=1 \
  DW3_AUTO_PAD=confirm DW3_FORCE_WIN=1 \
  DW3_DISC_BIN="$DISC" DW3_RUNTIME=runtime/us \
  ./build/pc/dw3-pc --runtime runtime/us
```

Observed (`build/p3-gate/genji_win.log`):

| Step | Note |
|---|---|
| `FIELDSTG` `0x200` | stageFile=451 |
| `FIGHTSTG_start` → `0x600` | event battle |
| `FIGHTSTG setParty(0) (empty party)` | patch `0024` — avoids `computeStats` SEGV |
| `DW3_FORCE_WIN` → `STFGTREP` (`0x1400`) | report battle=200; +EXP |
| Return | `fieldMode=0x200` then script leave `0x206` |

Same Genji enter/win also from Tamer Service boot
(`DW3_START_MODE=0x204 DW3_PROGRESS=2` → `SCRIPT_LEAVE 0x200` → battle).

### Earlier Done-whens (not re-run for this doc)

| Task | Evidence |
|---|---|
| P3.1–P3.5 | `docs/P3.1_64BIT_AUDIT.md` … `docs/P3.5_CARDGAME.md` |
| P3.6 / P3.7 | `docs/P3.6_AUDIO.md`, `docs/P3.7_MOVIES.md` |
| P3.8 | 237/237 live PASS; `WSTAG460` stubbed (`docs/P3.8_STAGES.md`) |
| Forced random encounter (not Genji) | `docs/P3.3_BATTLE.md` |

## Blocked / not proven

| Item | Why |
|---|---|
| **Continuous start → Genji** | After A, automation stalls in Asuka Lobby (`0x203`); no pad path yet through Digimon Center pack pick (`SPECIAL` / `WSTAG212`) to streets |
| **Playable Genji KO** | Proven with `DW3_FORCE_WIN` only (same class as P3.3 aid) |
| **P3.9 Harness** | Needs owner DuckStation oracle + capture fill |
| **WSTAG460** | Host `updateWanderer` SEGV; weak stub only |
| **EU playthrough** | Not in gate claim |

## Hard blockers needing owner / more glue

1. **Lobby → Tamer Service → streets** — pad / script automation (or a
   scoped progress aid that sets `SPECIAL` + leaves to `0x200` without
   lying that the continuous segment ran).
2. **Owner DuckStation** — P3.9 visual oracle / screenshot harness.
3. **WSTAG460** — optional unless P3.8 is reinterpreted as 239/239.
4. **Gate tick** — only after a start→first-boss segment runs without
   crashes (US at minimum), with movies on the honest intro path.

## Honesty rule

Do **not** mark Phase 3 gate passed until a start→first-boss segment runs
without crashes on PC (US at minimum). Forced battle / menu / stage-sweep
evidence and mode-warped Genji wins do not substitute for that continuous
segment. Real movie Start-skip on `0xE01`/`0xE02` is allowed; faking the
mode away from movies is not.
