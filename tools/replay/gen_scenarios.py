#!/usr/bin/env python3
"""Generate the P6.15 pad recordings (scenarios/*.rec).

The recordings are plain pad masks keyed by game frame (PadChkVsync count);
they contain no game data. They can equally be produced by hand-playing the
PC build with DW3_REPLAY_RECORD=<file>; this script exists so the five EU
language scenarios stay consistent and reviewable.

Format: see port/psyz_missing/psyz_pad_host.c (pad / shot / end directives).
Raw masks: START 0x0008, UP 0x0010, RIGHT 0x0020, DOWN 0x0040, LEFT 0x0080,
CROSS 0x4000 (diagonals are the OR of two directions).

Gameplay scenarios (P6.15 part 2) replay a long, mostly "mash Cross" input:
the new-game opening is scripted and every box advances on Cross, so a fixed
Cross cadence from the menu reaches the first field with control (the Asuka
City main lobby) after ~16400 game frames on both discs, deterministically.
The frame numbers below were measured on the PC build; a failing checkpoint
there is the signal that the opening changed.
"""
import sys
from pathlib import Path

START, DOWN, CROSS = 0x0008, 0x0040, 0x4000
TRIANGLE = 0x1000
UP, RIGHT, LEFT = 0x0010, 0x0020, 0x0080
EU_LANGS = ["en", "fr", "de", "it", "es"]
HOLD = 5  # frames a button is held; PadChkVsync can miss a 1-frame edge


class Rec:
    def __init__(self, title):
        self.lines = [f"# {title}"]
        self.last = 0

    def press(self, frame, mask):
        self.lines.append(f"pad {frame} 0x{mask:04x}")
        self.lines.append(f"pad {frame + HOLD} 0x0000")

    def shot(self, frame, name):
        self.lines.append(f"shot {frame} {name}")

    def end(self, frame):
        self.lines.append(f"end {frame}")

    def text(self, chronological=False):
        lines = self.lines
        if chronological:  # stable sort by frame; the header comment stays first
            head, body = lines[:1], lines[1:]
            body.sort(key=lambda l: int(l.split()[1]))
            lines = head + body
        return "\n".join(lines) + "\n"


def us_title():
    r = Rec("US: boot, skip splash/opening movie, title, menu")
    r.press(20, START)           # skip the opening FMV
    r.shot(200, "title")
    r.press(210, START)          # PRESS START -> New Game / Options / Load
    r.shot(310, "menu")
    r.end(320)
    return r


def eu_language(idx):
    lang = EU_LANGS[idx]
    r = Rec(f"EU: language select -> {lang}, title, menu")
    r.shot(80, "select-default")
    f = 90
    for _ in range(idx):
        r.press(f, DOWN)
        f += 15
    r.shot(f + 10, "select-cursor")
    c = f + 20
    r.press(c, START)            # confirm language
    for k in (50, 80, 110):      # skip splash/FMV stages
        r.press(c + k, START)
    r.shot(c + 160, "title")
    r.press(c + 170, START)      # PRESS START -> Start / Options / Continue
    r.shot(c + 270, "menu")
    r.end(c + 280)
    return r


class Script:
    """Cursor-based builder for long gameplay recordings."""

    def __init__(self, rec, frame):
        self.r, self.f = rec, frame

    def hold(self, mask, frames):
        self.r.lines.append(f"pad {self.f} 0x{mask:04x}")
        self.f += frames

    def release(self):
        self.r.lines.append(f"pad {self.f} 0x0000")

    def wait(self, frames):
        self.f += frames

    def tap(self, mask):
        self.r.press(self.f, mask)
        self.f += HOLD + 5

    def walk(self, mask, frames):
        self.hold(mask, frames)
        self.release()

    def shot(self, name):
        self.r.shot(self.f, name)


# --- gameplay (P6.15 part 2) ------------------------------------------------

CROSS_PERIOD = 30
INTRO_END = 16400   # last Cross of the opening spam
LOBBY = 16700       # first frame with control in the Asuka City main lobby

# Opening checkpoints (frame, name): all in static or deterministic scenes,
# none during the movie that plays around frame 14400-14900.
OPENING_SHOTS = [
    (2100, "news"), (4600, "online-center"), (6800, "account-clerk"),
    (10000, "name-entry"), (12700, "chamber"), (15700, "login-room-intro"),
]


