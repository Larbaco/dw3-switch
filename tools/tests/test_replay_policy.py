"""Replay region policy (docs/REPLAY_POLICY.md): which region a changed file has
to be tested on, the cross-region canary, the area sweep and the plan that
`replay_tests.py --changed` builds from them.

No game data and no PC build are needed.
"""
from __future__ import annotations

import importlib.util
import json
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


def _plan(areas, scenarios, files, text=None, **kw):
    sel = areas.select(files, scenarios, None, (lambda p: text.get(p)) if text else None)
    return sel, areas.plan(sel, scenarios, **kw)


def _regions(areas, scenarios, names):
    by = {s.name: s for s in scenarios}
    out = {}
    for n in names:
        out.setdefault(areas.scenario_region(by[n]), set()).add(n)
    return out


# --- the classification of one file -------------------------------------------

@pytest.mark.parametrize("path,cls", [
    ("port/options/cheats.c", "default"),
    ("port/pacing/text_speed.c", "default"),
    ("port/ui/widgets.c", "default"),
    ("port/overlays/overlay_table_us.c", "us"),
    ("port/overlays/overlay_table_eu.c", "eu"),
    ("port/overlays/overlay_sections_eu_extra.c", "eu"),
    ("port/overlays/overlay_sections.c", "both"),
    ("port/saves/dw3_save.c", "both"),
    ("port/memcard/memcard_host.c", "both"),
    ("port/options/lang_select.c", "langs"),
    ("port/options/region_relaunch.c", "langs"),
    ("port/help/help_text.c", "langs"),
    ("port/walkthrough/walkthrough.c", "langs"),
    ("port/patches/0055-PORT_HOST-p62-eu-country-select-boot.patch", "langs"),
    ("port/patches/0197-PORT_HOST-p5-wstag460-link.patch", "both"),
    ("port/patches/DECOMP_PIN", "both"),
    ("vendor/dw3_decomp", "both"),
])
def test_region_class_by_path(areas, path, cls):
    assert areas.region_class(path, None) == cls


def test_region_class_by_content_mark(areas):
    text = {"port/options/cheats.c": "#if VERSION_EU\nint x;\n#endif\n",
            "port/options/turbo.c": "int plain;\n",
            "port/patches/0099-PORT_HOST-p99-foo.patch": "+#if VERSION_EU && 1\n",
            "docs/X.md": "VERSION_EU mentioned in prose"}
    rd = text.get
    assert areas.region_class("port/options/cheats.c", rd) == "both"
    assert areas.region_class("port/options/turbo.c", rd) == "default"
    assert areas.region_class("port/patches/0099-PORT_HOST-p99-foo.patch", rd) == "both"
    assert areas.region_class("docs/X.md", rd) == "default"      # prose is not code
    assert areas.region_class("port/options/cheats.c", None) == "default"   # no reader: path only


def test_scenario_region(areas, scenarios):
    by = {s.name: s for s in scenarios}
    assert areas.scenario_region(by["us-title"]) == "us"
    assert areas.scenario_region(by["eu-lang-fr"]) == "eu"
    for n in ("xr-us-to-eu", "xr-store-eu-to-us", "single-both-pick-eu", "us-options-region-relaunch-no"):
        assert areas.scenario_region(by[n]) == "cross", n
    assert areas.scenario_region(by["single-us-only"]) == "us"   # a one-region launcher run


# --- the canary ---------------------------------------------------------------

def test_canary_names_exist_and_cover_the_cross_region_checks(areas, scenarios):
    have = {s.name for s in scenarios}
    assert set(areas.CANARY) <= have, sorted(set(areas.CANARY) - have)
    assert len(set(areas.CANARY)) == len(areas.CANARY)
    c = areas.canary_names(scenarios)
    by = {s.name: s for s in scenarios}
    assert all(by[n].tier in ("quick", "full") for n in c)       # runs without --bin-both
    r = _regions(areas, scenarios, c)
    assert r["us"] and r["eu"] and r["cross"]                    # boots both regions, saves cross
    assert "eu-extra-stage-920" in c and "eu-lang-fr" in c       # the EU-only chapter, a language page
    assert any(n.startswith("xr-store-") for n in c)


def test_canary_adds_both_and_single_tiers_only_with_their_binary(areas, scenarios):
    base = areas.canary_names(scenarios)
    with_both = areas.canary_names(scenarios, bin_both=True)
    with_launcher = areas.canary_names(scenarios, launcher=True)
    by = {s.name: s for s in scenarios}
    assert {by[n].tier for n in with_both - base} == {"both"} and with_both - base
    assert {by[n].tier for n in with_launcher - base} == {"single"} and with_launcher - base
    assert not {n for n in base if by[n].tier in ("both", "single")}


# --- the plan --------------------------------------------------------------------

