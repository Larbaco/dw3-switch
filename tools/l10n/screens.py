"""Which box shows a dialogue string: a talk box or the bottom message box.

The two boxes have very different widths, and the engine never wraps text, so a
translation is only "inside the box" if it is measured against the box that
really shows it. This module reads the decomp's stage sources (code, not game
text) and says, per text key, which box a string reaches:

* ``TALKnn:<entry>``: the entries of every ``FieldTalk`` table of a stage whose
  ``STAGE_TEXT`` is ``TEXT_TALKS_nn``. ``FIELDSTG_createTalk`` always opens a
  **talk box** (speech.c).
* ``DMGnnn:<table>:<entry>``: the ``SCRIPT_TALK(box, entry, id, kind)`` commands
  of a stage's event scripts, with the table from the event's ``EVENT_TEXT(n)``
  and the file from ``EVENT_TEXT_FILE``. ``kind`` 4 is the **message box**
  (``FIELDSTG_createSpeech(..., 4, 1)``), 0..3 a **talk box** at that corner.

Everything else (names, descriptions, menus, battle) goes through windows that
are sized per call site; those stay on the per-index limit of the other EU
languages.

Box widths (decomp, ``createTalkBox`` in talk_box.c and ``createMessageBox``):

* talk box: the text window is as wide as the widest line of the string
  (``measureText`` of the text after the speaker's name), **clamped to 95..139
  px**, in a frame 32 px wider; text wider than 139 px leaves the frame.
* message box: a text window at x=18, 3 lines, in a panel that spans the screen.
"""

from __future__ import annotations

import re
from pathlib import Path

TALK_BOX = "talk"
MESSAGE_BOX = "message"

#: createTalkBox: w < 0x5F -> 0x5F, w >= 0x8C -> 0x8B
TALK_BOX_MAX_PX = 0x8B
TALK_BOX_MIN_PX = 0x5F

_NUM = r"(?:0x[0-9A-Fa-f]+|\d+)"
_SCRIPT = re.compile(r"\bs16\s+(\w+)\[\w*\]\s*=\s*\{(.*?)^\};", re.S | re.M)
_TALK_CMD = re.compile(r"SCRIPT_TALK\(\s*(%s)\s*,\s*(%s)\s*,\s*([^,]+?)\s*,\s*(%s)\s*\)" % (_NUM, _NUM, _NUM))
_EVENT = re.compile(r"\{\s*[^,{}]+,\s*(?:WSTAG\d+_)?(\w+)\s*,\s*(?:EVENT|STORY)_TEXT\(\s*(%s)\s*\)" % _NUM)
_FIELD_TALK = re.compile(r"\bFieldTalk\s+\w+\[\]\s*=\s*\{(.*?)^\};", re.S | re.M)
_TALK_ENTRY = re.compile(r"\{\s*(?:NULL|\w+)\s*,\s*(?:NULL|\w+)\s*,\s*(%s)\s*\}" % _NUM)


def classify(decomp: Path) -> dict[str, set[str]]:
    """{text key: {box kinds}} for every dialogue string the stage sources reach."""
    out: dict[str, set[str]] = {}
    stages = Path(decomp) / "src" / "field" / "stages"
    for path in sorted(stages.rglob("wstag*.c")):
        src = path.read_text(errors="replace")
        talk_groups = set(re.findall(r"STAGE_TEXT\s+TEXT_FILE\(TEXT_TALKS_(\d\d)\)", src))
        event_groups = set(re.findall(r"EVENT_TEXT_FILE\s+\(TEXT_EVENTS_(\d+)", src))
        if re.search(r"EVENT_TEXT_FILE\s+EXTRA_TEXT", src):
            event_groups.add("900")  # the extra chapter's (stage.h: EXTRA_TEXT)
        if path.name == "wstag260.c":
            event_groups.add("260")  # its events are FIELDSTG_events in field_mode/data/fieldstg.c
        for group in talk_groups:
            for blk in _FIELD_TALK.finditer(src):
                for m in _TALK_ENTRY.finditer(blk.group(1)):
                    entry = int(m.group(1), 0)
                    if entry:  # the { NULL, NULL, 0 } terminator
                        out.setdefault(f"TALK{group}:{entry}", set()).add(TALK_BOX)
        tables: dict[str, set[int]] = {}
        events_src = src
        if path.name == "wstag260.c":
            events_src = (Path(decomp) / "src/field/field_mode/data/fieldstg.c").read_text(errors="replace")
        for m in _EVENT.finditer(events_src):
            tables.setdefault(m.group(1), set()).add(int(m.group(2), 0))
        for name, body in _SCRIPT.findall(src):
            for m in _TALK_CMD.finditer(body):
                entry, kind = int(m.group(2), 0), int(m.group(4), 0)
                box = MESSAGE_BOX if kind == 4 else TALK_BOX
                for group in event_groups:
                    for table in tables.get(name, ()):
                        out.setdefault(f"DMG{group}:{table}:{entry}", set()).add(box)
    return out
