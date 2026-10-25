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


SCROLL_END = 9400   # frames; the opening's scrolling dialogue is on screen by ~8800


def text_scroll(version):
    """P6.18 Instant: the opening's dialogue has pages whose last line scrolls
    in. The same mash of Cross as the opening, cut after those pages, no
    checkpoints: the log tells how the scrolls went (patch 0068)."""
    r = Rec(f"{version.upper()}: New Game, the opening up to the scrolling dialogue pages")
    new_game_opening(r, version)
    r.lines = [l for l in r.lines
               if not l.startswith(("shot", "end"))
               and (l.startswith("#") or int(l.split()[1]) <= SCROLL_END)]
    r.end(SCROLL_END)
    return r


def field_walk(version):
    r = Rec(f"{version.upper()}: opening, then lobby <-> Login Room door tour")
    new_game_opening(r, version)
    s = Script(r, LOBBY)
    door_tour(s)
    r.end(s.f + 30)
    return r


def banner_walk(version):
    """The field-walk recording without checkpoints: the lobby <-> Login Room doors
    open the area name banner after dialogue windows were freed (the rc6 OLED
    log's 103-frame 'page'). The manifest runs it at Text speed Instant."""
    r = Rec(f"{version.upper()}: opening, then lobby <-> Login Room door tour, no checkpoints")
    new_game_opening(r, version)
    s = Script(r, LOBBY)
    door_tour(s)
    r.lines = [l for l in r.lines if not l.startswith(("shot", "end"))]
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


def file_cache_full(version):
    """Patches 0095, 0096: the two forced battles of two_battles without
    checkpoints. The manifest's env gives the file cache 16 slots
    (DW3_FILE_CACHE_SLOTS), which the two battles fill; the proof is the
    eviction line and both battles ending (before the fix: SIGSEGV)."""
    door, tech2, end = TWO_BATTLES[version]
    r = Rec(f"{version.upper()}: two forced battles with a small file cache (env in manifest)")
    _tech(r, 350)
    for f in range(520, door - 20, 30):
        r.press(f, CROSS)
    _tech(r, tech2)
    for f in range(tech2 + 150, end - 100, 30):
        r.press(f, CROSS)
    r.end(end)
    return r


def p77_battle(version):
    """P7.7: one forced battle (Kotemon vs Kunemon, the env of the manifest
    sets the technique, the enemy's HP and item, the partner's Digimon), the
    first technique of the Tech menu, then Cross to the report and the field.
    Log only: the proof is in the `dw3-p77:` lines the patches print."""
    r = Rec(f"{version.upper()}: P7.7 forced battle, Tech once, Cross to the end (env in manifest)")
    _tech(r, 350)
    for f in range(520, 1470, 30):
        r.press(f, CROSS)
    r.end(1500)
    return r


def battle_report(version):
    """P6.1 (patch 0074): one forced battle (the env of the manifest), a
    technique, Cross to the win and the battle report; checkpoints on the
    report. Measured on the PC build (DW3_TURBO=4): the report is up from about
    frame 1060 to 1140 (EU) and 1220 (US). The manifest forbids the 4:3 gutter
    (`gutter 0 px`) between the report's overlay restore and the field's."""
    r = Rec(f"{version.upper()}: P6.1 battle report in widescreen (env in manifest)")
    _tech(r, 350)
    for f in range(520, 1470, 30):
        r.press(f, CROSS)
    r.shot(1080, "report-a")
    r.shot(1120, "report-b")
    r.end(1500)
    return r


# P6.1 (patches 0085-0087): checkpoints of the title menu, the opening's news
# caption and the player name (Digimon Online registration) screen, per disc
# (frame, name). Measured on the PC build with DW3_TURBO=4; PAL reaches the
# registration screen about 180 frames earlier.
WS_TITLE_INTRO_SHOTS = {
    "us": ((300, "title-menu"), (340, "title-fade"), (2160, "news-caption"),
           (10080, "register-welcome"), (10500, "register-name"),
           (10740, "register-confirm"), (10920, "register-done")),
    "eu": ((360, "title-menu"), (410, "title-fade"), (2160, "news-caption"),
           (9900, "register-welcome"), (10320, "register-name"),
           (10620, "register-confirm"), (10680, "register-done")),
}


def ws_title_intro(version):
    """P6.1 (patches 0085-0087): the New Game path of `newgame-field` up to the
    end of the player name screen, with checkpoints on the title menu and its
    edge fade (0085, the title backdrop in the gutters), the news caption bar
    of the opening (0086, the field message box across 16:9) and the
    registration screen's pages (0087, the hexagon backdrop and the side
    lines in the gutters). The manifest seeds widescreen on."""
    r = Rec(f"{version.upper()}: P6.1 title menu, news caption, registration screen in widescreen")
    new_game_opening(r, version)
    shots = WS_TITLE_INTRO_SHOTS[version]
    last = shots[-1][0]
    r.lines = [l for l in r.lines
               if l.startswith("#") or (l.startswith("pad") and int(l.split()[1]) <= last)]
    for frame, name in shots:
        r.shot(frame, name)
    r.end(last + 10)
    return r


R1 = 0x0800
L1 = 0x0400
# Evidence runs only (measured): the frames later that the PAL battle reaches turn
# 2's message, and the Crosses it needs to get there (the enemy acts twice before turn 2)
P79_EVIDENCE_SHIFT = {"us": 0, "eu": 960}
P79_EVIDENCE_CROSSES = {"us": (), "eu": tuple(range(1600, 2400, 100))}


def _p79_turn1(r, page2):
    """Turn 1 of a P7.9 battle: Fight -> Tech, open the list, pick a technique
    that is not the first line: line 2 (Down), or with `page2` the first line
    of the second page (R1; the manifest's DW3_EXTRA_TECHS gives 8 techniques)."""
    _pad(r, 350, DOWN)               # Fight -> Tech
    _pad(r, 360, 0)
    r.press(410, CROSS)              # open the tech list
    r.press(440, R1 if page2 else DOWN)
    r.press(470, CROSS)              # use it


