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
        page = _rest[-1]
        # a condensed page is named and titled by its first step: the other steps carry no title
        for layer in ("title", "goal", "help"):
            key = f"wt.{sid.replace('.', '_')}.{layer}"
            if layer == "title" and page != sid:
                assert key not in keys, f"{key}: step {sid} is on page {page}, it has no title of its own"
                continue
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
    # slice 3: the last battle starts from the Control Room; the EU extra chapter's rows are the
    # extra stages (WSTAG92x to 95x, the ones FIELDSTG_extraStages starts in the extra chapter)
    "ch11.s01": 820, "ch12.s01": 925, "ch12.s02": 956, "ch12.s03": 927,
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
                         ndone=int(f[10]), done=int(f[11]), nunlock=int(f[12]), unlock=int(f[13]),
                         page=f[14]))
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
    extra = [r for r in rows if r["frm"] > 0x29]
    assert [r["id"] for r in extra] == ["ch11.s01", "ch12.s01", "ch12.s02", "ch12.s03"]
    rows = [r for r in rows if r["frm"] <= 0x29]
    by_progress: dict[int, list[dict]] = {}
    for r in rows:
        assert r["frm"] == r["to"], r["id"]            # one value per row
        by_progress.setdefault(r["frm"], []).append(r)
    assert sorted(by_progress) == list(range(0x2A))     # no gap from 0 to 0x29 (0x2A is not written)
    for p, group in by_progress.items():
        if p < 5 or p > 0x29:
            continue
        nxt = "MOVIE_BATTLES" if p == 0x29 else p + 1
        assert STEP_STAGE[group[-1]["id"]] in WRITERS[nxt], \
            f"progress {p:#x}: last row {group[-1]['id']} is stage {STEP_STAGE[group[-1]['id']]}"
    assert set(STEP_STAGE) - {r["id"] for r in extra} == {r["id"] for r in rows if r["chapter"] != "ch01"}


def test_row_conditions_match_the_decomp(wt_bin: Path):
    """Every `done` code is a flag or an item the game sets (in DONE_SETTER's stage), another row of
    the same progress value gates on it, and the code decodes with the game's own macros."""
    need_decomp()
    files = stage_files()
    rows = [r for r in table_rows(wt_bin) if r["kind"] == 0 and r["frm"] <= 0x29]

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
    ex = src.index("StageEntry FIELDSTG_extraStages[] = {")
    extra_modes = {int(m.group(3)): int(m.group(1)) for m in re.finditer(
        r"\{(\d+), (\d+), WSTAG(\d+)_startStage\}", src[ex:src.index("};", ex)])}
    for r in table_rows(wt_bin):
        if r["kind"] != 0 or r["chapter"] == "ch01":
            continue
        # the EU extra chapter's rows are started by FIELDSTG_extraStages (the extra stages)
        mode = (extra_modes if r["region"] == 2 else modes[0])[STEP_STAGE[r["id"]]]
        v = areas[mode & 0xFF]
        want = -1 if v == 0xFF else v & 0x7F
        assert r["map_area"] == want, f"{r['id']}: map area {r['map_area']:#x}, stage {STEP_STAGE[r['id']]} says {want:#x}"


def test_chapters_cover_the_story(wt_bin: Path):
    """The chapter list: twelve chapters in order. ch01 to ch10 are one contiguous progress range
    each, 0 to 0x29; ch11 is 0x2B (0x2A is written by no script) and ch12 is the EU extra chapter at
    0x2D (region EU, every other row is for both discs). A title for every chapter and an area text
    for every area used."""
    rows = [r for r in table_rows(wt_bin) if r["kind"] == 0]
    chapters: dict[str, list[int]] = {}
    for r in rows:
        chapters.setdefault(r["chapter"], []).append(r["frm"])
        assert r["region"] == (2 if r["chapter"] == "ch12" else 0), r["id"]
    assert list(chapters) == [f"ch{n:02d}" for n in range(1, 13)]
    prev_last = -1
    for ch, ps in list(chapters.items())[:10]:
        assert ps == sorted(ps), ch
        assert ps[0] == prev_last + 1, f"{ch} starts at {ps[0]:#x}"
        assert sorted(set(ps)) == list(range(ps[0], ps[-1] + 1)), f"{ch} has a gap"
        prev_last = ps[-1]
    assert prev_last == 0x29
    assert set(chapters["ch11"]) == {0x2B} and set(chapters["ch12"]) == {0x2D}
    assert len(chapters["ch11"]) == 1 and len(chapters["ch12"]) == 3
    # the game's own town-list buckets (map_screen.c: < 7, < 0xF, < 0x1B, < 0x1E) are chapter
    # starts, except 7 (inside chapter 2: the South Station step reveals the south, not a new act)
    starts = {ps[0] for ps in chapters.values()}
    assert {0xF, 0x1B, 0x1E} <= starts and 7 not in starts
    _, keys = parse_text(WT / "text" / "en.txt")
    for ch in chapters:
        assert f"wt.{ch}.title" in keys and len(keys[f"wt.{ch}.title"]) <= 24
    # nothing of the old "more chapters" placeholder is left in any language
    for lang in LANGS:
        _, k = parse_text(WT / "text" / f"{lang}.txt")
        assert "wt.ui.str.more" not in k and "wt.ui.str.more_body" not in k, lang


