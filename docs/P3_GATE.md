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
| P3.8 | 237/237 live PASS; `WSTAG460` stubbed (`docs/P3.8_STAGES.md`); linked 2026-10-30, 238/238 |
| Forced random encounter (not Genji) | `docs/P3.3_BATTLE.md` |

## EU build attempt (2026-10-10)

Attempted host compile with decomp-correct `-DVERSION_EU` (not
`-DVERSION_US=0 -DVERSION_EU=1` — `version.h` requires exactly one
`-DVERSION_*`).

| Probe | Result |
|---|---|
| All `src/engine/**/*.c` (43 TUs) under `-DVERSION_EU` | **OK** |
| `src/menus/title/**/*.c` (21 TUs) under `-DVERSION_EU` | **OK** |
| Full EU `dw3-pc` CMake/link | **OK** (link scaffold — `docs/P3_EU_BUILD.md`) |
| Owner `runtime/eu` smoke-boot / EU playthrough | **open** (not run) |

**Status (honest):** Before `phase3-eu`, `port/CMakeLists.txt` hardcoded
`VERSION_US=1` and `overlay_table_us.c` only (`FILE_FIELDSTG` `0x158` US /
`0x166` EU). **Link scaffold** (`-DDW3_VERSION=eu`, `overlay_table_eu.c`,
`runtime/eu` default) landed post-gate; configure+link proven without owner
disc assets. Run still needs packed `runtime/eu` from owner `disks/eu`;
`CNTY_SEL` remains stubbed. EU playthrough was not run and is not required
for the Phase 3 gate.

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
| EU `dw3-pc` runtime + playthrough | Link scaffold in `docs/P3_EU_BUILD.md`; owner `runtime/eu`; CNTY_SEL linked in P6.2 |
| P3.9 Harness | Owner DuckStation oracle + capture fill (side track) |
| `WSTAG460` | Host `updateWanderer` SEGV; weak stub only (closed 2026-10-30, patch `0197`) |

## Honesty rule

Phase 3 gate passes on a **US** start→first-boss segment without crashes,
with real movie Start-skip (not faking the mode away from movies). Documented
Lobby `START_EVENT` aid + scoped `FORCE_WIN` are allowed and called out.
Mode-warping straight to Genji (`DW3_START_MODE=0x200`) does not substitute
for path C. EU playthrough is not required for this gate; claiming “both
versions build” for a full EU `dw3-pc` image requires the scaffold above.

## History moved from ROADMAP.md

Moved verbatim from `docs/ROADMAP.md` on 2026-10-15 (roadmap hygiene PR).
Dates are the dates of each note; later notes and the roadmap entry
supersede earlier "open" statements.

### Phase 3 header: tracking and EU notes

