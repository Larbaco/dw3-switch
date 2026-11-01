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

import re
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
    # P6.51: expect_log used instead when the run is on the combined
    # (--bin-both) binary, for rows that differ there (the Region row of the
    # Gameplay group). Empty = expect_log.
    expect_log_both: tuple[tuple[int, str], ...] = ()
    # The same, but the regex must NOT match anywhere in that phase's log
    # (P6.18: "no FIGHTSTG_start while Encounter rate is Off").
    forbid_log: tuple[tuple[int, str], ...] = ()
    # port_options.cfg keys written before the first phase (P6.18 seeds the
    # pacing rows this way, so the option file reader is part of the test).
    cfg: tuple[tuple[str, str], ...] = ()
    # D2: the seeded file is written as a build before D2 wrote it, with no
    # `defaults_gen` marker (the runner adds `defaults_gen=2` to every other seeded
    # cfg), so the one-shot migration of the defaults runs on it.
    legacy_cfg: bool = False
    # D3 (docs/OPTIONS_REORG.md 6.2): the per-region files an older build wrote, seeded
    # as `<config dir>/us/port_options.cfg` and `.../eu/port_options.cfg` (no
    # `defaults_gen` unless the tuple holds it) next to the global file. `cfg` then is the
    # old top-level file (e.g. `last_region`), also seeded without a marker. The
    # `*-options-global-migrate*` scenarios use them; nothing else writes a per-region cfg.
    cfg_us: tuple[tuple[str, str], ...] = ()
    cfg_eu: tuple[tuple[str, str], ...] = ()
    # D3: block the global file's temp name with a directory, so the migration cannot
    # write the new file (a failed-write case): the old files must stay.
    block_global_write: bool = False
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
    # P6.25 slice 2: run with the shared store (`save_store=1` in the seeded
    # cfg, or per phase through phase_env DW3_SAVE_STORE) and DW3_SAVE_STORE_DIR
    # = a folder the regions share, so a save made on one disc is read from the
    # store by the other. With it, phase_mutate edits the store's card0.dw3s
    # (canonical layout, container CRC redone) instead of save0.bin.
    store: bool = False
    # P6.24 single build: run the phases through the region launcher instead
    # of a per-region binary. `launch` lists the region trees the launcher finds
    # under its runtime base ("us", "eu" or "us+eu"; symlinks to the runtimes);
    # `env` can force the region (DW3_REGION) or script the picker
    # (DW3_REGION_PICK_KEYS). The runner leaves DW3_RUNTIME unset, so the
    # launcher's choice decides which binary and runtime run. `version` is the
    # region that must come out. PC only.
    launch: str = ""
    # P6.38 Restart into the chosen region: the same idea for the combined
    # (--bin-both) binary running one scenario: a runtime base folder holding
    # symlinks to the runtimes listed here ("us+eu") is given to every phase as
    # DW3_RUNTIME_BASE, so the Region at start row finds the other region's data
    # on the "card". DW3_RUNTIME still names the run's own region. PC only.
    base: str = ""
    # Compare the captures with the references of this other scenario instead
    # of the scenario's own (the launcher run must be bit-identical to the
    # plain per-region run, so no reference of its own is needed).
    ref_from: str = ""
    # P9.7 slice 2: checkpoints compared with a named checkpoint of another
    # scenario instead of their own reference: (shot, scenario, its shot). The
    # battle's excursion run takes the plain run's pictures a few frames later
    # and must match them; run the plain scenario with --update first.
    ref_map: tuple[tuple[str, str, str], ...] = ()
    # P6.32 phase 7: with ref_from, these checkpoints still have a reference of this
    # scenario's own (a picture that differs on purpose from the other scenario's, such
    # as the map view's help line under another button layout); --update writes only them.
    own_refs: tuple[str, ...] = ()
    # MOD map upscale: "synthetic" makes the runner write the synthetic map pack
    # (tools/mods/make_synthetic_pack.py, gradients, no game art) into the run's
    # config folder (`mods/`) before the first phase. PC only.
    mod_pack: str = ""
    # P6.7 text pack (docs/LOCALIZATION.md): the runner runs the scenario on a private copy of
    # the runtime (symlinks) whose l10n/ folder holds a synthetic text pack built from the
    # disc's own English text by tools/l10n/build_pack.py: "upper" (every string upper-cased,
    # ã õ ç added), "partial" (a third of the strings kept in English), "stress" (every line as
    # wide as the widest of the other four EU languages), "proof" (English, but the card messages are
    # the glyph proof) or "none" (no pack at all); "+keys" adds the key variants. The
    # scenario selects it with `cfg` (text_language=ptbr) or `env` (DW3_LANG=ptbr). Nothing of
    # it is committed: the references are local, like every other. PC only.
    l10n: str = ""
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

# Patch 0199: PRESS START is drawn only on the frames the pad is read. The log
# line names the frame the prompt is drawn (N) and the frame Start starts to be
# read (M); the game prints `cause none` only when M <= N and `cause dead window`
# when the prompt came first (the original drew it seven frames before it read
# Start). `title-ready` presses Start every other frame from the first frame of
# the boot, so the first press the title can take is accepted at M or one frame
# later; the return to title (`-return-title` below) is the same check after the
# field menu's Return to title.
_READY = r"dw3-title: press start shown at frame \d+, input ready at frame \d+ \(cause none: "
_READY_ACC = r"dw3-title: first Start accepted at frame \d+ \(\+[01] after input opened\)"
_READY_BAD = r"dw3-title: press start shown at frame \d+, input ready at frame \d+ \(cause dead window"

SCENARIOS: list[Scenario] = []


def _add(*s: Scenario) -> None:
    SCENARIOS.extend(s)


_add(Scenario("us-title", "us", "quick", expect_restore=((0x1C1, M),),
              expect_log=((0, r"dw3-title-menu: no autosave\.bin: plaques New Game, Options, "
                              r"Load Game \(slot list\), Quit game"),
                          # the title's short version label (replays pin the text)
                          (0, r'dw3-title-overlay: version label "replay US" \(short form')),))
for _lang in EU_LANGS:
    # P6.1 (patch 0177): the language select takes the 16:9 gutter; the
    # title that follows still has the four plaques (no auto-save file).
    _add(Scenario(f"eu-lang-{_lang}", "eu", "quick",
                  expect_log=((0, r"dw3-widescreen: gutter 53 px per side \(was -?\d+; 16:9\) mode file 0x165"
                                  r"(?s:.*)dw3-title-menu: no autosave\.bin: plaques New Game, Options, "
                                  r"Load Game \(slot list\), Quit game"),
                              (0, r'dw3-title-overlay: version label "replay EU" \(short form')),))

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
           r"dw3-port-options: group open display \(4 row\(s\), title\)(?s:.*)"
           r"dw3-port-options: group close display \(back to the group list\)(?s:.*)"
           r"dw3-port-options: group open gameplay \(4 row\(s\), title\)(?s:.*)"
           r"dw3-port-options: group close gameplay \(back to the group list\)(?s:.*)"
           # D1: the Restart hotkey is the last row of Controls now
           r"dw3-port-options: group open controls \(5 row\(s\), title\)(?s:.*)"
           r"dw3-port-options: restart hotkey enabled(?s:.*)"
           r"dw3-port-options: group close controls \(back to the group list\)(?s:.*)"
           r"dw3-port-options: close \(resume runOptions\)",
           r"dw3-options: saved \S*port_options\.cfg \(widescreen=0 fast_forward=3 restart_hotkey=1 music_volume=10 sfx_volume=10 "
           r"encounter_rate=0 text_speed=0 battle_menu_memory=0\)")
_OPT_P2 = (r"dw3-port-options: group open gameplay \(4 row\(s\), title\)(?s:.*)"
           r"dw3-port-options: group open controls \(5 row\(s\), title\)",
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
           r"dw3-port-options: group open sound \(3 row\(s\), title\)",
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

# P6.35 walkthrough (docs/P6.35_WALKTHROUGH.md): the step resolver on the real game
# state. The debug probe (patch 0186, DW3_WALKTHROUGH_PROBE) logs
# `dw3-walkthrough: progress P half H area A -> chapter C step S (rule R)` when
# the progress, the map or the flag changes. Phase 1: three points of the first
# chapter: progress 0 in Kusanagi City's street corner (the real world, no map
# area), progress 2 in the Login Room, progress 3 in the Central Sector (Genji's
# flag 0x400D is clear: the step has a sub-condition, rule progress+flag).
# Slice 2: the start, the middle and the end of every other chapter of the first
# half, booted in the stage of the step (progress 4 closes chapter 1). The
# `rule progress+flag` points are values whose first row waits for a flag.
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

# (tag, DW3_START_MODE, progress, half, area, chapter, step, rule): start, middle and end of
# chapters 2 to 10, and the end of chapter 1 (progress 4). The mode is the stage of the step.
WT_CHAPTER_POINTS = (
    ("p4", "0x200", 4, 0, 19, "ch01", "ch01.s06", "progress"),
    ("p5", "0x22C", 5, 0, 30, "ch02", "ch02.s01", "progress"),
    ("p8", "0x239", 8, 0, 36, "ch02", "ch02.s04", "progress+flag"),
    ("pa", "0x240", 10, 0, 41, "ch02", "ch02.s10", "progress"),
    ("pb", "0x204", 11, 0, 19, "ch03", "ch03.s01", "progress"),
    ("pc", "0x20B", 12, 0, 19, "ch03", "ch03.s02", "progress"),
    ("pe", "0x207", 14, 0, 19, "ch03", "ch03.s05", "progress"),   # the Reliability Spot (WSTAG460); it had no overlay table entry until patch 0197
    ("pf", "0x207", 15, 0, 19, "ch04", "ch04.s01", "progress"),
    ("p10", "0x25C", 16, 0, 38, "ch04", "ch04.s02", "progress"),
    ("p11", "0x252", 17, 0, 7, "ch04", "ch04.s03", "progress"),
    ("p12", "0x254", 18, 0, 7, "ch05", "ch05.s01", "progress"),
    ("p14", "0x255", 20, 0, 7, "ch05", "ch05.s03", "progress+flag"),
    ("p17", "0x219", 23, 0, 19, "ch05", "ch05.s07", "progress+flag"),
    ("p18", "0x260", 24, 0, 33, "ch06", "ch06.s01", "progress"),
    ("p19", "0x240", 25, 0, 41, "ch06", "ch06.s02", "progress"),
    ("p1a", "0x246", 26, 0, 35, "ch06", "ch06.s03", "progress"),
    ("p1b", "0x29C", 27, 1, 14, "ch07", "ch07.s01", "progress"),
    ("p1c", "0x2AD", 28, 1, 41, "ch07", "ch07.s02", "progress"),
    ("p1d", "0x207", 29, 0, 19, "ch07", "ch07.s03", "progress"),
    ("p1e", "0x207", 30, 0, 19, "ch08", "ch08.s01", "progress"),
    ("p20", "0x26D", 32, 0, 8, "ch08", "ch08.s03", "progress+flag"),
    ("p21", "0x218", 33, 0, 19, "ch08", "ch08.s05", "progress+flag"),
    ("p22", "0x2C7", 34, 1, 33, "ch09", "ch09.s01", "progress"),
    ("p23", "0x2D5", 35, 1, 5, "ch09", "ch09.s02", "progress"),
    ("p24", "0x272", 36, 1, 19, "ch09", "ch09.s03", "progress"),
    ("p25", "0x288", 37, 1, 19, "ch10", "ch10.s01", "progress+flag"),
    ("p27", "0x288", 39, 1, 19, "ch10", "ch10.s04", "progress"),
    ("p29", "0x2DF", 41, -1, -1, "ch10", "ch10.s06", "progress+flag"),
    # slice 3: the last battle (progress 0x2B, both discs), in the Control Room that starts it
    ("p2b", "0x2DF", 43, -1, -1, "ch11", "ch11.s01", "progress"),
)
# The EU extra chapter (progress 0x2D): the start (the Login Room, WSTAG925), the middle (Qing Long
# City, WSTAG956, where a challenger waits) and the end (the Digimon Arena, WSTAG927). Every row of
# the chapter waits for a flag, and flags cannot be set from the environment, so all three points
# resolve to the first row; they prove that the extra stages boot at 0x2D and that the resolver runs
# in them. The same three boots on US must not show the chapter: US has no row at 0x2D.
WT_EXTRA_POINTS = (
    ("p2d", "0x276", 1, 19),
    ("p2d-mid", "0x29C", 1, 14),
    ("p2d-end", "0x278", 1, 19),
)
for _v in ("us", "eu"):
    for _tag, _mode, _prog, _half, _area, _ch, _step, _rule in WT_CHAPTER_POINTS:
        _line = (rf"progress {_prog} half {_half} area {_area} -> chapter {_ch} step "
                 + re.escape(_step) + r" \(rule " + re.escape(_rule) + r"\)")
        _add(Scenario(f"{_v}-walkthrough-{_tag}", _v, "full", turbo=4, timeout=120, log_only=True,
                      phases=(f"{_v}-walkthrough",),
                      env=(("DW3_START_MODE", _mode), ("DW3_PROGRESS", str(_prog)),
                           ("DW3_WALKTHROUGH_PROBE", "1")),
                      expect_log=((0, r"dw3-walkthrough: " + _line),),
                      forbid_log=((0, r"step not recognised|chapter - step -"),),
                      note=f"P6.35 slice {3 if _ch == 'ch11' else 2}: progress {_prog} in mode {_mode} resolves to {_step}"))
for _tag, _mode, _half, _area in WT_EXTRA_POINTS:
    _add(Scenario(f"eu-walkthrough-{_tag}", "eu", "full", turbo=4, timeout=120, log_only=True,
                  phases=("eu-walkthrough",),
                  env=(("DW3_START_MODE", _mode), ("DW3_PROGRESS", "45"), ("DW3_WALKTHROUGH_PROBE", "1")),
                  expect_log=((0, rf"dw3-walkthrough: progress 45 half {_half} area {_area} -> chapter ch12 "
                                  r"step ch12\.s01 \(rule progress\+flag\)"),),
                  forbid_log=((0, r"step not recognised|chapter - step -"),),
                  note=f"P6.35 slice 3: the EU extra chapter's stage {_mode} at progress 0x2D resolves to its first row"))
    _add(Scenario(f"us-walkthrough-{_tag}", "us", "full", turbo=4, timeout=120, log_only=True,
                  phases=("us-walkthrough",),
                  env=(("DW3_START_MODE", _mode), ("DW3_PROGRESS", "45"), ("DW3_WALKTHROUGH_PROBE", "1")),
                  expect_log=((0, rf"dw3-walkthrough: progress 45 half {_half} area {_area} -> chapter - "
                                  r"step - \(rule above-table\)"),),
                  forbid_log=((0, r"ch12"),),
                  note=f"P6.35 slice 3: region gating, the US build never shows the extra chapter "
                       f"(progress 0x2D in mode {_mode} is past its table)"))

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

# P6.36 music at normal speed while turbo is on (docs/P6.36_TURBO_MUSIC.md,
# patch 0191, port/audio/turbo_music.c). The runner pins Turbo music = Fast for
# every turbo scenario above (their recordings were made with the music ticking
# once per game frame); these set it to 1x with the frame clock (a replay never
# uses the wall clock) and judge by the `dw3-audio: music tick` lines, since a
# replay cannot listen. `rate=` is the ticks per 60 (EU: 50) game frames on the
# frame clock: 15 (EU 12.5) at 4x, 40 (EU 33.3) at the menu's 1.5x.
_TM_ENV = (("DW3_TURBO_MUSIC", "0"), ("DW3_TURBO_MUSIC_CLOCK", "frames"))
_TM_WIN = r"music tick window frames=60 ticks=15 "
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-turbo-music-field", _v, "full", turbo=4, timeout=300, log_only=True,
                  env=_ENC_ENV + _TM_ENV, cfg=(("encounter_rate", "2"),),
                  expect_log=((0, r"music tick source=frames rate=(?:15|12\.5)/s game_rate=4x(?s:.*)"
                                  + _TM_WIN + r"(?s:.*)" + _TM_WIN + r"(?s:.*)" + _TM_WIN),),
                  forbid_log=((0, r"music tick window frames=60 ticks=(?!1[456] )\d+ "),
                              (0, r"SEP end-of-track")),
                  note="turbo 4, Turbo music 1x: 15 ticks per 60 game frames (a quarter of today's 60), the field music never ends (P6.36)"))
    _add(Scenario(f"{_v}-turbo-music-battle", _v, "full", turbo=4, timeout=600, log_only=True,
                  phases=(f"{_v}-victory-music",),
                  env=_VICTORY_ENV + _TM_ENV,
                  expect_log=((0, r"music tick source=frames rate=(?:15|12\.5)/s game_rate=4x(?s:.*)"
                                  r"FIGHTSTG_start(?s:.*)"
                                  r"music tick source=frames rate=(?:40|33\.3)/s game_rate=1\.5x(?s:.*)"
                                  r"SEP NRPN VAB attr \d+=\d+ \(msb 16\) ignored seq=1 tr=14(?s:.*)"
                                  r"WFIGHTMN_endBattle result=1(?s:.*)"
                                  r"STFGTREP_start(?s:.*)"
                                  r"STFGTREP return(?s:.*)"
                                  r"SEP loop count 127 seq=2 tr=0"),
                              (0, _TM_WIN)),
                  forbid_log=((0, r"STFGTREP return(?s:.*)SEP end-of-track seq=2 tr=0"),
                              (0, r"music tick window frames=60 ticks=(?!1[456] |39 |40 |41 )\d+ ")),
                  note="turbo 4 battle won with Cross, Turbo music 1x: 15 ticks per 60 frames (40 in the battle menu), the win jingle (seq=1 tr=14) starts at the knock-out, the field music follows the report (P6.36)"))

# P6.36 phase 3 (patch 0194): the win jingle is held for 4 s of music time, i.e.
# 240 music ticks (EU-PAL 200) = 960 game frames at turbo 4, before the field
# music request that follows the battle is played. The log must show the hold
# start at the knock-out, the report returning, then the release after 4000 ms
# (elapsed) and only then the field music (seq=2 tr=0). With Turbo music Fast
# (the runner's pin for turbo recordings) there is no hold line at all and the
# field music follows the report as before. two-battles: the second battle's
# own music call ends the hold ("music request") and the request kept is dropped.
_HOLD_RELEASE = r"jingle hold release after 4000 ms \(elapsed\)"
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-turbo-music-hold", _v, "full", turbo=4, timeout=600, log_only=True,
                  phases=(f"{_v}-victory-music",),
                  env=_VICTORY_ENV + _TM_ENV,
                  expect_log=((0, r"jingle hold start \(4000 ms of music time = (?:240|200) ticks, game_rate=4x\)(?s:.*)"
                                  r"STFGTREP_start(?s:.*)"
                                  r"STFGTREP return(?s:.*)"
                                  + _HOLD_RELEASE + r"(?s:.*)"
                                  r"SEP loop count 127 seq=2 tr=0"),),
                  forbid_log=((0, r"jingle hold start(?s:.*?)SEP loop count 127 seq=2 tr=0(?s:.*?)" + _HOLD_RELEASE),
                              (0, r"jingle hold (?:superseded|release after \d+ ms \((?!elapsed))"),
                              (0, r"SEP end-of-track seq=(?:1 tr=14|2 tr=0)")),
                  note="turbo 4, Turbo music 1x: the win jingle plays 4 s of music time (960 game frames) before the field music, which waited for it (P6.36 phase 3)"))
    _add(Scenario(f"{_v}-turbo-music-nohold", _v, "full", turbo=4, timeout=600, log_only=True,
                  phases=(f"{_v}-victory-music",),
                  env=_VICTORY_ENV + (("DW3_TURBO_MUSIC", "1"),),
                  expect_log=((0, r"SEP loop count 127 seq=1 tr=14(?s:.*)"
                                  r"STFGTREP return(?s:.*)"
                                  r"SEP loop count 127 seq=2 tr=0"),),
                  forbid_log=((0, r"jingle hold"), (0, r"music tick")),
                  note="turbo 4, Turbo music Fast: no hold, the field music follows the report as before (P6.36 phase 3)"))
    _add(Scenario(f"{_v}-turbo-music-hold-stopped", _v, "full", turbo=4, timeout=600,
                  phases=(f"{_v}-two-battles",),
                  env=(("DW3_START_MODE", "0x600"), ("DW3_FORCE_ENCOUNTER", "34"), ("DW3_FORCE_PARTY", "0"), ("DW3_AUTO_DOOR_MODE", "0x600"), ("DW3_AUTO_DOOR_FRAMES", "120")) + _TM_ENV,
                  expect_log=((0, r"jingle hold start(?:(?!jingle hold release)(?s:.))*"
                                  r"AUTO_DOOR requestMode=0x600(?:(?!jingle hold release)(?s:.))*"
                                  r"jingle hold release after \d+ ms \((?:stopped|music request)\)"),),
                  forbid_log=((0, r"jingle hold start(?:(?!jingle hold release)(?s:.))*"
                                  r"jingle hold release after \d+ ms \(elapsed\)(?s:.*)AUTO_DOOR requestMode=0x600"),),
                  note="turbo 4, Turbo music 1x: a second battle begins during the hold; its own music call ends it and the kept field request is dropped (P6.36 phase 3)"))

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


# Patch 0198 (BUG-6 of the p56 OLED run): the battle's Digivolve command crashed in
# GPU_Enqueue a few frames after the pick that goes back to a lower form. The Digimon
# change draws the old Digimon as a wireframe (FIGHTSTG_drawMeshWireframe), and that
# routine linked its lines with a 24-bit PS1 address, so the layer's OT held a 32-bit
# piece of a 64-bit pointer and DrawOTag followed it. The forced battle of p77 with
# DW3_FORCE_DIGI_SLOTS=386 (Kotemon may digivolve into Dinohumon) and an enemy that
# lasts: the command up, back to the base form, up again. The log must show all
# three changes starting and ending (the HUD name follows 383 -> 386 -> 383 -> 386)
# and no crash (the runner's own verdict); before 0198 the second change segfaulted
# on the PC (SIGSEGV in GPU_Enqueue, the Switch's `CRASH user exception`). Log only.
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-battle-digivolve-command", _v, "full", turbo=4, timeout=600, log_only=True,
                  phases=(f"{_v}-battle-digivolve-command",),
                  env=_P77_ENV_REPORT + (("DW3_FORCE_DIGI_SLOTS", "386"), ("DW3_FORCE_ENEMY_HP", "9999")),
                  expect_log=((0, r"DW3_FORCE_DIGI_SLOTS partner 0 slot 0 is Digimon id 386(?s:.*)"
                                  r"close list=digivolve count=2 sel=1(?s:.*)"
                                  r"digimon change wireframe starts \(to Digimon id 386, digivolve command\)(?s:.*)"
                                  r"digimon change done, back to the battle \(Digimon id 386\)(?s:.*)"
                                  r"close list=digivolve count=2 sel=0(?s:.*)"
                                  r"digimon change wireframe starts \(to Digimon id 383, digivolve command\)(?s:.*)"
                                  r"digimon change done, back to the battle \(Digimon id 383\)(?s:.*)"
                                  r"close list=digivolve count=2 sel=1(?s:.*)"
                                  r"digimon change wireframe starts \(to Digimon id 386, digivolve command\)(?s:.*)"
                                  r"digimon change done, back to the battle \(Digimon id 386\)"),),
                  forbid_log=((0, r"CRASH|Segmentation fault"),),
                  note="the Digivolve command up, back to the base form (the BUG-6 crash) and up again: the wireframe's lines link with the full pointer (patch 0198)"))

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

