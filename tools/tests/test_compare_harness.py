"""CLI contract for the P3.9 compare harness. Does not launch DuckStation or dw3-pc."""

from __future__ import annotations

import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "compare_harness" / "capture_pair.py"


def load():
    spec = importlib.util.spec_from_file_location("capture_pair", SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


def test_battle_pc_env_strips_force_win():
    mod = load()
    env = mod.pc_subject_env(
        {
            "DW3_FORCE_WIN": "1",
            "DW3_AUTO_PAD": "confirm",
            "DW3_START_MODE": "title",
            "DW3_FORCE_ENCOUNTER": "99",
            "DW3_FORCE_PARTY": "0",
            "HOME": "/tmp",
        },
        "battle",
        34,
        party=1,
    )
    assert env["DW3_START_MODE"] == "0x600"
    assert env["DW3_FORCE_ENCOUNTER"] == "34"
    assert env["DW3_FORCE_PARTY"] == "1"
    assert "DW3_FORCE_WIN" not in env
    assert "DW3_AUTO_PAD" not in env
    assert env["HOME"] == "/tmp"


def test_title_env_clears_battle_aids():
    mod = load()
    env = mod.pc_subject_env(
        {
            "DW3_START_MODE": "0x600",
            "DW3_FORCE_WIN": "1",
            "DW3_FORCE_ENCOUNTER": "1",
            "DW3_FORCE_PARTY": "1",
        },
        "title",
        34,
    )
    assert "DW3_START_MODE" not in env
    assert "DW3_FORCE_WIN" not in env
    assert "DW3_FORCE_ENCOUNTER" not in env
    assert "DW3_FORCE_PARTY" not in env


def test_battle_duck_path_without_anchor_is_blocked():
    mod = load()
    assert mod.main(["--label", "battle", "--out", "/tmp/dw3-compare-unused"]) == 3


def test_pc_only_battle_does_not_need_duckstation():
    mod = load()
    args = mod.build_parser().parse_args(["--label", "battle", "--pc-only"])
    args.duckstation = None
    args.disc = None
    args.statefile = None
    args.save = None
    assert mod.validate_capture(args) is None


def test_duck_only_and_pc_only_conflict():
    mod = load()
    code = mod.main(
        [
            "--label",
            "battle",
            "--duck-only",
            "--pc-only",
            "--statefile",
            "/tmp/does-not-matter.state",
        ]
    )
    assert code == 2


def test_battle_pc_only_dry_run():
    mod = load()
    assert mod.main(["--label", "battle", "--pc-only", "--dry-run", "--encounter", "1"]) == 0