def stage_text(num: int) -> str:
    return stage_files()[num].read_text()


def test_last_battle_matches_decomp():
    """ch11: endEvent1080 of the Control Room (820) writes 0x2B and starts event battle 1, which is
    battle 0x144 (324) with three enemies; winning its last enemy plays the ending; the ending's
    movies lead to MODE_NEW_GAME (Kusanagi City, 780), whose updateStage starts event 1500 at 0x2B
    (on EU it ends by writing FIELD_PROGRESS_EXTRA)."""
    need_decomp()
    d = ROOT / "vendor" / "dw3_decomp"
    s820 = stage_text(820)
    m = re.search(r"void endEvent1080\(void\) \{(.*?)\n\}", s820, re.S)
    assert m and "GAME.progress = FIELD_PROGRESS_MOVIE_BATTLES;" in m.group(1)
    assert "FLAGS_00.applyAction(EVENT_BATTLE(1), 1);" in m.group(1)
    assert "AreaBattle area3Battle1 = { 324," in s820
    stg = (d / "src" / "field" / "field_mode" / "data" / "fieldstg.c").read_text()
    assert stg.count("{{&FIELDSTG_enemies[324], &FIELDSTG_enemies[325], &FIELDSTG_enemies[326]}") == 2
    bh = (d / "include" / "battle" / "battle_mode" / "battle.h").read_text()
    assert re.search(r"BATTLE_KIND_FINAL 4 /\* battle 0x144, the last one", bh)
    assert "winning plays the ending" in bh
    menu = (d / "src" / "battle" / "normal_battle" / "menu.c").read_text()
    assert re.search(r"mode = MODE_ENDING;\s+if \(FIGHTSTG_battle\.state\.kind != BATTLE_KIND_FINAL_LAST\)", menu)
    gs = (d / "include" / "engine" / "game_state.h").read_text()
    assert "#define MODE_NEW_GAME 0x2D7" in gs
    movies = (d / "src" / "menus" / "title" / "data" / "stdwtitl.c").read_text()
    assert movies.count("MODE_ENDING + 1}") == 2 and "MODE_NEW_GAME}" in movies
    s780 = stage_text(780)
    assert "} else if (GAME.progress == FIELD_PROGRESS_MOVIE_BATTLES) {\n            children[0] = FIELDSTG_startEvent(0x5DC);" in s780
    assert "{ 1500, script1500, EVENT_TEXT(0x24), NULL, NULL }," in s780            # US: nothing follows
    assert "{ 1500, script1500, EVENT_TEXT(0x24), NULL, endEvent1500 }," in s780    # EU: the extra chapter
    assert "void endEvent1500(void) {\n    GAME.progress = FIELD_PROGRESS_EXTRA;" in s780
    # no script writes 0x2A or 0x2C
    assert 0x2A not in WRITERS and 0x2C not in WRITERS


