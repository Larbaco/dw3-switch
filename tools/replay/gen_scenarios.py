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
CIRCLE = 0x2000
L2, R2 = 0x0100, 0x0200
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


# P9.9: the Options screen is a list of groups; a group opens a sub-screen
# with its rows and Triangle goes back to the list with the cursor kept.
# D1 (docs/OPTIONS_REORG.md): the list is Display, Sound, Gameplay, Menus,
# Controls, Saves, Cheats, Advanced, Help, Walkthrough (the default build; a
# --no-cheats build has no Cheats and no Advanced). These are the positions in it.
GRP_DISPLAY, GRP_SOUND, GRP_GAMEPLAY, GRP_MENUS = 0, 1, 2, 3
GRP_CONTROLS = 4  # P9.8: the Button layout row (D1: before Saves)
GRP_SAVES = 5
GRP_CHEATS = 6    # P51 item 5: always on the list (greyed in game)
GRP_ADVANCED = 7  # D1: the stage sweep (listed with Cheats)
GRP_HELP = 8
GRP_WALK = 9
# Rows inside a group (D1). Gameplay: Fast-forward, Encounter rate, Text speed, Battle
# cursor (Restart hotkey moved to Controls, Region at start to Saves). Controls: Button
# layout, Customize buttons, Map buttons, Reset to default, [Switch glyphs on the Switch],
# Restart hotkey. Saves: Auto-save, [Region at start in the combined image], Shared saves.
ROW_GP_ENCOUNTER, ROW_GP_TEXT, ROW_GP_BATTLE = 1, 2, 3
ROW_CT_CUSTOMIZE, ROW_CT_MAPBTN, ROW_CT_RESET, ROW_CT_RESTART = 1, 2, 3, 4
ROW_SV_REGION = 1


def _keys(r, f, *masks, gap=20):
    """Press the masks one after the other, `gap` frames apart; returns the
    frame after the last one."""
    for m in masks:
        r.press(f, m)
        f += gap
    return f


def _group(r, f, frm, to):
    """From the list's cursor on group `frm`, move to group `to` and open it."""
    step = DOWN if to > frm else UP
    return _keys(r, f, *([step] * abs(to - frm)), CROSS)


def us_title():
    r = Rec("US: boot, skip splash/opening movie, title, menu")
    r.press(20, START)           # skip the opening FMV
    r.shot(200, "title")
    r.press(210, START)          # PRESS START -> New Game / Options / Load
    r.shot(310, "menu")
    r.end(320)
    return r


def eu_language(idx, confirm=START):
    lang = EU_LANGS[idx]
    how = " with Cross" if confirm == CROSS else ""
    r = Rec(f"EU: language select -> {lang}{how}, title, menu")
    r.shot(80, "select-default")
    f = 90
    for _ in range(idx):
        r.press(f, DOWN)
        f += 15
    r.shot(f + 10, "select-cursor")
    c = f + 20
    r.press(c, confirm)          # confirm language (patch 0187: Cross works too)
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


# Patch 0198: the battle's Digivolve command three times in one forced battle (the
# env of the manifest: Kotemon vs Kunemon, DW3_FORCE_DIGI_SLOTS=386 gives Kotemon
# Dinohumon to digivolve into, DW3_FORCE_ENEMY_HP keeps the fight going): up
# (Kotemon -> Dinohumon, the list's second line), back to the base form (the
# list's first line, the path of the BUG-6 crash) and up again. Between them
# Cross is mashed through the Digimon change's message and the enemy's two attacks
# until the command menu is open again (measured on the PC build, DW3_TURBO=4: the
# menu waits for input, so a Cross after it opens would pick Fight). Per disc:
# (mash 1 from, to; menu 2 at; mash 2 from, to; menu 3 at; end).
DIGIVOLVE_COMMAND = {
    "us": (1500, 1850, 1950, 3070, 3420, 3520, 5200),
    "eu": (1500, 1800, 1900, 3020, 3320, 3420, 5000),
}


def battle_digivolve_command(version):
    m1a, m1b, c2, m2a, m2b, c3, end = DIGIVOLVE_COMMAND[version]
    r = Rec(f"{version.upper()}: forced battle, the Digivolve command up, back to the base form, up again (env in manifest)")

    def digivolve(t, below):
        r.press(t, DOWN)             # Fight -> Tech
        r.press(t + 30, DOWN)        # Tech -> DV
        r.press(t + 60, CROSS)       # open the Digimon list
        if below:                    # the list starts on its first line: the form below the base
            r.press(t + 120, DOWN)
            r.press(t + 150, CROSS)
        else:
            r.press(t + 120, CROSS)

    def mash(a, b):
        for f in range(a, b + 1, 50):
            r.press(f, CROSS)

    digivolve(350, True)
    mash(m1a, m1b)
    digivolve(c2, False)
    mash(m2a, m2b)
    digivolve(c3, True)
    r.end(end)
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


def xr_save(version):
    """P6.25: the save half of the cross-region scenarios. The save screen is
    forced over the field (the manifest's phase env, as save_screen), slot 1,
    YES, save: the game itself writes the card. No checkpoints; the verdict is
    the log (`STGMCARD_writeSave ... status=1`)."""
    r = Rec(f"{version.upper()}: P6.25 save a game from the forced save screen (log only)")
    r.press(700, CROSS)              # Slot 1
    r.press(800, CROSS)              # YES, create the block
    r.press(1000, CROSS)             # save
    r.end(1310)
    return r


def xr_load(version):
    """P6.25: the load half. Title -> Load Game / Continue -> slot 1 on a card
    that holds the OTHER disc's save (imported and converted at boot) -> the
    load -> the field. No checkpoints; the verdict is the log and the overlay
    restores."""
    r = Rec(f"{version.upper()}: P6.25 title, Continue a save made on the other disc (log only)")
    m = _to_title_menu(r, version)
    r.press(m + 10, DOWN)
    r.press(m + 30, DOWN)            # third entry: Load Game / Continue
    r.press(m + 70, CROSS)
    r.press(m + 420, CROSS)          # memory card slot 1 (port A)
    r.press(m + 720, CROSS)          # the cursor is on the saved game: Load
    r.press(m + 900, CROSS)          # close the "loaded" message; the field follows
    r.end(m + XR_LOAD_FRAMES)
    return r


XR_LOAD_FRAMES = 1400


def load_view(version):
    """Bug 4 (p56 OLED): the Load Game screen with a saved game on the card.
    Title -> Load Game / Continue -> slot 1 on a card that holds the save made
    by the xr-save phase (same disc) -> the save list with its footer prompts
    (confirm / back). Checkpoints: the slot choice and the list. Nothing is
    loaded (no final Cross): the shots are the verdict, plus the log."""
    r = Rec(f"{version.upper()}: Load Game screen with a save on the card (footer prompts)")
    m = _to_title_menu(r, version)
    r.press(m + 10, DOWN)
    r.press(m + 30, DOWN)            # third entry: Load Game / Continue
    r.press(m + 70, CROSS)
    r.shot(m + 400, "slot-choice")
    r.press(m + 420, CROSS)          # memory card slot 1 (port A)
    r.shot(m + 700, "save-list")
    r.end(m + 720)
    return r


def xr_open(version):
    """P6.25 slice 2: start the build, let it open the cards, leave. No input, no
    checkpoints; the verdict is the `dw3-store:` open line in the log. Used to
    show that a card the other region only looked at is still whole."""
    r = Rec(f"{version.upper()}: P6.25 slice 2 start, open the cards and leave (log only)")
    r.end(120)
    return r

# P6.31 auto-save: the load half of xr_load, then more frames in the field so a
# forced exit (DW3_FORCE_EXIT, the manifest's env) and the area change it makes
# can be waited out. AS_LOAD_FRAMES is measured on the PC build.
AS_LOAD_FRAMES = {"us": 2600, "eu": 2800}
AS_WALK = {"us": (1300, 4400), "eu": (1500, 4800)}


def as_load(version):
    """P6.31: Continue slot 1 of the card the first phase saved, then idle in
    the field. The manifest's env forces an exit to another area a hundred idle
    frames in (DW3_FORCE_EXIT); the auto-save, if it is on, fires on arrival.
    No checkpoints; the verdict is the log."""
    r = Rec(f"{version.upper()}: P6.31 Continue, then an area change in the field (log only)")
    m = _to_title_menu(r, version)
    r.press(m + 10, DOWN)
    r.press(m + 30, DOWN)            # third entry: Load Game / Continue
    r.press(m + 70, CROSS)
    r.press(m + 420, CROSS)          # memory card slot 1 (port A)
    r.press(m + 720, CROSS)          # the cursor is on the saved game: Load
    r.press(m + 900, CROSS)          # close the "loaded" message; the field follows
    r.end(m + AS_LOAD_FRAMES[version])
    return r


def as_continue(version):
    """P6.31 follow-up: with autosave.bin on the card the title's third row is
    Continue and loads it at once (no slot list). Down, Down, Cross, then idle
    in the field. The verdict is the log."""
    r = Rec(f"{version.upper()}: P6.31 title Continue loads the auto-save (log only)")
    m = _to_title_menu(r, version)
    r.press(m + 10, DOWN)
    r.press(m + 30, DOWN)            # third entry: Continue (the auto-save)
    r.press(m + 70, CROSS)
    r.end(m + 900)
    return r


def as_loadrow(version):
    """P6.31 follow-up: the same title, one row lower: Load opens the slot
    list, and the player's own save loads from it (not the auto-save)."""
    r = Rec(f"{version.upper()}: P6.31 title Load opens the slot list (log only)")
    m = _to_title_menu(r, version)
    r.press(m + 10, DOWN)
    r.press(m + 30, DOWN)
    r.press(m + 50, DOWN)            # fourth entry: Load Game
    r.press(m + 90, CROSS)
    r.press(m + 440, CROSS)          # memory card slot 1 (port A)
    r.press(m + 740, CROSS)          # the cursor is on the saved game: Load
    r.press(m + 920, CROSS)          # close the "loaded" message; the field follows
    r.end(m + XR_LOAD_FRAMES)
    return r


def as_title(version):
    """P6.31 follow-up: the five-plaque title menu (an auto-save exists):
    checkpoints with the focus on New Game, Continue, Load Game, Quit game."""
    r = Rec(f"{version.upper()}: P6.31 title with Continue and Load rows")
    m = _to_title_menu(r, version)
    r.shot(m, "new-focus")
    f = m + 20
    for name in ("options-focus", "continue-focus", "load-focus", "quit-focus"):
        r.press(f, DOWN)
        r.shot(f + 20, name)
        f += 40
    r.end(f + 10)
    return r


def as_walk(version):
    """P6.31: Continue slot 1, then walk Up in a random-battle field until a
    battle starts and goes on (the encounter walk). The auto-save must not
    write: the area never changes, and never in a battle."""
    start, end = AS_WALK[version]
    r = Rec(f"{version.upper()}: P6.31 Continue, walk into a random battle (log only)")
    m = _to_title_menu(r, version)
    r.press(m + 10, DOWN)
    r.press(m + 30, DOWN)
    r.press(m + 70, CROSS)
    r.press(m + 420, CROSS)
    r.press(m + 720, CROSS)
    r.press(m + 900, CROSS)
    _pad(r, m + start, UP)
    r.end(m + end)
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


def battle_list_bar(version):
    """P51 item 1: the bar of the battle's bar-only lists. One forced battle
    (the p79 env: Kotemon vs an enemy with 2000 HP, extra techniques, a full
    bag). Turn 1: Fight -> Tech -> Item, open the Items list and move Down
    twice, back, up to Tech, open the technique list and move Down. A
    checkpoint per cursor position: the highlighted bar must be in each."""
    r = Rec(f"{version.upper()}: battle Items / technique list bar (env in manifest)")
    for f in (350, 375, 400, 425):   # Fight -> Tech -> DV -> Tag -> Item
        r.press(f, DOWN)
    r.shot(450, "battle-cmd-items")
    r.press(460, CROSS)              # open the Items list
    r.shot(530, "items-line0")
    r.press(540, DOWN)
    r.shot(600, "items-line1")
    r.press(610, DOWN)
    r.shot(670, "items-line2")
    r.press(680, TRIANGLE)           # back to the command menu
    for f in (720, 745, 770, 795):   # Item -> Tag -> DV -> Tech -> Fight
        r.press(f, UP)
    r.press(820, DOWN)               # Fight -> Tech
    r.press(850, CROSS)              # open the technique list
    r.shot(920, "tech-line0")
    r.press(930, DOWN)
    r.shot(990, "tech-line1")
    r.press(1000, TRIANGLE)          # close the list: the log line of its bar
    r.end(1040)
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


def _to_options(r, version):
    """Title -> Options (the group list); returns the frame it is open at."""
    if version == "us":
        r.press(20, START)           # skip the opening FMV
        r.press(210, START)          # PRESS START
        m = 320                      # title menu is up
    else:
        for f in (110, 160, 190, 220, 280):   # confirm English, skip splash/FMV, PRESS START
            r.press(f, START)
        m = 390
    r.press(m, DOWN)                 # New Game -> Options
    r.press(m + 20, CROSS)           # open the Options screen (the group list)
    return m


def options_cheats_listed(version):
    """P51 item 5: the Cheats group is on the title's group list with the master
    switch off. The list (`cheats-listed`), Down to Cheats and Cross: the Cheats
    screen opens on its master switch (`cheats-open`, master OFF), Triangle
    back to the list with the cursor on Cheats (`cheats-back`), Triangle."""
    r = Rec(f"{version.upper()}: P51 Cheats group listed with the master off")
    m = _to_options(r, version)
    r.shot(m + 60, "cheats-listed")
    f = _group(r, m + 70, GRP_DISPLAY, GRP_CHEATS)
    r.shot(f + 40, "cheats-open")
    r.press(f + 60, TRIANGLE)        # back to the list
    r.shot(f + 120, "cheats-back")
    r.press(f + 140, TRIANGLE)       # leave
    r.end(f + 200)
    return r


def options_region(version, phase):
    """P51 item 4 (the combined image only, run with --bin-both): Options ->
    Saves -> Down to the Region at start row (D1: it moved here from Gameplay). Phase 1: Cross steps Ask ->
    US -> EU (`region-ask`, `region-us`, `region-eu`), Triangle out. Phase 2 is a
    new process on the same config folder: the row is read back (`region-back`)."""
    r = Rec(f"{version.upper()}: P51 Region at start row, phase {phase}")
    m = _to_options(r, version)
    f = _group(r, m + 70, GRP_DISPLAY, GRP_SAVES)
    f = _keys(r, f + 20, *([DOWN] * ROW_SV_REGION))          # Region at start
    if phase == 1:
        r.shot(f + 20, "region-ask")
        r.press(f + 40, CROSS)       # Ask -> US
        r.shot(f + 80, "region-us")
        r.press(f + 100, CROSS)      # US -> EU
        r.shot(f + 140, "region-eu")
        f += 160
    else:
        r.shot(f + 20, "region-back")
        f += 40
    r.press(f, TRIANGLE)             # back to the list
    r.press(f + 40, TRIANGLE)        # leave
    r.end(f + 100)
    return r


# --- D1 (docs/OPTIONS_REORG.md): the Menus group, the rows that were file-only keys, the
# Shared saves and Stage sweep windows and the Map buttons screen -------------------

def options_menus(version, phase):
    """D1: title -> Options -> Menus (the fourth plaque). Phase 1: the six switches
    (the five Items-menu ones and Instant menu exit, patch 0200) at their defaults
    (all ON, `menus-defaults`), Confirm on each (Down between them) turns all six OFF
    (`menus-off`); phase 2 is a fresh process on the same config dir: the page shows
    the six values read back (`menus-restored`)."""
    r = Rec(f"{version.upper()}: D1 Menus group, phase {phase}")
    m = _to_options(r, version)
    f = _group(r, m + 70, GRP_DISPLAY, GRP_MENUS)
    if phase == 1:
        r.shot(f + 20, "menus-defaults")
        f = _keys(r, f + 40, CROSS, DOWN, CROSS, DOWN, CROSS, DOWN, CROSS, DOWN, CROSS, DOWN, CROSS)
        r.shot(f + 40, "menus-off")
    else:
        r.shot(f + 40, "menus-restored")
    f = _keys(r, f + 60, TRIANGLE, TRIANGLE)           # list, then leave
    r.end(f + 40)
    return r


def options_display(version, phase):
    """D1 + D5 + P6.11: Display has nine rows (Widescreen, Internal resolution, Native res
    while turbo, Map screen, Map scaler, Output filter, Map mod pack, Version label, Text language). Phase 1: Down x4 to Map
    scaler and Left (FSR 1 -> Lanczos2, `scaler-changed`), Down to Output filter and Right x2 (Nearest ->
    Bilinear -> Sharp bilinear, `filter-changed`), Down to Map mod pack and Confirm
    (OFF), Down to Version label and Confirm (OFF) (`display-off`); phase 2 reads them back
    from the file (`display-restored`)."""
    r = Rec(f"{version.upper()}: D1 / D5 Display rows Map scaler, Output filter, Map mod pack and Version label, phase {phase}")
    m = _to_options(r, version)
    f = _group(r, m + 70, GRP_DISPLAY, GRP_DISPLAY)
    if phase == 1:
        r.shot(f + 20, "display-rows")
        f = _keys(r, f + 40, DOWN, DOWN, DOWN, DOWN, LEFT)
        r.shot(f + 40, "scaler-changed")
        f = _keys(r, f + 60, DOWN, RIGHT, RIGHT)
        r.shot(f + 40, "filter-changed")
        f = _keys(r, f + 60, DOWN, CROSS, DOWN, CROSS)
        r.shot(f + 40, "display-off")
    else:
        f = _keys(r, f + 20, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN)
        r.shot(f + 40, "display-restored")
    f = _keys(r, f + 60, TRIANGLE, TRIANGLE)
    r.end(f + 40)
    return r


