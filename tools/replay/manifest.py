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
  nx     P9.7 Switch-build-only scenarios (Ryubing, replay NRO with the env baked
         in); only with --only
  single P6.24 launcher scenarios; only with --single or --only (need the
         launcher binary, --launcher / DW3_REPLAY_LAUNCHER)
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
    # P6.25 cross-region saves: the disc each phase runs on (same length as
    # phases), and extra environment per phase (same length as phases). A phase
    # runs the binary and runtime of its own region, with its own card folder
    # (one per region) and DW3_MEMCARD_IMPORT_DIR pointing at the other
    # region's folder, so the save made on one disc is imported on the other.
    # PC only: the Ryubing runner skips these.
    phase_versions: tuple[str, ...] = ()
    phase_env: tuple[tuple[tuple[str, str], ...], ...] = ()
    # After phase i, edit the save that phase wrote ("" = leave it): the runner
    # derives a synthetic variant of the game's own save at run time, for the
    # refusal cases. "progress:0x2D" sets GameState.progress (the EU extra
    # chapter) and "badsum" flips the slot checksum. Nothing is committed.
    phase_mutate: tuple[str, ...] = ()
    # P6.24 single build: run the phases through the region launcher instead
    # of a per-region binary. `launch` lists the region trees the launcher finds
    # under its runtime base ("us", "eu" or "us+eu"; symlinks to the runtimes);
    # `env` can force the region (DW3_REGION) or script the picker
    # (DW3_REGION_PICK_KEYS). The runner leaves DW3_RUNTIME unset, so the
    # launcher's choice decides which binary and runtime run. `version` is the
    # region that must come out. PC only.
    launch: str = ""
    # Compare the captures with the references of this other scenario instead
    # of the scenario's own (the launcher run must be bit-identical to the
    # plain per-region run, so no reference of its own is needed).
    ref_from: str = ""
    # P9.7 slice 2: checkpoints compared with a named checkpoint of another
    # scenario instead of their own reference: (shot, scenario, its shot). The
    # battle's excursion run takes the plain run's pictures a few frames later
    # and must match them; run the plain scenario with --update first.
    ref_map: tuple[tuple[str, str, str], ...] = ()
    # MOD map upscale: "synthetic" makes the runner write the synthetic map pack
    # (tools/mods/make_synthetic_pack.py, gradients, no game art) into the run's
    # config folder (`mods/`) before the first phase. PC only.
    mod_pack: str = ""
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


_add(Scenario("us-title", "us", "quick", expect_restore=((0x1C1, M),),
              expect_log=((0, r"dw3-title-menu: no autosave\.bin: plaques New Game, Options, "
                              r"Load Game \(slot list\), Quit game"),)))
for _lang in EU_LANGS:
    # P6.1 (patch 0177): the language select takes the 16:9 gutter; the
    # title that follows still has the four plaques (no auto-save file).
    _add(Scenario(f"eu-lang-{_lang}", "eu", "quick",
                  expect_log=((0, r"dw3-widescreen: gutter 53 px per side \(was -?\d+; 16:9\) mode file 0x165"
                                  r"(?s:.*)dw3-title-menu: no autosave\.bin: plaques New Game, Options, "
                                  r"Load Game \(slot list\), Quit game"),)))

# Patch 0187: Cross (the confirm button) picks the language as well as Start;
# the log names the button and the title follows.
_add(Scenario("eu-lang-cross-fr", "eu", "full",
              expect_log=((0, r"dw3-lang-select: language \d chosen by Cross \(confirm\)"
                              r"(?s:.*)dw3-title-menu: no autosave\.bin: plaques New Game, Options, "
                              r"Load Game \(slot list\), Quit game"),),
              note="the EU language select confirms with Cross"))

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
           r"dw3-port-options: group open display \(1 row\(s\), title\)(?s:.*)"
           r"dw3-port-options: group close display \(back to the group list\)(?s:.*)"
           r"dw3-port-options: group open gameplay \(5 row\(s\), title\)(?s:.*)"
           r"dw3-port-options: group close gameplay \(back to the group list\)(?s:.*)"
           r"dw3-port-options: close \(resume runOptions\)",
           r"dw3-options: saved \S*port_options\.cfg \(widescreen=0 fast_forward=3 restart_hotkey=1 music_volume=10 sfx_volume=10 "
           r"encounter_rate=0 text_speed=0 battle_menu_memory=0\)")
_OPT_P2 = (r"dw3-port-options: group open gameplay \(5 row\(s\), title\)",
           r"dw3-options: \S*port_options\.cfg \(widescreen=0 fast_forward=3 restart_hotkey=1 "
           r"music_volume=10 sfx_volume=10 encounter_rate=0 text_speed=0 battle_menu_memory=0\)",)
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-options-persist", _v, "full", timeout=300,
                  phases=(f"{_v}-options-persist-p1", f"{_v}-options-persist-p2"),
                  expect_log=tuple((0, r) for r in _OPT_P1) + tuple((1, r) for r in _OPT_P2),
                  note="Options toggled in one process are read back by the next (P9.4)"))

# P6.17: Music volume 10 -> 7 and SFX volume 10 -> 3 in phase 1 (Left on the
# volume rows), read back by phase 2 from the same config dir.
_VOL_P1 = (r"dw3-options: no \S*port_options\.cfg; defaults",
           r"dw3-port-options: group open sound \(2 row\(s\), title\)",
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
                  expect_log=((0, _snd("cursor", 10) + r"(?s:.*)" + _snd("select", 10) +
                                  r"(?s:.*)group open sound(?s:.*)" + _snd("cursor", 10) + r"(?s:.*)" +
                                  _snd("select", 9) + r"(?s:.*)" + _snd("select", 8) + r"(?s:.*)" +
                                  _snd("cancel", 8) + r"(?s:.*)group close sound(?s:.*)" +
                                  r"group open gameplay(?s:.*)encounter rate 1\.5x(?s:.*)" +
                                  _snd("select", 8) + r"(?s:.*)" + _snd("cancel", 8) + r"(?s:.*)" +
                                  _snd("cancel", 8)),),
                  forbid_log=((0, r"sound \w+ not played"),),
                  note="cursor ticks, opening a group and each SFX volume change play the choice sound at the new level, Triangle cancels"))

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

# P6.35 walkthrough, phase 1 (docs/P6.35_WALKTHROUGH.md): the step resolver on
# the real game state. The debug probe (patch 0186, DW3_WALKTHROUGH_PROBE) logs
# `dw3-walkthrough: progress P half H area A -> chapter C step S (rule R)` when
# the progress, the map or the flag changes. Three points of the first chapter:
# progress 0 in Kusanagi City's street corner (the real world, no map area),
# progress 2 in the Login Room, progress 3 in the Central Sector (Genji's flag
# 0x400D is clear: the step has a sub-condition, rule progress+flag).
for _v in ("us", "eu"):
    for _tag, _mode, _prog, _line in (
        ("p0", "0x2D7", "0", r"progress 0 half -1 area -1 -> chapter ch01 step ch01\.s01 \(rule progress\)"),
        ("p2", "0x207", "2", r"progress 2 half 0 area 19 -> chapter ch01 step ch01\.s03 \(rule progress\)"),
        ("p3", "0x200", "3", r"progress 3 half 0 area 19 -> chapter ch01 step ch01\.s04 \(rule progress\+flag\)"),
    ):
        _add(Scenario(f"{_v}-walkthrough-{_tag}", _v, "full", turbo=4, timeout=120, log_only=True,
                      phases=(f"{_v}-walkthrough",),
                      env=(("DW3_START_MODE", _mode), ("DW3_PROGRESS", _prog),
                           ("DW3_WALKTHROUGH_PROBE", "1")),
                      expect_log=((0, r"dw3-walkthrough: " + _line),),
                      forbid_log=((0, r"step not recognised|chapter - step -"),),
                      note="P6.35 phase 1: the resolver logs the step for this progress value"))

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


# P51 item 1: the selection bar of the battle's Items and technique lists
# (FIGHTSTG_drawCursorBar, a vsync callback that hands its ordering table to
# ContinueDraw; a no-op stub until this fix, so the lists had no marker). The
# forced battle of the p79 scenarios plus a full bag (DW3_FORCE_BATTLE_BAG,
# patch 0189). Checkpoints on both lists at several cursor lines. `-t4` runs
# turbo 4x (the pictures are the 1x run's), `-43` Widescreen off (own refs).
# PC only (env). The Switch renderer modes (gpu-tex, soft) are checked in
# Ryubing by hand (docs/P51_FEEDBACK.md item 1).
_BAR_ENV = _P79_ENV + (("DW3_FORCE_BATTLE_BAG", "1-402"),)
_BAR_SHOTS = ("battle-cmd-items", "items-line0", "items-line1", "items-line2", "tech-line0", "tech-line1")
_BAR_LOG = ((0, r"dw3-battle-cursor: open list=items(?s:.*)"
                r"dw3-battle-cursor: close list=items count=\d+ sel=2 bar=submitted(?s:.*)"
                r"dw3-battle-cursor: open list=tech(?s:.*)"
                r"dw3-battle-cursor: close list=tech count=\d+ sel=1 bar=submitted"),)
