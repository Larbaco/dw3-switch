"""P6.7 phase 1: tools/l10n/build_pack.py on synthetic text files only (no game data), and the
guard that no template or pack of game text is tracked by git."""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "l10n"))
sys.path.insert(0, str(ROOT / "tools" / "tests"))

import build_pack as B  # noqa: E402
import extract_text as X  # noqa: E402
import textfmt as T  # noqa: E402
from test_l10n_extract import flat_metrics, make_runtime, write_fake_decomp  # noqa: E402

EU = T.codec_for("eu")


def m(text: str) -> bytes:
    return T.encode(text, EU)


@pytest.fixture()
def rt(tmp_path):
    return make_runtime(tmp_path / "rt")


@pytest.fixture()
def tpls(rt):
    mt = {1: flat_metrics(), 2: flat_metrics(), 3: flat_metrics()}
    return X.build_template("eu", rt, mt)


def test_builder_translates_and_falls_back(rt, tpls, tmp_path):
    sysf = tpls["SYSTEM"]
    sysf["entries"][0]["pt"] = "Sim"
    sysf["entries"][1]["pt"] = "Não\nSegunda linha, bem mais longa"
    dmg = tpls["DMG200"]
    dmg["entries"][1]["pt"] = "Espere{wait:2}{pg}Vá"  # table 0, string 1
    out = tmp_path / "pack"
    pack = B.build_pack(tpls, rt, "eu", "ptbr", out)

    assert pack["format"] == B.PACK_FORMAT and pack["language"] == "ptbr" and pack["region"] == "eu"
    by = {f["stem"]: f for f in pack["files"]}
    assert set(by) == {"SYSTEM", "DMG200"}
    # SYSTEM: 3 strings (one is empty), two translated, the empty one falls back
    s = by["SYSTEM"]
    assert (s["strings"], s["translated"], s["fallback"]) == (3, 2, 1)
    data = (out / s["file"]).read_bytes()
    assert len(data) == s["bytes"] and s["sectors"] == 1
    c = T.parse_container(data)
    assert [T.decode(x, EU) for x in c.tables[0]] == ["Sim", "Não\nSegunda linha, bem mais longa", ""]
    # the archive keeps its two tables; the untouched strings keep the original bytes
    d = T.parse_container((out / by["DMG200"]["file"]).read_bytes())
    assert d.kind == "archive" and len(d.tables) == 2
    assert T.decode(d.tables[0][0], EU) == "Hi {name:0}"
    assert T.decode(d.tables[0][1], EU) == "Espere{wait:2}{pg}Vá"
    assert T.decode(d.tables[1][0], EU) == "Second table"
    assert pack["totals"] == {"files": 2, "strings": 6, "translated": 3, "fallback": 3}


def test_pack_json_records_source_hashes_and_is_scannable(rt, tpls, tmp_path):
    out = tmp_path / "pack"
    pack = B.build_pack(tpls, rt, "eu", "ptbr", out)
    doc = json.loads((out / "pack.json").read_text())
    assert doc == pack
    ids = X.file_ids(rt, "eu", "ENG")
    for f in doc["files"]:
        src = (rt / "eu" / next(p for p in [f"AAA/DAT/COUNTRY/ENG/ES{f['stem']}.BIN"])).read_bytes()
        assert f["src_sha256"] == B.sha256(src) and f["src_bytes"] == len(src)
        assert f["id"] == ids[f["stem"]]
        assert f["sha256"] == B.sha256((out / f["file"]).read_bytes())
    # the C loader reads one object per line: keep that shape
    lines = (out / "pack.json").read_text().splitlines()
    assert sum(1 for ln in lines if ln.lstrip().startswith('{"id"')) == len(doc["files"])


def test_build_is_deterministic(rt, tpls, tmp_path):
    a, b = tmp_path / "a", tmp_path / "b"
    B.build_pack(tpls, rt, "eu", "ptbr", a)
    B.build_pack(tpls, rt, "eu", "ptbr", b)
    for f in sorted(a.iterdir()):
        assert f.read_bytes() == (b / f.name).read_bytes()


def test_rebuild_removes_stale_files(rt, tpls, tmp_path):
    out = tmp_path / "pack"
    out.mkdir()
    (out / "999.bin").write_bytes(b"stale")
    B.build_pack(tpls, rt, "eu", "ptbr", out)
    assert not (out / "999.bin").exists()


