"""P6.7 text tooling (tools/l10n): the container parser, the font codec, the template
extraction and the --check validator. Synthetic data only: no game text, no runtime."""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "l10n"))

import extract_text as X  # noqa: E402
import textfmt as T  # noqa: E402

EU = T.codec_for("eu")
US = T.codec_for("us")


def m(text: str, codec=EU) -> bytes:
    return T.encode(text, codec)


# A tiny font: every glyph and icon is 5 px wide, except the space (icon 1), 3 px.
def flat_metrics() -> T.Metrics:
    icons = [5] * 114
    icons[0] = 3
    return T.Metrics([5] * 230, icons)


FAKE_SOURCE = """
Glyph FONT_GLYPHS_1[3] = {
#if VERSION_US
    { 1, 0, 0, 48, 232, 8, 12, 1, 0, 6, 12 },
#elif VERSION_EU
    { 1, 0, 0, 48, 232, 8, 12, 1, 0, 7, 12 }, { 1, 0, 0, 48, 232, 8, 12, 0, 0, 5, 12 },
#endif
    { 1, 0, 0, 48, 232, 8, 12, 2, 0, 6, 12 },
};
Glyph FONT_ICONS_1[2] = {
#if VERSION_US
    { 1, 0, 0, 48, 232, 8, 12, 1, 0, 3, 12 }, { 1, 0, 0, 48, 232, 8, 12, 1, 0, 4, 12 },
#elif VERSION_EU
    { 1, 0, 0, 48, 232, 8, 12, 1, 0, 8, 12 }, { 1, 0, 0, 48, 232, 8, 12, 1, 0, 9, 12 },
#endif
};
"""


def write_fake_decomp(root: Path, source: str | None = None) -> Path:
    """A decomp folder whose text_tools.c has just the glyph arrays: 230 glyphs of 7 px
    (EU) or 6 px (US) and 114 icons of 9 px (EU) or 8 px (US), in all three sizes."""
    src = root / "src" / "engine" / "text"
    src.mkdir(parents=True)
    if source is None:
        def cells(adv, n):
            return ", ".join("{ 1, 0, 0, 48, 232, 8, 12, 1, 0, %d, 12 }" % adv for _ in range(n))

        parts = []
        for size in (1, 2, 3):
            for name, n, eu, us in (("GLYPHS", 230, 6, 5), ("ICONS", 114, 8, 7)):
                parts.append("Glyph FONT_%s_%d[%d] = {\n#if VERSION_US\n%s,\n#elif VERSION_EU\n%s,\n#endif\n};\n"
                             % (name, size, n, cells(us, n), cells(eu, n)))
        source = "".join(parts)
    (src / "text_tools.c").write_text(source)
    return root


# --------------------------------------------------------------------------
# codec

def test_text_roundtrip_and_accents():
    text = "Você não pôde, Ção! 100% {name:1}?{wait:2}{pg}"
    raw = m(text)
    assert T.decode(raw, EU) == text
    # a letter is one glyph byte, an accent or a space is icon + index
    assert m("A") == bytes((14,))
    assert m(" ") == b"\x01\x01"
    assert m("ã") == b"\x01\x4d"  # icon 77
    assert m("a\nb") == bytes((40, 2, 1, 41))


def test_every_ptbr_letter_is_in_the_eu_font_and_not_in_the_us_one():
    for ch in T.PTBR_ACCENTS:
        assert EU.can_encode(ch), ch
    assert not US.can_encode("ã") and not US.can_encode("ç")
    with pytest.raises(T.EncodeError):
        T.encode("não", US)


def test_unknown_and_malformed_tokens_are_rejected():
    for bad in ("{bogus}", "{wait}", "{wait:999}", "abc{", "中", "tab\there"):
        with pytest.raises(T.EncodeError):
            T.encode(bad, EU)


def test_unmapped_bytes_and_codes_roundtrip_through_tokens():
    raw = bytes((1, 200, 150, 2, 4, 1, 2, 3, 3, 2, 9)) + b"\x02\x09"
    text = T.decode(raw, EU)
    assert "{i:200}" in text and "{g:150}" in text and "{raw:" in text and "{x}" in text
    assert T.encode(text, EU) == raw


# --------------------------------------------------------------------------
# containers

def test_table_and_archive_roundtrip():
    strings = [m("One"), b"", m("Two\nlines{pg}x")]
    table = T.build_table(strings)
    c = T.parse_container(table)
    assert c.kind == "table" and c.tables == [strings]
    assert T.build_container(c) == table

    arch = T.build_container(T.Container("archive", [strings, [m("only")]]))
    c2 = T.parse_container(arch)
    assert c2.kind == "archive" and len(c2.tables) == 2 and c2.tables[1] == [m("only")]
    assert [(t, i) for t, i, _ in c2.strings()] == [(0, 0), (0, 1), (0, 2), (1, 0)]


