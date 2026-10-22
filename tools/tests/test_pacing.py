"""P6.18 pacing policy and glue (port/pacing/*.c): the encounter step scaling,
the text delay mapping and the log lines the replay tests rely on."""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / "tools" / "tests"
PACING = ROOT / "port" / "pacing"


@pytest.fixture(scope="module")
def pacing_bin(tmp_path_factory) -> Path:
    out = tmp_path_factory.mktemp("pacing") / "pacing_test"
    cmd = ["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
           "-DDW3_PORT_PACING_TEST", "-o", str(out),
           str(TESTS / "pacing_main.c"), str(PACING / "pacing.c"),
           str(PACING / "pacing_policy.c"), f"-I{ROOT / 'port' / 'include'}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def test_pacing_policy_and_glue(pacing_bin: Path):
    r = subprocess.run([str(pacing_bin)], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK: pacing policy" in r.stdout


def test_pacing_log_lines(pacing_bin: Path):
    err = subprocess.run([str(pacing_bin)], capture_output=True, text=True).stderr
    # Off logs the first footstep and then every 32nd (the replay tests count on it).
    assert "dw3-pacing: encounters off: 1 footstep(s) in battle areas ignored" in err
    assert "dw3-pacing: encounters off: 33 footstep(s) in battle areas ignored" in err
    assert "dw3-pacing: encounters off: 97 footstep(s) in battle areas ignored" in err
    assert "encounters off: 2 footstep" not in err
    # 0.5x: 80 footsteps counted for a countdown that takes 40 at 1x.
    assert re.search(r"random battle after 81 footstep\(s\) \(encounter rate 0\.5x\)", err)
    assert "dw3-pacing: map-forced area battle on the first footstep" in err
    # Text: two pages, three and two frames, once each.
    pages = re.findall(r"text page typed in (\d+) frame\(s\) \(text speed (\w+)\)", err)
    assert pages == [("3", "fast"), ("2", "fast")]
