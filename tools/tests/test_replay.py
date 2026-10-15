"""P6.15 replay tooling: committed recordings and the image comparison.

No game data is needed: the images are synthetic.
"""
from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

import pytest

PIL = pytest.importorskip("PIL")
from PIL import Image  # noqa: E402

REPLAY = Path(__file__).resolve().parents[1] / "replay"


def _load(name):
    spec = importlib.util.spec_from_file_location(name, REPLAY / f"{name}.py")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


def test_committed_recordings_match_generator(tmp_path):
    gen = _load("gen_scenarios")
    gen.main(tmp_path)
    names = sorted(p.name for p in tmp_path.glob("*.rec"))
    assert names == sorted(p.name for p in (REPLAY / "scenarios").glob("*.rec"))
    for n in names:
        assert (tmp_path / n).read_text() == (REPLAY / "scenarios" / n).read_text(), n


def test_every_runner_scenario_has_a_recording_with_shots():
    rt = _load("replay_tests")
    for name, version in rt.SCENARIOS:
        rec = REPLAY / "scenarios" / f"{name}.rec"
        assert rec.is_file(), name
        assert rt.shot_names(name), name
        assert version in ("us", "eu")


def test_compare_identical_and_noise(tmp_path):
    rt = _load("replay_tests")
    a = Image.new("RGB", (64, 64), (10, 20, 30))
    b = a.copy()
    b.putpixel((3, 3), (14, 20, 30))  # +4 on one channel: under tolerance
    a.save(tmp_path / "a.png")
    b.save(tmp_path / "b.png")
    n, total, err = rt.compare_images(tmp_path / "a.png", tmp_path / "b.png", None)
    assert (n, total, err) == (0, 4096, None)


def test_compare_detects_block_and_writes_diff(tmp_path):
    rt = _load("replay_tests")
    a = Image.new("RGB", (64, 64), (10, 20, 30))
    b = a.copy()
    for x in range(8):
        for y in range(8):
            b.putpixel((x, y), (200, 20, 30))
    a.save(tmp_path / "a.png")
    b.save(tmp_path / "b.png")
    n, total, err = rt.compare_images(tmp_path / "a.png", tmp_path / "b.png",
                                      tmp_path / "d.png")
    assert n == 64 and err is None
    assert Image.open(tmp_path / "d.png").size == (64 * 3, 64)


def test_compare_size_mismatch(tmp_path):
    rt = _load("replay_tests")
    Image.new("RGB", (8, 8)).save(tmp_path / "a.png")
    Image.new("RGB", (9, 8)).save(tmp_path / "b.png")
    _, _, err = rt.compare_images(tmp_path / "a.png", tmp_path / "b.png", None)
    assert err and "size" in err