def l10n_row(version, with_pack):
    """P6.7 phase 2: Options > Display, Down x8 to Text language (the ninth and last row; P6.11 put two rows before the map ones, the Output filter one after the Map scaler). With a pack:
    `row-original` (Original), Right (Portugues (BR), `row-ptbr`), Left (`row-back`). With no
    pack: `row-original-nopack` (the message line says no pack is found) and Right, which
    is not refused since phase 3 (`row-ptbr-nopack`): the port's own screens are in Portuguese,
    the game text stays original."""
    r = Rec(f"{version.upper()}: P6.7 Display > Text language, {'pack installed' if with_pack else 'no pack'}")
    m = _to_options(r, version)
    f = _group(r, m + 70, GRP_DISPLAY, GRP_DISPLAY)
    f = _keys(r, f + 20, *([DOWN] * 8))
    if with_pack:
        r.shot(f + 40, "row-original")
        f = _keys(r, f + 60, RIGHT)
        r.shot(f + 40, "row-ptbr")
        f = _keys(r, f + 60, LEFT)
        r.shot(f + 40, "row-back")
    else:
        r.shot(f + 40, "row-original-nopack")
        f = _keys(r, f + 60, RIGHT)
        r.shot(f + 40, "row-ptbr-nopack")
    f = _keys(r, f + 60, TRIANGLE, TRIANGLE)
    r.end(f + 40)
    return r


def ptbr_options(version):
    """P6.7 phase 3: the port's own screens in Portuguese (text_language=ptbr in the cfg, no
    text pack). The title menu (`menu`: the Options plaque reads Opcoes), the Options list
    (`options-list`), Display (`display`) down to its last row, Text language
    (`row-text-language`: the value and the "no text pack" line), Gameplay, Controls and Saves."""
    r = Rec(f"{version.upper()}: P6.7 phase 3, the Options in Portuguese (cfg text_language=ptbr, no pack)")
    m = _to_options(r, version)
    r.shot(m - 20, "menu")
    r.shot(m + 60, "options-list")
    f = _group(r, m + 70, GRP_DISPLAY, GRP_DISPLAY)
    r.shot(f + 30, "display")
    f = _keys(r, f + 40, *([DOWN] * 8))
    r.shot(f + 40, "row-text-language")
    f = _keys(r, f + 60, TRIANGLE)
    for frm, to, tag in ((GRP_DISPLAY, GRP_GAMEPLAY, "gameplay"), (GRP_GAMEPLAY, GRP_CONTROLS, "controls"),
                         (GRP_CONTROLS, GRP_SAVES, "saves")):
        f = _group(r, f + 20, frm, to)
        r.shot(f + 40, tag)
        f = _keys(r, f + 60, TRIANGLE)
    f = _keys(r, f + 20, TRIANGLE)
    r.end(f + 40)
    return r


def defaults_migrate(version, phase):
    """D2 (docs/OPTIONS_REORG.md 6.1): the seeded file is one an older build wrote
    (button_layout=playstation, switch_glyphs=0, no defaults_gen), so the boot migrates it.
    Phase 1: Options -> Controls, the Button layout row reads Nintendo (`migrated-controls`),
    Left puts PlayStation back (`back-to-playstation`). Phase 2 is a fresh process on the same
    config dir: the migration must not run again and PlayStation stays (`kept-playstation`)."""
    r = Rec(f"{version.upper()}: D2 defaults migration, phase {phase}")
    m = _to_options(r, version)
    f = _group(r, m + 70, GRP_DISPLAY, GRP_CONTROLS)       # Controls
    if phase == 1:
        r.shot(f + 20, "migrated-controls")
        f = _keys(r, f + 40, LEFT)                         # Nintendo -> PlayStation
        r.shot(f + 40, "back-to-playstation")
    else:
        r.shot(f + 40, "kept-playstation")
    f = _keys(r, f + 60, TRIANGLE, TRIANGLE)
    r.end(f + 40)
    return r


def defaults_view(version):
    """D2: Options -> Controls on a file the migration keeps (a custom layout) or on a
    fresh install: one picture of the Button layout row (`controls-view`)."""
    r = Rec(f"{version.upper()}: D2 Controls page after the defaults change")
    m = _to_options(r, version)
    f = _group(r, m + 70, GRP_DISPLAY, GRP_CONTROLS)       # Controls
    r.shot(f + 40, "controls-view")
    f = _keys(r, f + 60, TRIANGLE, TRIANGLE)
    r.end(f + 40)
    return r


def options_global_view(version):
    """D3 (docs/OPTIONS_REORG.md 6.2): Options -> Controls and back, no picture. The
    `*-options-global-migrate*` scenarios seed the per-region files an older build wrote
    (or none, or a blocked write) and read the proof in the log: the one global
    port_options.cfg, the merged values at boot, the lines of the migration."""
    r = Rec(f"{version.upper()}: D3 one global options file")
    m = _to_options(r, version)
    f = _group(r, m + 70, GRP_DISPLAY, GRP_CONTROLS)       # Controls
    f = _keys(r, f + 60, TRIANGLE, TRIANGLE)
    r.end(f + 40)
    return r


def options_saves(version):
    """D1: Options -> Saves -> Shared saves (experimental). Cross turns it ON: the
    Yes / No window opens with the cursor on No (`store-ask`); Cross answers No
    (`store-declined`, still OFF); again, Down to Yes and Cross (`store-on`); a Cross
    turns it OFF with no window (`store-off`)."""
    r = Rec(f"{version.upper()}: D1 Saves group, Shared saves window")
    m = _to_options(r, version)
    f = _group(r, m + 70, GRP_DISPLAY, GRP_SAVES)
    r.shot(f + 20, "saves-rows")
    f = _keys(r, f + 40, DOWN, DOWN, CROSS)            # Shared saves (the last row in both builds) ON: the window
    r.shot(f + 40, "store-ask")
    f = _keys(r, f + 60, CROSS)                        # No
    r.shot(f + 40, "store-declined")
    f = _keys(r, f + 60, CROSS)                        # the window again
    f = _keys(r, f + 40, DOWN)                         # the cursor on Yes
    r.shot(f + 40, "store-ask-yes")
    f = _keys(r, f + 60, CROSS)                        # Yes
    r.shot(f + 40, "store-on")
    f = _keys(r, f + 60, CROSS)                        # OFF again: no window
    r.shot(f + 40, "store-off")
    f = _keys(r, f + 60, TRIANGLE, TRIANGLE)
    r.end(f + 40)
    return r


def options_advanced(version):
    """D1: Options -> Advanced (listed with Cheats): Stage sweep OFF -> Run opens the
    window (`sweep-ask`), No (`sweep-declined`, still OFF); Down to Sweep frames,
    Right x2 (90 -> 150) and R1 (+300: 450) (`frames-changed`)."""
    r = Rec(f"{version.upper()}: D1 Advanced group, Stage sweep window and Sweep frames")
    m = _to_options(r, version)
    f = _group(r, m + 70, GRP_DISPLAY, GRP_ADVANCED)
    r.shot(f + 20, "advanced-rows")
    f = _keys(r, f + 40, CROSS)                        # Stage sweep -> Run: the window
    r.shot(f + 40, "sweep-ask")
    f = _keys(r, f + 60, CROSS)                        # No
    r.shot(f + 40, "sweep-declined")
    f = _keys(r, f + 60, DOWN, RIGHT, RIGHT, R1)  # Sweep frames 90 -> 120 -> 150 -> 450
    r.shot(f + 40, "frames-changed")
    f = _keys(r, f + 60, TRIANGLE, TRIANGLE)
    r.end(f + 40)
    return r


def options_mapbtn(version):
    """D1: Options -> Controls -> Map buttons (the third row). The seven bindings at
    their defaults (`mapbtn-defaults`); Zoom out Right: l1 -> r1, which Zoom in had, so
    the two swap (`mapbtn-swapped`); Down to the Reset row (the list scrolls to offset
    2, `mapbtn-scrolled`), Cross puts the defaults back (`mapbtn-reset`)."""
    r = Rec(f"{version.upper()}: D1 Map buttons screen")
    m = _to_options(r, version)
    f = _group(r, m + 70, GRP_DISPLAY, GRP_CONTROLS)
    r.shot(f + 20, "controls-rows")
    f = _keys(r, f + 40, *([DOWN] * ROW_CT_MAPBTN), CROSS)
    r.shot(f + 40, "mapbtn-defaults")
    f = _keys(r, f + 60, DOWN, RIGHT)                  # Zoom out: l1 -> r1 (swapped with Zoom in)
    r.shot(f + 40, "mapbtn-swapped")
    f = _keys(r, f + 60, *([DOWN] * 6))                # the Reset row
    r.shot(f + 40, "mapbtn-scrolled")
    f = _keys(r, f + 60, CROSS)                        # Reset map buttons
    r.shot(f + 40, "mapbtn-reset")
    f = _keys(r, f + 60, TRIANGLE, TRIANGLE, TRIANGLE)   # Controls, the list, leave
    r.end(f + 40)
    return r


def region_relaunch(version, mode):
    """P6.38 (the combined image only, --bin-both): Options -> Saves -> Region at
    start, stepped to the OTHER region (US run: Cross twice, Ask -> US -> EU; EU run:
    once, Ask -> US), then Up off the row: the window "Restart in <other> now?" opens.
      no    the window (`dialog`), No (`declined`); then the row stepped on to Ask and to
            the running region, each left with Up: no window (`ask-no-window`,
            `same-no-window`); Triangle out
      yes   the window with the cursor moved to Yes (`dialog-yes`), Cross: the relaunch
            (the manifest sets DW3_RELAUNCH_TEST, so the run logs it and exits 0)
      exec  the same Yes with the real exec (no hook): the cfg is seeded with the running
            region, Left / Right move to the other one, and the recording is written so the
            relaunched process, which replays it from frame 0 in the other region, ends it
            without a window (its first step is a no-op or leads to Ask)
      game  from the field menu's Options row (the env of `field_options`): the window
            carries the progress warning (`dialog-game`), No (`declined`)."""
    r = Rec(f"{version.upper()}: P6.38 restart into the chosen region ({mode})")
    steps = 2 if version == "us" else 1
    if mode == "game":
        f = _field_options_open(r, shots=False, last="options-open", shot_last=False)
        f = _group(r, f, GRP_DISPLAY, GRP_SAVES)
        f = _keys(r, f + 20, *([DOWN] * ROW_SV_REGION))
    else:
        m = _to_options(r, version)
        f = _group(r, m + 70, GRP_DISPLAY, GRP_SAVES)
        f = _keys(r, f + 20, *([DOWN] * ROW_SV_REGION))          # Region at start
    if mode == "exec":
        f = _keys(r, f, RIGHT if version == "us" else LEFT)
    else:
        f = _keys(r, f, *([CROSS] * steps))
        if mode != "game":
            r.shot(f + 20, "region-other")
            f += 40
    f = _keys(r, f + 20, UP)                                      # off the row: the window
    if mode == "exec":
        f = _keys(r, f + 40, DOWN, CROSS)                         # Yes
        r.end(f + 200)
        return r
    r.shot(f + 40, "dialog-game" if mode == "game" else "dialog")
    if mode == "yes":
        f = _keys(r, f + 60, DOWN)
        r.shot(f + 40, "dialog-yes")
        f = _keys(r, f + 60, CROSS)
        r.end(f + 300)
        return r
    f = _keys(r, f + 60, CROSS)                                   # No
    r.shot(f + 40, "declined")
    if mode == "game":
        f = _keys(r, f + 60, TRIANGLE, TRIANGLE)
        r.end(f + 100)
        return r
    # Ask never asks, and neither does the region that is running: US run: EU -> Ask
    # -> US, EU run: US -> EU -> Ask.
    f = _keys(r, f + 60, DOWN, CROSS)
    f = _keys(r, f + 20, UP)
    r.shot(f + 40, "ask-no-window" if version == "us" else "same-no-window")
    f = _keys(r, f + 60, DOWN, CROSS)
    f = _keys(r, f + 20, UP)
    r.shot(f + 40, "same-no-window" if version == "us" else "ask-no-window")
    f = _keys(r, f + 60, TRIANGLE, TRIANGLE)
    r.end(f + 100)
    return r


def options_persist(version, phase):
    """P9.4: title -> Options. Phase 1 turns Widescreen OFF, Fast-forward
    2x -> 3x and the Restart hotkey ON (D1: it is the last row of Controls now),
    then leaves with Triangle; phase 2 is a fresh process on the same config dir that
    only opens the screen, so its checkpoints show the values read back from
    port_options.cfg."""
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
    r.press(m + 20, CROSS)           # open the Options screen (the group list)
    if phase == 1:
        r.shot(m + 60, "options-defaults")
        f = _group(r, m + 70, GRP_DISPLAY, GRP_DISPLAY)    # Display
        r.shot(f + 20, "display-defaults")
        r.press(f + 40, CROSS)       # Widescreen ON -> OFF
        f = _keys(r, f + 60, TRIANGLE)                     # back to the list
        f = _group(r, f, GRP_DISPLAY, GRP_GAMEPLAY)        # Gameplay
        r.press(f, CROSS)            # Fast-forward 2x -> 3x
        r.shot(f + 40, "options-changed")
        f = _keys(r, f + 60, TRIANGLE)                     # back to the list
        f = _group(r, f, GRP_GAMEPLAY, GRP_CONTROLS)       # Controls
        f = _keys(r, f + 20, *([DOWN] * ROW_CT_RESTART), CROSS)   # Restart hotkey OFF -> ON
        r.shot(f + 40, "restart-changed")
        f = _keys(r, f + 60, TRIANGLE, TRIANGLE)           # list, then leave
        r.end(f + 40)
    else:
        f = _group(r, m + 70, GRP_DISPLAY, GRP_GAMEPLAY)   # Gameplay
        r.shot(f + 40, "options-restored")
        f = _keys(r, f + 60, TRIANGLE, UP, UP, CROSS)      # Display again
        r.shot(f + 40, "display-restored")
        f = _keys(r, f + 60, TRIANGLE)
        f = _group(r, f, GRP_DISPLAY, GRP_CONTROLS)        # Controls
        f = _keys(r, f + 20, *([DOWN] * ROW_CT_RESTART))
        r.shot(f + 40, "restart-restored")
        f = _keys(r, f + 60, TRIANGLE, TRIANGLE)
        r.end(f + 40)
    return r


def controls_layout(version, phase):
    """P9.8: title -> Options -> Controls -> Button layout. Phase 1 shows the
    group list (the Controls plaque), the row on PlayStation, then switches it
    to Nintendo with Confirm and leaves; phase 2 is a fresh process on the same
    config dir whose checkpoint shows the value read back from the file."""
    r = Rec(f"{version.upper()}: P9.8 Button layout row, phase {phase}")
    if version == "us":
        r.press(20, START)           # skip the opening FMV
        r.press(210, START)          # PRESS START
        m = 320                      # title menu is up
    else:
        for f in (110, 160, 190, 220, 280):   # confirm English, skip splash/FMV, PRESS START
            r.press(f, START)
        m = 390
    r.press(m, DOWN)                 # New Game -> Options
    r.press(m + 20, CROSS)           # open the Options screen (the group list)
    if phase == 1:
        r.shot(m + 60, "options-list")
        f = _group(r, m + 70, GRP_DISPLAY, GRP_CONTROLS)   # Controls
        r.shot(f + 20, "controls-playstation")
        r.press(f + 40, CROSS)       # PlayStation -> Nintendo
        r.shot(f + 80, "controls-nintendo")
        f = _keys(r, f + 100, TRIANGLE, TRIANGLE)          # list, then leave
        r.end(f + 40)
    else:
        f = _group(r, m + 70, GRP_DISPLAY, GRP_CONTROLS)   # Controls
        r.shot(f + 40, "controls-restored")
        f = _keys(r, f + 60, TRIANGLE, TRIANGLE)
        r.end(f + 40)
    return r


# P9.8 layout scenarios (docs/CONTROLS_BUNDLE.md, section 6). The replay hands the
# game the PS buttons it must see, so the map is bypassed; with the dev variable
# DW3_REPLAY_SOURCE=1 the masks are positions as a real pad reports them (Switch
# A is the Circle position, B Cross, X Triangle, Y Square) and go through the map.
# These recordings are the plain ones turned into positions under the Nintendo
# layout: the game must see exactly the original input, so every checkpoint equals
# the plain scenario's reference.
HELP_CUSTOM_DIGITS = "0123674589"    # P6.32 phase 7: L1<-ZL, R1<-ZR, L2<-L, R2<-R (two-letter glyphs)
CUSTOM_MAP_DIGITS = "0127456389"     # P6.32 phase 4: Square<-ZR position, R2<-Y position
NINTENDO_DIGITS = "1203456789"      # Cross<-A (Circle pos), Circle<-X, Triangle<-B
_FACE_BITS = {"cross": 14, "circle": 13, "triangle": 12, "square": 15,
              "l1": 10, "r1": 11, "l2": 8, "r2": 9, "l3": 1, "r3": 2}
