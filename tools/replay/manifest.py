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
    # The same, but the regex must NOT match anywhere in that phase's log
    # (P6.18: "no FIGHTSTG_start while Encounter rate is Off").
    forbid_log: tuple[tuple[int, str], ...] = ()
    # port_options.cfg keys written before the first phase (P6.18 seeds the
    # pacing rows this way, so the option file reader is part of the test).
    cfg: tuple[tuple[str, str], ...] = ()
    # Extra environment for the PC run (debug aids such as DW3_START_MODE).
    # The Switch build has no environment, so the Ryubing runner skips these.
    env: tuple[tuple[str, str], ...] = ()
    # The recording has no `shot` lines: the verdicts are the process, the
    # overlay markers and the log lines only (no reference captures needed).
    log_only: bool = False
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
           r"dw3-options: saved \S*port_options\.cfg \(widescreen=0 fast_forward=3 restart_hotkey=1 music_volume=10 sfx_volume=10 "
           r"encounter_rate=0 text_speed=0\)")
_OPT_P2 = (r"dw3-options: \S*port_options\.cfg \(widescreen=0 fast_forward=3 restart_hotkey=1 "
           r"music_volume=10 sfx_volume=10 encounter_rate=0 text_speed=0\)",)
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-options-persist", _v, "full", timeout=300,
                  phases=(f"{_v}-options-persist-p1", f"{_v}-options-persist-p2"),
                  expect_log=tuple((0, r) for r in _OPT_P1) + tuple((1, r) for r in _OPT_P2),
                  note="Options toggled in one process are read back by the next (P9.4)"))

# P6.17: Music volume 10 -> 7 and SFX volume 10 -> 3 in phase 1 (Left on the
# volume rows), read back by phase 2 from the same config dir.
_VOL_P1 = (r"dw3-options: no \S*port_options\.cfg; defaults",
           r"dw3-options: saved \S*port_options\.cfg \(widescreen=1 fast_forward=2 restart_hotkey=0 "
           r"music_volume=7 sfx_volume=3 encounter_rate=0 text_speed=0\)")
_VOL_P2 = (r"dw3-options: \S*port_options\.cfg \(widescreen=1 fast_forward=2 restart_hotkey=0 "
           r"music_volume=7 sfx_volume=3 encounter_rate=0 text_speed=0\)",)
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-volume-persist", _v, "full", timeout=300,
                  phases=(f"{_v}-volume-persist-p1", f"{_v}-volume-persist-p2"),
                  expect_log=tuple((0, r) for r in _VOL_P1) + tuple((1, r) for r in _VOL_P2),
                  note="Music / SFX volume set in one process is read back by the next (P6.17)"))


# P6.18 pacing options. Log-only: the proof is in the game log (port patches
# 0062 / 0063 print `dw3-pacing:` lines), so there are no reference captures.
# The option file is seeded by the runner (Scenario.cfg), which tests the
# reader as well.
_PACING_PERSIST_P1 = (r"dw3-options: saved \S*port_options\.cfg \(widescreen=1 fast_forward=2 restart_hotkey=0 "
                      r"music_volume=10 sfx_volume=10 encounter_rate=1 text_speed=2\)",)
_PACING_PERSIST_P2 = (r"dw3-options: \S*port_options\.cfg \(widescreen=1 fast_forward=2 restart_hotkey=0 "
                      r"music_volume=10 sfx_volume=10 encounter_rate=1 text_speed=2\)",)
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-pacing-persist", _v, "full", timeout=300, log_only=True,
                  phases=(f"{_v}-pacing-persist-p1", f"{_v}-pacing-persist-p2"),
                  expect_log=tuple((0, r) for r in _PACING_PERSIST_P1) +
                             tuple((1, r) for r in _PACING_PERSIST_P2),
                  note="Encounter rate 0.5x and Text speed Instant set in one process are read back by the next (P6.18)"))

# Text speed: New Game, then no input; the first scripted pages type out and
# wait. `text page typed in N frame(s)` is the typing time of a page.
for _v in ("us", "eu"):
    for _name, _speed, _rx in (
            ("normal", 0, r"text page typed in [0-9]{3,} frame\(s\) \(text speed normal\)"),
            ("fast", 1, r"text page typed in 1[0-9]{2} frame\(s\) \(text speed fast\)"),
            ("instant", 2, r"text page typed in [0-9] frame\(s\) \(text speed instant\)")):
        _add(Scenario(f"{_v}-text-{_name}", _v, "full", timeout=300, log_only=True,
                      phases=(f"{_v}-text-opening",),
                      cfg=(("text_speed", str(_speed)),),
                      expect_log=((0, _rx),),
                      note=f"first dialogue pages at Text speed {_name} (P6.18)"))

