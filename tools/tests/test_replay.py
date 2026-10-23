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
    seen = set()
    for sc in rt.SCENARIOS:
        assert sc.name not in seen, sc.name
        seen.add(sc.name)
        assert sc.version in ("us", "eu")
        assert sc.tier in ("quick", "full", "soak", "repro")
        for phase in sc.phases:
            assert (REPLAY / "scenarios" / f"{phase}.rec").is_file(), phase
        # P6.18: a log-only scenario has no checkpoints (and then needs a check
        # in the log); every other one needs at least one.
        assert bool(rt.shot_names(sc.name)) != sc.log_only, sc.name
        if sc.log_only:
            assert sc.expect_log or sc.forbid_log, sc.name
    # every committed recording belongs to a scenario phase
    phases = {p for sc in rt.SCENARIOS for p in sc.phases}
    assert {p.stem for p in (REPLAY / "scenarios").glob("*.rec")} == phases


def test_gameplay_scenarios_cover_both_discs():
    rt = _load("replay_tests")
    for kind in ("newgame-field", "field-walk", "soak"):
        for v in ("us", "eu"):
            assert any(sc.name == f"{v}-{kind}" for sc in rt.SCENARIOS), (v, kind)
    # --quick (the old behaviour) stays the title-only set
    quick = {sc.name for sc in rt.SCENARIOS if sc.tier == "quick"}
    assert quick == {"us-title"} | {f"eu-lang-{l}" for l in ("en", "fr", "de", "it", "es")}


def test_recordings_end_after_their_last_checkpoint():
    for rec in (REPLAY / "scenarios").glob("*.rec"):
        shots, end = [], 0
        for line in rec.read_text().splitlines():
            f = line.split("#", 1)[0].split()
            if len(f) == 3 and f[0] == "shot":
                shots.append(int(f[1]))
            elif len(f) == 2 and f[0] == "end":
                end = int(f[1])
        assert end > 0, rec.name
        assert not shots or max(shots) < end, rec.name