_FACE_ORDER = ("cross", "circle", "triangle", "square", "l1", "r1", "l2", "r2", "l3", "r3")


def source_mask(mask, digits):
    """The position word that produces `mask` (PS buttons) under the button map
    `digits` (port_button_map.h: digit i is the position that makes PS button i)."""
    out = mask & ~sum(1 << b for b in _FACE_BITS.values())
    for ps, digit in enumerate(digits):
        if mask & (1 << _FACE_BITS[_FACE_ORDER[ps]]):
            out |= 1 << _FACE_BITS[_FACE_ORDER[int(digit)]]
    return out


def nintendo_source_mask(mask):
    """The position word that produces `mask` (PS buttons) under Nintendo."""
    return source_mask(mask, NINTENDO_DIGITS)


def source_rec(text, note, digits=NINTENDO_DIGITS):
    lines = text.splitlines()
    head = lines[0] if lines and lines[0].startswith("#") else "# recording"
    out = [f"{head} ({note})"]
    for line in lines[1:] if head == lines[0] else lines:
        parts = line.split()
        if parts and parts[0] == "pad":
            mask = source_mask(int(parts[2], 16), digits)
            out.append(f"pad {parts[1]} 0x{mask:04x}")
        else:
            out.append(line)
    return "\n".join(out) + "\n"


def nintendo_source_rec(text, note):
    return source_rec(text, note, NINTENDO_DIGITS)


# (new recording, plain recording it is made from)
NINTENDO_RECS = (
    ("{v}-nintendo-menu", "{v}-field-options-p1"),
    ("{v}-nintendo-map-view", "{v}-map-view"),
    ("{v}-nintendo-title-quit", "{v}-title-quit"),
    ("{v}-nintendo-map-rebind", "{v}-map-rebind"),   # P6.32 phase 4
    ("{v}-nintendo-map-help", "{v}-map-help"),       # P6.32 phase 7
)


def _controls_open(r, version):
    """Boot to the title, open Options and the Controls group (Button layout row
    focused). Returns the frame after."""
    m = _to_title_menu(r, version)
    r.press(m, DOWN)                 # New Game -> Options
    r.press(m + 20, CROSS)           # open the Options screen (the group list)
    return _group(r, m + 70, GRP_DISPLAY, GRP_CONTROLS)


def _help_controls_page(r, f):
    """From the group list with the cursor on Controls: down to Help, open it, the
    page list's second row (Controls), open the page. Returns the frame after."""
    return _keys(r, f, *([DOWN] * (GRP_HELP - GRP_CONTROLS)), CROSS, DOWN, CROSS)


def controls_custom(version, phase):
    """P6.12: Controls -> Customize buttons. The masks are button positions
    (DW3_REPLAY_SOURCE=1 in the manifest). Phase 1: the Controls rows, the Customize
    list, Left / Right is not used; the L1 row waits for a press (shot), R (the R1
    position) is pressed, so L1 <- R and R1 <- L (shot), back to Controls (Button
    layout reads Custom), then the Help: on its Controls page the L1 position now
    turns the page FORWARD and the R1 position back (the pages turn accordingly).
    Phase 2 is a fresh process on the same config dir: the custom map is read back
    (shots, the L1 position still turns forward), then Reset to default and the R1
    position turns forward again."""
    r = Rec(f"{version.upper()}: P6.12 Customize buttons, phase {phase}")
    f = _controls_open(r, version)
    if phase == 1:
        r.shot(f + 20, "controls-rows")
        f = _keys(r, f + 40, DOWN, CROSS)                  # Customize buttons
        r.shot(f + 20, "customize-list")
        f = _keys(r, f + 40, DOWN, DOWN, DOWN, DOWN, CROSS)  # the L1 row waits
        r.shot(f + 20, "customize-prompt")
        r.press(f + 40, R1)                                # the R position: L1 <- R, R1 <- L
        r.shot(f + 80, "customize-swapped")
        f = _keys(r, f + 100, TRIANGLE)                    # back to Controls
        r.shot(f + 20, "controls-custom")
        f = _keys(r, f + 40, TRIANGLE)                     # the group list
        f = _help_controls_page(r, f + 20)
        r.shot(f + 30, "help-controls")
        r.press(f + 50, L1)                                # the L position is R1 now: next page
        r.shot(f + 90, "help-turned-forward")
        r.press(f + 110, R1)                               # the R position is L1 now: back
        r.shot(f + 150, "help-turned-back")
        f = _keys(r, f + 170, TRIANGLE, TRIANGLE, TRIANGLE)  # page list, group list, close
        r.end(f + 40)
    else:
        r.shot(f + 20, "controls-restored")
        f = _keys(r, f + 40, DOWN, CROSS)                  # Customize buttons
        r.shot(f + 20, "customize-restored")
        f = _keys(r, f + 40, TRIANGLE, TRIANGLE)           # Controls, the group list
        f = _help_controls_page(r, f + 20)
        r.press(f + 30, L1)                                # still R1 (the map was read back)
        r.shot(f + 70, "help-turned-restored")
        f = _keys(r, f + 90, TRIANGLE, TRIANGLE)           # page list, group list
        f = _keys(r, f + 20, *([UP] * (GRP_HELP - GRP_CONTROLS)), CROSS)   # Controls again
        f = _keys(r, f + 20, *([DOWN] * ROW_CT_RESET), CROSS)              # Reset to default
        r.shot(f + 40, "controls-reset")
        f = _keys(r, f + 60, TRIANGLE)                     # the group list
        f = _help_controls_page(r, f + 20)
        r.press(f + 30, R1)                                # the identity again: R1 is next
        r.shot(f + 70, "help-turned-reset")
        f = _keys(r, f + 90, TRIANGLE, TRIANGLE, TRIANGLE)
        r.end(f + 40)
    return r


def controls_custom_invalid(version):
    """P6.12: port_options.cfg holds button_layout=custom with a button_map that is
    not a permutation (manifest cfg): the default applies, with one log line, and
    the Controls row reads PlayStation."""
    r = Rec(f"{version.upper()}: P6.12 invalid button_map in the cfg")
    f = _controls_open(r, version)
    r.shot(f + 30, "controls-invalid-default")
    f = _keys(r, f + 50, TRIANGLE, TRIANGLE)
    r.end(f + 40)
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
        f = _group(r, m + 70, GRP_DISPLAY, GRP_SOUND)      # Sound: Music volume
        f = _keys(r, f, LEFT, LEFT, LEFT)                  # Music volume 10 -> 7
        f = _keys(r, f, DOWN)                              # SFX volume
        f = _keys(r, f, *([LEFT] * 7))                     # SFX volume 10 -> 3
        r.shot(f + 40, "volume-changed")
        f = _keys(r, f + 50, TRIANGLE, TRIANGLE)           # list, then leave
        r.end(f + 40)
    else:
        f = _group(r, m + 70, GRP_DISPLAY, GRP_SOUND)
        r.shot(f + 40, "volume-restored")
        f = _keys(r, f + 50, TRIANGLE, TRIANGLE)
        r.end(f + 40)
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
        f = _group(r, m + 70, GRP_DISPLAY, GRP_GAMEPLAY)   # Gameplay
        f = _keys(r, f, *([DOWN] * ROW_GP_ENCOUNTER))  # Fast-forward -> Encounter rate
        f = _keys(r, f, LEFT)        # 1x -> 0.5x (the screen steps OFF, 0.5x, 1x, 1.5x, 2x)
        f = _keys(r, f, DOWN)        # Text speed
        f = _keys(r, f, RIGHT, RIGHT)  # Normal -> Fast -> Instant
        f = _keys(r, f + 20, TRIANGLE, TRIANGLE)           # list, then leave
        r.end(f + 40)
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
    f = _group(r, m + 70, GRP_DISPLAY, GRP_GAMEPLAY)       # Gameplay
    f = _keys(r, f, *([DOWN] * ROW_GP_BATTLE))   # Fast-forward -> ... -> Battle cursor
    if phase == 1:
        r.shot(f + 20, "battle-cursor-off")
        r.press(f + 40, RIGHT)       # Off -> Command
        r.shot(f + 80, "battle-cursor-command")
        r.press(f + 100, RIGHT)      # Command -> Command+Tech
        r.shot(f + 140, "battle-cursor-command-tech")
        f = _keys(r, f + 160, TRIANGLE, TRIANGLE)          # list, then leave
        r.end(f + 40)
    else:
        r.shot(f + 20, "battle-cursor-restored")
        f = _keys(r, f + 40, TRIANGLE, TRIANGLE)
        r.end(f + 40)
    return r


def options_sound(version):
    """Options screen sound feedback: title -> Options; Down (a cursor tick) and
    Cross open Sound (the choice sound), Down to SFX volume and Left twice (10
    -> 8, each change plays the choice sound at the new level), Triangle (the
    cancel sound) back to the list, Down and Cross open Gameplay, two rows down
    to Encounter rate and Right (1x -> 1.5x), Triangle twice (the cancel sound
    each). The log carries every sound with the SFX volume it was played at."""
    r = Rec(f"{version.upper()}: Options screen sound feedback")
    m = _to_title_menu(r, version)
    r.press(m, DOWN)                 # New Game -> Options
    r.press(m + 20, CROSS)           # open the Options screen
    f = _group(r, m + 70, GRP_DISPLAY, GRP_SOUND)          # Sound
    f = _keys(r, f, DOWN)            # SFX volume
    f = _keys(r, f, LEFT, LEFT)      # SFX volume 10 -> 9 -> 8
    f = _keys(r, f, TRIANGLE)        # back to the list (the cancel sound)
    f = _group(r, f, GRP_SOUND, GRP_GAMEPLAY)              # Gameplay
    f = _keys(r, f, *([DOWN] * ROW_GP_ENCOUNTER))   # Encounter rate
    f = _keys(r, f, RIGHT)           # 1x -> 1.5x
    f = _keys(r, f + 10, TRIANGLE, TRIANGLE)               # list, then leave
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


def turbo_music_field(version):
    """P6.36: the field music alone for 700 game frames with turbo 4 (env in
    manifest); the tick window lines show the gate's cadence."""
    r = Rec(f"{version.upper()}: field music idle at turbo 4, the music tick gate counts (env in manifest)")
    r.end(700)
    return r


WALKTHROUGH_END = 600


def walkthrough(version):
    """P6.35: no input; the manifest's env boots a field map at a progress value
    and the debug probe logs the resolved step (env in manifest)."""
    r = Rec(f"{version.upper()}: idle in a field map, the walkthrough probe logs the step (env in manifest)")
    r.end(WALKTHROUGH_END)
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


STAGE236_END = 800


def stage_0x236(version):
    """Walking into the Reliability Spot (WSTAG460, mode 0x236): the manifest's
    env boots in the stage before it (mode 0x235, WSTAG455) at progress 14 and
    DW3_FORCE_EXIT (patch 0110) leaves for 0x236 where event 360 does
    (SCRIPT_LEAVE(0x236, 0xD6, 0x84, 1)) after 100 idle frames. Checkpoints
    while the stage loads (the area banner), once it is up, and after Down held
    for 100 frames (the hero has control and the wanderers' tasks run)."""
    r = Rec(f"{version.upper()}: into the Reliability Spot (mode 0x236) from the stage before it, then Down (env in manifest)")
    r.shot(300, "spot-load")
    r.shot(450, "spot-arrive")
    _pad(r, 460, DOWN)
    _pad(r, 560, 0)
    r.shot(700, "spot-walked")
    r.end(STAGE236_END)
    return r


IRIS_SHOTS = (207, 216, 225, 234, 243, 252)
# 4:3 iris: in widescreen off the iris closes in 32 frames from ~205 and the
# grid in 20 more, so the late checkpoints are where the map must stay hidden.
IRIS43_SHOTS = (207, 225, 234, 240, 246, 252, 258, 264, 272)
# 1104: the walk, 1110-1122: the swirl's first frames, then its way out
SPIRAL_SHOTS = (1104, 1110, 1116, 1122, 1128, 1140, 1152, 1164, 1188, 1206)


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
    """The encounter spiral in 16:9 (patch 0175: the swirl is a copy of the
    display with its gutters, in tiles that span 16:9): the Encounter rate 1x walk of
    `encounter_walk` (Up from frame 300, battle after ~102 footsteps) with a
    checkpoint on the swirl's way out."""
    r = Rec(f"{version.upper()}: random-battle spiral, 16:9 (env in manifest)")
    _pad(r, 300, UP)
    for f in SPIRAL_SHOTS:
        r.shot(f, f"spiral-{f}")
    r.end(1270)
    return r


R3 = 0x0004


def ws_battle_spiral_turbo(version):
    """The same walk and checkpoints as `ws_battle_spiral`, with the turbo toggled
    on by R3 at frame 250 (the Fast-forward level comes from the option file, 2 to
    4): the in-game route to the speeds the Switch runs the swirl at, where the
    CPU rasterizer defers drawing. The pictures must be the turbo-4 run's."""
    r = Rec(f"{version.upper()}: random-battle spiral, 16:9, turbo toggled on with R3 (env and cfg in manifest)")
    _pad(r, 250, R3)
    _pad(r, 256, 0)
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


def items_select_equip(version):
    """P6.33 finding (patch 0180): Cross on a weapon in the Items menu's list.
    The original game does nothing (no sound, no state change); the port logs
    one `dw3-items-menu: Cross on weapon` line per press. The env of
    `items_screen` (3 of every item); the path: field menu, Items, Down to the
    second option, Cross (its list), Cross on the first item, Down, Cross on
    the second, then back out. Run with `items_partner_switch` 1 and 0: the
    result is the same, so patch 0128 is not the cause."""
    r = Rec(f"{version.upper()}: P6.33 Cross on equipment in the Items menu (env and cfg in manifest)")
    for f in range(300, 1200, 30):
        r.press(f, CROSS)            # battle report -> field
    r.press(1250, START)             # field menu
    r.press(1420, CROSS)             # Items (loads the STSTATUS overlay)
    r.press(1700, DOWN)              # the second option
    r.press(1800, CROSS)             # its list
    r.press(2000, CROSS)             # Cross on the first item: nothing happens
    r.press(2100, DOWN)
    r.press(2150, CROSS)             # and on the second
    r.press(2300, TRIANGLE)          # close the list
    r.press(2450, TRIANGLE)          # back to the field menu
    r.press(2600, TRIANGLE)          # and to the field
    r.end(2700)
    return r


def items_equip_from_items(version):
    """P6.34 phase 1 (patch 0188): equip from the Items menu. The env and cfg
    of `items_select_equip`. Items, the Weapons option and its list; Cross on
    the first weapon opens the partner step (`partner-step`), Cross on
    member 0 equips (`equipped`) or is refused (the partner cannot wear it);
    Cross closes the result and the list is rebuilt (`list-after`); Triangle
    at the partner step goes back to the list. Frames measured on the PC build."""
    r = Rec(f"{version.upper()}: P6.34 equip from the Items menu (env and cfg in manifest)")
    for f in range(300, 1200, 30):
        r.press(f, CROSS)            # battle report -> field
    r.press(1250, START)             # field menu
    r.press(1420, CROSS)             # Items (loads the STSTATUS overlay)
    r.press(1700, DOWN)              # Weapons
    r.press(1800, CROSS)             # its list
    r.shot(1990, "weapons-list")
    r.press(2000, CROSS)             # the first weapon: the partner step
    r.shot(2300, "partner-step")
    r.press(2350, CROSS)             # member 0
    r.shot(2500, "result")
    r.press(2600, CROSS)             # back to the list
    r.shot(2900, "list-after")
    r.press(3000, CROSS)             # the same weapon again
    r.press(3300, TRIANGLE)          # the partner step: back to the list
    r.shot(3600, "list-cancelled")
    r.press(3700, TRIANGLE)          # close the list
    r.end(3900)
    return r


def _eq_open(r, f, downs=0):
    """Cross on the list's item (the partner step opens), then `downs` Downs
    to the member, then Cross on that member; returns the frame after."""
    r.press(f, CROSS)                # the item: the partner step
    f += 300
    for _ in range(downs):
        r.press(f, DOWN)
        f += 40
    r.press(f, CROSS)                # the member
    return f + 150


def _eq_confirm(r, f):
    """Cross closes the result line; the list opens again on the item."""
    r.press(f, CROSS)
    return f + 300


def _eq_start(r):
    for f in range(300, 1200, 30):
        r.press(f, CROSS)            # battle report -> field
    r.press(1250, START)             # field menu
    r.press(1420, CROSS)             # Items (loads the STSTATUS overlay)