_BAR_FORBID = ((0, r"bar=dropped"),)
for _v in ("us", "eu"):
    _bar = (f"{_v}-battle-list-bar",)
    _add(
        Scenario(f"{_v}-battle-list-bar", _v, "full", timeout=300, phases=_bar, env=_BAR_ENV,
                 expect_log=_BAR_LOG, forbid_log=_BAR_FORBID,
                 note="the Items and technique lists show the cursor bar (P51 item 1, patch 0189)"),
        Scenario(f"{_v}-battle-list-bar-t4", _v, "full", turbo=4, timeout=300, phases=_bar, env=_BAR_ENV,
                 ref_map=tuple((n, f"{_v}-battle-list-bar", n) for n in _BAR_SHOTS),
                 expect_log=_BAR_LOG, forbid_log=_BAR_FORBID,
                 note="the same lists under turbo 4x: bar submitted, same pictures (P51 item 1)"),
        Scenario(f"{_v}-battle-list-bar-43", _v, "full", timeout=300, phases=_bar, env=_BAR_ENV,
                 cfg=(("widescreen", "0"),),
                 expect_log=_BAR_LOG, forbid_log=_BAR_FORBID,
                 note="the same lists with Widescreen off (4:3) (P51 item 1)"),
    )

# P51 item 1 on the Switch build in Ryubing (tier nx, only with --only; log
# judgement plus informational screenshots): the same recording, the replay NRO
# built with DW3_NX_TEST_ENV="DW3_START_MODE=0x600,DW3_FORCE_ENCOUNTER=34,
# DW3_FORCE_PARTY=0,DW3_FORCE_ENEMY_HP=2000,DW3_FORCE_BATTLE_BAG=1-402" (and
# DW3_MESH_SOFT=1 for the `-soft` one: the CPU renderer instead of the default
# gpu-tex mesh). A comma cannot go in that variable, so there are no extra
# techniques: the technique list has one line. The `mode=` token of the open
# line names the renderer the bar was submitted under.
for _v in ("us", "eu"):
    for _suffix, _mode in (("", "gpu-tex"), ("-soft", "soft")):
        _add(Scenario(f"{_v}-battle-list-bar-nx{_suffix}", _v, "nx", timeout=900,
                      phases=(f"{_v}-battle-list-bar",),
                      expect_log=((0, r"dw3-battle-cursor: open list=items mode=" + _mode + r" (?s:.*)"
                                      r"dw3-battle-cursor: close list=items count=\d+ sel=2 bar=submitted"),),
                      forbid_log=((0, r"bar=dropped"),),
                      note=f"the battle Items list's bar on the Switch build, renderer {_mode} (P51 item 1)"))

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

# P6.33 phase 1 (patch 0128): ZL / ZR (R2 / L2 in the recording) change the
# partner on the equip panel's slot list. The env and cfg of menu-screens;
# The party has 3 members: R2 goes 1 -> 2, R2 wraps 2 -> 0, L2 wraps back 0 -> 2,
# R2 again 2 -> 0. `-43` runs the same recording with Widescreen off (the 4:3 layout). The log
# line "partner switch" carries the bag and worn totals, which must be the
# same on every switch (switching writes nothing in GAME). References come
# from the patched build. PC only (env).
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158, 0x1F9)),
                 ("eu", (0x167, 0x208, 0x1D1, 0x166, 0x207))):
    _fight, _menu, _report, _field, _status = _ids
    for _suffix, _cfg in (("", (("encounter_rate", "2"),)),
                          ("-43", (("encounter_rate", "2"), ("widescreen", "0")))):
        _add(Scenario(f"{_v}-items-equip-switch{_suffix}", _v, "full", turbo=4, timeout=400,
                      phases=(f"{_v}-items-equip-switch",), env=_ITEMS_ENV, cfg=_cfg,
                      expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M),
                                      (_status, M)),
                      expect_log=((0, r"partner switch member 1 -> 2 .*bag=(\d+) worn=(\d+)"
                                      r"(?s:.*)partner switch member 2 -> 0 .*bag=\1 worn=\2"
                                      r"(?s:.*)partner switch member 0 -> 2 .*bag=\1 worn=\2"
                                      r"(?s:.*)partner switch member 2 -> 0 .*bag=\1 worn=\2"),),
                      note="R2 / L2 change the partner on the equip slot list, counts unchanged (patch 0128)"))

# P6.33 finding (patch 0180): Cross on equipment in the Items menu does nothing
# in the original game. The log line says so; run with items_partner_switch on
# (the default) and off (patch 0128's path out), the same line either way.
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158, 0x1F9)),
                 ("eu", (0x167, 0x208, 0x1D1, 0x166, 0x207))):
    _fight, _menu, _report, _field, _status = _ids
    # P6.34 patch 0188 turned equip-from-Items on by default, so these two run
    # with items_equip_from_items=0: the key-off proof that the original
    # behavior (and the 0180 line) is intact.
    for _suffix, _cfg in (("", (("encounter_rate", "2"), ("items_equip_from_items", "0"))),
                          ("-switch0", (("encounter_rate", "2"), ("items_equip_from_items", "0"),
                                        ("items_partner_switch", "0")))):
        _add(Scenario(f"{_v}-items-select-equip{_suffix}", _v, "full", timeout=300, log_only=True,
                      phases=(f"{_v}-items-select-equip",), env=_ITEMS_ENV, cfg=_cfg,
                      expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M), (_status, M)),
                      expect_log=((0, r"dw3-items-menu: Cross on \w+ \d+ \(list [1-4]\): the original game "
                                      r"does nothing here(?s:.*)dw3-items-menu: Cross on \w+ \d+ \(list [1-4]\)"),),
                      forbid_log=((0, r"dw3-items-menu: partner switch"),),
                      note="Cross on equipment in the Items menu: the original game does nothing (patch 0180)"))

# P6.34 phase 1 (patch 0188): the result and refusal lines in the four extra
# EU languages (draft words, native review pending; accents are icons).
for _lang, _id in (("fr", 3), ("it", 4), ("de", 5), ("es", 6)):
    _add(Scenario(f"eu-items-equip-refused-{_lang}", "eu", "full", turbo=4, timeout=400,
                  phases=("eu-items-equip-refused",),
                  env=_ITEMS_ENV + (("DW3_LANGUAGE", str(_id)),), cfg=(("encounter_rate", "2"),),
                  expect_log=((0, rf"LANGUAGE={_id} \(DW3_LANGUAGE\)(?s:.*)equip-from-items refused partner=\d+ item=92 "
                                  r"reason=partner-mask(?s:.*)equip-from-items done partner=0 slot=2"),),
                  note=f"the equip result and refusal lines in {_lang} (patch 0188)"))