# P51 item 5: the Cheats group is on the title's group list with the master
# switch OFF (it used to appear only once the master was on). Opens the Cheats
# screen from the list and returns with the cursor on Cheats.
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-options-cheats-listed", _v, "full", timeout=300,
                  expect_log=((0, r"dw3-port-options: group list n=10 \(cheats listed, master off, "
                                  r"locked in game 0\)(?s:.*)dw3-port-options: group open cheats \(group\)"),
                              # P51 item 6: the game's menu background under the Options screens
                              (0, r"dw3-options: menu background tile 96x96 from the status sprites "
                                  r"\(sprite 0x1D\) ready in \d+ us"),),
                  forbid_log=((0, r"menu background tile not available"),),
                  note="the Cheats group is listed with the master switch off (P51 item 5)"))

# P6.37: `show_version=0` (a file-only key, seeded here) takes the port version out of
# the title's corner (title / menu pictures) while the Options footer stays (always
# shown). The runner pins the text to "replay" (DW3_VERSION_OVERRIDE), so the other
# title and Options pictures keep one reference across commits. Recordings are the
# existing title and Options ones; the references are this scenario's own.
for _v, _rec in (("us", "us-title"), ("eu", "eu-lang-en")):
    _add(Scenario(f"{_v}-title-noversion", _v, "full", phases=(_rec,), timeout=300,
                  cfg=(("show_version", "0"),),
                  expect_log=((0, r"dw3-options: file keys .* show_version=0"),),
                  forbid_log=((0, r"dw3-title-overlay: version label"),),
                  note="show_version=0: the title's corner is clear, no label is rendered (P6.37)"))
    _add(Scenario(f"{_v}-options-noversion", _v, "full", phases=(f"{_v}-options-cheats-listed",),
                  timeout=300, cfg=(("show_version", "0"),),
                  expect_log=((0, r"dw3-options: file keys .* show_version=0(?s:.*)"
                                  r"dw3-port-options: group open cheats \(group\)"),),
                  note="show_version=0: the Options footer stays (P6.37)"))

# P51 item 4: the Region at start row of the combined image (tier `both`: needs
# `--bin-both`, the one binary of both game copies; a plain US or EU build has no
# such row). Two phases on one config folder, which is also the top-level folder
# here (DW3_CONFIG_DIR), so the row writes region_default into the file the
# picker reads.
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-options-region", _v, "both", timeout=300,
                  phases=(f"{_v}-options-region-p1", f"{_v}-options-region-p2"),
                  expect_log=((0, r"dw3-port-options: group open saves \(3 row\(s\), title\)(?s:.*)"
                                  r"dw3-options: region_default us saved to \S+port_options\.cfg "
                                  r"\(takes effect at the next start\)(?s:.*)"
                                  r"dw3-options: region_default eu saved to \S+port_options\.cfg"),
                              (1, r"dw3-options: region_default eu read from \S+port_options\.cfg")),
                  forbid_log=((0, r"NOT saved"), (1, r"NOT saved")),
                  note="Options > Gameplay > Region at start: Ask, US, EU, read back (P51 item 4, --bin-both)"))

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
    # menu_instant_exit=0: the recording checks the dissolve back to the field, which the
    # instant exit (patch 0200, default on) skips
    _add(Scenario(f"{_v}-items-menu", _v, "full", turbo=4, timeout=300,
                  phases=(f"{_v}-items-menu",), env=_ITEMS_ENV,
                  cfg=(("encounter_rate", "2"), ("menu_instant_exit", "0")),
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
    # menu_instant_exit=0: its last checkpoint is taken after the dissolve back to the field
    # (patch 0200 skips it by default; the glyphs-*-items scenarios inherit the pin)
    _add(Scenario(f"{_v}-menu-screens", _v, "full", turbo=4, timeout=400,
                  phases=(f"{_v}-menu-screens",), env=_ITEMS_ENV,
                  cfg=(("encounter_rate", "2"), ("menu_instant_exit", "0")),
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

# Instant menu exit (patch 0200, Options > Menus > Instant menu exit, key
# menu_instant_exit, default on): leaving the field menu's screens for the field
# skips the block dissolve and keeps the game state. One recording, three exits
# (the field menu closed over the field, Items and Map left for the field); the
# env and cfg of items-menu. `-off` is the same recording with the key at 0:
# the original dissolve, whose log line carries the same clocks and RNG as the
# instant one (compared by hand in the PR; the dissolve takes 96 frames on US and
# 80 on EU, the instant exit says how many it skipped).
for _v, _ids, _skip in (("us", (0x159, 0x1FA, 0x1C2, 0x158, 0x1F9), r"9\d"),
                        ("eu", (0x167, 0x208, 0x1D1, 0x166, 0x207), r"[78]\d")):
    _fight, _menu, _report, _field, _status = _ids
    _restore = ((_fight, M), (_menu, S), (_report, M), (_field, M), (_status, M), (_field, M),
                (_status, M), (_field, M))
    _add(Scenario(f"{_v}-menu-instant-exit", _v, "full", turbo=4, timeout=400,
                  phases=(f"{_v}-menu-instant-exit",), env=_ITEMS_ENV,
                  cfg=(("encounter_rate", "2"),), expect_restore=_restore,
                  expect_log=((0, r"menu_instant_exit=1(?s:.*)"
                                  r"dw3-menu: exit to field instant \(skipped %s transition frames, from items\)(?s:.*)"
                                  r"dw3-menu: exit to field instant \(skipped %s transition frames, from map\)" % (_skip, _skip)),),
                  forbid_log=((0, r"exit to field with the original transition"),),
                  note="Items and Map left for the field without the dissolve; the field menu closed "
                       "over the field logs nothing (patch 0200)"))
    _add(Scenario(f"{_v}-menu-instant-exit-off", _v, "full", turbo=4, timeout=400,
                  phases=(f"{_v}-menu-instant-exit",), env=_ITEMS_ENV,
                  cfg=(("encounter_rate", "2"), ("menu_instant_exit", "0")), expect_restore=_restore,
                  expect_log=((0, r"menu_instant_exit=0(?s:.*)"
                                  r"dw3-menu: exit to field with the original transition \(instant exit off, from items\)(?s:.*)"
                                  r"dw3-menu: exit to field with the original transition \(instant exit off, from map\)"),),
                  forbid_log=((0, r"exit to field instant"),),
                  note="menu_instant_exit=0: the original dissolve on both exits (patch 0200)"))

# P6.34 phase 2 (patch 0190): equip in Status > Equipment and return to the slot
# list without the long fade round trip, key on (the default). `-off` is the
# same recording with items_equip_stays=0: the original flow, no equip-stays line.
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158, 0x1F9)),
                 ("eu", (0x167, 0x208, 0x1D1, 0x166, 0x207))):
    _fight, _menu, _report, _field, _status = _ids
    _add(Scenario(f"{_v}-items-equip-stays", _v, "full", turbo=4, timeout=400,
                  phases=(f"{_v}-items-equip-stays",), env=_ITEMS_ENV,
                  cfg=(("encounter_rate", "2"),),
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M), (_status, M)),
                  expect_log=((0, r"file keys items_equip_from_items=1 items_equip_stays=1(?s:.*)"
                                  r"equip-stays begin partner=\d+ slot=2 item=\d+ bag=(\d+) worn=(\d+)(?s:.*)"
                                  r"equip-stays slot=2 after=equip frames=\d{1,2} bag=\d+ worn=\d+(?s:.*)"
                                  r"equip-stays list-open slot=2 row=[1-9]\d* cursor=\d+ scroll=\d+ item=\d+ count=\d+(?s:.*)"
                                  r"equip-stays begin partner=\d+ slot=2(?s:.*)"
                                  r"equip-stays slot=2 after=equip frames=\d{1,2} "),),
                  note="equip in Status > Equipment returns to the slot list at once, "
                       "the list opens on the worn item (patch 0190)"))
    _add(Scenario(f"{_v}-items-equip-stays-off", _v, "full", turbo=4, timeout=400,
                  phases=(f"{_v}-items-equip-stays",), env=_ITEMS_ENV,
                  cfg=(("encounter_rate", "2"), ("items_equip_stays", "0")),
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M), (_status, M)),
                  expect_log=((0, r"file keys items_equip_from_items=1 items_equip_stays=0"),),
                  forbid_log=((0, r"equip-stays (?:begin|slot=|list-open)"),),
                  note="items_equip_stays=0: the original equip round trip, no equip-stays line (patch 0190)"))

# P6.34 phase 3 (patch 0192): the whole bag in the equip list (Square cycles the
# filter) and moving an item between Digimon. One recording; `-43` runs it with
# Widescreen off; `-off` has items_bag_view=0 (Square does nothing, no bag-view
# or move line).
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158, 0x1F9)),
                 ("eu", (0x167, 0x208, 0x1D1, 0x166, 0x207))):
    _fight, _menu, _report, _field, _status = _ids
    for _suffix, _cfg in (("", (("encounter_rate", "2"),)),
                          ("-43", (("encounter_rate", "2"), ("widescreen", "0")))):
        _add(Scenario(f"{_v}-items-bag-view{_suffix}", _v, "full", turbo=4, timeout=500,
                      phases=(f"{_v}-items-bag-view",), env=_ITEMS_ENV, cfg=_cfg,
                      expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M), (_status, M)),
                      expect_log=((0, r"file keys items_equip_from_items=1 items_equip_stays=1 items_bag_view=1(?s:.*)"
                                      r"equip-stays begin partner=(\d+) slot=4 item=(\d+) (?s:.*)"
                                      r"bag-view filter=weapons slot=4 rows=\d+ worn-elsewhere=1(?s:.*)"
                                      r"bag-view filter=gear slot=4 rows=\d+ worn-elsewhere=1(?s:.*)"
                                      r"bag-view filter=worn-by-others slot=4 rows=1 worn-elsewhere=1(?s:.*)"
                                      r"move item=\2 from=\1 from-slot=4 to=(\d+) slot=4 bag=(\d+)->\4 worn=(\d+)->\5(?s:.*)"
                                      r"bag-view filter=worn-by-others slot=4 rows=1 worn-elsewhere=1(?s:.*)"
                                      r"move item=\2 from=\3 from-slot=4 to=\1 slot=4 bag=\4->\4 worn=\5->\5"),),
                      note="Square cycles the equip list's filter; Cross on a row worn by another "
                           "Digimon moves it (patch 0192)"))
    _add(Scenario(f"{_v}-items-bag-view-off", _v, "full", turbo=4, timeout=500,
                  phases=(f"{_v}-items-bag-view",), env=_ITEMS_ENV,
                  cfg=(("encounter_rate", "2"), ("items_bag_view", "0")),
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M), (_status, M)),
                  expect_log=((0, r"file keys items_equip_from_items=1 items_equip_stays=1 items_bag_view=0"),),
                  forbid_log=((0, r"bag-view filter=|dw3-items-menu: move item="),),
                  note="items_bag_view=0: Square does nothing in the equip list, no bag-view or move line (patch 0192)"))

# P6.34 phase 3 (patch 0192): the filter titles and the wearer note in the four
# extra EU languages (draft words, native review pending; accents are icons).
for _lang, _id in (("fr", 3), ("it", 4), ("de", 5), ("es", 6)):
    _add(Scenario(f"eu-items-bag-view-{_lang}", "eu", "full", turbo=4, timeout=500,
                  phases=("eu-items-bag-view",),
                  env=_ITEMS_ENV + (("DW3_LANGUAGE", str(_id)),), cfg=(("encounter_rate", "2"),),
                  expect_log=((0, rf"LANGUAGE={_id} \(DW3_LANGUAGE\)(?s:.*)bag-view filter=worn-by-others slot=4 rows=1 "
                                  r"worn-elsewhere=1(?s:.*)move item=\d+ from=\d+ from-slot=4 to=\d+ slot=4"),),
                  note=f"the bag view's titles and wearer note in {_lang} (patch 0192)"))

# P6.34 phase 4 (patch 0193): "use item" stays on the partner step, key on (the
# default). `-off` is the same recording with items_use_stays=0: the original
# round trip on every use (the key-off proof: its captures must equal a
# pre-patch build's); `-43` is Widescreen off; the language runs are EU only.
_USE_LOG = (r"file keys items_equip_from_items=1 items_equip_stays=1 items_bag_view=1 items_use_stays=1(?s:.*)"
            r"use-stays item=(\d+) partner=\d+ member=1 count=3->2 effect=used list=stays row=8 uses=1(?s:.*)"
            r"use-stays item=\1 partner=\d+ member=2 count=2->1 effect=used list=stays row=8 uses=2(?s:.*)"
            r"use-stays item=\1 partner=\d+ member=0 count=1->0 effect=used list=closes row=8 uses=2")
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158, 0x1F9)),
                 ("eu", (0x167, 0x208, 0x1D1, 0x166, 0x207))):
    _fight, _menu, _report, _field, _status = _ids
    for _suffix, _cfg in (("", (("encounter_rate", "2"),)),
                          ("-43", (("encounter_rate", "2"), ("widescreen", "0")))):
        _add(Scenario(f"{_v}-items-use-stays{_suffix}", _v, "full", turbo=4, timeout=500,
                      phases=(f"{_v}-items-use-stays",), env=_ITEMS_ENV, cfg=_cfg,
                      expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M), (_status, M)),
                      expect_log=((0, _USE_LOG),),
                      note="Use item on three Digimon without leaving the partner step; the last "
                           "copy takes the original way back (patch 0193)"))
    _add(Scenario(f"{_v}-items-use-stays-off", _v, "full", turbo=4, timeout=500,
                  phases=(f"{_v}-items-use-stays",), env=_ITEMS_ENV,
                  cfg=(("encounter_rate", "2"), ("items_use_stays", "0")),
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M), (_status, M)),
                  expect_log=((0, r"file keys items_equip_from_items=1 items_equip_stays=1 items_bag_view=1 "
                                  r"items_use_stays=0"),),
                  forbid_log=((0, r"dw3-items-menu: use-stays"),),
                  note="items_use_stays=0: the original use round trip, no use-stays line (patch 0193)"))
for _lang, _id in (("fr", 3), ("it", 4), ("de", 5), ("es", 6)):
    _add(Scenario(f"eu-items-use-stays-{_lang}", "eu", "full", turbo=4, timeout=500,
                  phases=("eu-items-use-stays",),
                  env=_ITEMS_ENV + (("DW3_LANGUAGE", str(_id)),), cfg=(("encounter_rate", "2"),),
                  expect_log=((0, rf"LANGUAGE={_id} \(DW3_LANGUAGE\)(?s:.*)use-stays item=\d+ partner=\d+ member=1 "
                                  r"count=3->2(?s:.*)use-stays left|list=closes"),),
                  note=f"use item stays on the partner step in {_lang} (patch 0193)"))

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
                  cfg=(("encounter_rate", "2"), ("map_screen", "1")),
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
                                  r"markers=\d+ home=\d+,\d+ turbo=1 zoom=1\.50 hud=off"
                                  r"(?s:.*)dw3-map: zoom source=button step"
                                  r"(?s:.*)scaler LANCZOS2 (?:built in \d+ ms \(1176x1056, reveal hash [0-9a-f]{8}\)"
                                  r"|loaded from cache in \d+ ms \(1176x1056, cache hit \S+, reveal hash [0-9a-f]{8}\))"
                                  r"(?s:.*)scaler AMD FSR 1 (?:built in|loaded from cache in) \d+ ms"
                                  r"(?s:.*)dw3-map: scaler BILINEAR built in \d+ ms \(no cache, 392x352\)"
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
                                  r"(?s:.*)dw3-map: HUD on \(the bars; the name box stays\)"
                                  r"(?s:.*)dw3-map: HUD off \(the bars; the name box stays\)"),
                              # OBS-3 (p56): the name box's plate is drawn by the view from the
                              # game's own texels, so no pixel of the game lands on its border
                              (0, r"dw3-map: name box plate \(frame 0x3B of FILE_STATUS_SPRITES\): "
                                  r"\d+x\d+ texels at \d+,\d+, \d+ transparent, border box x \d+\.\.\d+ y "
                                  r"\d+\.\.\d+, the game's text is left in x \d+\.\.\d+ y \d+\.\.\d+"),
                              # owner, preview.48: the cursor and the balloon keep one size
                              (0, r"dw3-map: sprites at fixed size \(scale 1/Z, Z = zoom, "
                                  r"reference zoom 1\.0 = [\d.]+ px per source px; the reveal layer"),
                  # P6.32 phase 1 done-when, one line each: no black strip at the open
                  # (the map rectangle covers the whole 16:9 picture), the game's pad is
                  # held for the whole time (and Triangle closes only the view), R3 is
                  # ignored and turbo stays at 1 although DW3_TURBO=4 is set, and the
                  # game's Map screen comes back with the cursor and scroll it was
                  # frozen with
                              (0, r"dw3-map: open half=\d image=392x352 .* turbo=1 zoom=1\.50 hud=off\n"
                                  r"dw3-map: picture coverage full: map rect -?\d+,-?\d+-\d+,\d+ of 1280x720 "
                                  r"\(black strips: none\)"),
                              (0, r"dw3-map: game Map screen frozen underneath: cursor -?\d+,-?\d+ scroll -?\d+,-?\d+"),
                              (0, r"dw3-map: R3 pressed in the view: ignored \(no fast-forward here; turbo=1\)"),
                              (0, r"dw3-map: close requested by the cancel button"
                                  r"(?s:.*)dw3-map: pad held by the view for [1-9]\d* frames \(the game's Map screen "
                                  r"read none of them\), R3 presses ignored 1, turbo=1"
                                  r"(?s:.*)dw3-map: game Map screen back after the close: cursor "
                                  r"(-?\d+),(-?\d+) scroll (-?\d+),(-?\d+) \(at the open \1,\2 / \3,\4\): unchanged"),),
                  # R3 inside the view must not flip the fast-forward state
                  forbid_log=((0, r"dw3-map: capture failed|NOTHING DRAWN|not drawn|MISSING|"
                                  r"dw3-turbo: (?:on|off)\n|\): CHANGED|dw3-map: name box plate NOT BUILT"),),
                  note="Square opens the new full-screen map: balloon, cursor, name box, zoom, scaler cycle, back (patch 0146)"))
    # MOD map upscale: the same recording with a synthetic pack in the config
    # folder. The pack replaces the map picture (1568x1408 texture, same
    # geometry), the reveal layer is the game's own cells drawn 4x larger, and the
    # log says so; references are this scenario's own (run with --update).
    _add(Scenario(f"{_v}-map-view-mod", _v, "full", turbo=4, timeout=600,
                  phases=(f"{_v}-map-view",), env=_ITEMS_ENV + (("DW3_MAP_SYNC", "1"),),
                  cfg=(("encounter_rate", "2"), ("map_screen", "1")), mod_pack="synthetic",
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
    # P51 item 3: the same with ONE shared map for both regions (map_h<half>@4x.png,
    # no per-region file): the loader falls back to it on US and EU alike. The
    # pictures are the per-region synthetic pack's (same gradients): ref_map.
    _add(Scenario(f"{_v}-map-view-mod-shared", _v, "full", turbo=4, timeout=600,
                  phases=(f"{_v}-map-view",), env=_ITEMS_ENV + (("DW3_MAP_SYNC", "1"),),
                  cfg=(("encounter_rate", "2"), ("map_screen", "1")), mod_pack="synthetic-shared",
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M),
                                  (_status, M)),
                  expect_log=((0, r"dw3-map: mod: pack \"synthetic-test\" 0\.0\.1 loaded: "
                                  r"map_h0@4x\.png 1568x1408, reveal 0 pieces from the pack, "
                                  r"[1-9]\d* at 1x nearest.* shared=1 region=0"),),
                  forbid_log=((0, r"dw3-map: mod: original map|dw3-map: capture failed|NOTHING DRAWN"),),
                  note="one shared map pack serves this region (P51 item 3)"))
    _add(Scenario(f"{_v}-map-view-43", _v, "full", turbo=4, timeout=600,
                  phases=(f"{_v}-map-view",), env=_ITEMS_ENV,
                  cfg=(("encounter_rate", "2"), ("map_screen", "1"), ("widescreen", "0")),
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M),
                                  (_status, M)),
                  expect_log=((0, r"dw3-map: Square pressed, view not available "
                                  r"\(host=1 map_screen=1 widescreen shift=0\)"),),
                  forbid_log=((0, r"dw3-map: open"),),
                  note="widescreen off: Square does nothing, the new screen never opens (owner decision 8)"))
    # P6.32 phase 1: the option OFF, which is the default (no map_screen key in the
    # file). The recording presses Square on the Map and everything the view
    # would take; nothing opens, nothing is built, no cache folder, no precompute.
    # The key-off proof for the pictures is us/eu-map-screen (0 differing pixels
    # against main); this one proves the new code is inert.
    _add(Scenario(f"{_v}-map-view-off", _v, "full", turbo=4, timeout=500,
                  phases=(f"{_v}-map-view-off",), env=_ITEMS_ENV,
                  cfg=(("encounter_rate", "2"),),
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M),
                                  (_status, M)),
                  expect_log=((0, r"dw3-options: file keys .* map_screen=0 "
                                  r"(?s:.*)dw3-map: option map_screen=0 \(the default\): the "
                                  r"full-screen map is off, no cache folder, no precompute"
                                  r"(?s:.*)dw3-map: Square pressed, view not available "
                                  r"\(host=1 map_screen=0 widescreen shift=\d+\)"),),
                  forbid_log=((0, r"dw3-map: open|dw3-map: precompute|dw3-map: cache folder|"
                                  r"dw3-map: close|dw3-map: R3 pressed"),),
                  note="the map_screen option off (default): Square on the Map does nothing, the "
                       "game's Map screen is the one of patch 0080 (P6.32 phase 1)"))
    _add(Scenario(f"{_v}-battle-results", _v, "full", turbo=4, timeout=300,
                  env=_P77_ENV_REPORT,
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M)),
                  expect_log=((0, r"WFIGHTMN_endBattle result=1"),),
                  note="the battle results' header bar hangs from the 16:9 left edge (patch 0081)"))
# P6.32 phase 2 (patches 0146, 0147): the area name under the new cursor for EVERY
# visited location, in every disc language. The test aids (PC only): the whole map is
# marked visited (DW3_FORCE_MAP_AREAS=all) and the host's tour (DW3_MAP_TOUR=8) puts the
# cursor on each target in turn: probes at the image edge in the strips the PS1 cursor
# cannot reach, then the 46 locations. The game's own name window answers; the host
# logs one `area name shown` line each, with the source (the game's window, the text
# file and language, the string of the table), the text and whether it fits the box.
# References are the PC host overlay's pictures, from the patched build only.
_NAMES_ENV = _ITEMS_ENV + (("DW3_FORCE_MAP_AREAS", "all"), ("DW3_MAP_TOUR", "8"), ("DW3_MAP_SYNC", "1"))
# (scenario suffix, DW3_LANGUAGE or None for the US text, log name, text file number)
_NAMES_LANGS = {"us": ((None, None, r"en \(US text\)", "9", "en US"),),
                "eu": (("", 2, "en", "a", "en"), ("-fr", 3, "fr", "b", "fr"), ("-it", 4, "it", "c", "it"),
                       ("-de", 5, "de", "d", "de"), ("-es", 6, "es", "e", "es"))}


