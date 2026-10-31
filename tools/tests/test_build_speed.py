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


def test_wireframe_patch_selects_the_digivolve_command_scenarios(areas, scenarios):
    sel = areas.select(["port/patches/0198-PORT_HOST-battle-wireframe-64bit-tag.patch"], scenarios)
    assert not sel.everything
    for name in ("us-battle-digivolve-command", "eu-battle-digivolve-command", "us-p77-hud-form"):
        assert name in sel.names, name
    assert "us-cheats-screen" not in sel.names


def test_overlay_table_and_stage_patch_select_the_stage_scenarios(areas, scenarios):
    sel = areas.select(["port/overlays/overlay_table_eu.c", "port/overlays/overlay_sections_eu_extra.c",
                        "port/patches/0197-PORT_HOST-wstag460-wanderer-no-object.patch"], scenarios)
    assert not sel.everything
    for name in ("us-stage-0x236", "eu-stage-0x236", "eu-extra-stage-974", "us-walkthrough-pe",
                 "eu-underground-exit", "us-stage-sweep"):
        assert name in sel.names, name
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


def test_glyph_files_select_the_icon_screens(areas, scenarios):
    sel = areas.select(["port/options/switch_glyphs.c", "port/patches/0195-PORT_HOST-p106-icon-census.patch"],
                       scenarios)
    assert not sel.everything
    assert {"us-glyphs-on-shop", "eu-glyphs-off", "us-item-shop", "eu-save-screen"} <= sel.names
    assert "us-soak" not in sel.names


# --- CMake area rule ------------------------------------------------------

_PORT_CMAKE = "port/CMakeLists.txt"
_ADD_SOURCE = """\
--- a/port/CMakeLists.txt
+++ b/port/CMakeLists.txt
@@ -361,0 +362 @@ add_library(dw3_port_options${_S} OBJECT
+  "${CMAKE_CURRENT_SOURCE_DIR}/walkthrough/walkthrough_screen.c"
"""


def test_cmake_source_added_selects_its_area_not_everything(areas, scenarios):
    sel = areas.select([_PORT_CMAKE], scenarios, lambda p: _ADD_SOURCE)
    assert not sel.everything
    assert "us-walkthrough-p0" in sel.names
    assert "us-cheats-screen" not in sel.names
    assert len(sel.names) < 0.7 * len({s.name for s in scenarios if s.tier in ("quick", "full")})


def test_cmake_source_removed_is_the_same(areas, scenarios):
    diff = _ADD_SOURCE.replace("\n+  ", "\n-  ")
    sel = areas.select([_PORT_CMAKE], scenarios, lambda p: diff)
    assert not sel.everything and "us-walkthrough-p0" in sel.names


def test_cmake_source_with_no_rule_still_selects_everything(areas, scenarios):
    diff = _ADD_SOURCE.replace("walkthrough/walkthrough_screen.c", "never_seen/x.c")
    assert areas.select([_PORT_CMAKE], scenarios, lambda p: diff).everything


@pytest.mark.parametrize("line", [
    "target_compile_definitions(dw3_port PRIVATE DW3_CHEATS=1)",     # definition
    "add_library(dw3_new OBJECT",                                    # target
    "if(DW3_CHEATS)",                                                # condition
    "-O2",                                                           # flag
    '"${DECOMP_SRC}/src/menus/title/scene.c"',                       # variable not mapped
    '"${CMAKE_CURRENT_SOURCE_DIR}/../../outside.c"',                 # leaves the repo
])
def test_cmake_other_changes_select_everything(areas, scenarios, line):
    diff = _ADD_SOURCE + f"+  {line}\n"
    assert areas.select([_PORT_CMAKE], scenarios, lambda p: diff).everything


