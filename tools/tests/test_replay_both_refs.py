"""P6.51: per-binary replay overrides (<refs>/<scenario>/both/) and both-only log lines."""
import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RUNNER_DIR = ROOT / "tools" / "replay"
sys.path.insert(0, str(RUNNER_DIR))
import manifest  # noqa: E402

spec = importlib.util.spec_from_file_location("replay_tests", RUNNER_DIR / "replay_tests.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


def test_plain_run_uses_scenario_refs(tmp_path):
    p = runner.reference_path(tmp_path, "us-field-options", "", "options-open", "png",
                              update=False, both=False)
    assert p == tmp_path / "us-field-options" / "options-open.png"


def test_both_run_prefers_both_override_when_present(tmp_path):
    own = tmp_path / "us-field-options" / "both" / "options-open.png"
    own.parent.mkdir(parents=True)
    own.write_bytes(b"x")
    p = runner.reference_path(tmp_path, "us-field-options", "", "options-open", "png",
                              update=False, both=True)
    assert p == own


def test_both_run_without_override_falls_back_to_plain(tmp_path):
    p = runner.reference_path(tmp_path, "us-field-options", "", "options-open", "png",
                              update=False, both=True)
    assert p == tmp_path / "us-field-options" / "options-open.png"


def test_both_update_writes_override_and_leaves_plain_alone(tmp_path):
    p = runner.reference_path(tmp_path, "us-field-options", "", "options-open", "png",
                              update=True, both=True)
    assert p == tmp_path / "us-field-options" / "both" / "options-open.png"


def test_ref_from_is_still_honoured_on_plain_runs(tmp_path):
    p = runner.reference_path(tmp_path, "us-nintendo-menu", "us-field-options",
                              "options-open", "png", update=False, both=False)
    assert p == tmp_path / "us-field-options" / "options-open.png"


def test_gameplay_region_row_expected_on_both_binary_only():
    fo = manifest.by_name()["us-field-options"]
    plain = [r for p, r in fo.expect_log if "gameplay" in r]
    both = [r for p, r in fo.expect_log_both if "gameplay" in r]
    assert any("(5 row" in r for r in plain)
    assert not any("(5 row" in r for r in both)
    assert any("(6 row" in r for r in both)
    # every other line is unchanged
    assert [r for p, r in fo.expect_log if "gameplay" not in r] == \
           [r for p, r in fo.expect_log_both if "gameplay" not in r]


def test_nintendo_menu_inherits_both_expectation():
    nm = manifest.by_name()["us-nintendo-menu"]
    assert any("(6 row" in r for p, r in nm.expect_log_both)
    assert not any("(5 row" in r for p, r in nm.expect_log_both if "gameplay" in r)