def _names_expect(lang, tfile):
    one = (r"dw3-map: area name shown: location id=%d source=game name window \(text file 0x" + tfile
           + r" language " + lang + r"(?: \(\d\))?, string %d of the table, \d+ font codes, hash [0-9a-f]{8}, "
           r"window text is the table's string\) text=\"[^\"]+\" \(0 undecoded\) width \d+ of \d+ px "
           r"at window 16,\d+ \d lines: fits")
    return (
        (0, r"dw3: DW3_FORCE_MAP_AREAS spots 1\.\.46 visited"),
        (0, r"dw3-map: marker list: 46 locations \(46 visited\), 22 towns \(\d+ listed for this progress\), "
            r"current area \d+ = location id=[1-9]\d*; area names: text file 0x" + tfile + r", language "
            + lang + r" \(\d\); PS1 cursor reach x 22\.\.370 y 15\.\.290: locations partly out of it 0, wholly out 0"),
        (0, r"dw3-map: town id=13 at 21,242 art .* touches the strip\(s\) the PS1 cursor cannot reach \((?:left )?bottom\)"),
        (0, r"dw3-map: tour armed \(DW3_MAP_TOUR=8\): \d+ targets"),
        # the cursor goes through every strip (left, right, top, bottom) and comes back
        (0, r"dw3-map: cursor in the strip the PS1 cursor cannot reach \(left \) at -?\d+,-?\d+, location id=0"),
        (0, r"dw3-map: cursor in the strip the PS1 cursor cannot reach \(right \) at -?\d+,-?\d+, location id=0"),
        (0, r"dw3-map: cursor in the strip the PS1 cursor cannot reach \(top \) at -?\d+,-?\d+, location id=0"),
        (0, r"dw3-map: cursor in the strip the PS1 cursor cannot reach \(bottom\) at -?\d+,-?\d+, location id=0"),
        (0, r"dw3-map: cursor back inside the PS1 cursor's reach at -?\d+,-?\d+, location id=[1-9]\d*"),
    ) + tuple((0, one % (i, i)) for i in range(1, 47)) + (
        (0, r"dw3-map: tour done: 46 of 46 visited locations named by the game's own name window, 0 bad"),
        (0, r"dw3-map: area names this open: [1-9]\d* shown by the game's name window, 0 bad"),
    )


_NAMES_FORBID = ((0, r"dw3-map: area name MISMATCH|: BAD \(|IS NOT the table|dw3-map: capture failed|NOTHING DRAWN|"
                     r"dw3-map: current area \d+ gives location|"
                     r"dw3-map: name box plate NOT BUILT"),)
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158, 0x1F9)),
                 ("eu", (0x167, 0x208, 0x1D1, 0x166, 0x207))):
    _fight, _menu, _report, _field, _status = _ids
    for _suffix, _lang_id, _lang, _tfile, _label in _NAMES_LANGS[_v]:
        _env = _NAMES_ENV + ((("DW3_LANGUAGE", str(_lang_id)),) if _lang_id else ())
        _add(Scenario(f"{_v}-map-names{_suffix or ''}", _v, "full", turbo=4, timeout=600,
                      phases=(f"{_v}-map-names",), env=_env,
                      cfg=(("encounter_rate", "2"), ("map_screen", "1")),
                      expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M), (_status, M)),
                      expect_log=_names_expect(_lang, _tfile),
                      forbid_log=_NAMES_FORBID,
                      note=f"the new full-screen map names every visited location with the game's own name "
                           f"window (language {_label}), through the strips the PS1 cursor cannot "
                           f"reach (P6.32 phase 2, patch 0147)"))

# P6.32 phase 3 (no patch): zoom about the cursor, the step ladder, the pan limits,
# the centre-on-your-area button (Select), the cursor's acceleration and analog speed
# (the left stick), the FIT backdrop and the bilinear scaler. The proof is in the log
# lines (rule 12); the references are the PC host overlay's pictures. One recording
# for US and EU.
_ZOOM_NUM = r"-?\d+(?:\.\d+)?"
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158, 0x1F9)),
                 ("eu", (0x167, 0x208, 0x1D1, 0x166, 0x207))):
    _fight, _menu, _report, _field, _status = _ids
    _add(Scenario(f"{_v}-map-zoom", _v, "full", turbo=4, timeout=700,
                  phases=(f"{_v}-map-zoom",),
                  # DW3_MAP_LSTICK: the left stick's keyframes (view frame:x:y, raw -127..127) for the
                  # analog speed (a digital replay pad has no stick): half tilt left for 60 frames, then
                  # full tilt left for 30 (the view opens at game frame 4001, so view frame k is game
                  # frame 4001 + k: the recording's Select, L1 presses and shots are around it)
                  env=_ITEMS_ENV + (("DW3_MAP_SYNC", "1"), ("DW3_MAP_LSTICK", "1509:-64:0,1569:0:0,1589:-127:0,1619:0:0")),
                  cfg=(("encounter_rate", "2"), ("map_screen", "1")),
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M),
                                  (_status, M)),
                  expect_log=(
                      # the limits of 4.2g and the ladder of 4.4 at the open; the view opens
                      # on the current area and the backdrop is built
                      (0, r"dw3-map: zoom limits: FIT 0\.\d+, steps 1\.00 1\.25 1\.50 2\.00 3\.00, top "
                          r"3\.00x \(3\.0: the source cannot show more, 4\.0 needs a higher-resolution "
                          r"source\); pan limit = the image box 392x352 at every zoom \(no margin, "
                          r"centred where the whole map fits\); the cursor reaches the whole image, the "
                          r"strips the PS1 cursor cannot reach included \(x < 22, x > 370, y < 15, y > 290\); "
                          r"Select centres on the current area; backdrop beside the map: the map's own "
                          r"colours \(32x18 cells, dim 50%\)"),
                      # zoom about the cursor: the cursor does not move on the picture in a
                      # step the pan limit allows (in 1.5 -> 2.0 -> 3.0 and out again)
                      (0, r"dw3-map: zoom source=button step 4 \(x\) 1\.50x -> 2\.00x anchor=cursor map "
                          r"\d+,\d+ \(sprite \d+,\d+\) on the picture " + _ZOOM_NUM + r"," + _ZOOM_NUM
                          + r" -> " + _ZOOM_NUM + r"," + _ZOOM_NUM + r" \(moved 0\.0 px\) view centre"),
                      (0, r"dw3-map: zoom source=button step 5 \(x\) 2\.00x -> 3\.00x anchor=cursor map "
                          r"\d+,\d+ \(sprite \d+,\d+\) on the picture " + _ZOOM_NUM + r"," + _ZOOM_NUM
                          + r" -> " + _ZOOM_NUM + r"," + _ZOOM_NUM + r" \(moved 0\.0 px\) view centre"),
                      (0, r"dw3-map: zoom source=button step 5 \(x\) 3\.00x -> 3\.00x anchor=cursor .* ladder top 3\.00x"),
                      (0, r"dw3-map: zoom source=button step 3 \(x\) 2\.00x -> 1\.50x anchor=cursor"),
                      # 1.0: the 392 px map is 1176 px of the 1280 picture: centred, the cursor
                      # moves because the pan limit wins; FIT too, with the backdrop beside it
                      (0, r"dw3-map: zoom source=button step 1 \(x\) 1\.25x -> 1\.00x anchor=cursor .*pan limit: map fits, centred"),
                      (0, r"dw3-map: zoom source=button step 0 \(fit\) 1\.00x -> 0\.\d+x anchor=cursor .*pan limit: map fits, centred"),
                      (0, r"dw3-map: pan clamp free: view at the image edge: none, whole map fits on x\+y \(zoom;"),
                      (0, r"dw3-map: picture coverage partial: map rect 52,-?\d+-1228,\d+ of 1280x720 \(black strips: none; "
                          r"the backdrop of the map's own colours \(32x18 cells, dimmed to 50%, linear filter\) fills "
                          r"left 52 right 52 top 0 bottom 0 px of 1280x720\)"),
                      (0, r"dw3-map: picture coverage partial: map rect 239,0-1041,720 of 1280x720 \(black strips: none; "
                          r"the backdrop of the map's own colours \(32x18 cells, dimmed to 50%, linear filter\) fills "
                          r"left 239 right 239 top 0 bottom 0 px of 1280x720\)"),
                      (0, r"dw3-map: pc backdrop 32x18 uploaded \(gen \d+, linear\)"),
                      (0, r"dw3-map: picture coverage full: map rect -?\d+,-?\d+-\d+,\d+ of 1280x720 \(black strips: none\)"
                          r"(?s:.*)dw3-map: picture coverage partial(?s:.*)dw3-map: picture coverage full"),
                      # the right stick: smooth, about the cursor
                      (0, r"dw3-map: zoom source=stick \(axis 127\) zoom " + _ZOOM_NUM + r" \(from " + _ZOOM_NUM
                          + r"\) anchor=cursor map \d+,\d+ .*\(moved 0\.0 px\)"),
                      # a zoom the image's edge does not allow: the pan limit wins and the line says so
                      (0, r"dw3-map: zoom source=button step \d \(x\) [\d.]+x -> [\d.]+x anchor=cursor map 2,\d+ .*"
                          r"\(moved [1-9][\d.]* px, pan limit: left\)"),
                      (0, r"dw3-map: zoom source=button step \d \(x\) [\d.]+x -> [\d.]+x anchor=cursor map \d+,\d+ .*"
                          r"\(moved [1-9][\d.]* px, pan limit: right\)"),
                      # the pan limit holds the view at the image's edges at 3.0, in the corners:
                      # the view's edge is the image's, never past it
                      (0, r"dw3-map: pan clamp holds: view at the image edge: left\+top, whole map fits on neither axis "
                          r"\(the view follows the cursor; zoom 3\.000, view centre " + _ZOOM_NUM + r"," + _ZOOM_NUM
                          + r" wanted " + _ZOOM_NUM + r"," + _ZOOM_NUM + r", map 392x352, view covers x 0\.0\.\.[\d.]+ y 0\.0\.\.[\d.]+ "
                          r"of the image, never past it\)"),
                      (0, r"dw3-map: pan clamp holds: view at the image edge: [a-z+]*right[a-z+]*, whole map fits on neither axis .*zoom 3\.000"),
                      (0, r"dw3-map: pan clamp holds: view at the image edge: [a-z+]*bottom, whole map fits on neither axis .*zoom 3\.000"
                          r".*view covers x [\d.]+\.\.392\.0 y [\d.]+\.\.352\.0 of the image"),
                      (0, r"dw3-map: pan clamp free: view at the image edge: none, whole map fits on neither axis \(the view follows"),
                      (0, r"dw3-map: cursor in the strip the PS1 cursor cannot reach \([a-z ]*left [a-z ]*\) at -?\d+,-?\d+"),
                      (0, r"dw3-map: cursor in the strip the PS1 cursor cannot reach \([a-z ]*top [a-z ]*\) at -?\d+,-?\d+"),
                      (0, r"dw3-map: cursor in the strip the PS1 cursor cannot reach \([a-z ]*right[a-z ]*\) at -?\d+,-?\d+"),
                      (0, r"dw3-map: cursor in the strip the PS1 cursor cannot reach \([a-z ]*bottom\) at -?\d+,-?\d+"),
                      # acceleration: the game's 2, 3, 4 px a frame, then +1 px every 15 frames up to the
                      # cap (10 screen px: 10 source px at zoom 1.0)
                      (0, r"dw3-map: pan speed source=d-pad frame 1: ramp 2\.00 px/frame \(zoom 1\.000, cap 10\.00\)"),
                      (0, r"dw3-map: pan speed source=d-pad frame 11: ramp 3\.00 px/frame \(zoom 1\.000, cap 10\.00\)"),
                      (0, r"dw3-map: pan speed source=d-pad frame 21: ramp 4\.00 px/frame \(zoom 1\.000, cap 10\.00\)"),
                      (0, r"dw3-map: pan speed source=d-pad frame 7\d: ramp 5\.00 px/frame \(zoom 1\.000, cap 10\.00\)"),
                      (0, r"dw3-map: pan speed source=d-pad frame 1[45]\d: ramp 10\.00 px/frame \(zoom 1\.000, cap 10\.00\)"),
                      (0, r"dw3-map: pan end source=d-pad after \d+ frames, peak ramp 10\.00 px/frame, cursor \d+,\d+ -> -?\d+,-?\d+"),
                      # no acceleration where 4 source px is already 12 screen px (3.0)
                      (0, r"dw3-map: pan end source=d-pad after 160 frames, peak ramp 4\.00 px/frame, cursor .* zoom 3\.000"),
                      # Select: the cursor goes back to the current area, the view centres on it
                      (0, r"dw3-map: centre on the current area: location id=[1-9]\d* \(area \d+\), home mark at "
                          r"sprite \d+,\d+, cursor -?\d+,-?\d+ -> \d+,\d+, view centre"),
                      # the left stick: analog speed, half tilt slower than full
                      (0, r"dw3-map: pan speed source=left stick frame 1: ramp 2\.00 px/frame \(zoom 1\.250, cap 8\.00\) "
                          r"x tilt 0\.[34]\d/0\.00 = -0\.[89]\d/0\.00 px/frame"),
                      (0, r"dw3-map: pan end source=left stick after 60 frames, peak ramp 4\.00 px/frame, "
                          r"cursor 309,227 -> 2\d\d,227 \(moved -[5-9]\d,0\)"),
                      (0, r"dw3-map: pan speed source=left stick frame 1: ramp 2\.00 px/frame \(zoom 1\.250, cap 8\.00\) "
                          r"x tilt 1\.00/0\.00 = -2\.00/0\.00 px/frame"),
                      (0, r"dw3-map: pan end source=left stick after 30 frames, peak ramp 4\.00 px/frame"),
                      # the bilinear scaler (L2 from nearest wraps to it): a GPU filter on the map itself
                      (0, r"dw3-map: scaler BILINEAR built in \d+ ms \(no cache, 392x352\)"),
                      (0, r"dw3-map: texture 392x352 queued for the GPU \(gen \d+, scaler BILINEAR, linear\)"),
                      (0, r"dw3-map: view session: [1-9]\d* zoom changes \([1-9]\d* button, [1-9]\d* stick frames\), "
                          r"1 centre-on-area presses, [1-9]\d* cursor goes"),
                      (0, r"dw3-map: game Map screen back after the close: .*: unchanged"),),
                  forbid_log=((0, r"dw3-map: capture failed|NOTHING DRAWN|area name MISMATCH|"
                                  r"NOT BUILT|\): CHANGED|dw3-turbo: (?:on|off)\n|"
                                  r"dw3-map: cursor on location id=0"),),
                  note="the new full-screen map: zoom about the cursor, the 4.4 ladder, pan limits at every "
                       "zoom, Select centres on the current area, cursor acceleration and analog speed, "
                       "the FIT backdrop, the bilinear scaler (P6.32 phase 3)"))

# P6.32 phase 4 (no patch): the map view's buttons are a binding table (open, zoom out,
# zoom in, centre, scaler previous / next), file-only keys of port_options.cfg, on top
# of the P6.12 / P9.8 button map. The recording uses logical buttons; the Nintendo and
# custom-map variants press positions (DW3_REPLAY_SOURCE=1) and must give the same
# pictures. The `map_screen` row of the Display group is switched on in one process and
# read back, with the binding keys, by the next.
_BIND_CFG = (("map_view_open", "r2"), ("map_view_zoom_out", "square"), ("map_view_zoom_in", "cross"),
             ("map_view_centre", "l3"), ("map_view_scaler_prev", "l1"), ("map_view_scaler_next", "r1"))
_BIND_FILE = (r"dw3-map: bindings read from port_options.cfg: map_view_open=R2 \(file\) "
              r"map_view_zoom_out=Square \(file\) map_view_zoom_in=Cross \(file\) map_view_centre=L3 \(file\) "
              r"map_view_scaler_prev=L1 \(file\) map_view_scaler_next=R1 \(file\)")


def _bind_in_effect(layout, digits, open_, zout, zin, cent, sprev, snext, close, bars):
    return (r"dw3-map: bindings in effect \(at the open\): open=R2\(" + open_ + r"\) zoom_out=Square\(" + zout
            + r"\) zoom_in=Cross\(" + zin + r"\) centre=L3\(" + cent + r"\) scaler_prev=L1\(" + sprev
            + r"\) scaler_next=R1\(" + snext + r"\); fixed: close=Triangle\(" + close
            + r", the game's cancel\) bars=Circle\(" + bars + r"\) R3=ignored; PS button \(console button\) "
            r"under button layout " + layout + r", map " + digits)


_BIND_ANY = _bind_in_effect(r"\w+", r"\d{10}", r"\w+", r"\w+", r"\w+", r"\w+", r"\w+", r"\w+", r"\w+", r"\w+")


def _rebind_expect(bind_line):
    return (
        (0, _BIND_FILE),
        (0, _BIND_ANY),
        # Square (the old open key) opened nothing: the first and only open is R2's
        (0, r"dw3-map: open half=\d image=392x352 .* turbo=1 zoom=1\.50 hud=off"),
        (0, r"dw3-map: L2 pressed in the view: not bound to an action \(zoom out Square, zoom in Cross, "
            r"centre L3, scaler L1/R1, open R2\)"),
        (0, r"dw3-map: Select pressed in the view: not bound to an action \(zoom out Square, zoom in Cross, "
            r"centre L3, scaler L1/R1, open R2\)"),
        # L3 centres; the old centre key (Select) did not, and nothing else did
        (0, r"dw3-map: Select pressed in the view: not bound(?s:.*)dw3-map: centre on the current area: location id="),
        # Square zooms out (1.5 -> 1.25), Cross zooms in (1.25 -> 1.5 -> 2.0), about the cursor
        (0, r"dw3-map: zoom source=button step 2 \(x\) 1\.50x -> 1\.25x anchor=cursor(?s:.*)"
            r"dw3-map: zoom source=button step 3 \(x\) 1\.25x -> 1\.50x anchor=cursor(?s:.*)"
            r"dw3-map: zoom source=button step 4 \(x\) 1\.50x -> 2\.00x anchor=cursor"),
        # L1 is the previous scaler (nearest wraps to bilinear), R1 the next (back to nearest)
        (0, r"dw3-map: scaler BILINEAR built in \d+ ms \(no cache, 392x352\)(?s:.*)dw3-map: scaler NEAREST"),
        (0, r"dw3-map: R2 ignored in the view \(it opens the view; close with the cancel button, PS-side bit 14\)"),
        (0, r"dw3-map: close requested by the cancel button"),
        (0, r"dw3-map: game Map screen back after the close: .*: unchanged"),
        (0, bind_line),
    )


_REBIND_FORBID = ((0, r"dw3-map: BINDING PROBLEM|dw3-map: capture failed|NOTHING DRAWN|\): CHANGED|"
                      r"dw3-map: Square pressed, view not available|"
                      # one open, one centre: the old keys (Square, Select) opened and centred nothing
                      r"dw3-map: open half=\d(?s:.*)dw3-map: open half=\d|"
                      r"dw3-map: centre on the current area(?s:.*)dw3-map: centre on the current area|"
                      r"dw3-map: L1 pressed in the view: not bound|dw3-map: R1 pressed in the view: not bound|"
                      r"dw3-map: Cross pressed in the view: not bound|dw3-map: Square pressed in the view"),)
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158, 0x1F9)),
                 ("eu", (0x167, 0x208, 0x1D1, 0x166, 0x207))):
    _fight, _menu, _report, _field, _status = _ids
    _add(Scenario(f"{_v}-map-rebind", _v, "full", turbo=4, timeout=600,
                  phases=(f"{_v}-map-rebind",), env=_ITEMS_ENV + (("DW3_MAP_SYNC", "1"),),
                  cfg=(("encounter_rate", "2"), ("map_screen", "1")) + _BIND_CFG,
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M), (_status, M)),
                  expect_log=_rebind_expect(_bind_in_effect(
                      "playstation", "0123456789", "ZR", "Y", "B", "StickL", "L", "R", "X", "A")),
                  forbid_log=_REBIND_FORBID,
                  note="the map view with rebound buttons (open R2, zoom out Square, zoom in Cross, centre L3, "
                       "scaler L1 / R1): the old keys do nothing and say so (P6.32 phase 4)"))
    # the file-only keys and the Display row survive a relaunch: process 1 (the title's Options)
    # turns the Full-screen map row on, process 2 (a fresh start, the Map screen) finds the option
    # on and the bindings still in the file the screen rewrote
    _add(Scenario(f"{_v}-map-rebind-relaunch", _v, "full", turbo=4, timeout=600,
                  phases=(f"{_v}-map-rebind-relaunch-p1", f"{_v}-map-rebind-relaunch-p2"),
                  phase_env=((), _ITEMS_ENV + (("DW3_MAP_SYNC", "1"),)),
                  cfg=(("encounter_rate", "2"),) + _BIND_CFG,
                  expect_log=(
                      (0, r"dw3-port-options: group open display \(4 row\(s\), title\)"),
                      (0, r"dw3-port-options: map screen \(full-screen map view\) enabled"),
                      (0, r"dw3-options: saved \S+ \(.*\)"),
                      (1, r"dw3-options: file keys .* map_screen=1 "),
                      (1, _BIND_FILE),
                      (1, _bind_in_effect("playstation", "0123456789", "ZR", "Y", "B", "StickL", "L", "R", "X", "A")),
                      (1, r"dw3-map: open half=\d image=392x352 .* turbo=1 zoom=1\.50 hud=off"),
                      (1, r"dw3-map: zoom source=button step 2 \(x\) 1\.50x -> 1\.25x anchor=cursor"),
                      (1, r"dw3-map: close requested by the cancel button"),
                      (1, r"dw3-map: game Map screen back after the close: .*: unchanged")),
                  forbid_log=((0, r"dw3-port-options: map screen \(full-screen map view\) disabled"),
                              (1, r"dw3-map: BINDING PROBLEM|dw3-map: capture failed|\): CHANGED|"
                                  r"dw3-map: option map_screen=0|dw3-map: Square pressed, view not available")),
                  note="Options > Display > Full-screen map ON in one process; the next start opens the view "
                       "with the rebound keys read back from the same file (P6.32 phase 4)"))
    # the P6.12 hook-up: a custom button map (the Square and R2 positions swapped); the masks are
    # positions. The bindings are logical, so the view behaves as in `map-rebind`, and the log names
    # the console buttons that now make them (open = R2 = the Y position, zoom out = Square = ZR)
    _add(Scenario(f"{_v}-custom-map-rebind", _v, "full", turbo=4, timeout=600,
                  phases=(f"{_v}-custom-map-rebind",), env=_ITEMS_ENV + (("DW3_MAP_SYNC", "1"),) + (("DW3_REPLAY_SOURCE", "1"),),
                  cfg=(("encounter_rate", "2"), ("map_screen", "1"), ("button_layout", "custom"),
                       ("button_map", "0127456389")) + _BIND_CFG,
                  expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M), (_status, M)),
                  expect_log=_rebind_expect(_bind_in_effect(
                      "custom", "0127456389", "Y", "ZR", "B", "StickL", "L", "R", "X", "A"))
                  + ((0, r"dw3-controls: map loaded layout=custom source=cfg"),),
                  forbid_log=_REBIND_FORBID + ((0, r"dw3-controls: map invalid"),),
                  ref_from=f"{_v}-map-rebind",
                  note="the rebound view buttons through a custom button map (P6.12): the positions "
                       "change, the logical bindings and the pictures do not (P6.32 phase 4)"))

# P6.32 phase 6: the full-screen map straight from the walking field (patch 0148). The path of
# `map-rebind` to the idle field; the key goes down there. `field-map-ctl` is the control run
# (no key); `field-map` holds the field for 400 frames under the view and must reproduce the
# control's pictures after the return (ref_map, FH_D frames later), a walk included;
# `field-map-off` has the option off (default): nothing opens and every picture is the control's
# at the same frame; `field-map-square` rebinds the key (map_view_field_open=square): Select does
# nothing, Square opens it, and the pictures are `field-map`'s.
_FH_ENV = _ITEMS_ENV + (("DW3_MAP_SYNC", "1"),)
_FH_AFTER = ("fk-after-return", "fk-after-120", "fk-walk", "fk-end")
_FH_BAD = (r"dw3-map: BINDING PROBLEM|: CHANGED|dw3-turbo: off|dw3-map: capture failed|NOTHING DRAWN|"
           r"dw3-map: field hotkey: the view could not open|overlay restore file=0x(?:1f9|207) ")
