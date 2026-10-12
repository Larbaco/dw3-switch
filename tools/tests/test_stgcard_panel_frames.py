"""STGMCARD panel duration override (port patch 0043).

Flat MemCard* finishes in ~80 sector-frames; PS1 panel used ~1240.
Option B: PORT_HOST defines both constants as 100.
"""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PATCH = ROOT / "port" / "patches" / "0043-PORT_HOST-stgcard-panel-frames.patch"
HEADER = ROOT / "vendor" / "dw3_decomp" / "include" / "menus" / "memory_card.h"
APPLY = ROOT / "tools" / "apply_patches.sh"


def test_unpatched_us_constants_remain_ps1() -> None:
    text = HEADER.read_text(encoding="utf-8")
    assert "#define MEMCARD_LOAD_FRAMES 1239" in text
    assert "#define MEMCARD_SAVE_FRAMES 1266" in text
    assert "PORT_HOST" not in text or "#if PORT_HOST" not in text


def test_patch_sets_host_frames_to_100() -> None:
    text = PATCH.read_text(encoding="utf-8")
    assert "Upstream-bound: no" in text
    assert re.search(r"\+#if PORT_HOST", text)
    assert re.search(r"\+#define MEMCARD_LOAD_FRAMES 100", text)
    assert re.search(r"\+#define MEMCARD_SAVE_FRAMES 100", text)
    # Only the phase-1 duration macros change; no saves.c / panel.c hunks.
    assert "saves.c" not in text
    assert "panel.c" not in text


def test_apply_patches_check_accepts_series() -> None:
    r = subprocess.run(
        [str(APPLY), "--check"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr + r.stdout