def new_game_opening(r, version):
    """Boot -> title -> New Game -> the scripted opening, with checkpoints.
    Returns nothing; the lobby (control) starts at LOBBY on both discs."""
    if version == "us":
        r.press(20, START)           # skip the opening FMV
        r.press(210, START)          # PRESS START
        r.press(320, CROSS)          # New Game
    else:
        for f in (110, 160, 190, 220, 280):   # confirm English, skip splash/FMV, PRESS START
            r.press(f, START)
        r.press(390, CROSS)          # Start (New Game)
    f = 900 if version == "eu" else 800
    while f < INTRO_END:
        r.press(f, CROSS)
        f += CROSS_PERIOD
    for frame, name in OPENING_SHOTS:
        r.shot(frame, name)
    r.shot(LOBBY - 10, "lobby")


def door_tour(s):
    """Lobby <-> Login Room through the door pad, twice (4 map transitions),
    with a walk to a gate pod in between. Never walks DOWN in the lobby:
    that route reaches the FIELDSTG_drawBalloon crash (see docs)."""
    s.walk(UP, 240)                  # onto the door pad (arrow icon)
    s.tap(CROSS)                     # lobby -> Login Room
    s.wait(300)
    s.shot("login-room")
    s.walk(DOWN, 120)                # back to the door in the Login Room
    s.tap(CROSS)                     # Login Room -> lobby
    s.wait(310)
    s.shot("lobby-again")
    s.walk(UP, 240)
    s.tap(CROSS)                     # lobby -> Login Room (2nd time)
    s.wait(300)
    s.walk(UP | RIGHT, 240)          # across the walkway to a gate pod
    s.wait(60)
    s.shot("gate-pod")
    s.walk(DOWN | LEFT, 240)         # and back to the door
    s.walk(DOWN, 120)
    s.tap(CROSS)                     # Login Room -> lobby (4th transition)
    s.wait(310)
    s.shot("lobby-end")


def newgame_field(version):
    r = Rec(f"{version.upper()}: New Game -> opening -> control in the first field")
    new_game_opening(r, version)
    s = Script(r, LOBBY)
    s.walk(UP, 60)                   # prove control: the player moves
    s.wait(30)
    s.shot("walked")
    s.wait(30)
    r.end(s.f)
    return r


def field_walk(version):
    r = Rec(f"{version.upper()}: opening, then lobby <-> Login Room door tour")
    new_game_opening(r, version)
    s = Script(r, LOBBY)
    door_tour(s)
    r.end(s.f + 30)
    return r


SOAK_FRAMES = 61000   # ~17 min at 60 Hz (US), ~20 min at 50 Hz (EU)


def soak(version):
    r = Rec(f"{version.upper()}: opening, then door/pod walking for ~{SOAK_FRAMES} frames")
    new_game_opening(r, version)
    s = Script(r, LOBBY)
    n = 0
    while s.f < LOBBY + SOAK_FRAMES:
        s.walk(UP, 240)
        s.tap(CROSS)
        s.wait(300)
        s.walk(UP | RIGHT, 240)
        s.wait(30)
        s.walk(DOWN | LEFT, 240)
        s.walk(DOWN, 120)
        s.tap(CROSS)
        s.wait(310)
        n += 1
        if n % 10 == 0:
            s.shot(f"cycle{n:02d}")
    r.end(s.f + 30)
    return r


def load_menu(version):
    """Title -> Load Game/Continue -> memory card slot choice -> slot 1 on an
    empty card -> "File cannot be found". Exercises the flat-save host and the
    STGMCARD screen on a fresh memory card (the save/load round trip itself
    is not reachable by pad input in the opening, see the docs)."""
    r = Rec(f"{version.upper()}: title, Load Game on an empty card")
    if version == "us":
        r.press(20, START)
        r.press(210, START)
        m = 310                      # title menu is up
    else:
        for f in (110, 160, 190, 220, 280):
            r.press(f, START)
        m = 380
    r.press(m + 10, DOWN)
    r.press(m + 30, DOWN)            # third entry: Load Game / Continue
    r.shot(m + 60, "load-selected")
    r.press(m + 70, CROSS)
    r.shot(m + 400, "slot-choice")
    r.press(m + 420, CROSS)          # slot 1
    r.shot(m + 600, "not-found")
    r.end(m + 620)
    return r


def repro_balloon(version):
    """Regression: walking DOWN/LEFT in the lobby used to reach a null
    dereference in FIELDSTG_drawBalloon (US, game frame ~17400-17440); fixed by
    port patch 0058 (docs/BALLOON_NULL_ACTOR.md)."""
    r = Rec(f"{version.upper()}: lobby walk that used to crash FIELDSTG_drawBalloon (fixed by patch 0058)")
    new_game_opening(r, version)
    s = Script(r, LOBBY - 80)
    s.walk(LEFT, 240)
    s.walk(DOWN | LEFT, 180)
    s.walk(DOWN, 180)
    s.walk(DOWN | RIGHT, 120)
    s.wait(100)
    r.end(s.f + 10)
    return r