def test_version_wiring_in_the_pc_cmake_selects_the_title_and_options_scenarios(areas, scenarios):
    # P6.37: include() of the version module + dw3_version_setup() add port_version.c
    diff = """\
--- a/platforms/pc/CMakeLists.txt
+++ b/platforms/pc/CMakeLists.txt
@@ -113,0 +114,3 @@ x
+# P6.37: the port version label
+include(${CMAKE_SOURCE_DIR}/../../port/version/version.cmake)
+dw3_version_setup(dw3-pc)
"""
    sel = areas.select(["platforms/pc/CMakeLists.txt"], scenarios, lambda p: diff)
    assert not sel.everything
    assert "us-title-noversion" in sel.names and "us-options-cheats-listed" in sel.names
    assert "us-walkthrough-p0" not in sel.names
    other = diff + "+target_compile_definitions(dw3-pc PRIVATE X=1)\n"
    assert areas.select(["platforms/pc/CMakeLists.txt"], scenarios, lambda p: other).everything
    assert not areas.select(["port/version/version.cmake"], scenarios).everything


def test_cmake_without_a_diff_selects_everything(areas, scenarios):
    assert areas.select([_PORT_CMAKE], scenarios).everything               # --changed-files
    assert areas.select([_PORT_CMAKE], scenarios, lambda p: None).everything  # untracked


def test_cmake_comment_only_change_selects_no_extra_scenarios(areas, scenarios):
    diff = "@@ -1 +1 @@\n-# old note\n+# new note\n+\n"
    sel = areas.select([_PORT_CMAKE], scenarios, lambda p: diff)
    assert not sel.everything
    assert len(sel.names) < 20


def test_build_file_sources_resolves_each_path_form(areas):
    diff = ("+  src/a.c\n"
            '+  "${CMAKE_CURRENT_SOURCE_DIR}/sub/b.c"\n'
            "+  ${CMAKE_SOURCE_DIR}/../../port/pages/pages.c\n"
            '+  "${DW3_ROOT}/port/saves/c.c"  # note\n')
    assert areas.build_file_sources("platforms/pc/CMakeLists.txt", diff) == [
        "platforms/pc/src/a.c", "platforms/pc/sub/b.c", "port/pages/pages.c", "port/saves/c.c"]


def test_cmake_files_outside_port_and_pc_keep_their_old_rules(areas, scenarios):
    assert not areas.select(["platforms/switch/CMakeLists.txt"], scenarios).everything
    assert areas.select(["CMakeLists.txt"], scenarios).everything


def test_runner_reads_a_real_git_diff_of_the_build_file(tmp_path):
    """build_file_diff() against a throwaway repo: tracked edit, untracked file."""
    pytest.importorskip("PIL")
    spec = importlib.util.spec_from_file_location("replay_tests_diff", REPLAY / "replay_tests.py")
    sys.path.insert(0, str(REPLAY))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    git = lambda *a: subprocess.run(["git", "-C", str(tmp_path), "-c", "user.name=t",
                                     "-c", "user.email=t@t", *a], check=True, capture_output=True)
    git("init", "-q", "-b", "main")
    (tmp_path / "port").mkdir()
    cm = tmp_path / "port" / "CMakeLists.txt"
    cm.write_text('add_library(x\n  "${CMAKE_CURRENT_SOURCE_DIR}/a.c"\n)\n')
    git("add", "."), git("commit", "-q", "-m", "base")
    cm.write_text('add_library(x\n  "${CMAKE_CURRENT_SOURCE_DIR}/a.c"\n'
                  '  "${CMAKE_CURRENT_SOURCE_DIR}/walkthrough/b.c"\n)\n')
    (tmp_path / "port" / "new.cmake").write_text("x\n")
    old_root = mod.ROOT
    mod.ROOT = tmp_path
    try:
        diff = mod.build_file_diff("main", "port/CMakeLists.txt")
        assert mod.build_file_diff("main", "port/new.cmake") is None
    finally:
        mod.ROOT = old_root
    sys.path.insert(0, str(REPLAY))
    areas_mod = sys.modules.get("areas") or _load("areas")
    assert areas_mod.build_file_sources("port/CMakeLists.txt", diff) == ["port/walkthrough/b.c"]