def test_garbage_is_not_a_container():
    with pytest.raises(T.TextFormatError):
        T.parse_container(b"\x05\x00\x00\x00" + b"x" * 40)
    with pytest.raises(T.TextFormatError):
        T.parse_container(b"")


def test_string_scanner_follows_control_code_lengths():
    # 02 05 00 is 3 bytes: the 0 inside a code argument is not the end of the string
    data = bytes((40, 2, 5, 0, 41, 0, 99))
    assert T.scan_string(data, 0) == 5
    with pytest.raises(T.TextFormatError):
        T.scan_string(bytes((40, 2)), 0)
    with pytest.raises(T.TextFormatError):
        T.scan_string(bytes((40, 2, 99, 0)), 0)  # unknown control code


# --------------------------------------------------------------------------
# widths and lines

def test_widths_and_pages():
    mt = flat_metrics()
    raw = m("ab cd\nxyz{pg}q")
    assert T.line_widths(raw, mt) == [5 + 5 + 3 + 5 + 5, 15, 5]
    assert T.page_lines(raw) == [2, 1]
    assert T.page_lines(m("one")) == [1]
    assert T.line_widths(m("a{buf:1}"), mt) == [5]  # a work-buffer insert has no width of its own


def test_load_metrics_picks_the_version_branch(tmp_path):
    # the array length is only checked for the icons, so a short glyph array is fine here
    tiny = FAKE_SOURCE + FAKE_SOURCE.replace("_1[", "_2[") + FAKE_SOURCE.replace("_1[", "_3[")
    tiny = tiny.replace("FONT_ICONS_1[2]", "FONT_ICONS_1[2]")
    decomp = write_fake_decomp(tmp_path, tiny)
    eu = T.load_metrics(decomp, "eu")[1]
    us = T.load_metrics(decomp, "us")[1]
    assert eu.glyphs == [8, 5, 8] and us.glyphs == [7, 8]
    assert eu.icons == [9, 10] and us.icons == [4, 5]
    with pytest.raises(OSError):
        T.load_metrics(tmp_path / "missing", "eu")


@pytest.mark.skipif(not (ROOT / "vendor/dw3_decomp/src/engine/text/text_tools.c").exists(), reason="decomp submodule not checked out")
def test_real_decomp_tables_parse():
    for ver in ("eu", "us"):
        mt = T.load_metrics(ROOT / "vendor" / "dw3_decomp", ver)
        assert len(mt[1].icons) == 114 and len(mt[1].glyphs) == 230


# --------------------------------------------------------------------------
# extraction on a synthetic runtime

def make_runtime(root: Path):
    """runtime/eu with the five language rows of two text files; the files are made up."""
    eu = root / "eu"
    files = {}
    n = 1
    plan = {
        "ENG": {"ESSYSTEM": [m("Yes"), m("No\nLong second line"), b""], "ESDMG200": None},
        "FRA": {"FSSYSTEM": [m("Oui"), m("Non\nLa deuxième ligne est plus longue"), b""]},
        "ITA": {"ISSYSTEM": [m("Sì"), m("No"), b""]},
        "GER": {"DSSYSTEM": [m("Ja"), m("Nein"), b""]},
        "SPN": {"SSSYSTEM": [m("Sí"), m("No"), b""]},
    }
    dmg = T.build_container(T.Container("archive", [[m("Hi {name:0}"), m("Wait{wait:2}{pg}Go")], [m("Second table")]]))
    for lang, items in plan.items():
        (eu / "AAA" / "DAT" / "COUNTRY" / lang).mkdir(parents=True, exist_ok=True)
        for name, strings in items.items():
            data = dmg if strings is None else T.build_table(strings)
            if strings is None:
                name = "ESDMG200"
            path = f"AAA/DAT/COUNTRY/{lang}/{name}.BIN"
            (eu / path).write_bytes(data)
            files[str(n)] = {"path": path, "lba": n, "sectors": 1, "bytes": len(data), "form": 1}
            n += 1
    (eu / "files_eu.json").write_text(json.dumps({"version": "eu", "executable": "x", "files": files}))
    return root