def p79_menu(version, page2=False, evidence=False):
    """P7.9: one forced battle (Kotemon vs Kunemon, an enemy with 2000 HP and
    extra techniques through the manifest's env). Turn 1 picks a technique
    below the first line (_p79_turn1). Then Cross every 30 frames: with
    battle_menu_memory the second turn's menu already stands on Techniques
    (the first Cross opens the list, the second uses the technique under the
    cursor); without it the Crosses attack. The committed recording is log
    only. `evidence` stops the Cross just before turn 2's menu and takes the
    captures of docs/P7.9_BATTLE_MENU_MEMORY.md (run by hand with --scen-dir,
    never committed)."""
    name = "menu-page2" if page2 else "menu"
    r = Rec(f"{version.upper()}: P7.9 battle menu memory, {name} (env and cfg in manifest)")
    _p79_turn1(r, page2)
    if evidence:
        d = P79_EVIDENCE_SHIFT[version]
        for f in range(520, 1300, 30):
            r.press(f, CROSS)
        for f in P79_EVIDENCE_CROSSES[version]:
            r.press(f, CROSS)
        r.shot(1400 + d, "turn2-message")
        r.press(1420 + d, CROSS)     # closes "damage!"; turn 2's menu opens
        r.shot(1500 + d, "turn2-menu")
        r.shot(1510 + d, "turn2-menu-b")  # the cursor's square blinks: second phase
        r.press(1530 + d, CROSS)     # Tech: opens the list
        r.shot(1620 + d, "turn2-list")
        r.end(1630 + d)
        return r
    for f in range(520, 1470, 30):
        r.press(f, CROSS)
    r.end(1500)
    return r


def p79_two_battles(version):
    """P7.9: the two forced battles of two_battles, but battle 2 starts with
    Cross, Cross (no Down): with the memory on its first menu already stands
    on Techniques, so Cross opens the list and Cross uses the first technique."""
    door, tech2, end = TWO_BATTLES[version]
    r = Rec(f"{version.upper()}: P7.9 menu memory across two forced battles (env and cfg in manifest)")
    _tech(r, 350)
    for f in range(520, door - 20, 30):
        r.press(f, CROSS)
    r.press(tech2 + 60, CROSS)       # the list (the menu stands on Tech)
    r.press(tech2 + 110, CROSS)      # the first technique
    for f in range(tech2 + 150, end - 100, 30):
        r.press(f, CROSS)
    r.end(end)
    return r


SQUARE = 0x8000


def p77_square(version):
    """P7.7: Square pressed in a field (a tap, a second tap, a 60 frame hold,
    and Square with Circle), as the "Square map glitch" of the community
    notes would need. The original game reads no Square in the field, so the
    field must not change mode (the manifest forbids a second mode restore)."""
    r = Rec(f"{version.upper()}: P7.7 Square in the field (env in manifest)")
    r.press(300, SQUARE)
    r.press(400, SQUARE)
    _pad(r, 500, SQUARE)
    _pad(r, 560, 0)
    _pad(r, 600, SQUARE | 0x2000)    # Square + Circle
    _pad(r, 620, 0)
    r.end(700)
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
    """P6.18: Options -> Encounter rate 1x -> 0.5x (Left) and Text speed
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
        r.press(f, LEFT)             # 1x -> 0.5x (the screen steps OFF, 0.5x, 1x, 1.5x, 2x)
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


def battle_cursor_persist(version, phase):
    """P7.9: Options -> down to the last row (Battle cursor), Right twice
    (Off -> Command -> Command+Tech) with a capture of each word, then leave.
    Phase 2 is a fresh process on the same config dir: it opens the screen and
    goes to the row again, the log carries the value read back."""
    r = Rec(f"{version.upper()}: Battle cursor persistence, phase {phase}")
    m = _to_title_menu(r, version)
    r.press(m, DOWN)                 # New Game -> Options
    r.press(m + 20, CROSS)           # open the Options screen
    f = m + 70
    for _ in range(7):               # Widescreen -> ... -> Battle cursor
        r.press(f, DOWN)
        f += 20
    if phase == 1:
        r.shot(f + 20, "battle-cursor-off")
        r.press(f + 40, RIGHT)       # Off -> Command
        r.shot(f + 80, "battle-cursor-command")
        r.press(f + 100, RIGHT)      # Command -> Command+Tech
        r.shot(f + 140, "battle-cursor-command-tech")
        r.press(f + 160, TRIANGLE)   # leave
        r.end(f + 200)
    else:
        r.shot(f + 20, "battle-cursor-restored")
        r.press(f + 40, TRIANGLE)
        r.end(f + 80)
    return r


def options_sound(version):
    """Options screen sound feedback: title -> Options, four cursor moves down
    to SFX volume, Left twice (10 -> 8, each change plays the choice sound at
    the new level), one more row down to Encounter rate and Right (1x -> 1.5x),
    then Triangle (the cancel sound). The log carries every sound with the SFX
    volume it was played at."""
    r = Rec(f"{version.upper()}: Options screen sound feedback")
    m = _to_title_menu(r, version)
    r.press(m, DOWN)                 # New Game -> Options
    r.press(m + 20, CROSS)           # open the Options screen
    f = m + 70
    for _ in range(4):               # Widescreen -> ... -> SFX volume
        r.press(f, DOWN)
        f += 20
    for _ in range(2):               # SFX volume 10 -> 9 -> 8
        r.press(f, LEFT)
        f += 20
    r.press(f, DOWN)                 # Encounter rate
    f += 20
    r.press(f, RIGHT)                # 1x -> 1.5x
    f += 20
    r.press(f + 10, TRIANGLE)        # leave
    r.end(f + 60)
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


def battle_music(version):
    """Random battle, then no input: the battle theme (a long song with an NRPN
    loop) runs to its loop end. Same walk as encounter_walk, then released."""
    start, end = ENCOUNTER[version]
    r = Rec(f"{version.upper()}: walk into a random battle, then idle in it (env in manifest)")
    _pad(r, start, UP)
    _pad(r, end, 0)
    r.end(BATTLE_MUSIC_END)
    return r


BATTLE_MUSIC_END = 7500


def music_loop(version):
    """The field music alone: no input for 3000 frames (env in manifest). The
    log shows the song's NRPN loop jumping back, never an end-of-track."""
    r = Rec(f"{version.upper()}: field music idle, the NRPN loop must repeat (env in manifest)")
    r.end(3000)
    return r


UNDERGROUND_EXIT_END = 700


def underground_exit(version):
    """Leaving the Underground Path (mode 0x20B) for the streets by the Cargo
    Tower (0x201) at the exit slot's arrival point; the manifest's env boots in
    the path and DW3_FORCE_EXIT (patch 0110) leaves it after 100 idle frames.
    Checkpoints: the frame right after the exit, then while walking Down (the
    camera scrolls). Before patch 0111 the canal water was black in both."""
    r = Rec(f"{version.upper()}: Underground Path exit to Central Sector, then walk Down (env in manifest)")
    r.shot(450, "after-exit")
    _pad(r, 470, DOWN)
    r.shot(560, "scroll-mid")
    r.shot(660, "scroll-end")
    _pad(r, 665, 0)
    r.end(UNDERGROUND_EXIT_END)
    return r