_FH_SESSION = (
    (0, r"dw3-map: field hotkey open: key (?:Select\(Minus\)|Square\(Y\)) went down in the walking field; "
        r"field held: frame (\d+) time (\d+) progress 6 battle_steps (-?\d+) play (\d+:\d\d:\d\d\.\d+) rand (\d+) "
        r"player (-?\d+),(-?\d+), (\d+) field task\(s\) paused .*turbo forced to 1x \(was 4x\), R3 ignored"),
    (0, r"dw3-map: field hotkey: the game's Map screen is up in the field menu's slot; the view opens on it"),
    (0, r"dw3-map: open half=0 image=392x352 .* turbo=1 zoom=1\.50 hud=off"),
    (0, r"dw3-turbo: speed 1x(?s:.*)dw3-turbo: speed 4x"),
    (0, r"dw3-map: R3 pressed in the view: ignored"),
    (0, r"dw3-map: close requested by the cancel button"),
    (0, r"dw3-map: field hotkey: the view closed; the game's Map screen is closed with it and the field comes back"),
    # frame, time, progress, battle steps, play time, roll sequence and the player's place are the
    # open's again at the return (the line ends in `unchanged`, never `CHANGED`)
    (0, r"dw3-map: field hotkey return: the field resumes \(session 1\); during the hold the vsync clock ran on "
        r"[1-9]\d*, the frame counter [1-9]\d* and the main loop took [1-9]\d* roll\(s\), all put back, "
        r"[1-9]\d* bytes of VRAM given back; [1-9]\d* field task\(s\) running again; at the open -> now: .*: unchanged"),
)
for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158, 0x1F9)),
                 ("eu", (0x167, 0x208, 0x1D1, 0x166, 0x207))):
    _fight, _menu, _report, _field, _status = _ids
    _fh_restore = ((_fight, M), (_menu, S), (_report, M), (_field, M))
    _fh_cfg = (("encounter_rate", "2"),)
    _add(Scenario(f"{_v}-field-map-ctl", _v, "full", turbo=4, timeout=600,
                  env=_FH_ENV, cfg=_fh_cfg + (("map_screen", "1"),), expect_restore=_fh_restore,
                  forbid_log=((0, r"dw3-map: field hotkey|dw3-map: open half"),),
                  note="control run of the field hotkey: the idle field, no key, the same walk (P6.32 phase 6)"),
         Scenario(f"{_v}-field-map", _v, "full", turbo=4, timeout=600,
                  env=_FH_ENV, cfg=_fh_cfg + (("map_screen", "1"),), expect_restore=_fh_restore,
                  ref_map=tuple((n, f"{_v}-field-map-ctl", n) for n in ("fk-before",) + _FH_AFTER),
                  own_refs=("fk-open", "fk-cursor", "fk-late"),
                  expect_log=_FH_SESSION + (
                      (0, r"dw3-map: field hotkey binding \(at the open, field hotkey\): field_open=Select\(Minus\)"),),
                  forbid_log=((0, _FH_BAD + r"|dw3-map: field hotkey ignored"),),
                  note="Select in the idle field opens the full-screen map, the field held under it (timers, "
                       "encounter steps, actors, rolls), R3 does nothing, Triangle returns to the exact field "
                       "(P6.32 phase 6)"),
         Scenario(f"{_v}-field-map-off", _v, "full", turbo=4, timeout=600,
                  env=_FH_ENV, cfg=_fh_cfg, expect_restore=_fh_restore,
                  ref_map=tuple((n, f"{_v}-field-map-ctl", n) for n in ("fk-before", "fk-off-60", "fk-off-160")),
                  expect_log=((0, r"dw3-map: field hotkey ignored: Options > Display > Full-screen map is OFF "
                                  r"\(map_screen=0\) \(key Select = map_view_field_open\)(?s:.*)"
                                  r"dw3-map: field hotkey ignored: Options > Display > Full-screen map is OFF"),),
                  forbid_log=((0, r"dw3-map: field hotkey open|dw3-map: open half|dw3-map: BINDING PROBLEM"),),
                  note="the option off (the default): the key does nothing and the log says why, twice (P6.32 phase 6)"),
         Scenario(f"{_v}-field-map-43", _v, "full", turbo=4, timeout=600, log_only=True,
                  env=_FH_ENV + (("DW3_WIDESCREEN", "0"),), cfg=_fh_cfg + (("map_screen", "1"),),
                  expect_restore=_fh_restore,
                  expect_log=((0, r"dw3-map: field hotkey ignored: widescreen is off \(the view needs the 16:9 picture, "
                                  r"shift=0\) \(key Select = map_view_field_open\)(?s:.*)"
                                  r"dw3-map: field hotkey ignored: widescreen is off"),),
                  forbid_log=((0, r"dw3-map: field hotkey open|dw3-map: open half|dw3-map: BINDING PROBLEM"),),
                  note="widescreen off: the key does nothing and the log says why, twice (P6.32 phase 6)"),
         Scenario(f"{_v}-field-map-square", _v, "full", turbo=4, timeout=600,
                  env=_FH_ENV, cfg=_fh_cfg + (("map_screen", "1"), ("map_view_field_open", "square")),
                  expect_restore=_fh_restore,
                  ref_from=f"{_v}-field-map",
                  ref_map=tuple((n, f"{_v}-field-map-ctl", n) for n in ("fk-before",) + _FH_AFTER),
                  expect_log=_FH_SESSION + (
                      (0, r"dw3-map: bindings read from port_options.cfg:.* map_view_field_open=Square \(file\)"),
                      (0, r"dw3-map: field hotkey binding \(at the open, field hotkey\): field_open=Square\(Y\)")),
                  forbid_log=((0, _FH_BAD + r"|dw3-map: field hotkey ignored|"
                                  # one open: Select, the old key, pressed before it opened nothing
                                  r"dw3-map: field hotkey open(?s:.*)dw3-map: field hotkey open"),),
                  note="map_view_field_open=square: Square opens the view from the field, with the same pictures "
                       "as Select (P6.32 phase 6)"))

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

# P6.25 slice 2: the same game saves, but through the shared per-profile store
# (`save_store=1` in the seeded cfg; DW3_SAVE_STORE_DIR is the folder both
# regions share). The write, the container and the open on the other disc are
# the verdicts: `dw3-store:` lines, the load that follows and the field reached.
# No cross-import line (slice 1 is not used while the store serves the port) and
# no flat saveN.bin written. Log verdicts only; PC only (env).
_ST_CFG = (("save_store", "1"),)
_ST_WRITE = (0, r"dw3-store: write card0 by %s slots_changed=100 header=1")
_ST_COMMIT = (0, r"dw3-save: write card0\.dw3s committed")
_ST_BAD = ((1, r"STGMCARD_loadSave (?:checksum|version|read) fail"),
           (0, r"dw3-store: (?:REFUSED|.*FAILED|card\d\.dw3s refused)"),
           (1, r"dw3-store: (?:REFUSED|.*FAILED|card\d\.dw3s refused)"),
           (0, r"cross-import slot"), (1, r"cross-import slot"),
           (0, r"write save0\.bin committed"), (1, r"write save0\.bin committed"))
for _a, _b in (("us", "eu"), ("eu", "us")):
    _add(Scenario(f"xr-store-{_a}-to-{_b}", _a, "full", timeout=600, log_only=True,
                  phases=(f"{_a}-xr-save", f"{_b}-xr-load"), phase_versions=(_a, _b),
                  phase_env=(_SAVE_ENV, ()), cfg=_ST_CFG, store=True,
                  expect_restore=((_XR_FIELD[_a], M), (_XR_MCARD[_a], M),
                                  (_XR_MCARD[_b], M), (_XR_FIELD[_b], M)),
                  expect_log=((_ST_WRITE[0], _ST_WRITE[1] % _a), _ST_COMMIT,
                              (1, r"dw3-store: open card0 as %s results=%d,0,0"
                                  % (_b, 1 if _b == "eu" else 2)),
                              (1, r"STGMCARD_loadSave ok port=0 slot=0 money=\d+ progress=6"
                                  r"(?s:.*)FIELDSTG_pickStage mode=0x233")),
                  forbid_log=_ST_BAD,
                  note=f"P6.25 slice 2: saved on the {_a.upper()} disc into the shared store, "
                       f"opened and loaded on the {_b.upper()} disc (no cross-import, no saveN.bin)"))
# Two-way: the other region opens the store and the first one reads it again,
# so a session that only opens a card changes nothing the first region needs.
for _a, _b in (("us", "eu"), ("eu", "us")):
    _add(Scenario(f"xr-store-{_a}-{_b}-{_a}", _a, "full", timeout=900, log_only=True,
                  phases=(f"{_a}-xr-save", f"{_b}-xr-load", f"{_a}-xr-load"),
                  phase_versions=(_a, _b, _a),
                  phase_env=(_SAVE_ENV, (), ()), cfg=_ST_CFG, store=True,
                  expect_restore=((_XR_FIELD[_a], M), (_XR_MCARD[_a], M),
                                  (_XR_MCARD[_b], M), (_XR_FIELD[_b], M)),
                  expect_log=((_ST_WRITE[0], _ST_WRITE[1] % _a), _ST_COMMIT,
                              (1, r"STGMCARD_loadSave ok port=0 slot=0 money=\d+ progress=6"),
                              (2, r"dw3-store: open card0 as %s results=%d,0,0" % (_a, 1 if _a == "eu" else 2)),
                              (2, r"STGMCARD_loadSave ok port=0 slot=0 money=\d+ progress=6"
                                  r"(?s:.*)FIELDSTG_pickStage mode=0x233")),
                  forbid_log=_ST_BAD + ((2, r"STGMCARD_loadSave (?:checksum|version|read) fail"),
                                        (2, r"dw3-store: (?:REFUSED|.*FAILED)"),
                                        (1, r"dw3-store: write card0"), (2, r"dw3-store: write card0")),
                  note=f"P6.25 slice 2: {_a.upper()} saves, {_b.upper()} loads from the store, {_a.upper()} loads it "
                       f"again (opening never writes the card)"))
# The opt-in works against a legacy flat save: phase 0 saves with the store off
# (DW3_SAVE_STORE=0 beats the seeded cfg), so the card is the flat save0.bin;
# phase 1 runs the other disc with the store on: the first start copies it in
# (migration, the original is only read) and loads it from the store.
for _a, _b in (("us", "eu"), ("eu", "us")):
    _add(Scenario(f"xr-store-migrate-{_a}-{_b}", _a, "full", timeout=600, log_only=True,
                  phases=(f"{_a}-xr-save", f"{_b}-xr-load"), phase_versions=(_a, _b),
                  phase_env=(_SAVE_ENV + (("DW3_SAVE_STORE", "0"),), ()),
                  cfg=_ST_CFG, store=True,
                  expect_restore=((_XR_FIELD[_a], M), (_XR_MCARD[_a], M),
                                  (_XR_MCARD[_b], M), (_XR_FIELD[_b], M)),
                  expect_log=(_XR_WRITE,
                              (1, r"dw3-store: migrate \S+save0\.bin slot=0 \(%s, name \w+\.\.\.\): imported -> card0 slot 0" % _a),
                              (1, r"dw3-store: migration done imported=1 duplicate=0 refused=0 left_in_legacy=0"),
                              (1, r"dw3-store: open card0 as %s results=%d,0,0"
                                  % (_b, 1 if _b == "eu" else 2)),
                              (1, r"STGMCARD_loadSave ok port=0 slot=0 money=\d+ progress=6"
                                  r"(?s:.*)FIELDSTG_pickStage mode=0x233")),
                  forbid_log=((0, r"dw3-store"), (1, r"STGMCARD_loadSave (?:checksum|version|read) fail"),
                              (1, r"dw3-store: (?:REFUSED|.*FAILED)"), (1, r"cross-import slot"),
                              (1, r"write save0\.bin committed")),
                  note=f"P6.25 slice 2: a flat save made on the {_a.upper()} disc with the store off is copied into "
                       f"the store on the first start with it on, and loads on the {_b.upper()} disc"))
# Refusals through the store, made from the game's own save by a run-time edit of the
# canonical card (phase_mutate; the container CRC is redone): an EU save past the base game
# (progress 0x2D) is shown to the US build with its bytes kept, so the US game shows its
# own error 8 and never loads it, and the EU disc still plays it afterwards; a slot with a
# wrong checksum is not loaded on either disc. No crash. (After the US disc the EU disc
# only opens the cards, xr-open: its game cannot play a synthetic progress-0x2D save out of
# the extra chapter's story, it hangs in the field; the unit test checks the slot byte for
# byte and the open line shows the card is whole.)
_add(Scenario("xr-store-eu-extra-us", "eu", "full", timeout=900, log_only=True,
              phases=("eu-xr-save", "us-xr-load", "eu-xr-open"), phase_versions=("eu", "us", "eu"),
              phase_env=(_SAVE_ENV, (), ()), phase_mutate=("progress:0x2D", "", ""),
              cfg=_ST_CFG, store=True,
              expect_restore=((_XR_MCARD["eu"], M), (_XR_MCARD["us"], M)),
              expect_log=((0, r"dw3-store: write card0 by eu slots_changed=100 header=1"),
                          (1, r"dw3-store: open card0 as us results=3,0,0"),
                          (1, r"dw3-store: card0 slot 0 needs the EU extra chapter"),
                          (1, r"STGMCARD_loadSave version fail got=4"),
                          (2, r"dw3-store: open card0 as eu results=1,0,0")),
              forbid_log=((1, r"STGMCARD_loadSave ok"), (1, r"overlay restore file=0x%x kind=mode" % _XR_FIELD["us"]),
                          (1, r"dw3-store: write card0"), (2, r"dw3-store: write card0"),
                          (1, r"dw3-store: (?:REFUSED|.*FAILED)"), (2, r"dw3-store: (?:REFUSED|.*FAILED)"),
                          (2, r"STGMCARD_loadSave (?:checksum|version|read) fail")),
              note="P6.25 slice 2: an EU extra-chapter save is the game's own error 8 on the US disc (not loaded, "
                   "not written); the EU disc opens the same card whole"))
_add(Scenario("xr-store-us-badsum-eu", "us", "full", timeout=600, log_only=True,
              phases=("us-xr-save", "eu-xr-load"), phase_versions=("us", "eu"),
              phase_env=(_SAVE_ENV, ()), phase_mutate=("badsum", ""),
              cfg=_ST_CFG, store=True,
              expect_restore=((_XR_MCARD["us"], M), (_XR_MCARD["eu"], M)),
              expect_log=((0, r"dw3-store: write card0 by us slots_changed=100 header=1"),
                          (1, r"dw3-store: open card0 as eu results=4,0,0"),
                          (1, r"STGMCARD_loadSave checksum fail")),
              forbid_log=((1, r"STGMCARD_loadSave ok"), (1, r"overlay restore file=0x%x kind=mode" % _XR_FIELD["eu"]),
                          (1, r"dw3-store: write card0"), (1, r"dw3-store: (?:REFUSED|.*FAILED)")),
              note="P6.25 slice 2: a store slot with a wrong checksum is not loaded (the game's own error), "
                   "nothing is rewritten"))


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

# Stage 0x236 (WSTAG460, the Reliability Spot) had no entry in the host's
# overlay table (US file 0x355, EU 0x364) and its fourth wanderer wrote through
# a NULL tile (patch 0197): entering it aborted (replay) or crashed. Boot in the
# stage before it (0x235, WSTAG455) at progress 14 and leave for 0x236 as event
# 360 does (DW3_FORCE_EXIT, patch 0110), no input.
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-stage-0x236", _v, "full", timeout=300,
                  cfg=(("widescreen", "1"),),
                  env=(("DW3_START_MODE", "0x235"), ("DW3_PROGRESS", "14"),
                       ("DW3_FORCE_EXIT", "0x236:214:132:1:100"),
                       ("DW3_WALKTHROUGH_PROBE", "1")),
                  expect_log=((0, r"DW3_FORCE_EXIT leaveField mode=0x236 x=214 y=132 dir=1(?s:.*)"
                                  r"FIELDSTG_pickStage mode=0x236 stageFile=(?:853|868)"),),
                  forbid_log=((0, r"overlay restore failed|FATAL"),),
                  note="the Reliability Spot loads from the stage before it (overlay table entry, patch 0197)"))

# The EU extra chapter's 55 stages (FIELDSTG_extraStages, WSTAG920 to WSTAG974,
# picked while GAME.progress is 0x2D, STORY_EXTRA) had no overlay table
# entry either. Boot four of them (the first, the Digimon Lab whose colour
# lives in wstag924_head.c, one in the middle, the last) at progress 0x2D and
# idle: strict restore, the stage file the extra table names, 600 frames. All
# 55 were booted by hand the same way (docs/P5.5_STAGE_SWEEP.md). EU only.
for _num, _mode, _file in ((920, 0x270, 1851), (924, 0x275, 2221), (949, 0x295, 2246), (974, 0x2EE, 2271)):
    _add(Scenario(f"eu-extra-stage-{_num}", "eu", "full", turbo=4, timeout=120, log_only=True,
                  phases=("eu-walkthrough",),
                  env=(("DW3_START_MODE", f"0x{_mode:X}"), ("DW3_PROGRESS", "45")),
                  expect_log=((0, rf"FIELDSTG_pickStage mode=0x{_mode:x} stageFile={_file}(?s:.*)"
                                  rf"overlay restore file=0x{_file:x} kind=sub"),),
                  forbid_log=((0, r"overlay restore failed|FATAL"),),
                  note=f"EU extra chapter: WSTAG{_num} (mode 0x{_mode:X}) loads at progress 0x2D"))

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
                                  r"dw3-port-restart: fade finished"), (0, _READY)),
                  forbid_log=((0, r"save flush failed"), (0, _READY_BAD)),
                  note="field menu Return to title: No keeps the menu, Yes fades to black, runs the P6.9 reset on black and fades the title in (P6.27); the 4:3 gutter switch falls inside the black (US; EU stays 16:9 since patch 0177)"),
         Scenario(f"{_v}-return-title-rows7", _v, "full", turbo=4, timeout=600,
                  env=_ITEMS_ENV, cfg=_RT_CFG,
                  expect_log=((0, r"dw3-port-confirm: open return to title \(cursor on No\)(?s:.*)"
                                  r"dw3-port-confirm: cancelled \(return to title\); nothing changed"),),
                  forbid_log=((0, r"dw3-port-restart"),),
                  note="the field menu with the card folders row and Return to title: seven rows, cancelled (P6.27)"))

for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-title-ready", _v, "full", timeout=300, log_only=True,
                  expect_log=((0, _READY), (0, _READY_ACC)),
                  forbid_log=((0, _READY_BAD),),
                  note="PRESS START is drawn on the frame the pad is read, never before (patch 0199); Start pressed every other frame from boot"))

# P9.7 slice 1: the port Options screen from the field menu's Options row
# (patch 0160, port/options/field_options.c). Phase 1: Widescreen refuses a
# change (locked in game), Music volume 10 -> 7 and Encounter rate 1x -> OFF
# apply at once and are saved, the screen closes and the menu and the field go
# on; phase 2 (a new process on the same config dir) shows the values read back.
_FO_P1 = (r"dw3-port-options: open in game(?s:.*)"
          r"dw3-port-options: group open display \(4 row\(s\), in game\)(?s:.*)"
          r"dw3-port-options: Widescreen is locked in game \(change it at the title\)(?s:.*)"
          r"dw3-port-options: group open sound \(3 row\(s\), in game\)(?s:.*)"
          r"dw3-port-options: music volume 7(?s:.*)"
          r"dw3-port-options: group open gameplay \(4 row\(s\), in game\)(?s:.*)"
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
# P6.38 restart into the chosen region (docs/PORT_OPTIONS.md, port/options/
# region_relaunch.c). Combined binary only (tier `both`); `base` puts both regions'
# data on the "card" so the other region counts as present. no: the window shows
# (title: no progress warning), No keeps "takes effect at the next start", Ask and the
# running region never ask. yes: Yes writes the choice and asks the host to relaunch;
# the PC host's test hook (DW3_RELAUNCH_TEST) logs it and exits 0 instead of
# exec'ing, because the replay harness cannot follow an exec. exec: the same without
# the hook: the process execs itself into the other region and the second run (the
# same recording from frame 0, written to be harmless there) must end normally.
# game: from the field menu's Options row, with the progress warning.
for _v in ("us", "eu"):
    _o, _O = ("eu", "EU") if _v == "us" else ("us", "US")
    _no_logs = ((0, rf"dw3-region: restart prompt for {_o} \(title\)(?s:.*)"
                    rf"dw3-port-confirm: open restart in {_O} \(title, cursor on No\)(?s:.*)"
                    rf"dw3-port-confirm: No \(restart in region\); nothing changed(?s:.*)"
                    rf"dw3-region: relaunch declined; {_o} takes effect at the next start"),
                (0, r"dw3-region: region_default ask: no restart prompt"),
                (0, rf"dw3-region: region_default {_v} is the running region: no restart prompt"),
                (0, rf"dw3-options: region_default {_o} saved to \S+port_options\.cfg"))
    _add(Scenario(f"{_v}-options-region-relaunch-no", _v, "both", timeout=300, base="us+eu",
                  expect_log=_no_logs,
                  forbid_log=((0, r"relaunch to \w+ requested"), (0, r"NOT saved"),
                              (0, r"dw3-region: no restart prompt for")),
                  note="Region at start: the restart window after a change to the other region; No keeps the next-start behaviour; Ask and the running region never ask (P6.38, --bin-both)"),
         Scenario(f"{_v}-options-region-relaunch-yes", _v, "both", timeout=300, base="us+eu", quits=True,
                  env=(("DW3_RELAUNCH_TEST", "1"),),
                  expect_log=((0, rf"dw3-port-confirm: cursor on Yes(?s:.*)"
                                  rf"dw3-port-confirm: Yes \(restart in region\)(?s:.*)"
                                  rf"dw3-region: relaunch to {_o} requested(?s:.*)"
                                  rf"dw3-region: relaunch to {_o}: leaving the game now \(execv \S+ with --relaunch-region {_o}\)(?s:.*)"
                                  rf"dw3-region: relaunch to {_o}: DW3_RELAUNCH_TEST set, not starting the new process; exiting"),
                              (0, rf"dw3-options: region_default {_o} saved to \S+port_options\.cfg")),
                  forbid_log=((0, r"relaunch to \w+ failed"), (0, r"save flush failed")),
                  note="Region at start: Yes saves, flushes and asks the host to relaunch; the test hook stops short of the exec and exits 0 (P6.38, --bin-both)"),
         Scenario(f"{_v}-options-region-relaunch-exec", _v, "both", timeout=300, base="us+eu", log_only=True,
                  cfg=(("region_default", _v),),
                  expect_log=((0, rf"dw3-region: restart prompt for {_o} \(title\)(?s:.*)"
                                  rf"dw3-region: relaunch to {_o} requested(?s:.*)"
                                  rf"dw3-region: relaunch to {_o}: starting \S+ again(?s:.*)"
                                  rf"dw3-region: started by relaunch \({_o}\)(?s:.*)"
                                  rf"dw3-region: running {_o} \(flag\)(?s:.*)"
                                  rf"dw3-pc: region={_o}(?s:.*)"
                                  rf"dw3-replay: end at frame"),),
                  forbid_log=((0, r"relaunch to \w+ failed"), (0, r"DW3_RELAUNCH_TEST"),
                              (0, rf"dw3-region: running {_o} \(env\)")),
                  note="Region at start: Yes execs the program again, the new process boots the other region (flag, not DW3_REGION) and finishes the recording (P6.38, --bin-both, real exec)"))

# P6.38 in game: the same window from the field menu's Options row; the title's has no
# progress warning, this one has it (the env is the field-options scenario's).
for _v in ("us", "eu"):
    _o, _O = ("eu", "EU") if _v == "us" else ("us", "US")
    _add(Scenario(f"{_v}-options-region-relaunch-game", _v, "both", turbo=4, timeout=600, base="us+eu",
                  env=_RT_ENV,
                  expect_log=((0, rf"dw3-port-options: open in game(?s:.*)"
                                  rf"dw3-region: restart prompt for {_o} \(in game\)(?s:.*)"
                                  rf"dw3-port-confirm: open restart in {_O} \(in game, cursor on No\)(?s:.*)"
                                  rf"dw3-region: relaunch declined; {_o} takes effect at the next start"),),
                  forbid_log=((0, r"relaunch to \w+ requested"), (0, r"save flush failed")),
                  note="Region at start in game: the restart window carries the progress warning; No goes on (P6.38, --bin-both)"))

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
# P6.36: the same recording with Turbo music on 1x (see the field and battle scenarios
# above): the music cadence follows the game rate, 1x in real time.
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-turbo-music-menu", _v, "full", turbo=4, timeout=600, log_only=True,
                  phases=(f"{_v}-turbo-menu",),
                  env=_RT_ENV + _TM_ENV,
                  expect_log=((0, r"music tick source=frames rate=(?:15|12\.5)/s game_rate=4x(?s:.*)"
                                  r"music tick source=frames rate=(?:40|33\.3)/s game_rate=1\.5x(?s:.*)"
                                  r"dw3-turbo: menu 1\.5x \(speed 4x\)(?s:.*)"
                                  r"dw3-turbo: menu off \(frames=\d+\)(?s:.*)"
                                  r"music tick source=frames rate=(?:15|12\.5)/s game_rate=4x"),),
                  note="turbo 4, field menu at 1.5x: the music cadence follows the game rate (ticks per 60 frames 15 -> 40 -> 15), i.e. 1x in real time (P6.36)"))
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
                                  r"dw3-port-options: group open sound \(3 row\(s\), in game\)(?s:.*)"
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
                                  r"dw3-port-options: group open sound \(3 row\(s\), in game\)(?s:.*)"
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
              r"dw3-help: page hotkeys \(from next, \d+ lines, panel 13 lines, unverified blocks 1\)(?s:.*)"
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
# The base game's other languages (machine-drafted text, needs native review):
# the help is in the language, with no English fallback line.
for _lang in ("fr", "de", "it", "es"):
    _add(Scenario(f"eu-help-{_lang}", "eu", "full", timeout=300,
                  expect_log=((0, r"dw3-help: open \(from title, region EU, platform pc, language " + _lang +
                                  r", 10 pages\)"),
                              (0, r"dw3-pages: text help " + _lang + r" loaded \(\d+ keys, \d+ qualified, "
                                  r"\d+ filtered out, \d+ bytes\)"),
                              (0, r"dw3-help: page controls \(from list,(?s:.*)dw3-help: page options \(from list,")),
                  forbid_log=((0, r"unresolved"), (0, r"dw3-pages: text fallback"),
                              (0, r"cannot be (opened|allocated|laid out)")),
                  note=f"technical help with the EU disc set to {_lang}: the machine-drafted translation "
                       "(page list, Controls, Options explained), no English fallback (P6.30, Q3)"))

