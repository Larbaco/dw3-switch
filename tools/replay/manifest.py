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
    # The process is meant to end by itself before the recording's end (the
    # title menu's Quit game, P6.28): exit status 0 is the expected verdict.
    quits: bool = False
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
           r"encounter_rate=0 text_speed=0 battle_menu_memory=0\)")
_OPT_P2 = (r"dw3-options: \S*port_options\.cfg \(widescreen=0 fast_forward=3 restart_hotkey=1 "
           r"music_volume=10 sfx_volume=10 encounter_rate=0 text_speed=0 battle_menu_memory=0\)",)
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-options-persist", _v, "full", timeout=300,
                  phases=(f"{_v}-options-persist-p1", f"{_v}-options-persist-p2"),
                  expect_log=tuple((0, r) for r in _OPT_P1) + tuple((1, r) for r in _OPT_P2),
                  note="Options toggled in one process are read back by the next (P9.4)"))

# P6.17: Music volume 10 -> 7 and SFX volume 10 -> 3 in phase 1 (Left on the
# volume rows), read back by phase 2 from the same config dir.
_VOL_P1 = (r"dw3-options: no \S*port_options\.cfg; defaults",
           r"dw3-options: saved \S*port_options\.cfg \(widescreen=1 fast_forward=2 restart_hotkey=0 "
           r"music_volume=7 sfx_volume=3 encounter_rate=0 text_speed=0 battle_menu_memory=0\)")
_VOL_P2 = (r"dw3-options: \S*port_options\.cfg \(widescreen=1 fast_forward=2 restart_hotkey=0 "
           r"music_volume=7 sfx_volume=3 encounter_rate=0 text_speed=0 battle_menu_memory=0\)",)
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
                      r"music_volume=10 sfx_volume=10 encounter_rate=1 text_speed=2 battle_menu_memory=0\)",)
_PACING_PERSIST_P2 = (r"dw3-options: \S*port_options\.cfg \(widescreen=1 fast_forward=2 restart_hotkey=0 "
                      r"music_volume=10 sfx_volume=10 encounter_rate=1 text_speed=2 battle_menu_memory=0\)",)

# P7.9 Battle cursor row (the old file-only key battle_menu_memory): phase 1
# opens Options, goes down to the last row and steps Off -> Command ->
# Command+Tech (captures of both words, the widest value of the screen);
# phase 2 is a new process that opens Options and goes to the row again.
_BC_P1 = (r"dw3-port-options: battle cursor command(?!\+)(?s:.*)dw3-port-options: battle cursor command\+technique(?s:.*)"
          r"dw3-options: saved \S*port_options\.cfg \(widescreen=1 fast_forward=2 restart_hotkey=0 "
          r"music_volume=10 sfx_volume=10 encounter_rate=0 text_speed=0 battle_menu_memory=2\)",)
_BC_P2 = (r"dw3-options: \S*port_options\.cfg \(widescreen=1 fast_forward=2 restart_hotkey=0 "
          r"music_volume=10 sfx_volume=10 encounter_rate=0 text_speed=0 battle_menu_memory=2\)",)
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-battle-cursor-persist", _v, "full", timeout=300,
                  phases=(f"{_v}-battle-cursor-persist-p1", f"{_v}-battle-cursor-persist-p2"),
                  expect_log=tuple((0, r) for r in _BC_P1) + tuple((1, r) for r in _BC_P2),
                  forbid_log=((0, r"is not valid"), (1, r"is not valid")),
                  note="Battle cursor Off -> Command -> Command+Tech set on the Options screen is read back by the next process (P7.9)"))
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-pacing-persist", _v, "full", timeout=300, log_only=True,
                  phases=(f"{_v}-pacing-persist-p1", f"{_v}-pacing-persist-p2"),
                  expect_log=tuple((0, r) for r in _PACING_PERSIST_P1) +
                             tuple((1, r) for r in _PACING_PERSIST_P2),
                  note="Encounter rate 0.5x and Text speed Instant set in one process are read back by the next (P6.18)"))

# Options screen sound feedback: the log carries every sound the screen plays
# with the SFX volume it was played at (voice is the SPU voice of the key-on).
for _v in ("us", "eu"):
    _snd = lambda kind, vol: rf"dw3-port-options: sound {kind} \(sfx volume {vol}, voice -?[0-9]+\)"
    _add(Scenario(f"{_v}-options-sound", _v, "full", timeout=300, log_only=True,
                  expect_log=((0, _snd("cursor", 10) + r"(?s:.*)" + _snd("select", 9) + r"(?s:.*)" +
                                  _snd("select", 8) + r"(?s:.*)" + r"encounter rate 1\.5x(?s:.*)" +
                                  _snd("select", 8) + r"(?s:.*)" + _snd("cancel", 8)),),
                  forbid_log=((0, r"sound \w+ not played"),),
                  note="cursor ticks, each SFX volume change plays the choice sound at the new level, Triangle cancels"))

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

# Instant and a page that scrolls (patch 0068): the full page is held, then the
# new line appears whole; Fast never logs a scroll.
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-text-scroll-instant", _v, "full", turbo=4, timeout=600, log_only=True,
                  phases=(f"{_v}-text-scroll",), cfg=(("text_speed", "2"),),
                  expect_log=((0, r"dw3-pacing: text scroll 1 after a hold of 12 frame\(s\) \(text speed instant\)(?s:.*)"
                                  r"dw3-pacing: text scroll 4 after a hold of 12 frame\(s\) \(text speed instant\)"),),
                  note="Instant: a full page is held 12 frames, then the new line shows at once (P6.18)"),
         Scenario(f"{_v}-text-scroll-fast", _v, "full", turbo=4, timeout=600, log_only=True,
                  phases=(f"{_v}-text-scroll",), cfg=(("text_speed", "1"),),
                  expect_log=((0, r"text page typed in [0-9]+ frame\(s\) \(text speed fast\)"),),
                  forbid_log=((0, r"dw3-pacing: text scroll"),),
                  note="Fast types the scrolled lines as before and never holds a page (P6.18)"))