IRIS_SHOTS = (207, 216, 225, 234, 243, 252)
# 4:3 iris: in widescreen off the iris closes in 32 frames from ~205 and the
# grid in 20 more, so the late checkpoints are where the map must stay hidden.
IRIS43_SHOTS = (207, 225, 234, 240, 246, 252, 258, 264, 272)
SPIRAL_SHOTS = (1104, 1122, 1140, 1164, 1188, 1206)


def ws_door_iris(version):
    """The area-change iris and grid cover in 16:9 (patch 0115): the manifest's
    env boots in the Underground Path (0x20B) and DW3_FORCE_EXIT (0110) leaves
    it after 100 idle frames; the iris closes from frame ~205 and the grid
    from ~213. Checkpoints during both."""
    r = Rec(f"{version.upper()}: area-change iris and grid cover, 16:9 (env in manifest)")
    for f in IRIS_SHOTS:
        r.shot(f, f"iris-{f}")
    r.end(280)
    return r


def door_iris43(version):
    """The same door as `ws_door_iris` with widescreen off (4:3). After the iris
    closes `closeField` leaves the map layer with an empty (0x0) drawing area,
    which must draw nothing; PSY-Z `sdl3_gpu` used to keep the previous scissor
    and showed the map again (dw3-empty-scissor)."""
    r = Rec(f"{version.upper()}: area-change iris, 4:3, empty drawing area (env in manifest)")
    for f in IRIS43_SHOTS:
        r.shot(f, f"iris43-{f}")
    r.end(285)
    return r


def ws_battle_spiral(version):
    """The encounter spiral in 16:9 (patch 0115 leaves it alone: the swirl is
    a copy of the 320x240 display): the Encounter rate 1x walk of
    `encounter_walk` (Up from frame 300, battle after ~102 footsteps) with a
    checkpoint on the swirl's way out."""
    r = Rec(f"{version.upper()}: random-battle spiral, 16:9 (env in manifest)")
    _pad(r, 300, UP)
    for f in SPIRAL_SHOTS:
        r.shot(f, f"spiral-{f}")
    r.end(1270)
    return r


VICTORY_END = 12000


def victory_music(version):
    """A real random battle, won: the Encounter rate 1x walk of
    `battle_music` (Up from the first control frame), then Cross mashed through
    the battle menus, the knock-out, the win jingle, the report and the way
    back to the field, then idle there so the field music has time to play.
    The manifest's env forces the enemy and the party (no DW3_FORCE_WIN)."""
    start, end = ENCOUNTER[version]
    r = Rec(f"{version.upper()}: walk into a random battle, win it, back to the field (env in manifest)")
    _pad(r, start, UP)
    _pad(r, end, 0)
    for f in range(end + 100, VICTORY_END - 2500, 30):
        r.press(f, CROSS)
    r.end(VICTORY_END)
    return r


def items_screen(version, evidence=False):
    """Field menu -> Items after a battle (patch 0069, root cause of the
    preview.30-rc6 crash; patch 0070's DW3_FORCE_ITEMS fills the bag, the
    manifest's env sets the rest). The forced event battle ends at once
    (DW3_FORCE_WIN); Cross walks the report back to the field, Start opens the
    field menu, Cross opens Items, Cross opens the Items list (the frame that
    used to crash: the panel's icon of an item not chosen yet), the cursor
    moves, an item is picked and used on the first partner, the list is
    reopened, closed, Weapons is opened (second page), and the menus are left
    with Triangle. Log only; `evidence` adds the captures of
    docs/BALLOON_NULL_ACTOR.md (run by hand with --scen-dir, never
    committed). Menus run at the normal speed, a step is ~100-300 frames."""
    r = Rec(f"{version.upper()}: field menu Items after a battle (env and cfg in manifest)")
    for f in range(300, 1200, 30):
        r.press(f, CROSS)            # battle report -> field
    r.press(1250, START)             # field menu
    r.press(1420, CROSS)             # Items (loads the STSTATUS overlay)
    r.press(1650, CROSS)             # open the Items list: used to crash
    r.press(1850, DOWN)
    r.press(1900, DOWN)
    r.press(2050, CROSS)             # pick the item: choose a Digimon
    r.press(2250, CROSS)             # use it
    r.press(2500, CROSS)             # back to the list
    r.press(2850, TRIANGLE)          # close the list
    r.press(3000, DOWN)              # Weapons
    r.press(3050, CROSS)             # its list opens with another item panel
    r.press(3250, R1)                # second page
    r.press(3450, TRIANGLE)
    r.press(3600, TRIANGLE)          # back to the field menu
    r.press(3750, TRIANGLE)          # and to the field
    if evidence:
        for f, n in ((1200, "field"), (1400, "field-menu"), (1600, "item-options"),
                     (1800, "item-list"), (2000, "item-list-moved"), (2200, "choose-digimon"),
                     (2450, "item-used"), (2800, "item-list-again"), (3200, "weapons"),
                     (3400, "weapons-page2"), (3700, "field-menu-back"), (3900, "field-after")):
            r.shot(f, n)
    r.end(3950)
    return r


def items_menu(version):
    """P6.1 (patch 0075): field menu -> Items in widescreen and back, with
    checkpoints on the pictures. The same forced event battle as
    `items_screen` gives the party (the manifest's env), Cross walks the
    report back to the field, Start opens the field menu and Cross picks
    Items: the block dissolve to the menu (checkpoint `dissolve-in`, half
    way), the item screen (`item-options`), the Items list (`item-list`),
    Triangle back to the field menu inside the menu mode (`status-menu`) and
    Triangle again: the dissolve back to the field (`dissolve-out`). With
    0075 the dissolve covers the side gutters; before, it was 320 wide with
    the field (or the menu background) lit beside it. Frames measured on the
    PC build, the same on both discs."""
    r = Rec(f"{version.upper()}: P6.1 field menu Items in widescreen (env and cfg in manifest)")
    for f in range(300, 1200, 30):
        r.press(f, CROSS)            # battle report -> field
    r.press(1250, START)             # field menu
    r.press(1420, CROSS)             # Items: the dissolve, then STSTATUS
    r.shot(1466, "dissolve-in")
    r.shot(1640, "item-options")
    r.press(1650, CROSS)             # the Items list
    r.shot(1800, "item-list")
    r.press(1850, DOWN)
    r.press(2050, TRIANGLE)          # close the list
    r.press(2250, TRIANGLE)          # back to the field menu
    r.shot(2400, "status-menu")
    r.press(2450, TRIANGLE)          # leave: the dissolve back to the field
    r.shot(2488, "dissolve-out")
    r.end(2660)                      # the field is restored by ~2600 (US)
    return r


