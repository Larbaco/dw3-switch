# Phase 3 gate — start → first boss (PC / US)

## Status (2026-10-10)

**Gate not met.** Do not tick Phase 3 complete (EU not claimed; boss KO
still via scoped `DW3_FORCE_WIN` like P3.3).

**US continuous start → Genji is proven** with one documented Lobby aid:
`DW3_FORCE_NPC_ACTION=0x9004@0x203` (`START_EVENT(4)` → field event 9,
the Digimon Center “download partners now?” menu — same `applyAction`
path as talking to actor0 twice). That is **not** a mode-warp past Lobby /
Tamer Service: event 9 → 13 leaves to `WSTAG212` (`0x204`); Tamer Service
`updateStage` runs events 10–12 from `SPECIAL(partySet)` set during
`STPLNMET`; `endEvents10To12` sets progress 3 and leaves to `0x200`; Genji
event battle → `FORCE_WIN` → `STFGTREP` → field → `0x206`.

Diagnosis of the old `0x203` stall: free-walk / `AUTO_PAD=confirm` never
triggers Digimon Center event 9 (no door to `0x204`; only script leave
from event 13). Pack / `SPECIAL(0..2)` are `GAME.partySet` from name-entry
`setParty`, not a missing bag item. Actor24’s `ITEM(0,0)==0` only gates an
NPC appearance.

Branch tip at evidence time: see git (`phase3-pc-bringup`). Local logs under
`build/p3-gate/` (gitignored). Patch `0036` adds optional `@mode` on
`DW3_FORCE_NPC_ACTION` so the aid waits for Lobby on a continuous boot.

## Gate criteria (ROADMAP)

> a full playthrough segment (start → first boss) on PC without crashes;
> both versions build.

| Criterion | Status |
|---|---|
| US `dw3-pc` builds / links Phase 3 overlays | **yes** (255 registered overlays) |
| Start → field playable without crash | **yes** (continuous US segment through Genji return) |
| Reach / defeat first story boss | **yes with scoped aid** (Genji enter + `FORCE_WIN` → report → field; continuous from `0x2D7`) |
| EU build claimed | **no** (US only) — blocks gate tick |
| Audio / movies on the segment | **yes for intro movies + SEP** (P3.6/P3.7 Done-when; Start-skip on `0xE02`) |

## First story boss (US)

| Item | Value |
|---|---|
| Stage | `WSTAG200` — Asuka Central Sector streets (file 451) |
| Mode | `0x200` |
| Story step | `GAME.progress == 3` → events 50/52 (`endEvent50` sets `EVENT_BATTLE(1)`; `endEvent52` → progress 4) |
| Opponent | Tamer Genji (event battle) |
| Prerequisites | `STPLNMET` `setParty` → `partySet` / `SPECIAL(0..2)`; Lobby event 9→13 → Tamer Service events 10–12 → progress 3 |

## Proven (automated, `SDL_VIDEODRIVER=offscreen`)

Disc for Form2 XA / STR: `DW3_DISC_BIN` → owner
`/mnt/windows/Jogos/PS1/Digimon World 3/Digimon World 3.bin`.

### C — Continuous new-game → movie → Lobby → Tamer → Genji

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  DW3_START_MODE=0x2D7 DW3_FAST_SCRIPT=1 \
  DW3_FORCE_NPC_ACTION=0x9004@0x203 \
  DW3_AUTO_PAD=confirm,start DW3_FORCE_WIN=1 \
  DW3_DISC_BIN="$DISC" DW3_RUNTIME=runtime/us \
  ./build/pc/dw3-pc --runtime runtime/us
