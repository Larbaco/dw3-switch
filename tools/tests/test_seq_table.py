"""The host's libsnd SEQ table keeps PS1 geometry: rows of 16 entries, one
after the other, so SsSepPlay(0, 16..31) reaches the system bank's second SEP
file (win jingle, menu sounds). Before the fix the rows were 0x100 apart while
an entry is 0xC0 bytes, so those tracks never played."""

from __future__ import annotations

import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PSYZ = ROOT / "vendor" / "psyz"


def test_seq_table_geometry(tmp_path: Path):
    out = tmp_path / "seq_table_test"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O1", "-D__psyz",
           "-o", str(out),
           str(ROOT / "tools" / "tests" / "seq_table_main.c"),
           str(ROOT / "platforms" / "pc" / "src" / "psyz_host_overrides.c"),
           f"-I{PSYZ / 'decomp' / 'src' / 'libsnd'}",
           f"-I{PSYZ / 'psyz' / 'include'}",
           f"-I{PSYZ / 'decomp' / 'include'}",
           f"-I{PSYZ / 'decomp' / 'src'}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    run = subprocess.run([str(out)], capture_output=True, text=True)
    assert run.returncode == 0, run.stdout + run.stderr
    assert "OK: seq table" in run.stdout
    # the constant the override uses is the size of the struct
    assert "sizeof(struct SeqStruct) = 192" in run.stdout