# The area name banner types at delay 5 (not a dialogue delay) and is left alone
# by Text speed. The rc6 OLED log's "text page typed in 103 frame(s)" / "slow
# Instant page" lines were these windows, timed as dialogue pages because the
# game gave them the address of a freed dialogue window. DW3_PACING_TRACKED=256
# makes the port remember every dialogue window it saw, which is how that
# collision is forced on the PC build (the Switch heap makes it happen by itself
# after a battle report); the run of the opening and the door tour opens the
# banner after many dialogue windows.
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-banner-instant", _v, "full", turbo=4, timeout=1200, log_only=True,
                  phases=(f"{_v}-banner-walk",), cfg=(("text_speed", "2"),),
                  env=(("DW3_PACING_TRACKED", "256"),),
                  expect_log=((0, r"dw3-pacing: text typed at delay 5 is not a dialogue page(?s:.*)"
                                  r"dw3-pacing: text typed at delay 5 is not a dialogue page"),),
                  forbid_log=((0, r"slow Instant page"),),
                  note="Instant: the area name banner is not timed as a dialogue page (P6.18, rc6 log)"))

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

# Music loops (psyz libsnd NRPN loop count, docs/P3.6_AUDIO.md). The game plays
# every song with SsSepPlay(l_count 1), so the field music only keeps going if
# the reader honours the song's own NRPN loop (CC#99 = 20 / CC#6 count 127 /
# CC#99 = 30). The recording gives no input: the field music runs for 3000
# game frames, three to four passes at DW3_TURBO=4 (US 3, EU 4). Before the
# fix the log shows one `SEP end-of-track` shortly after frame 600 and the
# music is silent after it.
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-music-loop", _v, "full", turbo=4, timeout=300, log_only=True,
                  env=_ENC_ENV, cfg=(("encounter_rate", "2"),),
                  expect_log=((0, r"SEP loop count 127 seq=\d+ tr=\d+(?s:.*)"
                                  r"SEP loop end -> loop start \(count infinite\) seq=\d+ tr=\d+(?s:.*)"
                                  r"SEP loop end -> loop start \(count infinite\) seq=\d+ tr=\d+"),),
                  forbid_log=((0, r"SEP end-of-track"),),
                  note="field music jumps back at its loop end at least twice (US 3, EU 4 times in 3000 frames) and never reaches end-of-track"))

# The battle theme is the same bug: a long song (bank 2 on the US disc) whose
# NRPN loop never repeated, so a battle was silent after one pass. The walk is
# the Encounter rate 1x one (a random battle after ~100 footsteps), then no
# input: the log shows `SEP loop count` after FIGHTSTG_start and a jump back.
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-battle-music-loop", _v, "full", turbo=4, timeout=600, log_only=True,
                  phases=(f"{_v}-battle-music",), env=_ENC_ENV,
                  expect_log=((0, r"FIGHTSTG_start(?s:.*)"
                                  r"SEP loop count 127 seq=\d+ tr=0(?s:.*)"
                                  r"SEP loop end -> loop start \(count infinite\) seq=\d+ tr=0"),),
                  forbid_log=((0, r"FIGHTSTG_start(?s:.*)SEP end-of-track seq=\d+ tr=0"),),
                  note="the battle theme jumps back at its loop end and never reaches end-of-track"))

# Music around a won battle. Two things were wrong on the host: the win jingle
# is SEP track 30 of the system bank, which the game plays as SsSepPlay(0, 30)
# and libsnd's table reaches as entry (1, 14), but the host's table rows were
# 0x100 bytes apart for 0xC0 byte entries, so every track past 15 (the jingle,
# the menu and battle blips) never played: the report was silent. The log must
# show the jingle's own loop (`seq=1 tr=14`, it is entry (1, 14)) while the
# report is up, and the field music (`seq=2 tr=0`) starting again after the
# report and looping. A random battle (Encounter rate 1x walk of the
# battle-music scenario), forced enemy and party (Kunemon, Kotemon), won with
# Cross. PC only (env). Build from before the fix: no `seq=1 tr=14` line.
_VICTORY_ENV = _ENC_ENV + (("DW3_FORCE_ENCOUNTER", "34"), ("DW3_FORCE_PARTY", "0"))
for _v, _ids in (("us", (0x158, 0x159, 0x1FA, 0x1C2)), ("eu", (0x166, 0x167, 0x208, 0x1D1))):
    _field, _fight, _menu, _report = _ids
    _add(Scenario(f"{_v}-victory-music", _v, "full", turbo=4, timeout=600, log_only=True,
                  env=_VICTORY_ENV,
                  expect_restore=((_field, M), (_fight, M), (_menu, S), (_report, M), (_field, M)),
                  expect_log=((0, r"WFIGHTMN_endBattle result=1(?s:.*)"
                                  r"STFGTREP_start(?s:.*)"
                                  r"SEP loop count 127 seq=1 tr=14(?s:.*)"
                                  r"STFGTREP return(?s:.*)"
                                  r"SEP loop count 127 seq=2 tr=0(?s:.*)"
                                  r"SEP loop end -> loop start \(count infinite\) seq=2 tr=0(?s:.*)"
                                  r"SEP loop end -> loop start \(count infinite\) seq=2 tr=0"),),
                  forbid_log=((0, r"STFGTREP return(?s:.*)SEP end-of-track seq=2 tr=0"),),
                  note="won battle: the win jingle (track 30 = entry (1, 14)) plays in the report and the field music loops again after it"))