_RT_BATTLE_ENV = _RT_ENV[2:]   # the forced event battle: NPC action, encounter, party, win (no mode, no progress)
# P6.35 phase 2 (docs/P6.35_WALKTHROUGH.md "As built, phase 2"): the Walkthrough
# group of the Options screen. In game it opens on the page of the current step,
# read from the real game state at a forced story point (env DW3_START_MODE /
# DW3_PROGRESS plus the forced event battle that gives the player a party); from
# the title it is a browse-only list. The game's own field menu opens from
# progress 4 (field.c openFieldMenu), so the in-game points are 4 (Seiryu City,
# step ch01.s06) and 6 (past the table: "more chapters coming"). The proof is
# the `dw3-walkthrough:` lines (AGENTS.md rule 12) and the pictures.
_WT_FORBID = ((0, r"unresolved"), (0, r"dw3-walkthrough: text fallback"),
              (0, r"title too wide|cannot be (opened|laid out|loaded)"),
              (0, r"save flush failed"), (0, r"sound \w+ not played"))
_WT_CLOSE = r"dw3-port-options: group close walkthrough \(back to the group list\)"


# chapters, pages and steps per disc (the EU extra chapter has 3 steps more, each on a page of its own,
# and a chapter of its own). Since the condensing (FEEDBACK-5) consecutive short steps of one area share
# a page: 56 steps are 30 pages, the EU disc's 59 are 33.
_WT_COUNT = {"US": (11, 30, 56), "EU": (12, 33, 59)}


def _wt_open_re(reg):
    """The `open (...)` log line's tail: language en, chapters, pages and steps of the disc."""
    chapters, pages, steps = _WT_COUNT[reg]
    return rf", platform pc, language en, {chapters} chapters, {pages} pages, {steps} steps\)(?s:.*)"


def _wt_title(lang, reg):
    """expect_log of the browse-only run from the title (chapter 1 opened, first step shown,
    second hidden)."""
    chapters, pages, steps = _WT_COUNT[reg]
    return ((0, r"dw3-walkthrough: open \(from title, region " + reg + r", platform pc, language " + lang +
                rf", {chapters} chapters, {pages} pages, {steps} steps\)(?s:.*)"
                r"dw3-walkthrough: browse mode \(no game state\)(?s:.*)"
                r"dw3-port-options: group open walkthrough \(title\)(?s:.*)"
                r"dw3-walkthrough: first frame composed [\d.]+ ms after open(?s:.*)"
                r"dw3-walkthrough: chapter ch01 opened \(reached y, current n, 4 pages, 6 steps\)(?s:.*)"
                r"dw3-walkthrough: page ch01\.s01 \(kind MAIN, from list, reached y, current n, \d+ lines(?s:.*)"
                r"dw3-walkthrough: spoiler ch01\.s01 closed \(answer available\)(?s:.*)"
                r"dw3-walkthrough: spoiler ch01\.s01 asked(?s:.*)"
                r"dw3-walkthrough: spoiler ch01\.s01 shown(?s:.*)"
                r"dw3-walkthrough: page ch01\.s01 closed after \d+ ms \(spoiler shown y\)(?s:.*)"
                r"dw3-walkthrough: page ch01\.s02 \(kind MAIN, from list, reached n, current n, \d+ lines(?s:.*)"
                r"dw3-walkthrough: page ch01\.s02 closed after \d+ ms \(spoiler shown n\)(?s:.*)"
                r"dw3-walkthrough: closed after \d+ ms \(pages opened 2, spoilers shown 1\)(?s:.*)" + _WT_CLOSE),
            (0, r"dw3-port-options: close \(resume runOptions\)"))


for _v, _reg in (("us", "US"), ("eu", "EU")):
    _add(Scenario(f"{_v}-walkthrough-screen-p4", _v, "full", turbo=4, timeout=300,
                  env=(("DW3_START_MODE", "0x22E"), ("DW3_PROGRESS", "4")) + _RT_BATTLE_ENV,
                  expect_log=((0, r"dw3-port-options: open in game(?s:.*)"
                                  r"dw3-walkthrough: open \(from field, region " + _reg + _wt_open_re(_reg) +
                                  r"dw3-walkthrough: progress 4 half 0 area 14 -> chapter ch01 step ch01\.s06 "
                                  r"\(rule progress\) \[open mode 0x22e\](?s:.*)"
                                  r"dw3-walkthrough: flags read 0x400d=0(?s:.*)"
                                  r"dw3-walkthrough: page ch01\.s06 \(kind MAIN, from open, reached y, current y, "
                                  r"\d+ lines(?s:.*)"
                                  r"dw3-walkthrough: spoiler ch01\.s06 closed \(answer available\)(?s:.*)"
                                  r"dw3-port-options: group open walkthrough \(in game\)(?s:.*)"
                                  r"dw3-walkthrough: first frame composed [\d.]+ ms after open(?s:.*)"
                                  r"dw3-walkthrough: spoiler ch01\.s06 asked(?s:.*)"
                                  r"dw3-walkthrough: spoiler ch01\.s06 shown(?s:.*)"
                                  r"dw3-walkthrough: page ch01\.s06 closed after \d+ ms \(spoiler shown y\)(?s:.*)"
                                  r"dw3-walkthrough: page ch01\.x01 \(kind SIDE, from list, reached y, current n(?s:.*)"
                                  r"dw3-walkthrough: spoiler ch01\.x01 asked(?s:.*)"
                                  r"dw3-walkthrough: spoiler ch01\.x01 cancelled(?s:.*)"
                                  r"dw3-walkthrough: page ch01\.x01 closed after \d+ ms \(spoiler shown n\)(?s:.*)"
                                  r"dw3-walkthrough: closed after \d+ ms \(pages opened 2, spoilers shown 1\)(?s:.*)"
                                  + _WT_CLOSE + r"(?s:.*)dw3-port-options: close in game \(back to the field menu\)"),),
                  forbid_log=_WT_FORBID + ((0, r"widescreen (enabled|disabled)"),
                                           (0, r"step not recognised|browse mode"),),
                  note="P6.35 phase 2: the walkthrough from the field menu at progress 4 opens on the "
                       "current step (ch01.s06), the answer needs two actions, the next entry is the "
                       "optional row, Back goes up through the chapter's steps and the chapter list"))
    _add(Scenario(f"{_v}-walkthrough-screen-p14", _v, "full", turbo=4, timeout=300,
                  env=(("DW3_START_MODE", "0x255"), ("DW3_PROGRESS", "20")) + _RT_BATTLE_ENV,
                  expect_log=((0, r"dw3-port-options: open in game(?s:.*)"
                                  r"dw3-walkthrough: open \(from field, region " + _reg + _wt_open_re(_reg) +
                                  r"dw3-walkthrough: progress 20 half 0 area 7 -> chapter ch05 step ch05\.s03 "
                                  r"\(rule progress\+flag\) \[open mode 0x255\](?s:.*)"
                                  r"dw3-walkthrough: flags read 0x400d=0(?s:.*)"
                                  r"dw3-walkthrough: page ch05\.s01 \(kind MAIN, from open, reached y, current y, "
                                  r"\d+ lines(?s:.*)"
                                  r"dw3-walkthrough: spoiler ch05\.s01 closed \(answer available\)(?s:.*)"
                                  r"dw3-port-options: group open walkthrough \(in game\)(?s:.*)"
                                  r"dw3-walkthrough: first frame composed [\d.]+ ms after open(?s:.*)"
                                  r"dw3-walkthrough: spoiler ch05\.s01 asked(?s:.*)"
                                  r"dw3-walkthrough: spoiler ch05\.s01 shown(?s:.*)"
                                  r"dw3-walkthrough: page ch05\.s01 closed after \d+ ms \(spoiler shown y\)(?s:.*)"
                                  r"dw3-walkthrough: page ch05\.s05 \(kind MAIN, from list, reached n, current n(?s:.*)"
                                  r"dw3-walkthrough: spoiler ch05\.s05 asked(?s:.*)"
                                  r"dw3-walkthrough: spoiler ch05\.s05 cancelled(?s:.*)"
                                  r"dw3-walkthrough: page ch05\.s05 closed after \d+ ms \(spoiler shown n\)(?s:.*)"
                                  r"dw3-walkthrough: closed after \d+ ms \(pages opened 2, spoilers shown 1\)(?s:.*)"
                                  + _WT_CLOSE + r"(?s:.*)dw3-port-options: close in game \(back to the field menu\)"),),
                  forbid_log=_WT_FORBID + ((0, r"widescreen (enabled|disabled)"),
                                           (0, r"step not recognised|browse mode"),),
                  note="P6.35 slice 2: the walkthrough in the middle of the first half (progress 0x14, "
                       "chapter 5) opens on the page of its chapter that holds the current step (ch05.s03 is on the page "
                       "ch05.s01); the next page is not reached "
                       "yet, so its title is hidden and the reveal says it may spoil; Back shows the "
                       "chapter list with the arrow on chapter 5"))
    _add(Scenario(f"{_v}-walkthrough-screen-p2b", _v, "full", turbo=4, timeout=300,
                  env=(("DW3_START_MODE", "0x2DF"), ("DW3_PROGRESS", "43")) + _RT_BATTLE_ENV,
                  expect_log=((0, r"dw3-port-options: open in game(?s:.*)"
                                  r"dw3-walkthrough: open \(from field, region " + _reg + _wt_open_re(_reg) +
                                  r"dw3-walkthrough: progress 43 half -1 area -1 -> chapter ch11 step ch11\.s01 "
                                  r"\(rule progress\) \[open mode 0x2df\](?s:.*)"
                                  r"dw3-walkthrough: page ch11\.s01 \(kind MAIN, from open, reached y, current y, "
                                  r"\d+ lines(?s:.*)"
                                  r"dw3-walkthrough: spoiler ch11\.s01 closed \(answer available\)(?s:.*)"
                                  r"dw3-port-options: group open walkthrough \(in game\)(?s:.*)"
                                  r"dw3-walkthrough: spoiler ch11\.s01 asked(?s:.*)"
                                  r"dw3-walkthrough: spoiler ch11\.s01 shown(?s:.*)"
                                  r"dw3-walkthrough: page ch11\.s01 closed after \d+ ms \(spoiler shown y\)(?s:.*)"
                                  r"dw3-walkthrough: page area\.network \(kind AREA, from list, reached y, "
                                  r"current n(?s:.*)"
                                  r"dw3-walkthrough: spoiler area\.network asked(?s:.*)"
                                  r"dw3-walkthrough: spoiler area\.network cancelled(?s:.*)"
                                  r"dw3-walkthrough: closed after \d+ ms \(pages opened 2, spoilers shown 1\)(?s:.*)"
                                  + _WT_CLOSE + r"(?s:.*)dw3-port-options: close in game \(back to the field menu\)"),),
                  forbid_log=_WT_FORBID + ((0, r"widescreen (enabled|disabled)"),
                                           (0, r"step not recognised|browse mode"),),
                  note="P6.35 slice 3: the walkthrough at the last battle (progress 0x2B, both discs) opens "
                       "on the only step of chapter 11; the chapter list marks it, and on EU chapter 12 is "
                       "not reached yet"))
    _add(Scenario(f"{_v}-walkthrough-screen-title", _v, "full", timeout=300,
                  expect_log=_wt_title("en", _reg), forbid_log=_WT_FORBID + ((0, r"step not recognised"),),
                  note="P6.35 phase 2: the walkthrough from the title is browse-only: the first step "
                       "has its text, the second keeps its title hidden"))
_add(Scenario("eu-walkthrough-screen-title-fr", "eu", "full", timeout=300,
              expect_log=_wt_title("fr", "EU"), forbid_log=_WT_FORBID + ((0, r"step not recognised"),),
              note="P6.35 phase 2: the walkthrough from the title in EU French (machine-drafted text, "
                   "needs native review), no English fallback"))
_add(Scenario("eu-walkthrough-screen-p2d", "eu", "full", turbo=4, timeout=300,
              env=(("DW3_START_MODE", "0x29C"), ("DW3_PROGRESS", "45")) + _RT_BATTLE_ENV,
              expect_log=((0, r"dw3-port-options: open in game(?s:.*)"
                              r"dw3-walkthrough: open \(from field, region EU" + _wt_open_re("EU") +
                              r"dw3-walkthrough: progress 45 half 1 area 14 -> chapter ch12 step ch12\.s01 "
                              r"\(rule progress\+flag\) \[open mode 0x29c\](?s:.*)"
                              r"dw3-walkthrough: page ch12\.s01 \(kind MAIN, from open, reached y, current y, "
                              r"\d+ lines(?s:.*)"
                              r"dw3-walkthrough: spoiler ch12\.s01 asked(?s:.*)"
                              r"dw3-walkthrough: spoiler ch12\.s01 shown(?s:.*)"
                              r"dw3-walkthrough: page ch12\.s02 \(kind MAIN, from list, reached n, current n(?s:.*)"
                              r"dw3-walkthrough: spoiler ch12\.s02 asked(?s:.*)"
                              r"dw3-walkthrough: spoiler ch12\.s02 cancelled(?s:.*)"
                              r"dw3-walkthrough: closed after \d+ ms \(pages opened 2, spoilers shown 1\)(?s:.*)"
                              + _WT_CLOSE + r"(?s:.*)dw3-port-options: close in game \(back to the field menu\)"),),
              forbid_log=_WT_FORBID + ((0, r"widescreen (enabled|disabled)"),
                                       (0, r"step not recognised|browse mode"),),
              note="P6.35 slice 3: the EU extra chapter's Qing Long City at progress 0x2D (the Login Room's Tour Leader scene "
                   "blocks the field menu): the walkthrough opens "
                   "on ch12.s01, the next step is not reached yet (title hidden, the reveal says it may spoil)"))
_add(Scenario("us-walkthrough-screen-p2d", "us", "full", turbo=4, timeout=300,
              env=(("DW3_START_MODE", "0x29C"), ("DW3_PROGRESS", "45")) + _RT_BATTLE_ENV,
              expect_log=((0, r"dw3-port-options: open in game(?s:.*)"
                              r"dw3-walkthrough: open \(from field, region US" + _wt_open_re("US") +
                              r"dw3-walkthrough: progress 45 half 1 area 14 -> chapter - step - "
                              r"\(rule above-table\) \[open mode 0x29c\](?s:.*)"
                              r"dw3-walkthrough: step not recognised \(progress 45\)(?s:.*)"
                              r"dw3-walkthrough: closed after \d+ ms \(pages opened 0, spoilers shown 0\)(?s:.*)"
                              + _WT_CLOSE + r"(?s:.*)dw3-port-options: close in game \(back to the field menu\)"),),
              forbid_log=_WT_FORBID + ((0, r"widescreen (enabled|disabled)"), (0, r"ch12"),
                                       (0, r"browse mode"),),
              note="P6.35 slice 3: region gating on screen, progress 0x2D on US is past its table: the "
                   "chapter list (11 chapters, no extra chapter) with the 'not recognised' notice"))

# P6.35 slice 4 (docs/P6.35_WALKTHROUGH.md "As built, slice 4"): the optional rows. (tag, start
# mode, progress, half, area, chapter, current step, rule, optional rows). Shown = the chapter's first
# value: the steps list has the block (`optional rows listed 2 of 2`) and its rows are read. Hidden =
# one value before the chapter: the next chapter is `Not reached yet`, its steps list has no optional
# row (`listed 0 of 2`) and no SIDE page is ever opened. ch04 and ch08 are on both discs; ch12 (the
# extra chapter) on EU only: US at progress 0x2D is the existing `us-walkthrough-screen-p2d`. The
# first two chapters boot in the Login Room (0x207): the stages of the chapters' own steps (the
# Underground Cave, the Battle Gate) write the next progress value as soon as they start.
_WT_SIDE_SHOWN = (
    ("pf", "0x207", 15, 0, 19, "ch04", "ch04.s01", "progress", ("ch04.x01", "ch04.x02"), "both"),
    ("p1e", "0x207", 30, 0, 19, "ch08", "ch08.s01", "progress", ("ch08.x01", "ch08.x02"), "both"),
    ("p2d", "0x29C", 45, 1, 14, "ch12", "ch12.s01", r"progress\+flag", ("ch12.x01", "ch12.x02"), "eu"),
)
# (tag, start mode, progress, half, area, chapter now, step now, the chapter ahead, its pages, its main steps)
_WT_SIDE_HIDDEN = (
    ("pe", "0x207", 14, 0, 19, "ch03", "ch03.s05", "ch04", 1, 3, "both"),
    ("p1d", "0x207", 29, 0, 19, "ch07", "ch07.s03", "ch08", 2, 7, "both"),
    ("p2b", "0x2DF", 43, -1, -1, "ch11", "ch11.s01", "ch12", 3, 3, "eu"),
)
for _v, _reg in (("us", "US"), ("eu", "EU")):
    _open = (r"dw3-port-options: open in game(?s:.*)dw3-walkthrough: open \(from field, region " + _reg +
             _wt_open_re(_reg))
    for _tag, _mode, _prog, _half, _area, _ch, _step, _rule, _ids, _who in _WT_SIDE_SHOWN:
        if _who == "eu" and _v == "us":
            continue
        _add(Scenario(f"{_v}-walkthrough-screen-side-{_tag}", _v, "full", turbo=4, timeout=300,
                      env=(("DW3_START_MODE", _mode), ("DW3_PROGRESS", str(_prog))) + _RT_BATTLE_ENV,
                      expect_log=((0, _open +
                                      rf"dw3-walkthrough: progress {_prog} half {_half} area {_area} -> chapter {_ch} "
                                      rf"step {re.escape(_step)} \(rule {_rule}\) \[open mode {_mode.lower()}\](?s:.*)"
                                      rf"dw3-walkthrough: chapter {_ch} optional rows listed 2 of 2(?s:.*)"
                                      rf"dw3-walkthrough: page {re.escape(_ids[0])} \(kind SIDE, from list, reached y, current n, \d+ lines(?s:.*)"
                                      rf"dw3-walkthrough: spoiler {re.escape(_ids[0])} closed \(answer available\)(?s:.*)"
                                      rf"dw3-walkthrough: spoiler {re.escape(_ids[0])} asked(?s:.*)"
                                      rf"dw3-walkthrough: spoiler {re.escape(_ids[0])} shown(?s:.*)"
                                      rf"dw3-walkthrough: page {re.escape(_ids[0])} closed after \d+ ms \(spoiler shown y\)(?s:.*)"
                                      rf"dw3-walkthrough: page {re.escape(_ids[1])} \(kind SIDE, from list, reached y, current n, \d+ lines(?s:.*)"
                                      rf"dw3-walkthrough: page {re.escape(_ids[1])} closed after \d+ ms \(spoiler shown n\)(?s:.*)"
                                      r"dw3-walkthrough: closed after \d+ ms \(pages opened 3, spoilers shown 1\)(?s:.*)"
                                      + _WT_CLOSE + r"(?s:.*)dw3-port-options: close in game \(back to the field menu\)"),),
                      forbid_log=_WT_FORBID + ((0, r"widescreen (enabled|disabled)"),
                                               (0, r"step not recognised|browse mode"),
                                               (0, r"chapter ch(04|08|12) optional rows listed [01] of")),
                      note=f"P6.35 slice 4: at progress {_prog} ({_ch} starts) the steps list shows the chapter's "
                           f"optional block; both rows open as SIDE pages with the answer behind the two actions"))
    for _tag, _mode, _prog, _half, _area, _ch, _step, _ahead, _npages, _nmain, _who in _WT_SIDE_HIDDEN:
        if _who == "eu" and _v == "us":
            continue
        _add(Scenario(f"{_v}-walkthrough-screen-side-{_tag}", _v, "full", turbo=4, timeout=300,
                      env=(("DW3_START_MODE", _mode), ("DW3_PROGRESS", str(_prog))) + _RT_BATTLE_ENV,
                      expect_log=((0, _open +
                                      rf"dw3-walkthrough: progress {_prog} half {_half} area {_area} -> chapter {_ch} "
                                      rf"step {re.escape(_step)} \(rule progress\) \[open mode {_mode.lower()}\](?s:.*)"
                                      rf"dw3-walkthrough: chapter {_ahead} optional rows listed 0 of 2(?s:.*)"
                                      rf"dw3-walkthrough: chapter {_ahead} opened \(reached n, current n, {_npages} pages, {_nmain} steps\)(?s:.*)"
                                      r"dw3-walkthrough: closed after \d+ ms \(pages opened 1, spoilers shown 0\)(?s:.*)"
                                      + _WT_CLOSE + r"(?s:.*)dw3-port-options: close in game \(back to the field menu\)"),),
                      forbid_log=_WT_FORBID + ((0, r"widescreen (enabled|disabled)"),
                                               (0, r"step not recognised|browse mode"),
                                               (0, r"kind SIDE"), (0, r"chapter ch(04|08|12) optional rows listed [12] of")),
                      note=f"P6.35 slice 4: one value before {_ahead} (progress {_prog}) the chapter is not reached yet, "
                           f"its steps list has no optional row and no SIDE page is opened"))

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


# D1: Region at start moved from Gameplay (4 rows everywhere now) to Saves, which has one
# row more in the combined build.
_GP_ROWS = (
    (r"saves \(2 row\(s\), in game\)", r"saves \(3 row\(s\), in game\)"),
    (r"saves \(2 row\(s\), title\)", r"saves \(3 row\(s\), title\)"),
)


