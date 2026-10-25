"""P5.1 soak tools (tools/memaudit): the generated recording and the report's
growth maths. The soak itself needs a built dw3-pc and the game data, so only
the pure parts run here."""

from __future__ import annotations

import importlib.util
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]


def load(name: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools" / "memaudit" / f"{name}.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


@pytest.mark.parametrize("version", ["us", "eu"])
def test_soak_recording_is_valid(version, tmp_path):
    soak = load("soak")
    rec = tmp_path / "soak.rec"
    soak.make_rec(version, 20000, rec, shot_every=5000)
    pads, end = [], None
    for ln in rec.read_text().splitlines():
        if ln.startswith("#"):
            continue
        p = ln.split()
        if p[0] == "pad":
            pads.append((int(p[1]), int(p[2], 16)))
        elif p[0] == "end":
            end = int(p[1])
    frames = [f for f, _ in pads]
    assert frames == sorted(frames), "pad lines must be in frame order"
    assert end == 20000 and frames[-1] <= end
    assert pads[-1][1] == 0, "the last pad line releases the buttons"
    assert all(m & ~0xFFFF == 0 for _, m in pads)
    # the Continue sequence of the as-load recording comes first, then the loop
    assert any(f > soak.LOOP_START[version] for f in frames)
    n = len(soak.prefix_lines(version, soak.CONTINUE_END[version]))
    assert 0 < n < len(pads) and max(frames[:n]) < soak.LOOP_START[version]


def test_slope_and_report_helpers():
    report = load("report")
    assert report.slope([0, 1, 2, 3], [5, 7, 9, 11]) == pytest.approx(2.0)
    assert report.slope([0, 1, 2, 3], [4, 4, 4, 4]) == pytest.approx(0.0)
    assert report.slope([0], [1]) == 0.0
    assert report.parse_pc("/usr/lib/libc.so.6+0x1a2b") == ("/usr/lib/libc.so.6", 0x1A2B)