# P6.34 phase 1 (patch 0188): equip from the Items menu, key on (the default).
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158, 0x1F9)),
                 ("eu", (0x167, 0x208, 0x1D1, 0x166, 0x207))):
    _fight, _menu, _report, _field, _status = _ids
    _add(Scenario(f"{_v}-items-equip-from-items", _v, "full", turbo=4, timeout=400,
                  phases=(f"{_v}-items-equip-from-items",), env=_ITEMS_ENV,
                  cfg=(("encounter_rate", "2"),),
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M), (_status, M)),
                  expect_log=((0, r"equip-from-items open item=92 kind=weapon list=1(?s:.*)"
                                  r"slot=2 rule=hand-empty replaces=0(?s:.*)"
                                  r"done partner=0 slot=2 item=92 bag=(\d+)->(\d+) worn=0->1(?s:.*)"
                                  r"list rebuilt count=\d+ cursor=0(?s:.*)"
                                  r"open item=92(?s:.*)cancelled at=partner"),),
                  forbid_log=((0, r"unmapped-kind|Cross on \w+ \d+ \(list"),),
                  note="Cross on equipment in the Items menu opens the partner step and equips (patch 0188)"))
    _eq_logs = {
        "refused": (r"open item=92(?s:.*)refused partner=6 item=92 reason=partner-mask(?s:.*)"
                    r"refused partner=7 item=92 reason=partner-mask(?s:.*)"
                    r"done partner=0 slot=2 item=92 bag=\d+->\d+ worn=0->1"),
        "replace": (r"slot=2 rule=hand-empty replaces=0(?s:.*)slot=3 rule=hand-empty replaces=0(?s:.*)"
                    r"slot=2 rule=hand-replace replaces=92(?s:.*)"
                    r"done partner=0 slot=2 item=96 bag=(\d+)->\1 worn=2->2(?s:.*)"
                    r"open item=\d+ kind=armor list=2(?s:.*)slot=0 rule=head(?s:.*)"
                    r"open item=\d+ kind=accessory list=3(?s:.*)slot=4 rule=acc-empty(?s:.*)"
                    r"done partner=0 slot=4 item=\d+ bag=\d+->\d+ worn=3->4"),
    }
    for _name in ("refused", "replace"):
        _add(Scenario(f"{_v}-items-equip-{_name}", _v, "full", turbo=4, timeout=600,
                      phases=(f"{_v}-items-equip-{_name}",), env=_ITEMS_ENV,
                      cfg=(("encounter_rate", "2"),),
                      expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M), (_status, M)),
                      expect_log=((0, _eq_logs[_name]),),
                      forbid_log=((0, r"unmapped-kind"),),
                      note="equip from the Items menu: " + (
                          "a partner that cannot wear the item is refused" if _name == "refused" else
                          "both hand slots full, then armor and an accessory") + " (patch 0188)"))

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
    # P6.32 phase 0 (patch 0146): the new full-screen map screen, opened with Square
    # from the Map; the references are the PC host overlay's picture. DW3_MAP_VERIFY
    # logs the cross-check of the captured map against the disc-built one.
    _add(Scenario(f"{_v}-map-view", _v, "full", turbo=4, timeout=600,
                  phases=(f"{_v}-map-view",), env=_ITEMS_ENV + (("DW3_MAP_VERIFY", "1"), ("DW3_MAP_SYNC", "1")),
                  cfg=(("encounter_rate", "2"),),
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M),
                                  (_status, M)),
                  # the reveal layer (the towns of the progress list and the visited
                  # area cells) is composed into the picture: at least one piece
                  # drawn, none refused; the right stick zooms (source=stick); the
                  # scaled images are keyed by the reveal hash
                  expect_log=((0, r"dw3-map: reveal layer: \d+ pieces from the game "
                                  r"\(\d+ towns \+ \d+ visited areas drawn\), [1-9]\d* drawn, "
                                  r"hash [0-9a-f]{8}\n"
                                  r"(?s:.*)dw3-map: open half=0 image=392x352 source=\w+ in \d+ ms "
                                  r"markers=\d+ home=\d+,\d+ turbo=1"
                                  r"(?s:.*)dw3-map: zoom source=button step"
                                  r"(?s:.*)scaler LANCZOS2 (?:built in \d+ ms \(1176x1056, reveal hash [0-9a-f]{8}\)"
                                  r"|loaded from cache in \d+ ms \(1176x1056, cache hit \S+, reveal hash [0-9a-f]{8}\))"
                                  r"(?s:.*)scaler AMD FSR 1 (?:built in|loaded from cache in) \d+ ms"
                                  r"(?s:.*)dw3-map: zoom source=stick \(axis 127\) zoom"
                                  r"(?s:.*)dw3-map: zoom source=stick \(axis -127\) zoom"
                                  r"(?s:.*)dw3-map: close after \d+ frames \(zoom: [1-9]\d* stick frames, "
                                  r"[1-9]\d* button presses, last source stick; reveal hash [0-9a-f]{8}\)"),
                  # the game's own elements (owner, after preview.45): the balloon on
                  # the current area, the cursor on a location (location id + name
                  # id), the game's name box shown, the cursor moved off and back,
                  # the HUD toggle hiding the name box
                              (0, r"dw3-map: sprite bank: \d+ of 12 frames built \(cursor ok, hover "
                                  r"cursor ok, balloon ok, balloon turned (?:ok|no \(US\))\)"
                                  r"(?s:.*)dw3-map: cursor on location id=([1-9]\d*) \(name id=\1, spot at \d+,\d+\)"
                                  r"(?s:.*)dw3-map: balloon at sprite \d+,\d+ \(image \d+,\d+\) on "
                                  r"location id=\1, frame 0x3[5b] \((?:plain|turned)\)"
                                  r"(?s:.*)dw3-map: name box shown for location id=\1 \(name id=\1\) at the "
                                  r"(?:top|bottom) of the picture, logical x \d+\.\.\d+ y \d+\.\.\d+"
                                  r"(?s:.*)dw3-map: cursor left location id=\1, free at \d+,\d+"
                                  r"(?s:.*)dw3-map: name box hidden \(cursor on no location\)"
                                  r"(?s:.*)dw3-map: cursor at rest at \d+,\d+, location id=\1 name id=\1"
                                  r"(?s:.*)dw3-map: HUD off \(name box hidden\)"
                                  r"(?s:.*)dw3-map: HUD on \(name box allowed\)"),
                              # owner, preview.48: the cursor and the balloon keep one size
                              (0, r"dw3-map: sprites at fixed size \(scale 1/Z, Z = zoom, "
                                  r"reference zoom 1\.0 = [\d.]+ px per source px; the reveal layer"),),
                  forbid_log=((0, r"dw3-map: capture failed|NOTHING DRAWN|not drawn|MISSING"),),
                  note="Square opens the new full-screen map: balloon, cursor, name box, zoom, scaler cycle, back (patch 0146)"))
    # MOD map upscale: the same recording with a synthetic pack in the config
    # folder. The pack replaces the map picture (1568x1408 texture, same
    # geometry), the reveal layer is the game's own cells drawn 4x larger, and the
    # log says so; references are this scenario's own (run with --update).
    _add(Scenario(f"{_v}-map-view-mod", _v, "full", turbo=4, timeout=600,
                  phases=(f"{_v}-map-view",), env=_ITEMS_ENV + (("DW3_MAP_SYNC", "1"),),
                  cfg=(("encounter_rate", "2"),), mod_pack="synthetic",
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M),
                                  (_status, M)),
                  expect_log=((0, r"dw3-map: mod: pack \"synthetic-test\" 0\.0\.1 loaded: "
                                  r"map_%s_h0@4x\.png 1568x1408, reveal [1-9]\d* pieces from the pack|"
                                  r"dw3-map: mod: pack \"synthetic-test\" 0\.0\.1 loaded: "
                                  r"map_%s_h0@4x\.png 1568x1408, reveal 0 pieces from the pack, [1-9]\d* at 1x nearest"
                                  % (_v, _v)),
                              (0, r"dw3-map: texture 1568x1408 queued for the GPU"),),
                  forbid_log=((0, r"dw3-map: mod: original map|dw3-map: capture failed|NOTHING DRAWN"),),
                  note="the map view with a mod pack: 4x texture, same geometry, silent fallback otherwise"))
    _add(Scenario(f"{_v}-map-view-43", _v, "full", turbo=4, timeout=600,
                  phases=(f"{_v}-map-view",), env=_ITEMS_ENV,
                  cfg=(("encounter_rate", "2"), ("widescreen", "0")),
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M),
                                  (_status, M)),
                  forbid_log=((0, r"dw3-map: open"),),
                  note="widescreen off: Square does nothing, the new screen never opens (owner decision 8)"))
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


# P6.25 cross-region saves: the game itself saves on one disc (the forced save
# screen, as save-screen) and the other disc's build loads that save from the
# Title menu (Load Game / Continue). The two phases run on different binaries
# and runtimes; each has its own card folder, and the second one imports the
# first one's (DW3_MEMCARD_IMPORT_DIR, set by the runner). Log verdicts only
# (no checkpoints, no references): the write, the import and conversion, the
# load of the right state, and the field reached with the saved mode.
_XR_FIELD = {"us": 0x158, "eu": 0x166}
_XR_MCARD = {"us": 0x1E3, "eu": 0x1EE}
_XR_WRITE = (0, r"STGMCARD_writeSave port=0 slot=0 status=1")
_XR_BAD = ((1, r"STGMCARD_loadSave (?:checksum|version|read) fail"),
           (1, r"cross-import slot=\d FAILED"))
for _a, _b in (("us", "eu"), ("eu", "us")):
    _add(Scenario(f"xr-{_a}-to-{_b}", _a, "full", timeout=600, log_only=True,
                  phases=(f"{_a}-xr-save", f"{_b}-xr-load"), phase_versions=(_a, _b),
                  phase_env=(_SAVE_ENV, ()),
                  expect_restore=((_XR_FIELD[_a], M), (_XR_MCARD[_a], M),
                                  (_XR_MCARD[_b], M), (_XR_FIELD[_b], M)),
                  expect_log=(_XR_WRITE,
                              (1, r"cross-import slot=0 from=%s to=%s converter=1 usable=1 converted=1"
                                  % (_a, _b)),
                              (1, r"STGMCARD_loadSave ok port=0 slot=0 money=\d+ progress=6"
                                  r"(?s:.*)FIELDSTG_pickStage mode=0x233")),
                  forbid_log=_XR_BAD,
                  note=f"P6.25: saved on the {_a.upper()} disc, loaded on the {_b.upper()} disc "
                       f"(converted copy), field reached with the saved state"))
# Two refusals, made from the game's own save by a run-time edit (phase_mutate):
# an EU save past the base game (progress 0x2D, the extra chapter) has no stage
# table in the US game, so nothing is imported and the US card stays empty; and
# a save whose slot checksum is wrong is not imported either. Both: the Load
# Game screen shows an empty card, no crash, nothing written, no import marker.
for _name, _a, _b, _mut, _why in (
        ("xr-eu-extra-us", "eu", "us", "progress:0x2D", "an EU extra-chapter save"),
        ("xr-us-badsum-eu", "us", "eu", "badsum", "a save with a wrong slot checksum")):
    _add(Scenario(_name, _a, "full", timeout=600, log_only=True,
                  phases=(f"{_a}-xr-save", f"{_b}-xr-load"), phase_versions=(_a, _b),
                  phase_env=(_SAVE_ENV, ()), phase_mutate=(_mut, ""),
                  expect_restore=((_XR_MCARD[_a], M), (_XR_MCARD[_b], M)),
                  expect_log=(_XR_WRITE,
                              (1, r"cross-import slot=0 skipped: nothing usable")),
                  forbid_log=((1, r"cross-import slot=\d from="), (1, r"STGMCARD_loadSave ok"),
                              (1, r"overlay restore file=0x%x kind=mode" % _XR_FIELD[_b])),
                  note=f"P6.25: {_why} is not imported into the other card (no crash)"))