# P6.1 patch 0076: the field menu's screens, in order. Each step is the
# checkpoint taken just before its press (None: no checkpoint), the press and
# the frames to the next step; the screens open in ~100-200 frames.
MENU_SCREENS_STEPS = (
    # the item screen (the dissolve ends ~1640)
    ("item-options", CROSS, 160), ("item-list", DOWN, 60), (None, R1, 80),
    ("item-list-page2", L1, 80), (None, CROSS, 140), ("item-use", DOWN, 60), (None, CROSS, 160),
    ("item-used", CROSS, 160), (None, TRIANGLE, 160), (None, TRIANGLE, 200),
    # the field menu inside STSTATUS (now with Card folders), Sort: pick two and swap
    ("status-menu", DOWN, 40), (None, CROSS, 200), ("sort", CROSS, 120), (None, DOWN, 80),
    ("sort-swap", CROSS, 160), (None, TRIANGLE, 200),
    # Map (left as it was), Techniques (none learned: the message)
    (None, DOWN, 40), (None, CROSS, 260), ("map", TRIANGLE, 220),
    (None, DOWN, 40), (None, CROSS, 200), (None, CROSS, 140), ("tech-message", CROSS, 140),
    (None, TRIANGLE, 220),
    # Status: the second member, the question, Digivolve, the equipment and a list
    (None, DOWN, 40), (None, CROSS, 200), ("status", DOWN, 60), (None, CROSS, 200),
    ("status-options", CROSS, 200), ("status-digivolve", TRIANGLE, 200), (None, DOWN, 60),
    (None, CROSS, 200), ("status-equip", CROSS, 200), ("status-equip-list", TRIANGLE, 200),
    (None, TRIANGLE, 200), (None, TRIANGLE, 200), (None, TRIANGLE, 220),
    # Card folders: its question, then back to the field
    (None, DOWN, 40), (None, CROSS, 220), ("card", TRIANGLE, 220), (None, TRIANGLE, 250),
    ("field-after", None, 0),
)


def menu_screens(version):
    """P6.1 (patch 0076): every screen of the field menu in widescreen, for
    the 16:9 layout (the left column one gutter further left, the right column
    one gutter further right, the help bar and the item list frame widened).
    The start of `items_menu` (the env of `items_screen`: the party from a
    forced event battle, 3 of every item), the field menu over the field, then
    the item screen, its list (both pages), using an item, the field menu in
    STSTATUS, Sort, Map, Techniques, Status (Digivolve, equipment and its
    list) and Card folders (listed once the menu reopens in STSTATUS: the
    forced item 0x192). Frames measured on the PC build, the same on both
    discs."""
    r = Rec(f"{version.upper()}: P6.1 field menu screens in widescreen (env and cfg in manifest)")
    for f in range(300, 1200, 30):
        r.press(f, CROSS)            # battle report -> field
    r.press(1250, START)             # field menu, over the field
    r.shot(1400, "field-menu")
    r.press(1420, CROSS)             # Items: the dissolve, then STSTATUS
    f = 1640
    for name, button, wait in MENU_SCREENS_STEPS:
        if name:
            r.shot(f - 3, name)
        if button:
            r.press(f, button)
        f += wait
    r.end(f + 50)
    return r


def map_screen(version):
    """P6.1 (patch 0080): the field menu's Map screen in widescreen. The same
    path as `menu_screens` (the manifest's env and cfg) up to the Map entry;
    then the cursor goes to every edge of the map. In 16:9 the map is drawn
    whole and centred (392 px of 426) with black beside it, the cursor, the
    spots and the home mark follow it, and the PS1's scroll (0 to -0x48) is
    not used: the cursor covers the picture's edges and the screen's top.
    Checkpoints: the open map (with the area name box), the cursor at the
    far left, two moves right, at the far right, at the top, at the bottom
    and at the left top. Frames measured on the PC build, the same on both
    discs."""
    r = Rec(f"{version.upper()}: P6.1 field menu Map in widescreen (env and cfg in manifest)")
    for f in range(300, 1200, 30):
        r.press(f, CROSS)            # battle report -> field
    r.press(1250, START)             # field menu, over the field
    r.press(1420, CROSS)             # Items: the dissolve, then STSTATUS
    f = 1640
    for name, button, wait in MENU_SCREENS_STEPS:
        if name == "map":
            break
        if button:
            r.press(f, button)
        f += wait
    assert f == 4000, f              # the Triangle that closes Map, the step's own
    r.shot(3997, "map-open")
    _pad(r, 4000, LEFT)              # to the far left
    _pad(r, 4200, 0)
    r.shot(4210, "map-left")
    _pad(r, 4220, RIGHT)             # all the way right, scrolling the cursor
    r.shot(4295, "map-right-a")
    r.shot(4395, "map-right-b")
    _pad(r, 4440, 0)
    r.shot(4450, "map-right")
    _pad(r, 4460, UP)
    _pad(r, 4620, 0)
    r.shot(4630, "map-up")
    _pad(r, 4640, DOWN)
    _pad(r, 4840, 0)
    r.shot(4850, "map-down")
    _pad(r, 4860, LEFT | UP)
    _pad(r, 5000, 0)
    r.shot(5010, "map-left-up")
    r.press(5030, TRIANGLE)          # back to the field menu
    r.end(5200)
    return r


# The battle report's checkpoints (patch 0081): the header bar opens from the
# left edge, then holds the partner's exp, then the fade. The EU report comes
# ~120 frames before the US one (measured, DW3_TURBO=4).
RESULTS_SHOTS = {
    "us": ((1164, "header-closed"), (1176, "header-opening"), (1200, "header-exp"),
           (1266, "page-two"), (1374, "fade")),
    "eu": ((1032, "header-closed"), (1056, "header-opening"), (1092, "header-exp"),
           (1128, "header-exp-late"), (1266, "fade")),
}


def battle_results(version):
    """P6.1 (patch 0081): the battle results (report) in widescreen. One
    forced battle (the env of `battle_report`), a technique and Cross to the
    win; Cross every 30 frames until 1000, then every 60 so each page stays
    up. Checkpoints on the header bar at the picture's left edge, closed,
    opening, holding the exp, and the fade."""
    r = Rec(f"{version.upper()}: P6.1 battle results in widescreen (env in manifest)")
    _pad(r, 350, DOWN)               # Fight -> Tech
    _pad(r, 360, 0)
    r.press(410, CROSS)              # open the tech list
    r.press(460, CROSS)              # use the first tech
    for f in range(520, 1001, 30):
        r.press(f, CROSS)
    for f in range(1060, 1781, 60):
        r.press(f, CROSS)
    for f, name in RESULTS_SHOTS[version]:
        r.shot(f, name)
    r.end(1800)
    return r


