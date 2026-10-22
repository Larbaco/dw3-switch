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
    """Known PC crash: walking DOWN/LEFT in the lobby reaches a null
    dereference in FIELDSTG_drawBalloon (reproduced on US at frame ~17410; the same walk on EU does not crash)."""
    r = Rec(f"{version.upper()}: lobby walk that crashes FIELDSTG_drawBalloon (known bug)")
    new_game_opening(r, version)
    s = Script(r, LOBBY - 80)
    s.walk(LEFT, 240)
    s.walk(DOWN | LEFT, 180)
    s.walk(DOWN, 180)
    s.walk(DOWN | RIGHT, 120)
    s.wait(100)
    r.end(s.f + 10)
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
    (out / "us-repro-balloon.rec").write_text(repro_balloon("us").text(True))


if __name__ == "__main__":
    main(Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent / "scenarios")