# Two consecutive battles, a technique used in each (patch 0061): every battle
# is a mode change (FIGHTSTG <- report/field), so WFIGHTMN is restored per
# battle, as the PS1 reloads it. PC only (needs the env debug aids).
_BATTLE_ENV = (("DW3_START_MODE", "0x600"), ("DW3_FORCE_ENCOUNTER", "34"),
               ("DW3_FORCE_PARTY", "0"), ("DW3_AUTO_DOOR_MODE", "0x600"),
               ("DW3_AUTO_DOOR_FRAMES", "120"))
_P77_ENV_REPORT = (("DW3_START_MODE", "0x600"), ("DW3_FORCE_ENCOUNTER", "34"),
                   ("DW3_FORCE_PARTY", "0"))
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158)), ("eu", (0x167, 0x208, 0x1D1, 0x166))):
    _fight, _menu, _report, _field = _ids
    _battle = ((_fight, M), (_menu, S), (_report, M), (_field, M))
    _add(Scenario(f"{_v}-two-battles", _v, "full", turbo=4, timeout=600,
                  env=_BATTLE_ENV, expect_restore=_battle * 2,
                  expect_log=((0, r"WFIGHTMN_endBattle result=1(?s:.*)"
                                  r"AUTO_DOOR requestMode=0x600(?s:.*)"
                                  r"WFIGHTMN_endBattle result=1"),),
                  note="two forced battles, Tech in each; WFIGHTMN restored per battle"))
    # P6.1 (patch 0074): the battle report keeps the widescreen gutter. The
    # gutter line (`dw3-widescreen: gutter N px per side`, logged on change)
    # must not say 0 between the report's overlay restore and the next one
    # (the field's); before the patch the report ran 4:3 (gutter 0). The two
    # checkpoints are the report's exp page and the next one.
    _add(Scenario(f"{_v}-battle-report", _v, "full", turbo=4, timeout=300,
                  env=_P77_ENV_REPORT, expect_restore=_battle,
                  expect_log=((0, r"WFIGHTMN_endBattle result=1"),),
                  forbid_log=((0, r"overlay restore file=0x%x kind=mode(?:(?!overlay restore)(?s:.))*?"
                                  r"gutter 0 px per side" % _report),),
                  note="the battle report draws with the widescreen gutter (patch 0074)"))

# Post-battle pacing (preview.31-rc1 OLED: "even with turbo on and Text speed
# Instant, the post-battle text still runs slowly"). Fast-forward used to drop
# to 1x for the battle report (its own mode file, STFGTREP), and the area name
# banner that opens the field after the battle typed its names at the game's
# speed in every Text speed. Now the report keeps the battle's speed and
# Instant shows the banner's names whole (port/options/turbo.c,
# port/pacing). The forced battle of p77 (Tech once, Cross to the end, the
# report and the banner). `instant`: turbo 4x and Instant; the turbo line
# `speed 1x` between the report's start and its return is forbidden (a build
# without the fix logs it at STFGTREP_start), the report's pages show in 1-2
# frames and both banner names are shown whole. `normal`: no turbo, Normal: the
# game's own pacing, no turbo line at all and the banner left alone. PC only.
_REPORT_AT_1X = r"STFGTREP_start(?:(?!STFGTREP return)(?s:.))*?dw3-turbo: speed 1x"
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158)), ("eu", (0x167, 0x208, 0x1D1, 0x166))):
    _fight, _menu, _report, _field = _ids
    _battle = ((_fight, M), (_menu, S), (_report, M), (_field, M))
    _add(Scenario(f"{_v}-postbattle-instant", _v, "full", turbo=4, timeout=300, log_only=True,
                  phases=(f"{_v}-p77-battle",), env=_P77_ENV_REPORT, cfg=(("text_speed", "2"),),
                  expect_restore=_battle,
                  expect_log=((0, r"dw3-turbo: speed 4x(?s:.*)STFGTREP_start(?s:.*)"
                                  r"text page typed in [12] frame\(s\) \(text speed instant\)(?s:.*)"
                                  r"STFGTREP return(?s:.*)"
                                  r"\(area name banner\): shown whole \(text speed instant\)(?s:.*)"
                                  r"\(area name banner\): shown whole \(text speed instant\)"),),
                  forbid_log=((0, _REPORT_AT_1X), (0, r"slow Instant page"),
                              (0, r"area name banner\): text speed leaves it alone")),
                  note="turbo keeps running in the battle report; Instant shows the area banner's names whole"),
         Scenario(f"{_v}-postbattle-normal", _v, "full", timeout=300, log_only=True,
                  phases=(f"{_v}-p77-battle",), env=_P77_ENV_REPORT,
                  expect_restore=_battle,
                  expect_log=((0, r"STFGTREP_start(?s:.*)STFGTREP return(?s:.*)"
                                  r"\(area name banner\): text speed leaves it alone(?s:.*)"
                                  r"\(area name banner\): text speed leaves it alone"),),
                  forbid_log=((0, r"dw3-turbo: speed"), (0, r"shown whole")),
                  note="no turbo, Normal: the report and the banner at the game's own pacing"))


# The file cache runs out of slots (patches 0095 hook, 0096 fix). The preview.31-rc1
# OLED crash: after about two hours of play all 64 slots of the file cache were
# taken (the host heap is 8 MiB, so nothing evicted, unlike the PS1's 1.33 MB)
# and the battle report's next requestFile wrote through a NULL slot. The
# DW3_FILE_CACHE_SLOTS hook makes the two-battles path fill a small cache in one
# run. Before 0096 the second battle ended in SIGSEGV in requestFile (CRASH);
# now the log must show the eviction and both battles end. PC only (env), log
# only, the two-battles path without checkpoints.
_FC_SLOTS = "16"
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158)), ("eu", (0x167, 0x208, 0x1D1, 0x166))):
    _fight, _menu, _report, _field = _ids
    _battle = ((_fight, M), (_menu, S), (_report, M), (_field, M))
    _add(Scenario(f"{_v}-file-cache-full", _v, "full", turbo=4, timeout=600, log_only=True,
                  env=_BATTLE_ENV + (("DW3_FILE_CACHE_SLOTS", _FC_SLOTS),),
                  expect_restore=_battle * 2,
                  expect_log=((0, r"DW3_FILE_CACHE_SLOTS=%s of 64(?s:.*)"
                                  r"dw3-file-cache: all %s slots taken, evicted the least recently used file(?s:.*)"
                                  r"WFIGHTMN_endBattle result=1(?s:.*)"
                                  r"AUTO_DOOR requestMode=0x600(?s:.*)"
                                  r"WFIGHTMN_endBattle result=1" % (_FC_SLOTS, _FC_SLOTS)),),
                  forbid_log=((0, r"FATAL all \d+ slots taken"),),
                  note="two forced battles with a 16-slot file cache: it fills, evicts, and both end (patches 0095, 0096)"))


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


