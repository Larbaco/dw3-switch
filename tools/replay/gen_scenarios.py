#!/usr/bin/env python3
"""Generate the P6.15 pad recordings (scenarios/*.rec).

The recordings are plain pad masks keyed by game frame (PadChkVsync count);
they contain no game data. They can equally be produced by hand-playing the
PC build with DW3_REPLAY_RECORD=<file>; this script exists so the five EU
language scenarios stay consistent and reviewable.

Format: see port/psyz_missing/psyz_pad_host.c (pad / shot / end directives).
Raw masks: START 0x0008, UP 0x0010, DOWN 0x0040, CROSS 0x4000.
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

    def text(self):
        return "\n".join(self.lines) + "\n"


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


def main(out):
    out.mkdir(parents=True, exist_ok=True)
    (out / "us-title.rec").write_text(us_title().text())
    for i, lang in enumerate(EU_LANGS):
        (out / f"eu-lang-{lang}.rec").write_text(eu_language(i).text())
    for version in ("us", "eu"):
        (out / f"{version}-bridge-partner-gate.rec").write_text(
            bridge_partner_gate(version).text())


if __name__ == "__main__":
    main(Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent / "scenarios")