```

Observed (`build/p3-gate/continuous_lobby_genji.log`):

| Step | Mode | Note |
|---|---|---|
| Field start | `0x2D7` | `WSTAG780` |
| Online Center / name | `0x2D8` → `0x500` → `0x2D8` | `STPLNMET` `setParty` (pack / `partySet`) |
| Chamber | `0x2D9` | `WSTAG790` |
| Opening 2 | `0xE02` | `dw3-movie: Start-skip -> TASK_KILL` |
| Post-movie | `0x207` → `0x203` | progress 2 |
| Lobby aid | `0x203` | `FORCE_NPC 0x9004@0x203` → event 9 → leave `0x204` |
| Tamer Service | `0x204` | events 10–12; leave `0x200` progress 3 |
| Genji | `0x600` → `0x1400` | `FORCE_WIN` → `STFGTREP` battle=200; +EXP |
| Return | `0x200` → `0x206` | field then script leave |

### D — Lobby-only segment (same aid, no intro)

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  DW3_START_MODE=0x203 DW3_PROGRESS=2 DW3_FAST_SCRIPT=1 \
  DW3_FORCE_NPC_ACTION=0x9004@0x203 \
  DW3_AUTO_PAD=confirm DW3_FORCE_WIN=1 \
  DW3_DISC_BIN="$DISC" DW3_RUNTIME=runtime/us \
  ./build/pc/dw3-pc --runtime runtime/us
```

Observed (`build/p3-gate/lobby_tamer_genji.log`): same `0x203` → `0x204` →
`0x200` → Genji → report → `0x206` (host `setParty(0)` if party empty).

### A — Honest new-game → `0xE02` movie → Asuka Lobby (no Tamer aid)

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  DW3_START_MODE=0x2D7 DW3_FAST_SCRIPT=1 DW3_AUTO_PAD=confirm,start \
  DW3_DISC_BIN="$DISC" DW3_RUNTIME=runtime/us \
  ./build/pc/dw3-pc --runtime runtime/us
```

Observed (`build/p3-gate/newgame_e02_skip.log`): stalls in Lobby `0x203`
without `0x9004@0x203` (no free-pad path to event 9 yet).

### B — Genji via progress aids (mode-warped; still valid unit proof)

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  DW3_START_MODE=0x200 DW3_PROGRESS=3 DW3_FAST_SCRIPT=1 \
  DW3_AUTO_PAD=confirm DW3_FORCE_WIN=1 \
  DW3_DISC_BIN="$DISC" DW3_RUNTIME=runtime/us \
  ./build/pc/dw3-pc --runtime runtime/us
```

Observed (`build/p3-gate/genji_win.log`): enter + `FORCE_WIN` → report →
field. Does **not** by itself satisfy the continuous-segment bar (see C).

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
| **EU playthrough / EU PC image** | Not claimed — **gate tick blocked** (“both versions build”) |
| **Playable Genji KO** | Proven with `DW3_FORCE_WIN` only (same class as P3.3; clearly scoped) |
| **Free-pad Lobby → event 9** | Aid `0x9004@0x203` substitutes for walking up to Digimon Center NPC |
| **P3.9 Harness** | Needs owner DuckStation oracle + capture fill |
| **WSTAG460** | Host `updateWanderer` SEGV; weak stub only |

## Aids used on the continuous path (honest)

| Aid | Role |
|---|---|
| `DW3_AUTO_PAD=confirm,start` | Advance dialogue / menus; movie Start-skip |
| `DW3_FAST_SCRIPT=1` | Speed event scripts (not skip) |
| `DW3_FORCE_NPC_ACTION=0x9004@0x203` | Real `START_EVENT(4)` → event 9 at Lobby only (`0036`) |
| `DW3_FORCE_WIN=1` | Scoped boss completion (P3.3 class) |

## Honesty rule

Do **not** mark Phase 3 gate passed until a start→first-boss segment runs
without crashes on PC **and** both versions build (ROADMAP). US continuous
segment with real movie Start-skip + documented Lobby `START_EVENT` aid +
scoped `FORCE_WIN` is evidenced above; **EU remains open**. Mode-warping
straight to Genji (`DW3_START_MODE=0x200`) does not substitute for path C.
Faking the mode away from movies is not allowed.
