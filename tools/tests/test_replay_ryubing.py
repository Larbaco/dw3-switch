"""Ryubing replay runner helpers (tools/replay/ryubing.py). No emulator, no
game data: the log text is synthetic."""
from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

import pytest

pytest.importorskip("PIL")

REPLAY = Path(__file__).resolve().parents[1] / "replay"


def _load():
    sys.path.insert(0, str(REPLAY))
    spec = importlib.util.spec_from_file_location("ryubing_under_test", REPLAY / "ryubing.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _mem(heap_hw, fc_hw, libc_hw):
    return ("00:00:01.000 |W| HLE.GuestThread.28 KernelSvc OutputDebugString: "
            f"[I][nx_mem_budget.c:170][f] mem dbg: heap=300K/8192K hw={heap_hw}K free=7870K "
            f"fcache=158K/hw={fc_hw}K vram=3840K libc=170000K/hw={libc_hw}K")


def test_guest_lines_strip_emulator_prefix():
    ry = _load()
    out = ry.guest_lines("00:00:41.860 |W| HLE.GuestThread.28 KernelSvc OutputDebugString: replay shot 200 title\n"
                         "00:00:42.000 |I| Something else\n")
    assert out[0] == "replay shot 200 title" and out[1].endswith("Something else")
    assert ry.SHOT_LINE.search(out[0]).groups() == ("200", "title")


def test_mem_verdict_flat_and_growing():
    ry = _load()
    flat = [_mem(321, 158, 175000)] * 20
    ok, detail = ry.mem_verdict(flat, 0)
    assert ok and "+0K" in detail
    # high-water marks that keep rising after the 60% mark fail
    grow = [_mem(321 + i * 100, 158, 175000 + i * 300) for i in range(40)]
    ok, detail = ry.mem_verdict(grow, 0)
    assert not ok
    # a one-time libc step of 565-601 KiB passes; heap and file cache keep 512
    step = [_mem(321, 158, 175000)] * 14 + [_mem(321, 158, 175601)] * 6
    assert ry.mem_verdict(step, 0)[0]
    step = [_mem(321, 158, 175000)] * 14 + [_mem(321, 158 + 601, 175000)] * 6
    assert not ry.mem_verdict(step, 0)[0]
    step = [_mem(321, 158, 175000)] * 14 + [_mem(321, 158, 175900)] * 6
    assert not ry.mem_verdict(step, 0)[0]
    # nothing to judge without an nxlink build's samples
    assert ry.mem_verdict(["no samples here"] * 20, 0) is None
    assert ry.mem_verdict(flat[:3], 0) is None


def test_fatal_patterns():
    ry = _load()
    assert ry.FATAL_RE.search("00:03:00 |E| Cpu: Unhandled exception caught: Ryujinx...")
    assert ry.FATAL_RE.search("Invalid memory access at 0x0")
    assert not ry.FATAL_RE.search("replay end 16820")


def test_phase_expectations_and_nro_names(tmp_path):
    ry = _load()
    man = sys.modules["manifest"]
    rt = sys.modules["replay_tests"]
    (tmp_path / "x.rec").write_text("pad 1 0x8\nshot 5 a\nshot 9 b # c\nend 20\n")
    rt.SCEN_DIR = tmp_path
    sc = man.Scenario("x", "us", "full", phases=("x",))
    assert ry.phase_expectations(sc, 0) == ([(5, "a"), (9, "b")], 20)
    assert ry.nro_path("eu", True).parts[-2:] == ("switch-eu-nxlink-replay", "dw3-switch-nxlink-replay.nro")
    assert ry.nro_path("us", False).name == "dw3-switch-replay.nro"


def test_prepare_root_never_uses_another_agents_root(tmp_path):
    ry = _load()
    seed = tmp_path / "seed"
    seed.mkdir()
    with pytest.raises(SystemExit):
        ry.prepare_root(tmp_path / "dw3-preview29-ryu", "us", seed)
    with pytest.raises(SystemExit):
        ry.prepare_root(seed.parent, "seed", seed)   # root == the seed root itself