def save_screen(version):
    """P6.1 (patch 0082): the memory card's slot picker, in save mode, in
    widescreen (the env forces the save menu over the field; the manifest's
    env). Checkpoints: before and at the open animation (144, 148: the slot
    strip used to be parked in the right gutter, one card showing), the
    slot question (196), the slot strip sliding in from the right (854, 862,
    870), the slot's block list (886) and the saved game (1294). Cross picks
    slot 1 (700), YES (800) and saves (1000). Measured on the PC build, the
    same on both discs."""
    r = Rec(f"{version.upper()}: P6.1 save screen in widescreen (env in manifest)")
    for f, name in ((144, "before-open"), (148, "opening"), (196, "slot-question")):
        r.shot(f, name)
    r.press(700, CROSS)              # Slot 1
    for f, name in ((854, "slots-in-a"), (862, "slots-in-b"), (870, "slots-in-c"),
                    (886, "slots")):
        r.shot(f, name)
    r.press(800, CROSS)              # YES, create the block
    r.press(1000, CROSS)             # save
    r.shot(1294, "saved")
    r.end(1310)
    return r


R1 = 0x0800

# P6.1 (patches 0100-0104): the card, gym and Lab screens in widescreen. Each
# is forced over the field (the manifest's env: DW3_FORCE_MENU=<mode>, and
# DW3_FORCE_CARDS for the album and the deck editor), so the recordings only
# press what the screen itself takes. Frames measured on the PC build; the
# same on both discs.


def card_album(version):
    """STCRDABM: the panels open from the picture's edges (160), the album
    with card details (300), the next page (R1, 640), the cursor moved (800)."""
    r = Rec(f"{version.upper()}: P6.1 card album in widescreen (env in manifest)")
    r.shot(160, "opening")
    r.shot(300, "album")
    r.press(500, R1)                 # next page
    r.shot(640, "page-two")
    r.press(700, RIGHT)
    r.press(740, DOWN)
    r.shot(800, "cursor")
    r.end(820)
    return r


def deck_editor(version):
    """STCRDDEK: the folders (250), the folder's options (350), the editor
    (550), a card's info (R1, 750), the list of the cards owned (Cross, 1000)
    and two lines down it (1150)."""
    r = Rec(f"{version.upper()}: P6.1 deck editor in widescreen (env in manifest)")
    r.shot(250, "folders")
    r.press(300, CROSS)              # folder 1
    r.shot(350, "options")
    r.press(450, CROSS)              # Edit Folder
    r.shot(550, "editor")
    r.press(650, R1)                 # explain the card
    r.shot(750, "info")
    r.press(760, R1)
    r.press(850, CROSS)              # the list of the cards owned
    r.shot(1000, "list")
    r.press(1050, DOWN)
    r.press(1070, DOWN)
    r.shot(1150, "list-down")
    r.end(1200)
    return r


def card_shop(version):
    """STCRDSHP: the shop (250), Buy One Card (500) and its card details
    (Cross, 700: not enough BIT)."""
    r = Rec(f"{version.upper()}: P6.1 card packs shop in widescreen (env in manifest)")
    r.shot(250, "shop")
    r.press(300, CROSS)              # Buy One Card
    r.shot(500, "buy")
    r.press(550, CROSS)              # the first card
    r.shot(700, "not-enough")
    r.end(900)
    return r


def gym(version):
    """STGTRAIN: the party to train (300, 600: the panels open from the
    edges), a partner chosen (Cross, 900) and a training (Cross, 1200)."""
    r = Rec(f"{version.upper()}: P6.1 gym in widescreen (env in manifest)")
    r.shot(300, "party")
    r.press(400, CROSS)              # first partner
    r.shot(700, "trainings")
    r.press(750, CROSS)              # first training
    r.shot(1000, "session")
    r.end(1100)
    return r


def digimon_lab(version):
    """STGDGLAB: the Select Action menu (250), the party (Switch Digimon,
    500) and the party's details (Cross, 700)."""
    r = Rec(f"{version.upper()}: P6.1 Digimon Lab in widescreen (env in manifest)")
    r.shot(250, "menu")
    r.press(300, CROSS)              # Switch Digimon
    r.shot(500, "party")
    r.press(550, CROSS)              # details
    r.shot(700, "details")
    r.end(900)
    return r


def lab_digivolve(version):
    """STGDGLAB: Digivolve (Down, Cross: the party to choose from, 600, then
    the digivolve type screen, 850 and 1100)."""
    r = Rec(f"{version.upper()}: P6.1 Digimon Lab digivolve in widescreen (env in manifest)")
    r.press(300, DOWN)
    r.press(400, CROSS)              # Digivolve
    r.shot(600, "who")
    r.press(650, CROSS)              # the first partner
    r.shot(850, "type")
    r.press(900, CROSS)              # Change digivolve type
    r.shot(1100, "type-choice")
    r.end(1150)
    return r


# P6.20 Cheats screen (docs/PORT_OPTIONS.md "Cheats"). Square is 0x8000.
SQUARE = 0x8000


def cheats_screen(version, phase):
    """Two phases on one config dir. Phase 1: title -> Options -> Square (the
    Cheats screen) -> master switch ON, Fill bag ready, Instant win ON, Battle
    now ready, Triangle back to Options, Triangle. Phase 2 is a new process:
    Options -> Square -> Triangle -> Triangle. The logs carry the toggles saved
    and read back, and the one-shots not kept. No checkpoints."""
    r = Rec(f"{version.upper()}: Cheats screen, phase {phase}")
    m = _to_title_menu(r, version)
    r.press(m, DOWN)                 # New Game -> Options
    r.press(m + 20, CROSS)           # open the Options screen
    f = m + 70
    r.press(f, SQUARE)               # the Cheats screen
    f += 40
    if phase == 1:
        r.press(f, CROSS)            # master switch ON
        f += 20
        for _ in range(3):           # Fill bag, Instant win, Battle now
            r.press(f, DOWN)
            f += 20
            r.press(f, CROSS)
            f += 20
    r.press(f, TRIANGLE)             # back to Options
    f += 40
    r.press(f, TRIANGLE)             # leave
    r.end(f + 50)
    return r


def cheat_fill_bag(version):
    """Fill bag: on the Encounter rate field (env in the manifest, the
    one-shot armed by DW3_CHEATS_ARM), Start opens the field menu and Cross
    opens Items, which starts STSTATUS: patch 0070's hook fills the bag from
    the cheat. Then the Items list opens on a full bag."""
    start, _ = ENCOUNTER[version]
    r = Rec(f"{version.upper()}: field menu -> Items (Fill bag; env and cfg in manifest)")
    r.press(start + 100, START)      # field menu
    r.press(start + 270, CROSS)      # Items: STSTATUS_createMenu
    r.press(start + 500, CROSS)      # the Items list
    r.end(start + 900)
    return r


