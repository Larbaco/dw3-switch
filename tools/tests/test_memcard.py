"""Wave 2 tests for port/memcard via the tools/memcard_rw host CLI.

No DuckStation: uses tools/testdata/memcard/ and a temp slot directory.
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
RW_C = ROOT / "tools" / "memcard_rw.c"

CARD_SIZE = 131072
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
    # Directory entry 0 (sector 1) marked free.
    assert data[SECTOR_SIZE] == 0xA0


def test_ensure_creates_slot_images(memcard_rw: Path, tmp_path: Path):
    card_dir = tmp_path / "cards"
    card_dir.mkdir()
    r = _run(memcard_rw, "--dir", str(card_dir), "ensure", "--slot", "0")
    assert r.returncode == 0
    path = card_dir / "memcard0.mcr"
    assert path.is_file()
    assert path.stat().st_size == CARD_SIZE
    assert path.read_bytes()[0:2] == b"MC"

    r = _run(memcard_rw, "--dir", str(card_dir), "ensure", "--slot", "1")
    assert r.returncode == 0
    assert (card_dir / "memcard1.mcr").is_file()


def test_sector_read_write_roundtrip(memcard_rw: Path, tmp_path: Path):
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


def test_byte_offset_read_write(memcard_rw: Path, tmp_path: Path):
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

    # Direct file check at the same absolute offset.
    disk = (card_dir / "memcard0.mcr").read_bytes()
    assert disk[TEST_OFFSET + 16 : TEST_OFFSET + 16 + len(payload)] == payload


def test_rejects_out_of_range_offset(memcard_rw: Path, tmp_path: Path):
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