# --- both-binary subset ----------------------------------------------------

def test_both_subset_has_one_scenario_per_area_and_region(areas, scenarios):
    names = areas.both_subset(scenarios)
    assert names == sorted(names) and len(names) == len(set(names))
    by = {s.name: s for s in scenarios}
    assert all(by[n].tier in ("quick", "full", "both") for n in names)
    assert {by[n].version for n in names} == {"us", "eu"}
    # every (area, region) of the pool has exactly one representative
    pool = [s for s in scenarios if s.tier in ("quick", "full", "both")]
    for key in {(areas.scenario_area(s.name), s.version) for s in pool}:
        assert sum((areas.scenario_area(n), by[n].version) == key for n in names) == 1, key


def test_both_subset_adds_overrides_and_ignores_unknown_names(areas, scenarios):
    base = set(areas.both_subset(scenarios))
    extra = next(s.name for s in scenarios if s.tier == "full" and s.name not in base)
    got = set(areas.both_subset(scenarios, [extra, "no-such-scenario"]))
    assert got == base | {extra}


def test_both_subset_representative_prefers_both_then_quick_then_name(areas):
    from types import SimpleNamespace as NS
    mk = lambda n, t: NS(name=n, tier=t, version="us", phases=())
    pool = [mk("us-options-a", "full"), mk("us-options-b", "quick"), mk("us-options-c", "full")]
    assert areas.both_subset(pool) == ["us-options-b"]
    pool.append(mk("us-options-z", "both"))
    assert areas.both_subset(pool) == ["us-options-z"]
    pool = [mk("us-options-b", "full"), mk("us-options-a", "full"), mk("us-solo-x", "full"),
            mk("us-soak-y", "soak"), mk("us-nx-z", "nx")]
    assert areas.both_subset(pool) == ["us-options-a", "us-solo-x"]   # own area; soak and nx are out


def test_both_subset_runner_option_matches_the_function(areas, scenarios, tmp_path):
    """--both-subset --dry-run lists areas.both_subset() plus the refs overrides."""
    pytest.importorskip("PIL")
    refs = tmp_path / "refs"
    extra = next(s.name for s in scenarios if s.tier == "full"
                 and s.name not in areas.both_subset(scenarios))
    (refs / extra / "both").mkdir(parents=True)
    r = subprocess.run([sys.executable, str(REPLAY / "replay_tests.py"), "--both-subset",
                        "--dry-run", "--refs", str(refs)], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    listed = {l.strip() for l in r.stdout.splitlines()[1:]}
    assert listed == set(areas.both_subset(scenarios, [extra]))


_MAIN_C = "platforms/pc/src/main.c"


def test_pc_main_c_selects_the_full_tier_with_its_own_reason(areas, scenarios):
    """main.c is the PC host entry (main(), boot, frame hooks): full tier, and the
    printed reason says why, not the generic 'no rule' fallback."""
    sel = areas.select([_MAIN_C], scenarios)
    assert sel.everything
    assert sel.names == {s.name for s in scenarios if s.tier in ("quick", "full")}
    (path, why), = [r for r in sel.reasons if r[0] == _MAIN_C]
    assert "PC host entry" in why and "full tier" in why
    assert "no rule" not in why


def test_pc_main_c_is_full_tier_even_with_a_cmake_diff(areas, scenarios):
    """The diff callback is not used for main.c: only BUILD rules read a diff."""
    sel = areas.select([_MAIN_C], scenarios, lambda p: "+ \"${CMAKE_CURRENT_SOURCE_DIR}/x.c\"\n")
    assert sel.everything


def test_other_pc_host_files_keep_the_generic_full_tier(areas, scenarios):
    """Only main.c has the explicit rule; the rest of platforms/pc/ still falls back."""
    sel = areas.select(["platforms/pc/src/never_seen.c"], scenarios)
    assert sel.everything
    assert any("no rule" in why for _p, why in sel.reasons)