def test_extra_chapter_matches_decomp():
    """ch12 (EU): the Chamber Room's movie lands in the extra Login Room at 0x2D; its tour event
    (flag 0x40CD); the four challengers (flags 0x100D to 0x1010, which SPECIAL(0x54) tests) and the
    Arena (event 1608, flag 0x100C)."""
    need_decomp()
    d = ROOT / "vendor" / "dw3_decomp"
    stg = (d / "src" / "field" / "field_mode" / "data" / "fieldstg.c").read_text()
    ex = stg.index("StageEntry FIELDSTG_extraStages[] = {")
    entries = re.findall(r"\{(\d+), (\d+), WSTAG(\d+)_startStage\}", stg[ex:stg.index("};", ex)])
    assert [int(e[2]) for e in entries] == list(range(920, 975))      # the 55 extra stages
    mode_of = {int(e[2]): int(e[0]) for e in entries}
    assert mode_of[925] == 0x276 and mode_of[927] == 0x278
    assert mode_of[956] == 0x29C and mode_of[949] == 0x295 and mode_of[954] == 0x29A
    movie = (d / "src" / "menus" / "title" / "movie.c").read_text()
    assert "if (task->movie == 2 && GAME.progress == 0x2D) {\n                task->nextMode = 0x276;" in movie
    assert "SCRIPT_LEAVE(0xE03" in stage_text(790)                    # the Chamber Room: movie index 2
    s925 = stage_text(925)
    assert re.search(r"if \(FLAGS_00\.checkCondition\(FLAG\(0x40, 0xCD\), 0\)\) \{\s+"
                     r"children\[0\] = FIELDSTG_startEvent\(0x640\);", s925)   # 0x640 = 1600
    assert re.search(r"void endEvent1600\(void\) \{\s+FLAGS_00\.applyAction\(FLAG\(0x40, 0xCD\), 1\);", s925)
    assert "{ 1600, script1600, EVENT_TEXT(0), NULL, endEvent1600 }," in s925
    # the four challengers: a talk sets the flag and starts the stage's event battle, but only
    # while SPECIAL(0x96) holds (the EU's second item set)
    for stage, flags in ((956, ("0xD",)), (949, ("0xE", "0xF")), (954, ("0x10",))):
        text = stage_text(stage)
        for f in flags:
            assert re.search(r"Talk1Conditions\[\] = \{ SPECIAL\(0x96\), 1, FLAG\(0x10, %s\), 0," % f, text), (stage, f)
            assert re.search(r"Talk1Actions\[\] = \{[^}]*FLAG\(0x10, %s\), 1[^}]*EVENT_BATTLE\(\d\), 1" % f
                             + r"|Talk1Actions\[\] = \{ EVENT_BATTLE\(\d\), 1, FLAG\(0x10, %s\), 1" % f, text), (stage, f)
    ev = (d / "src" / "engine" / "game" / "events.c").read_text()
    eu = ev[ev.index("#elif VERSION_EU\n    { 0x3A, 0x00, 0x00 }"):]
    assert "{ 0x54, 0x41, 0x00 }" in eu and "{ 0x96, 0x01, 0x00 }" in eu
    chk = ev[ev.index("s32 checkFlagCount(s32 group, s32 mode) {"):]
    assert re.search(r"case 1:\s+if \(testBit\(GAME\.flags10, 0xD, 1\)\s*&&\s*testBit\(GAME\.flags10, 0xE, 1\)\s*&&\s*"
                     r"testBit\(GAME\.flags10, 0xF, 1\)\s*&&\s*testBit\(GAME\.flags10, 0x10, 1\)\)", chk)
    items = ev[ev.index("s32 checkItemSet(s32 op, s32 arg) {"):]
    assert re.search(r"case 1:\s+if \(\(GAME\.items\[0x69\]", items)
    # the Arena: Kurt's event starts while flag 0x100C is clear and ends in the battle
    s927 = stage_text(927)
    assert re.search(r"checkCondition\(FLAG\(0x10, 0xC\), 0\)\) \{\s+children\[1\] = FIELDSTG_startEvent\(0x648\);", s927)
    assert re.search(r"void endEvent1608\(void\) \{\s+FLAGS_00\.applyAction\(FLAG\(0x10, 0xC\), 1\);\s+"
                     r"FLAGS_00\.applyAction\(EVENT_BATTLE\(0\), 1\);", s927)
    assert "{ 1608, script1608, EVENT_TEXT(4), NULL, endEvent1608 }," in s927
    assert re.search(r"SPECIAL\(0x54\), 1, FLAG\(0x10, 0xC\), 0, CODES_END", stage_text(926))
    # nothing in the extra chapter writes progress (the steps go by flags)
    for n in range(920, 975):
        assert "GAME.progress =" not in stage_text(n), n


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
    assert "dw3-walkthrough: open (from field, region EU, platform pc, language en, 12 chapters, 33 pages, 59 steps)" in log
    assert "dw3-walkthrough: open (from title, region EU, platform pc, language en, 12 chapters, 33 pages, 59 steps)" in log
    assert re.search(r"dw3-walkthrough: progress 2 half 0 area 19 -> chapter ch01 step ch01\.s03 "
                     r"\(rule progress\) \[open mode 0x200\]", log)
    assert "dw3-walkthrough: flags read 0x400d=1" in log
    # progress 2 is step ch01.s03, on the condensed page named by its first step, ch01.s02
    assert re.search(r"dw3-walkthrough: page ch01\.s02 \(kind MAIN, from open, reached y, current y, "
                     r"\d+ lines, \d+ us\)", log)
    assert "dw3-walkthrough: page ch01.s02 holds 2 step(s) ch01.s02 ch01.s03, 2 shown, current step ch01.s03" in log
    assert "dw3-walkthrough: spoiler ch01.s02 closed (answer available)" in log
    assert "dw3-walkthrough: spoiler ch01.s02 asked" in log
    assert "dw3-walkthrough: spoiler ch01.s02 cancelled" in log
    assert "dw3-walkthrough: spoiler ch01.s02 shown" in log
    assert "dw3-walkthrough: page ch01.s02 closed after" in log and "(spoiler shown y)" in log
    assert "(spoiler shown n)" in log
    assert re.search(r"dw3-walkthrough: page ch01\.s04 \(kind MAIN, from list, reached n, current n", log)
    assert "dw3-walkthrough: step not recognised (progress 32768)" in log
    assert "dw3-walkthrough: browse mode (no game state)" in log
    assert re.search(r"dw3-walkthrough: closed after \d+ ms \(pages opened \d+, spoilers shown 1\)", log)
    assert "dw3-walkthrough: text fallback wt.ch01_s03.goal (de)" in log
    # nothing overflows a plaque and no token was left open
    assert "title too wide" not in log, "a title is wider than its plaque: shorten the text"
    assert "unresolved" not in log


