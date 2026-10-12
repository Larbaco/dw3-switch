"""Tests for port/memcard flat saves and legacy .mcr fixtures.

No DuckStation: uses tools/testdata/memcard/ and temp directories.
"""

from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "tools" / "testdata" / "memcard" / "empty_slot.mcr"
HOST_C = ROOT / "port" / "memcard" / "memcard_host.c"
HOST_H = ROOT / "port" / "memcard" / "memcard_host.h"
LIB_C = ROOT / "port" / "memcard" / "memcard_libmcrd.c"
RW_C = ROOT / "tools" / "memcard_rw.c"
FLAT_TEST_C = ROOT / "tools" / "memcard_flat_test.c"

CARD_SIZE = 131072
FLAT_SIZE = 0x8000
SECTOR_SIZE = 128
# Data area starts after the directory block (block 0 = 64 sectors).
TEST_SECTOR = 64
TEST_OFFSET = TEST_SECTOR * SECTOR_SIZE
TEST_PATTERN = bytes.fromhex("DEADC0DECAFEBABE0123456789ABCDEF")


@pytest.fixture(scope="module")
def memcard_rw(tmp_path_factory) -> Path:
    build_dir = tmp_path_factory.mktemp("memcard_build")
    out = build_dir / "memcard_rw"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-o",
        str(out),
        str(RW_C),
        str(HOST_C),
        f"-I{ROOT / 'port' / 'memcard'}",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    assert out.is_file()
    assert HOST_H.is_file()
    return out


@pytest.fixture(scope="module")
def memcard_flat_test(tmp_path_factory) -> Path:
    build_dir = tmp_path_factory.mktemp("memcard_flat_build")
    out = build_dir / "memcard_flat_test"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wno-stringop-truncation",
        "-O2",
        "-o",
        str(out),
        str(FLAT_TEST_C),
        str(LIB_C),
        str(HOST_C),
        f"-I{ROOT / 'port' / 'memcard'}",
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def _run(rw: Path, *args: str, check: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(
        [str(rw), *args],
        capture_output=True,
        check=check,
    )


def test_fixture_is_formatted_128kib():
    assert FIXTURE.is_file()
    data = FIXTURE.read_bytes()
    assert len(data) == CARD_SIZE
    assert data[0:2] == b"MC"
    assert data[SECTOR_SIZE] == 0xA0


def test_ensure_empty_port_ok(memcard_rw: Path, tmp_path: Path):
    card_dir = tmp_path / "cards"
    card_dir.mkdir()
    r = _run(memcard_rw, "--dir", str(card_dir), "ensure", "--slot", "0")
    assert r.returncode == 0
    # Empty port: no save yet, and no auto-created 128 KiB card.
    assert not (card_dir / "save0.bin").exists()
    assert not (card_dir / "memcard0.mcr").exists()

    r = _run(memcard_rw, "--dir", str(card_dir), "ensure", "--slot", "1")
    assert r.returncode == 0


def test_flat_write_read_roundtrip(memcard_rw: Path, tmp_path: Path):
    card_dir = tmp_path / "cards"
    card_dir.mkdir()
    payload = b"dw3-flat-save"
    r = _run(
        memcard_rw,
        "--dir",
        str(card_dir),
        "flat-write",
        "--slot",
        "0",
        "--offset",
        "16",
        "--hex",
        payload.hex(),
    )
    assert r.returncode == 0, r.stderr
    path = card_dir / "save0.bin"
    assert path.is_file()
    assert path.stat().st_size == FLAT_SIZE
    assert path.read_bytes()[16 : 16 + len(payload)] == payload

    r = _run(
        memcard_rw,
        "--dir",
        str(card_dir),
        "flat-read",
        "--slot",
        "0",
        "--offset",
        "16",
        "--len",
        str(len(payload)),
    )
    assert r.returncode == 0, r.stderr
    assert r.stdout == payload


def test_memcard_flat_api(memcard_flat_test: Path, tmp_path: Path):
    """format/create, MemCard* round-trip, one flush, .mcr migration."""
    card_dir = tmp_path / "flat"
    card_dir.mkdir()
    r = subprocess.run(
        [str(memcard_flat_test), str(card_dir)],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stderr + r.stdout
    assert "ok" in r.stdout


def test_legacy_sector_read_write_roundtrip(memcard_rw: Path, tmp_path: Path):
    card_dir = tmp_path / "cards"
    card_dir.mkdir()
    shutil.copy(FIXTURE, card_dir / "memcard0.mcr")

    hex_pat = TEST_PATTERN.hex()
    r = _run(
        memcard_rw,
        "--dir",
        str(card_dir),
        "write-sector",
        "--slot",
        "0",
        "--sector",
        str(TEST_SECTOR),
        "--hex",
        hex_pat,
    )
    assert r.returncode == 0, r.stderr

    r = _run(
        memcard_rw,
        "--dir",
        str(card_dir),
        "read-sector",
        "--slot",
        "0",
        "--sector",
        str(TEST_SECTOR),
    )
    assert r.returncode == 0, r.stderr
    assert len(r.stdout) == SECTOR_SIZE
    assert r.stdout[: len(TEST_PATTERN)] == TEST_PATTERN


def test_legacy_byte_offset_read_write(memcard_rw: Path, tmp_path: Path):
    card_dir = tmp_path / "cards"
    card_dir.mkdir()
    shutil.copy(FIXTURE, card_dir / "memcard0.mcr")

    payload = b"dw3-memcard-host"
    r = _run(
        memcard_rw,
        "--dir",
        str(card_dir),
        "write",
        "--slot",
        "0",
        "--offset",
        str(TEST_OFFSET + 16),
        "--hex",
        payload.hex(),
    )
    assert r.returncode == 0, r.stderr

    r = _run(
        memcard_rw,
        "--dir",
        str(card_dir),
        "read",
        "--slot",
        "0",
        "--offset",
        str(TEST_OFFSET + 16),
        "--len",
        str(len(payload)),
    )
    assert r.returncode == 0, r.stderr
    assert r.stdout == payload

    disk = (card_dir / "memcard0.mcr").read_bytes()
    assert disk[TEST_OFFSET + 16 : TEST_OFFSET + 16 + len(payload)] == payload


def test_rejects_out_of_range_legacy_offset(memcard_rw: Path, tmp_path: Path):
    card_dir = tmp_path / "cards"
    card_dir.mkdir()
    shutil.copy(FIXTURE, card_dir / "memcard0.mcr")
    r = _run(
        memcard_rw,
        "--dir",
        str(card_dir),
        "read",
        "--slot",
        "0",
        "--offset",
        str(CARD_SIZE - 4),
        "--len",
        "8",
        check=False,
    )
    assert r.returncode != 0