def test_rss_verdict():
    rt = _load("replay_tests")
    flat = [(f, 200000 + (f % 3) * 100) for f in range(0, 4000, 100)]
    assert rt.rss_verdict(flat, 0)[0]
    leak = [(f, 200000 + f * 100) for f in range(0, 4000, 100)]   # +39 MiB per 1000 frames
    ok, detail = rt.rss_verdict(leak, 0)
    assert not ok and "monotonic=True" in detail
    noisy = [(f, 200000 + (1 if (f // 100) % 2 else -1) * 90000) for f in range(0, 4000, 100)]
    assert rt.rss_verdict(noisy, 0)[0]      # big swings that do not climb are fine
    assert rt.rss_verdict(leak[:3], 0)[0]   # too short to judge: reported, not failed


def test_check_restores():
    rt = _load("replay_tests")
    log = ("dw3: overlay restore file=0x1c1 kind=mode data=0\n"
           "dw3: overlay restore file=0x158 kind=mode data=1\n"
           "dw3: overlay restore file=0x1da kind=sub data=1\n")
    assert rt.check_restores(log, ((0x1C1, "mode"), (0x1DA, "sub"))) == []
    assert rt.check_restores(log, ((0x1DA, "sub"), (0x1C1, "mode")))      # wrong order
    assert rt.check_restores(log, ((0x999, "sub"),))                       # missing
    bad = log + "dw3: overlay restore file=0x7 kind=unknown UNKNOWN\n"
    assert any("unknown" in p for p in rt.check_restores(bad, ()))


STUB = """#!/bin/sh
# stub dw3-pc: behaviour chosen by $STUB_MODE
echo "dw3-replay: frame 600"
case "$STUB_MODE" in
  ok)      echo "dw3-replay: end at frame 900"; exit 0;;
  noend)   exit 0;;
  exit3)   exit 3;;
  segv)    kill -SEGV $$;;
  hang)    sleep 30;;
  memcard) touch "$DW3_MEMCARD_DIR/phase-$STUB_PHASE"; ls "$DW3_MEMCARD_DIR" | tr '\\n' ' '; echo "dw3-replay: end at frame 900"; exit 0;;
esac
"""


def _stub(tmp_path):
    p = tmp_path / "dw3-pc"
    p.write_text(STUB)
    p.chmod(0o755)
    return p


@pytest.mark.parametrize("mode,verdict", [("ok", "OK"), ("noend", "NOEND"),
                                          ("exit3", "EXIT"), ("segv", "CRASH"),
                                          ("hang", "HANG")])
def test_process_verdicts(tmp_path, monkeypatch, mode, verdict):
    rt = _load("replay_tests")
    monkeypatch.setenv("STUB_MODE", mode)
    rec = tmp_path / "x.rec"
    rec.write_text("shot 5 a\nend 9\n")
    r = rt.run_phase(rec, "us", _stub(tmp_path), tmp_path, ":99", tmp_path / "out",
                     tmp_path / "mc", "stamp", "t", timeout=60,
                     stall=2 if mode == "hang" else 60, turbo=0)
    assert r["verdict"] == verdict, r
    if verdict == "CRASH":
        assert "SIGSEGV" in r["detail"]
    if verdict == "HANG":
        assert "no game-frame progress" in r["detail"] and r["secs"] < 20
    assert r["frame"] >= 600 or verdict == "HANG"


def test_phases_share_one_memcard_dir(tmp_path, monkeypatch):
    rt = _load("replay_tests")
    man = _load("manifest")
    monkeypatch.setenv("STUB_MODE", "memcard")
    (tmp_path / "p1.rec").write_text("shot 5 a\nend 9\n")
    (tmp_path / "p2.rec").write_text("shot 5 b\nend 9\n")
    monkeypatch.setattr(rt, "SCEN_DIR", tmp_path)
    sc = man.Scenario("fake", "us", "full", phases=("p1", "p2"))
    monkeypatch.setattr(rt, "start_xvfb",
                        lambda work: (type("P", (), {"terminate": lambda s: None,
                                                      "wait": lambda s, t=None: 0})(), ":99"))
    seen = []
    orig = rt.run_phase

    def spy(*a, **k):
        seen.append(a[6])  # memdir
        monkeypatch.setenv("STUB_PHASE", str(len(seen)))
        return orig(*a, **k)

    monkeypatch.setattr(rt, "run_phase", spy)
    results, _ = rt.run_scenario(sc, _stub(tmp_path), tmp_path, tmp_path / "o", "s", "fake", 60)
    assert [r["verdict"] for r in results] == ["OK", "OK"]
    assert seen[0] == seen[1]                      # the same directory
    text = results[1]["log"].read_text()
    assert "phase-1" in text                       # phase 2 saw phase 1's file


def test_second_phase_skipped_after_failure(tmp_path, monkeypatch):
    rt = _load("replay_tests")
    man = _load("manifest")
    monkeypatch.setenv("STUB_MODE", "exit3")
    (tmp_path / "p1.rec").write_text("shot 5 a\nend 9\n")
    (tmp_path / "p2.rec").write_text("shot 5 b\nend 9\n")
    monkeypatch.setattr(rt, "SCEN_DIR", tmp_path)
    sc = man.Scenario("fake", "us", "full", phases=("p1", "p2"))
    monkeypatch.setattr(rt, "start_xvfb",
                        lambda work: (type("P", (), {"terminate": lambda s: None,
                                                      "wait": lambda s, t=None: 0})(), ":99"))
    results, _ = rt.run_scenario(sc, _stub(tmp_path), tmp_path, tmp_path / "o", "s", "fake", 60)
    assert len(results) == 1 and results[0]["verdict"] == "EXIT"


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



def test_log_expectations_and_seeded_option_file(tmp_path):
    rt = _load("replay_tests")
    text = "a dw3-pacing: random battle after 102 footstep(s) b\nFIGHTSTG_start\n"
    assert rt.check_log_expect(((0, r"random battle after 1\d\d"),), 0, text) == []
    assert rt.check_log_expect(((0, r"after 2\d\d"),), 0, text) != []
    # forbid_log: a match is a problem, a miss is fine; other phases are ignored
    assert rt.check_log_expect((), 0, text, ((0, r"FIGHTSTG_start"),)) != []
    assert rt.check_log_expect((), 0, text, ((0, r"encounters off"),)) == []
    assert rt.check_log_expect((), 0, text, ((1, r"FIGHTSTG_start"),)) == []
    # the seeded port_options.cfg is the P9.4 format and ends with end=1
    rt.write_seed_cfg(tmp_path, (("encounter_rate", "2"), ("text_speed", "1")))
    lines = (tmp_path / "port_options.cfg").read_text().splitlines()
    assert lines[1:] == ["version=1", "encounter_rate=2", "text_speed=1", "end=1"]


def test_pacing_scenarios_are_defined_for_both_discs():
    rt = _load("replay_tests")
    names = {sc.name: sc for sc in rt.SCENARIOS}
    for v in ("us", "eu"):
        for n in ("encounter-1x", "encounter-half", "encounter-off", "encounter-off-event",
                  "text-normal", "text-fast", "text-instant", "pacing-persist"):
            assert f"{v}-{n}" in names, (v, n)
        # the encounter ones need host env (PC only); the text ones do not
        assert names[f"{v}-encounter-off"].env and not names[f"{v}-text-fast"].env
        assert dict(names[f"{v}-encounter-off"].cfg) == {"encounter_rate": "2"}
        assert dict(names[f"{v}-text-instant"].cfg) == {"text_speed": "2"}
        # Off must forbid any battle
        assert any("FIGHTSTG_start" in rx for _, rx in names[f"{v}-encounter-off"].forbid_log)


def test_items_screen_scenarios_cover_both_discs_and_both_text_speeds():
    """Patch 0069: the Items list after a battle, Instant (the owner's setting)
    and Normal, on both discs, one recording per disc."""
    rt = _load("replay_tests")
    names = {sc.name: sc for sc in rt.SCENARIOS}
    status = {"us": 0x1F9, "eu": 0x207}
    for v in ("us", "eu"):
        a, b = names[f"{v}-items-screen"], names[f"{v}-items-screen-normal"]
        assert a.phases == b.phases == (f"{v}-items-screen",)
        assert dict(a.cfg) == {"text_speed": "2"} and dict(b.cfg) == {"text_speed": "0"}
        for sc in (a, b):
            assert sc.log_only and dict(sc.env)["DW3_FORCE_ITEMS"] and sc.expect_log, sc.name
            # the field menu's screens overlay is restored after the battle's
            assert sc.expect_restore[-1] == (status[v], "mode"), sc.name
            assert "patch 0069" in sc.expect_log[0][1], sc.name


def test_p77_scenarios_are_defined_for_both_discs():
    rt = _load("replay_tests")
    names = {sc.name: sc for sc in rt.SCENARIOS}
    for v in ("us", "eu"):
        for n in ("claw-ko", "claw-alive", "ko-no-claw", "hud-rookie", "hud-form", "hud-long",
                  "hud-nickname", "square-field"):
            sc = names[f"{v}-p77-{n}"]
            assert sc.log_only and sc.env and sc.expect_log, sc.name   # PC-only, log verdicts
    for lang in ("fr", "it", "de", "es"):
        assert names[f"eu-p77-hud-form-{lang}"].env
    # the control scenarios must forbid what the fixed ones expect
    assert any("knocked-out" in rx for _, rx in names["us-p77-claw-alive"].forbid_log)
    assert any("steal item" in rx for _, rx in names["eu-p77-ko-no-claw"].forbid_log)


def test_every_manifest_env_variable_is_scrubbed_by_the_runner():
    # a DW3_* variable of the user's shell must not leak into a scenario that
    # does not set it: the runner pops each key it knows before it applies the
    # scenario's own env
    rt = _load("replay_tests")
    src = (REPLAY / "replay_tests.py").read_text()
    for sc in rt.SCENARIOS:
        for key, _ in sc.env:
            assert f'"{key}"' in src, (sc.name, key)


def test_p77_claw_expectation_needs_the_steal_before_the_end_of_the_battle(tmp_path):
    rt = _load("replay_tests")
    sc = {s.name: s for s in rt.SCENARIOS}["us-p77-claw-ko"]
    ok = ("dw3: DW3_FORCE_TECH partner uses tech 127 (menu picked 53)\n"
          "dw3-p77: claw steal on a knocked-out enemy (tech 127)\n"
          "dw3-p77: steal item 1 from enemy slot 0 (hp 0)\n"
          "dw3: WFIGHTMN_endBattle result=1 requestMode=0x1400 fieldMode=0x2d7\n")
    assert rt.check_log_expect(sc.expect_log, 0, ok, sc.forbid_log) == []
    lines = ok.splitlines(True)
    swapped = "".join([lines[0], lines[3], lines[1], lines[2]])   # the battle ends first
    assert rt.check_log_expect(sc.expect_log, 0, swapped, sc.forbid_log) != []