def items_equip_refused(version):
    """P6.34 phase 1 (patch 0188): a partner that cannot wear the item stays
    selectable and Cross is refused. The first weapon (Short Sword) fits only
    member 0: Cross on member 1 and on member 2 is refused (`refused`, the
    "cannot equip" line), then Up Up and Cross on member 0 equips it."""
    r = Rec(f"{version.upper()}: P6.34 equip from Items, partner refused (env and cfg in manifest)")
    _eq_start(r)
    r.press(1700, DOWN)              # Weapons
    r.press(1800, CROSS)             # its list
    r.press(2000, CROSS)             # the first weapon: the partner step
    r.press(2300, DOWN)              # member 1
    r.press(2350, CROSS)             # refused
    r.shot(2450, "refused-1")
    r.press(2500, DOWN)              # member 2
    r.press(2550, CROSS)             # refused
    r.shot(2650, "refused-2")
    r.press(2700, UP)                # member 1: the refusal line goes
    r.press(2750, UP)                # member 0
    r.press(2800, CROSS)             # equipped
    r.shot(2950, "equipped")
    r.press(3000, CROSS)             # the list again
    r.end(3400)
    return r


def items_equip_replace(version):
    """P6.34 phase 1 (patch 0188): both hand slots full. Three weapons that fit
    member 0 go on in turn (the first two fill slots 2 and 3, the third replaces
    slot 2 and the first returns to the bag, said in the result line), then the
    first armor and the first accessory. The bag and worn totals of `done`
    must move by exactly one each time."""
    r = Rec(f"{version.upper()}: P6.34 equip from Items, both slots and every list (env and cfg in manifest)")
    _eq_start(r)
    r.press(1700, DOWN)              # Weapons
    r.press(1800, CROSS)             # its list
    f = _eq_open(r, 2000)            # item 1 -> slot 2
    f = _eq_confirm(r, f)
    r.press(f, DOWN)                 # item 2
    f = _eq_open(r, f + 60)          # -> slot 3
    f = _eq_confirm(r, f)
    r.press(f, DOWN)                 # item 3
    f = _eq_open(r, f + 60)          # both full: replaces slot 2
    r.shot(f, "replaced")
    f = _eq_confirm(r, f)
    r.shot(f, "list-after-replace")
    r.press(f, TRIANGLE)             # close the list
    f += 300
    r.press(f, DOWN)                 # Armor
    r.press(f + 60, CROSS)           # its list
    f = _eq_open(r, f + 300)
    r.shot(f, "armor")
    f = _eq_confirm(r, f)
    r.press(f, TRIANGLE)
    f += 300
    r.press(f, DOWN)                 # Accessories
    r.press(f + 60, CROSS)
    f = _eq_open(r, f + 300)
    r.shot(f, "accessory")
    f = _eq_confirm(r, f)
    r.end(f + 100)
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


def menu_instant_exit(version):
    """Instant menu exit (patch 0200): the exits from the field menu's screens to
    the walking field, three of them in one run. The env and cfg of `items_menu`
    (the party from a forced event battle; Cross walks the report back to the
    field). 1) Start opens the field menu over the field and Triangle closes it
    again: no mode change, no dissolve, so no exit line. 2) Start, Items, its
    list, Triangle twice (back to the field menu inside STSTATUS), Triangle: the
    exit to the field, `from items`. 3) Start, Down twice, Cross (Map), Triangle
    (back to the field menu), Triangle: the exit, `from map`. Checkpoints 60
    frames into each exit (with the key on the field is loading or loaded, with
    it off the dissolve is running) and well after, with the field live again.
    The same recording runs with the key on and off; the waits cover the longer
    original fade (about 150 frames to the field)."""
    r = Rec(f"{version.upper()}: instant menu exit, three exits (env and cfg in manifest)")
    for f in range(300, 1200, 30):
        r.press(f, CROSS)            # battle report -> field
    r.press(1250, START)             # field menu, over the field
    r.shot(1400, "field-menu")
    r.press(1420, TRIANGLE)          # close it: stays in the field
    r.shot(1560, "field-menu-closed")
    r.press(1600, START)
    r.press(1780, CROSS)             # Items: the dissolve, then STSTATUS
    r.shot(2000, "item-options")
    r.press(2010, CROSS)             # the Items list
    r.shot(2160, "item-list")
    r.press(2200, TRIANGLE)          # close the list
    r.press(2400, TRIANGLE)          # back to the field menu inside STSTATUS
    r.shot(2540, "status-menu")
    r.press(2560, TRIANGLE)          # exit 1, from items
    r.shot(2620, "exit-items-60")
    r.shot(3000, "field-after-items")
    r.press(3050, START)             # the field answers: menu again
    r.press(3250, DOWN)
    r.press(3300, DOWN)              # Map
    r.press(3350, CROSS)             # the dissolve, then STSTATUS and the Map screen
    r.shot(3700, "map")
    r.press(3720, TRIANGLE)          # close the Map: field menu inside STSTATUS
    r.shot(3940, "status-menu-2")
    r.press(3960, TRIANGLE)          # exit 2, from map
    r.shot(4020, "exit-map-60")
    r.shot(4400, "field-after-map")
    r.end(4450)
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


def items_equip_switch(version):
    """P6.33 phase 1 (patch 0128): R2 / L2 change the partner on the equip
    panel's slot list. The path of `menu_screens` (the manifest's env and cfg)
    to the slot list of the second party member (the `status-equip` step),
    then R2, R2 (wraps), L2, Down to the second slot, R2 (the slot cursor
    stays), Cross (the item list of the new partner), Triangle, Triangle.
    The log line `dw3-items-menu: partner switch` carries the bag and worn
    totals, which must not change. Frames measured on the PC build."""
    r = Rec(f"{version.upper()}: P6.33 equip panel partner cycle (env and cfg in manifest)")
    for f in range(300, 1200, 30):
        r.press(f, CROSS)            # battle report -> field
    r.press(1250, START)             # field menu, over the field
    r.press(1420, CROSS)             # Items: the dissolve, then STSTATUS
    f = 1640
    for name, button, wait in MENU_SCREENS_STEPS:
        if name == "status-equip":
            break
        if button:
            r.press(f, button)
        f += wait
    # f: the equipment panel's slot list is up for the second partner
    r.shot(f, "slots-before")
    steps = (("switch-next", R2), ("switch-wrap", R2), ("switch-prev", L2),
             ("slot-moved", DOWN), ("switch-slot", R2), ("item-list", CROSS),
             (None, TRIANGLE), (None, TRIANGLE))
    f += 40
    for name, button in steps:
        r.press(f, button)
        if name:
            r.shot(f + 30, name)
        f += 90
    r.end(f + 100)
    return r


def items_equip_stays(version):
    """P6.34 phase 2 (patch 0190): equip in Status > Equipment and come back to
    the slot list without the long fade round trip. The path of
    `items_equip_switch` to the slot list of the second partner; Down twice (the
    right hand slot), Cross (the list), Down, Cross (the first fitting item
    is worn: the panel is back on the slot list, `after-equip`), Cross again (the
    list opens on the worn item, which has spare copies: `list-on-worn`), Down,
    Cross (a second equip), Cross (the list again), Triangle (the original
    long way back), Triangle. The waits after an equip are long enough for the
    original flow too, so the same recording runs with the key off.
    Frames measured on the PC build."""
    r = Rec(f"{version.upper()}: P6.34 equip stays on the slot list (env and cfg in manifest)")
    for f in range(300, 1200, 30):
        r.press(f, CROSS)            # battle report -> field
    r.press(1250, START)             # field menu, over the field
    r.press(1420, CROSS)             # Items: the dissolve, then STSTATUS
    f = 1640
    for name, button, wait in MENU_SCREENS_STEPS:
        if name == "status-equip":
            break
        if button:
            r.press(f, button)
        f += wait
    r.shot(f, "slots-before")
    r.press(f + 40, DOWN)            # head -> body
    r.press(f + 80, DOWN)            # right hand
    f += 120
    r.press(f, CROSS)               # the list for the slot
    r.shot(f + 200, "list-first")
    r.press(f + 210, DOWN)
    r.press(f + 260, CROSS)         # equips the first item that fits
    f += 300
    r.shot(f, "after-equip-1")       # key on: slot list, cursor on the right hand
    f += 200
    r.press(f, CROSS)               # the list again: on the worn item
    r.shot(f + 220, "list-on-worn")
    r.press(f + 230, DOWN)
    r.shot(f + 260, "list-on-worn-down")
    r.press(f + 280, CROSS)         # a second equip
    f += 320
    r.shot(f, "after-equip-2")
    f += 200
    r.press(f, CROSS)               # the list again
    r.shot(f + 220, "list-again")
    r.press(f + 230, TRIANGLE)      # cancel: the original way back
    f += 450
    r.shot(f, "slots-after")
    r.press(f + 20, TRIANGLE)
    r.end(f + 300)
    return r


def items_bag_view(version):
    """P6.34 phase 3 (patch 0192): the whole bag in the equip list and moving an
    item between Digimon. The path of `items_equip_stays` to the first accessory
    slot of the second party member (B; the accessories are the items several
    Digimon can wear, the weapons are one Digimon's each): Cross, Down, Cross wears the first fitting
    item. R2 switches to the third member (C); Cross opens the list, Square three
    times (all weapons, all armor and gear, worn by others: B's item is a
    row, the wearer note shows), Down, Cross moves the item to C. L2 back to B,
    Cross, Square x3 (C's item is the row), Down, Cross moves it back. Square a
    fourth time returns to this slot's list; Triangle, Triangle. With the key off
    Square does nothing. Frames measured on the PC build."""
    r = Rec(f"{version.upper()}: P6.34 bag view and move between Digimon (env and cfg in manifest)")
    for f in range(300, 1200, 30):
        r.press(f, CROSS)            # battle report -> field
    r.press(1250, START)             # field menu, over the field
    r.press(1420, CROSS)             # Items: the dissolve, then STSTATUS
    f = 1640
    for name, button, wait in MENU_SCREENS_STEPS:
        if name == "status-equip":
            break
        if button:
            r.press(f, button)
        f += wait
    for k in range(4):
        r.press(f + 40 * (k + 1), DOWN)  # head, body, right hand, left hand, first accessory
    f += 200
    r.press(f, CROSS)                # B: the list for the slot
    r.press(f + 60, DOWN)
    r.press(f + 110, CROSS)          # B wears the first fitting item
    f += 300
    r.shot(f, "b-worn")
    r.press(f + 40, R2)              # C
    f += 80
    r.press(f, CROSS)                # C: the list for the accessory
    r.shot(f + 150, "c-list-slot")
    for name in ("c-weapons", "c-gear", "c-worn"):
        f += 160
        r.press(f, SQUARE)
        r.shot(f + 100, name)
    r.press(f + 120, DOWN)           # the first row: B's item, the wearer note
    r.shot(f + 170, "c-worn-row")
    r.press(f + 200, CROSS)          # moved to C
    f += 520
    r.shot(f, "c-moved")
    r.press(f + 40, L2)              # back to B
    f += 100
    r.press(f, CROSS)
    for name in ("b-weapons", "b-gear", "b-worn-filter"):
        f += 160
        r.press(f, SQUARE)
        r.shot(f + 100, name)
    r.press(f + 120, DOWN)
    r.shot(f + 170, "b-worn-row")
    r.press(f + 200, CROSS)          # moved back to B
    f += 520
    r.shot(f, "b-moved")
    r.press(f + 40, CROSS)           # the list again
    r.press(f + 200, SQUARE)         # key on: weapons; key off: nothing
    r.shot(f + 300, "b-list-again")
    r.press(f + 340, TRIANGLE)
    f += 800
    r.press(f, TRIANGLE)
    r.end(f + 300)
    return r


def items_use_stays(version):
    """P6.34 phase 4 (patch 0193): "use item" stays on the partner step. The
    path of `menu_screens` to the partner step of the Items list (row 4 of the usable
    items, 3 copies in the test bag). Member B (Down): use, the
    result stays in the help window, the owned count falls; Down to C: use; Up
    twice to A: the last copy is used the original way (the pages close, the
    list reopens without the row); Cross; Triangle twice leaves the screen.
    With the key off every use takes the original way back, so the recording is
    the same and the screenshots of the `-off` run are the pre-patch ones.
    Frames measured on the PC build."""
    r = Rec(f"{version.upper()}: P6.34 use item stays on the partner step (env and cfg in manifest)")
    for f in range(300, 1200, 30):
        r.press(f, CROSS)            # battle report -> field
    r.press(1250, START)             # field menu, over the field
    r.press(1420, CROSS)             # Items: the dissolve, then STSTATUS
    f = 1640
    for name, button, wait in MENU_SCREENS_STEPS:
        if name == "item-list-page2":
            break
        if button:
            r.press(f, button)
        f += wait
    r.press(f, L1)                   # page 1, the cursor on its first row (item 43, a potion
    f += 80                          # that does nothing at full HP)
    for k in range(4):
        r.press(f + 40 * k, DOWN)    # row 4: the first stat item (item 47), 3 copies
    r.press(f + 200, CROSS)          # the partner step
    f += 400
    r.shot(f - 3, "partner-step")
    r.press(f, DOWN)                 # member B
    r.press(f + 120, CROSS)          # use 1
    r.shot(f + 220, "use-1")
    r.press(f + 260, DOWN)           # member C
    r.shot(f + 360, "moved")
    r.press(f + 400, CROSS)          # use 2
    r.shot(f + 500, "use-2")
    r.press(f + 540, UP)
    r.press(f + 600, UP)             # member A
    r.press(f + 680, CROSS)          # use 3: the last copy
    r.shot(f + 780, "use-3")
    f += 900
    r.press(f, CROSS)                # the original way back to the list
    r.shot(f + 400, "list-after")
    r.press(f + 450, TRIANGLE)
    r.press(f + 700, TRIANGLE)
    r.end(f + 1000)
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


CIRCLE_MAP, SQUARE_MAP, R3_MAP = 0x2000, 0x8000, 0x0004
L1_MAP, R1_MAP, L2_MAP, R2_MAP = 0x0400, 0x0800, 0x0100, 0x0200
# Bits above the 16 pad bits stand for the right stick in a recording (the
# host turns them into the analog axis the zoom reads, port/psyz_missing/
# psyz_pad_host.c): 0x10000 up (zoom in), 0x20000 down (zoom out).
RSTICK_UP, RSTICK_DOWN = 0x10000, 0x20000


def map_view(version):
    """P6.32 phase 0 (patch 0146): the new full-screen map screen. Same path as
    `map_screen` up to the open Map; Square opens the new screen (the captured
    map with its reveal layer, sharp bilinear, zoom 1.0), the pad pans it, R1 /
    L1 step the zoom (1.25, 1.5, 2.0, 3.0, then back down to fit), the right
    stick zooms smoothly (`mv-stick-in`, `mv-stick-out`), R2 cycles the scaler (Lanczos2
    and FSR 1 are built on the CPU, with a log line each), R3 does nothing (no
    turbo), Circle shows the help bars (hidden at open; the view opens at 1.5x), Triangle returns to the unchanged game
    Map and a second Triangle closes it. With widescreen off (the `-43`
    scenario) Square is ignored and the screen never opens. The host draws the
    screen, so the references come from the PC build only."""
    r = Rec(f"{version.upper()}: P6.32 new full-screen map screen (env and cfg in manifest)")
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
    assert f == 4000, f
    r.shot(3997, "game-map")
    r.press(4000, SQUARE_MAP)        # the new screen
    r.shot(4040, "mv-open")
    # the cursor (patch 0146): it starts on the current area (balloon, hover
    # cursor, the game's name box), leaves it to the right, comes back, then
    # goes down to the next visited location
    _pad(r, 4050, RIGHT)
    _pad(r, 4070, 0)
    r.shot(4080, "mv-cursor-free")
    _pad(r, 4090, LEFT)
    _pad(r, 4110, 0)
    r.shot(4120, "mv-cursor-back")
    _pad(r, 4130, RIGHT)         # to the map's right edge and the bottom: the
    _pad(r, 4170, 0)             # arrow stays on the map
    _pad(r, 4180, DOWN)
    _pad(r, 4240, 0)
    r.shot(4245, "mv-cursor-down")
    f = 4250
    # P51 item 2: the view opens at 1.5x; two steps down is the 1.0 the zoom
    # tour below starts from (it used to be the opening zoom)
    r.press(f, L1_MAP)
    r.press(f + 20, L1_MAP)
    r.shot(f + 40, "mv-zoom-100-open")
    f += 60
    for name, button in (("mv-zoom-125", R1_MAP), ("mv-zoom-150", R1_MAP),
                         ("mv-zoom-200", R1_MAP), ("mv-zoom-300", R1_MAP)):
        r.press(f, button)
        r.shot(f + 20, name)
        f += 40
    for name, button in (("mv-zoom-out-200", L1_MAP), (None, L1_MAP), (None, L1_MAP),
                         (None, L1_MAP), ("mv-fit", L1_MAP)):
        r.press(f, button)
        if name:
            r.shot(f + 20, name)
        f += 40
    r.press(f, R1_MAP)               # back to 1.0 for the scalers
    r.shot(f + 20, "mv-zoom-100")
    f += 40
    for name in ("mv-sharp", "mv-lanczos2", "mv-fsr"):  # starts on nearest
        r.press(f, R2_MAP)           # the CPU builds stall the game thread, not the frame count
        r.shot(f + 100, name)
        f += 120
    # P6.32 phase 3: Bilinear follows FSR 1 in the cycle (five scalers); the next press
    # wraps to nearest, in the same 120-frame window the last scaler always had
    r.press(f, R2_MAP)               # bilinear
    r.shot(f + 30, "mv-bilinear")
    r.press(f + 50, R2_MAP)          # nearest again
    r.shot(f + 100, "mv-nearest")
    f += 120
    r.press(f, R3_MAP)               # turbo toggle: nothing
    r.press(f + 30, CIRCLE_MAP)      # P51 item 2: the bars come on (off at open)
    r.shot(f + 60, "mv-hud-on")
    r.press(f + 70, CIRCLE_MAP)      # and go again
    # the right stick zooms smoothly (the owner's request): up 40 frames from
    # the 1.0 step, then down 40 frames; both end between the table steps
    _pad(r, f + 90, RSTICK_UP)
    _pad(r, f + 130, 0)
    r.shot(f + 150, "mv-stick-in")
    _pad(r, f + 160, RSTICK_DOWN)
    _pad(r, f + 190, 0)
    r.shot(f + 210, "mv-stick-out")
    r.press(f + 230, TRIANGLE)       # back to the game's Map
    r.shot(f + 290, "mv-closed")
    r.press(f + 330, TRIANGLE)       # the game's Triangle closes the Map
    r.end(f + 530)
    return r


