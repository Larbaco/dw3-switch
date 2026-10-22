"""P6.15 scenario manifest, shared by the PC runner (replay_tests.py), the
Ryubing runner (ryubing.py) and the recording generator (gen_scenarios.py).

A scenario is one or more phases. Each phase is one process run replaying one
recording from tools/replay/scenarios/<phase>.rec; all phases of a scenario
share one memory-card directory (so a second phase can Continue from the save
the first one wrote), and each scenario starts from a fresh one.

Tiers:
  quick  the original title-menu scenarios (a few seconds each)
  full   gameplay scenarios (a few minutes each; run by default)
  soak   long walk, 15-20 min of game time (run with --soak or --only)
  repro  recordings of known, unfixed bugs; only with --only (expected to fail)
"""
from __future__ import annotations

from dataclasses import dataclass

EU_LANGS = ["en", "fr", "de", "it", "es"]


@dataclass(frozen=True)
class Scenario:
    name: str
    version: str                       # "us" | "eu"
    tier: str                          # "quick" | "full" | "soak"
    phases: tuple[str, ...] = ()       # recording basenames, in order
    turbo: int = 0                     # DW3_TURBO speed for the PC run (0 = off)
    timeout: int = 180                 # seconds per phase (PC)
    # Overlay restore markers expected, in order (a subsequence of the log):
    # (file id, "mode"|"sub"). Extra restores in between are fine.
    expect_restore: tuple[tuple[int, str], ...] = ()
    # Log lines expected per phase: (phase index, regex), each searched in that
    # phase's log (PC stderr / Ryubing guest output). Used by the P9.4
    # persistence scenario, where the proof is a value read back from disk.
    expect_log: tuple[tuple[int, str], ...] = ()
    # Memory sanity (soak): analyse RSS from this game frame on.
    rss_from_frame: int = 0
    note: str = ""

    def __post_init__(self):
        if not self.phases:
            object.__setattr__(self, "phases", (self.name,))


# Overlay file ids (docs/P4.6_MODULES_STATUS.md; US and EU differ). The
# restore sequences were measured from the PC log of the opening; the Asuka
# City main lobby is sub 0x1c5 (US) / 0x1d4 (EU), the Login Room 0x1c7 / 0x1d6.
M, S = "mode", "sub"
US_OPENING = ((0x1C1, M), (0x158, M), (0x1DA, S), (0x1DB, S), (0x1F8, M), (0x158, M),
              (0x1DC, S), (0x1C1, M), (0x158, M), (0x1C7, S), (0x1C5, S))
EU_OPENING = ((0x165, M), (0x1D0, M), (0x166, M), (0x1E9, S), (0x1EA, S), (0x206, M),
              (0x166, M), (0x1EB, S), (0x1D0, M), (0x166, M), (0x1D6, S), (0x1D4, S))
US_DOOR = ((0x1C7, S), (0x1C5, S)) * 2     # two more door round trips
EU_DOOR = ((0x1D6, S), (0x1D4, S)) * 2

SCENARIOS: list[Scenario] = []


def _add(*s: Scenario) -> None:
    SCENARIOS.extend(s)


_add(Scenario("us-title", "us", "quick", expect_restore=((0x1C1, M),)))
for _lang in EU_LANGS:
    _add(Scenario(f"eu-lang-{_lang}", "eu", "quick"))

# Gameplay. DW3_TURBO=4 only changes pacing (field/fight modes); the replay
# is frame-keyed, so captures do not depend on it.
_add(
    Scenario("us-newgame-field", "us", "full", turbo=4, timeout=900,
             expect_restore=US_OPENING,
             note="New Game -> opening -> first field with control"),
    Scenario("eu-newgame-field", "eu", "full", turbo=4, timeout=900,
             expect_restore=EU_OPENING,
             note="English; same flow as US"),
    Scenario("us-field-walk", "us", "full", turbo=4, timeout=1200,
             expect_restore=US_OPENING + US_DOOR,
             note="4 map transitions (lobby <-> Login Room, twice)"),
    Scenario("eu-field-walk", "eu", "full", turbo=4, timeout=1200,
             expect_restore=EU_OPENING + EU_DOOR,
             note="4 map transitions (lobby <-> Login Room, twice)"),
    Scenario("us-load-menu", "us", "full", timeout=300, expect_restore=((0x1C1, M), (0x1E3, M)),
             note="Load Game on an empty card: card access, block list, no data"),
    Scenario("eu-load-menu", "eu", "full", timeout=300,
             note="Continue on an empty card: card access, block list, no data"),
    Scenario("us-soak", "us", "soak", turbo=4, timeout=5400, rss_from_frame=20000,
             expect_restore=US_OPENING + US_DOOR,
             note="~17 min of game time walking doors/pods"),
    Scenario("eu-soak", "eu", "soak", turbo=4, timeout=5400, rss_from_frame=20000,
             expect_restore=EU_OPENING + EU_DOOR,
             note="~20 min of game time walking doors/pods"),
    Scenario("us-bridge-partner-gate", "us", "full", turbo=4, timeout=1200,
             expect_restore=US_OPENING,
             note="bridge gate dialogue; regression for the balloon NULL actor (patch 0058)"),
    Scenario("eu-bridge-partner-gate", "eu", "full", turbo=4, timeout=1200,
             expect_restore=EU_OPENING,
             note="same on the PAL build"),
    Scenario("us-repro-balloon", "us", "full", turbo=4, timeout=900,
             expect_restore=US_OPENING,
             note="lobby walk that used to crash FIELDSTG_drawBalloon (patch 0058)"),
)

# P9.4: the options written by phase 1 are read back by phase 2, a fresh
# process on the same config dir (PC: DW3_CONFIG_DIR temp dir shared by the
# phases; Ryubing: sdmc:/switch/dw3/<ver>/port_options.cfg, removed at the
# start of the scenario). Phase 1 switches Widescreen OFF, Fast-forward
# 2x -> 3x and the Restart hotkey ON; phase 2 only opens the screen again.
_OPT_P1 = (r"dw3-options: no \S*port_options\.cfg; defaults",
           r"dw3-options: saved \S*port_options\.cfg \(widescreen=0 fast_forward=3 restart_hotkey=1\)")
_OPT_P2 = (r"dw3-options: \S*port_options\.cfg \(widescreen=0 fast_forward=3 restart_hotkey=1\)",)
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-options-persist", _v, "full", timeout=300,
                  phases=(f"{_v}-options-persist-p1", f"{_v}-options-persist-p2"),
                  expect_log=tuple((0, r) for r in _OPT_P1) + tuple((1, r) for r in _OPT_P2),
                  note="Options toggled in one process are read back by the next (P9.4)"))


def by_name() -> dict[str, Scenario]:
    return {s.name: s for s in SCENARIOS}