def cheats_screen2(version):
    """PR 2: title -> Options -> Square -> master ON, then page 2: Money +100,000
    (Cross twice), Warp to the third area (Digimon Arena, Cross three times),
    Item id 43 + 3 (Right) + 10 (R1) = 56, Item count 10 + 10 (R1) - 1 (Left) =
    19, Give item ready, Triangle twice. The logs carry what was armed. No
    checkpoints."""
    r = Rec(f"{version.upper()}: Cheats screen page 2")
    m = _to_title_menu(r, version)
    r.press(m, DOWN)                 # New Game -> Options
    r.press(m + 20, CROSS)           # open the Options screen
    f = m + 70
    r.press(f, SQUARE)               # the Cheats screen
    f += 40
    r.press(f, CROSS)                # master switch ON
    f += 20
    for _ in range(5):               # page 1's other rows, then Money
        r.press(f, DOWN)
        f += 20
    for _ in range(2):               # Money: +10,000, +100,000
        r.press(f, CROSS)
        f += 20
    r.press(f, DOWN)                 # Warp
    f += 20
    for _ in range(3):               # Asuka City, Digimon Lab, Digimon Arena
        r.press(f, CROSS)
        f += 20
    r.press(f, DOWN)                 # Item id
    f += 20
    for _ in range(3):
        r.press(f, RIGHT)
        f += 20
    r.press(f, R1)
    f += 20
    r.press(f, DOWN)                 # Item count
    f += 20
    r.press(f, R1)
    f += 20
    r.press(f, LEFT)
    f += 20
    r.press(f, DOWN)                 # Give item
    f += 20
    r.press(f, CROSS)
    f += 40
    r.press(f, TRIANGLE)             # back to Options
    f += 40
    r.press(f, TRIANGLE)             # leave
    r.end(f + 50)
    return r


# --- P6.9 restart validation ------------------------------------------------

# The restart chord, L1 + R1 + Start + Select (PS1 logical pad). The option
# (restart_hotkey=1) is seeded by the manifest's cfg. The game frame counter
# keeps running across the in-process restart, so the boot sequence after the
# chord is the cold-boot sequence shifted by the frame of the chord.
CHORD = 0x0C09


def _boot_to_menu(r, version, f):
    """Cold-boot input after a restart at frame f: US skips the opening movie
    and presses PRESS START; EU picks the default language (English), skips
    the splashes and presses PRESS START. Returns the frame of the Start /
    Continue menu (every offset is the cold boot's, see us_title, load_menu)."""
    if version == "us":
        r.press(f + 20, START)
        r.press(f + 210, START)
        return f + 310
    for k in (110, 160, 190, 220, 280):
        r.press(f + k, START)
    return f + 380


def _restart_battle_inputs(r):
    """The forced battle of p77_battle: Tech once, Cross through the report."""
    _tech(r, 350)
    for f in range(520, 1470, 30):
        r.press(f, CROSS)


def _restart_battle_inputs_at(r, base):
    """The same inputs, shifted to the chord's frame."""
    _tech(r, base + 350)
    for f in range(base + 520, base + 1470, 30):
        r.press(f, CROSS)


def restart_after_battle(version):
    """Restart in the field after a finished battle (patch 0124: START_MODE is boot-only, the
    restart lands on the real boot screen). Battle, report, field (turbo 4);
    the chord at 1600; then the boot screen and the title menu."""
    r = Rec(f"{version.upper()}: restart in the field after a battle (env in manifest)")
    _restart_battle_inputs(r)
    r.shot(1580, "field-after-battle")
    r.press(1600, CHORD)
    r.shot(1660, "boot-a")
    m = _boot_to_menu(r, version, 1600)
    r.shot(m - 100, "title")
    r.shot(m + 40, "menu")
    r.end(m + 60)
    return r


def restart_battle_again(version):
    """Restart after a battle with DW3_START_MODE_EVERY: the restart re-enters
    the forced battle (a fresh GAME), and the same inputs, shifted by the
    chord's frame, must give the same battle and report again."""
    r = Rec(f"{version.upper()}: restart after a battle, then the same battle again (env in manifest)")
    _restart_battle_inputs(r)
    r.shot(1000, "battle-1-late")
    r.shot(1580, "field-after-battle")
    base = 1600
    r.press(base, CHORD)
    # the battle again, 6 frames of the chord's release give the same start
    for k in (100, 200, 300):
        r.shot(base + k, f"again-{k}")
    _tech(r, base + 350)
    for f in range(base + 520, base + 1470, 30):
        r.press(f, CROSS)
    r.shot(base + 1000, "battle-2-late")
    r.shot(base + 1580, "field-after-battle-2")
    r.end(base + 1600)
    return r


def restart_in_battle(version):
    """The chord inside a battle (the command menu is up): the host restarts
    from any mode. START_MODE is boot-only (0124): boot screen, title menu afterwards."""
    r = Rec(f"{version.upper()}: restart during a battle (env in manifest)")
    r.shot(340, "battle-menu")
    _tech(r, 350)
    r.shot(560, "battle-tech")
    base = 600
    r.press(base, CHORD)
    r.shot(base + 60, "boot-a")
    m = _boot_to_menu(r, version, base)
    r.shot(m - 100, "title")
    r.shot(m + 40, "menu")
    r.end(m + 60)
    return r


def restart_in_battle_redo(version, base=650):
    """The chord inside a battle with DW3_START_MODE_EVERY: the restart re-enters
    the forced battle (a fresh GAME after a battle torn down mid-turn) and the
    same battle is played to the end, report and field. The restart does not
    reseed the RNG, so who moves first in the second battle depends on the
    chord's frame (US at 600, EU at 637 and 700 give the enemy the first move
    and the script's Tech press then lands on a text box); 650 gives the
    command menu on both."""
    r = Rec(f"{version.upper()}: restart during a battle, then the battle to the end (env in manifest)")
    _tech(r, 350)
    r.shot(560, "battle-tech")
    r.press(base, CHORD)
    _restart_battle_inputs_at(r, base)
    for k in (100, 200, 300):
        r.shot(base + k, f"again-{k}")
    r.shot(base + 340, "menu-again")
    r.shot(base + 560, "tech-again")
    r.shot(base + 700, "turn-again")
    r.shot(base + 1580, "field-after-battle")
    r.end(base + 1600)
    return r