@pytest.fixture(scope="module")
def pages_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("walkthrough_pages") / "walkthrough_pages_test"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O1",
           "-DDW3_PORT_OPTIONS_TEST", "-DDW3_BUTTON_MAP_TEST",
           f"-I{ROOT / 'port' / 'include'}", f"-I{WT}", f"-I{PAGES}", f"-I{HELP}",
           "-o", str(out), str(TESTS / "walkthrough_pages_main.c"),
           str(WT / "walkthrough_screen.c"), *map(str, SOURCES), str(HELP / "help.c"),
           str(PAGES / "pages.c"), str(PAGES / "glyph_atlas.c"),
           str(ROOT / "port" / "options" / "button_map.c")]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_condensed_pages_follow_the_rules(wt_bin: Path):
    """FEEDBACK-5 (option B): a page holds consecutive MAIN steps of one chapter and one area (and one
    region), named by the first of them; the step ids and the table do not change."""
    rows = [r for r in table_rows(wt_bin) if r["kind"] == 0]
    by_page: dict[str, list[dict]] = {}
    for r in rows:
        by_page.setdefault(r["page"], []).append(r)
    for page, members in by_page.items():
        assert members[0]["id"] == page, f"page {page} is not named by its first step"
        assert len(members) <= 6, f"page {page} holds {len(members)} steps"
        for a, b in zip(members, members[1:]):
            assert rows.index(b) == rows.index(a) + 1, f"page {page}: {a['id']} and {b['id']} are not consecutive"
            assert (a["chapter"], a["area"], a["region"]) == (b["chapter"], b["area"], b["region"]), \
                f"page {page}: {a['id']} and {b['id']} differ in chapter, area or region"
    assert sum(len(m) for m in by_page.values()) == len(rows)
    first_half = [p for p in by_page if int(p[2:4]) <= 10]
    assert len(first_half) == 29, "the first half (55 steps) is read on 29 pages"
    us = [p for p, m in by_page.items() if m[0]["region"] in (0, 1)]
    assert len(us) == 30 and len(by_page) == 33


def test_condensed_pages_fit_the_panel(pages_bin: Path):
    """The page the player sees first (the goals of every step of the page, with the player past the
    whole table, and the 'answer is hidden' note) is at most 12 lines, one panel (13 lines), in every
    language and both regions: no scrolling to read what to do."""
    r = subprocess.run([str(pages_bin)], capture_output=True, text=True, timeout=120)
    assert r.returncode == 0, r.stderr
    log = r.stdout + r.stderr
    assert "title too wide" not in log, "a page title is wider than its plaque: shorten it"
    assert "text fallback" not in log, "a language has no text for a page"
    pages = [l.split() for l in r.stdout.splitlines() if l.startswith("PAGE ")]
    assert len(pages) >= 5 * (30 + 33)   # the pages, plus the optional rows and the area entries
    worst = max(int(p[5]) for p in pages)
    assert worst <= 12, f"a page takes {worst} lines: {[p for p in pages if int(p[5]) > 12][:5]}"


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


def test_hook_region_is_the_build_not_a_defined_macro():
    """The decomp headers define VERSION_EU as 0 on US, so `defined(VERSION_EU)` alone is true on both
    discs: the probe resolved the EU-only extra chapter on US. The region test must read the value."""
    src = (WT / "walkthrough_hook.c").read_text()
    assert "#if defined(VERSION_EU) && VERSION_EU\n#define WT_THIS_REGION WT_REGION_EU" in src


# --------------------------------------------------------------- slice 4: the side content

def decomp() -> Path:
    need_decomp()
    return ROOT / "vendor" / "dw3_decomp"