def _both_expect(expect):
    """The combined (--bin-both) build has the Region row in the Saves group (D1; it
    was in Gameplay), so that group logs 3 rows there, not 2 (P6.51)."""
    out = []
    for p, r in expect:
        for plain, both in _GP_ROWS:
            r = r.replace(plain, both)
        out.append((p, r))
    return tuple(out)


# D1: field-options and options-persist no longer differ on the combined binary in their log
# (Gameplay has four rows everywhere); only scenarios that open Saves do (below).


# D1 (docs/OPTIONS_REORG.md): the Menus group, the rows that were file-only keys, the
# Shared saves and Stage sweep windows, the Map buttons screen. Recordings:
# tools/replay/gen_scenarios.py options_menus .. options_mapbtn.
_NO_BAD = ((0, r"is not valid"), (0, r"BINDING PROBLEM"))
for _v in ("us", "eu"):
    _add(Scenario(f"{_v}-options-menus", _v, "full", timeout=300,
                  phases=(f"{_v}-options-menus-p1", f"{_v}-options-menus-p2"),
                  expect_log=((0, r"dw3-port-options: group open menus \(6 row\(s\), title\)(?s:.*)"
                                  r"dw3-port-options: equip from items disabled(?s:.*)"
                                  r"dw3-port-options: equip stays on list disabled(?s:.*)"
                                  r"dw3-port-options: whole bag disabled(?s:.*)"
                                  r"dw3-port-options: use item stays disabled(?s:.*)"
                                  r"dw3-port-options: partner switch disabled(?s:.*)"
                                  r"dw3-port-options: instant menu exit disabled(?s:.*)"
                                  r"dw3-port-options: group close menus \(back to the group list\)"),
                              (1, r"dw3-options: file keys items_equip_from_items=0 items_equip_stays=0 "
                                  r"items_bag_view=0 items_use_stays=0 items_partner_switch=0(?s:.*)menu_instant_exit=0")),
                  forbid_log=_NO_BAD + ((1, r"is not valid"),),
                  note="Options > Menus: the six switches (the five Items-menu ones and Instant menu exit) turned OFF are read back by the next process (D1, patch 0200)"),
         Scenario(f"{_v}-options-display", _v, "full", timeout=300,
                  phases=(f"{_v}-options-display-p1", f"{_v}-options-display-p2"),
                  expect_log=((0, r"dw3-port-options: group open display \(7 row\(s\), title\)(?s:.*)"
                                  r"dw3-port-options: map scaler lanczos2(?s:.*)"
                                  r"dw3-port-options: map mod pack disabled(?s:.*)"
                                  r"dw3-port-options: version label disabled"),
                              (1, r"dw3-options: file keys .* show_version=0 map_mod=0 .*map_scaler=lanczos2")),
                  forbid_log=_NO_BAD + ((1, r"is not valid"),),
                  note="Options > Display: Map scaler (FSR 1 to Lanczos2, D5), Map mod pack and Version label turned OFF are read back by the next process (D1)"),
         Scenario(f"{_v}-options-saves", _v, "full", timeout=300,
                  expect_log=((0, r"dw3-port-options: group open saves \(2 row\(s\), title\)(?s:.*)"
                                  r"dw3-port-confirm: open shared saves \(cursor on No\)(?s:.*)"
                                  r"dw3-port-confirm: No \(shared saves\); nothing changed(?s:.*)"
                                  r"dw3-options: save_store 0->1 declined(?s:.*)"
                                  r"dw3-port-confirm: open shared saves \(cursor on No\)(?s:.*)"
                                  r"dw3-port-confirm: cursor on Yes(?s:.*)"
                                  r"dw3-port-confirm: Yes \(shared saves\)(?s:.*)"
                                  r"dw3-options: save_store 0->1 \(confirmed, next start\)(?s:.*)"
                                  r"dw3-port-options: shared saves disabled"),),
                  forbid_log=_NO_BAD,
                  note="Options > Saves > Shared saves: ON asks first (cursor on No); No keeps it OFF, Yes turns it on, Confirm turns it off with no window (D1)"),
         Scenario(f"{_v}-options-advanced", _v, "full", timeout=300,
                  expect_log=((0, r"dw3-port-options: group open advanced \(2 row\(s\), title\)(?s:.*)"
                                  r"dw3-port-confirm: open stage sweep \(cursor on No\)(?s:.*)"
                                  r"dw3-port-confirm: No \(stage sweep\); nothing changed(?s:.*)"
                                  r"dw3-options: stage_sweep 0->1 declined(?s:.*)"
                                  r"dw3-port-options: sweep frames 120 \(next start\)(?s:.*)"
                                  r"dw3-port-options: sweep frames 150 \(next start\)(?s:.*)"
                                  r"dw3-port-options: sweep frames 450 \(next start\)(?s:.*)"
                                  r"dw3-options: saved \S+ \(.*\)"),),
                  forbid_log=_NO_BAD + ((0, r"dw3-sweep:"),),
                  note="Options > Advanced: the Stage sweep window (No keeps it OFF) and Sweep frames by 30 and 300 (D1)"),
         Scenario(f"{_v}-options-mapbtn", _v, "full", timeout=300,
                  expect_log=((0, r"dw3-port-options: group open mapbtn \(8 row\(s\), title\)(?s:.*)"
                                  r"dw3-options: map button map_view_zoom_out l1->r1 "
                                  r"\(swapped: map_view_zoom_in r1->l1\)(?s:.*)"
                                  r"dw3-options: map buttons reset to the defaults \(open=square zoom_out=l1 "
                                  r"zoom_in=r1 centre=select scaler_prev=l2 scaler_next=r2 field_open=select\)(?s:.*)"
                                  r"dw3-port-options: group close mapbtn \(back to controls\)"),),
                  forbid_log=_NO_BAD,
                  note="Options > Controls > Map buttons: a button another action has is swapped; Reset puts the seven defaults back (D1)"),
         )
for _v in ("us", "eu"):
    _sc = by_name()[f"{_v}-options-saves"]
    SCENARIOS[SCENARIOS.index(_sc)] = dataclasses.replace(
        _sc, expect_log_both=_both_expect(_sc.expect_log))


def _nintendo(base_name, name, phase, note):
    b = by_name()[base_name]
    logs = tuple(e for e in b.expect_log if e[0] == 0)
    full = logs + tuple((0, r) for r in _NIN_LOG)
    return dataclasses.replace(
        b, name=name, phases=(phase,), cfg=b.cfg + _NIN_CFG, env=b.env + _NIN_ENV,
        expect_log=full, expect_log_both=_both_expect(full),
        forbid_log=tuple(e for e in b.forbid_log if e[0] == 0)
        + ((0, r"dw3-controls: map invalid"),),
        ref_from=base_name, note=note)


def _nintendo_rebind(v):
    """The rebound view buttons under the Nintendo layout: the plain run's checks, with the
    console buttons of the bindings line (open R2 = ZR is the same, zoom out Square = Y, zoom in
    Cross = A, close Triangle = B, bars Circle = X)."""
    sc = _nintendo(f"{v}-map-rebind", f"{v}-nintendo-map-rebind", f"{v}-nintendo-map-rebind",
                   "Nintendo layout: the rebound view buttons (open R2, zoom out Square, zoom in Cross, ...) "
                   "go through the map; same pictures as the plain run (P6.32 phase 4)")
    ps = _bind_in_effect("playstation", "0123456789", "ZR", "Y", "B", "StickL", "L", "R", "X", "A")
    nin = _bind_in_effect("nintendo", "1203456789", "ZR", "Y", "A", "StickL", "L", "R", "B", "X")
    fix = lambda es: tuple((i, nin if r == ps else r) for i, r in es)
    return dataclasses.replace(sc, expect_log=fix(sc.expect_log), expect_log_both=fix(sc.expect_log_both))


for _v in ("us", "eu"):
    _add(
        _nintendo(f"{_v}-field-options", f"{_v}-nintendo-menu", f"{_v}-nintendo-menu",
                  "Nintendo layout: A opens the field menu and the Options screen, B backs out "
                  "of the group, the screen and the menu (P9.8); same pictures as the plain run"),
        _nintendo(f"{_v}-map-view", f"{_v}-nintendo-map-view", f"{_v}-nintendo-map-view",
                  "Nintendo layout: the full-screen map opens with Square (Y) and closes with the "
                  "logical cancel bit, B (P9.8, P6.32)"),
        _nintendo_rebind(_v),
        _nintendo(f"{_v}-title-quit", f"{_v}-nintendo-title-quit", f"{_v}-nintendo-title-quit",
                  "Nintendo layout: the title menu and the Quit Yes / No window confirm with A "
                  "and cancel with B (P9.8)"),
        Scenario(f"{_v}-controls-layout", _v, "full", timeout=300,
                 phases=(f"{_v}-controls-layout-p1", f"{_v}-controls-layout-p2"),
                 expect_log=((0, r"dw3-controls: map loaded layout=playstation source=default(?s:.*)"
                                 r"dw3-port-options: group open controls \(5 row\(s\), title\)(?s:.*)"
                                 r"dw3-controls: layout changed playstation -> nintendo \(screen\)(?s:.*)"
                                 r"dw3-controls: map cross<-A circle<-X triangle<-B square<-Y"),
                             (1, r"dw3-controls: map loaded layout=nintendo source=cfg"),
                             (1, r"dw3-port-options: group open controls \(5 row\(s\), title\)")),
                 forbid_log=((0, r"dw3-controls: map invalid"), (1, r"dw3-controls: map invalid")),
                 note="Controls group: the Button layout row switches to Nintendo and is read back "
                      "from port_options.cfg by the next process (P9.8)"),
    )

# --- P6.12 custom remap (docs/CONTROLS_BUNDLE.md, phase 2) -----------------------
# The Customize buttons screen. DW3_REPLAY_SOURCE=1: the recording's masks are button
# positions, so the assignments go through the map; the references are their own
# (the pictures of the new screens), kept outside git.
for _v in ("us", "eu"):
    _add(
        Scenario(f"{_v}-controls-custom", _v, "full", timeout=400,
                 phases=(f"{_v}-controls-custom-p1", f"{_v}-controls-custom-p2"),
                 env=_NIN_ENV,
                 expect_log=((0, r"dw3-controls: map loaded layout=playstation source=default(?s:.*)"
                                 r"dw3-port-options: group open customize \(10 row\(s\), title\)(?s:.*)"
                                 r"dw3-port-options: customize waiting for a button for L1(?s:.*)"
                                 r"dw3-controls: assign l1<-R \(r1<-L swapped in\) \[custom\](?s:.*)"
                                 r"dw3-controls: map cross<-B circle<-A triangle<-X square<-Y "
                                 r"l1<-R r1<-L l2<-ZL r2<-ZR l3<-StickL r3<-StickR(?s:.*)"
                                 r"dw3-port-options: group open help"),
                             (0, r"dw3-options: saved \S+ \(.*\)"),
                             (1, r"dw3-controls: map loaded layout=custom source=cfg(?s:.*)"
                                 r"dw3-controls: map cross<-B circle<-A triangle<-X square<-Y "
                                 r"l1<-R r1<-L(?s:.*)"
                                 r"dw3-controls: reset to default \(layout=playstation\)")),
                 forbid_log=((0, r"dw3-controls: map invalid"), (1, r"dw3-controls: map invalid")),
                 note="Customize buttons: swap L1 and R1, the Help pages turn accordingly, the map "
                      "is read back from port_options.cfg, Reset restores (P6.12)"),
        # D2: no DW3_REPLAY_SOURCE here: the fallback layout is Nintendo now, and the recording's
        # masks name PS buttons (positions through the Nintendo map would not reach the Controls page).
        Scenario(f"{_v}-controls-custom-invalid", _v, "full", timeout=200,
                 cfg=(("button_layout", "custom"), ("button_map", "0123456780")),
                 expect_log=((0, r"dw3-controls: map invalid \(button_map is not a permutation of 0-9\), "
                                 r"using default(?s:.*)"
                                 r"dw3-controls: map loaded layout=playstation source=default"),),
                 note="button_layout=custom with a button_map that is not a permutation: default + "
                      "one log line (P6.12)"),
    )
_add(_nintendo("eu-lang-cross-fr", "eu-nintendo-lang-cross-fr", "eu-nintendo-lang-cross-fr",
               "Nintendo layout: the EU language select confirms with A, the logical Cross (patch "
               "0187, P9.8)"))

# --- D2: defaults_gen and the one-shot migration (docs/OPTIONS_REORG.md section 6.1) ------
# The runner seeds every cfg with `defaults_gen=2`; these scenarios are the exception. A
# `legacy_cfg` scenario seeds the file as a build before D2 wrote it, with no marker.
#  defaults-migrate         old file (button_layout=playstation, switch_glyphs=0): the layout flips at boot and
#                           the file is written at once; the player puts PlayStation back; the second process
#                           must not migrate again and keeps PlayStation
#  defaults-migrate-custom  a custom layout is kept (and the PC leaves the glyphs alone: nothing to flip)
#  defaults-migrate-fresh   no file: not migrated, born on the new defaults (Nintendo; glyphs OFF on the PC)
# The PC has no Switch glyphs row: switch_glyphs is never flipped there and the line says so (the Switch flip,
# `switch_glyphs 0->1`, is a unit test, test_options_persist.py with the row forced).
_MIG_ALL = (r"dw3-options: migrate defaults_gen 1->2 button_layout playstation->nintendo "
            r"\(kept: switch_glyphs \(pc\)\)")
_MIG_BAD = ((0, r"NOT saved"), (0, r"dw3-controls: map invalid"), (1, r"dw3-controls: map invalid"))
for _v in ("us", "eu"):
    _add(
        Scenario(f"{_v}-defaults-migrate", _v, "full", timeout=300,
                 phases=(f"{_v}-defaults-migrate-p1", f"{_v}-defaults-migrate-p2"),
                 cfg=(("button_layout", "playstation"), ("switch_glyphs", "0")), legacy_cfg=True,
                 expect_log=((0, r"dw3-controls: map loaded layout=nintendo source=cfg(?s:.*)"
                                 r"dw3-options: switch_glyphs=off(?s:.*)" + _MIG_ALL + r"(?s:.*)"
                                 r"dw3-port-options: group open controls \(5 row\(s\), title\)(?s:.*)"
                                 r"dw3-controls: layout changed nintendo -> playstation \(screen\)"),
                             (1, r"dw3-controls: map loaded layout=playstation source=cfg"),
                             (1, r"dw3-options: switch_glyphs=off")),
                 forbid_log=_MIG_BAD + ((1, r"migrate defaults_gen"), (1, r"nothing to flip"),
                                        (1, r"defaults_gen=2 \(new file\)")),
                 note="An old port_options.cfg (PlayStation, glyphs 0, no defaults_gen) is migrated once at boot: "
                      "Nintendo, glyphs left alone on the PC, written at once; PlayStation put back by the player "
                      "is kept by the next process (D2)"),
        Scenario(f"{_v}-defaults-migrate-custom", _v, "full", timeout=200,
                 phases=(f"{_v}-defaults-view",),
                 cfg=(("button_layout", "custom"), ("button_map", "0123546789"), ("switch_glyphs", "0")),
                 legacy_cfg=True,
                 expect_log=((0, r"dw3-controls: map loaded layout=custom source=cfg(?s:.*)"
                                 r"dw3-controls: map cross<-B circle<-A triangle<-X square<-Y l1<-R r1<-L(?s:.*)"
                                 r"dw3-options: switch_glyphs=off(?s:.*)"
                                 r"dw3-options: defaults_gen 1->2 nothing to flip"),),
                 forbid_log=_MIG_BAD + ((0, r"button_layout playstation->nintendo"),),
                 note="A custom layout is not touched by the migration; on the PC the glyphs are not either, "
                      "so there is nothing to flip (D2)"),
        Scenario(f"{_v}-defaults-migrate-fresh", _v, "full", timeout=200,
                 phases=(f"{_v}-defaults-view",),
                 expect_log=((0, r"dw3-options: defaults_gen=2 \(new file\)(?s:.*)"
                                 r"dw3-controls: map loaded layout=nintendo source=default(?s:.*)"
                                 r"dw3-controls: map cross<-A circle<-X triangle<-B square<-Y(?s:.*)"
                                 r"dw3-options: switch_glyphs=off"),),
                 forbid_log=_MIG_BAD + ((0, r"migrate defaults_gen"), (0, r"nothing to flip")),
                 note="A fresh install is born on the new defaults (Nintendo; glyphs OFF on the PC, ON on the "
                      "Switch) and is not migrated (D2)"),
    )

# D3 / P6.26 (docs/OPTIONS_REORG.md 6.2): ONE global port_options.cfg. The scenarios seed the
# per-region files an older build wrote (`us/` and `eu/` under the config folder, no marker) and
# the old top-level file (`last_region`), boot, and read the proof in the log. They carry no
# picture: the proof is the three log lines of the migration, the merged values at boot, and the
# second process finding nothing left to do. The recording is the same for all four cases.
_G_FILE = r"\S+port_options\.cfg"
_G_BOOT = (r"dw3-options: " + _G_FILE + r" \(widescreen=%s fast_forward=2 restart_hotkey=0 music_volume=%s "
           r"sfx_volume=10 encounter_rate=0 text_speed=%s battle_menu_memory=0\)")
_G_QUIET = ((1, r"dw3-options: migrate "), (1, r"dw3-options: deleted "), (1, r"migrated from"),
            (1, r"no us/eu port_options"), (1, r"migration NOT done"))
for _v in ("us", "eu"):
    _o = "eu" if _v == "us" else "us"      # the region that is NOT running: its file wins the conflict
    _add(
        Scenario(f"{_v}-options-global-migrate", _v, "full", timeout=300, log_only=True,
                 phases=(f"{_v}-options-global-view", f"{_v}-options-global-view"),
                 cfg=(("region_default", "ask"), ("last_region", _o)),
                 cfg_us=(("defaults_gen", "2"), ("widescreen", "0"), ("music_volume", "4"),
                         ("items_bag_view", "0"), ("sfx_volume", "10")),
                 cfg_eu=(("defaults_gen", "2"), ("widescreen", "1"), ("music_volume", "6"),
                         ("text_speed", "2"), ("sfx_volume", "10")),
                 expect_log=((0, rf"dw3-options: migrate winner={_o} \(last_region={_o}\)"),
                             (0, r"dw3-options: migrate key=widescreen us=0 eu=1 kept=" + ("1" if _o == "eu" else "0")),
                             (0, r"dw3-options: migrate key=music_volume us=4 eu=6 kept=" + ("6" if _o == "eu" else "4")),
                             (0, r"dw3-options: global file " + _G_FILE + r" \(migrated from us\+eu, \d+ keys, "
                                 r"2 conflicts: (widescreen, music_volume|music_volume, widescreen)\)"),
                             (0, r"dw3-options: backup us/port_options\.cfg -> " + _G_FILE + r"\.us\.bak \(\d+ bytes, verified\)"),
                             (0, r"dw3-options: backup eu/port_options\.cfg -> " + _G_FILE + r"\.eu\.bak \(\d+ bytes, verified\)"),
                             (0, r"dw3-options: deleted us/port_options\.cfg after read-back ok"),
                             (0, r"dw3-options: deleted eu/port_options\.cfg after read-back ok"),
                             (0, _G_BOOT % (("1", "6", "2") if _o == "eu" else ("0", "4", "2"))),
                             (1, _G_BOOT % (("1", "6", "2") if _o == "eu" else ("0", "4", "2")))),
                 forbid_log=_G_QUIET + _MIG_BAD + ((0, r"migrate defaults_gen"), (0, r"migration NOT done")),
                 note=f"The old us/ and eu/ files are merged into the one global file: the {_o.upper()} file (last_region) wins "
                      "the two conflicts, a key in one file is taken, the old files are backed up and deleted after the "
                      "read-back; the second process finds nothing to do (D3)"),
        Scenario(f"{_v}-options-global-migrate-single", _v, "full", timeout=300, log_only=True,
                 phases=(f"{_v}-options-global-view",),
                 **{f"cfg_{_o}": (("defaults_gen", "2"), ("widescreen", "0"), ("music_volume", "5"), ("sfx_volume", "10"))},
                 expect_log=((0, r"dw3-options: global file " + _G_FILE + rf" \(migrated from {_o}, \d+ keys, 0 conflicts\)"),
                             (0, rf"dw3-options: backup {_o}/port_options\.cfg -> " + _G_FILE + rf"\.{_o}\.bak"),
                             (0, rf"dw3-options: deleted {_o}/port_options\.cfg after read-back ok"),
                             (0, _G_BOOT % ("0", "5", "0"))),
                 forbid_log=_MIG_BAD + ((0, r"migrate winner"), (0, r"migrate key="), (0, rf"deleted {_v}/"),
                                        (0, r"migration NOT done")),
                 note=f"Only the {_o.upper()} file exists: it is taken whole, no conflict, then backed up and deleted (D3)"),
        Scenario(f"{_v}-options-global-migrate-fresh", _v, "full", timeout=200, log_only=True,
                 phases=(f"{_v}-options-global-view",),
                 expect_log=((0, r"dw3-options: global file " + _G_FILE + r" \(no us/eu port_options\.cfg to migrate\)"),
                             (0, r"dw3-options: defaults_gen=2 \(new file\)")),
                 forbid_log=_MIG_BAD + ((0, r"dw3-options: migrate "), (0, r"dw3-options: deleted "),
                                        (0, r"migrated from"), (0, r"migration NOT done")),
                 note="A fresh install has no per-region file: nothing is migrated, nothing is created or deleted (D3)"),
        Scenario(f"{_v}-options-global-migrate-failed", _v, "full", timeout=300, log_only=True,
                 phases=(f"{_v}-options-global-view", f"{_v}-options-global-view"),
                 cfg=(("last_region", _v),),
                 cfg_us=(("defaults_gen", "2"), ("widescreen", "0")),
                 cfg_eu=(("defaults_gen", "2"), ("widescreen", "1")),
                 block_global_write=True,
                 expect_log=((0, r"dw3-options: migrate winner=" + _v),
                             (0, r"dw3-options: migration NOT done \(cannot write " + _G_FILE + r": .+\), old files kept"),
                             (1, r"dw3-options: migration NOT done \(cannot write " + _G_FILE + r": .+\), old files kept")),
                 forbid_log=((0, r"dw3-options: deleted "), (0, r"dw3-options: backup "), (0, r"migrated from"),
                             (1, r"dw3-options: deleted "), (1, r"dw3-options: backup "), (1, r"migrated from")),
                 note="The global file cannot be written: nothing is deleted or backed up, the old files are still there "
                      "for the retry at the next start (the second process tries again and says so) (D3)"),
    )

# P6.32 phase 7 (no patch): the help line of the full-screen map view names the buttons the
# way the player sees them (port/map_view/map_view_help.c). The footer follows the button
# layout, a custom map and the phase 4 bindings: the PC prints the PS word of the position
# that makes the button, the Switch the cap text or, with switch_glyphs=1, the glyph; the
# words around them are in the language of the game's text. The PC never draws glyphs, so
# `switch_glyphs=1` there still prints labels (`glyphs-pc`: bit-identical to the plain run);
# the test aid DW3_MAP_HELP_SWITCH=1 (PC only, scrubbed) draws the line as the Switch does,
# so a picture of the Switch's text and glyphs exists (`sw`, `sw-glyphs`). `mh-open` has no
# bars and is the same in every variant (ref_from); `mh-help`, `mh-help-zoom` differ by design.
#  map-help            PlayStation layout (the new references)
#  nintendo-map-help   button_layout=nintendo: Circle is made by the Triangle position, Triangle by Cross
#  custom-map-help     a custom map with L1 on ZL, R1 on ZR, L2 on L, R2 on R
#  map-help-glyphs-pc  switch_glyphs=1 on the PC: the same labels, 0 px against map-help
#  map-help-sw*        the Switch's line on the PC (aid): Nintendo text; Nintendo glyphs; custom glyphs
#  eu-map-help-fr/de/it/es  the five-language words (fr, de also with glyphs under the aid)
_HELP_HUD = ("mh-help", "mh-help-zoom")
_HELP_PS = "0123456789"
_HELP_NIN = "1203456789"
_HELP_CUSTOM = "0123674589"
_HELP_GL_PC = r"off \(switch_glyphs=on, but a PC build draws labels\)"


