#!/usr/bin/env python3
"""P6.32 phase 2 cross-check of the `*-map-names*` replay scenarios (one US, five EU
languages). The runner proves each scenario on its own (every one of the 46 locations
named by the game's own name window, in its language). This script proves what only
the six together can show, from a results directory of the runner:

  * every language names the same 46 locations, from its own text file (US 0x9, EU
    0xa English, 0xb French, 0xc Italian, 0xd German, 0xe Spanish);
  * the names differ between languages (they are not the English strings reused), and
    the EU English names are the US ones;
  * the name box the new screen leaves to the game is pixel-identical to the game's own
    Map screen name box: the interior of the box (both text lines) at the current area,
    `game-map` (the game's screen) against `names-open` (the new screen), 0 differing
    pixels in every language.

  tools/replay/map_names_check.py RESULTS_DIR [RESULTS_DIR ...]

Each RESULTS_DIR is a runner output folder (<out>/<timestamp>) holding scenario
folders; the newest folder with a scenario wins. Needs Pillow for the pixel part."""

from __future__ import annotations

import re
import sys
from pathlib import Path

SCENARIOS = {  # name: (language, text file number)
    "us-map-names": ("en (US text)", "9"),
    "eu-map-names": ("en", "a"),
    "eu-map-names-fr": ("fr", "b"),
    "eu-map-names-it": ("it", "c"),
    "eu-map-names-de": ("de", "d"),
    "eu-map-names-es": ("es", "e"),
}
LINE = re.compile(r"dw3-map: area name shown: location id=(\d+) source=game name window "
                  r"\(text file 0x([0-9a-f]+) language ([^,]+), string (\d+) of the table, "
                  r"(\d+) font codes, hash ([0-9a-f]{8}), window text (is|IS NOT) the table's "
                  r"string\) text=\"([^\"]*)\" \((\d+) undecoded\) width (\d+) of (\d+) px")
# the box interior in the runner's 1280x960 capture: both text lines of the name box at
# the top of the picture (logical x 9..253), inside its frame
INTERIOR = (200, 192, 882, 282)


def find(dirs: list[Path], name: str) -> Path | None:
    hits = []
    for d in dirs:
        p = d / name
        if p.is_dir():
            hits.append(p)
    return hits[-1] if hits else None


def names_of(scen_dir: Path) -> dict[int, tuple]:
    out: dict[int, tuple] = {}
    for log in sorted(scen_dir.glob("*.log")):
        if log.name == "xvfb.log":
            continue
        for ln in log.read_text(errors="replace").splitlines():
            m = LINE.search(ln)
            if m and int(m.group(1)) not in out:
                out[int(m.group(1))] = (m.group(6), m.group(8), int(m.group(10)), m.group(2),
                                        m.group(3), m.group(7))
    return out


def box_diff(scen_dir: Path) -> int | None:
    try:
        from PIL import Image, ImageChops
    except ImportError:
        return None
    a = Image.open(scen_dir / "game-map.png").convert("RGB").crop(INTERIOR)
    b = Image.open(scen_dir / "names-open.png").convert("RGB").crop(INTERIOR)
    return sum(1 for p in ImageChops.difference(a, b).getdata() if p != (0, 0, 0))


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 2
    dirs = [Path(a) for a in argv[1:]]
    bad = 0
    data: dict[str, dict[int, tuple]] = {}
    for name, (lang, tfile) in SCENARIOS.items():
        d = find(dirs, name)
        if d is None:
            print(f"FAIL {name}: not found in {', '.join(map(str, dirs))}")
            bad += 1
            continue
        names = names_of(d)
        data[name] = names
        ok = (sorted(names) == list(range(1, 47)) and
              all(v[3] == tfile and v[4] == lang and v[5] == "is" and v[0] != "" for v in names.values()))
        diff = box_diff(d)
        widest = max(names.items(), key=lambda kv: kv[1][2]) if names else (0, ("", "", 0))
        print(f"{'ok  ' if ok else 'FAIL'} {name:17s} {len(names)} locations, text file 0x{tfile}, "
              f"language {lang}; widest name id={widest[0]} {widest[1][2]} px "
              f"\"{widest[1][1]}\"; name box vs the game's own: "
              f"{'n/a (no Pillow)' if diff is None else f'{diff} differing px'}")
        if not ok:
            bad += 1
        if diff:
            bad += 1
    en = data.get("eu-map-names")
    us = data.get("us-map-names")
    if en and us:
        same = sum(1 for i in range(1, 47) if en[i][0] == us[i][0])
        print(f"{'ok  ' if same == 46 else 'FAIL'} EU English = US text: {same} of 46 names have "
              f"the same hash")
        bad += same != 46
    for name in ("eu-map-names-fr", "eu-map-names-it", "eu-map-names-de", "eu-map-names-es"):
        other = data.get(name)
        if en and other:
            diff = sum(1 for i in range(1, 47) if other[i][0] != en[i][0])
            print(f"{'ok  ' if diff >= 30 else 'FAIL'} {name}: {diff} of 46 names differ from "
                  f"the English ones (the rest are proper names the language keeps)")
            bad += diff < 30
    print("OK" if not bad else f"{bad} problem(s)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