def special_table(region: str) -> dict[int, tuple[int, int]]:
    """SPECIAL_CONDITIONS of the US or the EU build: id -> (check, arg)."""
    ev = (decomp() / "src" / "engine" / "game" / "events.c").read_text()
    a = ev.index("SpecialCondition SPECIAL_CONDITIONS[] = {")
    cut = ev[a:ev.index("};", a)]
    if region == "US":
        body = cut[cut.index("#if VERSION_US"):cut.index("#elif VERSION_EU")]
    else:
        body = cut[cut.index("#elif VERSION_EU"):]
    return {int(i, 16): (int(c, 16), int(g, 16))
            for i, c, g in re.findall(r"\{ (0x[0-9A-F]{2}), (0x[0-9A-F]{2}), (0x[0-9A-F]{2}) \}", body)}


def progress_ranges() -> list[tuple[int, int]]:
    ev = (decomp() / "src" / "engine" / "game" / "events.c").read_text()
    a = ev.index("u8 PROGRESS_RANGES[][2] = {")
    return [(int(x, 16), int(y, 16)) for x, y in re.findall(r"\{ (0x[0-9A-F]{2}), (0x[0-9A-F]{2}) \}",
                                                           ev[a:ev.index("};", a)])]


def chapter_start(rows: list[dict], chapter: str) -> int:
    return min(r["frm"] for r in rows if r["kind"] == 0 and r["chapter"] == chapter)


def test_side_rows_table_shape(wt_bin: Path):
    """The slice-4 rows: six SIDE rows, each in the chapter whose first value is its gate, in the
    area the text file knows, EU only in the extra chapter, and with all five languages' text."""
    rows = table_rows(wt_bin)
    side = {r["id"]: r for r in rows if r["kind"] == 1}
    assert list(side) == ["ch01.x01", "ch04.x01", "ch04.x02", "ch08.x01", "ch08.x02", "ch12.x01", "ch12.x02"]
    for sid, r in side.items():
        if sid != "ch01.x01":
            assert r["frm"] == chapter_start(rows, r["chapter"]), sid        # shown from the chapter on
        assert r["to"] == -1
        assert r["region"] == (2 if r["chapter"] == "ch12" else 0), sid
    # the table is ordered by progress and its `order` grows over the whole table
    assert [r["frm"] for r in rows] == sorted(r["frm"] for r in rows)
    assert [r["order"] for r in rows] == sorted(r["order"] for r in rows)
    assert len({r["order"] for r in rows}) == len(rows)
    # the map area is that of the row's main place: the Underground (825 to 895, 970, 971) has none, Kulon
    # Weapons (731) is in the North Sector
    src = (decomp() / "src" / "field" / "field_mode" / "data" / "fieldstg.c").read_text()
    t = src[src.index("StageEntry FIELDSTG_stages[] = {"):]
    mode = {int(m.group(2)): int(m.group(1)) for m in re.finditer(r"\{(\d+), \d+, WSTAG(\d+)_startStage\}", t[:t.index("};")])}
    ex = src[src.index("StageEntry FIELDSTG_extraStages[] = {"):]
    emode = {int(m.group(2)): int(m.group(1)) for m in re.finditer(r"\{(\d+), \d+, WSTAG(\d+)_startStage\}", ex[:ex.index("};")])}
    mine = re.search(r"k_map_areas\[\d+\] = \{(.*?)\};", (WT / "walkthrough_areas.c").read_text(), re.S)
    areas = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", mine.group(1))]

    def area(m: int) -> int:
        v = areas[m & 0xFF]
        return -1 if v == 0xFF else v & 0x7F
    assert side["ch12.x01"]["map_area"] == area(mode[731]) == 8
    for sid, stage in (("ch04.x01", 825), ("ch04.x02", 845), ("ch08.x01", 865), ("ch08.x02", 875)):
        assert side[sid]["map_area"] == area(mode[stage]) == -1, sid
    assert side["ch12.x02"]["map_area"] == area(emode[970]) == area(emode[971]) == -1
    for lang in LANGS:
        _, keys = parse_text(WT / "text" / f"{lang}.txt")
        for sid in list(side)[1:]:
            for layer in ("title", "goal", "help"):
                assert keys[f"wt.{sid.replace('.', '_')}.{layer}"], (lang, sid, layer)
        # a goal never carries the answer: it must not name the places the help names
        assert "Seehomon" not in keys["wt.ch04_x02.goal"] and "Kulon" not in keys["wt.ch12_x01.goal"]