def restart_newgame(version):
    """No env, so Ryubing runs it too (the option file seeds the hotkey): New
    Game, the opening's TV news, the chord, the boot screen, New Game again and
    the same news again."""
    r = Rec(f"{version.upper()}: restart during the New Game opening, then New Game again")
    first = 900 if version == "eu" else 800
    if version == "us":
        r.press(20, START)
        r.press(210, START)
        r.press(320, CROSS)          # New Game
    else:
        for f in (110, 160, 190, 220, 280):
            r.press(f, START)
        r.press(390, CROSS)          # Start (New Game)
    for f in range(first, 2300, CROSS_PERIOD):
        r.press(f, CROSS)
    r.shot(2100, "news")
    base = 2400
    r.press(base, CHORD)
    r.shot(base + 60, "boot-a")
    m = _boot_to_menu(r, version, base)
    r.shot(m + 4, "menu")
    r.press(m + 10, CROSS)           # New Game / Start again
    for f in range(base + first, base + 2300, CROSS_PERIOD):
        r.press(f, CROSS)
    r.shot(base + 2100, "news-again")
    r.end(base + 2300)
    return r


def restart_before_battle(version):
    """Restart in a random-battle field before any fight, then walk: Battle
    now (the cheats hook) makes the first footstep a battle. START_MODE is not
    one-shot here, so the restart re-enters the field. Log only."""
    r = Rec(f"{version.upper()}: restart in a field, then a random battle (env in manifest)")
    r.press(200, CHORD)
    _pad(r, 500, UP)
    r.end(1500)
    return r


def restart_save_continue(version):
    """Field save, restart, Continue. The save screen of save_screen (slot 1,
    YES, save), the chord, the boot screen, then Load Game / Continue, slot 1
    and the loaded game. Checkpoints keep the pre-restart saved screen, the
    title after the restart and the loaded field."""
    r = Rec(f"{version.upper()}: field save, restart, Continue (env in manifest)")
    for f, name in ((120, "field-before"), (196, "slot-question"), (886, "slots")):
        r.shot(f, name)
    r.press(700, CROSS)              # Slot 1
    r.press(800, CROSS)              # YES, create the block
    r.press(1000, CROSS)             # save
    r.shot(1294, "saved")
    base = 1320
    r.press(base, CHORD)
    m = _boot_to_menu(r, version, base)
    r.shot(m - 100, "title")
    r.press(m + 10, DOWN)
    r.press(m + 30, DOWN)            # third entry: Load Game / Continue
    r.shot(m + 60, "load-selected")
    r.press(m + 70, CROSS)
    r.shot(m + 400, "slot-choice")
    r.press(m + 420, CROSS)          # slot 1
    r.shot(m + 600, "slot-result")   # the block saved before the restart
    r.press(m + 620, CROSS)          # execute
    r.shot(m + 700, "loading")
    r.press(m + 760, CROSS)
    r.press(m + 900, CROSS)
    r.shot(m + 1100, "continued")
    r.end(m + 1120)
    return r


RESTART_RECS = (("restart-after-battle", restart_after_battle),
                ("restart-in-battle-redo", restart_in_battle_redo),
                ("restart-newgame", restart_newgame),
                ("restart-battle-again", restart_battle_again),
                ("restart-in-battle", restart_in_battle),
                ("restart-before-battle", restart_before_battle),
                ("restart-save-continue", restart_save_continue))


def title_quit(version):
    """P6.28: the title menu's Quit game. Down to the fourth row (checkpoint
    `quit-focus`), Cross opens the Yes / No window with the cursor on No
    (`confirm-no`), Cross answers No (`after-no`: the menu as before), Cross
    opens it again, Down moves to Yes (`confirm-yes`), Cross answers Yes: the
    saves are flushed and the process ends by itself (the manifest allows the
    early exit and expects the flush line)."""
    r = Rec(f"{version.upper()}: title menu Quit game, No then Yes (P6.28)")
    m = _to_title_menu(r, version)
    r.shot(m, "menu")
    f = m + 20
    for _ in range(3):               # New Game, Options, Load Game, Quit game
        r.press(f, DOWN)
        f += 20
    r.shot(f + 20, "quit-focus")
    r.press(f + 40, CROSS)           # the window, cursor on No
    r.shot(f + 100, "confirm-no")
    r.press(f + 120, CROSS)          # No
    r.shot(f + 180, "after-no")
    r.press(f + 200, CROSS)          # again
    r.press(f + 250, DOWN)           # cursor to Yes
    r.shot(f + 310, "confirm-yes")
    r.press(f + 330, CROSS)          # Yes: flush, quit
    r.end(f + 700)
    return r


def title_quit_lang(idx):
    """P6.28: the Quit game plaque and the Yes / No window in one of the other
    four EU languages (`eu-title-quit` is English): language select, title,
    Down to the fourth row (`quit-focus`), Cross (`confirm-no`), Down to Yes
    (`confirm-yes`), Triangle cancels (`after-cancel`). The process runs to the
    end; nothing is quit."""
    lang = EU_LANGS[idx]
    r = Rec(f"EU: language select -> {lang}, title menu Quit game window (P6.28)")
    f = 90
    for _ in range(idx):
        r.press(f, DOWN)
        f += 15
    c = f + 20
    r.press(c, START)                # confirm the language
    for k in (50, 80, 110):          # skip the splash / FMV
        r.press(c + k, START)
    r.press(c + 170, START)          # PRESS START
    g = c + 290
    for i in range(3):               # New Game, Options, Load Game, Quit game
        r.press(g + 20 * i, DOWN)
    r.shot(g + 100, "quit-focus")
    r.press(g + 120, CROSS)
    r.shot(g + 180, "confirm-no")
    r.press(g + 200, DOWN)
    r.shot(g + 260, "confirm-yes")
    r.press(g + 280, TRIANGLE)
    r.shot(g + 340, "after-cancel")
    r.end(g + 400)
    return r


