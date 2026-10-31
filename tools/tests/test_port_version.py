"""P6.37: port/version/port_version.c (the label the game shows and logs) built
with the header tools/dw3_nro_version.py writes. Needs vendor/psyz headers."""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "port" / "version" / "port_version.c"
MAIN = ROOT / "tools" / "tests" / "port_version_main.c"
PSYZ_INC = ROOT / "vendor" / "psyz" / "psyz" / "include"
SCRIPT = ROOT / "tools" / "dw3_nro_version.py"
VERSION = "v0.9.0-preview.1"


# Version inputs the header script reads from the environment. CI sets some of
# them (GITHUB_SHA, GITHUB_REF, ...), and they would add a sha to the label or
# override the tag the test passes.
_VERSION_ENV = ("GITHUB_SHA", "GITHUB_REF", "GITHUB_REF_NAME", "RELEASE_TAG",
                "DW3_RELEASE_TAG", "DW3_NRO_VERSION", "SOURCE_DATE_EPOCH")


def hermetic_env() -> dict[str, str]:
    return {k: v for k, v in os.environ.items() if k not in _VERSION_ENV}


def build(tmp: Path, *defs: str, version: str | None = None) -> Path:
    if not (PSYZ_INC / "psyz.h").exists():
        pytest.skip("vendor/psyz is not checked out")
    gen = tmp / "gen"
    gen.mkdir(exist_ok=True)
    subprocess.run(
        [sys.executable, str(SCRIPT), "--version", version or VERSION, "--repo", str(tmp),
         "--write-header", str(gen / "dw3_version_gen.h"), "--format", "full"],
        check=True, capture_output=True, text=True, env=hermetic_env())
    out = tmp / ("pv" + "".join(d.replace("=", "") for d in defs))
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O1", "-o", str(out),
           str(MAIN), str(SRC), *defs, f"-I{ROOT / 'port' / 'include'}",
           f"-I{ROOT / 'port' / 'region'}", f"-I{gen}", f"-I{PSYZ_INC}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr + r.stdout
    return out


def run(binary: Path, **env) -> dict[str, str]:
    e = {k: v for k, v in os.environ.items() if k != "DW3_VERSION_OVERRIDE"}
    e.update(env)
    r = subprocess.run([str(binary)], capture_output=True, text=True, env=e)
    assert r.returncode == 0, r.stderr
    return dict(l.split("=", 1) for l in r.stdout.splitlines() if "=" in l)


def test_us_label_region_and_log_line(tmp_path):
    out = run(build(tmp_path, "-DVERSION_US=1"))
    # no git sha in a fresh temp dir: the tag alone
    assert out["text"].startswith("dw3-switch v0.9.0-preview.1")
    assert out["text"].endswith(" US")
    assert out["line"].startswith("dw3-version: dw3-switch v0.9.0-preview.1")
    assert " US build=" in out["line"]
    assert out["logged"].startswith("1 ")  # announce twice, logged once
    assert "dw3-pc - dw3-switch v0.9.0-preview.1" in out["logged"]


def test_short_text_for_the_title(tmp_path):
    out = run(build(tmp_path, "-DVERSION_US=1"))
    assert out["short"] == "v0.9.0-p.1 US"
    out = run(build(tmp_path, "-DVERSION_EU=1", version="0.6.5-preview.57"))
    assert out["short"] == "v0.6.5-p.57 EU"
    out = run(build(tmp_path, "-DVERSION_US=1"), DW3_VERSION_OVERRIDE="replay")
    assert out["short"] == "replay US"


def test_eu_region_tag(tmp_path):
    out = run(build(tmp_path, "-DVERSION_EU=1"))
    assert out["text"].endswith(" EU")


def test_override_replaces_the_label(tmp_path):
    out = run(build(tmp_path, "-DVERSION_US=1"), DW3_VERSION_OVERRIDE="replay")
    assert out["text"] == "dw3-switch replay US"


def test_both_image_announces_again_once_the_region_is_picked(tmp_path):
    out = run(build(tmp_path, "-DDW3_BOTH=1"))
    assert out["text"].endswith(" US+EU")
    assert out["picked"].startswith("dw3-switch v0.9.0-preview.1") and " EU logged=2" in out["picked"]


@pytest.mark.parametrize("tag", ["v0.6.5-preview.57", "v1.0.0-rc.2", "v0.9.0"])
def test_label_for_the_other_tag_forms(tmp_path, tag):
    """The old series, a release candidate and a plain release all show as
    `dw3-switch <tag> <REGION>`."""
    out = run(build(tmp_path, "-DVERSION_US=1", version=tag))
    assert out["text"] == f"dw3-switch {tag} US"
