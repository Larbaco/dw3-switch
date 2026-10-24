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
    # 1.5x and 2x: 27 and 20 footsteps for the same countdown.
    assert re.search(r"random battle after 27 footstep\(s\) \(encounter rate 1\.5x\)", err)
    assert re.search(r"random battle after 20 footstep\(s\) \(encounter rate 2x\)", err)
    assert "dw3-pacing: map-forced area battle on the first footstep" in err
    # Text: two pages, three and two frames, once each.
    pages = re.findall(r"text page typed in (\d+) frame\(s\) \(text speed (\w+)\)", err)
    assert pages[:2] == [("3", "fast"), ("2", "fast")]
    assert pages[2:] == [("31", "instant"), ("23", "instant"), ("1", "instant"), ("2", "instant")]


def test_scroll_hold_log_lines(pacing_bin: Path):
    err = subprocess.run([str(pacing_bin)], capture_output=True, text=True).stderr
    # Instant, a text that scrolls: one line per scroll, after the 12 frame hold.
    assert "dw3-pacing: text scroll 1 after a hold of 12 frame(s) (text speed instant)" in err
    assert "dw3-pacing: text scroll 2 after a hold of 12 frame(s) (text speed instant)" in err
    assert "text scroll 3" not in err


def test_slow_instant_page_diagnostic(pacing_bin: Path):
    err = subprocess.run([str(pacing_bin)], capture_output=True, text=True).stderr
    # 31 typed frames and no scroll: flagged once; the page held for a scroll is not.
    assert err.count("slow Instant page with no scroll hold (31 frames)") == 1
    assert err.count("slow Instant page") == 1


def test_banner_on_a_dialogue_address_is_not_a_page(pacing_bin: Path):
    """The rc6 OLED log: 'typed in 103 frame(s)' / 'slow Instant page' after every
    battle were the area name banner (delay 5, 17 characters at 6 frames, +1)
    on the address of a freed dialogue window, not a report page."""
    err = subprocess.run([str(pacing_bin)], capture_output=True, text=True).stderr
    assert "typed in 103 frame" not in err
    assert err.count("dw3-pacing: text typed at delay 5 is not a dialogue page") == 2  # the early probe and the banner
    # Normal leaves the banner alone; Instant shows its names whole.
    assert err.count("(area name banner): text speed leaves it alone") == 1
    assert err.count("(area name banner): shown whole (text speed instant)") == 1