def test_extract_builds_the_template(tmp_path):
    rt = make_runtime(tmp_path / "rt")
    mt = {1: flat_metrics(), 2: flat_metrics(), 3: flat_metrics()}
    tpl = X.build_template("eu", rt, mt, refs=("fr",))
    assert sorted(tpl) == ["DMG200", "SYSTEM"]
    sysf = tpl["SYSTEM"]
    assert sysf["category"] == "menus" and sysf["container"] == "table" and sysf["file_id"] == 1
    e0, e1, e2 = sysf["entries"]
    assert (e0["key"], e0["en"], e0["pt"], e0["fr"]) == ("SYSTEM:0", "Yes", "", "Oui")
    # the limit is the widest of the five languages: the French second line is the longest
    assert e1["en"] == "No\nLong second line" and e1["limit_px"] > T.line_widths(m("No\nLong second line"), mt[1])[1]
    assert e1["max_lines"] == 1 or e1["max_lines"] == 2
    assert e2["en"] == ""
    dmg = tpl["DMG200"]
    assert dmg["container"] == "archive" and dmg["category"] == "dialogue_event"
    assert [e["key"] for e in dmg["entries"]] == ["DMG200:0:0", "DMG200:0:1", "DMG200:1:0"]
    assert dmg["entries"][0]["en"] == "Hi {name:0}"

    st = X.stats(tpl)
    assert st["total"]["strings"] == 6 and st["total"]["nonempty"] == 5
    assert st["categories"]["menus"]["words"] == 5  # Yes, No, Long, second, line
    assert "TOTAL" in X.format_stats(st, "eu")


def test_stem_and_category_names():
    assert X.stem_of("AAA/DAT/COUNTRY/ENG/ESITMNAM.BIN") == "ITMNAM"
    assert X.stem_of("AAA/DAT/COUNTRY/USA/USTALK03.BIN") == "TALK03"
    assert X.stem_of("AAA/DAT/COUNTRY/GER/DSDMG305.BIN") == "DMG305"
    assert X.stem_of("AAA/DAT/COUNTRY/SPN/SSITMNAM.BIN") == "ITMNAM"  # the Spanish prefix is SS
    assert X.category_of("DIGNAM") == "names" and X.category_of("TALK04") == "dialogue_talk"
    assert X.category_of("XYZ") == "other"


def test_cli_writes_outside_git_and_refuses_tracked_paths(tmp_path, capsys):
    rt = make_runtime(tmp_path / "rt")
    decomp = write_fake_decomp(tmp_path / "decomp")
    out = tmp_path / "out"
    assert X.main(["--runtime", str(rt), "--region", "eu", "--out", str(out), "--decomp", str(decomp)]) == 0
    assert (out / "eu" / "SYSTEM.json").exists() and (out / "eu" / "_index.json").exists()
    doc = json.loads((out / "eu" / "SYSTEM.json").read_text(encoding="utf-8"))
    assert doc["format"] == X.FORMAT and doc["entries"][0]["pt"] == ""
    # a path inside the repository that git does not ignore is refused
    assert X.main(["--runtime", str(rt), "--region", "eu", "--out", str(ROOT / "docs" / "l10n_test"), "--decomp", str(decomp)]) == 2
    assert not (ROOT / "docs" / "l10n_test").exists()
    # tmp/ is ignored
    assert X.is_git_ignored(ROOT / "tmp" / "l10n" / "ptbr")
    # no runtime for a region: skipped, not a crash
    assert X.main(["--runtime", str(rt), "--region", "us", "--stats", "--decomp", str(decomp)]) == 1


# --------------------------------------------------------------------------
# --check

def template(tmp_path: Path, entries: list[dict], **extra) -> Path:
    d = {"format": X.FORMAT, "region": "eu", "stem": "TALK00", "file_id": 199, "container": "table",
         "category": "dialogue_talk", "entries": entries}
    d.update(extra)
    p = tmp_path / "TALK00.json"
    p.write_text(json.dumps(d, ensure_ascii=False), encoding="utf-8")
    return p


def run_check(path, tmp_path, **kw):
    decomp = write_fake_decomp(tmp_path / "decomp") if not (tmp_path / "decomp").exists() else tmp_path / "decomp"
    return X.check_paths([path], strict=kw.pop("strict", False), decomp=decomp, max_width=kw.pop("max_width", None),
                         glossary=kw.pop("glossary", None), against=kw.pop("against", None))


def entry(key="TALK00:0", en="Hello {name:1}!", pt="", **kw):
    e = {"key": key, "en": en, "pt": pt}
    e.update(kw)
    return e


def test_check_accepts_a_good_translation_and_counts_untranslated(tmp_path):
    p = template(tmp_path, [entry(pt="Olá {name:1}!"), entry("TALK00:1", "Bye", ""), entry("TALK00:2", "", "")])
    rep = run_check(p, tmp_path)
    assert rep.errors == [] and rep.counts["translated"] == 1 and rep.counts["untranslated"] == 2


