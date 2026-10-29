"""P6.30 technical help: the page engine (parser, filters, tokens, layout,
scroll), the help screen flow on the real text and atlas, and the text files
(validator, language stubs). Plain C and text files, no game data."""

from __future__ import annotations

import importlib.util
import re
import subprocess
import sys
import warnings
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"
PAGES = ROOT / "port" / "pages"
HELP = ROOT / "port" / "help"
CFLAGS = ["-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O1", "-g",
          f"-I{ROOT / 'port' / 'include'}", f"-I{HELP}", f"-I{PAGES}"]


def build(out: Path, srcs: list[Path], extra: list[str] | None = None) -> Path:
    cmd = ["gcc", *CFLAGS, *(extra or []), "-o", str(out), *map(str, srcs)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


@pytest.fixture(scope="module")
def pages_bin(tmp_path_factory) -> Path:
    return build(tmp_path_factory.mktemp("pages") / "pages_test",
                 [TESTS / "pages_main.c", PAGES / "pages.c"],
                 ["-fsanitize=address,undefined"])


@pytest.fixture(scope="module")
def help_bin(tmp_path_factory) -> Path:
    return build(tmp_path_factory.mktemp("help") / "help_test",
                 [TESTS / "help_main.c", HELP / "help.c", PAGES / "pages.c",
                  PAGES / "glyph_atlas.c", ROOT / "port" / "options" / "button_map.c"],
                 ["-DDW3_PORT_OPTIONS_TEST", "-DDW3_BUTTON_MAP_TEST",
                  "-fsanitize=address,undefined"])


def test_page_engine(pages_bin: Path):
    r = subprocess.run([str(pages_bin)], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK: page engine" in r.stdout


def test_help_flow(help_bin: Path):
    r = subprocess.run([str(help_bin)], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK: help" in r.stdout
    log = r.stderr
    # AGENTS.md rule 12: the log lines that answer the test plan
    assert re.search(r"dw3-pages: atlas \d+ glyphs in 3 faces, \d+ bytes of coverage in the "
                     r"image, index built in \d+ us", log)
    assert re.search(r"dw3-pages: text help en loaded \(\d+ keys, \d+ qualified, \d+ filtered "
                     r"out for US/pc, \d+ bytes\)", log)
    assert "dw3-help: open (from title, region US, platform pc, language en, 10 pages)" in log
    assert "dw3-help: open (from field, region US, platform switch, language en, 10 pages)" in log
    assert "dw3-help: open (from battle, region EU, platform switch, language en, 10 pages)" in log
    assert re.search(r"dw3-help: page controls \(from list, \d+ lines, panel 13 lines, "
                     r"unverified blocks 0\)", log)
    assert "dw3-help: page hotkeys (from next," in log
    assert "dw3-help: page controls (from prev," in log
    assert re.search(r"dw3-help: page controls end reached \(line \d+ of \d+\)", log)
    assert re.search(r"dw3-help: page controls closed after \d+ ms \(read to line \d+ of \d+\)", log)
    assert re.search(r"dw3-help: layout controls \d+ lines in \d+ us", log)
    assert re.search(r"dw3-help: closed after \d+ ms \(pages opened \d+\)", log)
    assert 'dw3-pages: token {btn:cross} -> "[B]"' in log      # Switch names
    assert 'dw3-pages: token {btn:cross} -> "[X]"' in log      # PC names
    assert "unresolved" not in log
    # a stub language: one line; a partial pack: one line per missing key
    assert "dw3-pages: text fallback help * (fr): the language file has no keys yet" in log
    assert "dw3-pages: text fallback help help.setup.title (fr)" in log


def test_page_lengths_and_tokens(help_bin: Path):
    r = subprocess.run([str(help_bin), "dump"], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    rows = [l.split() for l in r.stdout.splitlines() if l.startswith("PAGE")]
    assert len(rows) == 40            # 10 pages x (US, EU) x (PC, Switch)
    for row in rows:
        lines = int(row[4].split("=")[1])
        assert row[5] == "unresolved=0", row
        if lines > 120:
            warnings.warn(f"help page {row[3]} has {lines} lines (> 120)")
        assert lines <= 200, row


@pytest.mark.parametrize("src", [PAGES / "pages.c", PAGES / "glyph_atlas.c", HELP / "help.c"])
def test_compiles_as_c90(src: Path, tmp_path: Path):
    """The port library is built as gnu89: these files must be valid there."""
    cmd = ["gcc", "-std=gnu89", "-Wall", "-Wextra", "-Werror", "-Wno-long-long",
           "-DDW3_PORT_OPTIONS_TEST", f"-I{ROOT / 'port' / 'include'}", f"-I{HELP}", f"-I{PAGES}", "-c",
           "-o", str(tmp_path / "o.o"), str(src)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout


# --- the text files ---------------------------------------------------------

def load_generator():
    spec = importlib.util.spec_from_file_location(
        "generate_help_assets", ROOT / "tools" / "generate_help_assets.py")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


def test_text_validator():
    pytest.importorskip("PIL")
    gen = load_generator()
    if not (gen.FONT_ROOT / "DejaVuSans.ttf").exists():
        pytest.skip("DejaVu Sans (fonts-dejavu-core) is needed to measure titles")
    errors, warns = gen.validate()
    assert not errors, "\n".join(errors)
    # P6.30 owner decision 2026-10-28: the help must exist in every base-game
    # language at launch. While the files are stubs this only warns (and the
    # help falls back to English with a log line); `--release` makes it fail.
    for w in warns:
        warnings.warn(f"P6.30 launch gap: {w}")
    errors, _ = gen.validate(release=True)
    stubs = [e for e in errors if "stub" in e or "partial" in e]
    drafts = [e for e in errors if "@status" in e]
    # The release gate must fire while a language is missing or the text is draft.
    if warns or drafts:
        assert errors, "release validation must fail while text is a stub or draft"
    assert not stubs or warns


def test_embedded_text_is_current():
    gen = load_generator()
    assert gen.TEXT_INC.read_text(encoding="utf-8") == gen.build_text_inc(), \
        "run tools/generate_help_assets.py"


def test_atlas_covers_the_text_and_is_sorted():
    inc = (PAGES / "help_atlas.inc").read_text()
    for face in range(3):
        m = re.search(r"atlas_g%d\[\d+\] = \{(.*?)\n\};" % face, inc, re.S)
        cps = [int(x) for x in re.findall(r"\{(\d+),", m.group(1))]
        assert cps == sorted(cps) and len(cps) == len(set(cps))
        assert 0xFFFD in cps and 0xE9 in cps
    # every glyph's bitmap lies inside the coverage table
    size = int(re.search(r"atlas_cov\[(\d+)\]", inc).group(1))
    for g in re.findall(r"\{(\d+),(\d+),(\d+),(\d+),(-?\d+),(-?\d+),(\d+)\}", inc):
        _, adv, w, h, _, _, off = map(int, g)
        assert off + w * h <= size and adv < 256


def test_keys_have_titles_and_blurbs():
    gen = load_generator()
    headers, entries, errs = gen.parse_text(HELP / "text" / "en.txt")
    assert not errs
    pages: dict[str, set[str]] = {}
    for key, *_ in entries:
        _, page, kind, *_ = key.split(".")
        pages.setdefault(page, set()).add(kind)
    for page, kinds in pages.items():
        if page != "ui":
            assert {"title", "blurb"} <= kinds, page
    assert headers["lang"] == "en" and headers["format"] == "1"