# P6.31 auto-save (docs/P6.31_AUTOSAVE.md, patches 0140-0142): the game itself
# saves in the Underground Path (the forced save screen, as xr-save), a second
# process Continues that save and leaves the field for another area
# (DW3_FORCE_EXIT, patch 0110: 0x20B -> 0x201), and a third Continues again.
#  autosave-area: the option is on (cfg autosave=1). The area change writes the
#    slot the player loaded (log), and the third process lands in the NEW area
#    (0x201), not the saved one (0x20B).
#  autosave-off: the same without the option. Nothing is logged or written, and
#    the third process lands in the player's own save (0x20B): option off gives
#    the game as before, and a manual save is untouched.
#  autosave-walk: the option is on, the game is loaded in Bulk Swamp and walks
#    into a random battle: no area change, nothing is written, in or after the
#    battle.
# Log verdicts only (no checkpoints, no references). PC only (env).
_AS_SAVE_ENV = (("DW3_START_MODE", "0x20B"), ("DW3_PROGRESS", "6"),
                ("DW3_FORCE_MENU", "0xC00:0x80000000"))
_AS_EXIT_ENV = (("DW3_FORCE_EXIT", "0x201:240:264:7:100"),)
_AS_BAD = ((1, r"STGMCARD_loadSave (?:checksum|version|read) fail"),
           (2, r"STGMCARD_loadSave (?:checksum|version|read) fail"),
           (1, r"dw3-autosave: .*(?:FAILED|not saved: )"))