# --- bridge partner gate (regression for the FIELDSTG_drawBalloon NULL actor) --

# New Game, the scripted opening (every box advances on Cross), then a walk
# from the first control frame in the Digital World to the bridge tile where
# the hero is stopped ("Oh yeah! I need my partner Digimon data!", WSTAG210
# event 0x1E / script30). That script closes its "!" balloon twice (script
# command 0x323, 0x326); the second close creates a fresh balloon that never
# ran TASK_INIT, so FIELDSTG_drawBalloon read task->actor == NULL and the
# 64-bit hosts faulted (port patch 0058). The route is keyed by game frame, so
# it is a pure function of the pad log like every other scenario here.
BRIDGE_CROSS_END = 16400     # last Cross of the opening spam
BRIDGE_START = 16700         # first walking frame (control since ~16150-16520)
BRIDGE_SHOTS = [(17300, "gate"), (17900, "dialogue"), (18200, "after"), (18600, "walked")]


def _pad(r, frame, mask):
    r.lines.append(f"pad {frame} 0x{mask:04x}")


def bridge_partner_gate(version):
    r = Rec(f"{version.upper()}: opening, walk to the bridge gate, partner dialogue, on")
    if version == "us":
        r.press(20, START)           # skip the opening FMV
        r.press(210, START)          # PRESS START
        r.press(320, CROSS)          # New Game
        f = 800
    else:
        for fr in (110, 160, 190, 220, 280):   # confirm English, skip splash/FMV, PRESS START
            r.press(fr, START)
        r.press(390, CROSS)          # Start (New Game)
        f = 900
    while f < BRIDGE_CROSS_END:
        r.press(f, CROSS)
        f += 30
    # DOWN to the corner, LEFT across the first grating to the stair foot,
    # then down the landing and RIGHT onto the second grating. PAL moves 20%
    # further per frame (speed 0x4CC), so its LEFT is shorter and its landing
    # leg needs a DOWN+RIGHT slide to stay on the narrow path.
    if version == "us":
        route = [(BRIDGE_START, DOWN), (16900, LEFT), (17000, DOWN), (17200, RIGHT)]
        release = 17380
    else:
        route = [(BRIDGE_START, DOWN), (16900, LEFT), (16984, DOWN | RIGHT),
                 (17034, DOWN), (17184, RIGHT)]
        release = 17384
    for frame, mask in route:
        _pad(r, frame, mask)
    _pad(r, release, 0)
    r.press(18010, CROSS)            # close the box
    r.press(18120, CROSS)
    _pad(r, 18400, UP)               # control is back: walk away from the gate
    _pad(r, 18500, 0)
    for frame, name in BRIDGE_SHOTS:
        r.shot(frame, name)
    r.end(18700)
    return r


# Two consecutive battles (overlay restore of the WFIGHTMN sub-overlay per
# battle, patch 0061). The scenario's env boots straight into a forced
# encounter (DW3_START_MODE=0x600, DW3_FORCE_ENCOUNTER=34 Kunemon,
# DW3_FORCE_PARTY=0 Kotemon lead) and DW3_AUTO_DOOR_MODE=0x600 starts the
# second battle 120 field frames after the report returns to the field.
# Each battle opens with a technique (Tech -> first tech -> use: "Hot
# Head!"), then Cross is mashed through the fight, the report and the field.
# The second battle's menu waits for input, so the mash stops just before
# the AUTO_DOOR frame and the second Tech input lands on the idle menu.
# Measured on the PC build (DW3_TURBO=4; the replay is frame-keyed).
TWO_BATTLES = {
    #     door   (mash stops just before it)  tech2  end
    "us": (1360, 1820, 5000),
    "eu": (1290, 1750, 5000),
}


def _tech(r, f):
    _pad(r, f, DOWN)                 # Fight -> Tech
    _pad(r, f + 10, 0)
    r.press(f + 60, CROSS)           # open the tech list
    r.press(f + 110, CROSS)          # use the first tech


