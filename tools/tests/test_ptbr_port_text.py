"""P6.7: the PT-BR drafts of the port's own screens (Help, Walkthrough, Options lettering).
They are our own writing; no game data is read. Since phase 3 the generators embed them by
default (language index 5), so these tests prove that the drafts are complete, fit and are what
the committed generated files carry."""

from __future__ import annotations

import importlib.util
import re
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "l10n"))
sys.path.insert(0, str(ROOT / "tools" / "tests"))

BUDGET = {"title": 40, "goal": 240, "help": 480}
# generate_help_assets.CHARSET: Latin-1 and a few symbols
ATLAS = set(range(0x20, 0x7F)) | set(range(0xA0, 0x100)) | {
    0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026}


def load(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


def parse_wt(path: Path):
    headers, keys = {}, {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        left, _, value = line.partition("=")
        left, value = left.strip(), value.strip()
        if left.startswith("@"):
            headers[left[1:]] = value
        else:
            assert left not in keys, f"duplicate key {left}"
            keys[left] = value
    return headers, keys


def test_walkthrough_pt_is_complete_fits_and_is_flagged():
    wt = ROOT / "port" / "walkthrough" / "text"
    _, en = parse_wt(wt / "en.txt")
    headers, pt = parse_wt(wt / "pt.txt")
    assert headers["lang"] == "pt" and "owner review pending" in headers["status"]
    assert list(pt) == list(en), "keys differ from en.txt"
    for k, v in pt.items():
        assert v, f"{k} is empty"
        layer = k.rsplit(".", 1)[1]
        if re.match(r"wt\.ch\d+_[sx]\d+\.", k) or (k.startswith("wt.area_") and layer == "help"):
            limit = BUDGET[layer]
        elif k.startswith("wt.ui."):
            limit = BUDGET["goal"]
        else:
            limit = BUDGET["title"]
        assert len(v) <= limit, f"{k} over budget ({len(v)} > {limit})"
        assert "TODO(verify)" not in v
        assert not {c for c in v if ord(c) not in ATLAS}, f"{k}: glyphs outside the atlas"
        assert v.count("{{") == en[k].count("{{")


def test_help_pt_validates_and_is_complete():
    gen = load(ROOT / "tools" / "generate_help_assets.py", "gen_help")
    errors, warnings = gen.validate()
    assert [e for e in errors if "pt.txt" in e] == []
    _, en_entries, _ = gen.parse_text(gen.TEXT_DIR / "en.txt")
    headers, pt_entries, errs = gen.parse_text(gen.TEXT_DIR / "pt.txt")
    assert errs == [] and headers["lang"] == "pt" and "owner review pending" in headers["status"]
    assert [e[0] for e in pt_entries] == [e[0] for e in en_entries], "keys differ from en.txt"
    assert [e[1] for e in pt_entries] == [e[1] for e in en_entries], "qualifiers differ from en.txt"
    assert not [w for w in warnings if "pt.txt" in w], "pt.txt is partial or a stub"
    # the same tokens, in the same order, as the English line
    pt_tokens = {k: gen.expand_chars(v)[1] for k, _q, v, _n in pt_entries}
    for k, _q, v, _n in en_entries:
        assert gen.expand_chars(v)[1] == pt_tokens[k], f"{k}: tokens differ"


def test_embedded_text_files_carry_pt_by_default():
    """Phase 3: pt is language index 5 of every embedded table, with no flag: the committed .inc files
    are what the generators write, and pt is neither optional nor an unused array any more."""
    gen = load(ROOT / "tools" / "generate_help_assets.py", "gen_help2")
    assert gen.LANGS == ("en", "fr", "de", "es", "it", "pt")
    assert gen.TEXT_INC.read_text(encoding="utf-8") == gen.build_text_inc()
    assert "help_text_pt" in gen.build_text_inc()
    wt = load(ROOT / "tools" / "generate_walkthrough_text.py", "gen_wt")
    assert wt.LANGS == ("en", "fr", "de", "es", "it", "pt")
    assert wt.OUT.read_text(encoding="utf-8") == wt.build()
    assert "wt_text_pt" in wt.build()
    for inc in (gen.TEXT_INC, wt.OUT):
        assert "__attribute__((unused))" not in inc.read_text(encoding="utf-8")
    # the Help text itself: the Portuguese the player reads is the draft file
    assert 'help_text_pt[] =' in gen.TEXT_INC.read_text(encoding="utf-8")
    assert "Instalação" in gen.TEXT_INC.read_text(encoding="utf-8")


def test_lettering_inc_bakes_pt_for_every_english_entry():
    inc = (ROOT / "platforms" / "pc" / "src" / "title_overlay_lettering.inc").read_text(encoding="utf-8")
    assert "__attribute__((unused))" not in inc
    import ptbr_options as po

    names = set(re.findall(r"static const unsigned char title_(\w+)\[\]", inc))
    for stem in po.PT_LETTERING:
        name = "options_pt" if stem == "options_plaque" else stem + "_pt"
        assert name in names, f"{name} is not baked"
    # every English entry has its _pt twin (the generator fails otherwise, this proves the output)
    for n in names:
        if n.endswith("_en") and not n.startswith(("title_", "credit")) and n not in ("start",):
            assert n[:-3] + "_pt" in names or n[:-3] in ("start",), f"no _pt for {n}"


def test_options_lettering_pt_covers_every_english_entry_and_fits():
    pytest.importorskip("PIL")
    if not Path("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf").exists():
        pytest.skip("DejaVu fonts not installed")
    inc = ROOT / "platforms" / "pc" / "src" / "title_overlay_lettering.inc"
    before = inc.read_bytes()
    r = subprocess.run([sys.executable, str(ROOT / "tools" / "generate_pc_title_lettering.py"), "--check-pt"],
                       capture_output=True, text=True, cwd=ROOT)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "PT-BR lettering ok" in r.stdout
    # --check-pt writes nothing
    assert inc.read_bytes() == before


def test_options_lettering_pt_text_is_latin1_and_non_empty():
    import ptbr_options as po

    assert po.PT_LETTERING and "review pending" in po.STATUS
    for k, v in po.PT_LETTERING.items():
        assert v.strip(), k
        assert not {c for c in v if ord(c) not in ATLAS}, f"{k}: {v!r}"