for _v, _ids in (("us", (0x158, 0x1E3)), ("eu", (0x166, 0x1EE))):
    _field, _mcard = _ids
    _add(Scenario(f"{_v}-autosave-area", _v, "full", timeout=600, log_only=True,
                  phases=(f"{_v}-xr-save", f"{_v}-as-load", f"{_v}-xr-load"),
                  phase_env=(_AS_SAVE_ENV, _AS_EXIT_ENV, ()),
                  cfg=(("autosave", "1"),),
                  expect_restore=((_field, M), (_mcard, M)),
                  expect_log=(_XR_WRITE,
                              (1, r"dw3-options: autosave=1"),
                              (1, r"STGMCARD_loadSave ok port=0 slot=0"),
                              (1, r"dw3-autosave: mode 0x20b: not saved \(first area \(baseline\)\)"),
                              (1, r"DW3_FORCE_EXIT leaveField mode=0x201"),
                              (1, r"dw3-autosave: area change to mode 0x201: saving to autosave\.bin"),
                              (1, r"dw3-autosave: wrote autosave\.bin \(area mode 0x201, #1\)"),
                              (2, r"dw3-autosave: autosave\.bin found: Continue loads it"
                                  r"(?s:.*)dw3-autosave: Continue loads autosave\.bin"
                                  r"(?s:.*)FIELDSTG_pickStage mode=0x201")),
                  forbid_log=_AS_BAD + ((2, r"FIELDSTG_pickStage mode=0x20b"),
                                        (2, r"STGMCARD_loadSave ok"),
                                        (1, r"save0\.bin\.pre-auto")),
                  note="P6.31: an area change writes autosave.bin (not the slot); Continue on the title loads it and lands in the new area"))
    # The title has both entries: Continue (the auto-save) and Load (the slot
    # list, which still holds the player's own save at the old area).
    _add(Scenario(f"{_v}-autosave-load", _v, "full", timeout=900, log_only=True,
                  phases=(f"{_v}-xr-save", f"{_v}-as-load", f"{_v}-as-continue", f"{_v}-as-loadrow"),
                  phase_env=(_AS_SAVE_ENV, _AS_EXIT_ENV, (), ()),
                  cfg=(("autosave", "1"),),
                  expect_restore=((_field, M), (_mcard, M)),
                  expect_log=(_XR_WRITE,
                              (1, r"dw3-autosave: wrote autosave\.bin \(area mode 0x201, #1\)"),
                              (2, r"dw3-autosave: Continue loads autosave\.bin"
                                  r"(?s:.*)FIELDSTG_pickStage mode=0x201"),
                              (3, r"STGMCARD_loadSave ok port=0 slot=0 money=\d+ progress=6"
                                  r"(?s:.*)FIELDSTG_pickStage mode=0x20b")),
                  forbid_log=_AS_BAD + ((2, r"STGMCARD_loadSave ok"),
                                        (2, r"FIELDSTG_pickStage mode=0x20b"),
                                        (3, r"dw3-autosave: Continue loads"),
                                        (3, r"FIELDSTG_pickStage mode=0x201")),
                  note="P6.31: title Continue loads the auto-save (new area), Load opens the slot list (the player's save, old area)"))
    # Pictures of the five-plaque title (an auto-save exists).
    _add(Scenario(f"{_v}-title-continue-load", _v, "full", timeout=900,
                  phases=(f"{_v}-xr-save", f"{_v}-as-load", f"{_v}-as-title"),
                  phase_env=(_AS_SAVE_ENV, _AS_EXIT_ENV, ()),
                  cfg=(("autosave", "1"),),
                  expect_restore=((_field, M), (_mcard, M)),
                  expect_log=((2, r"dw3-title-menu: plaques New Game, Options, Continue \(autosave\.bin\), "
                                  r"Load Game \(slot list\), Quit game"),),
                  note="P6.31: the title menu with Continue and Load Game rows (an auto-save exists), focus on each row"))
    _add(Scenario(f"{_v}-autosave-off", _v, "full", timeout=600, log_only=True,
                  phases=(f"{_v}-xr-save", f"{_v}-as-load", f"{_v}-xr-load"),
                  phase_env=(_AS_SAVE_ENV, _AS_EXIT_ENV, ()),
                  expect_restore=((_field, M), (_mcard, M)),
                  expect_log=(_XR_WRITE,
                              (1, r"DW3_FORCE_EXIT leaveField mode=0x201"),
                              (1, r"FIELDSTG_pickStage mode=0x201"),
                              (2, r"STGMCARD_loadSave ok port=0 slot=0 money=\d+ progress=6"
                                  r"(?s:.*)FIELDSTG_pickStage mode=0x20b")),
                  forbid_log=((0, r"dw3-autosave"), (1, r"dw3-autosave"), (2, r"dw3-autosave"),
                              (1, r"dw3-options: autosave="), (2, r"FIELDSTG_pickStage mode=0x201")),
                  note="P6.31: option off (the default): no auto-save line, the player's own save is the one Continue loads"))
    # P6.20: an auto-save made after a cheat carries the cheat mark (the cheat
    # money row is applied at the first idle frame of the loaded field, the area
    # change then writes the slot and marks it; the next Continue keeps the game
    # marked). Not in the replay of a build without cheats.
    _add(Scenario(f"{_v}-autosave-cheat", _v, "full", timeout=600, log_only=True,
                  phases=(f"{_v}-xr-save", f"{_v}-as-load", f"{_v}-xr-load"),
                  phase_env=(_AS_SAVE_ENV, _AS_EXIT_ENV + (("DW3_CHEATS_ARM", "money_10k"),), ()),
                  cfg=(("autosave", "1"), ("cheat_enabled", "1")),
                  expect_restore=((_field, M), (_mcard, M)),
                  expect_log=(_XR_WRITE,
                              (1, r"dw3-cheats: money \+10000"),
                              (1, r"dw3-autosave: wrote autosave\.bin \(area mode 0x201, #1\)"),
                              (1, r"dw3-cheats: autosave\.bin slot 0 marked: written with cheats"),
                              (2, r"dw3-cheats: loaded autosave\.bin slot 0, which was written with cheats")),
                  forbid_log=_AS_BAD,
                  note="P6.31 + P6.20: an auto-save after a cheat carries the cheat mark"))
    _add(Scenario(f"{_v}-autosave-walk", _v, "full", timeout=900, log_only=True,
                  phases=(f"{_v}-xr-save", f"{_v}-as-walk"),
                  phase_env=(_SAVE_ENV, ()),
                  cfg=(("autosave", "1"),),
                  expect_restore=((_field, M), (_mcard, M)),
                  expect_log=(_XR_WRITE,
                              (1, r"STGMCARD_loadSave ok port=0 slot=0"),
                              (1, r"dw3-autosave: mode 0x233: not saved \(first area \(baseline\)\)"),
                              (1, r"FIGHTSTG_start")),
                  forbid_log=((1, r"dw3-autosave: (?:area change|wrote)"),
                              (1, r"dw3-autosave: .*FAILED")),
                  note="P6.31: loaded, walked into a random battle: nothing is auto-saved (no area change, never in a battle)"))


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
    ("item-shop", "0xF00", (("DW3_CHEATS_ARM", "money_10k"),), 0x1F7, 0x205,
     "the item shop: panels, Buy list and an item's details reach the 16:9 edges (patch 0179)"),
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
                      cfg=(("cheat_enabled", "1"),) if _name == "item-shop" else (),
                      expect_log=((0, r"dw3-widescreen: screen STITSHOP \(arg 0\) opened: gutter 53 px, 16:9 layout, "
                                      r"mode file 0x%x \(STITSHOP\)" % _ovl),) if _name == "item-shop" else (),
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
    _add(Scenario(f"{_v}-cheats-screen", _v, "full", timeout=300,
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
    _add(Scenario(f"{_v}-cheats-screen2", _v, "full", timeout=300,
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
# encounter-walk; since patch 0175 the swirl spans 16:9 (PSY-Z copies the
# gutters into the transition image, gutter-width tiles hang off the first and
# last tile of each row). The 1x variant is the same recording without turbo:
# the frames must be the same.
def _SPIRAL_REFS(v):
    return tuple((f"spiral-{f}", f"{v}-ws-battle-spiral", f"spiral-{f}")
                 for f in (1104, 1110, 1116, 1122, 1128, 1140, 1152, 1164, 1188, 1206))


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
                  note="encounter spiral in 16:9: the swirl spans the gutters (patch 0175)"),
         Scenario(f"{_v}-ws-battle-spiral-1x", _v, "full", timeout=900,
                  phases=(f"{_v}-ws-battle-spiral",),
                  cfg=(("widescreen", "1"),),
                  env=(("DW3_START_MODE", "0x233"), ("DW3_PROGRESS", "6")),
                  ref_map=_SPIRAL_REFS(_v),
                  expect_log=((0, r"FIGHTSTG_start"),),
                  note="the same spiral at turbo off: the pictures of the 4x run (patch 0175)"))
    # Turbo toggled on in game (R3, Fast-forward level from the option file): the
    # speeds the Switch runs the swirl at; the pictures are the 4x run's.
    for _t in (2, 3, 4):
        _add(Scenario(f"{_v}-ws-battle-spiral-t{_t}", _v, "full", timeout=900,
                      phases=(f"{_v}-ws-battle-spiral-turbo",),
                      cfg=(("widescreen", "1"), ("fast_forward", str(_t))),
                      env=(("DW3_START_MODE", "0x233"), ("DW3_PROGRESS", "6")),
                      ref_map=_SPIRAL_REFS(_v),
                      expect_log=((0, r"FIGHTSTG_start(?s:.*)"),
                                  (0, rf"dw3-turbo: speed {_t}x")),
                      note=f"the same spiral with turbo {_t}x toggled on by R3: the pictures of the 4x run (patch 0175)"))

# The spiral on the Switch build in Ryubing (tier "nx", log only: the replay NRO
# is built with DW3_NX_TEST_ENV="DW3_START_MODE=0x233,DW3_PROGRESS=6"). The
# turbo runs set the Fast-forward level through the option file and toggle it
# with R3; with turbo on the CPU rasterizer defers drawing in the field, which
# the swirl's gutter copy has to draw first (patch 0175). The log proves the
# copy happened (53 px) and the battle was reached; the informational
# screenshots are for the owner's eyes.
for _v, _ids in (("us", (0x158, 0x159)), ("eu", (0x166, 0x167))):
    _field, _fight = _ids
    _add(Scenario(f"{_v}-ws-battle-spiral-nx", _v, "nx", timeout=900,
                  phases=(f"{_v}-ws-battle-spiral",),
                  cfg=(("widescreen", "1"),),
                  expect_restore=((_field, M), (_fight, M)),
                  expect_log=((0, r"dw3-widescreen: gutters copied beside the image at x 640: 53 px"),),
                  note="Switch build: the encounter spiral in 16:9 at turbo off (patch 0175)"))
    for _t in (2, 3, 4):
        _add(Scenario(f"{_v}-ws-battle-spiral-nx-t{_t}", _v, "nx", timeout=900,
                      phases=(f"{_v}-ws-battle-spiral-turbo",),
                      cfg=(("widescreen", "1"), ("fast_forward", str(_t))),
                      expect_restore=((_field, M), (_fight, M)),
                      expect_log=((0, rf"dw3-turbo: speed {_t}x(?s:.*)"
                                      r"dw3-widescreen: gutters copied beside the image at x 640: 53 px"),),
                      note=f"Switch build: the encounter spiral in 16:9 with turbo {_t}x (deferred drawing) (patch 0175)"))

# P6.9 restart validation (docs/P6.9_RESTART.md). The restart hotkey is turned
# on through the option file (restart_hotkey=1, so the P9.4 reader is part of
# the test) and the recording presses L1+R1+Start+Select (0x0C09). The game
# frame counter keeps running across the in-process restart, so the checkpoints
# after the chord are at the chord's frame plus the cold boot's. PC only (env).
# Patch 0124 makes DW3_START_MODE apply on the boot's newGame only, so a restart
# reaches the real boot screen (US title, EU language select). Patch 0127's
# DW3_START_MODE_EVERY brings back the re-entry of the forced mode (a second
# forced battle or field on a reset GAME) for the scenarios that need it.
_RST_CFG = (("restart_hotkey", "1"),)
_RST_LINE = r"dw3-port-restart: saves flushed; returning to the boot screen in-process"
_RST_EVERY = (("DW3_START_MODE_EVERY", "1"),)
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158)), ("eu", (0x167, 0x208, 0x1D1, 0x166))):
    _fight, _menu, _report, _field = _ids
    _battle = ((_fight, M), (_menu, S), (_report, M), (_field, M))
    # field, memory card screen, the boot screen the restart reaches (US title,
    # EU language select), the memory card screen of Continue, the field again
    _sfield, _smcard, _stitle = (0x158, 0x1E3, 0x1C1) if _v == "us" else (0x166, 0x1EE, 0x165)
    _add(Scenario(f"{_v}-restart-after-battle", _v, "full", turbo=4, timeout=400,
                  env=_P77_ENV_REPORT, cfg=_RST_CFG, expect_restore=_battle,
                  expect_log=((0, r"WFIGHTMN_endBattle result=1(?s:.*)" + _RST_LINE),),
                  note="restart in the field after a battle: the boot screen, title menu (P6.9)"),
         Scenario(f"{_v}-restart-battle-again", _v, "full", turbo=4, timeout=600,
                  env=_P77_ENV_REPORT + _RST_EVERY, cfg=_RST_CFG, expect_restore=_battle * 2,
                  expect_log=((0, r"WFIGHTMN_endBattle result=1(?s:.*)" + _RST_LINE
                                  + r"(?s:.*)WFIGHTMN_endBattle result=1"),),
                  note="restart after a battle re-enters the forced battle: the same battle again (P6.9)"),
         Scenario(f"{_v}-restart-in-battle", _v, "full", turbo=4, timeout=400,
                  env=_P77_ENV_REPORT, cfg=_RST_CFG,
                  expect_log=((0, r"FIGHTSTG_start(?s:.*)" + _RST_LINE),),
                  forbid_log=((0, r"WFIGHTMN_endBattle"),),
                  note="the chord inside a battle restarts from any mode (P6.9)"),
         Scenario(f"{_v}-restart-before-battle", _v, "full", turbo=4, timeout=400, log_only=True,
                  phases=(f"{_v}-restart-before-battle",),
                  env=_ENC_ENV + _RST_EVERY + (("DW3_CHEATS_ARM", "battle_now"),),
                  cfg=_RST_CFG + _CHEAT_ON + (("encounter_rate", "2"),),
                  expect_log=((0, _RST_LINE + r"(?s:.*)dw3-cheats: battle now: a random battle starts"
                                  r"(?s:.*)FIGHTSTG_start"),),
                  note="restart in a field, then a random battle (Battle now) starts normally (P6.9)"),
         Scenario(f"{_v}-restart-save-continue", _v, "full", timeout=900,
                  env=_SAVE_ENV, cfg=_RST_CFG,
                  expect_restore=((_sfield, M), (_smcard, M), (_stitle, M), (_smcard, M), (_sfield, M)),
                  expect_log=((0, r"STGMCARD_writeSave port=0 slot=0 status=1(?s:.*)" + _RST_LINE
                                  + r"(?s:.*)STGMCARD_loadSave ok port=0 slot=0 money=0 progress=6"),),
                  note="field save, restart, Load Game / Continue reads the save back (P6.9)"))

    _add(Scenario(f"{_v}-restart-in-battle-redo", _v, "full", turbo=4, timeout=600,
                  env=_P77_ENV_REPORT + _RST_EVERY, cfg=_RST_CFG, expect_restore=((_fight, M), (_menu, S)) + _battle,
                  expect_log=((0, r"FIGHTSTG_start(?s:.*)" + _RST_LINE
                                  + r"(?s:.*)FIGHTSTG_start(?s:.*)WFIGHTMN_endBattle result=1"),),
                  note="the chord inside a battle with DW3_START_MODE_EVERY: the battle starts again and ends (P6.9)"))
    # No env: the Ryubing runner runs it too (the option file turns the hotkey on).
    _add(Scenario(f"{_v}-restart-newgame", _v, "full", timeout=600, cfg=_RST_CFG,
                  note="restart during the New Game opening, then New Game and the opening again (P6.9)"))


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
                                  r"dw3-port-restart: fading to black before the reset \(field menu, 20 frames\)(?s:.*)"
                                  r"dw3-port-restart: screen is black after 20 frames; resetting now(?s:.*)"
                                  r"dw3-port-restart: saves flushed; returning to the boot screen(?s:.*)"
                                  # EU: the language select has the gutter too (patch 0177), so
                                  # the picture stays 16:9 through the reset: no switch at all
                                  + (r"dw3-widescreen: gutter 0 px per side \(was 53; 4:3 picture\)(?s:.*)"
                                     if _v == "us" else "") +
                                  r"dw3-port-restart: new screen ready after \d+ black frames; fading in(?s:.*)"
                                  r"dw3-port-restart: fade finished"),),
                  forbid_log=((0, r"save flush failed"),),
                  note="field menu Return to title: No keeps the menu, Yes fades to black, runs the P6.9 reset on black and fades the title in (P6.27); the 4:3 gutter switch falls inside the black (US; EU stays 16:9 since patch 0177)"),
         Scenario(f"{_v}-return-title-rows7", _v, "full", turbo=4, timeout=600,
                  env=_ITEMS_ENV, cfg=_RT_CFG,
                  expect_log=((0, r"dw3-port-confirm: open return to title \(cursor on No\)(?s:.*)"
                                  r"dw3-port-confirm: cancelled \(return to title\); nothing changed"),),
                  forbid_log=((0, r"dw3-port-restart"),),
                  note="the field menu with the card folders row and Return to title: seven rows, cancelled (P6.27)"))