SELECT_MAP = 0x0001


def map_zoom(version):
    """P6.32 phase 3 (no patch): zoom about the cursor, the step ladder, the pan
    limits, the centre-on-your-area button (Select), the cursor's acceleration and
    analog speed (left stick), the FIT backdrop and the bilinear scaler. Same path as
    `map_view` up to the open Map; Square opens the new screen at 1.5x on the current
    area; the cursor leaves the view centre, then L1 / R1 walk the ladder (the log
    line of every step says the cursor stayed where it was, and where the pan limit
    or the map fitting the picture made it move), the right stick zooms smoothly, a
    long hold at 1.0 accelerates, holds at 3.0 push the view to every corner, Select
    centres on the current area, the left stick (the host test aid DW3_MAP_LSTICK, the
    replay pad is digital) moves the cursor at half and full tilt, L2 selects bilinear. The host draws the screen: references from the PC build."""
    r = Rec(f"{version.upper()}: P6.32 phase 3 zoom about the cursor, pan limits, centre, cursor speed, backdrop (env and cfg in manifest)")
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
    assert f == 4000, f
    r.press(4000, SQUARE_MAP)        # the new screen, 1.5x on the current area
    r.shot(4040, "zm-open")
    # the cursor leaves the middle of the picture (left and up, into the middle of the
    # map: the zoom steps below are then free of the image's edge); the view follows
    _pad(r, 4050, LEFT | UP)
    _pad(r, 4090, LEFT)
    _pad(r, 4100, 0)
    r.shot(4120, "zm-cursor-off-centre")
    # zoom in about the cursor: 1.5 -> 2.0 -> 3.0 (-> 3.0: the top of the ladder)
    r.press(4130, R1_MAP)
    r.shot(4150, "zm-in-200")
    r.press(4160, R1_MAP)
    r.shot(4180, "zm-in-300")
    r.press(4190, R1_MAP)
    # and out again: 3.0 -> 2.0 -> 1.5 -> 1.25 -> 1.0 -> FIT
    r.press(4210, L1_MAP)
    r.press(4230, L1_MAP)
    r.shot(4250, "zm-out-150")
    r.press(4260, L1_MAP)
    r.press(4280, L1_MAP)
    r.shot(4300, "zm-out-100")       # 1.0: the map (1176 px) is narrower than the picture
    r.press(4310, L1_MAP)
    r.shot(4330, "zm-fit")           # FIT: bars at the sides, the backdrop of the map's colours
    # back up to 1.5 with the cursor still where it was
    r.press(4340, R1_MAP)
    r.press(4360, R1_MAP)
    r.press(4380, R1_MAP)
    r.press(4400, R1_MAP)
    r.shot(4420, "zm-back-150")
    # the right stick zooms smoothly, about the cursor too
    _pad(r, 4430, RSTICK_UP)
    _pad(r, 4470, 0)
    r.shot(4480, "zm-stick-in")
    _pad(r, 4490, RSTICK_DOWN)
    _pad(r, 4560, 0)
    r.shot(4570, "zm-stick-out")
    # a long hold at 1.0 accelerates (the game's 2, 3, 4 px a frame, then +1 every
    # 15 frames) and takes the cursor into the left strip the PS1 cursor cannot reach
    for k in range(5):
        r.press(4580 + 15 * k, L1_MAP)   # down to FIT, whatever the smooth zoom was
    r.press(4660, R1_MAP)                # FIT -> 1.0
    _pad(r, 4690, LEFT)
    _pad(r, 4850, 0)
    r.shot(4860, "zm-accel-left")
    # holds at 3.0 push the view into every corner: the pan limit is the image box
    for k in range(4):
        r.press(4870 + 20 * k, R1_MAP)   # 1.0 -> 1.25 -> 1.5 -> 2.0 -> 3.0
    r.shot(4960, "zm-300-left-strip")
    _pad(r, 4970, UP | LEFT)
    _pad(r, 5130, 0)
    r.shot(5140, "zm-corner-top-left")
    _pad(r, 5150, DOWN | RIGHT)
    _pad(r, 5390, 0)
    r.shot(5400, "zm-corner-bottom-right")
    # Select: back to the current area, the view centred on it
    r.press(5410, SELECT_MAP)
    r.shot(5440, "zm-centre")
    # the left stick: half tilt is slow, full tilt is the d-pad's speed. A digital replay
    # pad has no stick: the host test aid DW3_MAP_LSTICK (manifest env) moves it, half tilt
    # left from game frame 5510 to 5570, full tilt left from 5590 to 5620
    r.press(5450, L1_MAP)
    r.press(5470, L1_MAP)
    r.press(5490, L1_MAP)
    r.shot(5580, "zm-stick-half")
    r.shot(5630, "zm-stick-full")
    # the bilinear scaler (L2 from nearest wraps to it), then back
    r.press(5640, L2_MAP)
    r.shot(5670, "zm-bilinear")
    r.press(5680, R2_MAP)
    r.shot(5710, "zm-nearest")
    r.press(5730, TRIANGLE)          # back to the game's Map
    r.shot(5790, "zm-closed")
    r.press(5830, TRIANGLE)          # the game's Triangle closes the Map
    r.end(5930)
    return r


# P6.32 phase 2: the tour. The view opens at frame 4000 and the host test aid
# DW3_MAP_TOUR=8 (manifest env) puts the cursor on the next target every 8 frames,
# starting at view frame 10: first the edge and town probes, then every visited
# location in id order (DW3_FORCE_MAP_AREAS=all marks the 46 visited). The game's own
# name window answers each one and the host logs a `dw3-map: area name shown` line.
# MAP_NAMES_SHOTS are (target index from 0, name): the frame is the step's own plus 3,
# once the name box has been drawn.
MAP_TOUR_PERIOD, MAP_TOUR_START, MAP_TOUR_OPEN = 8, 10, 4001
MAP_NAMES_SHOTS = (
    (0, "names-strip-left-town"),   # the cursor in the left strip, beside town 13: no name
    (6, "names-strip-top"),         # top strip, on the way to the topmost location
    (7, "names-spot06-top"),        # location 6, the topmost
    (15, "names-spot14-2lines"),    # location 14, two lines
    (17, "names-strip-right"),
    (18, "names-spot16-right"),     # location 16, the rightmost
    (24, "names-spot22-widest"),    # location 22, the widest name in four languages
    (32, "names-spot29"),           # location 29, the widest in German
    (45, "names-strip-left-spot39"),
    (47, "names-spot39-left"),      # location 39, the leftmost
    (58, "names-spot45-bottom"),
    (59, "names-strip-bottom"),
)
MAP_NAMES_END = 4560


def map_names(version):
    """P6.32 phase 2 (patches 0146, 0147): the area name under the new cursor for
    every visited location. Same path as `map_view` up to the open Map; Square opens
    the new screen, then the host's tour (env, see MAP_TOUR_PERIOD) walks the cursor
    over every location with the whole map marked visited, so each of the 46 names
    comes from the game's own name window; the log has a line per name. The shots
    are the name box at the edges of the map (left, right, top, bottom), a strip probe
    and the widest names. One recording for the six disc languages (the EU scenarios
    differ in DW3_LANGUAGE)."""
    r = Rec(f"{version.upper()}: P6.32 area names under the new cursor, every location (env and cfg in manifest)")
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
    assert f == 4000, f
    r.shot(3997, "game-map")
    r.press(4000, SQUARE_MAP)        # the new screen
    r.shot(4008, "names-open")       # the opening name box (the current area), before the tour starts
    for k, name in MAP_NAMES_SHOTS:
        r.shot(MAP_TOUR_OPEN + MAP_TOUR_START + MAP_TOUR_PERIOD * k + 3, name)
    r.press(MAP_NAMES_END, TRIANGLE)  # back to the game's Map
    r.shot(MAP_NAMES_END + 60, "names-closed")
    r.press(MAP_NAMES_END + 100, TRIANGLE)
    r.end(MAP_NAMES_END + 300)
    return r


def map_view_off(version):
    """P6.32 phase 1: the `map_screen` option off (the default). Same path as
    `map_screen` up to the open Map; then everything the new screen would take is
    pressed on the game's own Map: Square (it must not open), the cursor, R3, L1 /
    R1 / R2 / Circle, which the game's screen does not read. The game's Map screen
    keeps working (the cursor moves, `off-cursor`) and Triangle leaves it for the
    field menu (`off-closed`). Checkpoints: the Map before and after Square (the
    only difference may be the cursor animation)."""
    r = Rec(f"{version.upper()}: P6.32 map_screen option off: Square does nothing (env and cfg in manifest)")
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
    assert f == 4000, f
    r.shot(3997, "off-map")
    r.press(4000, SQUARE_MAP)        # the option is off: nothing opens
    r.shot(4040, "off-square")
    r.press(4060, R3_MAP)
    r.press(4070, L1_MAP)
    r.press(4080, R1_MAP)
    r.press(4090, R2_MAP)
    r.press(4100, CIRCLE_MAP)
    _pad(r, 4120, RIGHT)             # the game's own cursor still moves
    _pad(r, 4180, 0)
    r.shot(4200, "off-cursor")
    r.press(4220, TRIANGLE)          # the game's Triangle closes the Map
    r.shot(4300, "off-closed")
    r.end(4400)
    return r


L3_MAP, SELECT_MAP_BIT = 0x0002, 0x0001


def _to_map_screen(r):
    """The path of `map_view` to the open Map screen; returns the frame (4000)."""
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
    assert f == 4000, f
    return f


def map_rebind(version):
    """P6.32 phase 4: the view's buttons from the file-only keys of port_options.cfg
    (manifest: open=r2 zoom_out=square zoom_in=cross centre=l3 scaler_prev=l1
    scaler_next=r1; map_screen=1). Same path as `map_view` up to the open Map.
    Square, the old open key, opens nothing; R2 opens the view; in it the old keys
    (L2, Select) say they are not bound and do nothing; L3 centres, Square steps the
    zoom down and Cross up (about the cursor), L1 / R1 move the scaler (bilinear,
    then nearest again), R2 is only logged as the open key, Triangle closes. The
    masks are logical buttons, so the Nintendo and custom-map variants (button
    positions, DW3_REPLAY_SOURCE=1) must give the same pictures and log."""
    r = Rec(f"{version.upper()}: P6.32 phase 4 rebound map view buttons (env and cfg in manifest)")
    f = _to_map_screen(r)
    r.shot(f - 3, "rb-game-map")
    r.press(f, SQUARE_MAP)           # the old open key: the game's Map screen ignores it
    r.shot(f + 40, "rb-square-nothing")
    r.press(f + 60, R2_MAP)          # the bound open key
    r.shot(f + 110, "rb-open")
    _pad(r, f + 120, LEFT | UP)      # the cursor leaves the area
    _pad(r, f + 170, 0)
    r.shot(f + 190, "rb-cursor-off")
    r.press(f + 200, L2_MAP)         # old scaler key: not bound any more
    r.press(f + 215, SELECT_MAP_BIT) # old centre key: not bound any more
    r.shot(f + 240, "rb-old-keys")
    r.press(f + 250, L3_MAP)         # centre on the current area
    r.shot(f + 270, "rb-centre")
    r.press(f + 280, SQUARE_MAP)     # zoom out one step: 1.5 -> 1.25
    r.shot(f + 300, "rb-zoom-out")
    r.press(f + 310, CROSS)          # zoom in: back to 1.5
    r.press(f + 330, CROSS)          # and on to 2.0
    r.shot(f + 350, "rb-zoom-in")
    r.press(f + 360, L1_MAP)         # previous scaler: nearest wraps to bilinear
    r.shot(f + 390, "rb-bilinear")
    r.press(f + 400, R1_MAP)         # next scaler: nearest again
    r.shot(f + 430, "rb-nearest")
    r.press(f + 440, R2_MAP)         # the open key inside the view: only logged
    r.press(f + 460, CIRCLE_MAP)     # the bars (fixed): they list the new buttons
    r.shot(f + 490, "rb-hud")
    r.press(f + 500, CIRCLE_MAP)
    r.press(f + 520, TRIANGLE)       # the cancel button closes the view
    r.shot(f + 580, "rb-closed")
    r.press(f + 620, TRIANGLE)       # and the game's Map screen
    r.end(f + 800)
    return r


# P6.32 phase 6: the field hotkey (patch 0148). The path of `map_view` to the idle field (the
# forced battle, its report, Cross to the field), then the key goes down on the walking field.
# FH_OPEN is the frame the key goes down, FH_CLOSE the frame Triangle closes the view. The field
# is held from FH_OPEN to the frame it runs again (FH_OPEN + FH_D replay frames later), so every
# field frame after it happens FH_D frames later than in the control run, which never opened the
# view: the checkpoints after the return are taken FH_D frames later than the control's.
FH_OPEN = 1300
FH_CLOSE = 1700
FH_D = 405   # frames the field was held (measured, see docs/P6.32_MAP_ZOOM.md 18.4)
FH_AFTER = ((20, "fk-after-return"), (120, "fk-after-120"), (260, "fk-walk"), (420, "fk-end"))


def _to_field(r):
    for f in range(300, 1200, 30):
        r.press(f, CROSS)            # battle report -> field
    r.shot(FH_OPEN - 20, "fk-before")


def field_map_ctl(version):
    """P6.32 phase 6, the control run: the idle field, no key, the same walk. The session
    run must reproduce these pictures FH_D frames later, pixel for pixel."""
    r = Rec(f"{version.upper()}: P6.32 phase 6 field hotkey control run, no key (env in manifest)")
    _to_field(r)
    r.shot(FH_OPEN + 60, "fk-off-60")  # the option-off run's checkpoints (the field never stopped there)
    r.shot(FH_OPEN + 160, "fk-off-160")
    base = FH_CLOSE + 12 - FH_D        # the control frame the session run's return corresponds to
    for off, name in FH_AFTER:
        if name == "fk-walk":
            _pad(r, base + 180, RIGHT)
            _pad(r, base + 230, 0)
        r.shot(base + off, name)
    r.end(base + 520)
    return r


def field_map(version, key=SELECT_MAP, old_key=None):
    """P6.32 phase 6: `key` goes down in the idle field (default Select): the field is held and
    the game's Map screen comes up in the field menu's slot with the view open on it
    (`fk-open`, 1.5x on your area, bars hidden). R3 is pressed (it does nothing: turbo is off
    in the view and the fast-forward state is kept), the cursor moves (`fk-cursor`), and the view
    stays open for hundreds of frames (`fk-late`: the same picture, the field behind it has
    not moved). Triangle closes it; the field runs again from the same frame: the control run's
    pictures, FH_D frames later, are reproduced (manifest ref_map), a walk included."""
    r = Rec(f"{version.upper()}: P6.32 phase 6 field hotkey, view open {FH_CLOSE - FH_OPEN} frames "
            "(env and cfg in manifest)")
    _to_field(r)
    if old_key is not None:
        r.press(FH_OPEN - 60, old_key)  # the old key after a rebind: nothing opens, nothing is logged
    r.press(FH_OPEN, key)
    r.shot(FH_OPEN + 120, "fk-open")
    r.press(FH_OPEN + 140, R3_MAP)      # nothing: no turbo here, and the toggle is not flipped
    _pad(r, FH_OPEN + 160, RIGHT)       # the view's cursor moves
    _pad(r, FH_OPEN + 190, 0)
    r.shot(FH_OPEN + 210, "fk-cursor")
    r.shot(FH_CLOSE - 20, "fk-late")
    r.press(FH_CLOSE, TRIANGLE)         # the cancel button closes the view, and the Map screen with it
    base = FH_CLOSE + 12                # the field runs again from about here
    for off, name in FH_AFTER:
        if name == "fk-walk":
            _pad(r, base + 180, RIGHT)
            _pad(r, base + 230, 0)
        r.shot(base + off, name)
    r.end(base + 520)
    return r