def test_region_agnostic_change_runs_the_default_region_plus_the_canary(areas, scenarios):
    sel, pl = _plan(areas, scenarios, ["port/options/cheats.c"])
    r = _regions(areas, scenarios, pl.names)
    cheats = {n for n in sel.names if "cheat" in n}
    assert {n for n in cheats if n.startswith("eu-")} <= pl.names
    assert not {n for n in cheats if n.startswith("us-")} & pl.names
    assert areas.canary_names(scenarios) <= pl.names             # the cross-region canary
    assert set(areas.SMOKE_AUTO) <= pl.names
    # the only US scenarios are the canary's and the quick tier's
    assert r["us"] <= set(areas.CANARY) | {"us-title"}


def test_language_copies_need_a_text_change(areas, scenarios):
    # help text: the French/German/... copies run, on both regions
    sel, pl = _plan(areas, scenarios, ["port/help/help_text.c"])
    assert {"eu-help-fr", "eu-help-de", "eu-help-it", "eu-help-es"} <= pl.names
    assert any(n.startswith("us-help-") for n in pl.names) and any(n.startswith("eu-help-") for n in pl.names)
    # an options change selects eu-help-* by glob too but does not need the language copies
    sel, pl = _plan(areas, scenarios, ["port/options/field_options.c"])
    copies = {n for n in pl.names if areas.LANG_COPY.match(n)}
    assert copies <= {"eu-lang-fr", "eu-lang-de", "eu-lang-it", "eu-lang-es"}   # the quick tier only


def test_region_sensitive_change_runs_the_affected_area_on_both(areas, scenarios):
    sel, pl = _plan(areas, scenarios, ["port/saves/dw3_save.c"])
    saves = {n for n in sel.names if "save" in n and not areas.LANG_COPY.match(n)}
    assert any(n.startswith("us-") for n in saves) and any(n.startswith("eu-") for n in saves)
    assert saves <= pl.names
    assert {n for n in sel.names if n.startswith("xr-")} <= pl.names


def test_content_mark_makes_a_region_agnostic_path_region_sensitive(areas, scenarios):
    text = {"port/options/cheats.c": "#if VERSION_EU\n#endif\n"}
    sel, pl = _plan(areas, scenarios, ["port/options/cheats.c"], text)
    assert {"us-cheat-money", "eu-cheat-money"} <= pl.names


def test_us_only_file_runs_on_us(areas, scenarios):
    sel, pl = _plan(areas, scenarios, ["port/overlays/overlay_table_us.c"])
    assert "us-stage-sweep" in pl.names and "us-walkthrough-p0" in pl.names
    assert "eu-walkthrough-p0" not in pl.names      # the EU table is untouched


def test_changed_recording_runs_its_own_scenarios_whatever_the_region(areas, scenarios):
    sc = next(s for s in scenarios if s.name.startswith("us-") and s.tier == "full" and "cheat" in s.name)
    sel, pl = _plan(areas, scenarios, [f"tools/replay/scenarios/{sc.phases[0]}.rec"])
    assert sc.name in pl.names


def test_engine_wide_file_runs_the_sweep_not_the_full_tier(areas, scenarios):
    full = {s.name for s in scenarios if s.tier in ("quick", "full")}
    sel, pl = _plan(areas, scenarios, ["platforms/pc/src/main.c"])
    assert sel.everything and sel.names == full                  # areas.select is unchanged
    assert len(pl.names) < len(full) / 5, len(pl.names)
    sweep_eu = set(areas.area_sweep(scenarios, ("eu",), tiers=("quick", "full")))
    assert sweep_eu <= pl.names
    assert areas.canary_names(scenarios) <= pl.names
    # one representative per area: no two sweep members of a region share an area
    areas_seen = [areas.scenario_area(n) for n in sweep_eu]
    assert len(areas_seen) == len(set(areas_seen))
    sel, pl = _plan(areas, scenarios, ["platforms/pc/src/main.c"], escalate="full")
    assert pl.names == full


def test_engine_wide_file_without_region_mark_sweeps_only_the_default_region(areas, scenarios):
    sel, pl = _plan(areas, scenarios, ["port/never_seen/x.c"], {"port/never_seen/x.c": "int x;\n"})
    assert sel.everything and sel.everything_class == "default"
    r = _regions(areas, scenarios, pl.names)
    assert r["us"] <= set(areas.CANARY) | {"us-title"}           # no US sweep
    sel, pl = _plan(areas, scenarios, ["port/never_seen/x.c"],
                    {"port/never_seen/x.c": "#if VERSION_US\n#endif\n"})
    assert sel.everything_class == "both"
    r = _regions(areas, scenarios, pl.names)
    assert len(r["us"]) > len(set(areas.CANARY) | {"us-title"})  # the US sweep is there


def test_explicit_regions(areas, scenarios):
    sel = areas.select(["port/options/cheats.c"], scenarios)
    both = areas.plan(sel, scenarios, region="both")
    assert both.names == sel.names                               # today's behaviour
    for reg in ("us", "eu"):
        only = areas.plan(sel, scenarios, region=reg)
        assert only.names and {areas.scenario_region({s.name: s for s in scenarios}[n])
                               for n in only.names} == {reg}
        assert only.names <= sel.names