def test_seabed_and_circuit_board_gates_match_decomp(wt_bin: Path):
    """ch04.x01 / x02: the UG Lakes (525, 526) and Duel Island (385) warp to mode 0x2E0 (WSTAG825) behind
    SPECIAL(0x93), which is PROGRESS_RANGES[0xC] = 0x0F..0x63 on both discs; ch08.x01 / x02: the sector
    maps warp to 0x2E8 / 0x2E9 (WSTAG865 / 870) behind SPECIAL(0x94) = PROGRESS_RANGES[0xD] = 0x1E..0x63."""
    d = decomp()
    rows = table_rows(wt_bin)
    ranges = progress_ranges()
    for region in ("US", "EU"):
        sp = special_table(region)
        assert sp[0x93] == (0x30, 0x0C) and sp[0x94] == (0x30, 0x0D), region
    assert ranges[0xC] == (0x0F, 0x63) and ranges[0xD] == (0x1E, 0x63)
    assert chapter_start(rows, "ch04") == ranges[0xC][0] and chapter_start(rows, "ch08") == ranges[0xD][0]
    ev = (d / "src" / "engine" / "game" / "events.c").read_text()
    chk = ev[ev.index("s32 checkProgressRange("):]
    assert "int min" not in chk and "value >= min" in chk and "max >= value" in chk
    for stage in (525, 526, 385):
        assert re.search(r"\{ \{ \{ SPECIAL\(0x93\), 1 \}, \{ CODES_END, 0 \} \}, SLOT_WARP0, 0x2E0,", stage_text(stage)), stage
    for stage in (395, 480, 481, 490, 491, 690, 706):
        assert re.search(r"SPECIAL\(0x94\), 1 \}, \{ CODES_END, 0 \} \}, SLOT_WARP1, 0x2E[89],", stage_text(stage)), stage
    stg = (d / "src" / "field" / "field_mode" / "data" / "fieldstg.c").read_text()
    for mode, stage in ((736, 825), (744, 865), (745, 870), (746, 875)):
        assert f"{{{mode}, " in stg and re.search(r"\{%d, \d+, WSTAG%d_startStage\}" % (mode, stage), stg), (mode, stage)
    assert 0x2E0 == 736 and 0x2E8 == 744 and 0x2E9 == 745
    # the stage headers the help text summarises
    h = {n: stage_text(n)[:600] for n in (825, 830, 835, 840, 845, 850, 855, 860, 865, 875, 885, 890, 895)}
    assert "below the Sewers, Duel Island, Kicking Forest" in h[825] and "UG Lake" in h[825]
    assert "below the bridges" in h[830]
    assert "Divermon's Lake" in h[835] and "South and West" in h[835]
    assert "Shell Beach, Plug Cape, South Cape and" in h[840] and "Lake of Ice" in h[840]
    assert "Central Park, Bulk Bridge and Phoenix Bay" in h[850]
    assert "junction" in h[855] and "Seehomon" in h[845] and "Depthmon" in h[845]
    assert "chests" in h[860]
    assert "Central, East, South, West and North Sector" in h[865]
    assert "Black Kingz" in h[875] and "Black Kingz and" in h[885] and "Numemon" in h[885]
    assert "guides to both servers" in h[890] and "guides to both servers" in h[895]


def test_seabed_forge_and_chests_match_decomp():
    """ch04.x02: Seehomon's and Depthmon's forge (WSTAG845): after a first talk (temporary flag 0), a second
    with the usable item swaps it (and the weapon one grade below) for the stronger weapon; two upgrades.
    ch04.x01 help: the dead end WSTAG860 has two chests that each give an item once."""
    s = stage_text(845)
    assert len(re.findall(r"Talk1Actions\[\] = \{ FLAG\(0, 0\), 1, CODES_END \}", s)) == 4
    recipes = re.findall(r"Talk3Actions\[\] = \{ ITEM\(3, (0x[0-9A-F]+)\), 1, ITEM\(3, (0x[0-9A-F]+)\), 0, "
                         r"ITEM\(2, (0x[0-9A-F]+)\), 0, CODES_END \}", s)
    assert sorted(recipes) == sorted([("0x76", "0x75", "0x72"), ("0x9B", "0x9A", "0x97"),
                                      ("0x9B", "0x9A", "0x97"), ("0x9B", "0x9A", "0x97")])
    assert len({r[0] for r in recipes}) == 2                                   # "two upgrades"
    for out, prev, _ in recipes:                                              # one grade below: the id minus one
        assert int(out, 16) - int(prev, 16) == 1
    # the talk that swaps needs the usable item, the temporary flag and that the result is not owned yet
    assert re.search(r"actor0Talk3Conditions\[\] = \{ ITEM\(3, 0x76\), 0, FLAG\(0, 0\), 1, ITEM\(2, 0x72\), 1, CODES_END \}", s)
    c = stage_text(860)
    assert re.search(r"actor0Talk0Actions\[\] = \{ FLAG\(2, 0x2C\), 1, ITEM\(1, 0x36\), 1, CODES_END \}", c)
    assert re.search(r"actor1Talk0Actions\[\] = \{ FLAG\(2, 0x55\), 1, ITEM\(1, 0x40\), 1, CODES_END \}", c)
    assert "actor0Conditions[] = { WARP_ARG(5), 1, WARP_ARG(0x1E), 1, FLAG(2, 0x2C), 0," in c
    gs = (decomp() / "include" / "engine" / "game_state.h").read_text()
    assert "#define ITEM_KIND_USABLE 2" in gs and "#define ITEM_KIND_WEAPON 3" in gs


