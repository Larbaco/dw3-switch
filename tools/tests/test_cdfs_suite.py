"""Formal P2.5 / P4.5 CDFS suite via tools/cdfs_suite.

Always runs the self-test (mini fixture + path reject). When a packed
runtime root is present — PC `runtime/us`, env `DW3_CDFS_ROOT`, or env
`DW3_SD_RUNTIME` (mounted/copied SD tree at …/switch/dw3/us) — also runs
the packed-root suite (map load, file-id reads, read_at, LBA lookup).
"""

from __future__ import annotations

import os
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
CDFS_C = ROOT / "port" / "cdfs" / "cdfs.c"
SUITE_C = ROOT / "tools" / "cdfs_suite.c"


def _packed_roots() -> list[tuple[str, Path]]:
    found: list[tuple[str, Path]] = []
    seen: set[Path] = set()

    def add(label: str, path: Path) -> None:
        try:
            resolved = path.resolve()
        except OSError:
            return
        if resolved in seen:
            return
        if (path / "files_us.json").is_file() or (path / "files_eu.json").is_file():
            seen.add(resolved)
            found.append((label, path))

    add("runtime/us", ROOT / "runtime" / "us")
    add("runtime/eu", ROOT / "runtime" / "eu")
    for env_name in ("DW3_CDFS_ROOT", "DW3_SD_RUNTIME"):
        raw = os.environ.get(env_name, "").strip()
        if raw:
            add(env_name, Path(raw))
    return found


@pytest.fixture(scope="module")
def cdfs_suite(tmp_path_factory) -> Path:
    build_dir = tmp_path_factory.mktemp("cdfs_suite_build")
    out = build_dir / "cdfs_suite"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-o",
        str(out),
        str(SUITE_C),
        str(CDFS_C),
        f"-I{ROOT / 'port' / 'cdfs'}",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    assert out.is_file()
    return out


def test_self_test_fixture_and_path_reject(cdfs_suite: Path, tmp_path: Path):
    work = tmp_path / "self"
    work.mkdir()
    r = subprocess.run(
        [str(cdfs_suite), "--self-test", str(work)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK  map load rejects ../ escape" in r.stdout
    assert "OK  map load rejects absolute path" in r.stdout
    assert "OK  cdfs_load_map" in r.stdout
    assert "OK  cdfs_read id=1" in r.stdout
    assert "OK  cdfs_read_at" in r.stdout
    assert "OK  cdfs_lookup_lba" in r.stdout
    assert "FAIL" not in r.stdout
    assert "summary:" in r.stdout and ", 0 fail" in r.stdout


@pytest.mark.parametrize(
    "label,root",
    _packed_roots()
    or [
        pytest.param(
            "no-packed-root",
            Path("."),
            marks=pytest.mark.skip(
                reason="no packed runtime (runtime/us or DW3_CDFS_ROOT / DW3_SD_RUNTIME)"
            ),
        )
    ],
)
def test_packed_runtime_suite(cdfs_suite: Path, label: str, root: Path):
    """Map load + file-id / read_at / LBA against a P1.6 packed tree."""
    r = subprocess.run(
        [
            str(cdfs_suite),
            "--root",
            str(root),
            "--samples",
            "8",
            "--max-full",
            str(1024 * 1024),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, f"{label}: {r.stderr}{r.stdout}"
    assert "OK  cdfs_load_map" in r.stdout
    assert "OK  cdfs_read id=" in r.stdout
    assert "OK  cdfs_read_at" in r.stdout
    assert "OK  cdfs_lookup_lba" in r.stdout
    assert "OK  unknown id rejected" in r.stdout
    assert "FAIL" not in r.stdout


@pytest.fixture(scope="module")
def cdfs_suite_lib(tmp_path_factory) -> Path:
    """tools/cdfs_suite.c in library mode (DW3_CDFS_SUITE_LIB) linked with
    tools/testdata/cdfs_suite_lib_main.c — the same compile the Switch .nro
    runner would link against (docs/P4.5_SD_SUITE.md)."""
    build_dir = tmp_path_factory.mktemp("cdfs_suite_lib_build")
    out = build_dir / "cdfs_suite_lib"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-o",
        str(out),
        str(SUITE_C),
        str(CDFS_C),
        str(ROOT / "tools" / "testdata" / "cdfs_suite_lib_main.c"),
        f"-I{ROOT / 'port' / 'cdfs'}",
        "-DDW3_CDFS_SUITE_LIB=1",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    assert out.is_file()
    return out


def test_lib_mode_matches_cli(
    cdfs_suite: Path, cdfs_suite_lib: Path, tmp_path: Path
):
    """The library entry must produce exactly the standalone CLI's result."""
    r_cli = subprocess.run(
        [str(cdfs_suite), "--self-test", str(tmp_path / "cli")],
        capture_output=True,
        text=True,
        check=False,
    )
    r_lib = subprocess.run(
        [str(cdfs_suite_lib), "--self-test", str(tmp_path / "lib")],
        capture_output=True,
        text=True,
        check=False,
    )
    assert r_cli.returncode == 0, r_cli.stdout + r_cli.stderr
    assert r_lib.returncode == r_cli.returncode
    assert r_lib.stdout == r_cli.stdout


def test_lib_mode_reusable_in_one_process(cdfs_suite_lib: Path, tmp_path: Path):
    """The in-.nro runner calls the suite twice in one process (self-test,
    then a packed root). Per-call counters must not accumulate across calls."""
    r = subprocess.run(
        [str(cdfs_suite_lib), "--lib-twice", "--self-test", str(tmp_path / "twice")],
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stdout + r.stderr
    assert "lib-twice rc=0,0" in r.stdout