# P9.7 slice 1: the port Options screen from the field menu's Options row
# (patch 0160, port/options/field_options.c). Phase 1: Widescreen refuses a
# change (locked in game), Music volume 10 -> 7 and Encounter rate 1x -> OFF
# apply at once and are saved, the screen closes and the menu and the field go
# on; phase 2 (a new process on the same config dir) shows the values read back.
_FO_P1 = (r"dw3-port-options: open in game(?s:.*)"
          r"dw3-port-options: group open display \(1 row\(s\), in game\)(?s:.*)"
          r"dw3-port-options: Widescreen is locked in game \(change it at the title\)(?s:.*)"
          r"dw3-port-options: group open sound \(2 row\(s\), in game\)(?s:.*)"
          r"dw3-port-options: music volume 7(?s:.*)"
          r"dw3-port-options: group open gameplay \(5 row\(s\), in game\)(?s:.*)"
          r"dw3-port-options: encounter rate off(?s:.*)"
          r"dw3-port-options: close in game(?s:.*)"
          r"dw3: field menu Triangle cancel \(stay field\)",
          r"dw3-options: saved \S*port_options\.cfg \(widescreen=1 fast_forward=2 restart_hotkey=0 "
          r"music_volume=7 sfx_volume=10 encounter_rate=2 text_speed=0 battle_menu_memory=0\)")
_FO_P2 = (r"dw3-options: \S*port_options\.cfg \(widescreen=1 fast_forward=2 restart_hotkey=0 "
          r"music_volume=7 sfx_volume=10 encounter_rate=2 text_speed=0 battle_menu_memory=0\)",
          r"dw3-port-options: open in game")
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-field-options", _v, "full", turbo=4, timeout=600,
                  phases=(f"{_v}-field-options-p1", f"{_v}-field-options-p2"),
                  env=_RT_ENV,
                  expect_log=tuple((0, r) for r in _FO_P1) + tuple((1, r) for r in _FO_P2),
                  forbid_log=((0, r"widescreen (enabled|disabled)"), (0, r"save flush failed"),
                              (0, r"dw3-port-restart"), (0, r"sound \w+ not played")),
                  note="field menu Options: Widescreen locked, Music volume and Encounter rate apply and persist, the menu and the field continue (P9.7)"),
         Scenario(f"{_v}-field-options-cheats", _v, "full", turbo=4, timeout=600,
                  env=_RT_ENV, cfg=(("cheat_enabled", "1"),),
                  expect_log=((0, r"dw3-port-options: open in game(?s:.*)"
                                  r"dw3-port-options: cheats are locked in game \(change them at the title\)(?s:.*)"
                                  r"dw3-port-options: cheats are locked in game \(change them at the title\)(?s:.*)"
                                  r"dw3-port-options: close in game"),),
                  forbid_log=((0, r"dw3-cheats: open"),),
                  note="the Cheats entry and the Cheats group are greyed in game: Square (Y) and Cross do nothing (P9.7, P9.9)"))
# P6.4 turbo menus (patch 0176, port/options/turbo.c). With turbo on, menus
# opened inside the field run at 1.5x: the field menu list, the Options screen
# over it and the Return to title Yes / No window. The log has `speed 4x` (the
# field's factor) before and after, `menu 1.5x (field 4x)` when the menu opens
# and `menu off (frames=N)` when it is gone, and no `speed` line in between (the
# field factor does not change, only the menu's rate does). The menu is up for
# about 700 game frames in this recording.
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-turbo-menu", _v, "full", turbo=4, timeout=600, log_only=True,
                  env=_RT_ENV,
                  expect_log=((0, r"dw3-turbo: speed 4x(?s:.*)"
                                  r"dw3-turbo: menu 1\.5x \(speed 4x\)(?s:.*)"
                                  r"dw3: field menu Triangle cancel \(stay field\)(?s:.*)"
                                  r"dw3-turbo: menu off \(frames=(?:[3-9]\d\d|\d{4,})\)"),),
                  forbid_log=((0, r"dw3-turbo: menu 1\.5x(?:(?!dw3-turbo: menu off)(?s:.))*?dw3-turbo: speed"),
                              (0, r"dw3-turbo: menu off(?s:.*)dw3-turbo: menu 1\.5x")),
                  note="turbo on: the field menu, Options and the Return to title window run at 1.5x, the field factor stays 4x (P6.4)"))
# P6.4: the battle's command menu and technique list (WFIGHTMN is a sub overlay
# of FIGHTSTG, so the mode file does not change there: they ran at the full
# turbo speed before patch 0176). The forced battle of `p77` with Tech once: the
# menu rate shows between the battle's start and the report, and goes off again.
_TB_ENV = (("DW3_START_MODE", "0x600"), ("DW3_FORCE_ENCOUNTER", "34"),
           ("DW3_FORCE_PARTY", "0"))
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-turbo-battle-menu", _v, "full", turbo=4, timeout=300, log_only=True,
                  phases=(f"{_v}-p77-battle",), env=_TB_ENV,
                  expect_log=((0, r"FIGHTSTG_start(?s:.*)"
                                  r"dw3-turbo: menu 1\.5x \(speed 4x\)(?s:.*)"
                                  r"dw3-turbo: menu off \(frames=\d+\)(?s:.*)"
                                  r"STFGTREP_start"),),
                  forbid_log=((0, r"dw3-turbo: menu 1\.5x(?:(?!dw3-turbo: menu off)(?s:.))*?dw3-turbo: speed"),),
                  note="turbo on: the battle command menu and technique list run at 1.5x, the battle itself at 4x (P6.4)"))
# The same on the Switch build in Ryubing (tier "nx": only with --only; the NRO
# has no environment, so the replay NRO is built with DW3_NX_TEST_ENV holding
# _RT_ENV, see docs/P6.15_REPLAY_TESTS.md). P9.7 after #268's release of the
# title overlay textures: the first Options open in the field builds them, a
# reopen after 200 idle frames rebuilds them, and the Yes / No window of Return
# to title (its own texture) is rebuilt too. The PC runner skips tier nx.
_NX_FO_FORBID = ((0, r"widescreen (enabled|disabled)"), (0, r"save flush failed"),
                 (0, r"dw3-port-restart"))
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-field-options-nx", _v, "nx", timeout=600,
                  phases=(f"{_v}-field-options-p1", f"{_v}-field-options-p2"),
                  expect_log=tuple((0, r) for r in _FO_P1) + tuple((1, r) for r in _FO_P2),
                  forbid_log=_NX_FO_FORBID,
                  note="Switch build: field menu Options (env baked into the NRO) (P9.7)"),
         Scenario(f"{_v}-field-options-release-nx", _v, "nx", timeout=600,
                  phases=(f"{_v}-field-options-release",),
                  expect_log=((0, r"dw3-port-options: open in game(?s:.*)"
                                  r"dw3-port-options: close in game(?s:.*)"
                                  r"dw3-title-overlay: title textures released(?s:.*)"
                                  r"dw3-port-options: open in game(?s:.*)"
                                  r"dw3-port-options: close in game(?s:.*)"
                                  r"dw3-title-overlay: title textures released(?s:.*)"
                                  r"dw3-port-confirm: open return to title \(cursor on No\)(?s:.*)"
                                  r"dw3-port-confirm: cancelled \(return to title\); nothing changed"),),
                  forbid_log=_NX_FO_FORBID,
                  note="Switch build: Options and the Yes / No window drawn after the title textures were released (P9.7, #268)"))

