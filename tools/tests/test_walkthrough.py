"""P6.35 walkthrough phase 1: the step resolver (port/walkthrough/), the step
table against the text files, and the map-area copy against the decomp.
Plain C and text files, no game data."""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"
WT = ROOT / "port" / "walkthrough"
SOURCES = [WT / "walkthrough_core.c", WT / "walkthrough_steps.c", WT / "walkthrough_areas.c"]
LANGS = ["en", "fr", "de", "it", "es"]
BUDGET = {"title": 40, "goal": 240, "help": 480}


@pytest.fixture(scope="module")
def wt_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("walkthrough") / "walkthrough_test"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2",
           f"-I{ROOT / 'port' / 'include'}", "-o", str(out),
           str(TESTS / "walkthrough_main.c"), *map(str, SOURCES)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def parse_text(path: Path) -> tuple[dict[str, str], dict[str, str]]:
    headers: dict[str, str] = {}
    keys: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        k, _, v = line.partition("=")
        k, v = k.strip(), v.strip()
        if k.startswith("@"):
            headers[k[1:]] = v
        else:
            assert k not in keys, f"{path.name}: duplicate key {k}"
            keys[k] = v
    return headers, keys


def test_resolver(wt_bin: Path):
    r = subprocess.run([str(wt_bin)], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK: walkthrough resolver" in r.stdout


@pytest.mark.parametrize("src", SOURCES)
def test_compiles_as_c90(src: Path, tmp_path: Path):
    """The port library is built as gnu89: the pure files must be valid there."""
    cmd = ["gcc", "-std=gnu89", "-Wall", "-Wextra", "-Werror", "-Wno-long-long",
           f"-I{ROOT / 'port' / 'include'}", "-c", "-o", str(tmp_path / "o.o"), str(src)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout


def test_text_keys_cover_the_table(wt_bin: Path):
    out = subprocess.run([str(wt_bin), "list-ids"], capture_output=True, text=True).stdout
    rows = [l.split() for l in out.splitlines() if l.startswith("ID ")]
    assert rows
    headers, keys = parse_text(WT / "text" / "en.txt")
    assert headers["lang"] == "en" and headers["format"] == "1"
    areas, chapters = set(), set()
    for _, sid, chapter, area, _kind, _region, *_rest in rows:
        chapters.add(chapter)
        areas.add(area)
        for layer in ("title", "goal", "help"):
            key = f"wt.{sid.replace('.', '_')}.{layer}"
            assert key in keys and keys[key], f"missing en text {key}"
            assert len(keys[key]) <= BUDGET[layer], f"{key} over budget ({len(keys[key])})"
    for c in chapters:
        assert f"wt.{c}.title" in keys
    for a in areas:
        assert f"wt.area_{a}.title" in keys and f"wt.area_{a}.help" in keys
        assert len(keys[f"wt.area_{a}.help"]) <= BUDGET["help"]
    # no key without a table row
    ids = {r[1] for r in rows}
    for k in keys:
        m = re.match(r"wt\.(ch\d+_[sx]\d+)\.(title|goal|help)$", k)
        if m:
            assert m.group(1).replace("_", ".") in ids, f"text key {k} has no step"


def test_all_launch_languages_exist():
    """The owner rule: en/fr/de/it/es files exist. Stubs are allowed in
    the tree but must say so; a release check refuses them."""
    for lang in LANGS:
        headers, keys = parse_text(WT / "text" / f"{lang}.txt")
        assert headers["lang"] == lang
        status = headers["status"]
        assert "draft" in status or "stub" in status
        if lang != "en":
            if "stub" in status:
                assert not keys, f"{lang}: a stub must not carry keys"
            else:
                _, en = parse_text(WT / "text" / "en.txt")
                assert set(keys) <= set(en), f"{lang}: keys not in en"


def test_translations_are_complete_and_fit():
    """fr/de/it/es carry every key of en.txt (a drafted language is complete,
    no English fallback), stay inside the budgets, keep the TODO(verify)
    markers, use only characters of the help glyph atlas (Latin-1 and the few
    symbols of generate_help_assets.CHARSET), and say they need native review."""
    _, en = parse_text(WT / "text" / "en.txt")
    charset = set(range(0x20, 0x7F)) | set(range(0xA0, 0x100)) | {
        0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026}
    for lang in LANGS[1:]:
        headers, keys = parse_text(WT / "text" / f"{lang}.txt")
        assert "machine-drafted" in headers["status"], lang
        assert "native review" in headers["status"], lang
        assert list(keys) == list(en), f"{lang}: keys differ from en.txt"
        for k, v in keys.items():
            layer = k.rsplit(".", 1)[1]
            assert v, f"{lang}: {k} is empty"
            if re.match(r"wt\.ch\d+_[sx]\d+\.", k):
                limit = BUDGET[layer]
            elif k.startswith("wt.area_") and layer == "help":
                limit = BUDGET["help"]
            elif k.startswith("wt.ui."):
                limit = BUDGET["goal"]
            else:
                limit = BUDGET["title"]
            assert len(v) <= limit, f"{lang}: {k} over budget ({len(v)} > {limit})"
            assert v.count("TODO(verify)") == en[k].count("TODO(verify)"), f"{lang}: {k} TODO marker"
            bad = {c for c in v if ord(c) not in charset}
            assert not bad, f"{lang}: {k} has glyphs outside the atlas: {bad}"


def test_no_unverified_marker_in_release():
    """A release build refuses TODO(verify). Every row of the table ships, so no text line of any
    language file may carry the marker (a comment line may mention it)."""
    for lang in LANGS:
        _, keys = parse_text(WT / "text" / f"{lang}.txt")
        bad = [k for k, v in keys.items() if "TODO(verify)" in v]
        assert not bad, f"{lang}.txt: a TODO(verify) marker is left in a shipped row: {bad}"


def test_map_area_copy_matches_decomp():
    helpers = ROOT / "vendor" / "dw3_decomp" / "src" / "menus" / "field_menu" / "helpers.c"
    if not helpers.exists():
        pytest.skip("decomp submodule not present")
    src = helpers.read_text()
    m = re.search(r"u8 STSTATUS_mapAreas\[\] = \{(.*?)\};", src, re.S)
    assert m
    decomp = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", m.group(1))]
    mine = re.search(r"k_map_areas\[\d+\] = \{(.*?)\};",
                     (WT / "walkthrough_areas.c").read_text(), re.S)
    assert mine
    copy = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", mine.group(1))]
    assert copy == decomp


# The stage (WSTAGnnn) that each MAIN row of chapters 2 to 10 is about, and the stage that sets
# each `done` flag or item: the evidence of docs/P6.35_WALKTHROUGH.md "As built, slice 2". A step at
# progress N is what to do to reach N+1, so the last row of each progress value is the stage that
# writes N+1. (Chapter 1's rows were checked in phase 1 by the replay logs, and name the place
# where the progress became N, not where it ends.)
STEP_STAGE = {
    "ch02.s01": 410, "ch02.s02": 440, "ch02.s03": 480, "ch02.s04": 475, "ch02.s05": 375,
    "ch02.s06": 380, "ch02.s07": 202, "ch02.s08": 320, "ch02.s09": 475, "ch02.s10": 520,
    "ch03.s01": 212, "ch03.s02": 237, "ch03.s03": 305, "ch03.s04": 212, "ch03.s05": 460,
    "ch04.s01": 685, "ch04.s02": 655, "ch04.s03": 605,
    "ch05.s01": 615, "ch05.s02": 620, "ch05.s03": 620, "ch05.s04": 620, "ch05.s05": 325,
    "ch05.s06": 305, "ch05.s07": 310, "ch05.s08": 210,
    "ch06.s01": 685, "ch06.s02": 520, "ch06.s03": 545,
    "ch07.s01": 421, "ch07.s02": 521, "ch07.s03": 526,
    "ch08.s01": 755, "ch08.s02": 750, "ch08.s03": 750, "ch08.s04": 750, "ch08.s05": 305,
    "ch08.s06": 295, "ch08.s07": 305,
    "ch09.s01": 676, "ch09.s02": 756, "ch09.s03": 206,
    "ch10.s01": 311, "ch10.s02": 226, "ch10.s03": 310, "ch10.s04": 311, "ch10.s05": 810,
    "ch10.s06": 820, "ch10.s07": 820,
}
# the stage whose script sets the code a row waits for (its `done`), when it is not the row's own
DONE_SETTER = {
    "ch01.s04": 200, "ch02.s04": 475, "ch02.s05": 202, "ch02.s07": 202, "ch02.s08": 320,
    "ch03.s03": 305, "ch05.s03": 525, "ch05.s07": 310, "ch08.s03": 780, "ch08.s05": 305,
    "ch08.s06": 295, "ch10.s01": 311, "ch10.s06": 820,
}
# GAME.progress assignments of src/field/stages (value -> stage numbers), FIELD_PROGRESS_* by name
WRITERS = {
    0: [780], 1: [785], 2: [225], 3: [212], 4: [200], 5: [420], 6: [410], 7: [440], 8: [480],
    9: [380], 0xA: [475], 0xB: [520], 0xC: [212], 0xD: [237], 0xE: [212], 0xF: [460],
    0x10: [685], 0x11: [655], 0x12: [605], 0x13: [615], 0x14: [620], 0x15: [620], 0x16: [325],
    0x17: [305], 0x18: [210], 0x19: [685], 0x1A: [520], 0x1B: [545], 0x1C: [421], 0x1D: [521],
    0x1E: [526], 0x1F: [755], 0x20: [750], 0x21: [750], 0x22: [305], 0x23: [676], 0x24: [756],
    0x25: [206], 0x26: [226], 0x27: [310], 0x28: [311], 0x29: [810],
    "MOVIE_BATTLES": [820], "EXTRA": [780],
}


def stage_files() -> dict[int, Path]:
    stages = ROOT / "vendor" / "dw3_decomp" / "src" / "field" / "stages"
    return {int(p.stem[5:]): p for p in stages.glob("*/wstag*.c") if p.stem[5:].isdigit()}


def table_rows(wt_bin: Path) -> list[dict]:
    out = subprocess.run([str(wt_bin), "list-ids"], capture_output=True, text=True).stdout
    rows = []
    for l in out.splitlines():
        if not l.startswith("ID "):
            continue
        f = l.split()
        rows.append(dict(id=f[1], chapter=f[2], area=f[3], kind=int(f[4]), region=int(f[5]),
                         map_area=int(f[6]), frm=int(f[7]), to=int(f[8]), order=int(f[9]),
                         ndone=int(f[10]), done=int(f[11]), nunlock=int(f[12]), unlock=int(f[13])))
    return rows


def need_decomp():
    if not (ROOT / "vendor" / "dw3_decomp" / "src" / "field" / "stages").exists():
        pytest.skip("decomp submodule not present")


def test_progress_values_match_decomp_scripts():
    """The step table's progress values are the ones the stage scripts assign: the set of
    `GAME.progress = N;` writers in src/field/stages is exactly WRITERS (so no value is
    missed or assigned somewhere the table does not know)."""
    need_decomp()
    files = stage_files()
    found: dict = {}
    for num, path in files.items():
        for m in re.finditer(r"GAME\.progress = (0x[0-9A-Fa-f]+|\d+|FIELD_PROGRESS_\w+);", path.read_text()):
            tok = m.group(1)
            key = {"FIELD_PROGRESS_MOVIE_BATTLES": "MOVIE_BATTLES",
                   "FIELD_PROGRESS_EXTRA": "EXTRA"}.get(tok) or int(tok, 0)
            found.setdefault(key, set()).add(num)
    assert {k: sorted(v) for k, v in found.items()} == {k: sorted(v) for k, v in WRITERS.items()}
    # the engine's own constants the table relies on
    fm = (ROOT / "vendor" / "dw3_decomp" / "include" / "field" / "field_map.h").read_text()
    assert re.search(r"FIELD_PROGRESS_MOVIE_BATTLES 0x2B\b", fm)
    assert re.search(r"FIELD_PROGRESS_EXTRA 0x2D\b", fm)


def test_every_step_is_the_writer_of_the_next_value(wt_bin: Path):
    """A step at progress N is what to do to reach N+1: the last row of each value is the stage
    that assigns N+1, for the whole first half (0 to 0x29, the last one into 0x2B)."""
    need_decomp()
    rows = [r for r in table_rows(wt_bin) if r["kind"] == 0]
    assert [r["id"] for r in rows] == sorted((r["id"] for r in rows), key=lambda i: (i[:4], i[5:]))
    by_progress: dict[int, list[dict]] = {}
    for r in rows:
        assert r["frm"] == r["to"], r["id"]            # one value per row
        by_progress.setdefault(r["frm"], []).append(r)
    assert sorted(by_progress) == list(range(0x2A))     # no gap from 0 to 0x29
    for p, group in by_progress.items():
        if p < 5:
            continue
        nxt = "MOVIE_BATTLES" if p == 0x29 else p + 1
        assert STEP_STAGE[group[-1]["id"]] in WRITERS[nxt], \
            f"progress {p:#x}: last row {group[-1]['id']} is stage {STEP_STAGE[group[-1]['id']]}"
    assert set(STEP_STAGE) == {r["id"] for r in rows if r["chapter"] != "ch01"}


def test_row_conditions_match_the_decomp(wt_bin: Path):
    """Every `done` code is a flag or an item the game sets (in DONE_SETTER's stage), another row of
    the same progress value gates on it, and the code decodes with the game's own macros."""
    need_decomp()
    files = stage_files()
    rows = [r for r in table_rows(wt_bin) if r["kind"] == 0]

    def num(v: int) -> str:
        return r"(?:0[xX]0*%X|%d)" % (v, v)

    def pattern(code: int) -> str:
        if code & 0xF000 == 0x8000:
            return r"ITEM\(\s*%s,\s*%s\)" % (num((code >> 9) & 7), num(code & 0x1FF))
        return r"FLAG\(\s*%s,\s*%s\)" % (num(code >> 8), num(code & 0xFF))

    seen = []
    for i, r in enumerate(rows):
        if not r["ndone"] or r["chapter"] == "ch01":
            continue
        code, what = r["done"], pattern(r["done"])
        setter = files[DONE_SETTER[r["id"]]].read_text()
        flag = what
        # set by an action: applyAction(FLAG(g, i), 1) or an `Actions[]` list { FLAG(g, i), 1 ... }
        assert re.search(flag + r",\s*1\b", setter), f"{r['id']}: {what} is not set in wstag{DONE_SETTER[r['id']]}"
        later = [x for x in rows[i + 1:] if x["frm"] == r["frm"]]
        assert later, f"{r['id']}: nothing follows it at progress {r['frm']:#x}"
        assert any(re.search(flag, files[STEP_STAGE[x["id"]]].read_text()) for x in later), \
            f"{r['id']}: no later row of progress {r['frm']:#x} is in a stage that tests {what}"
        seen.append(r["id"])
    assert sorted(seen) == sorted(k for k in DONE_SETTER if k != "ch01.s04")


def test_map_areas_match_the_stage_modes(wt_bin: Path):
    """A row's map area is STSTATUS_getArea() of its stage: the stage's mode (FIELDSTG_stages, the
    same on US and EU) through the area table."""
    need_decomp()
    src = (ROOT / "vendor" / "dw3_decomp" / "src" / "field" / "field_mode" / "data" / "fieldstg.c").read_text()
    tabs = [m.start() for m in re.finditer(r"StageEntry FIELDSTG_stages\[\] = \{", src)]
    assert len(tabs) == 2
    modes = []
    for t in tabs:
        body = src[t:src.index("};", t)]
        modes.append({int(m.group(3)): int(m.group(1))
                      for m in re.finditer(r"\{(\d+), (\d+), WSTAG(\d+)_startStage\}", body)})
    assert modes[0] == modes[1], "US and EU stage modes differ: the walkthrough rows would need a region"
    mine = re.search(r"k_map_areas\[\d+\] = \{(.*?)\};", (WT / "walkthrough_areas.c").read_text(), re.S)
    areas = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", mine.group(1))]
    for r in table_rows(wt_bin):
        if r["kind"] != 0 or r["chapter"] == "ch01":
            continue
        mode = modes[0][STEP_STAGE[r["id"]]]
        v = areas[mode & 0xFF]
        want = -1 if v == 0xFF else v & 0x7F
        assert r["map_area"] == want, f"{r['id']}: map area {r['map_area']:#x}, stage {STEP_STAGE[r['id']]} says {want:#x}"


def test_chapters_cover_the_first_half(wt_bin: Path):
    """The chapter list: ten chapters in order, one contiguous progress range each, no gap or
    overlap, 0 to 0x29; a title, and an area text for every area used."""
    rows = [r for r in table_rows(wt_bin) if r["kind"] == 0]
    chapters: dict[str, list[int]] = {}
    for r in rows:
        chapters.setdefault(r["chapter"], []).append(r["frm"])
    assert list(chapters) == [f"ch{n:02d}" for n in range(1, 11)]
    prev_last = -1
    for ch, ps in chapters.items():
        assert ps == sorted(ps), ch
        assert ps[0] in (prev_last, prev_last + 1) and ps[0] == prev_last + 1, f"{ch} starts at {ps[0]:#x}"
        assert sorted(set(ps)) == list(range(ps[0], ps[-1] + 1)), f"{ch} has a gap"
        prev_last = ps[-1]
    assert prev_last == 0x29
    # the game's own town-list buckets (map_screen.c: < 7, < 0xF, < 0x1B, < 0x1E) are chapter
    # starts, except 7 (inside chapter 2: the South Station step reveals the south, not a new act)
    starts = {ps[0] for ps in chapters.values()}
    assert {0xF, 0x1B, 0x1E} <= starts and 7 not in starts
    _, keys = parse_text(WT / "text" / "en.txt")
    for ch in chapters:
        assert f"wt.{ch}.title" in keys and len(keys[f"wt.{ch}.title"]) <= 24


def test_game_state_macros_decode():
    """The port's copies of the game's condition macros equal include/engine/game_state.h."""
    need_decomp()
    gs = (ROOT / "vendor" / "dw3_decomp" / "include" / "engine" / "game_state.h").read_text()
    assert "#define FLAG(group, id) ((group) << 8 | (id))" in gs
    assert "#define PROGRESS(n) (0x6000 | (n))" in gs
    assert "#define ITEM(kind, id) (0x8000 | (kind) << 9 | (id))" in gs
    h = (ROOT / "port" / "include" / "port_walkthrough.h").read_text()
    assert "#define WT_FLAG(group, id) (((group) << 8) | (id))" in h
    assert "#define WT_ITEM(kind, id) (0x8000 | ((kind) << 9) | (id))" in h


# --------------------------------------------------------------- phase 2: the screen

PAGES = ROOT / "port" / "pages"
HELP = ROOT / "port" / "help"


@pytest.fixture(scope="module")
def screen_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("walkthrough_screen") / "walkthrough_screen_test"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O1", "-g",
           "-fsanitize=address,undefined", "-DDW3_PORT_OPTIONS_TEST", "-DDW3_BUTTON_MAP_TEST",
           f"-I{ROOT / 'port' / 'include'}", f"-I{WT}", f"-I{PAGES}", f"-I{HELP}",
           "-o", str(out), str(TESTS / "walkthrough_screen_main.c"),
           str(WT / "walkthrough_screen.c"), *map(str, SOURCES), str(HELP / "help.c"),
           str(PAGES / "pages.c"), str(PAGES / "glyph_atlas.c"),
           str(ROOT / "port" / "options" / "button_map.c")]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_screen_flow(screen_bin: Path):
    r = subprocess.run([str(screen_bin)], capture_output=True, text=True, timeout=120)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK: walkthrough screen" in r.stdout
    log = r.stderr
    # AGENTS.md rule 12: the log lines that answer the OLED questions
    assert "dw3-walkthrough: open (from field, region EU, platform pc, language en, 10 chapters, 55 steps)" in log
    assert "dw3-walkthrough: open (from title, region EU, platform pc, language en, 10 chapters, 55 steps)" in log
    assert re.search(r"dw3-walkthrough: progress 2 half 0 area 19 -> chapter ch01 step ch01\.s03 "
                     r"\(rule progress\) \[open mode 0x200\]", log)
    assert "dw3-walkthrough: flags read 0x400d=1" in log
    assert re.search(r"dw3-walkthrough: page ch01\.s03 \(kind MAIN, from open, reached y, current y, "
                     r"\d+ lines, \d+ us\)", log)
    assert "dw3-walkthrough: spoiler ch01.s03 closed (answer available)" in log
    assert "dw3-walkthrough: spoiler ch01.s03 asked" in log
    assert "dw3-walkthrough: spoiler ch01.s03 cancelled" in log
    assert "dw3-walkthrough: spoiler ch01.s03 shown" in log
    assert "dw3-walkthrough: page ch01.s03 closed after" in log and "(spoiler shown y)" in log
    assert "(spoiler shown n)" in log
    assert re.search(r"dw3-walkthrough: page ch01\.s04 \(kind MAIN, from list, reached n, current n", log)
    assert "dw3-walkthrough: step not recognised (progress 32768)" in log
    assert "dw3-walkthrough: browse mode (no game state)" in log
    assert re.search(r"dw3-walkthrough: closed after \d+ ms \(pages opened \d+, spoilers shown 1\)", log)
    assert "dw3-walkthrough: text fallback wt.ch01_s03.goal (de)" in log
    # nothing overflows a plaque and no token was left open
    assert "title too wide" not in log, "a title is wider than its plaque: shorten the text"
    assert "unresolved" not in log


def test_embedded_text_is_current():
    r = subprocess.run(["python3", "-I", str(ROOT / "tools" / "generate_walkthrough_text.py"), "--check"],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr


def test_ui_strings_in_every_language():
    """The screen's own words exist in all five files (a missing one falls back to English)."""
    _, en = parse_text(WT / "text" / "en.txt")
    ui = {k for k in en if k.startswith("wt.ui.str.")}
    assert len(ui) >= 25
    for lang in LANGS[1:]:
        _, keys = parse_text(WT / "text" / f"{lang}.txt")
        assert ui <= set(keys), f"{lang}: missing {sorted(ui - set(keys))}"
