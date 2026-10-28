"""Build/test speed tooling (docs/BUILD_SPEED.md): the replay changed-area
selection, the --jobs auto sizing, and the incremental apply_patches stamp.

No game data and no PC build are needed.
"""
from __future__ import annotations

import importlib.util
import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
REPLAY = ROOT / "tools" / "replay"


def _load(name):
    spec = importlib.util.spec_from_file_location(name, REPLAY / f"{name}.py")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


@pytest.fixture(scope="module")
def areas():
    sys.path.insert(0, str(REPLAY))
    _load("manifest")
    return _load("areas")


@pytest.fixture(scope="module")
def scenarios(areas):
    return sys.modules["manifest"].SCENARIOS


def _names(sel):
    return sel.names


def test_docs_only_runs_just_quick_and_smoke(areas, scenarios):
    sel = areas.select(["docs/BUILD_SPEED.md", "README.md", "tools/tests/test_x.py"], scenarios)
    assert not sel.everything
    quick = {s.name for s in scenarios if s.tier == "quick"}
    assert quick <= sel.names
    assert set(areas.SMOKE) <= sel.names
    assert len(sel.names) < 20


def test_cheats_change_selects_cheat_scenarios_not_everything(areas, scenarios):
    sel = areas.select(["port/options/cheats.c"], scenarios)
    assert not sel.everything
    assert "us-cheats-screen" in sel.names and "eu-cheat-money" in sel.names
    assert "us-p77-claw-ko" not in sel.names


def test_patch_topic_maps_to_its_scenarios(areas, scenarios):
    sel = areas.select(["port/patches/0071-PORT_HOST-p77-battle-hud-form-name.patch"], scenarios)
    assert "us-p77-hud-form" in sel.names and "eu-p77-hud-form-fr" in sel.names
    assert "us-cheats-screen" not in sel.names


def test_unknown_game_file_selects_the_full_tier(areas, scenarios):
    sel = areas.select(["port/never_seen/x.c"], scenarios)
    assert sel.everything
    full = {s.name for s in scenarios if s.tier in ("quick", "full")}
    assert sel.names == full


def test_unmapped_non_game_file_is_harmless(areas, scenarios):
    assert not areas.select(["Makefile.local", "tools/get_ccache.sh"], scenarios).everything


def test_recording_change_selects_its_owner(areas, scenarios):
    sc = next(s for s in scenarios if s.name == "us-two-battles")
    sel = areas.select([f"tools/replay/scenarios/{sc.phases[0]}.rec"], scenarios)
    assert "us-two-battles" in sel.names and not sel.everything


def test_every_rule_glob_matches_a_scenario(areas, scenarios):
    """A renamed scenario must not silently drop out of a rule."""
    sel = areas.Selection()
    import fnmatch
    full = {s.name for s in scenarios if s.tier in ("quick", "full")}
    dead = []
    for _rx, what in areas.RULES:
        if isinstance(what, tuple):
            dead += [g for g in what if not any(fnmatch.fnmatchcase(n, g) for n in full)]
    assert not dead, dead
    assert sel.names == set()


def test_auto_jobs_is_bounded_by_cores_and_ram():
    sys.path.insert(0, str(REPLAY))
    spec = importlib.util.spec_from_file_location("replay_tests_mod", REPLAY / "replay_tests.py")
    pytest.importorskip("PIL")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    assert mod.auto_jobs(cpus=16, avail_mib=30000) == 8
    assert mod.auto_jobs(cpus=16, avail_mib=2000) == 2      # RAM limits first
    assert mod.auto_jobs(cpus=2, avail_mib=30000) == 1
    assert mod.auto_jobs(cpus=16, avail_mib=100) == 1       # never zero


@pytest.mark.skipif(not (ROOT / "vendor/dw3_decomp/.git").exists()
                    and not (ROOT / "vendor/dw3_decomp/.git").is_symlink(),
                    reason="decomp submodule not checked out")
def test_apply_patches_is_incremental(tmp_path):
    """Second run is a no-op; an edited build copy is restored; only files whose
    content changed get a new mtime."""
    script = ROOT / "tools" / "apply_patches.sh"
    work = tmp_path / "root"
    (work / "tools").mkdir(parents=True)
    shutil.copy(script, work / "tools" / "apply_patches.sh")
    (work / "port").mkdir()
    shutil.copytree(ROOT / "port" / "patches", work / "port" / "patches")
    (work / "vendor").mkdir()
    os.symlink(ROOT / "vendor" / "dw3_decomp", work / "vendor" / "dw3_decomp")
    env = dict(os.environ)

    def run():
        return subprocess.run(["bash", str(work / "tools" / "apply_patches.sh")],
                              capture_output=True, text=True, env=env)

    r = run()
    assert r.returncode == 0, r.stderr
    dst = work / "build" / "decomp-src"
    assert (dst / ".dw3-apply-hash").is_file()
    assert "Copying" in r.stdout
    r = run()
    assert r.returncode == 0 and "up to date" in r.stdout
    victim = next(p for p in sorted((dst / "src").rglob("*.c")))
    keep = next(p for p in sorted((dst / "src").rglob("*.c")) if p != victim)
    clean = victim.read_bytes()
    keep_mtime = keep.stat().st_mtime_ns
    victim.write_bytes(clean + b"\n// edited\n")
    r = run()
    assert r.returncode == 0 and "Copying" in r.stdout
    assert victim.read_bytes() == clean
    assert keep.stat().st_mtime_ns == keep_mtime