def two_battles(version):
    door, tech2, end = TWO_BATTLES[version]
    r = Rec(f"{version.upper()}: two forced battles, a technique in each (env in manifest)")
    _tech(r, 350)
    r.shot(500, "battle1-tech")
    for f in range(520, door - 20, 30):
        r.press(f, CROSS)
    r.shot(tech2 - 20, "battle2-menu")
    _tech(r, tech2)
    r.shot(tech2 + 130, "battle2-tech")
    for f in range(tech2 + 150, end - 100, 30):
        r.press(f, CROSS)
    r.shot(end - 10, "field-after")
    r.end(end)
    return r


def options_persist(version, phase):
    """P9.4: title -> Options. Phase 1 turns Widescreen OFF, Fast-forward
    2x -> 3x and the Restart hotkey ON, then leaves with Triangle; phase 2 is a
    fresh process on the same config dir that only opens the screen, so its
    checkpoint shows the values read back from port_options.cfg."""
    r = Rec(f"{version.upper()}: Options persistence, phase {phase}")
    if version == "us":
        r.press(20, START)           # skip the opening FMV
        r.press(210, START)          # PRESS START
        m = 320                      # title menu is up
    else:
        for f in (110, 160, 190, 220, 280):   # confirm English, skip splash/FMV, PRESS START
            r.press(f, START)
        m = 390
    r.press(m, DOWN)                 # New Game -> Options
    r.press(m + 20, CROSS)           # open the Options screen
    if phase == 1:
        r.shot(m + 60, "options-defaults")
        r.press(m + 70, CROSS)       # Widescreen ON -> OFF
        r.press(m + 90, DOWN)
        r.press(m + 110, CROSS)      # Fast-forward 2x -> 3x
        r.press(m + 130, DOWN)
        r.press(m + 150, CROSS)      # Restart hotkey OFF -> ON
        r.shot(m + 190, "options-changed")
        r.press(m + 200, TRIANGLE)   # leave
        r.end(m + 240)
    else:
        r.shot(m + 60, "options-restored")
        r.press(m + 70, TRIANGLE)
        r.end(m + 110)
    return r


def volume_persist(version, phase):
    """P6.17: title -> Options -> Music volume 10 -> 7 (Left x3) and SFX volume
    10 -> 3 (Left x7), then leave. Phase 2 is a fresh process on the same
    config dir that only opens the screen: its checkpoint and its log show the
    two steps read back from port_options.cfg."""
    r = Rec(f"{version.upper()}: Music / SFX volume persistence, phase {phase}")
    if version == "us":
        r.press(20, START)           # skip the opening FMV
        r.press(210, START)          # PRESS START
        m = 320                      # title menu is up
    else:
        for f in (110, 160, 190, 220, 280):   # confirm English, skip splash/FMV, PRESS START
            r.press(f, START)
        m = 390
    r.press(m, DOWN)                 # New Game -> Options
    r.press(m + 20, CROSS)           # open the Options screen
    if phase == 1:
        r.shot(m + 60, "volume-defaults")
        f = m + 70
        for _ in range(3):           # Widescreen -> Fast-forward -> Restart -> Music
            r.press(f, DOWN)
            f += 20
        for _ in range(3):           # Music volume 10 -> 7
            r.press(f, LEFT)
            f += 20
        r.press(f, DOWN)             # SFX volume
        f += 20
        for _ in range(7):           # SFX volume 10 -> 3
            r.press(f, LEFT)
            f += 20
        r.shot(f + 40, "volume-changed")
        r.press(f + 50, TRIANGLE)    # leave
        r.end(f + 90)
    else:
        r.shot(m + 60, "volume-restored")
        r.press(m + 70, TRIANGLE)
        r.end(m + 110)
    return r


# --- P6.18 pacing options (log-only: no checkpoints, nothing to reference) ---

def _to_title_menu(r, version):
    """Boot to the title menu; returns the frame it is up."""
    if version == "us":
        r.press(20, START)           # skip the opening FMV
        r.press(210, START)          # PRESS START
        return 320
    for f in (110, 160, 190, 220, 280):   # confirm English, skip splash/FMV, PRESS START
        r.press(f, START)
    return 390