# Field menu -> Items after a battle (patches 0069 fix, 0070 hook): the Items
# list used to crash the host (NULL ItemInfo in getItemCategory) as soon as the
# bag held one item, which is the owner's preview.30-rc6 OLED crash. A forced
# EVENT_BATTLE in the Bulk Swamp ends at once (DW3_FORCE_WIN), DW3_FORCE_ITEMS
# fills the bag. The same recording runs at Text speed Instant (the owner's
# setting) and Normal. PC only (env), log only: the verdicts are that the
# process reaches the end of the recording (a crash is CRASH), the STSTATUS
# overlay restore after the battle's, and the marker the guard of patch 0069
# prints for the frames that used to fault.
_ITEMS_ENV = (("DW3_START_MODE", "0x233"), ("DW3_PROGRESS", "6"),
              ("DW3_FORCE_NPC_ACTION", "0x7400"), ("DW3_FORCE_ENCOUNTER", "34"),
              ("DW3_FORCE_PARTY", "0"), ("DW3_FORCE_WIN", "1"),
              ("DW3_FORCE_ITEMS", "1-402"))
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158, 0x1F9)),
                 ("eu", (0x167, 0x208, 0x1D1, 0x166, 0x207))):
    _fight, _menu, _report, _field, _status = _ids
    for _suffix, _cfg, _speed in (("", (("text_speed", "2"),), "Instant"),
                                  ("-normal", (("text_speed", "0"),), "Normal")):
        _add(Scenario(f"{_v}-items-screen{_suffix}", _v, "full", timeout=300, log_only=True,
                      phases=(f"{_v}-items-screen",), env=_ITEMS_ENV, cfg=_cfg,
                      expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M), (_status, M)),
                      expect_log=((0, r"DW3_FORCE_ITEMS items 1\.\.402 x3(?s:.*)"
                                      r"dw3: items screen: panel icon skipped, no item chosen yet \(patch 0069\)"),),
                      note=f"the Items list opens with items in the bag after a battle, Text speed {_speed} (patch 0069)"))

# P6.1 (patch 0075): the field menu's block dissolve into and out of the Items
# screen covers the side gutters. The env of the items-screen scenarios gives
# the party (a forced event battle), encounters are Off so nothing interrupts
# the field. Checkpoints half way through both dissolves and on three menu
# screens; references come from the patched build (the gutters of the
# dissolve frames differ without it). PC only (env).
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158, 0x1F9)),
                 ("eu", (0x167, 0x208, 0x1D1, 0x166, 0x207))):
    _fight, _menu, _report, _field, _status = _ids
    _add(Scenario(f"{_v}-items-menu", _v, "full", turbo=4, timeout=300,
                  phases=(f"{_v}-items-menu",), env=_ITEMS_ENV, cfg=(("encounter_rate", "2"),),
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M),
                                  (_status, M), (_field, M)),
                  forbid_log=((0, r"overlay restore file=0x%x kind=mode(?:(?!overlay restore)(?s:.))*?"
                                  r"gutter 0 px per side" % _status),),
                  note="field menu -> Items and back: the dissolve spans 16:9 (patch 0075)"))

# P6.1 (patch 0076): the field menu's screens re-anchored to the 16:9 edges.
# The env and cfg of items-menu; one recording through the item screen and
# list, the field menu over the field and in STSTATUS, Sort, Map, Techniques,
# Status (Digivolve, equipment) and Card folders, a checkpoint on each.
# References come from the patched build. PC only (env).
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158, 0x1F9)),
                 ("eu", (0x167, 0x208, 0x1D1, 0x166, 0x207))):
    _fight, _menu, _report, _field, _status = _ids
    _add(Scenario(f"{_v}-menu-screens", _v, "full", turbo=4, timeout=400,
                  phases=(f"{_v}-menu-screens",), env=_ITEMS_ENV, cfg=(("encounter_rate", "2"),),
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M),
                                  (_status, M), (_field, M)),
                  forbid_log=((0, r"overlay restore file=0x%x kind=mode(?:(?!overlay restore)(?s:.))*?"
                                  r"gutter 0 px per side" % _status),),
                  note="the field menu's screens in widescreen: panels at the 16:9 edges (patch 0076)"))