# Encounter rate: START_MODE 0x233 / PROGRESS 6 is a South Sector field that
# is a random-battle area everywhere; the recording holds Up.
_ENC_ENV = (("DW3_START_MODE", "0x233"), ("DW3_PROGRESS", "6"))
for _v, _fight in (("us", 0x159), ("eu", 0x167)):
    _walk = f"{_v}-encounter-walk"
    _add(Scenario(f"{_v}-encounter-1x", _v, "full", timeout=300, log_only=True, phases=(_walk,),
                  env=_ENC_ENV,
                  expect_log=((0, r"dw3-pacing: random battle after 1[0-9]{2} footstep\(s\) \(encounter rate 1x\)"),
                              (0, r"FIGHTSTG_start")),
                  note="default: a random battle comes after ~100 footsteps (P6.18)"),
         Scenario(f"{_v}-encounter-half", _v, "full", timeout=300, log_only=True, phases=(_walk,),
                  env=_ENC_ENV, cfg=(("encounter_rate", "1"),),
                  expect_log=((0, r"dw3-pacing: random battle after 2[0-9]{2} footstep\(s\) \(encounter rate 0\.5x\)"),
                              (0, r"FIGHTSTG_start")),
                  note="0.5x: the same walk needs about twice the footsteps (P6.18)"),
         Scenario(f"{_v}-encounter-1_5x", _v, "full", timeout=300, log_only=True, phases=(_walk,),
                  env=_ENC_ENV, cfg=(("encounter_rate", "3"),),
                  expect_log=((0, r"dw3-pacing: random battle after 68 footstep\(s\) \(encounter rate 1\.5x\)"),
                              (0, r"FIGHTSTG_start")),
                  note="1.5x: the same walk needs about two thirds of the footsteps (stored value 3)"),
         Scenario(f"{_v}-encounter-2x", _v, "full", timeout=300, log_only=True, phases=(_walk,),
                  env=_ENC_ENV, cfg=(("encounter_rate", "4"),),
                  expect_log=((0, r"dw3-pacing: random battle after 51 footstep\(s\) \(encounter rate 2x\)"),
                              (0, r"FIGHTSTG_start")),
                  note="2x: the same walk needs about half the footsteps (stored value 4)"),
         Scenario(f"{_v}-encounter-off", _v, "full", timeout=300, log_only=True, phases=(_walk,),
                  env=_ENC_ENV, cfg=(("encounter_rate", "2"),),
                  expect_log=((0, r"dw3-pacing: encounters off: 129 footstep\(s\) in battle areas ignored"),),
                  forbid_log=((0, r"FIGHTSTG_start"), (0, r"random battle after")),
                  note="Off: the walk that fought at ~100 footsteps never fights (P6.18)"),
         Scenario(f"{_v}-encounter-off-event", _v, "full", timeout=300, log_only=True,
                  phases=(f"{_v}-encounter-event",),
                  env=_ENC_ENV + (("DW3_FORCE_NPC_ACTION", "0x7400"),),
                  cfg=(("encounter_rate", "2"),),
                  expect_log=((0, r"FIGHTSTG_start"),),
                  forbid_log=((0, r"random battle after"),),
                  note="Off does not touch a scripted EVENT_BATTLE (P6.18)"))

# Two consecutive battles, a technique used in each (patch 0061): every battle
# is a mode change (FIGHTSTG <- report/field), so WFIGHTMN is restored per
# battle, as the PS1 reloads it. PC only (needs the env debug aids).
_BATTLE_ENV = (("DW3_START_MODE", "0x600"), ("DW3_FORCE_ENCOUNTER", "34"),
               ("DW3_FORCE_PARTY", "0"), ("DW3_AUTO_DOOR_MODE", "0x600"),
               ("DW3_AUTO_DOOR_FRAMES", "120"))
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158)), ("eu", (0x167, 0x208, 0x1D1, 0x166))):
    _fight, _menu, _report, _field = _ids
    _battle = ((_fight, M), (_menu, S), (_report, M), (_field, M))
    _add(Scenario(f"{_v}-two-battles", _v, "full", turbo=4, timeout=600,
                  env=_BATTLE_ENV, expect_restore=_battle * 2,
                  expect_log=((0, r"WFIGHTMN_endBattle result=1(?s:.*)"
                                  r"AUTO_DOOR requestMode=0x600(?s:.*)"
                                  r"WFIGHTMN_endBattle result=1"),),
                  note="two forced battles, Tech in each; WFIGHTMN restored per battle"))


# P7.7 bug fixes (patches 0064 hooks, 0065 HUD name, 0066 claw steal). Log-only
# battles: the patches print `dw3-p77:` lines and the PC hooks of 0064 set the
# situation up, so the proof does not depend on a lucky roll. PC only.
_P77_ENV = (("DW3_START_MODE", "0x600"), ("DW3_FORCE_ENCOUNTER", "34"),
            ("DW3_FORCE_PARTY", "0"))