# After the reset (patch 0123): US lands on the title, EU on the language
# select (P6.9), where English is confirmed and the splash skipped as in
# `eu_language`. Frames measured on the PC build.
def return_title(version, rows7=False):
    """P6.27: the field menu's Return to title (env and cfg in manifest: the
    party of the forced event battle as in `items_menu`, Restart hotkey ON;
    rows7 also fills the bag, which lists the card folders). Cross walks the
    report back to the field, Start opens the field menu (`field-menu`), Down
    to the last row (`return-focus`), Cross opens the window on No
    (`confirm-no`), Cross answers No (`after-no`), Cross again, Down to Yes
    (`confirm-yes`), Cross answers Yes: the P6.9 reset, back at the title
    (`back-at-title`; on the EU disc the language select first,
    `back-at-select`). rows7: the field menu inside STSTATUS (Items, then
    Triangle), seven rows, the window and Triangle cancelling it."""
    r = Rec(f"{version.upper()}: field menu Return to title (env and cfg in manifest)")
    for f in range(300, 1200, 30):
        r.press(f, CROSS)            # battle report -> field
    r.press(1250, START)             # the field menu
    if rows7:
        # DW3_FORCE_ITEMS (patch 0070) runs when the menu mode starts: Items
        # (the dissolve, STSTATUS), Triangle back to the field menu inside it,
        # which now lists the card folders (the cursor stands on Items).
        r.press(1420, CROSS)
        r.press(1700, TRIANGLE)
        r.shot(2000, "field-menu")
        f = 2020
    else:
        r.shot(1400, "field-menu")
        f = 1420
    for _ in range(6 if rows7 else 5):   # to the last row
        r.press(f, DOWN)
        f += 20
    r.shot(f + 20, "return-focus")
    r.press(f + 40, CROSS)           # the window, cursor on No
    r.shot(f + 100, "confirm-no")
    if rows7:
        r.press(f + 120, TRIANGLE)   # cancel
        r.shot(f + 180, "after-cancel")
        r.press(f + 200, TRIANGLE)   # close the menu
        r.end(f + 300)
        return r
    r.press(f + 120, CROSS)          # No
    r.shot(f + 180, "after-no")
    r.press(f + 200, CROSS)          # again
    r.press(f + 250, DOWN)           # cursor to Yes
    r.shot(f + 310, "confirm-yes")
    r.press(f + 330, CROSS)          # Yes: the reset
    g = f + 330
    if version == "us":
        r.shot(g + 600, "back-at-title")
        r.end(g + 700)
    else:
        r.shot(g + 200, "back-at-select")
        r.press(g + 220, START)      # English
        for k in (270, 300, 330):    # skip the splash / FMV
            r.press(g + k, START)
        r.shot(g + 600, "back-at-title")
        r.end(g + 700)
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
        (out / f"{v}-banner-walk.rec").write_text(banner_walk(v).text(True))
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
        (out / f"{v}-text-scroll.rec").write_text(text_scroll(v).text(True))
        (out / f"{v}-options-sound.rec").write_text(options_sound(v).text(True))
        for phase in (1, 2):
            (out / f"{v}-battle-cursor-persist-p{phase}.rec").write_text(
                battle_cursor_persist(v, phase).text(True))
        # One recording per kind; the scenarios differ in the option file and
        # the environment the manifest gives them.
        (out / f"{v}-text-opening.rec").write_text(text_opening(v, TEXT_END[v]).text(True))
        (out / f"{v}-encounter-walk.rec").write_text(encounter_walk(v).text(True))
        (out / f"{v}-encounter-event.rec").write_text(encounter_event(v).text(True))
        (out / f"{v}-items-screen.rec").write_text(items_screen(v).text(True))
        (out / f"{v}-items-menu.rec").write_text(items_menu(v).text(True))
        (out / f"{v}-menu-screens.rec").write_text(menu_screens(v).text(True))
        (out / f"{v}-map-screen.rec").write_text(map_screen(v).text(True))
        (out / f"{v}-battle-results.rec").write_text(battle_results(v).text(True))
        (out / f"{v}-save-screen.rec").write_text(save_screen(v).text(True))
        (out / f"{v}-card-album.rec").write_text(card_album(v).text(True))
        (out / f"{v}-deck-editor.rec").write_text(deck_editor(v).text(True))
        (out / f"{v}-card-shop.rec").write_text(card_shop(v).text(True))
        (out / f"{v}-gym.rec").write_text(gym(v).text(True))
        (out / f"{v}-digimon-lab.rec").write_text(digimon_lab(v).text(True))
        (out / f"{v}-lab-digivolve.rec").write_text(lab_digivolve(v).text(True))
        (out / f"{v}-victory-music.rec").write_text(victory_music(v).text(True))
        (out / f"{v}-music-loop.rec").write_text(music_loop(v).text(True))
        (out / f"{v}-underground-exit.rec").write_text(underground_exit(v).text(True))
        (out / f"{v}-ws-door-iris.rec").write_text(ws_door_iris(v).text(True))
        (out / f"{v}-door-iris43.rec").write_text(door_iris43(v).text(True))
        (out / f"{v}-ws-battle-spiral.rec").write_text(ws_battle_spiral(v).text(True))
        (out / f"{v}-battle-music.rec").write_text(battle_music(v).text(True))
        (out / f"{v}-two-battles.rec").write_text(two_battles(v).text(True))
        (out / f"{v}-battle-report.rec").write_text(battle_report(v).text(True))
        (out / f"{v}-ws-title-intro.rec").write_text(ws_title_intro(v).text(True))
        (out / f"{v}-p77-battle.rec").write_text(p77_battle(v).text(True))
        (out / f"{v}-p77-square.rec").write_text(p77_square(v).text(True))
        (out / f"{v}-file-cache-full.rec").write_text(file_cache_full(v).text(True))
        (out / f"{v}-p79-menu.rec").write_text(p79_menu(v).text(True))
        (out / f"{v}-p79-menu-page2.rec").write_text(p79_menu(v, page2=True).text(True))
        (out / f"{v}-p79-two-battles.rec").write_text(p79_two_battles(v).text(True))
        for phase in (1, 2):
            (out / f"{v}-cheats-screen-p{phase}.rec").write_text(
                cheats_screen(v, phase).text(True))
        (out / f"{v}-cheat-fill-bag.rec").write_text(cheat_fill_bag(v).text(True))
        (out / f"{v}-cheats-screen2.rec").write_text(cheats_screen2(v).text(True))
        (out / f"{v}-title-quit.rec").write_text(title_quit(v).text(True))
        (out / f"{v}-return-title.rec").write_text(return_title(v).text(True))
        (out / f"{v}-return-title-rows7.rec").write_text(
            return_title(v, rows7=True).text(True))
    for i, lang in enumerate(EU_LANGS):
        if lang != "en":
            (out / f"eu-title-quit-{lang}.rec").write_text(title_quit_lang(i).text(True))
    for v in ("us", "eu"):
        for name, fn in RESTART_RECS:
            (out / f"{v}-{name}.rec").write_text(fn(v).text(True))
    (out / "us-repro-balloon.rec").write_text(repro_balloon("us").text(True))
    for version in ("us", "eu"):
        # Written unsorted, byte for byte as first committed with patch 0058.
        (out / f"{version}-bridge-partner-gate.rec").write_text(
            bridge_partner_gate(version).text())


if __name__ == "__main__":
    main(Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent / "scenarios")