# P6.1 (patches 0080, 0081, 0082): the three screens the owner's preview.31-rc1
# feedback named. PC only (env), references from the patched build.
#  map-screen: the env and cfg of menu-screens; Map opens at frame 4000 and the
#    cursor goes to every edge (the map is drawn whole and centred, patch 0080).
#  battle-results: the forced battle of battle-report, slower Crosses after
#    frame 1000, so the report's header bar (patch 0081) stays up.
#  save-screen: the memory card's save mode forced over the field
#    (DW3_FORCE_MENU), before and during the open animation (the slot strip
#    parked in the right gutter), the strip sliding in, the saved slot
#    (patch 0082).
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158, 0x1F9)),
                 ("eu", (0x167, 0x208, 0x1D1, 0x166, 0x207))):
    _fight, _menu, _report, _field, _status = _ids
    _add(Scenario(f"{_v}-map-screen", _v, "full", turbo=4, timeout=500,
                  phases=(f"{_v}-map-screen",), env=_ITEMS_ENV, cfg=(("encounter_rate", "2"),),
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M),
                                  (_status, M)),
                  forbid_log=((0, r"overlay restore file=0x%x kind=mode(?:(?!overlay restore)(?s:.))*?"
                                  r"gutter 0 px per side" % _status),),
                  note="the Map screen in widescreen: whole map centred, cursor to every edge (patch 0080)"))
    _add(Scenario(f"{_v}-battle-results", _v, "full", turbo=4, timeout=300,
                  env=_P77_ENV_REPORT,
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M)),
                  expect_log=((0, r"WFIGHTMN_endBattle result=1"),),
                  note="the battle results' header bar hangs from the 16:9 left edge (patch 0081)"))
_SAVE_ENV = (("DW3_START_MODE", "0x233"), ("DW3_PROGRESS", "6"),
             ("DW3_FORCE_MENU", "0xC00:0x80000000"))
for _v, _ids in (("us", (0x158, 0x1E3)), ("eu", (0x166, 0x1EE))):
    _field, _mcard = _ids
    _add(Scenario(f"{_v}-save-screen", _v, "full", timeout=400,
                  phases=(f"{_v}-save-screen",), env=_SAVE_ENV,
                  expect_restore=((_field, M), (_mcard, M)),
                  forbid_log=((0, r"overlay restore file=0x%x kind=mode(?:(?!overlay restore)(?s:.))*?"
                                  r"gutter 0 px per side" % _mcard),),
                  note="the save screen in widescreen: no card in the right gutter, header and slots at the 16:9 edges (patch 0082)"))


# P6.1 (patches 0100-0104): the card album, the deck editor, the card packs
# shop, the gym and the Digimon Lab in widescreen. Each screen is forced over
# the field (DW3_FORCE_MENU=<mode>); the album and the deck editor get a
# filled collection (DW3_FORCE_CARDS, patch 0100). PC only (env); the
# references come from the patched build. The widescreen check: the overlay
# must not run with "gutter 0 px per side".
_CARD_ENV = (("DW3_START_MODE", "0x233"), ("DW3_PROGRESS", "6"))
_CARDS = (("DW3_FORCE_CARDS", "1-120:2"),)
# (name, DW3_FORCE_MENU mode, extra env, US overlay, EU overlay, note)
_WS_SCREENS = (
    ("card-album", "0x1200", _CARDS, 0x177, 0x185,
     "the card album: background, plates and card details reach the 16:9 edges (patch 0101)"),
    ("deck-editor", "0x400", _CARDS, 0x1BC, 0x1CA,
     "the deck editor: folders, options, editor, card info and the list of cards owned (patch 0101)"),
    ("card-shop", "0x1300", (), 0x1C0, 0x1CE,
     "the card packs shop and Buy One Card (patch 0102)"),
    ("gym", "0xA00", (), 0x1F6, 0x1F2,
     "the gym: the party, the training list and a training (patch 0103)"),
    ("digimon-lab", "0xD00", (), 0x1DF, 0x1EC,
     "the Digimon Lab: the menu, the party and its details (patch 0104)"),
    ("lab-digivolve", "0xD00", (), 0x1DF, 0x1EC,
     "the Digimon Lab's Digivolve screens (patch 0104)"),
)
for _v, _field in (("us", 0x158), ("eu", 0x166)):
    for _name, _mode, _extra, _us, _eu, _note in _WS_SCREENS:
        _ovl = _us if _v == "us" else _eu
        _add(Scenario(f"{_v}-{_name}", _v, "full", timeout=400,
                      env=_CARD_ENV + (("DW3_FORCE_MENU", _mode),) + _extra,
                      expect_restore=((_field, M), (_ovl, M)),
                      forbid_log=((0, r"overlay restore file=0x%x kind=mode(?:(?!overlay restore)(?s:.))*?"
                                      r"gutter 0 px per side" % _ovl),),
                      note=_note))


