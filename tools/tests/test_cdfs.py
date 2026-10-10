"""Wave 1 tests for port/cdfs via the tools/cdfs_cat host CLI.

No real disc: uses tools/testdata/cdfs_mini/ only.
Owner runtime/us spot-checks are skipped unless that tree exists.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
MINI = ROOT / "tools" / "testdata" / "cdfs_mini"
MAP = MINI / "files_mini.json"
CDFS_C = ROOT / "port" / "cdfs" / "cdfs.c"
CDFS_H = ROOT / "port" / "cdfs" / "cdfs.h"
CAT_C = ROOT / "tools" / "cdfs_cat.c"

EXPECTED = {
    1: b"alpha-bytes",
    2: b"beta-bytes!!!!!!",
    10: b"gamma!!",
}


@pytest.fixture(scope="module")
def cdfs_cat(tmp_path_factory) -> Path:
    build_dir = tmp_path_factory.mktemp("cdfs_build")
    out = build_dir / "cdfs_cat"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-o",
        str(out),
        str(CAT_C),
        str(CDFS_C),
        f"-I{ROOT / 'port' / 'cdfs'}",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    assert out.is_file()
    assert CDFS_H.is_file()
    return out


def _run_cat(cat: Path, *args: str, check: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(
        [str(cat), *args],
        capture_output=True,
        check=check,
    )


def test_reads_are_byte_identical(cdfs_cat: Path):
    for file_id, expect in EXPECTED.items():
        r = _run_cat(
            cdfs_cat,
            "--root",
            str(MINI),
            "--map",
            str(MAP),
            str(file_id),
        )
        assert r.stdout == expect, f"id {file_id}"


def test_file_size(cdfs_cat: Path):
    for file_id, expect in EXPECTED.items():
        r = _run_cat(
            cdfs_cat,
            "--root",
            str(MINI),
            "--map",
            str(MAP),
            "--size",
            str(file_id),
        )
        assert r.stdout.decode().strip() == str(len(expect))


def test_default_map_files_us_json(cdfs_cat: Path, tmp_path: Path):
    root = tmp_path / "runtime_us"
    shutil.copytree(MINI, root)
    (root / "files_us.json").write_text(
        (root / "files_mini.json").read_text(encoding="utf-8"),
        encoding="utf-8",
    )
    r = _run_cat(cdfs_cat, "--root", str(root), "1")
    assert r.stdout == EXPECTED[1]


def test_unknown_id_fails(cdfs_cat: Path):
    r = _run_cat(
        cdfs_cat,
        "--root",
        str(MINI),
        "--map",
        str(MAP),
        "9999",
        check=False,
    )
    assert r.returncode != 0


def test_python_map_layout_matches_contract():
    """Fixture JSON shape matches build_file_map.py / pack_runtime output."""
    import json

    data = json.loads(MAP.read_text(encoding="utf-8"))
    assert data["version"] == "us"
    assert "files" in data
    for key, meta in data["files"].items():
        assert key.isdigit()
        rel = Path(meta["path"])
        assert not rel.is_absolute()
        host = MINI / rel
        assert host.is_file(), host
        assert host.stat().st_size == meta["bytes"]
        assert host.read_bytes() == EXPECTED[int(key)]


@pytest.mark.skipif(
    not (ROOT / "runtime" / "us" / "files_us.json").is_file(),
    reason="owner runtime/us not present (CI / no disc)",
)
def test_owner_runtime_sample_ids(cdfs_cat: Path):
    """Optional: read a few ids from a local packed tree (never in CI)."""
    runtime = ROOT / "runtime" / "us"
    # Prefer low ids that exist in the map; skip if missing on disk.
    import json

    data = json.loads((runtime / "files_us.json").read_text(encoding="utf-8"))
    samples = []
    for key in sorted(data["files"], key=int):
        meta = data["files"][key]
        path = runtime / meta["path"]
        if path.is_file() and path.stat().st_size > 0:
            samples.append((int(key), path))
        if len(samples) >= 3:
            break
    assert samples, "runtime/us map has no readable sample files"
    for file_id, path in samples:
        r = _run_cat(cdfs_cat, "--root", str(runtime), str(file_id))
        assert r.stdout == path.read_bytes()
