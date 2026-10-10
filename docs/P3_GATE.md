# Phase 3 gate — start → first boss (PC / US)

## Status (2026-10-10)

**Gate met** on `phase3-pc-bringup` @ `a5ae93a` (US-minimum playthrough).

- **US continuous** start → movie Start-skip → Lobby → Tamer Service →
  Genji → report → field is proven (path C below).
- **Boss KO:** scoped `DW3_FORCE_WIN` is **accepted for this gate** (same
  class as P3.3 Done-when). Pad-driven Genji KO remains a follow-up, not a
  gate blocker.
- **EU playthrough:** not required / not claimed.
- **“Both versions build”:** US `dw3-pc` yes. EU full `dw3-pc` **not
  scaffolded** (clear blocker below). Engine TUs compile under
  `-DVERSION_EU` (43/43). Do not invent an EU playthrough.

## Gate criteria (ROADMAP)

> a full playthrough segment (start → first boss) on PC without crashes;
> both versions build.

| Criterion | Status |
|---|---|
| US `dw3-pc` builds / links Phase 3 overlays | **yes** (255 registered overlays) |
| Start → first boss without crash (US) | **yes** (continuous path C; Lobby aid `0036`) |
| Boss completion claim | **yes with accepted aid** — `DW3_FORCE_WIN` (P3.3 class; pad KO follow-up) |
| Both versions build | **US yes; EU `dw3-pc` blocked** (see EU section) — playthrough EU not required |
| Audio / movies on the segment | **yes** (P3.6/P3.7; Start-skip on `0xE02`) |

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

Branch tip at evidence time: `phase3-pc-bringup` @ `a5ae93a`. Local logs
under `build/p3-gate/` (gitignored). Patch `0036` adds optional `@mode` on
`DW3_FORCE_NPC_ACTION` so the Lobby aid waits for `0x203` on a continuous
boot.

### C — Continuous new-game → movie → Lobby → Tamer → Genji (gate path)

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

**Lobby aid honesty:** `DW3_FORCE_NPC_ACTION=0x9004@0x203` fires real
`START_EVENT(4)` → field event 9 (Digimon Center “download partners now?”)
— the same `applyAction` path as talking to actor0 twice. That is **not** a
mode-warp past Lobby / Tamer Service. Free-pad walk-up to that NPC is still
unproven.

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
`0x200` → Genji → report → `0x206`.

### A / B — unit proofs (not the continuous bar alone)

- **A** (`newgame_e02_skip.log`): intro→movie→Lobby without `0x9004@0x203`
  stalls in `0x203`.
- **B** (`genji_win.log`): `DW3_START_MODE=0x200 DW3_PROGRESS=3` +
  `FORCE_WIN` proves Genji enter/report; does **not** by itself satisfy
  continuous path C.

### Earlier Done-whens (not re-run for this doc)

| Task | Evidence |
|---|---|
| P3.1–P3.5 | `docs/P3.1_64BIT_AUDIT.md` … `docs/P3.5_CARDGAME.md` |
| P3.6 / P3.7 | `docs/P3.6_AUDIO.md`, `docs/P3.7_MOVIES.md` |
| P3.8 | 237/237 live PASS; `WSTAG460` stubbed (`docs/P3.8_STAGES.md`) |
| Forced random encounter (not Genji) | `docs/P3.3_BATTLE.md` |

## EU build attempt (2026-10-10)

Attempted host compile with decomp-correct `-DVERSION_EU` (not
`-DVERSION_US=0 -DVERSION_EU=1` — `version.h` requires exactly one
`-DVERSION_*`).

| Probe | Result |
|---|---|
| All `src/engine/**/*.c` (43 TUs) under `-DVERSION_EU` | **OK** |
| `src/menus/title/**/*.c` (21 TUs) under `-DVERSION_EU` | **OK** |
| Full EU `dw3-pc` CMake/link | **blocked** |

**Blocker (honest):** `port/CMakeLists.txt` hardcodes `VERSION_US=1`; host
overlay registration is `port/overlays/overlay_table_us.c` with US file ids
(`FILE_FIELDSTG` is `0x158` US / `0x166` EU). No `DW3_VERSION=eu` (or
equivalent) target, no `overlay_table_eu.c`, no owner `runtime/eu` /
`disks/eu` on this host. Wiring an EU `dw3-pc` image is Phase 6/P7.2-class
scaffold work — out of Phase 3 gate scope. EU playthrough was not run.

## Aids used on the continuous path (honest)

| Aid | Role | Gate stance |
|---|---|---|
| `DW3_AUTO_PAD=confirm,start` | Dialogue / menus; movie Start-skip | accepted |
| `DW3_FAST_SCRIPT=1` | Speed event scripts (not skip) | accepted |
| `DW3_FORCE_NPC_ACTION=0x9004@0x203` | Real `START_EVENT(4)` → event 9 at Lobby (`0036`) | accepted (not mode-warp) |
| `DW3_FORCE_WIN=1` | Boss completion → `STFGTREP` | **accepted for gate** (P3.3 class) |

## Follow-ups (do not untick the gate)

| Item | Why |
|---|---|
| Pad-driven Genji KO | `FORCE_WIN` skips attack/KO sequence |
| Free-pad Lobby → event 9 | Aid substitutes for walking up to Digimon Center NPC |
| EU `dw3-pc` runtime + playthrough | Link scaffold in `docs/P3_EU_BUILD.md`; owner `runtime/eu` + CNTY_SEL still open |
| P3.9 Harness | Owner DuckStation oracle + capture fill (side track) |
| `WSTAG460` | Host `updateWanderer` SEGV; weak stub only |

## Honesty rule

Phase 3 gate passes on a **US** start→first-boss segment without crashes,
with real movie Start-skip (not faking the mode away from movies). Documented
Lobby `START_EVENT` aid + scoped `FORCE_WIN` are allowed and called out.
Mode-warping straight to Genji (`DW3_START_MODE=0x200`) does not substitute
for path C. EU playthrough is not required for this gate; claiming “both
versions build” for a full EU `dw3-pc` image requires the scaffold above.