# P6.20 Cheats screen (docs/PORT_OPTIONS.md "Cheats"), log only. The screen
# itself: two phases on one config dir (no env, so Ryubing runs it too): the
# master switch and Instant win are saved and read back, Fill bag and Battle
# now are not. The cheats: the Encounter rate field (env), the one-shots armed
# by the PC hook DW3_CHEATS_ARM, the toggles from the option file. Each has a
# master-off twin where nothing may happen.
_CHEAT_ON = (("cheat_enabled", "1"),)
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-cheats-screen", _v, "full", timeout=300, log_only=True,
                  phases=(f"{_v}-cheats-screen-p1", f"{_v}-cheats-screen-p2"),
                  expect_log=((0, r"dw3-cheats: cheats off \(instant win off\)"),
                              (0, r"dw3-cheats: open \(master off, fill bag off, instant win off, "
                                  r"battle now off, 0 marked save\(s\)\)"),
                              (0, r"dw3-options: saved cheat_enabled=1(?s:.*)"
                                  r"dw3-cheats: fill bag ready(?s:.*)"
                                  r"dw3-options: saved cheat_instant_win=1(?s:.*)"
                                  r"dw3-cheats: battle now ready(?s:.*)"
                                  r"dw3-cheats: close \(back to Options\)"),
                              (1, r"dw3-cheats: cheats ON \(instant win on\)"),
                              (1, r"dw3-cheats: open \(master on, fill bag off, instant win on, "
                                  r"battle now off, 0 marked save\(s\)\)")),
                  forbid_log=((0, r"is not valid"), (1, r"is not valid")),
                  note="Cheats screen: toggles saved and read back, one-shots not kept (P6.20)"))
    _add(Scenario(f"{_v}-cheat-fill-bag", _v, "full", timeout=300, log_only=True,
                  phases=(f"{_v}-cheat-fill-bag",),
                  env=_ENC_ENV + (("DW3_CHEATS_ARM", "fill_bag"),),
                  cfg=_CHEAT_ON + (("encounter_rate", "2"),),
                  expect_log=((0, r"dw3-cheats: fill bag: items 43\.\.359 x99 \(used up\)(?s:.*)"
                                  r"DW3_FORCE_ITEMS items 43\.\.359 x99"),),
                  note="Fill bag: the next field menu screen fills the bag through patch 0070's hook"),
         Scenario(f"{_v}-cheat-fill-bag-off", _v, "full", timeout=300, log_only=True,
                  phases=(f"{_v}-cheat-fill-bag",),
                  env=_ENC_ENV + (("DW3_CHEATS_ARM", "fill_bag"),),
                  cfg=(("encounter_rate", "2"),),
                  expect_log=((0, r"dw3-cheats: cheats off"),),
                  forbid_log=((0, r"DW3_FORCE_ITEMS items"), (0, r"fill bag: items")),
                  note="Fill bag with the master switch off: the bag is not touched"),
         Scenario(f"{_v}-cheat-battle-now", _v, "full", timeout=300, log_only=True,
                  phases=(f"{_v}-encounter-walk",),
                  env=_ENC_ENV + (("DW3_CHEATS_ARM", "battle_now"),),
                  cfg=_CHEAT_ON + (("encounter_rate", "2"),),
                  expect_log=((0, r"dw3-cheats: battle now: a random battle starts on this footstep "
                                  r"\(used up\)(?s:.*)FIGHTSTG_start"),),
                  forbid_log=((0, r"random battle after"),),
                  note="Battle now: a random battle on the first footstep, even at Encounter rate OFF"),
         Scenario(f"{_v}-cheat-battle-now-off", _v, "full", timeout=300, log_only=True,
                  phases=(f"{_v}-encounter-walk",),
                  env=_ENC_ENV + (("DW3_CHEATS_ARM", "battle_now"),),
                  cfg=(("encounter_rate", "2"),),
                  expect_log=((0, r"dw3-pacing: encounters off: 129 footstep\(s\) in battle areas ignored"),),
                  forbid_log=((0, r"FIGHTSTG_start"), (0, r"battle now:")),
                  note="Battle now with the master switch off: Encounter rate OFF still holds"),
         Scenario(f"{_v}-cheat-instant-win", _v, "full", turbo=4, timeout=600, log_only=True,
                  phases=(f"{_v}-battle-music",), env=_ENC_ENV,
                  cfg=_CHEAT_ON + (("cheat_instant_win", "1"),),
                  expect_log=((0, r"FIGHTSTG_start(?s:.*)"
                                  r"dw3-cheats: instant win \(battle 1 of this run\)(?s:.*)"
                                  r"WFIGHTMN_endBattle result=1"),),
                  note="Instant win: the random battle ends as won on its first turn, no input"),
         Scenario(f"{_v}-cheat-instant-win-off", _v, "full", turbo=4, timeout=600, log_only=True,
                  phases=(f"{_v}-battle-music",), env=_ENC_ENV,
                  cfg=(("cheat_instant_win", "1"),),
                  expect_log=((0, r"FIGHTSTG_start"),),
                  forbid_log=((0, r"instant win \(battle"), (0, r"WFIGHTMN_endBattle")),
                  note="Instant win saved but the master switch off: the battle goes on"))