def _hl(text1, text2, layout, digits, glyphs, lang, names):
    """Both help lines of a configuration, as the `dw3-map: help line` log writes them."""
    nm = {"pc": "PS words of the position", "sw": "Switch buttons"}[names]
    out = []
    for i, t in ((1, text1), (2, text2)):
        out.append((0, r'dw3-map: help line %d "%s" \(layout=%s, map=%s, glyphs=%s, lang=%s, names=%s, '
                       r'\d+ of 628 px: fits\)' % (i, re.escape(t), layout, digits, glyphs, lang, nm)))
    return tuple(out)


_EN1 = "PAD/L-STICK CURSOR  R-STICK/%s ZOOM  %s SCALER"
_HELP_TEXT = {
    # (lang, kind): (line 1, line 2); kind = the button map and who names the buttons
    ("en", "ps-pc"): (_EN1 % ("L1/R1", "L2/R2"), "SELECT CENTRE ON YOUR AREA  CIRCLE HUD  TRI BACK"),
    ("en", "nin-pc"): (_EN1 % ("L1/R1", "L2/R2"), "SELECT CENTRE ON YOUR AREA  TRI HUD  CROSS BACK"),
    ("en", "custom-pc"): (_EN1 % ("L2/R2", "L1/R1"), "SELECT CENTRE ON YOUR AREA  CIRCLE HUD  TRI BACK"),
    ("en", "nin-sw"): (_EN1 % ("L/R", "ZL/ZR"), "MINUS CENTRE ON YOUR AREA  X HUD  B BACK"),
    ("en", "nin-glyph"): (_EN1 % ("[L]/[R]", "[ZL]/[ZR]"), "MINUS CENTRE ON YOUR AREA  [X] HUD  [B] BACK"),
    ("en", "custom-glyph"): (_EN1 % ("[ZL]/[ZR]", "[L]/[R]"), "MINUS CENTRE ON YOUR AREA  [A] HUD  [X] BACK"),
    ("fr", "ps-pc"): ("PAD/STICK G. CURSEUR  STICK D./L1/R1 ZOOM  L2/R2 FILTRE",
                      "SELECT CENTRER SUR VOTRE ZONE  CIRCLE HUD  TRI RETOUR"),
    ("fr", "nin-glyph"): ("PAD/STICK G. CURSEUR  STICK D./[L]/[R] ZOOM  [ZL]/[ZR] FILTRE",
                          "MINUS CENTRER SUR VOTRE ZONE  [X] HUD  [B] RETOUR"),
    ("de", "nin-pc"): ("PAD/L-STICK CURSOR  R-STICK/L1/R1 ZOOM  L2/R2 SKALIERER",
                       "SELECT AUF DEINEM GEBIET ZENTRIEREN  TRI HUD  CROSS ZURUECK"),
    ("de", "custom-glyph"): ("PAD/L-STICK CURSOR  R-STICK/[ZL]/[ZR] ZOOM  [L]/[R] SKALIERER",
                             "MINUS AUF DEINEM GEBIET ZENTRIEREN  [A] HUD  [X] ZURUECK"),
    ("it", "ps-pc"): ("PAD/STICK SIN. CURSORE  STICK DES./L1/R1 ZOOM  L2/R2 RIDIMENS.",
                      "SELECT CENTRA SULLA TUA AREA  CIRCLE HUD  TRI INDIETRO"),
    ("es", "ps-pc"): ("PAD/STICK IZQ. CURSOR  STICK DER./L1/R1 ZOOM  L2/R2 ESCALADOR",
                      "SELECT CENTRAR EN TU ZONA  CIRCLE HUD  TRI VOLVER"),
}
_HELP_KIND = {  # kind -> (layout, digits, glyphs field)
    "ps-pc": ("playstation", _HELP_PS, "off"), "nin-pc": ("nintendo", _HELP_NIN, "off"),
    "custom-pc": ("custom", _HELP_CUSTOM, "off"), "nin-sw": ("nintendo", _HELP_NIN, "off"),
    "nin-glyph": ("nintendo", _HELP_NIN, "on"), "custom-glyph": ("custom", _HELP_CUSTOM, "on"),
}


def _help_expect(lang, kind, glyphs=None):
    layout, digits, gl = _HELP_KIND[kind]
    t1, t2 = _HELP_TEXT[(lang, kind)]
    return _hl(t1, t2, layout, digits, glyphs or gl, lang, "sw" if kind.endswith(("-sw", "-glyph")) else "pc")


_HELP_FORBID = ((0, r"dw3-map: help line \d [^\n]*(?:OVERFLOWS|DOES NOT FIT)|HELP LINE \d DOES NOT FIT|"
                    r"dw3-map: capture failed|NOTHING DRAWN|\): CHANGED|dw3-map: BINDING PROBLEM|"
                    # one open, one build of each line: a zoom step with the bars on does not log it again
                    r'dw3-map: help line 1 [^\n]*\n(?s:.*)dw3-map: help line 1 '),)


for _v, _ids in (("us", (0x159, 0x1FA, 0x1C2, 0x158, 0x1F9)),
                 ("eu", (0x167, 0x208, 0x1D1, 0x166, 0x207))):
    _fight, _menu, _report, _field, _status = _ids

    def _help(name, phase, expect, note, ref_from="", own=(), cfg=(), env=(), lang_ref=None):
        return Scenario(
            name, _v, "full", turbo=4, timeout=600, phases=(phase,),
            env=_ITEMS_ENV + (("DW3_MAP_SYNC", "1"),) + env,
            cfg=(("encounter_rate", "2"), ("map_screen", "1")) + cfg,
            expect_restore=((_fight, M), (_menu, S), (_report, M), (_field, M), (_status, M)),
            expect_log=(
                (0, r"dw3-map: open half=\d image=392x352 .* turbo=1 zoom=1\.50 hud=off"),
                (0, r"dw3-map: HUD on \(the bars; the name box stays\)(?s:.*)dw3-map: help line 1 "),
                (0, r"dw3-map: close requested by the cancel button"),
                (0, r"dw3-map: game Map screen back after the close: .*: unchanged"),
            ) + expect,
            forbid_log=_HELP_FORBID, ref_from=ref_from, own_refs=own, note=note)

    _pl = _help(f"{_v}-map-help", f"{_v}-map-help", _help_expect("en", "ps-pc"),
                "the map view's help line, PlayStation layout, PC labels (P6.32 phase 7)")
    _nin = _help(f"{_v}-nintendo-map-help", f"{_v}-nintendo-map-help",
                 _help_expect("en", "nin-pc") + ((0, _NIN_LOG[0]),),
                 "Nintendo layout: the help line says TRI HUD and CROSS BACK (P6.32 phase 7)",
                 ref_from=f"{_v}-map-help", own=_HELP_HUD, cfg=_NIN_CFG, env=_NIN_ENV)
    _cus = _help(f"{_v}-custom-map-help", f"{_v}-custom-map-help",
                 _help_expect("en", "custom-pc") + ((0, r"dw3-controls: map loaded layout=custom source=cfg"),),
                 "a custom map (L1 on L2's position ...): the help line names the positions (P6.32 phase 7)",
                 ref_from=f"{_v}-map-help", own=_HELP_HUD, env=_NIN_ENV,
                 cfg=(("button_layout", "custom"), ("button_map", _HELP_CUSTOM)))
    _gpc = _help(f"{_v}-map-help-glyphs-pc", f"{_v}-map-help",
                 _help_expect("en", "ps-pc", glyphs=_HELP_GL_PC) + ((0, r"dw3-options: switch_glyphs=on"),),
                 "switch_glyphs=1 on the PC still prints labels: bit-identical to the plain run (P6.32 phase 7)",
                 ref_from=f"{_v}-map-help", cfg=(("switch_glyphs", "1"),))
    _sw = _help(f"{_v}-map-help-sw", f"{_v}-nintendo-map-help",
                _help_expect("en", "nin-sw") + ((0, r"DW3_MAP_HELP_SWITCH test aid"),),
                "the Switch's help line on the PC (aid): cap text, Nintendo layout: X HUD, B BACK (P6.32 phase 7)",
                ref_from=f"{_v}-map-help", own=_HELP_HUD, cfg=_NIN_CFG,
                env=_NIN_ENV + (("DW3_MAP_HELP_SWITCH", "1"),))
    _swg = _help(f"{_v}-map-help-sw-glyphs", f"{_v}-nintendo-map-help",
                 _help_expect("en", "nin-glyph") + ((0, r"dw3-options: switch_glyphs=on"),),
                 "the Switch's help line with glyphs (aid), Nintendo layout: L R ZL ZR X B glyphs (P6.32 phase 7)",
                 ref_from=f"{_v}-map-help", own=_HELP_HUD, cfg=_NIN_CFG + (("switch_glyphs", "1"),),
                 env=_NIN_ENV + (("DW3_MAP_HELP_SWITCH", "1"),))
    _swc = _help(f"{_v}-map-help-sw-glyphs-custom", f"{_v}-custom-map-help",
                 _help_expect("en", "custom-glyph") + ((0, r"dw3-options: switch_glyphs=on"),),
                 "the Switch's help line with glyphs (aid), custom map: ZL ZR in the zoom pair (P6.32 phase 7)",
                 ref_from=f"{_v}-map-help", own=_HELP_HUD,
                 cfg=(("button_layout", "custom"), ("button_map", _HELP_CUSTOM), ("switch_glyphs", "1")),
                 env=_NIN_ENV + (("DW3_MAP_HELP_SWITCH", "1"),))
    for _sc in (_pl, _nin, _cus, _gpc, _sw, _swg, _swc):
        _add(_sc)

# EU: the words of the four other languages (the text file and language of the map view are
# the game's: DW3_LANGUAGE 3 fr, 4 it, 5 de, 6 es). Own references: the area name box is in the
# language too. fr and de also with the glyphs (aid), as variants of their own plain run.
for _lang, _id, _kind, _gkind in (("fr", 3, "ps-pc", "nin-glyph"), ("de", 5, "nin-pc", "custom-glyph"),
                                  ("it", 4, "ps-pc", None), ("es", 6, "ps-pc", None)):
    _e = (("DW3_LANGUAGE", str(_id)),)
    _lay = _HELP_KIND[_kind][0]
    _base = _help(f"eu-map-help-{_lang}", "eu-nintendo-map-help" if _kind == "nin-pc" else "eu-map-help",
                  _help_expect(_lang, _kind),
                  f"the map view's help line in {_lang}, {_lay} layout, PC labels: the five-language words "
                  f"(machine draft, P6.32 phase 7)", env=_e + (_NIN_ENV if _kind == "nin-pc" else ()),
                  cfg=_NIN_CFG if _kind == "nin-pc" else ())
    _add(_base)
    if _gkind:
        _c = {"nin-glyph": _NIN_CFG, "custom-glyph": (("button_layout", "custom"), ("button_map", _HELP_CUSTOM))}[_gkind]
        _ph = "eu-nintendo-map-help" if _gkind == "nin-glyph" else "eu-custom-map-help"
        _add(_help(f"eu-map-help-{_lang}-glyphs", _ph, _help_expect(_lang, _gkind),
                   f"the Switch's help line with glyphs (aid) in {_lang} (P6.32 phase 7)",
                   ref_from=f"eu-map-help-{_lang}", own=_HELP_HUD, env=_e + _NIN_ENV + (("DW3_MAP_HELP_SWITCH", "1"),),
                   cfg=_c + (("switch_glyphs", "1"),)))


# The scenarios that were bit-identical to a plain run by ref_from now show another help
# line when the bars are on, by design (the footer names the buttons of the layout in force):
# only that checkpoint has a reference of its own (own_refs), every other picture is still
# the plain run's, and the log line says what the footer reads.
def _help_amend(name, own, extra):
    for _i, _sc in enumerate(SCENARIOS):
        if _sc.name == name:
            SCENARIOS[_i] = dataclasses.replace(
                _sc, own_refs=own, expect_log=_sc.expect_log + extra,
                expect_log_both=_sc.expect_log_both + extra if _sc.expect_log_both else ())
            return
    raise KeyError(name)


_REB_L1 = "PAD/L-STICK CURSOR  R-STICK/%s ZOOM  L1/R1 SCALER"
for _v in ("us", "eu"):
    # the plain scenario has no help-line check of its own yet: the PlayStation layout, the
    # six phase 4 bindings (zoom Square / Cross, centre L3): the footer lists them
    _pl = by_name()[f"{_v}-map-rebind"]
    _help_amend(f"{_v}-map-rebind", (), _hl(_REB_L1 % "SQUARE/CROSS", "L3 CENTRE ON YOUR AREA  CIRCLE HUD  TRI BACK",
                                              "playstation", _HELP_PS, "off", "en", "pc"))
    # Nintendo: Cross is made by the Circle position and Triangle by the Cross position, so the
    # footer names CIRCLE for zoom in, TRI for the bars and CROSS for back
    _help_amend(f"{_v}-nintendo-map-rebind", ("rb-hud",),
                _hl(_REB_L1 % "SQUARE/CIRCLE", "L3 CENTRE ON YOUR AREA  TRI HUD  CROSS BACK",
                    "nintendo", _HELP_NIN, "off", "en", "pc"))
    # custom 0127456389 (the Square and R2 positions swapped): zoom out Square is made by R2
    _help_amend(f"{_v}-custom-map-rebind", ("rb-hud",),
                _hl(_REB_L1 % "R2/CROSS", "L3 CENTRE ON YOUR AREA  CIRCLE HUD  TRI BACK",
                    "custom", "0127456389", "off", "en", "pc"))
    # the Nintendo map view: the bars checkpoint (default bindings)
    _help_amend(f"{_v}-nintendo-map-view", ("mv-hud-on",),
                _hl(_EN1 % ("L1/R1", "L2/R2"), "SELECT CENTRE ON YOUR AREA  TRI HUD  CROSS BACK",
                    "nintendo", _HELP_NIN, "off", "en", "pc"))


# P6.34 phase 4 (patch 0193) turned "use item stays" on by default. Every
# recording that walks the item screen's original use flow (Cross on a partner,
# Cross to leave the result, back to the list) keeps that flow with
# items_use_stays=0: their references are the PS1 frames, and the key-off proof.
_USE_FLOW_RECS = ("items-screen", "menu-screens", "items-equip-switch", "items-equip-stays",
                  "items-bag-view", "map-screen", "map-view", "map-names", "map-zoom", "map-rebind",
                  "map-help")
for _i, _s in enumerate(SCENARIOS):
    if (any(r in p for p in _s.phases for r in _USE_FLOW_RECS)
            and not any(k == "items_use_stays" for k, _ in _s.cfg)):
        SCENARIOS[_i] = dataclasses.replace(_s, cfg=_s.cfg + (("items_use_stays", "0"),))


# P10.6 Switch glyphs (docs/P10.6_GLYPHS.md, port/options/switch_glyphs.c). The
# census found the button icons in the item shop (circle, triangle, the US cross is
# a multiplication sign there) and the save screen (the EU Cross disc and the
# triangle), so these two forced menus are replayed with `switch_glyphs=1` in the
# seeded port_options.cfg (the PC has no row: the key is the switch). The input
# bypasses the button map, so the layout only changes the glyph letters.
#  glyphs-off     the key written as 0: bit-identical to the plain scenario (its
#                 references, ref_from) and no `dw3-glyphs: on` line
#  glyphs-on-*    the PlayStation layout (B Cross, A Circle, Y Square, X Triangle)
#  glyphs-nintendo-shop  button_layout=nintendo (A, X, Y, B)
#  eu ... -fr/-de the same shop with the EU text in two other languages
# The references are new (the glyphs are different pictures); run with --update.
# Page-turn prompt (patch 0196, P10.6 follow-up): the "R1" / "L1" letters of the item
# shop's and the field menu's item list are drawn as the glyph of the Switch button
# that produces L1 / R1 (the shop and menu-screens cases above are the same walks):
#  glyphs-on-items       menu-screens (item list pages 1 and 2) with the glyphs on
#  glyphs-off-items      the same with switch_glyphs=0: bit-identical to menu-screens
#  glyphs-custom-shop    button_layout=custom, L1 on ZL and R1 on ZR (two-letter glyphs)
_GLY_CELLS = {
    ("us", "playstation"): r"patched 3 cells \(us, n=27:orig n=47:A n=50:Y n=52:X\)",
    ("us", "nintendo"): r"patched 3 cells \(us, n=27:orig n=47:X n=50:Y n=52:B\)",
    ("eu", "playstation"): r"patched 4 cells \(eu, n=27:B n=47:A n=50:Y n=52:X\)",
    ("eu", "nintendo"): r"patched 4 cells \(eu, n=27:A n=47:X n=50:Y n=52:B\)",
}


def _glyphs(base_name, name, layout=None, env=(), note="", cfg_extra=(), pt="L1=L R1=R",
            use=None):
    """`use`: a regex that must follow `dw3-glyphs: page-turn prompt ` (the prompt was drawn).
    `layout` None is the default layout, which is Nintendo since D2 (#408); the scenario then
    pins nothing, and a custom map (cfg_extra) looks up the PlayStation cells."""
    b = by_name()[base_name]
    v = b.version
    cfg = b.cfg + (("switch_glyphs", "1"),) + ((("button_layout", "nintendo"),)
                                                if layout == "nintendo" else ()) + cfg_extra
    eff = layout or "nintendo"
    map_name = "custom" if ("button_layout", "custom") in cfg_extra else eff
    cells = _GLY_CELLS[(v, "playstation" if map_name == "custom" else eff)]
    expect = (
        (0, rf"dw3-options: switch_glyphs=on(?s:.*)dw3-glyphs: on, map={map_name}, "
            + cells + r" \[font load\]"),
        (0, rf"dw3-glyphs: page-turn {pt} \(cells n=115 n=116\) \[font load\]"),
    )
    if use:
        expect += ((0, rf"dw3-glyphs: page-turn prompt {use}"),)
    return dataclasses.replace(
        b, name=name, cfg=cfg, env=b.env + env,
        expect_log=tuple(e for e in b.expect_log if e[0] == 0) + expect,
        forbid_log=((0, r"dw3-glyphs: .*(cell is not a 12x12|not a role colour|"
                        r"size-1 icon table has)"),),
        note=note)


for _v in ("us", "eu"):
    _b = by_name()[f"{_v}-item-shop"]
    _add(
        dataclasses.replace(
            _b, name=f"{_v}-glyphs-off",
            cfg=_b.cfg + (("switch_glyphs", "0"),), ref_from=f"{_v}-item-shop",
            forbid_log=((0, r"dw3-glyphs: (on|page-turn)"),),
            expect_log=tuple(e for e in _b.expect_log if e[0] == 0)
            + ((0, r"dw3-options: switch_glyphs=off(?s:.*)dw3-glyphs: off, map=nintendo, patched 0 cells"),),
            note="switch_glyphs=0: the item shop is bit-identical to the plain run (P10.6)"),
        _glyphs(f"{_v}-item-shop", f"{_v}-glyphs-on-shop", use=r"R1 drawn as glyph R \(scene=STITSHOP",
                note="Switch glyphs on, the default (Nintendo, since D2) layout: the item shop's "
                     "Circle prompt is an X glyph (US also keeps its multiplication sign) and "
                     "its page-turn prompt R1 an R glyph (P10.6)"),
        _glyphs(f"{_v}-item-shop", f"{_v}-glyphs-nintendo-shop", layout="nintendo",
                use=r"R1 drawn as glyph R \(scene=STITSHOP",
                note="Switch glyphs on, Nintendo layout: the Circle prompt is an X glyph, the "
                     "page-turn prompt an R glyph (P10.6)"),
        _glyphs(f"{_v}-item-shop", f"{_v}-glyphs-custom-shop", layout="playstation",
                cfg_extra=(("button_layout", "custom"), ("button_map", "0123674589")),
                pt="L1=ZL R1=ZR", use=r"R1 drawn as glyph ZR \(scene=STITSHOP",
                note="Switch glyphs on, custom map with L1 on ZL and R1 on ZR: the page-turn "
                     "prompt is a ZR glyph (P10.6 page-turn)"),
        _glyphs(f"{_v}-save-screen", f"{_v}-glyphs-on-save",
                note="Switch glyphs on, the default (Nintendo) layout: the save screen's Back "
                     "prompt is a B glyph and its Save prompt (the Cross: the EU disc, the US "
                     "letter X since bug 4) an A glyph (P10.6)"),
        _glyphs(f"{_v}-save-screen", f"{_v}-glyphs-nintendo-save", layout="nintendo",
                note="Switch glyphs on, Nintendo layout: the save screen's Save prompt is an A "
                     "glyph, its Back prompt a B glyph, on both discs (bug 4)"),
    )
# Bug 4 (p56 OLED): the Load Game screen with a save on the card. The game saves from
# the forced save screen (phase 0, as xr-save), then the title's Load Game opens the slot
# list with the footer prompts (phase 1: the slot choice and the list are the checkpoints).
#  load-saved             the plain screen (the references of the glyph cases below)
#  glyphs-off-load        switch_glyphs=0: bit-identical to load-saved
#  glyphs-on-load         glyphs on, PlayStation layout
#  glyphs-nintendo-load   glyphs on, button_layout=nintendo (the case of the bug)
for _v, _ids in (("us", (0x158, 0x1E3, 0x1C1)), ("eu", (0x166, 0x1EE, 0x1D0))):
    _lf, _lm, _lt = _ids
    _add(Scenario(f"{_v}-load-saved", _v, "full", timeout=600,
                  phases=(f"{_v}-xr-save", f"{_v}-load-view"),
                  phase_env=(_SAVE_ENV, ()),
                  expect_log=(_XR_WRITE,),
                  forbid_log=_XR_BAD,
                  note="the Load Game screen with a saved game: the slot list and its "
                       "confirm / back prompts (bug 4)"))


def _glyphs_load(v, name, layout, glyphs, note, env=()):
    b = by_name()[f"{v}-load-saved"]
    cfg = b.cfg + (("switch_glyphs", "1" if glyphs else "0"),)
    if layout == "nintendo":
        cfg += (("button_layout", "nintendo"),)
    eff = layout or "nintendo"      # None: the default layout, Nintendo since D2 (#408)
    if glyphs:
        expect = ((1, rf"dw3-options: switch_glyphs=on(?s:.*)dw3-glyphs: on, map={eff}, "
                      + _GLY_CELLS[(v, eff)] + r" \[font load\]"),)
        forbid = ((1, r"dw3-glyphs: .*(cell is not a 12x12|not a role colour|size-1 icon table has)"),)
    else:
        expect = ((1, r"dw3-options: switch_glyphs=off(?s:.*)dw3-glyphs: off, map=nintendo, "
                      r"patched 0 cells"),)
        forbid = ((1, r"dw3-glyphs: (on|page-turn)"),)
    return dataclasses.replace(b, name=name, cfg=cfg, env=b.env + env,
                               expect_log=b.expect_log + expect,
                               forbid_log=b.forbid_log + forbid,
                               ref_from=f"{v}-load-saved" if not glyphs else "", note=note)


for _v in ("us", "eu"):
    _add(
        _glyphs_load(_v, f"{_v}-glyphs-off-load", None, False,
                     "switch_glyphs=0: the Load Game screen is bit-identical to the plain run (bug 4)"),
        _glyphs_load(_v, f"{_v}-glyphs-on-load", None, True,
                     "Switch glyphs on, the default (Nintendo) layout: the Load Game screen's "
                     "confirm prompt is an A glyph, its back prompt a B glyph (bug 4)"),
        _glyphs_load(_v, f"{_v}-glyphs-nintendo-load", "nintendo", True,
                     "Switch glyphs on, Nintendo layout: the Load Game screen's confirm prompt is "
                     "an A glyph, its back prompt a B glyph (bug 4)"),
    )
