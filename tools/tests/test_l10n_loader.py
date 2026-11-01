"""P6.7 phase 1: the text pack loader (port/l10n/l10n_core.c, port/cdfs override) on a synthetic
runtime and pack: arrays patched and restored, cdfs reads follow the override, and every way a
pack can be wrong gives English with one `off reason=` line."""

from __future__ import annotations

import json
import shutil
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
from test_l10n_extract import flat_metrics, make_runtime  # noqa: E402


@pytest.fixture(scope="module")
def binary(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("l10n") / "l10n_test"
    cmd = ["gcc", "-std=gnu99", "-Wall", "-Wextra", "-Werror", "-O1", "-o", str(out),
           str(ROOT / "tools/tests/l10n_main.c"), str(ROOT / "port/l10n/l10n_core.c"),
           str(ROOT / "port/l10n/sha256.c"), str(ROOT / "port/cdfs/cdfs.c"),
           f"-I{ROOT / 'port/l10n'}", f"-I{ROOT / 'port/cdfs'}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


@pytest.fixture()
def world(tmp_path):
    """(runtime/eu path, templates) with a pack built from a made-up translation."""
    rt = make_runtime(tmp_path / "rt")
    eu = rt / "eu"
    mt = {1: flat_metrics(), 2: flat_metrics(), 3: flat_metrics()}
    tpls = X.build_template("eu", rt, mt)
    tpls["SYSTEM"]["entries"][0]["pt"] = "Sim"
    tpls["SYSTEM"]["entries"][1]["pt"] = "Não\nSegunda linha"
    tpls["DMG200"]["entries"][1]["pt"] = "Espere{wait:2}{pg}Vá"
    B.build_pack(tpls, rt, "eu", "ptbr", eu / "l10n" / "ptbr")
    return eu, tpls, rt


def run(binary, eu, scenario):
    r = subprocess.run([str(binary), str(eu), scenario], capture_output=True, text=True)
    logs = [ln[4:] for ln in r.stdout.splitlines() if ln.startswith("LOG ")]
    checks = {ln.split()[1]: ln.split()[2] for ln in r.stdout.splitlines() if ln.startswith("CHECK ")}
    reason = next((ln.split()[1] for ln in r.stdout.splitlines() if ln.startswith("REASON ")), "")
    return r.returncode, logs, checks, reason, r.stdout


def off_lines(logs):
    return [ln for ln in logs if ln.startswith("dw3-l10n: off reason=")]


def test_pack_applies_and_restores(binary, world):
    eu, _, _ = world
    rc, logs, checks, _, out = run(binary, eu, "ok")
    assert rc == 0, out
    assert all(v == "ok" for v in checks.values()) and len(checks) >= 13
    assert any(ln.startswith("dw3-l10n: pack ok region=eu files=2 strings=6 translated=3 fallback=2 src=sha256:") for ln in logs)
    assert any(ln.startswith("dw3-l10n: override id=1 stem=SYSTEM bytes=") and "lba=0x1000000 sectors=1" in ln for ln in logs)
    assert any(ln == "dw3-l10n: fallback key=DMG200:0:0 (empty pt)" for ln in logs)
    assert any(ln.startswith("dw3-l10n: fallback total=2 of 6") for ln in logs)
    assert any(ln.startswith("dw3-l10n: pack checked in ") for ln in logs)
    assert not off_lines(logs)


def test_apply_twice_is_idempotent(binary, world):
    rc, _, checks, _, out = run(binary, world[0], "twice")
    assert rc == 0 and checks["apply_again"] == "ok", out


def test_a_longer_file_gets_more_sectors(binary, world):
    eu, tpls, rt = world
    tpls["SYSTEM"]["entries"][1]["pt"] = "x" * 3000
    B.build_pack(tpls, rt, "eu", "ptbr", eu / "l10n" / "ptbr")
    rc, _, checks, _, out = run(binary, eu, "bigger")
    assert rc == 0 and checks["grew"] == "ok", out


def test_no_pack_is_english_with_one_line(binary, world):
    eu, _, _ = world
    shutil.rmtree(eu / "l10n")
    rc, logs, _, reason, out = run(binary, eu, "nopack")
    assert rc == 0, out
    assert reason == "no_pack" and off_lines(logs) == ["dw3-l10n: off reason=no_pack"]


def test_source_from_another_dump_is_refused(binary, world):
    eu, _, _ = world
    eng = eu / "AAA/DAT/COUNTRY/ENG/ESSYSTEM.BIN"
    data = bytearray(eng.read_bytes())
    data[-5] ^= 0x01  # same size, one bit different
    eng.write_bytes(bytes(data))
    rc, logs, _, reason, out = run(binary, eu, "hash")
    assert rc == 0, out
    assert reason == "hash_mismatch" and len(off_lines(logs)) == 1


def test_source_of_another_size_is_refused(binary, world):
    eu, _, _ = world
    eng = eu / "AAA/DAT/COUNTRY/ENG/ESSYSTEM.BIN"
    eng.write_bytes(eng.read_bytes() + b"\0\0\0\0")
    rc, _, _, reason, out = run(binary, eu, "hash2")
    assert rc == 0 and reason == "hash_mismatch", out


def test_short_or_missing_pack_file_is_refused(binary, world):
    eu, _, _ = world
    f = eu / "l10n/ptbr/1.bin"
    f.write_bytes(f.read_bytes()[:-8])
    rc, logs, _, reason, out = run(binary, eu, "short")
    assert rc == 0 and reason == "short_file" and len(off_lines(logs)) == 1, out
    f.unlink()
    rc, _, _, reason, out = run(binary, eu, "missing")
    assert rc == 0 and reason == "short_file", out


def test_longer_pack_file_than_declared_is_refused(binary, world):
    eu, _, _ = world
    f = eu / "l10n/ptbr/2.bin"
    f.write_bytes(f.read_bytes() + b"\0" * 4)
    rc, _, _, reason, out = run(binary, eu, "long")
    assert rc == 0 and reason == "bad_pack", out


@pytest.mark.parametrize("text", ["", "not json", "{", '{"format": "other"}', "[]",
                                  '{"format": "dw3-l10n-pack-v1", "region": "eu", "language": "ptbr", "files": []}'])
def test_garbage_pack_json_is_refused(binary, world, text):
    eu, _, _ = world
    (eu / "l10n/ptbr/pack.json").write_text(text)
    rc, logs, _, reason, out = run(binary, eu, "badjson")
    assert rc == 0 and reason == "bad_pack", out
    assert off_lines(logs) == ["dw3-l10n: off reason=bad_pack"]


def edit_pack(eu, fn):
    p = eu / "l10n/ptbr/pack.json"
    doc = json.loads(p.read_text())
    fn(doc)
    p.write_text(json.dumps(doc))


def test_pack_json_validation(binary, world):
    eu, _, _ = world
    good = (eu / "l10n/ptbr/pack.json").read_text()
    cases = {
        "sectors": lambda d: d["files"][0].__setitem__("sectors", 9),
        "sha": lambda d: d["files"][0].__setitem__("sha256", "xyz"),
        "dup": lambda d: d["files"][1].__setitem__("id", d["files"][0]["id"]),
        "stem": lambda d: d["files"][0].__setitem__("stem", "../x"),
        "zero": lambda d: d["files"][0].__setitem__("id", 0),
    }
    for name, fn in cases.items():
        (eu / "l10n/ptbr/pack.json").write_text(good)
        edit_pack(eu, fn)
        rc, _, _, reason, out = run(binary, eu, "badjson")
        assert rc == 0 and reason == "bad_pack", (name, out)


def test_pack_for_another_region_is_refused(binary, world):
    eu, _, _ = world
    edit_pack(eu, lambda d: d.__setitem__("region", "us"))
    rc, logs, _, reason, out = run(binary, eu, "region")
    assert rc == 0 and reason == "region_mismatch" and len(off_lines(logs)) == 1, out


def test_pack_for_a_file_id_the_disc_does_not_have(binary, world):
    eu, _, _ = world
    # the C test binds only two ids for this scenario: id 2 is out of range
    rc, _, _, reason, out = run(binary, eu, "badid")
    assert rc == 0 and reason == "bad_id", out


def test_pack_json_may_carry_unknown_keys(binary, world):
    eu, _, _ = world
    edit_pack(eu, lambda d: (d.__setitem__("tool", {"name": "x", "list": [1, 2, {"a": None}]}),
                             d["files"][0].__setitem__("extra", [1, "two"])))
    rc, _, checks, _, out = run(binary, eu, "ok")
    assert rc == 0 and checks["apply"] == "ok", out


def test_key_variants_are_read_when_asked(binary, world):
    eu, tpls, rt = world
    B.build_pack(tpls, rt, "eu", "ptbr", eu / "l10n" / "ptbr", with_keys=True)
    rc, logs, checks, _, out = run(binary, eu, "keys")
    assert rc == 0 and checks["keys_on"] == "ok" and checks["reads_variant"] == "ok", out
    assert any(" keys" in ln and ln.startswith("dw3-l10n: override id=1 ") for ln in logs)


def test_asking_for_keys_of_a_pack_without_them_plays_the_normal_text(binary, world):
    eu, _, _ = world  # built without --keys
    rc, logs, checks, _, out = run(binary, eu, "keysmissing")
    assert rc == 0 and checks["keys_ok"] == "ok" and checks["reads_variant"] == "ok", out
    assert any("show keys: this pack has no key variants" in ln for ln in logs)


def test_a_missing_key_variant_file_disables_keys(binary, world):
    eu, tpls, rt = world
    B.build_pack(tpls, rt, "eu", "ptbr", eu / "l10n" / "ptbr", with_keys=True)
    (eu / "l10n/ptbr/2.k.bin").unlink()
    rc, _, checks, _, out = run(binary, eu, "keysmissing")
    assert rc == 0 and checks["keys_on"] == "ok", out