# P6.20 PR 2 (page 2 of the Cheats screen: Money, Warp, Give item). The screen:
# one phase, what was armed is in the log. The cheats: the one-shots armed by
# DW3_CHEATS_ARM on the Encounter rate field (Bulk Swamp, mode 0x233), the
# first idle frame of the field applies them (patches 0118-0120); the
# recording is the Fill bag one (Start, Items, the Items list). Each has a
# master-off twin where nothing may happen.
for _v in ("us", "eu"):
    _bag = (f"{_v}-cheat-fill-bag",)
    _add(Scenario(f"{_v}-cheats-screen2", _v, "full", timeout=300, log_only=True,
                  phases=(f"{_v}-cheats-screen2",),
                  expect_log=((0, r"dw3-cheats: cheats off \(instant win off\)"),
                              (0, r"dw3-options: saved cheat_enabled=1(?s:.*)"
                                  r"dw3-cheats: money \+10000(?s:.*)"
                                  r"dw3-cheats: money \+100000(?s:.*)"
                                  r"dw3-cheats: warp to mode 0x200 \(area 1\)(?s:.*)"
                                  r"dw3-cheats: warp to mode 0x206 \(area 2\)(?s:.*)"
                                  r"dw3-cheats: warp to mode 0x209 \(area 3\)(?s:.*)"
                                  r"dw3-cheats: give item ready: id 56 x19(?s:.*)"
                                  r"dw3-cheats: close \(back to Options\)"),),
                  forbid_log=((0, r"is not valid"),),
                  note="Cheats screen page 2: money, warp area, item id and count armed on the screen (PR 2)"),
         Scenario(f"{_v}-cheat-money", _v, "full", timeout=300, log_only=True, phases=_bag,
                  env=_ENC_ENV + (("DW3_CHEATS_ARM", "money_10k"),),
                  cfg=_CHEAT_ON + (("encounter_rate", "2"),),
                  expect_log=((0, r"dw3-cheats: money \+10000: 0 -> 10000 \(used up\)(?s:.*)"
                                  r"dw3: P6\.20 cheat money 0 -> 10000(?s:.*)"
                                  r"field menu open \(START\)"),),
                  note="Money +10000: GAME.money goes from 0 to 10000 at the first idle frame of the field"),
         Scenario(f"{_v}-cheat-money-off", _v, "full", timeout=300, log_only=True, phases=_bag,
                  env=_ENC_ENV + (("DW3_CHEATS_ARM", "money_10k"),),
                  cfg=(("encounter_rate", "2"),),
                  expect_log=((0, r"dw3-cheats: cheats off"),),
                  forbid_log=((0, r"dw3-cheats: money \+"), (0, r"P6\.20 cheat money")),
                  note="Money armed with the master switch off: GAME.money is not touched"),
         Scenario(f"{_v}-cheat-warp", _v, "full", timeout=300, log_only=True, phases=_bag,
                  env=_ENC_ENV + (("DW3_CHEATS_ARM", "warp=0x206"),),
                  cfg=_CHEAT_ON + (("encounter_rate", "2"),),
                  expect_log=((0, r"dw3-cheats: warp to mode 0x206 \(used up\)(?s:.*)"
                                  r"dw3: P6\.20 cheat warp from mode=0x233 to mode=0x206(?s:.*)"
                                  r"dw3: leaveField mode=0x206 arg=0(?s:.*)"
                                  r"dw3: mode=0x206 mode_entry(?s:.*)"
                                  r"field menu open \(START\) mode=0x206"),),
                  note="Warp to the Digimon Lab: the field arrives on the target and its menu opens (savable)"),
         Scenario(f"{_v}-cheat-warp-off", _v, "full", timeout=300, log_only=True, phases=_bag,
                  env=_ENC_ENV + (("DW3_CHEATS_ARM", "warp=0x206"),),
                  cfg=(("encounter_rate", "2"),),
                  expect_log=((0, r"dw3-cheats: cheats off"),),
                  forbid_log=((0, r"dw3-cheats: warp to"), (0, r"P6\.20 cheat warp"),
                              (0, r"dw3: mode=0x206 mode_entry")),
                  note="Warp armed with the master switch off: the player stays in the Bulk Swamp"),
         Scenario(f"{_v}-cheat-item", _v, "full", timeout=300, log_only=True, phases=_bag,
                  env=_ENC_ENV + (("DW3_CHEATS_ARM", "item=47:7"),),
                  cfg=_CHEAT_ON + (("encounter_rate", "2"),),
                  expect_log=((0, r"dw3-cheats: give item: id 47 x7 \(used up\)(?s:.*)"
                                  r"dw3: P6\.20 cheat item 47 0 -> 7(?s:.*)"
                                  r"field menu open \(START\)"),),
                  forbid_log=((0, r"DW3_FORCE_ITEMS items"),),
                  note="Give item 47 x7: the bag count goes from 0 to 7, the Items list opens on it"),
         Scenario(f"{_v}-cheat-item-off", _v, "full", timeout=300, log_only=True, phases=_bag,
                  env=_ENC_ENV + (("DW3_CHEATS_ARM", "item=47:7"),),
                  cfg=(("encounter_rate", "2"),),
                  expect_log=((0, r"dw3-cheats: cheats off"),),
                  forbid_log=((0, r"give item: id"), (0, r"P6\.20 cheat item")),
                  note="Give item armed with the master switch off: the bag is not touched"))

# P6.1 (patches 0085-0087, owner's preview.31-rc1 OLED notes): the title menu,
# the opening's news caption bar and the player name (Digimon Online
# registration) screen fill 16:9. The New Game path of `newgame-field` up to
# the end of the registration, widescreen seeded on. The title overlay must
# take the gutter (53) after its restore, and the player name overlay must
# never be at gutter 0 (it was 4:3 before 0087).
for _v, _title, _field, _subs, _plnmet in (
        ("us", 0x1C1, 0x158, (0x1DA, 0x1DB), 0x1F8),
        ("eu", 0x1D0, 0x166, (0x1E9, 0x1EA), 0x206)):
    _add(Scenario(f"{_v}-ws-title-intro", _v, "full", turbo=4, timeout=900,
                  cfg=(("widescreen", "1"),),
                  expect_restore=((_title, M), (_field, M), (_subs[0], S), (_subs[1], S),
                                  (_plnmet, M)),
                  expect_log=((0, r"overlay restore file=0x%x kind=mode(?:(?!overlay restore)(?s:.))*?"
                                  r"gutter 53 px per side" % _title),),
                  forbid_log=((0, r"overlay restore file=0x%x kind=mode(?:(?!overlay restore)(?s:.))*?"
                                  r"gutter 0 px per side" % _plnmet),),
                  note="title menu, news caption and registration screen in 16:9 (patches 0085-0087)"))

# Owner report (preview.31-rc1 OLED, EU): leaving the Underground Path the water
# of the next area is wrong until the screen scrolls down. Cause (patch 0111):
# the map tile in column 4, row 1 of the Central Sector streets (WSTAG202) counts
# 6 sprites in depth 0 and the port's clamp read the other two depths from the
# wrong place, drawing huge garbage sprites over the canal. The 16:9 field has
# that tile in view from the arrival point. DW3_FORCE_EXIT (patch 0110) takes
# the exit slot's destination (mode 0x201, pixel (240, 264), facing 7) after
# 100 idle frames in the path (mode 0x20B), no walk needed. Checkpoints:
# `after-exit`, `scroll-mid`, `scroll-end` (Down held). Without the fix the
# water is black (all three captures differ from the reference).
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-underground-exit", _v, "full", timeout=300,
                  cfg=(("widescreen", "1"),),
                  env=(("DW3_START_MODE", "0x20B"), ("DW3_PROGRESS", "6"),
                       ("DW3_FORCE_EXIT", "0x201:240:264:7:100")),
                  expect_log=((0, r"DW3_FORCE_EXIT leaveField mode=0x201 x=240 y=264 dir=7(?s:.*)"
                                  r"dw3-tile: tile 12 depth 0 counts 6 sprites"),),
                  forbid_log=((0, r"corrupt sprite count"),),
                  note="Underground Path exit: the canal water is drawn (patches 0110, 0111)"))

