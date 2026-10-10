import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PACK = ROOT / "tools" / "pack_runtime.py"
BUILD_MAP = ROOT / "tools" / "build_file_map.py"
FT = ROOT / "tools" / "testdata" / "file_table_us_fixture.c"


def _write_disks(disks: Path, *, with_map: bool = True) -> None:
    pro = disks / "AAA" / "PRO"
    pro.mkdir(parents=True)
    (pro / "X.PRO").write_bytes(b"x-pro-bytes")
    (disks / "SLUS_014.36").write_bytes(b"fake-exe")
    if with_map:
        map_data = {
            "version": "us",
            "executable": "SLUS_014.36",
            "files": {
                "1": {
                    "path": "DUMMY.DAT",
                    "lba": 22,
                    "sectors": 1,
                    "bytes": 2048,
                    "form": 1,
                }
            },
        }
        (disks / "files_us.json").write_text(
            json.dumps(map_data, indent=2) + "\n", encoding="utf-8"
        )


def test_pack_copies_aaa_and_map(tmp_path):
    disks = tmp_path / "disks"
    out = tmp_path / "runtime"
    _write_disks(disks)

    r = subprocess.run(
        [
            sys.executable,
            str(PACK),
            "--version",
            "us",
            "--disks",
            str(disks),
            "--out",
            str(out),
        ],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stderr
    assert (out / "AAA" / "PRO" / "X.PRO").read_bytes() == b"x-pro-bytes"
    assert (out / "SLUS_014.36").read_bytes() == b"fake-exe"
    assert (out / "files_us.json").is_file()
    assert (out / "BOOT_IMAGE.txt").is_file()

    check = subprocess.run(
        [
            sys.executable,
            str(BUILD_MAP),
            "--check",
            str(out / "files_us.json"),
            "--file-table",
            str(FT),
        ],
        capture_output=True,
        text=True,
    )
    assert check.returncode == 0, check.stderr


def test_pack_refuses_missing_map(tmp_path):
    disks = tmp_path / "disks"
    out = tmp_path / "runtime"
    _write_disks(disks, with_map=False)

    r = subprocess.run(
        [
            sys.executable,
            str(PACK),
            "--version",
            "us",
            "--disks",
            str(disks),
            "--out",
            str(out),
        ],
        capture_output=True,
        text=True,
    )
    assert r.returncode != 0
    assert "files_us.json" in (r.stderr + r.stdout)