# The EU disc draws the Load prompts with the n = 27 icon, the same cell in the five
# languages: the Nintendo layout case in the four other languages (DW3_LANGUAGE).
for _lang, _id in (("fr", 3), ("it", 4), ("de", 5), ("es", 6)):
    _add(_glyphs_load("eu", f"eu-glyphs-nintendo-load-{_lang}", "nintendo", True,
                      f"Switch glyphs on, Nintendo layout, EU {_lang}: the Load Game screen's "
                      "confirm prompt is an A glyph, its back prompt a B glyph (bug 4)",
                      env=(("DW3_LANGUAGE", str(_id)),)))
# The field menu's item list (menu-screens: page 1, then page 2 with both prompts). The
# base has no log expectation of its own (log_only is off, it is a picture scenario).
for _v in ("us", "eu"):
    _m = by_name()[f"{_v}-menu-screens"]
    _add(
        _glyphs(f"{_v}-menu-screens", f"{_v}-glyphs-on-items", use=r"R1 drawn as glyph R \(scene=STSTATUS",
                note="Switch glyphs on: the field menu's item list shows R1 and, on page 2, L1 as "
                     "R and L glyphs (P10.6 page-turn)"),
        dataclasses.replace(
            _m, name=f"{_v}-glyphs-off-items", cfg=_m.cfg + (("switch_glyphs", "0"),),
            ref_from=f"{_v}-menu-screens",
            forbid_log=_m.forbid_log + ((0, r"dw3-glyphs: (on|page-turn)"),),
            note="switch_glyphs=0: the field menu's item list is bit-identical to the plain run "
                 "(P10.6 page-turn)"),
    )
for _lang, _id in (("fr", 3), ("de", 5)):
    _add(_glyphs("eu-item-shop", f"eu-glyphs-on-shop-{_lang}", env=(("DW3_LANGUAGE", str(_id)),),
                 note=f"Switch glyphs on, the EU item shop in {_lang} (P10.6)"))


# --- D2 + D5: what the new defaults mean for the older scenarios ----------------------
# (docs/P6.15_REPLAY_TESTS.md, "D2 + D5 reference refresh")
#
# D5, new scenarios first (they copy the older ones before the pins below):
#  map-scaler-default   the map view with the default scaler (FSR 1): opens on NEAREST at once and the
#                       worker thread (DW3_MAP_SYNC: the same call) swaps AMD FSR 1 in
#  map-scaler-lanczos2  `map_scaler=lanczos2` in the file is the scaler of the open (the Display row's key)
#  map-view-mod-fsr     with a map pack the default scaler (FSR 1) picks the linear GPU filter, no CPU build
# They run the recordings of map-help / map-view and have references of their own.
_FSR_NAMES = set()
for _v in ("us", "eu"):
    _h = by_name()[f"{_v}-map-help"]
    _m = by_name()[f"{_v}-map-view-mod"]
    _open = (r"dw3-map: scaler for this open: %s \(Options > Display > Map scaler, map_scaler=%s, first open\)")
    for _sc in (
        dataclasses.replace(
            _h, name=f"{_v}-map-scaler-default", cfg=tuple(c for c in _h.cfg),
            expect_log=((0, _open % (r"AMD FSR 1", "fsr1") + r"(?s:.*)"
                            r"dw3-map: opens on NEAREST while AMD FSR 1 builds on the worker thread \(no hitch\)(?s:.*)"
                            r"dw3-map: scaler AMD FSR 1 built in \d+ ms"),),
            forbid_log=((0, r"dw3-map: scaler \S+ FAILED"),), ref_from="", own_refs=(),
            note="Map scaler default (D5): the view opens on Nearest and FSR 1 replaces it"),
        dataclasses.replace(
            _h, name=f"{_v}-map-scaler-lanczos2", cfg=_h.cfg + (("map_scaler", "lanczos2"),),
            expect_log=((0, _open % (r"LANCZOS2", "lanczos2") + r"(?s:.*)"
                            r"dw3-map: opens on NEAREST while LANCZOS2 builds on the worker thread(?s:.*)"
                            r"dw3-map: scaler LANCZOS2 built in \d+ ms"),),
            forbid_log=((0, r"dw3-map: scaler \S+ FAILED"),), ref_from="", own_refs=(),
            note="map_scaler=lanczos2 (the Display > Map scaler row's key) is the scaler of the open (D5)"),
        dataclasses.replace(
            _m, name=f"{_v}-map-view-mod-fsr",
            expect_log=_m.expect_log[:1] + (
                (0, r"dw3-map: texture \d+x\d+ queued for the GPU \(gen \d+, scaler AMD FSR 1, linear\)"),),
            forbid_log=_m.forbid_log + ((0, r"dw3-map: scaler AMD FSR 1 requested"),), ref_from="", own_refs=(),
            note="Map mod pack with the default scaler (D5): FSR 1 means the linear GPU filter, no CPU build"),
    ):
        _add(_sc)
        _FSR_NAMES.add(_sc.name)

_PS_NAME = re.compile(r"^(us|eu)-(help-(title|field|battle|fr|de|it|es)|map-help(-glyphs-pc|-fr|-it|-es)?|"
                      r"map-rebind(-relaunch)?|controls-layout|controls-custom)$")


def _pin_cfg(sc, *pairs):
    have = {k for k, _ in sc.cfg}
    return dataclasses.replace(sc, cfg=sc.cfg + tuple(p for p in pairs if p[0] not in have))


for _i, _s in enumerate(SCENARIOS):
    _plain = dict(_s.cfg)
    _ps_log = any("layout playstation" in r or "layout=playstation" in r
                  for _, r in _s.expect_log + _s.expect_log_both)
    _pins = ()
    # D5: the older map scenarios are about the view's mechanics (bindings, help line, zoom, the
    # scaler cycle starting on Nearest), so they keep Nearest as their scaler.
    if _plain.get("map_screen") == "1" and _s.name not in _FSR_NAMES:
        _pins += (("map_scaler", "nearest"),)
    # D2: the scenarios about the PlayStation layout (its help tokens, its map-help line, the
    # Button layout flow that starts on PlayStation) say so in their file; the marker comes with it.
    if (_PS_NAME.match(_s.name) or _ps_log) and not _s.legacy_cfg and "button_layout" not in _plain \
            and "-defaults-migrate" not in _s.name:
        _pins += (("button_layout", "playstation"),)
    if _pins:
        SCENARIOS[_i] = _pin_cfg(_s, *_pins)

# The Display group has five rows since D5 (Map scaler), six since P6.7 (Text language), eight since P6.11
# (Internal resolution, Native res while turbo), and the layout flows start from the file now.
for _i, _s in enumerate(SCENARIOS):
    _rep = lambda es: tuple((p, re.sub(r"group open display \\\(\d row\\\(s\\\)", r"group open display \\(8 row\\(s\\)", r))
                            for p, r in es)
    _new = dataclasses.replace(_s, expect_log=_rep(_s.expect_log), expect_log_both=_rep(_s.expect_log_both))
    if _new.name.endswith(("-controls-layout", "-controls-custom")):
        _fix = lambda es: tuple((p, r.replace("source=default", "source=cfg").replace(
            r"reset to default \(layout=playstation\)", r"reset to default \(layout=nintendo\)")) for p, r in es)
        _new = dataclasses.replace(_new, expect_log=_fix(_new.expect_log), expect_log_both=_fix(_new.expect_log_both))
    if _new.name.endswith("-controls-custom-invalid"):
        _fix = lambda es: tuple((p, r.replace("layout=playstation source=default", "layout=nintendo source=default"))
                                for p, r in es)
        _new = dataclasses.replace(_new, expect_log=_fix(_new.expect_log), expect_log_both=_fix(_new.expect_log_both))
    SCENARIOS[_i] = _new


# --- P6.11 internal resolution (docs/P6.11_INTERNAL_RES.md section 5) -----------------------------
# Every scenario above runs at 1x: the runner takes DW3_INTERNAL_RES, DW3_INTERNAL_RES_TURBO and
# DW3_RES_DITHER out of the environment and the replays' option file has no internal_res key, so the
# rows sit at their defaults (1x). The 2x family below is small: the window grab (the .png checkpoint;
# the .ppm PS1 display capture is the nearest downsample of the scaled picture and hides what is
# tested) of the title, the field, a battle, the battle held under Options, the map, the movie, the
# fast-forward switch and the Options rows, on EU, and a US canary of four. PC only (the Switch draws
# the mesh scale on the GPU, which Ryubing's timing and fps cannot judge; its rows are OLED R1-R8).
_R2 = (("DW3_INTERNAL_RES", "2"),)
_R2_FIXED = _R2 + (("DW3_INTERNAL_RES_TURBO", "0"),)   # the recordings that run at turbo 4 stay at 2x
_SW_CFG = (("fast_forward", "4"),)                       # R3 in the recording turns turbo on and off


def _res(base, name, note, **kw):
    """A copy of scenario `base` under a new name (own references), full tier."""
    b = by_name()[base]
    return dataclasses.replace(b, name=name, tier="full", ref_from="", ref_map=(), own_refs=(),
                               note=note, **kw)


for _v in ("us", "eu"):
    _t = _res(f"{_v}-title" if _v == "us" else "eu-lang-en", f"{_v}-res2x-title",
              "2x: the title and its native overlays do not depend on the setting (P6.11)")
    _add(dataclasses.replace(
        _t, env=_t.env + _R2,
        expect_log=_t.expect_log + ((0, r"dw3-res: set 2x \(pc\)"),),
        forbid_log=_t.forbid_log + ((0, r"dw3-res: clamp"), (0, r"PSY-Z refused"))))
    _b = by_name()[f"{_v}-two-battles"]
    _add(_res(f"{_v}-two-battles", f"{_v}-res2x-two-battles",
              "2x: 3D at 2x in two forced battles, widescreen gutters included; fast-forward 4 with "
              "Native res while turbo OFF keeps 2x (P6.11)",
              env=_b.env + _R2_FIXED,
              expect_log=_b.expect_log + ((0, r"dw3-res: set 2x \(pc\)"),),
              forbid_log=_b.forbid_log + ((0, r"dw3-res: turbo (?:on|off)"), (0, r"dw3-res: clamp"),
                                          (0, r"PSY-Z refused"))))
    _add(Scenario(f"{_v}-res2x-turbo", _v, "full", timeout=600, cfg=_SW_CFG,
                  phases=(f"{_v}-res-switch",),
                  env=_b.env + _R2,
                  expect_restore=_b.expect_restore,
                  expect_log=((0, r"dw3-res: set 2x \(pc\)(?s:.*)dw3-turbo: on(?s:.*)"
                                  r"dw3-res: turbo on -> 1x(?s:.*)dw3-turbo: off(?s:.*)"
                                  r"dw3-res: turbo off -> 2x(?s:.*)"
                                  r"dw3-res: turbo on -> 1x(?s:.*)dw3-res: turbo off -> 2x"),),
                  forbid_log=((0, r"PSY-Z refused"), (0, r"dw3-res: clamp")),
                  note="2x: R3 turns fast-forward on and off in both battles: turbo on -> 1x, turbo off -> 2x "
                       "(one reserved target, no re-allocation); the captures after the switch are the steady "
                       "2x run's (P6.11 phase 3)"))

_uf = by_name()["eu-underground-exit"]
_add(_res("eu-underground-exit", "eu-res2x-field",
          "2x: the field (2D) is the same picture at 2x, gutters and the canal water included (P6.11)",
          env=_uf.env + _R2, expect_log=_uf.expect_log + ((0, r"dw3-res: set 2x \(pc\)"),)))
_bo = by_name()["eu-battle-options"]
_add(_res("eu-battle-options", "eu-res2x-battle-held",
          "2x: the Options screen over a battle (P9.7): the held picture is the 2x one, Music volume changed "
          "while held (P6.11)", env=_bo.env + _R2_FIXED,
          expect_log=_bo.expect_log + ((0, r"dw3-res: set 2x \(pc\)"),)))
_mv = by_name()["eu-map-view"]
_add(_res("eu-map-view", "eu-res2x-map",
          "2x: the full-screen map (output resolution) over a 2x game (P6.11)",
          env=_mv.env + _R2, expect_log=_mv.expect_log + ((0, r"dw3-res: set 2x \(pc\)"),)))
_add(Scenario("eu-res2x-movie", "eu", "full", turbo=4, timeout=900, phases=("eu-res-movie",),
              env=_R2, log_only=False,
              expect_log=((0, r"dw3-res: set 2x \(pc\)"),),
              forbid_log=((0, r"PSY-Z refused"),),
              note="2x: a frame of the opening movie is unscaled (P6.11)"))
_add(Scenario("eu-res2x-row", "eu", "full", timeout=300,
              phases=("eu-options-res-p1", "eu-options-res-p2"),
              expect_log=((0, r"dw3-port-options: internal resolution 2x(?s:.*)"
                              r"dw3-res: set 2x \(pc\)(?s:.*)"
                              r"dw3-port-options: native res while turbo disabled"),
                          (1, r"dw3-options: file keys .* internal_res=1 internal_res_turbo=0(?s:.*)"
                              r"dw3-res: set 2x \(pc\)(?s:.*)"
                              r"dw3-port-options: internal resolution 3x(?s:.*)"
                              r"dw3-res: set 3x \(pc\)(?s:.*)"
                              r"dw3-port-options: internal resolution 1x")),
              forbid_log=((0, r"PSY-Z refused"), (1, r"PSY-Z refused")),
              note="Options > Display: Internal resolution 1x -> 2x and Native res while turbo ON -> OFF are "
                   "written (internal_res=1, internal_res_turbo=0) and read back by the next process, which "
                   "steps 2x -> 3x -> 1x live (P6.11 phase 1)"))
# The experiment this replaces: DW3_PC_INTERNAL_RES=3 widened the title's background and set a 1280x720 window.
_t3 = by_name()["us-title"]
_add(_res("us-title", "us-res3x-title",
          "3x: the title background is widened and the window is 16:9, as the old DW3_PC_INTERNAL_RES=3 did (P6.11)",
          env=_t3.env + (("DW3_INTERNAL_RES", "3"),),
          expect_log=_t3.expect_log + ((0, r"dw3-res: set 3x \(pc\)"),)))

# P6.7 phase 1 (docs/LOCALIZATION.md section 3.7): the text pack on the EU disc. Every scenario runs
# on a private runtime (Scenario.l10n) whose pack is built from the disc's own English text with a
# visible transform, so no translation is needed: "upper" upper-cases every string and adds
# ã õ ç. Text language is set through the option file (text_language=ptbr), so the key's reader
# is part of the test. The recordings are the ones of the plain scenarios (title and Load Game,
# the opening's dialogue, the battle menus, the Digimon Lab); the references are local, like all
# the others, and never committed. PC only (Scenario.l10n).
_L10N_CFG = (("text_language", "ptbr"),)
_L10N_OK = r"dw3-l10n: pack ok region=eu files=50 strings=\d+ translated=\d+ fallback=\d+ src=sha256:[0-9a-f]{16}"
_L10N_NONE = (r"dw3-l10n: off reason=",)
_L10N_ON = ((0, _L10N_OK + r" synthetic=upper(?s:.*)dw3-l10n: option text_language=ptbr region=eu(?s:.*)"
                r"dw3-l10n: override id=\d+ stem=\w+ bytes=\d+ lba=0x1000000 sectors=\d+ \(was \d+\)"),)
_L10N_OFF_BAD = ((0, r"dw3-l10n: off reason=(?!option_original)"),)
_L10N_ENV_EU_LAB = _CARD_ENV + (("DW3_FORCE_MENU", "0xD00"),)
_add(
    Scenario("eu-l10n-title", "eu", "full", timeout=300, phases=("eu-load-menu",), l10n="upper", cfg=_L10N_CFG,
             expect_log=_L10N_ON, forbid_log=_L10N_OFF_BAD,
             note="Title, Continue on an empty card: the card messages (MEMCRD) read from the pack"),
    Scenario("eu-l10n-field-dialogue", "eu", "full", turbo=4, timeout=900, phases=("eu-newgame-field",),
             expect_restore=EU_OPENING, l10n="upper", cfg=_L10N_CFG,
             expect_log=_L10N_ON, forbid_log=_L10N_OFF_BAD,
             note="New Game: the name entry, the opening's dialogue and the first field, in the pack's text"),
    Scenario("eu-l10n-battle-menu", "eu", "full", timeout=300, phases=("eu-battle-list-bar",), env=_BAR_ENV,
             l10n="upper", cfg=_L10N_CFG, expect_log=_L10N_ON + _BAR_LOG, forbid_log=_L10N_OFF_BAD + _BAR_FORBID,
             note="A battle: the command menu, the Items and the technique lists, in the pack's text"),
    Scenario("eu-l10n-digimon-lab", "eu", "full", timeout=400, phases=("eu-digimon-lab",), env=_L10N_ENV_EU_LAB,
             expect_restore=((0x166, M), (0x1EC, M)), l10n="upper", cfg=_L10N_CFG,
             expect_log=_L10N_ON, forbid_log=_L10N_OFF_BAD,
             note="The Digimon Lab: the menu, the party and its details, in the pack's text"),
    Scenario("eu-l10n-fallback", "eu", "full", timeout=300, phases=("eu-load-menu",), l10n="partial", cfg=_L10N_CFG,
             expect_log=((0, r"dw3-l10n: pack ok region=eu files=50 strings=13084 translated=\d+ fallback=[1-9]\d* "
                             r"src=sha256:[0-9a-f]{16} synthetic=partial(?s:.*)"
                             r"dw3-l10n: fallback key=[A-Z0-9]+:\d+ \(empty pt\)(?s:.*)"
                             r"dw3-l10n: fallback total=[1-9]\d* of 13084"),),
             forbid_log=_L10N_OFF_BAD,
             note="A half-translated pack: the strings it lacks stay in English (the fallback path)"),
    Scenario("eu-l10n-nopack", "eu", "full", timeout=300, phases=("eu-load-menu",), l10n="none", cfg=_L10N_CFG,
             ref_from="eu-load-menu",
             expect_log=((0, r"dw3-l10n: off reason=no_pack"),),
             forbid_log=((0, r"dw3-l10n: off reason=no_pack(?s:.*)dw3-l10n: off reason="),
                         (0, r"dw3-l10n: (override|pack ok)"), (0, r"CRASH|Segmentation fault")),
             note="text_language=ptbr with no pack: English, exactly one off line, the original pictures"),
    Scenario("eu-l10n-glyph-proof", "eu", "full", timeout=300, phases=("eu-load-menu",), l10n="proof", cfg=_L10N_CFG,
             expect_log=((0, r"dw3-l10n: pack ok region=eu files=50 strings=13084 translated=\d+ fallback=\d+ "
                             r"src=sha256:[0-9a-f]{16} synthetic=proof(?s:.*)dw3-l10n: glyph proof n=29 "),),
             forbid_log=_L10N_OFF_BAD,
             note="the glyph proof: the 26 accented letters PT-BR needs and the marks (Ã Õ Ê Ô included) in the game's own font"),
    Scenario("eu-l10n-show-keys", "eu", "full", timeout=300, phases=("eu-load-menu",), l10n="upper+keys", cfg=_L10N_CFG,
             env=(("DW3_L10N_SHOW_KEYS", "1"),),
             expect_log=((0, r"dw3-l10n: override id=\d+ stem=\w+ bytes=\d+ lba=0x1000000 sectors=\d+ \(was \d+\) keys"),),
             forbid_log=_L10N_OFF_BAD + ((0, r"show keys: this pack has no key variants"),),
             note="DW3_L10N_SHOW_KEYS=1: every string starts with its key, \"(MEMCRD:3) \""),
    Scenario("eu-l10n-row", "eu", "full", timeout=300, l10n="upper",
             expect_log=((0, r"dw3-port-options: group open display \(6 row\(s\), title\)(?s:.*)"
                             r"dw3-port-options: text language ptbr(?s:.*)"
                             r"dw3-l10n: option text_language=ptbr region=eu(?s:.*)"
                             r"dw3-l10n: override id=\d+(?s:.*)"
                             r"dw3-port-options: text language original(?s:.*)"
                             r"dw3-l10n: restored 50 file\(s\) to the original text(?s:.*)"
                             r"dw3-l10n: off reason=option_original"),),
             forbid_log=_NO_BAD + ((0, r"dw3-l10n: off reason=(?!option_original)"),),
             note="Options > Display > Text language: Original, Right to Portugues (BR) applies the pack, Left restores it"),
    Scenario("eu-l10n-row-nopack", "eu", "full", timeout=300, l10n="none",
             expect_log=((0, r"dw3-l10n: off reason=no_pack(?s:.*)"
                             r"dw3-port-options: text language ptbr refused: no usable text pack \(no_pack\)"),),
             forbid_log=_NO_BAD + ((0, r"dw3-l10n: (override|pack ok)"), (0, r"dw3-port-options: text language ptbr\n")),
             note="With no pack the Text language row is greyed (message: no text pack found) and Right is refused"),
    Scenario("us-l10n-row-nopack", "us", "full", timeout=300, l10n="none",
             expect_log=((0, r"dw3-l10n: off reason=no_pack(?s:.*)"
                             r"dw3-port-options: text language ptbr refused: no usable text pack \(no_pack\)"),),
             forbid_log=_NO_BAD + ((0, r"dw3-l10n: (override|pack ok)"),),
             note="The same on the US disc: the row is there and greyed until phase 5 brings the US pack"),
    Scenario("eu-l10n-original", "eu", "full", timeout=300, phases=("eu-load-menu",), l10n="upper",
             ref_from="eu-load-menu",
             expect_log=((0, r"dw3-l10n: pack ok region=eu(?s:.*)dw3-l10n: off reason=option_original"),),
             forbid_log=((0, r"dw3-l10n: override"),),
             note="a pack is installed and the option is Original: English, the original pictures"),
)

# P6.7 phase 2, the measuring pass (docs/LOCALIZATION.md section 7a): the same recordings with a
# "stress" pack, every line as wide as the widest of the other four EU languages at that index
# (the longest a translator was allowed), to see which windows do not hold it. Tier "repro": only
# with --only (they are a look, with local references, not a gate). The windows: the message
# and talk boxes (the opening), the battle menu and its lists, the battle report, the Digimon
# Lab, the item shop, the card shop and deck editor, the card album, the gym, the field menu's
# screens, the Items menu, the save and the load screens.
_FIT_BASES = (("eu-newgame-field", "dialogue"), ("eu-battle-list-bar", "battle-menu"),
              ("eu-battle-results", "battle-results"), ("eu-digimon-lab", "lab"),
              ("eu-lab-digivolve", "lab-digivolve"), ("eu-item-shop", "item-shop"),
              ("eu-card-shop", "card-shop"), ("eu-deck-editor", "deck-editor"),
              ("eu-card-album", "card-album"), ("eu-gym", "gym"), ("eu-menu-screens", "menu-screens"),
              ("eu-items-menu", "items-menu"), ("eu-save-screen", "save-screen"),
              ("eu-load-menu", "load-menu"), ("eu-map-screen", "map-screen"))
_by = {_s.name: _s for _s in SCENARIOS}
for _base, _label in _FIT_BASES:
    _b = _by[_base]
    _add(dataclasses.replace(
        _b, name=f"eu-l10n-fit-{_label}", tier="repro", l10n="stress", ref_from="", own_refs=(), ref_map=(),
        cfg=_b.cfg + _L10N_CFG, expect_log=_b.expect_log + tuple((i, r.replace("synthetic=upper", "synthetic=stress")) for i, r in _L10N_ON),
        forbid_log=_L10N_OFF_BAD,  # the longer text can change the timing a base scenario's forbid lines rely on
        note=f"measuring pass: {_base} with the longest PT-like strings (stress pack)"))
