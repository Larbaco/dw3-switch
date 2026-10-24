"""tools/pack_release_elfs.py: ELF + NRO build-id check and the release zip."""

from __future__ import annotations

import subprocess
import sys
import zipfile
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import symbolize_crash as sc  # noqa: E402

SCRIPT = ROOT / "tools" / "pack_release_elfs.py"


def _elf(d: Path, name: str, value: int) -> Path:
    src = d / f"{name}.c"
    src.write_text(f"int main(void) {{ return {value}; }}\n")
    out = d / f"{name}.elf"
    r = subprocess.run(["gcc", "-Wl,--build-id=sha1", "-o", str(out), str(src)],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    return out


def _nro(path: Path, build_id: str) -> Path:
    head = bytearray(0x100)
    head[0x10:0x14] = b"NRO0"
    mid = bytes.fromhex(build_id)
    head[0x40:0x40 + len(mid)] = mid
    path.write_bytes(bytes(head))
    return path


def _run(out: Path, *items: tuple) -> subprocess.CompletedProcess:
    cmd = [sys.executable, str(SCRIPT), "--out", str(out), "--tag", "vT"]
    for it in items:
        cmd += ["--item", *map(str, it)]
    return subprocess.run(cmd, capture_output=True, text=True)


def test_pack_matching_pair(tmp_path):
    a, b = _elf(tmp_path, "a", 1), _elf(tmp_path, "b", 2)
    ida, idb = sc.elf_build_id(a), sc.elf_build_id(b)
    assert ida != idb
    na, nb = _nro(tmp_path / "a.nro", ida), _nro(tmp_path / "b.nro", idb)
    out = tmp_path / "dist" / "elf.zip"
    r = _run(out, (na, a, "us.elf"), (nb, b, "us-nxlink.elf"))
    assert r.returncode == 0, r.stderr
    with zipfile.ZipFile(out) as z:
        assert sorted(z.namelist()) == ["BUILD_IDS.txt", "us-nxlink.elf", "us.elf"]
        ids = z.read("BUILD_IDS.txt").decode()
        assert f"{ida}  us.elf" in ids and "a.nro" in ids
        assert f"{idb}  us-nxlink.elf" in ids and "b.nro" in ids
        assert z.read("us.elf") == a.read_bytes()  # nothing stripped


def test_pack_rejects_wrong_elf(tmp_path):
    a, b = _elf(tmp_path, "a", 1), _elf(tmp_path, "b", 2)
    na = _nro(tmp_path / "a.nro", sc.elf_build_id(a))
    out = tmp_path / "elf.zip"
    r = _run(out, (na, b, "wrong.elf"))
    assert r.returncode == 1
    assert "!=" in r.stderr
    assert not out.exists()


def test_pack_rejects_non_nro(tmp_path):
    a = _elf(tmp_path, "a", 1)
    bad = tmp_path / "bad.nro"
    bad.write_bytes(b"\0" * 0x100)
    r = _run(tmp_path / "elf.zip", (bad, a, "a.elf"))
    assert r.returncode != 0