def test_check_errors(tmp_path):
    cases = [
        (entry(pt="中文"), "not in the EU font"),
        (entry(pt="Olá!"), "control codes differ"),  # lost the {name:1}
        (entry(en="A{pg}B", pt="A B"), "control codes differ"),
        (entry(en="a\nb", pt="a\nb\nc\nd", max_lines=3), "lines on a page"),
        (entry(pt="Olá {name:1}!", **{"key": "TALK00:0"}), None),
    ]
    for e, msg in cases:
        rep = run_check(template(tmp_path, [e]), tmp_path)
        if msg is None:
            assert rep.errors == []
        else:
            assert any(msg in x for x in rep.errors), (msg, rep.errors)
    # a pure-dialogue category defaults to 3 lines when the entry has no max_lines
    rep = run_check(template(tmp_path, [entry(en="a", pt="a\nb\nc\nd")]), tmp_path)
    assert any("lines on a page" in x for x in rep.errors)
    # duplicate keys and a wrong file format
    rep = run_check(template(tmp_path, [entry(), entry()]), tmp_path)
    assert any("duplicate key" in x for x in rep.errors)
    bad = tmp_path / "bad.json"
    bad.write_text("{}")
    assert run_check(bad, tmp_path).errors


def test_check_width_limits(tmp_path):
    # with the fake EU metrics a letter is 7 px, so ten of them are 70 px
    long = "AAAAAAAAAA"
    p = template(tmp_path, [entry(en="A", pt=long, limit_px=30)])
    rep = run_check(p, tmp_path)
    assert rep.errors == [] and any("wider than every other EU language" in x for x in rep.warnings)
    rep = run_check(p, tmp_path, strict=True)
    assert rep.errors  # strict promotes the warning
    rep = run_check(template(tmp_path, [entry(en="A", pt=long)]), tmp_path, max_width=20)
    assert any("limit 20 px" in x for x in rep.errors)


def test_check_against_a_fresh_template(tmp_path):
    fresh = tmp_path / "fresh"
    fresh.mkdir()
    (fresh / "TALK00.json").write_text(json.dumps({
        "format": X.FORMAT, "region": "eu", "stem": "TALK00", "entries": [entry("TALK00:0", "A"), entry("TALK00:1", "B")]}), encoding="utf-8")
    mine = tmp_path / "mine"
    mine.mkdir()
    p = template(mine, [entry("TALK00:0", "A changed", "x"), entry("TALK00:9", "Z", "z")])
    rep = run_check(p, tmp_path, against=fresh)
    joined = "\n".join(rep.errors)
    assert "English changed" in joined and "missing key TALK00:1" in joined and "unknown key TALK00:9" in joined


def test_check_glossary(tmp_path):
    gl = tmp_path / "glossary.json"
    gl.write_text(json.dumps({"_comment": "x", "Agumon": "Agumon", "Digi-Egg": "Digiovo"}))
    p = template(tmp_path, [entry(en="Take the Digi-Egg.", pt="Pegue o ovo.")])
    rep = run_check(p, tmp_path, glossary=gl)
    assert rep.errors == [] and any("Digi-Egg" in x for x in rep.warnings)


def test_check_cli_exit_codes(tmp_path, capsys):
    decomp = write_fake_decomp(tmp_path / "decomp")
    ok = template(tmp_path, [entry(pt="Oi {name:1}!")])
    assert X.main(["--check", str(ok), "--decomp", str(decomp)]) == 0
    assert "1 translated" in capsys.readouterr().out
    bad = template(tmp_path, [entry(pt="中")])
    assert X.main(["--check", str(bad), "--decomp", str(decomp)]) == 1
    assert X.main(["--check", str(tmp_path / "nothing"), "--decomp", str(decomp)]) == 1
    # --strict fails while something is untranslated
    open_ = template(tmp_path, [entry(pt="")])
    assert X.main(["--check", str(open_), "--decomp", str(decomp), "--strict"]) == 1


def test_propagate_copies_to_identical_english(tmp_path):
    p = template(tmp_path, [entry("TALK00:0", "Same", "Igual"), entry("TALK00:1", "Same", ""), entry("TALK00:2", "Other", "")])
    assert X.propagate([tmp_path]) == 1
    doc = json.loads(p.read_text(encoding="utf-8"))
    assert [e["pt"] for e in doc["entries"]] == ["Igual", "Igual", ""]


# --------------------------------------------------------------------------
# the repository never holds extracted text

def test_no_template_or_pack_is_tracked():
    """The template and the pack contain the game's text: nothing carrying the template format
    marker may be tracked, and tmp/ (where the tool writes by default) is git-ignored."""
    import subprocess

    tracked = subprocess.run(["git", "ls-files", "*.json", "*.bin"], cwd=ROOT, capture_output=True, text=True)
    if tracked.returncode != 0:
        pytest.skip("not a git checkout")
    for name in tracked.stdout.split():
        p = ROOT / name
        if p.suffix == ".json" and p.stat().st_size < 5_000_000:
            head = p.read_text(encoding="utf-8", errors="ignore")[:400]
            assert X.FORMAT not in head, f"{name} looks like an extracted template"
    assert X.is_git_ignored(ROOT / "tmp" / "l10n" / "ptbr" / "eu")
