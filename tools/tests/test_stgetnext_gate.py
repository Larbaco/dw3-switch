"""Switch StGetNext empty-ring gate. The gate has no libnx dependency, so gcc
can compile it against the host model in stgetnext_gate_main.c.

The harness runs every scenario twice over the same starting ring -- ungated and
gated -- and prints ``same_<name>=1`` only when the return values, the read
cursor and the ring bytes match exactly. ``visits_empty_*`` is the scan-cost
metric the gate is meant to cut.
"""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope="module")
def gate_output(tmp_path_factory) -> dict[str, str]:
    out = tmp_path_factory.mktemp("stgetnext_gate") / "stgetnext_gate"
    cmd = [
        "gcc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-o",
        str(out),
        str(ROOT / "tools" / "tests" / "stgetnext_gate_main.c"),
        str(ROOT / "tools/tests/stgetnext_api_stubs.c"),
        str(ROOT / "platforms/switch/psyz_backend/stgetnext_invalidate.c"),
        "-D__psyz",
        f"-I{ROOT / 'vendor/psyz/psyz/include'}",
        "-Wl,--wrap=CdReady", "-Wl,--wrap=StSetRing",
        "-Wl,--wrap=StClearRing", "-Wl,--wrap=data_ready_callback",
        f"-I{ROOT / 'platforms' / 'switch' / 'psyz_backend'}",
    ]
    build = subprocess.run(cmd, capture_output=True, text=True)
    assert build.returncode == 0, build.stderr + build.stdout
    run = subprocess.run([str(out)], capture_output=True, text=True)
    assert run.returncode == 0, run.stderr + run.stdout
    parsed: dict[str, str] = {}
    for line in run.stdout.splitlines():
        key, _, value = line.partition("=")
        parsed[key.strip()] = value.strip()
    return parsed


def test_gate_preserves_wrapper_behaviour(gate_output):
    # Every scenario, plus the deterministic stress run, must be identical to
    # the ungated wrapper in return value, cursor and ring contents.
    for name in (
        "same_empty_polls",
        "same_ready_at_cursor",
        "same_body_same_head",
        "same_resync_to_head",
        "same_orphan_drop",
        "same_sentinel_at_cursor",
        "same_sentinel_no_endframe",
        "same_scan_finds_ready",
        "same_wrap_sentinel",
        "same_publish_invalidates",
        "same_stfree_reposition",
        "same_ring_reset",
        "same_random",
        "same_reset_publish_aba",
        "same_early_wrap_aba",
        "same_same_address_reuse",
    ):
        assert gate_output.get(name) == "1", name


def test_gate_cuts_empty_poll_scan(gate_output):
    assert gate_output["gate_cuts_scan_empty_polls"] == "1"
    ungated = int(gate_output["visits_empty_ungated"])
    gated = int(gate_output["visits_empty_gated"])
    # 64 polls of a 32-slot ring: 64 * 31 slot comparisons ungated, and only the
    # first poll's 31 gated.
    assert ungated == 64 * 31
    assert gated == 31


def test_gate_guard_rejects_unproven_states(gate_output):
    # A ring at or below DW3_STGETNEXT_RING_MIN, a moved cursor, or a different
    # ring must all miss, so the full scan still runs.
    assert gate_output["guard_rejects_small_ring"] == "1"
    assert gate_output["guard_allows_bigger_ring"] == "1"
    assert gate_output["guard_rejects_idx1_move"] == "1"
    assert gate_output["guard_rejects_idx3_move"] == "1"
    assert gate_output["guard_rejects_other_ring"] == "1"


def test_production_hook_invalidates_before_producer_attempt(gate_output):
    assert gate_output["producer_attempt_invalidates"] == "1"