# P6.1 area-change iris and grid cover (patch 0115) and the encounter spiral, in
# 16:9. Door: boot in the Underground Path, DW3_FORCE_EXIT leaves it; checkpoints
# while the view closes and the grid closes over the picture (the gutters show
# the grid, the view starts 320 + 2 * 53 wide). Battle: the walk of
# encounter-walk; the spiral's swirl is 4:3 with black gutters (documented), the
# checkpoints keep it from changing unnoticed.
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-ws-door-iris", _v, "full", timeout=300,
                  cfg=(("widescreen", "1"),),
                  env=(("DW3_START_MODE", "0x20B"), ("DW3_PROGRESS", "6"),
                       ("DW3_FORCE_EXIT", "0x201:240:264:7:100")),
                  expect_log=((0, r"DW3_FORCE_EXIT leaveField mode=0x201 x=240 y=264 dir=7"),),
                  note="area-change iris and grid cover span 16:9 (patch 0115)"),
         Scenario(f"{_v}-door-iris43", _v, "full", timeout=300,
                  cfg=(("widescreen", "0"),),
                  env=(("DW3_START_MODE", "0x20B"), ("DW3_PROGRESS", "6"),
                       ("DW3_FORCE_EXIT", "0x201:240:264:7:100")),
                  expect_log=((0, r"DW3_FORCE_EXIT leaveField mode=0x201 x=240 y=264 dir=7"),),
                  note="4:3 iris: the empty drawing area closeField leaves on the map layer draws nothing (PSY-Z dw3-empty-scissor)"),
         Scenario(f"{_v}-ws-battle-spiral", _v, "full", turbo=4, timeout=600,
                  cfg=(("widescreen", "1"),),
                  env=(("DW3_START_MODE", "0x233"), ("DW3_PROGRESS", "6")),
                  expect_log=((0, r"FIGHTSTG_start"),),
                  note="encounter spiral in 16:9: swirl 4:3, gutters black (documented)"))


# P6.28 Quit game on the title menu and P6.27 Return to title in the field menu
# (patch 0123, port/options/confirm.c). Quit: Down to the fourth row, the Yes / No
# window with the cursor on No (checkpoints), No, again, Yes: the saves are
# flushed (`dw3-port-quit: saves flushed`) and the process ends with status 0
# before the recording's end. Return to title: the South Sector field with the
# Start, Down to the last row,
# the window, No, again, Yes: P6.9's reset, then the title (US) / the language
# select and the title (EU). rows7: the same field with the card folders row
# (the six-row panel stretched for the seventh), cancelled. PC only (env / early exit).
_RT_CFG = (("encounter_rate", "2"),)
_RT_ENV = _ITEMS_ENV[:-1]    # the party of the forced event battle, no bag
for _v, _title in (("us", 0x1C1), ("eu", 0x1D0)):
    _add(Scenario(f"{_v}-title-quit", _v, "full", timeout=300, quits=True,
                  expect_log=((0, r"dw3-port-confirm: open quit \(cursor on No\)(?s:.*)"
                                  r"dw3-port-confirm: No \(quit\); nothing changed(?s:.*)"
                                  r"dw3-port-confirm: open quit \(cursor on No\)(?s:.*)"
                                  r"dw3-port-confirm: cursor on Yes(?s:.*)"
                                  r"dw3-port-confirm: Yes \(quit\)(?s:.*)"
                                  r"dw3-port-quit: saves flushed; asking the host to quit"),),
                  forbid_log=((0, r"save flush failed"),),
                  note="title menu Quit game: No keeps the menu, Yes flushes the saves and ends the process (P6.28)"),
         Scenario(f"{_v}-return-title", _v, "full", turbo=4, timeout=600,
                  env=_RT_ENV, cfg=_RT_CFG,
                  expect_restore=((_title, M),),
                  expect_log=((0, r"dw3-port-confirm: open return to title \(cursor on No\)(?s:.*)"
                                  r"dw3-port-confirm: No \(return to title\); nothing changed(?s:.*)"
                                  r"dw3-port-confirm: open return to title \(cursor on No\)(?s:.*)"
                                  r"dw3-port-confirm: Yes \(return to title\)(?s:.*)"
                                  r"dw3-port-restart: requested from the field menu(?s:.*)"
                                  r"dw3-port-restart: saves flushed; returning to the boot screen"),),
                  forbid_log=((0, r"save flush failed"),),
                  note="field menu Return to title: No keeps the menu, Yes runs the P6.9 reset (P6.27)"),
         Scenario(f"{_v}-return-title-rows7", _v, "full", turbo=4, timeout=600,
                  env=_ITEMS_ENV, cfg=_RT_CFG,
                  expect_log=((0, r"dw3-port-confirm: open return to title \(cursor on No\)(?s:.*)"
                                  r"dw3-port-confirm: cancelled \(return to title\); nothing changed"),),
                  forbid_log=((0, r"dw3-port-restart"),),
                  note="the field menu with the card folders row and Return to title: seven rows, cancelled (P6.27)"))

# The Quit game plaque and the window in the other four EU languages (the
# lettering is authored, docs/PORT_OPTIONS.md "Quit and return to title").
for _lang in ("fr", "de", "it", "es"):
    _add(Scenario(f"eu-title-quit-{_lang}", "eu", "full", timeout=300,
                  expect_log=((0, r"dw3-port-confirm: open quit \(cursor on No\)(?s:.*)"
                                  r"dw3-port-confirm: cursor on Yes(?s:.*)"
                                  r"dw3-port-confirm: cancelled \(quit\); nothing changed"),),
                  forbid_log=((0, r"dw3-port-quit"),),
                  note=f"Quit game plaque and window in {_lang}; Triangle cancels, nothing is quit (P6.28)"))


def by_name() -> dict[str, Scenario]:
    return {s.name: s for s in SCENARIOS}