def test_sweep_prefers_the_cheapest_representative(areas):
    from types import SimpleNamespace as NS
    mk = lambda n, t="full": NS(name=n, tier=t, version="eu", phases=(), launch="", base="", phase_versions=())
    pool = [mk("eu-options-a"), mk("eu-options-b"), mk("eu-options-c")]
    assert areas.area_sweep(pool, ("eu",), tiers=("full",)) == ["eu-options-a"]          # no costs: name order
    cost = {"eu-options-a": 90.0, "eu-options-b": 3.0, "eu-options-c": 40.0}
    assert areas.area_sweep(pool, ("eu",), tiers=("full",), cost=cost) == ["eu-options-b"]
    pool.append(mk("eu-options-q", "quick"))
    assert areas.area_sweep(pool, ("eu",), tiers=("quick", "full"), cost=cost) == ["eu-options-q"]   # quick first


def test_sweep_has_no_cross_region_scenarios(areas, scenarios):
    cross = {s.name for s in scenarios if areas.scenario_region(s) == "cross"}
    assert cross
    assert set(areas.area_sweep(scenarios, ("us", "eu"))).isdisjoint(cross)   # the canary covers those


# --- the runner ----------------------------------------------------------------

def _dry(*args):
    r = subprocess.run([sys.executable, str(REPLAY / "replay_tests.py"), "--dry-run", *args],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    return r


def test_runner_changed_files_default_is_single_region(areas, scenarios):
    r = _dry("--changed-files", "port/options/cheats.c")
    listed = {l.strip() for l in r.stdout.splitlines() if l.startswith("  ") and "policy" not in l
              and ":" not in l}
    assert "eu-cheat-money" in listed and "us-cheat-money" not in listed
    assert "estimate:" in r.stderr and "by region:" in r.stderr
    r = _dry("--changed-files", "port/options/cheats.c", "--region", "both")
    assert "us-cheat-money" in r.stdout


def test_runner_canary_and_sweep_options(areas, scenarios):
    r = _dry("--canary")
    listed = {l.strip() for l in r.stdout.splitlines()[1:]}
    assert listed == areas.canary_names(scenarios)
    r = _dry("--sweep")
    listed = {l.strip() for l in r.stdout.splitlines()[1:]}
    timings = {k: v for k, v in json.loads((REPLAY / "timings.json").read_text()).items() if not k.startswith("_")}
    assert set(areas.area_sweep(scenarios, ("eu",), tiers=("quick", "full"), cost=timings)) <= listed
    assert not {n for n in listed if n.startswith("us-")} - set(areas.CANARY)


def test_runner_region_filter_on_the_plain_tiers(areas, scenarios):
    r = _dry("--region", "us")
    listed = {l.strip() for l in r.stdout.splitlines()[1:]}
    assert listed and all(n.startswith("us-") for n in listed)
    r = _dry("--region", "eu")
    listed = {l.strip() for l in r.stdout.splitlines()[1:]}
    assert listed and all(n.startswith("eu-") for n in listed)


def test_timings_file_names_real_scenarios(scenarios):
    t = json.loads((REPLAY / "timings.json").read_text())
    have = {s.name for s in scenarios}
    stale = [k for k in t if not k.startswith("_") and k not in have]
    assert not stale, stale
    assert all(isinstance(v, (int, float)) and v >= 0 for k, v in t.items() if not k.startswith("_"))


def test_broad_rule_is_thinned_unless_thorough(areas, scenarios):
    files = ["platforms/pc/src/title_overlay_art.c"]       # 285 scenarios: shared infrastructure
    sel = areas.select(files, scenarios)
    thin = areas.plan(sel, scenarios, "auto").names
    full = areas.plan(sel, scenarios, "auto", thorough=True).names
    assert len(sel.classes["langs"]) > areas.BROAD
    assert len(thin) < len(full) / 2
    assert thin <= full
    # at most REPS_PER_AREA per (area, family, region) from the broad set, the language copies excepted
    by = {s.name: s for s in scenarios}
    from collections import Counter
    c = Counter((areas.scenario_area(n), n.split("-", 2)[1], areas.scenario_region(by[n]))
                for n in thin & sel.classes["langs"] if not areas.LANG_COPY.match(n))
    assert max(c.values()) <= areas.REPS_PER_AREA + 1      # +1: the quick tier / smoke can add one
    # a narrow rule (<= BROAD scenarios) is not thinned
    sel = areas.select(["port/options/cheats.c"], scenarios)
    assert len(sel.classes["default"]) <= areas.BROAD
    p = areas.plan(sel, scenarios, "auto")
    assert {n for n in sel.classes["default"] if n.startswith("eu-")} <= p.names
