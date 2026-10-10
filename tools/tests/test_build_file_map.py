import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools" / "build_file_map.py"
ISO = ROOT / "tools" / "testdata" / "mini_iso.bin"
FT = ROOT / "tools" / "testdata" / "file_table_us_fixture.c"


def test_maps_lba_to_path(tmp_path):
    out = tmp_path / "files_us.json"
    r = subprocess.run(
        [
            sys.executable,
            str(TOOL),
            "--bin",
            str(ISO),
            "--version",
            "us",
            "--file-table",
            str(FT),
            "--out",
            str(out),
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    data = json.loads(out.read_text())
    assert data["version"] == "us"
    assert data["files"]["1"]["path"].endswith("DUMMY.DAT")
    assert data["files"]["1"]["lba"] == 22
    assert data["files"]["1"]["sectors"] == 1


def test_check_passes_on_complete_map(tmp_path):
    out = tmp_path / "files_us.json"
    subprocess.run(
        [
            sys.executable,
            str(TOOL),
            "--bin",
            str(ISO),
            "--version",
            "us",
            "--file-table",
            str(FT),
            "--out",
            str(out),
        ],
        check=True,
    )
    r = subprocess.run(
        [
            sys.executable,
            str(TOOL),
            "--check",
            str(out),
            "--file-table",
            str(FT),
        ],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stderr
