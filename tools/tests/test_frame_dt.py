"""60-pump frame spacing. The helper has no libnx dependency, so gcc can run it."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope="module")
def frame_dt_output(tmp_path_factory) -> dict[str, str]:
    out = tmp_path_factory.mktemp("frame_dt") / "frame_dt"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-o",
        str(out),
        str(ROOT / "tools" / "tests" / "frame_dt_main.c"),
        f"-I{ROOT / 'platforms' / 'switch' / 'source'}",
    ]
    build = subprocess.run(cmd, capture_output=True, text=True)
    assert build.returncode == 0, build.stderr + build.stdout
    run = subprocess.run([str(out)], capture_output=True, text=True)
    assert run.returncode == 0, run.stderr + run.stdout
    parsed: dict[str, str] = {}
    for line in run.stdout.splitlines():
        key, _, value = line.partition("=")
        parsed[key.strip()] = value.strip()
    return parsed


def test_frame_dt_logs_only_after_two_samples(frame_dt_output):
    assert frame_dt_output["pump0"] == "1"
    assert frame_dt_output["no_line_before_60"] == "1"
    assert frame_dt_output["arm"] == "1"
    assert frame_dt_output["dt_after_arm"] == "1"
    assert frame_dt_output["sample_120"] == "1"
    assert frame_dt_output["dt_120"] == "1"
    assert frame_dt_output["skip_121"] == "1"
    assert frame_dt_output["sample_180"] == "1"
    assert frame_dt_output["dt_180"] == "1"
