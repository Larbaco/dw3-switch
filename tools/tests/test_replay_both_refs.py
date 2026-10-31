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


def test_gameplay_has_four_rows_on_both_binaries():
    # D1: Region at start moved from Gameplay to Saves, so Gameplay is the same everywhere
    fo = manifest.by_name()["us-field-options"]
    assert any(r"gameplay \(4 row" in r for p, r in fo.expect_log)
    assert not fo.expect_log_both


def test_saves_region_row_expected_on_both_binary_only():
    for v in ("us", "eu"):
        sv = manifest.by_name()[f"{v}-options-saves"]
        assert any(r"saves \(2 row" in r for p, r in sv.expect_log)
        assert sv.expect_log_both
        assert all(r"saves \(3 row" in r for p, r in sv.expect_log_both if "saves" in r and "row" in r)
        assert [r.replace(r"saves \(2 row", r"saves \(3 row") for p, r in sv.expect_log] == \
               [r for p, r in sv.expect_log_both]