# P9.7 slice 2: the port Options screen in battle (patch 0165, port/options/
# battle_options.c). The forced battle of p77 (env of _P77_ENV_REPORT: Kotemon
# vs Kunemon, Tech once, Cross to the report and the field). The control run
# has no Options; the excursion run presses Select at the command menu, changes
# Music volume, waits 300 frames and closes the screen. The battle is frozen for
# those 150 frames: the log's two state lines (the queued events, their times,
# the HP, the GFX frame and clock) are identical, and the plain run's pictures,
# taken 300 frames later, are reproduced pixel for pixel. PC only (env).
_BO_STATE = (r"gfx_frame=(\d+) time_counter=(\d+) events=([1-9]\d*) event_time=(-?\d+) hp=(\d+)\n")
_BO_STATE_BACK = r"gfx_frame=\1 time_counter=\2 events=\3 event_time=\4 hp=\5\n"
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158)), ("eu", (0x167, 0x208, 0x1D1, 0x166))):
    _fight, _menu, _report, _field = _ids
    _battle = ((_fight, M), (_menu, S), (_report, M), (_field, M))
    _shots = ("battle-before", "battle-after", "battle-tech", "battle-turn", "battle-mid")
    _add(Scenario(f"{_v}-battle-options-ref", _v, "full", turbo=4, timeout=300,
                  env=_P77_ENV_REPORT, expect_restore=_battle,
                  expect_log=((0, r"WFIGHTMN_endBattle result=1"),),
                  forbid_log=((0, r"dw3-battle-options"), (0, r"dw3-port-options: open")),
                  note="control run of the battle Options excursion: the plain forced battle (P9.7)"),
         Scenario(f"{_v}-battle-options", _v, "full", turbo=4, timeout=300,
                  env=_P77_ENV_REPORT, expect_restore=_battle,
                  ref_map=tuple((s, f"{_v}-battle-options-ref", s) for s in _shots),
                  expect_log=((0, r"dw3-battle-options: open at the command menu, battle frozen: (?s:.*)"
                                  r"dw3-port-options: open in game \(battle command menu; (?s:.*)"
                                  r"dw3-battle-options: held, battle state: " + _BO_STATE +
                                  r"(?s:.*)dw3-port-options: Widescreen is locked in game \(change it at the title\)(?s:.*)"
                                  r"dw3-port-options: group open sound \(2 row\(s\), in game\)(?s:.*)"
                                  r"dw3-port-options: music volume 7(?s:.*)"
                                  r"dw3-port-options: close in game \(back to the battle\)(?s:.*)"
                                  r"dw3-battle-options: resume after 300 held frame\(s\); the vsync clock ran on [1-9]\d* during the hold and is put back\n"
                                  r"dw3-battle-options: resume, battle state: " + _BO_STATE_BACK +
                                  r"(?s:.*)WFIGHTMN_endBattle result=1"),),
                  forbid_log=((0, r"widescreen (enabled|disabled)"), (0, r"save flush failed"),
                              (0, r"dw3-port-restart"), (0, r"reset while held"),
                              (0, r"sound \w+ not played")),
                  note="Select at the battle command menu: the battle is frozen under the Options screen, "
                       "resumes on the same frame (pictures identical to the control run) and ends normally (P9.7)"))

# The same excursion on the Switch build in Ryubing (tier "nx", log only): the
# replay NRO is built with DW3_NX_TEST_ENV="DW3_START_MODE=0x600,
# DW3_FORCE_ENCOUNTER=34,DW3_FORCE_PARTY=0" (the env of _P77_ENV_REPORT), so
# there is no `env` here. The held frame's picture (the mesh layer's) is for the
# owner's eyes and the informational screenshots; the log proves the freeze.
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158)), ("eu", (0x167, 0x208, 0x1D1, 0x166))):
    _fight, _menu, _report, _field = _ids
    _add(Scenario(f"{_v}-battle-options-nx", _v, "nx", timeout=900,
                  phases=(f"{_v}-battle-options",),
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M)),
                  expect_log=((0, r"dw3-battle-options: open at the command menu, battle frozen: (?s:.*)"
                                  r"dw3-port-options: open in game \(battle command menu; (?s:.*)"
                                  r"dw3-battle-options: held, battle state: " + _BO_STATE +
                                  r"(?s:.*)dw3-port-options: Widescreen is locked in game \(change it at the title\)(?s:.*)"
                                  r"dw3-port-options: group open sound \(2 row\(s\), in game\)(?s:.*)"
                                  r"dw3-port-options: music volume 7(?s:.*)"
                                  r"dw3-port-options: close in game \(back to the battle\)(?s:.*)"
                                  r"dw3-battle-options: resume after 300 held frame\(s\); the vsync clock ran on [1-9]\d* during the hold and is put back(?s:.*)"
                                  r"dw3-battle-options: resume, battle state: " + _BO_STATE_BACK +
                                  r"(?s:.*)WFIGHTMN_endBattle result=1"),),
                  forbid_log=((0, r"widescreen (enabled|disabled)"), (0, r"save flush failed"),
                              (0, r"dw3-port-restart"), (0, r"reset while held")),
                  note="Switch build: Select at the battle command menu, battle held under the Options screen, resumes and ends (P9.7)"))

# The Options row text and the screen's lock note in the other four EU languages.
for _id, _lang in ((3, "fr"), (5, "de"), (4, "it"), (6, "es")):
    _add(Scenario(f"eu-field-options-{_lang}", "eu", "full", turbo=4, timeout=600,
                  env=_RT_ENV + (("DW3_LANGUAGE", str(_id)),),
                  expect_log=((0, rf"LANGUAGE={_id} \(DW3_LANGUAGE\)(?s:.*)dw3-port-options: open in game"
                                  r"(?s:.*)dw3-port-options: close in game"),),
                  note=f"field menu Options row and the screen's lock note in {_lang} (P9.7)"))

# P6.30 technical help (port/help/, docs/P6.30_HELP.md): the Help plaque of the
# Options group list, the page list, Controls scrolled by a line and by
# screenfuls to its end, L1 / R1 between pages, Back to the list and out. The
# proof is the `dw3-help:` / `dw3-pages:` lines (AGENTS.md rule 12) and the
# pictures. The same from the title, the field menu's OPTIONS row and the
# battle's Options, on both discs.
_HELP_READ = (r"dw3-help: panel canvas allocated, 2764800 bytes \(960x720 RGBA\)(?s:.*)"
              r"dw3-help: first frame composed [\d.]+ ms after open(?s:.*)"
              r"dw3-help: page controls \(from list, \d+ lines, panel 13 lines, unverified blocks 0\)(?s:.*)"
              r"dw3-help: page controls end reached \(line \d+ of \d+\)(?s:.*)"
              r"dw3-help: page controls closed after \d+ ms \(read to line \d+ of \d+\)(?s:.*)"
              r"dw3-help: page hotkeys \(from next, \d+ lines, panel 13 lines, unverified blocks 0\)(?s:.*)"
              r"dw3-help: page turbo \(from next,(?s:.*)"
              r"dw3-help: page hotkeys \(from prev,(?s:.*)"
              r"dw3-help: page controls \(from prev,(?s:.*)"
              r"dw3-help: page controls closed after \d+ ms(?s:.*)"
              r"dw3-help: closed after \d+ ms \(pages opened 5\)(?s:.*)"
              r"dw3-help: panel canvas released(?s:.*)"
              r"dw3-port-options: group close help \(back to the group list\)")
_HELP_FORBID = ((0, r"unresolved"), (0, r"dw3-pages: text fallback"), (0, r"cannot be (opened|allocated|laid out)"),
                (0, r"save flush failed"), (0, r"sound \w+ not played"))
for _v, _reg in (("us", "US"), ("eu", "EU")):
    _add(Scenario(f"{_v}-help-title", _v, "full", timeout=300,
                  expect_log=((0, r"dw3-help: open \(from title, region " + _reg +
                                  r", platform pc, language en, 10 pages\)(?s:.*)"
                                  r"dw3-port-options: group open help \(title\)(?s:.*)" + _HELP_READ),
                              (0, r"dw3-pages: text help en loaded \(\d+ keys, \d+ qualified, \d+ filtered "
                                  r"out for " + _reg + r"/pc, \d+ bytes\)"),
                              (0, r"dw3-pages: token \{btn:cross\} -> \"\[X\]\""),
                              (0, r"dw3-port-options: close \(resume runOptions\)")),
                  forbid_log=_HELP_FORBID,
                  note="technical help from the title's Options: page list, Controls scrolled to its end, "
                       "L1 / R1 between pages, Back out (P6.30)"),
         Scenario(f"{_v}-help-field", _v, "full", turbo=4, timeout=600, env=_RT_ENV,
                  expect_log=((0, r"dw3-port-options: open in game(?s:.*)"
                                  r"dw3-help: open \(from field, region " + _reg +
                                  r", platform pc, language en, 10 pages\)(?s:.*)"
                                  r"dw3-port-options: group open help \(in game\)(?s:.*)" + _HELP_READ +
                                  r"(?s:.*)dw3-port-options: close in game \(back to the field menu\)"),),
                  forbid_log=_HELP_FORBID + ((0, r"widescreen (enabled|disabled)"),),
                  note="technical help from the field menu's OPTIONS row: the field waits behind it (P6.30)"))
    _fight, _menu, _report, _field = ((0x159, 0x1FA, 0x1C2, 0x158) if _v == "us"
                                      else (0x167, 0x208, 0x1D1, 0x166))
    _add(Scenario(f"{_v}-help-battle", _v, "full", turbo=4, timeout=300,
                  env=_P77_ENV_REPORT,
                  expect_restore=((_fight, M), (_menu, S)),
                  expect_log=((0, r"dw3-battle-options: open at the command menu, battle frozen: (?s:.*)"
                                  r"dw3-help: open \(from battle, region " + _reg +
                                  r", platform pc, language en, 10 pages\)(?s:.*)"
                                  r"dw3-help: page controls \(from list,(?s:.*)"
                                  r"dw3-help: page controls end reached(?s:.*)"
                                  r"dw3-help: closed after \d+ ms \(pages opened 1\)(?s:.*)"
                                  r"dw3-port-options: close in game \(back to the battle\)(?s:.*)"
                                  r"dw3-battle-options: resume after"),),
                  forbid_log=_HELP_FORBID,
                  note="technical help from the battle's Options (Select at the command menu): "
                       "the battle stays frozen and resumes after it (P6.30)"))