def field_map_43(version):
    """P6.32 phase 6, widescreen off (DW3_WIDESCREEN=0 in the manifest, the option on): the key
    does nothing and the log says why. Log only: the 4:3 picture has no control run here."""
    r = Rec(f"{version.upper()}: P6.32 phase 6 field hotkey with widescreen off (env in manifest)")
    for f in range(300, 1200, 30):
        r.press(f, CROSS)            # battle report -> field
    r.press(FH_OPEN, SELECT_MAP)
    r.press(FH_OPEN + 100, SELECT_MAP)
    r.end(FH_OPEN + 260)
    return r


def field_map_off(version):
    """P6.32 phase 6, the option off (the default): the key goes down and nothing happens
    except the log line that says why; every picture is the control run's at the same frame
    (the field never stopped)."""
    r = Rec(f"{version.upper()}: P6.32 phase 6 field hotkey with map_screen off (env in manifest)")
    _to_field(r)
    r.press(FH_OPEN, SELECT_MAP)
    r.shot(FH_OPEN + 60, "fk-off-60")
    r.press(FH_OPEN + 100, SELECT_MAP)  # and once more: one line each
    r.shot(FH_OPEN + 160, "fk-off-160")
    r.end(FH_OPEN + 260)
    return r


def map_rebind_relaunch_p1(version):
    """P6.32 phase 4, process 1 of the relaunch test: the title's Options, the Display
    group, its fourth row (Full-screen map; P6.11 put two rows before it) from OFF to ON; the file the screen writes
    keeps the binding keys the manifest seeded. Back out."""
    r = Rec(f"{version.upper()}: P6.32 phase 4 Full-screen map row, phase 1")
    m = _to_options(r, version)
    f = _group(r, m + 70, GRP_DISPLAY, GRP_DISPLAY)    # Display
    f = _keys(r, f + 20, DOWN, DOWN, DOWN)             # Widescreen -> Internal resolution -> Native res -> Full-screen map
    r.shot(f + 20, "fm-off")
    r.press(f + 40, CROSS)                             # OFF -> ON
    r.shot(f + 80, "fm-on")
    f = _keys(r, f + 100, TRIANGLE, TRIANGLE)          # list, then leave
    r.end(f + 40)
    return r


def map_rebind_relaunch_p2(version):
    """Process 2 (a fresh start on the same config folder, in the Map screen's forced
    state): the option and the bindings come back from the file process 1 rewrote.
    Square does nothing, R2 opens the view, Square zooms out, Triangle closes."""
    r = Rec(f"{version.upper()}: P6.32 phase 4 rebound map view buttons, phase 2 (relaunch)")
    f = _to_map_screen(r)
    r.shot(f - 3, "rr-game-map")
    r.press(f, SQUARE_MAP)           # not the open key any more
    r.press(f + 60, R2_MAP)          # the bound open key, read back from the file
    r.shot(f + 110, "rr-open")
    r.press(f + 120, SQUARE_MAP)     # zoom out, the rebound key
    r.shot(f + 150, "rr-zoom-out")
    r.press(f + 170, TRIANGLE)
    r.shot(f + 230, "rr-closed")
    r.press(f + 270, TRIANGLE)
    r.end(f + 450)
    return r


def map_help(version):
    """P6.32 phase 7: the help line of the full-screen map view. Same path as `map_view` to
    the open Map; Square opens the view (default bindings), Circle shows the bars (the help
    line is the green footer), Circle hides them, Triangle closes. The masks are logical
    buttons, so the Nintendo and custom-map variants (button positions, DW3_REPLAY_SOURCE=1)
    are the same walk; what differs is the text of the footer, which is the point."""
    r = Rec(f"{version.upper()}: P6.32 phase 7 map view help line (env and cfg in manifest)")
    f = _to_map_screen(r)
    r.press(f, SQUARE_MAP)           # opens at 1.5x on the current area
    r.shot(f + 50, "mh-open")        # no bars: the picture fills the screen
    r.press(f + 60, CIRCLE_MAP)      # the bars come on
    r.shot(f + 100, "mh-help")
    r.press(f + 110, R1_MAP)         # a zoom step: the bars stay, the help line is built again
    r.shot(f + 140, "mh-help-zoom")
    r.press(f + 150, CIRCLE_MAP)     # and go
    r.press(f + 170, TRIANGLE)       # the view closes, the game's Map comes back
    r.press(f + 230, TRIANGLE)       # and the Map screen
    r.end(f + 400)
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


def item_shop(version):
    """STITSHOP: the shop (250: money bar, Buy / Sell panel, back plate), the
    Buy list (Cross, 500), the details of the first item (580), its effects
    page (Circle, 700), the quantity (Cross, 900) and the total (Cross, 1100)
    with money from the cheat hook."""
    r = Rec(f"{version.upper()}: P6.1 item shop in widescreen (env in manifest)")
    r.shot(250, "shop")
    r.press(300, CROSS)              # Buy
    r.shot(500, "buy-list")
    r.press(520, 0x2000)             # Circle: effects page
    r.shot(700, "buy-effects")
    r.press(720, 0x2000)             # back to the description
    r.press(760, CROSS)              # the first item
    r.shot(900, "buy-quantity")
    r.press(950, CROSS)              # the quantity
    r.shot(1100, "buy-total")
    r.end(1200)
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
    and read back, and the one-shots not kept. P9.9: one checkpoint on the
    Cheats sub-screen per phase (`cheats-p1`: the rows just set, `cheats-p2`:
    the toggles read back)."""
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
    r.shot(f + 10, f"cheats-p{phase}")
    f += 30
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
    19, Give item ready, Triangle twice. The logs carry what was armed. P9.9: two
    checkpoints (`cheats-warp`, `cheats-give`)."""
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
    r.shot(f + 10, "cheats-warp")    # P9.9: the list has scrolled (Warp, row 7 of 10)
    f += 30
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
    r.shot(f, "cheats-give")         # P9.9: the end of the list (Give item, row 10 of 10)
    f += 20
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
    r = Rec(f"{version.upper()}: restart after a battle, then the same battle again (env in manifest); the chord is 20 frames earlier than before the return fade, so the reset lands on the same frame (the battle RNG depends on the frame count)")
    _restart_battle_inputs(r)
    r.shot(1000, "battle-1-late")
    r.shot(1575, "field-after-battle")
    base = 1600
    # the restart now fades to black for 20 frames before it resets (restart.c)
    r.press(base - 20, CHORD)
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