def test_bad_inputs_are_refused(rt, tpls, tmp_path):
    out = tmp_path / "pack"
    bad = json.loads(json.dumps(tpls))
    bad["SYSTEM"]["entries"][0]["pt"] = "日本語"  # not in the EU font
    with pytest.raises(B.PackError, match="not in the EU font"):
        B.build_pack(bad, rt, "eu", "ptbr", out)

    bad = json.loads(json.dumps(tpls))
    bad["SYSTEM"]["entries"][0]["pt"] = "Sim"
    bad["SYSTEM"]["entries"][0]["en"] = "Another disc"  # the template is from another dump
    with pytest.raises(B.PackError, match="English changed"):
        B.build_pack(bad, rt, "eu", "ptbr", out)

    bad = json.loads(json.dumps(tpls))
    bad["SYSTEM"]["entries"].append({"key": "SYSTEM:99", "en": "x", "pt": "y"})
    with pytest.raises(B.PackError, match="no such string"):
        B.build_pack(bad, rt, "eu", "ptbr", out)

    bad = json.loads(json.dumps(tpls))
    del bad["DMG200"]
    with pytest.raises(B.PackError, match="no DMG200.json"):
        B.build_pack(bad, rt, "eu", "ptbr", out)

    bad = json.loads(json.dumps(tpls))
    bad["SYSTEM"]["entries"] += [dict(bad["SYSTEM"]["entries"][0])]
    with pytest.raises(B.PackError, match="duplicate key"):
        B.build_pack(bad, rt, "eu", "ptbr", out)

    with pytest.raises(B.PackError, match="language code"):
        B.build_pack(tpls, rt, "eu", "../x", out)
    with pytest.raises(B.PackError, match="no English text files"):
        B.build_pack(tpls, tmp_path / "empty", "eu", "ptbr", out)