def pacing_persist(version, phase):
    """P6.18: Options -> Encounter rate 1x -> 0.5x (Right) and Text speed
    Normal -> Instant (Right x2), then leave. Phase 2 is a fresh process on the
    same config dir; the logs carry the values saved and read back."""
    r = Rec(f"{version.upper()}: Encounter rate / Text speed persistence, phase {phase}")
    m = _to_title_menu(r, version)
    r.press(m, DOWN)                 # New Game -> Options
    r.press(m + 20, CROSS)           # open the Options screen
    if phase == 1:
        f = m + 70
        for _ in range(5):           # Widescreen -> ... -> Encounter rate
            r.press(f, DOWN)
            f += 20
        r.press(f, RIGHT)            # 1x -> 0.5x
        f += 20
        r.press(f, DOWN)             # Text speed
        f += 20
        r.press(f, RIGHT)            # Normal -> Fast
        f += 20
        r.press(f, RIGHT)            # Fast -> Instant
        f += 20
        r.press(f + 20, TRIANGLE)    # leave
        r.end(f + 60)
    else:
        r.press(m + 70, TRIANGLE)
        r.end(m + 110)
    return r


TEXT_END = {"us": 2400, "eu": 2600}   # frames; the first pages are done long before


def text_opening(version, end):
    """P6.18: New Game and then no input at all: the first scripted boxes of
    the opening type out and wait for a button, so the log carries the frames
    a page took (port patch 0063). Run once per Text speed."""
    r = Rec(f"{version.upper()}: New Game, then wait: the first dialogue pages type out")
    if version == "us":
        r.press(20, START)           # skip the opening FMV
        r.press(210, START)          # PRESS START
        r.press(320, CROSS)          # New Game
    else:
        for f in (110, 160, 190, 220, 280):
            r.press(f, START)
        r.press(390, CROSS)          # Start (New Game)
    r.end(end)
    return r


# Encounter rate: DW3_START_MODE puts the player on a South Sector field
# (mode 0x233) with the story at step 6, where the whole map is a random
# battle area, and the recording only walks Up. The frames below were measured
# on the PC build: the walk starts once the stage is up and control is given.
ENCOUNTER = {
    #      walk from, until   (1x battle comes ~100 footsteps in, 0.5x ~200)
    "us": (300, 3300),
    "eu": (300, 3300),
}


def encounter_walk(version):
    start, end = ENCOUNTER[version]
    r = Rec(f"{version.upper()}: walk up a random-battle field for {end - start} frames (env in manifest)")
    _pad(r, start, UP)
    r.end(end)
    return r


def encounter_event(version):
    """Scripted battle with encounters Off: DW3_FORCE_NPC_ACTION=0x7400 runs
    the field action EVENT_BATTLE(0) (FIELDSTG_startEventBattle), the path the
    stages' events, bosses and hidden-spot battles use. No walking."""
    r = Rec(f"{version.upper()}: scripted EVENT_BATTLE with Encounter rate Off (env in manifest)")
    r.end(900)
    return r


def main(out):
    out.mkdir(parents=True, exist_ok=True)
    (out / "us-title.rec").write_text(us_title().text())
    for i, lang in enumerate(EU_LANGS):
        (out / f"eu-lang-{lang}.rec").write_text(eu_language(i).text())
    for v in ("us", "eu"):
        (out / f"{v}-newgame-field.rec").write_text(newgame_field(v).text(True))
        (out / f"{v}-field-walk.rec").write_text(field_walk(v).text(True))
        (out / f"{v}-soak.rec").write_text(soak(v).text(True))
    for v in ("us", "eu"):
        (out / f"{v}-load-menu.rec").write_text(load_menu(v).text(True))
    for v in ("us", "eu"):
        for phase in (1, 2):
            (out / f"{v}-options-persist-p{phase}.rec").write_text(
                options_persist(v, phase).text(True))
    for v in ("us", "eu"):
        for phase in (1, 2):
            (out / f"{v}-volume-persist-p{phase}.rec").write_text(
                volume_persist(v, phase).text(True))
    for v in ("us", "eu"):
        for phase in (1, 2):
            (out / f"{v}-pacing-persist-p{phase}.rec").write_text(
                pacing_persist(v, phase).text(True))
        # One recording per kind; the scenarios differ in the option file and
        # the environment the manifest gives them.
        (out / f"{v}-text-opening.rec").write_text(text_opening(v, TEXT_END[v]).text(True))
        (out / f"{v}-encounter-walk.rec").write_text(encounter_walk(v).text(True))
        (out / f"{v}-encounter-event.rec").write_text(encounter_event(v).text(True))
        (out / f"{v}-two-battles.rec").write_text(two_battles(v).text(True))
    (out / "us-repro-balloon.rec").write_text(repro_balloon("us").text(True))
    for version in ("us", "eu"):
        # Written unsorted, byte for byte as first committed with patch 0058.
        (out / f"{version}-bridge-partner-gate.rec").write_text(
            bridge_partner_gate(version).text())


if __name__ == "__main__":
    main(Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent / "scenarios")