_P77_CLAW = _P77_ENV + (("DW3_FORCE_TECH", "127"), ("DW3_FORCE_HIT", "1"),
                        ("DW3_FORCE_STEAL", "1"), ("DW3_FORCE_ENEMY_ITEM", "1"))
_P77_WON = r"WFIGHTMN_endBattle result=1"
for _v in ("us", "eu"):
    _bat = (f"{_v}-p77-battle",)
    _add(
        Scenario(f"{_v}-p77-claw-ko", _v, "full", timeout=300, log_only=True, phases=_bat,
                 env=_P77_CLAW + (("DW3_FORCE_ENEMY_HP", "1"),),
                 expect_log=((0, r"DW3_FORCE_TECH partner uses tech 127(?s:.*)"
                                 r"claw steal on a knocked-out enemy \(tech 127\)(?s:.*)"
                                 r"steal item 1 from enemy slot 0 \(hp 0\)(?s:.*)" + _P77_WON),),
                 note="Picking Claw on a 1 HP enemy: the steal happens on the killing blow (patch 0066)"),
        Scenario(f"{_v}-p77-claw-alive", _v, "full", timeout=300, log_only=True, phases=_bat,
                 env=_P77_CLAW + (("DW3_FORCE_ENEMY_HP", "2000"),),
                 expect_log=((0, r"steal item 1 from enemy slot 0 \(hp 19[0-9]{2}\)"),),
                 forbid_log=((0, r"knocked-out enemy"),),
                 note="the same claw on a 2000 HP enemy takes the old path (patch 0066)"),
        Scenario(f"{_v}-p77-ko-no-claw", _v, "full", timeout=300, log_only=True, phases=_bat,
                 env=_P77_ENV + (("DW3_FORCE_ENEMY_ITEM", "1"), ("DW3_FORCE_ENEMY_HP", "1"),
                                 ("DW3_FORCE_HIT", "1")),
                 expect_log=((0, _P77_WON),),
                 forbid_log=((0, r"knocked-out enemy"), (0, r"steal item")),
                 note="a technique without the steal effect that knocks the enemy out is untouched (patch 0066)"),
        Scenario(f"{_v}-p77-hud-rookie", _v, "full", timeout=300, log_only=True, phases=_bat,
                 env=_P77_ENV,
                 expect_log=((0, r"battle HUD partner name of the Digimon \(Digimon id 383, 43 px\)"),),
                 note="an unevolved Kotemon: the Digimon's own name is the Rookie string (patch 0065)"),
        Scenario(f"{_v}-p77-hud-form", _v, "full", timeout=300, log_only=True, phases=_bat,
                 env=_P77_ENV + (("DW3_FORCE_FORM", "386"),),
                 expect_log=((0, r"battle HUD partner name of the Digimon \(Digimon id 386, 54 px\)"),),
                 note="Kotemon fighting as Dinohumon: the HUD names Dinohumon (patch 0065)"),
        Scenario(f"{_v}-p77-hud-long", _v, "full", timeout=300, log_only=True, phases=_bat,
                 env=_P77_ENV + (("DW3_FORCE_FORM", "196"),),
                 expect_log=((0, r"battle HUD partner name of the Digimon \(Digimon id 196, 84 px\)"),),
                 note="MetalGarurumon (84 px) is squeezed to fit before the HP number (patch 0065)"),
        Scenario(f"{_v}-p77-hud-nickname", _v, "full", timeout=300, log_only=True, phases=_bat,
                 env=_P77_ENV + (("DW3_FORCE_FORM", "386"), ("DW3_FORCE_NICKNAME", "Spike")),
                 expect_log=((0, r"battle HUD partner name kept \(nickname or no form\) \(Digimon id 386, 28 px\)"),),
                 note="a nickname from the Nickname Shop is kept on a digivolved partner (patch 0065)"),
        Scenario(f"{_v}-p77-square-field", _v, "full", timeout=300, log_only=True,
                 phases=(f"{_v}-p77-square",), env=(("DW3_START_MODE", "0x233"), ("DW3_PROGRESS", "6")),
                 expect_log=((0, r"dw3-pad: buttons pressed mask=0x0080"),),
                 forbid_log=((0, r"kind=mode(?s:.*?)kind=mode"),),
                 note="Square (alone, held, with Circle) in a field changes nothing: the game reads no Square there"),
    )