def title_ready(version):
    """PRESS START only when the game is ready to answer it (patch 0199): Start
    is pressed on every other frame from the first frame of the boot (a press
    is the odd frames, a release the even ones, since the game sees an edge),
    through the EU language select and the movie skip, until the title takes
    it. The log carries the proof: the frame the prompt is drawn, the frame
    the pad starts to be read for it, and the first press accepted."""
    r = Rec(f"{version.upper()}: Start pressed every other frame from boot (title readiness)")
    for f in range(1, 700):
        r.press(f, START if f % 2 else 0)
    r.end(700)
    return r


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
    to the last row, below Options since P9.7 (`return-focus`), Cross opens the window on No
    (`confirm-no`), Cross answers No (`after-no`), Cross again, Down to Yes
    (`confirm-yes`), Cross answers Yes: the P6.9 reset, back at the title
    (`back-at-title`; on the EU disc the language select first,
    `back-at-select`). rows7: the field menu inside STSTATUS (Items, then
    Triangle), eight rows since P9.7, the window and Triangle cancelling it."""
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
    for _ in range(7 if rows7 else 6):   # to the last row (P9.7: Options is above it)
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


SQUARE_MASK = 0x8000


def _field_options_open(r, shots=True, last="options-open", shot_last=True):
    """Report -> field, Start (the field menu, `field-menu`), Down x5 to the
    Options row (`options-focus`), Cross: the port Options screen over the menu
    (`options-open`, frame 1620). Seven rows, no card folders (the env of
    `return_title`). Returns the frame after the open checkpoint."""
    for f in range(300, 1200, 30):
        r.press(f, CROSS)            # battle report -> field
    r.press(1250, START)             # the field menu
    if shots:
        r.shot(1400, "field-menu")
    f = 1420
    for _ in range(5):               # ITEMS .. STATUS, then Options
        r.press(f, DOWN)
        f += 20
    if shots:
        r.shot(f + 20, "options-focus")
    r.press(f + 40, CROSS)           # the Options screen opens (the menu waits)
    if shot_last:
        r.shot(f + 120, last)
    return f + 140


def field_options(version, phase=1):
    """P9.7 slice 1: the port Options screen from the field menu (env in the
    manifest: the party of the forced event battle as `return_title`; no cfg).
    Phase 1: the screen opens on the group list (`options-open`); Cross opens
    Display (`display-open`), Cross on Widescreen changes nothing (it is locked
    in game: `widescreen-locked`); Triangle, Down, Cross: Sound, Left x3 (Music
    10 -> 7, `music-changed`); Triangle, Down, Cross: Gameplay, Down to
    Encounter rate, Left x2 (1x -> 0.5x -> OFF, `encounter-off`); Triangle back to the
    list and Triangle again close the screen back to the field menu
    (`options-closed`); the menu still takes input (Down to Return to title,
    `menu-alive`) and Triangle leaves it for the field (`field-after`).
    Phase 2 is a new process on the same config dir: the same opening, and the
    groups show what phase 1 saved (`options-restored`: Music 7;
    `options-restored-gameplay`: Encounter OFF; Widescreen still ON)."""
    r = Rec(f"{version.upper()}: P9.7 port Options from the field menu, phase {phase} (env in manifest)")
    if phase == 2:
        f = _field_options_open(r, shots=False, last="options-open-restored")
        f = _group(r, f, GRP_DISPLAY, GRP_SOUND)           # Sound: Music 7
        r.shot(f + 40, "options-restored")
        f = _keys(r, f + 60, TRIANGLE)
        f = _group(r, f, GRP_SOUND, GRP_GAMEPLAY)          # Gameplay: Encounter OFF
        f = _keys(r, f, *([DOWN] * ROW_GP_ENCOUNTER))
        r.shot(f + 40, "options-restored-gameplay")
        r.end(f + 100)
        return r
    f = _field_options_open(r)
    r.press(f, CROSS)                # the Display group
    r.shot(f + 40, "display-open")
    r.press(f + 60, CROSS)           # Widescreen: locked, nothing changes
    r.shot(f + 120, "widescreen-locked")
    f = _keys(r, f + 140, TRIANGLE)  # back to the list
    f = _group(r, f, GRP_DISPLAY, GRP_SOUND)               # Sound: Music volume
    f = _keys(r, f, LEFT, LEFT, LEFT)                      # 10 -> 7
    r.shot(f + 40, "music-changed")
    f = _keys(r, f + 60, TRIANGLE)
    f = _group(r, f, GRP_SOUND, GRP_GAMEPLAY)              # Gameplay
    f = _keys(r, f, *([DOWN] * ROW_GP_ENCOUNTER))   # Fast-forward, Encounter rate
    f = _keys(r, f, LEFT, LEFT)      # 1x -> 0.5x -> OFF
    r.shot(f + 40, "encounter-off")
    f = _keys(r, f + 60, TRIANGLE)   # back to the list
    r.press(f, TRIANGLE)             # close: back on the field menu
    r.shot(f + 60, "options-closed")
    r.press(f + 80, DOWN)            # the menu has the pad again
    r.shot(f + 140, "menu-alive")
    r.press(f + 160, TRIANGLE)       # leave the menu: the field resumes
    r.shot(f + 360, "field-after")
    r.end(f + 420)
    return r


# --- P6.30 technical help ---------------------------------------------------
# The Help group is the ninth plaque of the group list (D1: Display, Sound,
# Gameplay, Menus, Controls, Saves, Cheats, Advanced, then Help). The L1 / R1 masks are the page keys of the book.


def _help_reading(r, f):
    """In the help (the page list is up): the list, the second page (Controls)
    scrolled by a line and by screenfuls to its end, the next two pages, two pages
    back, Back to the list, Back out of the help to the group list. Returns the frame after."""
    r.shot(f + 30, "help-list")
    f = _keys(r, f + 40, DOWN)           # Controls
    r.shot(f + 10, "help-list-controls")
    f = _keys(r, f + 20, CROSS)          # open the page
    r.shot(f + 30, "controls-top")
    f = _keys(r, f + 40, DOWN, DOWN, DOWN)
    r.shot(f + 10, "controls-line3")
    f = _keys(r, f + 20, RIGHT, RIGHT, RIGHT)   # screenfuls to the end
    r.shot(f + 10, "controls-end")
    f = _keys(r, f + 20, R1)             # next page: Port hotkeys
    r.shot(f + 10, "hotkeys-top")
    f = _keys(r, f + 20, R1)             # Turbo (the Setup page is not a checkpoint: it
    r.shot(f + 10, "turbo-top")          # shows the run's temp folder; help_main.c checks it)
    f = _keys(r, f + 20, L1, L1)         # back two pages: Controls again
    r.shot(f + 10, "controls-back")
    f = _keys(r, f + 20, TRIANGLE)       # to the page list
    r.shot(f + 10, "help-list-back")
    f = _keys(r, f + 20, TRIANGLE)       # out of the help: the group list
    r.shot(f + 10, "options-after")
    return f + 20


def help_title(version):
    """P6.30: title -> Options -> Help, the pages as above, then Triangle
    closes the Options screen. Checkpoints: `options-list` (the group list with
    the new Help plaque), then `_help_reading`."""
    r = Rec(f"{version.upper()}: P6.30 technical help from the title")
    m = _to_title_menu(r, version)
    r.press(m, DOWN)                     # New Game -> Options
    r.press(m + 20, CROSS)               # open the Options screen
    r.shot(m + 60, "options-list")
    f = _group(r, m + 70, GRP_DISPLAY, GRP_HELP)
    f = _help_reading(r, f)
    r.press(f, TRIANGLE)                 # close Options
    r.end(f + 60)
    return r


def help_field(version):
    """P6.30: the help from the field menu's OPTIONS row (env in the manifest, as
    `field_options`): the same reading, then Triangle closes the Options screen
    and the field menu has the pad again."""
    r = Rec(f"{version.upper()}: P6.30 technical help from the field menu (env in manifest)")
    f = _field_options_open(r, shots=False, shot_last=False)
    f = _group(r, f, GRP_DISPLAY, GRP_HELP)
    f = _help_reading(r, f)
    r.press(f, TRIANGLE)                 # close Options: back on the field menu
    r.press(f + 60, DOWN)                # the menu has the pad again
    r.shot(f + 120, "menu-alive")
    r.end(f + 180)
    return r


def help_battle(version):
    """P6.30: the help from the battle's Options (Select at the command menu; env
    in the manifest as `battle_options`): the page list, Controls and its end,
    then Triangle three times: the battle is released at the command menu."""
    r = Rec(f"{version.upper()}: P6.30 technical help from the battle (env in manifest)")
    r.press(BO_SELECT, SELECT)
    f = BO_SELECT + 50
    f = _group(r, f, GRP_DISPLAY, GRP_HELP)
    r.shot(f + 30, "help-list")
    f = _keys(r, f + 40, DOWN, CROSS)    # Controls
    r.shot(f + 30, "controls-top")
    f = _keys(r, f + 40, RIGHT, RIGHT, RIGHT)
    r.shot(f + 10, "controls-end")
    f = _keys(r, f + 20, TRIANGLE, TRIANGLE, TRIANGLE)   # list, group list, close
    r.shot(f + 40, "battle-after")
    r.end(f + 120)
    return r


def help_lang(idx):
    """P6.30 (EU): the help in one of the other four languages (fr, de, it, es:
    machine-drafted text, needs native review). The page list, the Controls page
    and the Options page (the longest one) are captured in the language."""
    lang = EU_LANGS[idx]
    r = Rec(f"EU: P6.30 technical help, language {lang} (machine-drafted text)")
    f = 90
    for _ in range(idx):
        r.press(f, DOWN)
        f += 15
    c = f + 20
    r.press(c, START)                    # confirm the language
    for k in (50, 80, 110):              # skip splash / FMV stages
        r.press(c + k, START)
    r.press(c + 170, START)              # PRESS START
    m = c + 270
    r.press(m, DOWN)                     # Start -> Options
    r.press(m + 20, CROSS)
    r.shot(m + 60, "options-list")
    f = _group(r, m + 70, GRP_DISPLAY, GRP_HELP)
    r.shot(f + 30, "help-list")
    f = _keys(r, f + 40, DOWN, CROSS)
    r.shot(f + 30, "controls-top")
    f = _keys(r, f + 40, TRIANGLE, DOWN, DOWN, DOWN, CROSS)   # list, Options explained
    r.shot(f + 30, "options-top")
    f = _keys(r, f + 40, TRIANGLE, TRIANGLE, TRIANGLE)
    r.end(f + 60)
    return r


# --- P6.35 phase 2: the walkthrough screen ----------------------------------
# The Walkthrough group is the tenth plaque of the group list (Cheats is listed
# in the PC build, greyed in game), right after Help. In game the screen opens
# on the page of the current step; from the title it is a browse-only list.
# (tag, name of the current page in the checkpoints, Downs from it to the next entry). The
# game's own field menu opens from GAME.progress 4 (field.c openFieldMenu), so no earlier
# story point can reach the Options screen in game: p4 closes chapter 1, p14 (progress 0x14) is
# the middle of chapter 5, p2b (progress 0x2B) is the last battle (chapter 11) and p2d (progress
# 0x2D) is the EU extra chapter's first step; on US p2d is past the table (region gating: the
# chapter list with the "not recognised" notice, no extra chapter). Since the condensing (FEEDBACK-5)
# the current step is read on the page that holds it: p14 (step ch05.s03) opens the page ch05.s01.
WT_FIELD_POINTS = (("p4", "s06", 1), ("p14", "s01", 1), ("p2b", "s01", 1), ("p2d", "s01", 1))


def _wt_reading(r, f, current, other_down):
    """In the walkthrough, opened on the page of the current step (`current`
    names it). A step: the page with its answer closed, Confirm (the question,
    cursor on No), Down and Confirm (the answer), Back to the chapter's steps (the
    cursor on the step), `other_down` Downs and Confirm on the next entry, Confirm
    (its question) and Back (cancelled), Back to the steps, Back to the chapters, and
    out. `current` "gate" (US at progress 0x2D, past its table): the chapter list with the notice,
    then out. Returns the frame after."""
    r.shot(f + 40, f"{current}-page")
    if current == "gate":
        f += 60
        r.shot(f + 20, "wt-list")
        f = _keys(r, f + 40, TRIANGLE)       # out of the walkthrough
    else:
        f = _keys(r, f + 60, CROSS)
        r.shot(f + 20, f"{current}-ask")
        f = _keys(r, f + 40, DOWN, CROSS)
        r.shot(f + 20, f"{current}-answer")
        f = _keys(r, f + 40, TRIANGLE)       # the chapter's steps, the cursor on the step
        r.shot(f + 20, "wt-list")
        f = _keys(r, f + 40, *([DOWN] * other_down), CROSS)
        r.shot(f + 20, "other-page")
        f = _keys(r, f + 40, CROSS)
        r.shot(f + 20, "other-ask")
        f = _keys(r, f + 40, TRIANGLE, TRIANGLE)   # cancel, then the steps
        r.shot(f + 20, "wt-list-back")
        f = _keys(r, f + 40, TRIANGLE)       # the chapter list
        r.shot(f + 20, "wt-chapters")
        f = _keys(r, f + 40, TRIANGLE)       # out of the walkthrough
    r.shot(f + 20, "options-after")
    return f + 40


def walkthrough_field(version, current, ahead_down):
    """P6.35 phase 2: the walkthrough from the field menu's OPTIONS row at a
    forced story point (env in the manifest: DW3_START_MODE and DW3_PROGRESS, plus the
    forced event battle of `field_options`, which gives the player a party: the field
    menu does not open without one)."""
    r = Rec(f"{version.upper()}: P6.35 walkthrough screen from the field menu, {current} (env in manifest)")
    f = _field_options_open(r, shots=False, last="options-list")
    f = _group(r, f, GRP_DISPLAY, GRP_WALK)
    f = _wt_reading(r, f, current, ahead_down)
    r.press(f, TRIANGLE)                     # close Options: back on the field menu
    r.end(f + 80)
    return r


# P6.35 slice 4: the side content. (tag, downs from the current step to the first optional row).
# Shown: the walkthrough opens at the first value of a chapter that has an optional block (ch04 at
# 0xF, ch08 at 0x1E, and on EU ch12 at 0x2D), the steps list shows the block and its two rows are
# read. Hidden: one value before the chapter (0xE, 0x1D, EU 0x2B) the chapter list marks the next
# chapter as not reached and its steps list has no optional row.
WT_SIDE_SHOWN = (("pf", 1), ("p1e", 2), ("p2d", 3))   # Downs from the current page to the first optional row
WT_SIDE_HIDDEN = ("pe", "p1d", "p2b")


def walkthrough_side_shown(version, tag, downs):
    """In game, the chapter's first value: Back to the steps (the block is listed), the first optional row
    (page, question, answer), then the second (page), and out."""
    r = Rec(f"{version.upper()}: P6.35 walkthrough optional rows listed at {tag} (env in manifest)")
    f = _field_options_open(r, shots=False, last="options-list")
    f = _group(r, f, GRP_DISPLAY, GRP_WALK)
    r.shot(f + 40, "cur-page")
    f = _keys(r, f + 60, TRIANGLE)           # the chapter's steps, the cursor on the current step
    r.shot(f + 20, "steps-with-optional")
    f = _keys(r, f + 40, *([DOWN] * downs), CROSS)   # the first optional row
    r.shot(f + 20, "side1-page")
    f = _keys(r, f + 40, CROSS)              # the question, cursor on No
    r.shot(f + 20, "side1-ask")
    f = _keys(r, f + 40, DOWN, CROSS)        # the answer
    r.shot(f + 20, "side1-answer")
    f = _keys(r, f + 40, TRIANGLE, DOWN, CROSS)    # the steps, the second optional row
    r.shot(f + 20, "side2-page")
    f = _keys(r, f + 40, TRIANGLE, TRIANGLE, TRIANGLE)   # steps, chapters, out of the walkthrough
    r.shot(f + 20, "options-after")
    r.press(f + 60, TRIANGLE)                # close Options: back on the field menu
    r.end(f + 140)
    return r


def walkthrough_side_hidden(version, tag):
    """In game, one value before the chapter: Back twice (the chapter list), Down to the next chapter (not
    reached yet), its steps: no optional row. Then out."""
    r = Rec(f"{version.upper()}: P6.35 walkthrough optional rows hidden before their chapter, {tag} (env in manifest)")
    f = _field_options_open(r, shots=False, last="options-list")
    f = _group(r, f, GRP_DISPLAY, GRP_WALK)
    r.shot(f + 40, "cur-page")
    f = _keys(r, f + 60, TRIANGLE, TRIANGLE)     # the steps, the chapter list
    r.shot(f + 20, "wt-chapters")
    f = _keys(r, f + 40, DOWN)                   # the next chapter: not reached yet
    r.shot(f + 20, "chapter-ahead")
    f = _keys(r, f + 40, CROSS)                  # its steps: titles hidden, no optional row
    r.shot(f + 20, "ahead-steps")
    f = _keys(r, f + 40, TRIANGLE, TRIANGLE)     # chapters, out of the walkthrough
    r.shot(f + 20, "options-after")
    r.press(f + 60, TRIANGLE)                    # close Options: back on the field menu
    r.end(f + 140)
    return r


def walkthrough_title(version, lang_idx=0):
    """P6.35 phase 2: title -> Options -> Walkthrough. No game state is read
    there: the step list opens in browse mode, the first step is the only one
    with a title. `lang_idx` > 0 (EU): the same in that language."""
    lang = EU_LANGS[lang_idx]
    r = Rec(f"{version.upper()}: P6.35 walkthrough screen from the title, browse mode, {lang}")
    if lang_idx == 0:
        m = _to_title_menu(r, version)
    else:
        f = 90
        for _ in range(lang_idx):
            r.press(f, DOWN)
            f += 15
        c = f + 20
        r.press(c, START)                    # confirm the language
        for k in (50, 80, 110):              # skip splash / FMV stages
            r.press(c + k, START)
        r.press(c + 170, START)              # PRESS START
        m = c + 270
    r.press(m, DOWN)                         # New Game -> Options
    r.press(m + 20, CROSS)                   # open the Options screen
    r.shot(m + 60, "options-list")
    f = _keys(r, m + 70, *([DOWN] * GRP_WALK))   # the cursor on the Walkthrough plaque
    r.shot(f + 20, "group-focus")            # its lettering and the message line under the list
    f = _keys(r, f + 40, CROSS)              # open it: the chapter list
    r.shot(f + 30, "wt-list")
    f = _keys(r, f + 40, CROSS)              # chapter 1: its steps
    r.shot(f + 20, "wt-steps")
    f = _keys(r, f + 40, CROSS)              # the first step
    r.shot(f + 20, "first-page")
    f = _keys(r, f + 40, CROSS)              # the question
    r.shot(f + 20, "first-ask")
    f = _keys(r, f + 40, DOWN, CROSS)        # the answer
    r.shot(f + 20, "first-answer")
    f = _keys(r, f + 40, TRIANGLE, DOWN, CROSS)   # steps, second step: not reached
    r.shot(f + 20, "second-page")
    f = _keys(r, f + 40, TRIANGLE, TRIANGLE, TRIANGLE, TRIANGLE)   # steps, chapters, group list, close
    r.end(f + 60)
    return r


def turbo_menu(version):
    """P6.4: menus run at 1.5x under turbo (env in the manifest: `field_options`
    env, turbo 4; no checkpoints, the proof is the `dw3-turbo:` log). Report ->
    field (full turbo), Start (the field menu list), Down x5 and Cross: the
    port Options screen over it, Triangle closes it, Down to Return to title,
    Cross: the Yes / No window on No, Triangle cancels it, Triangle leaves the
    menu, then 200 idle field frames (full turbo again)."""
    r = Rec(f"{version.upper()}: P6.4 turbo menus at 1.5x (env in manifest)")
    f = _field_options_open(r, shot_last=False, shots=False)
    r.press(f, TRIANGLE)             # close Options: back on the field menu
    r.press(f + 60, DOWN)            # Return to title
    r.press(f + 120, CROSS)          # the Yes / No window
    r.press(f + 200, TRIANGLE)       # cancel it
    r.press(f + 260, TRIANGLE)       # leave the menu: the field resumes
    r.end(f + 520)
    return r


def field_options_lang(idx):
    """P9.7: the Options row and the screen's lock note in one of the other four
    EU languages (env: the `field_options` env and DW3_LANGUAGE): the field menu
    (the row's text in the game's font), the screen with Widescreen focused (its
    message line is the lock note in that language), Triangle."""
    lang = EU_LANGS[idx]
    r = Rec(f"EU: P9.7 port Options from the field menu in {lang} (env in manifest)")
    f = _field_options_open(r)
    r.press(f, CROSS)                # Display: the lock note in that language
    r.shot(f + 60, "display-locked")
    f = _keys(r, f + 80, TRIANGLE, TRIANGLE)               # list, then close
    r.shot(f + 40, "options-closed")
    r.end(f + 100)
    return r


def field_options_release(version):
    """P9.7 on the Switch build (#268 releases the title overlay's textures
    90 draws after they leave the screen): the screen opens (first use), closes,
    stays away for 200 frames (the atlas is released), opens again
    (`reopen-early` 3 frames after the press, then `reopen`: it must not show a
    blank or garbage frame), closes, 200 more frames, then Down to Return to title
    and its Yes / No window (`confirm-early`, `confirm`: the dialog texture is
    rebuilt), Triangle cancels. Env is baked into the replay NRO (manifest)."""
    r = Rec(f"{version.upper()}: P9.7 Options and the confirm window after the texture release (Switch)")
    f = _field_options_open(r, shots=False, last="first-open")
    r.shot(f + 120, "first-open-late")   # Ryubing compiles the overlay shader on first use: the early shot can miss it
    r.press(f + 140, TRIANGLE)       # close
    f += 360                         # > 90 draws: atlas and dialog texture released
    r.press(f, CROSS)                # open again, on the Options row
    r.shot(f + 3, "reopen-early")
    r.shot(f + 100, "reopen")
    r.press(f + 120, TRIANGLE)
    f += 360
    r.press(f, DOWN)                 # Return to title
    r.press(f + 30, CROSS)           # the Yes / No window (its texture is rebuilt)
    r.shot(f + 33, "confirm-early")
    r.shot(f + 100, "confirm")
    r.press(f + 120, TRIANGLE)       # cancel
    r.shot(f + 180, "confirm-closed")
    r.end(f + 220)
    return r


SELECT = 0x0001

# P9.7 slice 2: the battle excursion. The forced battle of p77_battle (Tech
# once, Cross to the report and the field). BO_SELECT is the frame Select is
# pressed at (the command menu is up: the battle is idle there), BO_CLOSE the
# frame Triangle closes the screen; the battle is held from the frame after
# Select to the one Triangle is seen, so every later game frame happens
# BO_HELD = BO_CLOSE - BO_SELECT replay frames later than in the plain battle.
BO_SELECT = 330
BO_CLOSE = 630
BO_HELD = BO_CLOSE - BO_SELECT
# Plain battle: checkpoints (name, frame). The excursion run takes the same
# pictures BO_HELD frames later and must match them pixel for pixel.
BO_SHOTS = ((310, "battle-before"), (345, "battle-after"), (400, "battle-tech"),
            (520, "battle-turn"), (700, "battle-mid"))
BO_END = 1500


def battle_options_ref(version):
    """P9.7 slice 2, the control run: the forced battle without the Options
    screen, with the checkpoints the excursion run must reproduce."""
    r = Rec(f"{version.upper()}: P9.7 battle control run, no Options (env in manifest)")
    _restart_battle_inputs(r)
    for frame, name in BO_SHOTS:
        r.shot(frame, name)
    r.end(BO_END)
    return r


def battle_options(version):
    """P9.7 slice 2: Select at the battle's command menu opens the port Options
    screen over the held battle (`options-open`, the group list); Display and
    Widescreen refuse a change (`widescreen-locked`), Sound, Left x3 on Music
    volume (10 -> 7, `music-changed`); the screen stays open for BO_HELD frames
    (`options-held`: the battle behind it has not moved), Triangle goes back to the
    list and Triangle again closes it. The battle then goes on from the same frame: the
    plain battle's checkpoints, BO_HELD frames later, are pixel-identical
    (manifest ref_map), the Tech is used and the battle ends in the report and
    the field as the plain one."""
    r = Rec(f"{version.upper()}: P9.7 Options at the battle command menu, "
            f"{BO_HELD} frames held (env in manifest)")
    h = BO_HELD
    # The plain battle's inputs: before the excursion as they are, after it shifted.
    def after(f):
        return f + h if f > BO_SELECT else f
    _pad(r, after(350), DOWN)
    _pad(r, after(360), 0)
    r.press(after(410), CROSS)
    r.press(after(460), CROSS)
    for f in range(520, 1470, 30):
        r.press(after(f), CROSS)
    r.press(BO_SELECT, SELECT)
    r.shot(BO_SELECT + 30, "options-open")
    f = BO_SELECT + 50
    r.press(f, CROSS)                # the Display group
    r.press(f + 20, CROSS)           # Widescreen: locked, nothing changes
    r.shot(f + 50, "widescreen-locked")
    f = _keys(r, f + 60, TRIANGLE)   # back to the list
    f = _group(r, f, GRP_DISPLAY, GRP_SOUND)               # Sound: Music volume
    f = _keys(r, f, LEFT, LEFT, LEFT)                      # 10 -> 7
    r.shot(f + 10, "music-changed")
    assert f + 10 < BO_CLOSE - 40
    r.press(BO_CLOSE - 30, TRIANGLE)  # back to the list
    r.shot(BO_CLOSE - 10, "options-held")
    r.press(BO_CLOSE, TRIANGLE)      # close
    for frame, name in BO_SHOTS:
        if frame > BO_SELECT:
            r.shot(frame + h, name)
        else:
            r.shot(frame, name)
    r.end(BO_END + h)
    return r


def field_options_cheats(version):
    """P9.7: the Cheats entry is title-only. With the cheats master on (cfg
    cheat_enabled=1) the group list shows the Cheats group greyed; Square and
    Cross on that group do nothing in game (`cheats-locked`,
    `cheats-group-locked`: still the group list)."""
    r = Rec(f"{version.upper()}: P9.7 Cheats entry locked in game (env and cfg in manifest)")
    f = _field_options_open(r)
    r.press(f, SQUARE_MASK)          # the shortcut: refused
    r.shot(f + 60, "cheats-locked")
    f = _keys(r, f + 80, *([DOWN] * GRP_CHEATS), CROSS)    # the greyed Cheats group
    r.shot(f + 40, "cheats-group-locked")
    r.press(f + 60, TRIANGLE)
    r.end(f + 120)
    return r


# --- P6.11 internal resolution (docs/P6.11_INTERNAL_RES.md section 5) -----------------

def res_switch(version):
    """P6.11 phase 3: the two forced battles of `two_battles` with fast-forward toggled by
    R3 inside the first one: on at frame 470 (the picture drops to 1x), off at 490 (back to
    the chosen multiplier). The checkpoints are the ones of `two_battles`, so the capture
    after the switch can be held against the steady run's (turbo changes the speed, never
    the frame a picture belongs to). The manifest seeds Fast-forward 4x in the option file
    and no DW3_TURBO, so R3 is what turns it on."""
    door, tech2, end = TWO_BATTLES[version]
    r = Rec(f"{version.upper()}: two forced battles, turbo toggled on and off by R3 in the first (env and cfg in manifest)")
    _tech(r, 350)
    r.press(470, R3)                 # turbo on: dw3-res: turbo on -> 1x
    r.press(490, R3)                 # turbo off: dw3-res: turbo off -> Nx
    r.shot(500, "battle1-tech")
    for f in range(520, door - 20, 30):
        r.press(f, CROSS)
    r.shot(tech2 - 20, "battle2-menu")
    _tech(r, tech2)
    r.press(tech2 + 40, R3)          # on again inside the second battle
    r.press(tech2 + 100, R3)         # and off
    r.shot(tech2 + 130, "battle2-tech")
    for f in range(tech2 + 150, end - 100, 30):
        r.press(f, CROSS)
    r.shot(end - 10, "field-after")
    r.end(end)
    return r


RES_MOVIE_SHOT = 14000   # EU: the opening's second movie (MOVIED01) starts just after frame 13800
RES_MOVIE_END = 14050    # and runs about 450 frames; the shot is 200 frames into it


def res_movie(version):
    """P6.11: the New Game opening cut inside its movie, one checkpoint there. The movie is
    drawn from the RGB24 region and must be unscaled at 2x (the window grab shows it)."""
    r = Rec(f"{version.upper()}: New Game opening, cut inside the movie")
    new_game_opening(r, version)
    r.lines = [l for l in r.lines
               if not l.startswith(("shot", "end"))
               and (l.startswith("#") or int(l.split()[1]) <= RES_MOVIE_END)]
    r.shot(RES_MOVIE_SHOT, "opening-movie")
    r.end(RES_MOVIE_END)
    return r


def options_res(version, phase):
    """P6.11 phase 1: Options > Display > Internal resolution and Native res while turbo.
    Phase 1: Down to Internal resolution, Cross 1x -> 2x (`res-2x`), Down to Native res while
    turbo, Cross ON -> OFF (`res-turbo-off`). Phase 2 is a fresh process on the same config
    folder: both come back from the file (`res-restored`, 2x and OFF), then Cross on the
    first row steps 2x -> 3x (`res-3x`) and again 3x -> 1x (`res-1x`)."""
    r = Rec(f"{version.upper()}: P6.11 Internal resolution rows, phase {phase}")
    m = _to_options(r, version)
    f = _group(r, m + 70, GRP_DISPLAY, GRP_DISPLAY)
    f = _keys(r, f + 20, DOWN)                         # Widescreen -> Internal resolution
    if phase == 1:
        r.press(f + 20, CROSS)                         # 1x -> 2x
        r.shot(f + 60, "res-2x")
        f = _keys(r, f + 80, DOWN, CROSS)              # Native res while turbo ON -> OFF
        r.shot(f + 40, "res-turbo-off")
    else:
        r.shot(f + 40, "res-restored")
        r.press(f + 60, CROSS)                         # 2x -> 3x
        r.shot(f + 120, "res-3x")
        r.press(f + 140, CROSS)                        # 3x -> 1x
        r.shot(f + 200, "res-1x")
        f = f + 200
    f = _keys(r, f + 60, TRIANGLE, TRIANGLE)
    r.end(f + 40)
    return r


def main(out):
    out.mkdir(parents=True, exist_ok=True)
    (out / "us-title.rec").write_text(us_title().text())
    for i, lang in enumerate(EU_LANGS):
        (out / f"eu-lang-{lang}.rec").write_text(eu_language(i).text())
    # patch 0187: the confirm button picks the language as well as Start
    (out / "eu-lang-cross-fr.rec").write_text(eu_language(1, CROSS).text())
    for v in ("us", "eu"):
        (out / f"{v}-newgame-field.rec").write_text(newgame_field(v).text(True))
        (out / f"{v}-field-walk.rec").write_text(field_walk(v).text(True))
        (out / f"{v}-soak.rec").write_text(soak(v).text(True))
        (out / f"{v}-banner-walk.rec").write_text(banner_walk(v).text(True))
    for v in ("us", "eu"):
        (out / f"{v}-load-menu.rec").write_text(load_menu(v).text(True))
        (out / f"{v}-load-view.rec").write_text(load_view(v).text(True))
    for v in ("us", "eu"):
        for phase in (1, 2):
            (out / f"{v}-options-persist-p{phase}.rec").write_text(
                options_persist(v, phase).text(True))
    for v in ("us", "eu"):
        for phase in (1, 2):
            (out / f"{v}-controls-layout-p{phase}.rec").write_text(
                controls_layout(v, phase).text(True))
    for v in ("us", "eu"):
        for phase in (1, 2):
            (out / f"{v}-controls-custom-p{phase}.rec").write_text(
                controls_custom(v, phase).text(True))
        (out / f"{v}-controls-custom-invalid.rec").write_text(
            controls_custom_invalid(v).text(True))
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
        (out / f"{v}-map-view.rec").write_text(map_view(v).text(True))
        (out / f"{v}-map-view-off.rec").write_text(map_view_off(v).text(True))
        (out / f"{v}-map-names.rec").write_text(map_names(v).text(True))
        (out / f"{v}-map-zoom.rec").write_text(map_zoom(v).text(True))
        (out / f"{v}-map-help.rec").write_text(map_help(v).text(True))
        (out / f"{v}-map-rebind.rec").write_text(map_rebind(v).text(True))
        (out / f"{v}-field-map.rec").write_text(field_map(v).text(True))
        (out / f"{v}-field-map-ctl.rec").write_text(field_map_ctl(v).text(True))
        (out / f"{v}-field-map-off.rec").write_text(field_map_off(v).text(True))
        (out / f"{v}-field-map-43.rec").write_text(field_map_43(v).text(True))
        (out / f"{v}-field-map-square.rec").write_text(field_map(v, key=SQUARE_MAP, old_key=SELECT_MAP).text(True))
        (out / f"{v}-map-rebind-relaunch-p1.rec").write_text(map_rebind_relaunch_p1(v).text(True))
        (out / f"{v}-map-rebind-relaunch-p2.rec").write_text(map_rebind_relaunch_p2(v).text(True))
        (out / f"{v}-menu-instant-exit.rec").write_text(menu_instant_exit(v).text(True))
        (out / f"{v}-items-equip-switch.rec").write_text(items_equip_switch(v).text(True))
        (out / f"{v}-items-equip-stays.rec").write_text(items_equip_stays(v).text(True))
        (out / f"{v}-items-bag-view.rec").write_text(items_bag_view(v).text(True))
        (out / f"{v}-items-use-stays.rec").write_text(items_use_stays(v).text(True))
        (out / f"{v}-items-select-equip.rec").write_text(items_select_equip(v).text(True))
        (out / f"{v}-items-equip-from-items.rec").write_text(items_equip_from_items(v).text(True))
        (out / f"{v}-items-equip-refused.rec").write_text(items_equip_refused(v).text(True))
        (out / f"{v}-items-equip-replace.rec").write_text(items_equip_replace(v).text(True))
        (out / f"{v}-battle-results.rec").write_text(battle_results(v).text(True))
        (out / f"{v}-save-screen.rec").write_text(save_screen(v).text(True))
        (out / f"{v}-xr-save.rec").write_text(xr_save(v).text(True))
        (out / f"{v}-xr-load.rec").write_text(xr_load(v).text(True))
        if v == "eu":  # only the EU disc re-opens a card the US build refused
            (out / f"{v}-xr-open.rec").write_text(xr_open(v).text(True))
        (out / f"{v}-as-load.rec").write_text(as_load(v).text(True))
        (out / f"{v}-as-walk.rec").write_text(as_walk(v).text(True))
        (out / f"{v}-as-continue.rec").write_text(as_continue(v).text(True))
        (out / f"{v}-as-loadrow.rec").write_text(as_loadrow(v).text(True))
        (out / f"{v}-as-title.rec").write_text(as_title(v).text(True))
        (out / f"{v}-card-album.rec").write_text(card_album(v).text(True))
        (out / f"{v}-deck-editor.rec").write_text(deck_editor(v).text(True))
        (out / f"{v}-card-shop.rec").write_text(card_shop(v).text(True))
        (out / f"{v}-item-shop.rec").write_text(item_shop(v).text(True))
        (out / f"{v}-gym.rec").write_text(gym(v).text(True))
        (out / f"{v}-digimon-lab.rec").write_text(digimon_lab(v).text(True))
        (out / f"{v}-lab-digivolve.rec").write_text(lab_digivolve(v).text(True))
        (out / f"{v}-victory-music.rec").write_text(victory_music(v).text(True))
        (out / f"{v}-music-loop.rec").write_text(music_loop(v).text(True))
        (out / f"{v}-turbo-music-field.rec").write_text(turbo_music_field(v).text(True))
        (out / f"{v}-walkthrough.rec").write_text(walkthrough(v).text(True))
        (out / f"{v}-underground-exit.rec").write_text(underground_exit(v).text(True))
        (out / f"{v}-stage-0x236.rec").write_text(stage_0x236(v).text(True))
        (out / f"{v}-ws-door-iris.rec").write_text(ws_door_iris(v).text(True))
        (out / f"{v}-door-iris43.rec").write_text(door_iris43(v).text(True))
        (out / f"{v}-ws-battle-spiral.rec").write_text(ws_battle_spiral(v).text(True))
        (out / f"{v}-ws-battle-spiral-turbo.rec").write_text(ws_battle_spiral_turbo(v).text(True))
        (out / f"{v}-battle-music.rec").write_text(battle_music(v).text(True))
        (out / f"{v}-two-battles.rec").write_text(two_battles(v).text(True))
        (out / f"{v}-battle-report.rec").write_text(battle_report(v).text(True))
        (out / f"{v}-battle-digivolve-command.rec").write_text(battle_digivolve_command(v).text(True))
        (out / f"{v}-ws-title-intro.rec").write_text(ws_title_intro(v).text(True))
        (out / f"{v}-p77-battle.rec").write_text(p77_battle(v).text(True))
        (out / f"{v}-p77-square.rec").write_text(p77_square(v).text(True))
        (out / f"{v}-file-cache-full.rec").write_text(file_cache_full(v).text(True))
        (out / f"{v}-p79-menu.rec").write_text(p79_menu(v).text(True))
        (out / f"{v}-p79-menu-page2.rec").write_text(p79_menu(v, page2=True).text(True))
        (out / f"{v}-p79-two-battles.rec").write_text(p79_two_battles(v).text(True))
        (out / f"{v}-options-cheats-listed.rec").write_text(options_cheats_listed(v).text(True))
        for phase in (1, 2):
            (out / f"{v}-options-region-p{phase}.rec").write_text(
                options_region(v, phase).text(True))
            (out / f"{v}-options-menus-p{phase}.rec").write_text(
                options_menus(v, phase).text(True))
            (out / f"{v}-options-display-p{phase}.rec").write_text(
                options_display(v, phase).text(True))
        if v == "eu":  # the pack is EU-only until phase 5; both regions run the no-pack row
            (out / f"{v}-l10n-row.rec").write_text(l10n_row(v, True).text(True))
        (out / f"{v}-l10n-row-nopack.rec").write_text(l10n_row(v, False).text(True))
        (out / f"{v}-ptbr-options.rec").write_text(ptbr_options(v).text(True))
        for phase in (1, 2):
            (out / f"{v}-defaults-migrate-p{phase}.rec").write_text(
                defaults_migrate(v, phase).text(True))
        (out / f"{v}-defaults-view.rec").write_text(defaults_view(v).text(True))
        (out / f"{v}-options-global-view.rec").write_text(options_global_view(v).text(True))
        (out / f"{v}-options-saves.rec").write_text(options_saves(v).text(True))
        (out / f"{v}-options-advanced.rec").write_text(options_advanced(v).text(True))
        (out / f"{v}-options-mapbtn.rec").write_text(options_mapbtn(v).text(True))
        for mode in ("no", "yes", "exec", "game"):
            (out / f"{v}-options-region-relaunch-{mode}.rec").write_text(
                region_relaunch(v, mode).text(True))
        (out / f"{v}-battle-list-bar.rec").write_text(battle_list_bar(v).text(True))
        for phase in (1, 2):
            (out / f"{v}-cheats-screen-p{phase}.rec").write_text(
                cheats_screen(v, phase).text(True))
        (out / f"{v}-cheat-fill-bag.rec").write_text(cheat_fill_bag(v).text(True))
        (out / f"{v}-cheats-screen2.rec").write_text(cheats_screen2(v).text(True))
        (out / f"{v}-title-quit.rec").write_text(title_quit(v).text(True))
        (out / f"{v}-title-ready.rec").write_text(title_ready(v).text(True))
        (out / f"{v}-return-title.rec").write_text(return_title(v).text(True))
        (out / f"{v}-return-title-rows7.rec").write_text(
            return_title(v, rows7=True).text(True))
        for phase in (1, 2):
            (out / f"{v}-field-options-p{phase}.rec").write_text(
                field_options(v, phase).text(True))
        (out / f"{v}-turbo-menu.rec").write_text(turbo_menu(v).text(True))
        (out / f"{v}-help-title.rec").write_text(help_title(v).text(True))
        (out / f"{v}-help-field.rec").write_text(help_field(v).text(True))
        (out / f"{v}-help-battle.rec").write_text(help_battle(v).text(True))
        (out / f"{v}-walkthrough-screen-title.rec").write_text(walkthrough_title(v).text(True))
        for _tag, _cur, _ahead in WT_FIELD_POINTS:
            (out / f"{v}-walkthrough-screen-{_tag}.rec").write_text(
                walkthrough_field(v, "gate" if (v == "us" and _tag == "p2d") else _cur, _ahead).text(True))
        for _tag, _downs in WT_SIDE_SHOWN:
            if _tag != "p2d" or v == "eu":       # the extra chapter's rows exist on the EU disc only
                (out / f"{v}-walkthrough-screen-side-{_tag}.rec").write_text(
                    walkthrough_side_shown(v, _tag, _downs).text(True))
        for _tag in WT_SIDE_HIDDEN:
            if _tag != "p2b" or v == "eu":
                (out / f"{v}-walkthrough-screen-side-{_tag}.rec").write_text(
                    walkthrough_side_hidden(v, _tag).text(True))
        (out / f"{v}-field-options-cheats.rec").write_text(
            field_options_cheats(v).text(True))
        (out / f"{v}-field-options-release.rec").write_text(
            field_options_release(v).text(True))
        (out / f"{v}-battle-options-ref.rec").write_text(
            battle_options_ref(v).text(True))
        (out / f"{v}-res-switch.rec").write_text(res_switch(v).text(True))
        if v == "eu":
            (out / f"{v}-res-movie.rec").write_text(res_movie(v).text(True))
        if v == "eu":
            for phase in (1, 2):
                (out / f"{v}-options-res-p{phase}.rec").write_text(
                    options_res(v, phase).text(True))
        (out / f"{v}-battle-options.rec").write_text(battle_options(v).text(True))
    for i, lang in enumerate(EU_LANGS):
        if lang != "en":
            (out / f"eu-title-quit-{lang}.rec").write_text(title_quit_lang(i).text(True))
            (out / f"eu-field-options-{lang}.rec").write_text(field_options_lang(i).text(True))
            (out / f"eu-help-{lang}.rec").write_text(help_lang(i).text(True))
            if lang == "fr":   # one EU language is enough to prove the text is picked up
                (out / f"eu-walkthrough-screen-title-{lang}.rec").write_text(walkthrough_title("eu", i).text(True))
    for v in ("us", "eu"):
        for name, fn in RESTART_RECS:
            (out / f"{v}-{name}.rec").write_text(fn(v).text(True))
    (out / "us-repro-balloon.rec").write_text(repro_balloon("us").text(True))
    for version in ("us", "eu"):
        # Written unsorted, byte for byte as first committed with patch 0058.
        (out / f"{version}-bridge-partner-gate.rec").write_text(
            bridge_partner_gate(version).text())
    # P9.8: made last, from the plain recordings written above (or committed).
    note = "Nintendo layout: the masks are button positions, DW3_REPLAY_SOURCE=1"
    pairs = [(n.format(v=v), b.format(v=v)) for v in ("us", "eu") for n, b in NINTENDO_RECS]
    pairs.append(("eu-nintendo-lang-cross-fr", "eu-lang-cross-fr"))
    for v in ("us", "eu"):   # P6.32 phase 4: a custom map (Square and R2 positions swapped)
        (out / f"{v}-custom-map-rebind.rec").write_text(
            source_rec((out / f"{v}-map-rebind.rec").read_text(),
                       "custom map " + CUSTOM_MAP_DIGITS + ": the masks are button positions, "
                       "DW3_REPLAY_SOURCE=1", CUSTOM_MAP_DIGITS))
    for v in ("us", "eu"):   # P6.32 phase 7: the help line under a custom map (L1 on ZL, R1 on ZR)
        (out / f"{v}-custom-map-help.rec").write_text(
            source_rec((out / f"{v}-map-help.rec").read_text(),
                       "custom map " + HELP_CUSTOM_DIGITS + ": the masks are button positions, "
                       "DW3_REPLAY_SOURCE=1", HELP_CUSTOM_DIGITS))
    for new, base in pairs:
        (out / f"{new}.rec").write_text(
            nintendo_source_rec((out / f"{base}.rec").read_text(), note))
    for version in ("us", "eu"):
        # P5.5: no input; the sweep ends the process by itself (the end line is
        # only a guard).
        (out / f"{version}-stage-sweep.rec").write_text(
            f"# {version.upper()}: stage sweep, no input\nend 20000\n")


if __name__ == "__main__":
    main(Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent / "scenarios")
