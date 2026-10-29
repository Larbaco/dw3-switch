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
    for _, sid, chapter, area, _kind, _region in rows:
        chapters.add(chapter)
        areas.add(area)
        for layer in ("title", "goal", "help"):
            key = f"wt.{sid}.{layer}"
            assert key in keys and keys[key], f"missing en text {key}"
            assert len(keys[key]) <= BUDGET[layer], f"{key} over budget ({len(keys[key])})"
    for c in chapters:
        assert f"wt.{c}.title" in keys
    for a in areas:
        assert f"wt.area.{a}.title" in keys and f"wt.area.{a}.help" in keys
        assert len(keys[f"wt.area.{a}.help"]) <= BUDGET["help"]
    # no key without a table row
    ids = {r[1] for r in rows}
    for k in keys:
        m = re.match(r"wt\.(ch\d+\.[sx]\d+)\.(title|goal|help)$", k)
        if m:
            assert m.group(1) in ids, f"text key {k} has no step"


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
            if re.match(r"wt\.ch\d+\.[sx]\d+\.", k):
                limit = BUDGET[layer]
            elif k.startswith("wt.area.") and layer == "help":
                limit = BUDGET["help"]
            else:
                limit = BUDGET["title"]
            assert len(v) <= limit, f"{lang}: {k} over budget ({len(v)} > {limit})"
            assert v.count("TODO(verify)") == en[k].count("TODO(verify)"), f"{lang}: {k} TODO marker"
            bad = {c for c in v if ord(c) not in charset}
            assert not bad, f"{lang}: {k} has glyphs outside the atlas: {bad}"


def test_no_unverified_marker_in_release():
    """A release build refuses TODO(verify); today it is only counted, so the
    draft state is visible in CI output."""
    text = (WT / "text" / "en.txt").read_text(encoding="utf-8")
    n = len(re.findall(r"TODO\(verify\)", text))
    print(f"TODO(verify) markers in en.txt: {n}")


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


def test_progress_values_match_decomp_scripts():
    """The step table's progress values are the ones the stage scripts assign."""
    stages = ROOT / "vendor" / "dw3_decomp" / "src" / "field" / "stages"
    if not stages.exists():
        pytest.skip("decomp submodule not present")
    expect = {
        "kusanagi_city/wstag780.c": 0, "kusanagi_city/wstag785.c": 1,
        "asuka_city/wstag225.c": 2, "asuka_city/wstag212.c": 3,
        "central_sector/wstag200.c": 4, "east_sector/wstag420.c": 5,
    }
    for rel, value in expect.items():
        text = (stages / rel).read_text()
        assert re.search(rf"GAME\.progress = {value};", text), rel