# The base game's other languages: the files are stubs until the translations
# land, so the pages are English and the log says so (one line).
_add(Scenario("eu-help-fr", "eu", "full", timeout=300,
              expect_log=((0, r"dw3-help: open \(from title, region EU, platform pc, language fr, 10 pages\)"),
                          (0, r"dw3-pages: text fallback help \* \(fr\): the language file has no keys yet "
                              r"\(stub\), English shown"),
                          (0, r"dw3-help: page controls \(from list,")),
              forbid_log=((0, r"unresolved"), (0, r"cannot be (opened|allocated|laid out)")),
              note="technical help with the EU disc set to French: the stub falls back to English "
                   "with a log line, the screen's own lettering is French (P6.30, Q3)"))

# The Quit game plaque and the window in the other four EU languages (the
# lettering is authored, docs/PORT_OPTIONS.md "Quit and return to title").
for _lang in ("fr", "de", "it", "es"):
    _add(Scenario(f"eu-title-quit-{_lang}", "eu", "full", timeout=300,
                  expect_log=((0, r"dw3-port-confirm: open quit \(cursor on No\)(?s:.*)"
                                  r"dw3-port-confirm: cursor on Yes(?s:.*)"
                                  r"dw3-port-confirm: cancelled \(quit\); nothing changed"),),
                  forbid_log=((0, r"dw3-port-quit"),),
                  note=f"Quit game plaque and window in {_lang}; Triangle cancels, nothing is quit (P6.28)"))


# P5.5 stage sweep (port/sweep, docs/P5.5_STAGE_SWEEP.md): a five-stage subset
# of the sweep (Asuka City, the Digimon Lab, the Arena, the Bulk Swamp, the Noise
# Desert) from the boot, on a fresh card folder; the recording has no input.
# The full set is `DW3_STAGE_SWEEP=1` without ONLY (about 3 min on the PC).
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-stage-sweep", _v, "full", timeout=300, quits=True, log_only=True,
                  env=(("DW3_STAGE_SWEEP", "1"), ("DW3_STAGE_SWEEP_EXIT", "1"),
                       ("DW3_STAGE_SWEEP_FRAMES", "60"),
                       ("DW3_STAGE_SWEEP_ONLY", "0x200,0x206,0x209,0x233,0x248")),
                  expect_log=((0, r"dw3-sweep: stage WSTAG200 ok in \d+ ms \(frames 60, mem free \d+, "
                                  r"load \d+ ms\)(?s:.*)dw3-sweep: done 5/5 ok, failed: none"),),
                  forbid_log=((0, r"dw3-sweep: stage WSTAG\d+ FAILED"), (0, r"dw3-autosave: wrote"),
                              (0, r"null-guard hit")),
                  note="P5.5: five stages one after the other and back to the stage select, no save written"))


def by_name() -> dict[str, Scenario]:
    return {s.name: s for s in SCENARIOS}

# P6.24 slice 1: the same title recordings, started through the region
# launcher with only US, only EU, and both trees present (the region forced by
# DW3_REGION, or chosen in the picker by scripted keys). The captures must
# match the references of the plain per-region scenarios.
_SINGLE = (
    ("single-us-only", "us", "us", "us-title", (), "only the US data is present"),
    ("single-eu-only", "eu", "eu", "eu-lang-en", (), "only the EU data is present"),
    ("single-both-env-us", "us", "us+eu", "us-title", (("DW3_REGION", "us"),), "region forced"),
    ("single-both-env-eu", "eu", "us+eu", "eu-lang-en", (("DW3_REGION", "eu"),), "region forced"),
    ("single-both-pick-us", "us", "us+eu", "us-title", (("DW3_REGION_PICK_KEYS", "la"),),
     r"picker chose us"),
    ("single-both-pick-eu", "eu", "us+eu", "eu-lang-en", (("DW3_REGION_PICK_KEYS", "ra"),),
     r"picker chose eu"),
    ("single-both-countdown", "us", "us+eu", "us-title", (("DW3_REGION_PICK_KEYS", ""),),
     r"picker chose us \(countdown\)"),
)
for _n, _v, _l, _rec, _env, _why in _SINGLE:
    _add(Scenario(_n, _v, "single", phases=(_rec,), launch=_l, ref_from=_rec, env=_env,
                  timeout=120,
                  expect_log=((0, rf"dw3-region: starting {_v} "), (0, _why),
                              (0, rf"dw3-pc: cdfs root=\S*/{_v}\b")),
                  forbid_log=((0, rf"dw3-region: starting {'eu' if _v == 'us' else 'us'} "),),
                  note="P6.24: through the launcher; captures equal the plain run's"))


# --- P9.8 confirm / cancel layout (docs/CONTROLS_BUNDLE.md, ROADMAP P9.8) ------
# Replays hand the game the PS buttons it must see, so the layout map is
# bypassed and the plain scenarios prove the default is bit-identical. These
# scenarios turn on the dev variable DW3_REPLAY_SOURCE=1: the recording's masks
# are button *positions* (Switch A is the Circle position, B Cross, X Triangle,
# Y Square, as a real pad reports them) and go through the map, with
# button_layout=nintendo in the seeded port_options.cfg. The recordings are the
# plain ones converted by tools/replay/gen_scenarios.py, so the game sees exactly
# the original input and every capture is compared with the PLAIN scenario's
# reference (ref_from): a wrong map shows as a different picture or a different
# log, and no reference of its own is stored for them.
import dataclasses  # noqa: E402

_NIN_CFG = (("button_layout", "nintendo"),)
_NIN_ENV = (("DW3_REPLAY_SOURCE", "1"),)
_NIN_LOG = (r"dw3-controls: map loaded layout=nintendo source=cfg(?s:.*)"
            r"dw3-controls: map cross<-A circle<-X triangle<-B square<-Y(?s:.*)"
            r"dw3-controls: first press source=A -> logical cross",)


def _nintendo(base_name, name, phase, note):
    b = by_name()[base_name]
    logs = tuple(e for e in b.expect_log if e[0] == 0)
    return dataclasses.replace(
        b, name=name, phases=(phase,), cfg=b.cfg + _NIN_CFG, env=b.env + _NIN_ENV,
        expect_log=logs + tuple((0, r) for r in _NIN_LOG),
        forbid_log=tuple(e for e in b.forbid_log if e[0] == 0)
        + ((0, r"dw3-controls: map invalid"),),
        ref_from=base_name, note=note)


for _v in ("us", "eu"):
    _add(
        _nintendo(f"{_v}-field-options", f"{_v}-nintendo-menu", f"{_v}-nintendo-menu",
                  "Nintendo layout: A opens the field menu and the Options screen, B backs out "
                  "of the group, the screen and the menu (P9.8); same pictures as the plain run"),
        _nintendo(f"{_v}-map-view", f"{_v}-nintendo-map-view", f"{_v}-nintendo-map-view",
                  "Nintendo layout: the full-screen map opens with Square (Y) and closes with the "
                  "logical cancel bit, B (P9.8, P6.32)"),
        _nintendo(f"{_v}-title-quit", f"{_v}-nintendo-title-quit", f"{_v}-nintendo-title-quit",
                  "Nintendo layout: the title menu and the Quit Yes / No window confirm with A "
                  "and cancel with B (P9.8)"),
        Scenario(f"{_v}-controls-layout", _v, "full", timeout=300,
                 phases=(f"{_v}-controls-layout-p1", f"{_v}-controls-layout-p2"),
                 expect_log=((0, r"dw3-controls: map loaded layout=playstation source=default(?s:.*)"
                                 r"dw3-port-options: group open controls \(1 row\(s\), title\)(?s:.*)"
                                 r"dw3-controls: layout changed playstation -> nintendo \(screen\)(?s:.*)"
                                 r"dw3-controls: map cross<-A circle<-X triangle<-B square<-Y"),
                             (1, r"dw3-controls: map loaded layout=nintendo source=cfg"),
                             (1, r"dw3-port-options: group open controls \(1 row\(s\), title\)")),
                 forbid_log=((0, r"dw3-controls: map invalid"), (1, r"dw3-controls: map invalid")),
                 note="Controls group: the Button layout row switches to Nintendo and is read back "
                      "from port_options.cfg by the next process (P9.8)"),
    )
_add(_nintendo("eu-lang-cross-fr", "eu-nintendo-lang-cross-fr", "eu-nintendo-lang-cross-fr",
               "Nintendo layout: the EU language select confirms with A, the logical Cross (patch "
               "0187, P9.8)"))