def test_black_kingz_den_matches_decomp():
    """ch08.x02 (WSTAG875): eight Black Kingz offer a card duel until it is won; six give a card on the first
    win (the duel is offered only while the card is not owned), one gives a key item (ITEM(0, 0x18D))."""
    s = stage_text(875)
    battles = re.findall(r"actor(\d+)Talk0Actions\[\] = \{ CARD_BATTLE\((0x[0-9A-F]+), 0\), 1, CODES_END \}", s)
    assert [int(a) for a, _ in battles] == list(range(2, 10))                  # eight opponents, actors 2 to 9
    assert len(set(o for _, o in battles)) == 8
    cards = re.findall(r"actor(\d+)Talk3Actions\[\] = \{ FLAG\(0, 0x11\), 0, FLAG\(0, 0x10\), 0, CARD\((0x[0-9A-F]+)\), 1, CODES_END \}", s)
    assert [int(a) for a, _ in cards] == list(range(4, 10))                    # six give a card
    for a, card in cards:                                                      # offered only while it is not owned
        assert re.search(r"actor%sTalk0Conditions\[\] = \{[^}]*CARD\(%s\), 0" % (a, card), s), a
        assert re.search(r"actor%sTalk1Conditions\[\] = \{[^}]*CARD\(%s\), 1" % (a, card), s), a
    assert re.search(r"actor3Talk3Actions\[\] = \{\s*FLAG\(0, 0x11\), 0,\s*FLAG\(0, 0x10\), 0,\s*FLAG\(0x1C, 0x40\), 1,\s*ITEM\(0, 0x18D\), 1,", s)
    assert re.search(r"actor3Talk0Conditions\[\] = \{ FLAG\(0x1C, 0x40\), 0,", s)
    assert re.search(r"actor2Talk3Actions\[\] = \{[^}]*FLAG\(0x1C, 0x3F\), 1", s)    # the other one only sets a flag
    ev = (decomp() / "src" / "engine" / "game" / "events.c").read_text()
    assert "pendingFlag10" in ev and "applyAction(0x10, 1)" in ev                     # flag (0, 0x10) = won the duel
    assert "GAME.funcs.getPrevMode() == MODE_CARD_GAME" in ev


def test_five_weapons_match_decomp():
    """ch12.x01: the five weapons SPECIAL(0x96) asks for (items 0x69, 0x77, 0x83, 0x90, 0x9C) are what the five
    smiths of Kulon Weapons (WSTAG731, the Amaterasu twin of 730) hand out: each swaps one usable item (and the
    weapon one grade below, if carried) after a first talk."""
    s = stage_text(731)
    assert "Kulon Weapons" in s[:200] and "Amaterasu" in s[:200]
    want = [("0x9C", "0x9B", "0x98"), ("0x90", "0x8F", "0x8C"), ("0x77", "0x76", "0x73"),
            ("0x83", "0x82", "0x7F"), ("0x69", "0x68", "0x65")]
    got = re.findall(r"Talk3Actions\[\] = \{\s*ITEM\(3, (0x[0-9A-F]+)\), 1,\s*ITEM\(3, (0x[0-9A-F]+)\), 0,\s*"
                     r"ITEM\(2, (0x[0-9A-F]+)\), 0,\s*SPECIAL\(0x13\), 1,", s)
    assert got == want
    for out, prev, mat in want:
        assert int(out, 16) - int(prev, 16) == 1
        assert re.search(r"Talk3Conditions\[\] = \{ ITEM\(3, %s\), 0, FLAG\(0, \d\), 1, ITEM\(2, %s\), 1, CODES_END \}"
                         % (out, mat), s)
        assert re.search(r"Talk1Actions\[\] = \{ FLAG\(0, \d\), 1, CODES_END \}", s)
    ev = (decomp() / "src" / "engine" / "game" / "events.c").read_text()
    items = ev[ev.index("s32 checkItemSet(s32 op, s32 arg) {"):]
    case1 = items[items.index("case 1:"):items.index("break;", items.index("case 1:"))]
    assert re.findall(r"GAME\.items\[(0x[0-9A-F]+)\]", case1) == ["0x69", "0x77", "0x83", "0x90", "0x9C"]
    assert special_table("EU")[0x96] == (0x01, 0x00)                           # check 0x01 = item set, op 1
    gs = (decomp() / "include" / "engine" / "game_state.h").read_text()
    assert "#define ITEM_KIND_USABLE 2" in gs and "#define ITEM_KIND_WEAPON 3" in gs