Tracking: Phase 3 on `main` / `phase3-pc-bringup` (P3.5 merged from
`phase3-cardgame` @ `fcff119`). P3.1 done. **P3.2 Done-when met** (+
WSTAG790 follow-up). **P3.3 Done-when met:** forced encounter →
`STFGTREP` (XP/levels) → return to `FIELDSTG` (`docs/P3.3_BATTLE.md`).
**P3.4 Done-when met** (`READY_FOR_MAIN_ROADMAP_SYNC: P3.4`,
`docs/P3.4_MENUS.md`): menus open/return from field; **STSTATUS Triangle
UI cancel** (no `AUTO_MENU_RETURN`); **STGMCARD save + load** via P2.7
`.mcr`; pad START→status; FORCE_NPC shop/lab/naming/gym; **WSTAG245
`talkToActorAt` → shop** (warped proximity). Follow-up: free pad walk-up.
**P3.5 Done-when met**
(`READY_FOR_MAIN_ROADMAP_SYNC: P3.5`, `docs/P3.5_CARDGAME.md`): force-win
→ prize item → `FIELDSTG`; album `cardsSeen` 29→33 via vanilla
`STCRDSHP_openPack` `addCards` (host win `addCards` gated off). **P3.7 Done-when met** (`READY_FOR_MAIN_ROADMAP_SYNC: P3.7`,
`docs/P3.7_MOVIES.md`): sustained STR play + Start-skip → `MODE_TITLE` +
XA decode/mix + **owner audible DAC listen** (Pulse rebuild). Follow-up:
video presentation quality (frames/decode/skip already evidenced). Default
skip remains `MODE_TITLE` (`0010`).
**P3.8 Done-when met** (`READY_FOR_MAIN_ROADMAP_SYNC: P3.8`,
`docs/P3.8_STAGES.md`): `STAGSLCT` + batches 1–6 link 238/239 US stage
objs; live PASS 237/237; only `WSTAG460` left stubbed (host
`updateWanderer` SEGV; linked 2026-10-30, patch `0197`). **Phase 3 gate met** (`docs/P3_GATE.md`): US
continuous start→movie→Lobby→Tamer→Genji→field @ `a5ae93a` (Lobby
`0x9004@0x203` / patch `0036`); `DW3_FORCE_WIN` accepted for gate (P3.3
class; pad KO follow-up); EU playthrough not required. **EU `dw3-pc` link
scaffold** (`docs/P3_EU_BUILD.md`): `-DDW3_VERSION=eu` +
`overlay_table_eu.c` configure/link; owner EU disc pack + PC smoke to
opening movie validated (2026-10-12); OLED extraction and early gameplay
validated (2026-10-14); EU playthrough remains open.
**Switch extraction follow-up (2026-10-13):** the EU NRO now selects the EU
file-sector table, `SLES_039.36`, EU boot-TIM address and `files_eu.json` for
own-disc BIN/CUE extraction under `sdmc:/switch/dw3/eu/`; a US executable is
rejected. Synthetic extractor tests cover map identity, partial-copy retry,
and second-boot validation. Independent Ryubing extracted 2383 files from BIN/CUE and reused them on a
second boot; movie playback reached frame 2100 at about 15 fps with zero
skipped frames, then Plus skipped to the title. Natural completion, audible
A/V synchronization remain open. This emulator build
preceded FAT swap hardening, covered separately by synthetic host tests
(see `docs/P3_EU_BUILD.md`).
**OLED EU extraction (2026-10-14):** with only matching BIN/CUE supplied on SD,
the EU nxlink NRO logged `wrote=2383 skipped=0`, runtime `version=eu`,
`sdmc:/switch/dw3/eu`, `SLES_039.36`, and EU CDFS. Movie frame 1020 had zero
skips/restarts before owner Start-skip (`ended=0`). The owner is playing toward
the first save; successful save/relaunch and the EU playthrough are not yet
confirmed. Full EU movie image/audio acceptance and natural completion remain
open (see `docs/P3_EU_BUILD.md`).
**Update (2026-10-15):** the EU save and HOME-exit/Continue reload are now
owner-confirmed on the OLED (P5.3, #171). The full EU playthrough and the EU
movie acceptance above remain open.
P3.9 harness (`tools/compare_harness/`, `docs/P3.9_REFERENCE.md`):
**done** — title, field, and Kunemon vs Monmon Fight-menu battle pairs
(`DW3_FORCE_ENCOUNTER=34` + `DW3_FORCE_PARTY=1` on PC).

### Phase 3 header: experimental PC 3× internal render

**Experimental PC 3× internal render (2026-10-13):** the SDL3 GPU backend's
existing internal-resolution API is exposed on the PC host through
`DW3_PC_INTERNAL_RES=3`. Title and field runs rendered the 4:3 image in a
1280×720 window (960×720 game image, 160-pixel side bars). The standard 1×
default and Switch host are unchanged. This is an opt-in experiment; visual
quality across all layers and performance remain unassessed. Instructions:
[platforms/pc/README.md](../platforms/pc/README.md).