# The text language of the HUD name (the form's name is read from the current
# language's file; DW3_LANGUAGE is set before the new game copies the names).
for _lang, _id in (("fr", 3), ("it", 4), ("de", 5), ("es", 6)):
    _add(Scenario(f"eu-p77-hud-form-{_lang}", "eu", "full", timeout=300, log_only=True,
                  phases=("eu-p77-battle",),
                  env=_P77_ENV + (("DW3_FORCE_FORM", "386"), ("DW3_LANGUAGE", str(_id))),
                  expect_log=((0, rf"LANGUAGE={_id} \(DW3_LANGUAGE\)(?s:.*)"
                                  r"battle HUD partner name of the Digimon \(Digimon id 386, [0-9]+ px\)"),),
                  note=f"the Digimon's name is read from the {_lang} text file (patch 0065)"))

# P7.9 battle menu memory (patch 0067, port/battle_menu): the file-only key
# `battle_menu_memory` in port_options.cfg (Scenario.cfg) turns it on. The same
# forced battle (`<v>-p79-menu`, an enemy with 2000 HP so there is a second turn,
# and techniques added to the list by DW3_EXTRA_TECHS) for every mode: the log
# shows what the menus opened on. PC only (env), log only.
_P79_ENV = _P77_ENV + (("DW3_FORCE_ENEMY_HP", "2000"), ("DW3_EXTRA_TECHS", "54,55,57"))
_P79_LOG = r"dw3-battle-menu: "
_P79_OPENS = r"command menu opens on Techniques \(remembered\)"
for _v in ("us", "eu"):
    _m = (f"{_v}-p79-menu",)
    _add(
        Scenario(f"{_v}-p79-memory-off", _v, "full", timeout=300, log_only=True, phases=_m,
                 env=_P79_ENV,
                 expect_log=((0, r"DW3_FORCE_ENEMY_HP"),),
                 forbid_log=((0, _P79_LOG),),
                 note="default (key absent): no menu memory and no battle-menu line, as before (patch 0067)"),
        Scenario(f"{_v}-p79-memory-command", _v, "full", timeout=300, log_only=True, phases=_m,
                 env=_P79_ENV, cfg=(("battle_menu_memory", "1"),),
                 expect_log=((0, r"command menu opens on Attack(?s:.*)"
                                 r"remembered command Techniques(?s:.*)" + _P79_OPENS),),
                 forbid_log=((0, r"technique list of partner"), (0, r"remembered technique")),
                 note="battle_menu_memory=1: turn 2 opens on Techniques, the list on its first line (patch 0067)"),
        Scenario(f"{_v}-p79-memory-tech", _v, "full", timeout=300, log_only=True, phases=_m,
                 env=_P79_ENV, cfg=(("battle_menu_memory", "2"),),
                 expect_log=((0, r"remembered technique 54 for partner 0(?s:.*)" + _P79_OPENS + r"(?s:.*)"
                                 r"technique list of partner 0 opens on technique 54 \(page 0, line 1, remembered\)"),),
                 note="battle_menu_memory=2: turn 2 opens on Techniques and the list on Swing Swing, line 2 (patch 0067)"),
        Scenario(f"{_v}-p79-memory-page2", _v, "full", timeout=300, log_only=True,
                 phases=(f"{_v}-p79-menu-page2",),
                 env=_P79_ENV[:-1] + (("DW3_EXTRA_TECHS", "54,55,56,57,58,59,60"),),
                 cfg=(("battle_menu_memory", "2"),),
                 expect_log=((0, r"remembered technique 59 for partner 0(?s:.*)" + _P79_OPENS + r"(?s:.*)"
                                 r"technique list of partner 0 opens on technique 59 \(page 1, line 0, remembered\)"),),
                 note="the remembered technique is on the second page: the list opens turned to it (patch 0067)"),
        Scenario(f"{_v}-p79-memory-invalid", _v, "full", timeout=300, log_only=True, phases=_m,
                 env=_P79_ENV, cfg=(("battle_menu_memory", "7"),),
                 expect_log=((0, r"DW3_FORCE_ENEMY_HP"),),
                 forbid_log=((0, _P79_LOG),),
                 note="a value other than 0, 1, 2 is Off (patch 0067)"),
    )
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-p79-memory-two-battles", _v, "full", turbo=4, timeout=600, log_only=True,
                  phases=(f"{_v}-p79-two-battles",), env=_BATTLE_ENV,
                  cfg=(("battle_menu_memory", "2"),),
                  expect_log=((0, r"remembered command Techniques(?s:.*)"
                                  r"WFIGHTMN_endBattle result=1(?s:.*)"
                                  r"AUTO_DOOR requestMode=0x600(?s:.*)" + _P79_OPENS + r"(?s:.*)"
                                  r"WFIGHTMN_endBattle result=1"),),
                  note="the memory lasts across battles of one session: battle 2's first menu opens on Techniques (patch 0067)"))


def by_name() -> dict[str, Scenario]:
    return {s.name: s for s in SCENARIOS}