def test_tunnel_partners_match_decomp():
    """ch12.x02: WSTAG970 and 971 (the extra chapter's Circuit Board) hold a fighter for each of the eight
    partners. Each stands only while its partner is not unlocked (SPECIAL ... == 0) and SPECIAL(0x95) holds
    (every unlocked partner is level 0x2D = 45 or more); a first talk sets a temporary flag, a second starts the
    event battle once (flag 0xA:x), and the win unlocks the partner and gives a key item."""
    d = decomp()
    eu = special_table("EU")
    assert eu[0x95] == (0x17, 0x00)                         # check 0x17: partner op 7 (EU)
    ev = (d / "src" / "engine" / "game" / "events.c").read_text()
    case7 = ev[ev.index("    case 7:\n        result = 1;"):ev.index("    case 8:\n")]
    assert "STAT_LEVEL] < 0x2D" in case7 and "result = 0" in case7
    assert "case 0x10" not in case7
    unlock_of = {i: g for i, (c, g) in eu.items() if c == 0x13}                # SPECIAL(id) unlocks partner arg
    owned_of = {i: g for i, (c, g) in eu.items() if c == 0x11}                 # SPECIAL(id): partner arg is unlocked
    assert sorted(unlock_of.values()) == list(range(8)) and sorted(owned_of.values()) == list(range(8))
    unlocked = set()
    for stage in (970, 971):
        s = stage_text(stage)
        assert len(re.findall(r"EVENT_BATTLE\(\d\), 1", s)) == 4
        for m in re.finditer(r"u16 actor(\d+)Conditions\[\] = \{(.*?)CODES_END", s, re.S):
            if "SPECIAL(0x95), 1" not in m.group(2):
                continue
            a = m.group(1)
            if not re.search(r"actor%sTalk0Actions\[\] = \{ SPECIAL\(0x[0-9A-F]+\), 1, CODES_END" % a, s):
                continue                                                          # a guide, not a fighter
            owned = [int(x, 16) for x in re.findall(r"SPECIAL\((0x[0-9A-F]+)\), 0", m.group(2))]
            talk0 = re.search(r"actor%sTalk0Actions\[\] = \{ SPECIAL\((0x[0-9A-F]+)\), 1" % a, s)
            partner = unlock_of[int(talk0.group(1), 16)]
            assert [owned_of[o] for o in owned] == [partner], (stage, a)       # appears while that partner is not unlocked
            assert re.search(r"actor%sTalk1Actions\[\] = \{ FLAG\(0, 0\), 1, CODES_END \}" % a, s), (stage, a)
            assert re.search(r"actor%sTalk2Actions\[\] = \{ EVENT_BATTLE\(\d\), 1, FLAG\(0xA, 0x[0-9A-F]+\), 1, CODES_END \}" % a, s)
            sp, it = talk0.group(1), r"ITEM\(0, (?:0x)?[0-9A-F]+\), 1"
            assert re.search(r"actor%sTalk3Actions\[\] = \{ (?:SPECIAL\(%s\), 1, %s|%s, SPECIAL\(%s\), 1), CODES_END \}"
                             % (a, sp, it, it, sp), s), (stage, a)
            unlocked.add(partner)
    assert unlocked == set(range(8))
    stg = (d / "src" / "field" / "field_mode" / "data" / "fieldstg.c").read_text()
    ex = stg[stg.index("StageEntry FIELDSTG_extraStages[] = {"):]
    assert "{746, 2267, WSTAG970_startStage}" in ex and "{747, 2268, WSTAG971_startStage}" in ex
    assert "Black Kingz' den" in stage_text(970)[:300] and "reached only from the other tunnels" in stage_text(971)[:300]
    # the fighters' spots depend on how the player arrived (WARP_ARG), which the help says
    assert "WARP_ARG(4), 1" in stage_text(970)


def test_duel_island_has_no_confirmed_entry():
    """Left unshipped (P6.35 slice 4): nothing in the stages leads into Duel Island (modes 552 / 0x228 and 663 /
    0x297) from a field map: the only exits to those modes are the island's own and the Seabed's return exit,
    which exists only after arriving from the island. A row would send the player to a place the decomp does
    not show a way to. If this test fails, a way in was found: write the rows."""
    stages = ROOT / "vendor" / "dw3_decomp" / "src" / "field" / "stages"
    need_decomp()
    hits = []
    for p in sorted(stages.glob("*/wstag*.c")):
        for line in p.read_text().splitlines():
            if re.search(r"SLOT_(EXIT|WARP[01]|LAUNCH_OUT),\s*(0x228|0x297|552|663),", line):
                hits.append((p.name, line.strip()))
            if re.search(r"SCRIPT_LEAVE\((0x228|0x297|552|663),", line):
                hits.append((p.name, line.strip()))
    assert {h[0] for h in hits} <= {"wstag385.c", "wstag386.c"}, hits   # the island's own reset after an event