def test_sector_count_rounds_up(rt, tpls, tmp_path):
    big = "x" * 3000  # a long line: the file grows past one sector
    tpls["SYSTEM"]["entries"][1]["pt"] = big
    pack = B.build_pack(tpls, rt, "eu", "ptbr", tmp_path / "pack")
    f = next(f for f in pack["files"] if f["stem"] == "SYSTEM")
    assert f["bytes"] > 2048 and f["sectors"] == -(-f["bytes"] // 2048) == 2


def test_verify_accepts_a_good_pack_and_catches_tampering(rt, tpls, tmp_path):
    out = tmp_path / "pack"
    B.build_pack(tpls, rt, "eu", "ptbr", out)
    assert B.verify_pack(out, rt, "eu") == []
    assert B.verify_pack(out, None, None) == []

    victim = next(out.glob("*.bin"))
    victim.write_bytes(victim.read_bytes() + b"\0")
    assert any("differs from pack.json" in p for p in B.verify_pack(out, rt, "eu"))

    assert B.verify_pack(tmp_path / "nothing", rt, "eu")  # no pack.json


def test_verify_notices_a_pack_from_another_dump(rt, tpls, tmp_path):
    out = tmp_path / "pack"
    B.build_pack(tpls, rt, "eu", "ptbr", out)
    eng = rt / "eu" / "AAA" / "DAT" / "COUNTRY" / "ENG" / "ESSYSTEM.BIN"
    eng.write_bytes(T.build_table([m("Different"), m("Other"), b""]))
    problems = B.verify_pack(out, rt, "eu")
    assert any("not the one the pack was built from" in p for p in problems)


def test_verify_notices_changed_control_codes(rt, tpls, tmp_path):
    out = tmp_path / "pack"
    pack = B.build_pack(tpls, rt, "eu", "ptbr", out)
    f = next(f for f in pack["files"] if f["stem"] == "DMG200")
    cont = T.parse_container((out / f["file"]).read_bytes())
    cont.tables[0][1] = m("Wait")  # lost its {wait:2}{pg}
    data = T.build_container(cont)
    (out / f["file"]).write_bytes(data)
    doc = json.loads((out / "pack.json").read_text())
    for e in doc["files"]:
        if e["stem"] == "DMG200":
            e["bytes"], e["sha256"] = len(data), B.sha256(data)
    (out / "pack.json").write_text(json.dumps(doc))
    assert any("control codes differ" in p for p in B.verify_pack(out, rt, "eu"))


# --------------------------------------------------------------------------
# the synthetic transforms

def test_synth_upper_keeps_tokens_and_adds_accents():
    out = B.synth_upper("Want one? {name:1} can{wait:2}{pg}Once\nNice", EU)
    assert out == "WãNT õNE? {name:1} CãN{wait:2}{pg}õNçE\nNIçE"
    assert T.tokens(out) == ["{name:1}", "{wait:2}", "{pg}"]
    m(out)  # every character is writable with the EU font


def test_synth_upper_leaves_what_the_font_cannot_upper_case():
    # an icon that is not text and a lower-case letter whose capital is not in the font
    text = "ok {i:200} é ß"
    out = B.synth_upper(text, EU)
    assert "{i:200}" in out
    m(out)


def test_synth_stress_pads_each_line_to_the_limit():
    mt = flat_metrics()  # 5 px per glyph, 3 px per space
    text = "Hi {buf:1}\nYes{wait:1}"
    out = B.synth_stress(text, 40, EU, mt)
    lines = out.split("\n")
    assert T.tokens(out) == ["{buf:1}", "{wait:1}"]
    for line in lines:
        w = max(T.line_widths(m(line), mt))
        assert 40 - 8 < w <= 40, (line, w)
    # no limit, no change; a line already over the limit is left alone
    assert B.synth_stress(text, None, EU, mt) == text
    assert B.synth_stress("A very long line indeed", 10, EU, mt) == "A very long line indeed"


def test_synthetic_templates_build_a_pack(rt, tmp_path, monkeypatch):
    decomp = write_fake_decomp(tmp_path / "decomp")
    tp = B.synthetic_templates("upper", "eu", rt, decomp)
    assert tp["SYSTEM"]["entries"][0]["pt"] == "YES"
    assert tp["SYSTEM"]["entries"][2]["pt"] == ""  # empty strings stay empty
    pack = B.build_pack(tp, rt, "eu", "ptbr", tmp_path / "pack", "upper")
    assert pack["synthetic"] == "upper"
    assert B.verify_pack(tmp_path / "pack", rt, "eu") == []
    st = B.synthetic_templates("stress", "eu", rt, decomp)
    B.build_pack(st, rt, "eu", "ptbr", tmp_path / "pack2", "stress")
    with pytest.raises(B.PackError):
        B.synthetic_templates("lower", "eu", rt, decomp)


# --------------------------------------------------------------------------
# the command line

def test_cli_refuses_to_write_inside_the_repository(tmp_path):
    out = ROOT / "tools" / "l10n" / "not_ignored_pack"
    rc = B.main(["--synthetic", "upper", "--runtime", str(tmp_path), "--out", str(out)])
    assert rc == 2 and not out.exists()


def test_cli_needs_exactly_one_source(tmp_path):
    with pytest.raises(SystemExit):
        B.main(["--out", str(tmp_path / "p")])
    with pytest.raises(SystemExit):
        B.main(["--out", str(tmp_path / "p"), "--templates", str(tmp_path), "--synthetic", "upper"])


def test_cli_builds_from_a_template_folder(rt, tpls, tmp_path, capsys):
    tdir = tmp_path / "tpl"
    X.write_templates(tdir, "eu", tpls, X.stats(tpls))
    tpls["SYSTEM"]["entries"][0]["pt"] = "Sim"
    (tdir / "eu" / "SYSTEM.json").write_text(json.dumps(tpls["SYSTEM"], ensure_ascii=False))
    out = tmp_path / "out"
    assert B.main(["--templates", str(tdir / "eu"), "--runtime", str(rt), "--out", str(out)]) == 0
    assert "1 translated" in capsys.readouterr().out
    assert B.main(["--verify", str(out), "--runtime", str(rt)]) == 0
    # a folder with no templates
    assert B.main(["--templates", str(tmp_path), "--runtime", str(rt), "--out", str(tmp_path / "o2")]) == 1


# --------------------------------------------------------------------------
# no game text in git

def _tracked(pattern: str) -> list[str]:
    out = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, capture_output=True, check=True).stdout
    return [p for p in out.decode().split("\0") if p and re.search(pattern, p)]


def test_no_template_or_pack_of_game_text_is_tracked():
    """The template and the pack carry the game's text: never in git (docs/LOCALIZATION.md section 1)."""
    marker = re.compile(rb'"format"\s*:\s*"dw3-l10n(-pack)?-v1"')
    for path in _tracked(r"\.(json|bin)$"):
        f = ROOT / path
        if f.is_file() and f.stat().st_size < 50_000_000:
            assert not marker.search(f.read_bytes()), f"{path} looks like a template or a pack of game text"
    assert not _tracked(r"(^|/)l10n/[a-z0-9]+/[0-9]+\.bin$")
    assert not _tracked(r"(^|/)tmp/l10n/")
