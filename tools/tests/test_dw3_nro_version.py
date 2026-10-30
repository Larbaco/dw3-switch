"""Unit tests for tools/dw3_nro_version.py (NACP 15-char display_version)."""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "dw3_nro_version.py"


def _load():
    spec = importlib.util.spec_from_file_location("dw3_nro_version", SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    sys.modules["dw3_nro_version"] = mod
    spec.loader.exec_module(mod)
    return mod


mod = _load()
NACP_VERSION_MAX = mod.NACP_VERSION_MAX
compress_preview = mod.compress_preview
format_from_github = mod.format_from_github
nacp_version = mod.nacp_version
strip_v_prefix = mod.strip_v_prefix


def test_nacp_max_matches_libnx():
    assert NACP_VERSION_MAX == 15


@pytest.mark.parametrize(
    "raw,expected",
    [
        ("v0.6.5-preview.18", "0.6.5-preview.18"),
        ("0.6.5-preview.18", "0.6.5-preview.18"),
        ("a0e4973", "a0e4973"),
    ],
)
def test_strip_v_prefix(raw, expected):
    assert strip_v_prefix(raw) == expected


def test_compress_preview_keeps_number():
    assert compress_preview("0.6.5-preview.18") == "0.6.5-p.18"


@pytest.mark.parametrize(
    "full,expected",
    [
        # Exact preview tag: compress so .18 survives (naive truncate → .1).
        ("v0.6.5-preview.18", "0.6.5-p.18"),
        ("0.6.5-preview.18", "0.6.5-p.18"),
        ("0.6.5-preview.1", "0.6.5-preview.1"),  # already ≤15
        ("0.6.5-preview.10", "0.6.5-p.10"),
        # git describe style
        ("v0.6.5-preview.10-13-ga0e4973", "0.6.5-p.10+a0e4"),
        ("0.6.5-preview.18-dirty", "0.6.5-p.18+d"),
        # short SHA CI builds
        ("a0e4973", "a0e4973"),
        ("unknown", "unknown"),
    ],
)
def test_nacp_version_fits_and_keeps_identity(full, expected):
    out = nacp_version(full)
    assert len(out) <= NACP_VERSION_MAX
    assert out == expected


def test_naive_truncate_would_lose_preview_number():
    """Document why we compress: nacptool would turn this into preview.1."""
    full = "0.6.5-preview.18"
    naive = full[:NACP_VERSION_MAX]
    assert naive == "0.6.5-preview.1"
    assert nacp_version(full) == "0.6.5-p.18"


def test_format_from_github_release_tag(monkeypatch):
    monkeypatch.setenv("RELEASE_TAG", "v0.6.5-preview.18")
    monkeypatch.delenv("GITHUB_REF", raising=False)
    monkeypatch.delenv("GITHUB_SHA", raising=False)
    assert format_from_github() == "0.6.5-preview.18"


def test_format_from_github_tag_ref(monkeypatch):
    monkeypatch.delenv("RELEASE_TAG", raising=False)
    monkeypatch.delenv("DW3_RELEASE_TAG", raising=False)
    monkeypatch.setenv("GITHUB_REF", "refs/tags/v0.6.5-preview.18")
    monkeypatch.setenv("GITHUB_REF_NAME", "v0.6.5-preview.18")
    monkeypatch.setenv("GITHUB_SHA", "a0e4973deadbeef")
    assert format_from_github() == "0.6.5-preview.18"


def test_format_from_github_pr_uses_short_sha(monkeypatch):
    monkeypatch.delenv("RELEASE_TAG", raising=False)
    monkeypatch.delenv("DW3_RELEASE_TAG", raising=False)
    monkeypatch.setenv("GITHUB_REF", "refs/pull/99/merge")
    monkeypatch.setenv("GITHUB_REF_NAME", "99/merge")
    monkeypatch.setenv("GITHUB_SHA", "a0e4973deadbeef")
    assert format_from_github() == "a0e4973"


# --- P6.37: the in-game label and the generated header ---------------------

display_label = mod.display_label
header_text = mod.header_text
write_if_changed = mod.write_if_changed


@pytest.mark.parametrize(
    "raw,sha,expected",
    [
        ("0.6.5-preview.57", "962462d", "v0.6.5-preview.57 (962462d)"),
        ("v0.6.5-preview.57", "962462d", "v0.6.5-preview.57 (962462d)"),
        ("0.6.5-preview.57-dirty", "962462d", "v0.6.5-preview.57-dirty (962462d)"),
        ("0.6.5-preview.57", "", "v0.6.5-preview.57"),
        ("v0.6.5-preview.56-12-g8574e58", "8574e58", "v0.6.5-preview.56-12-g8574e58"),
        ("0.6.5-preview.56-12-g8574e58-dirty", "8574e58", "v0.6.5-preview.56-12-g8574e58-dirty"),
        ("8574e58", "8574e58", "8574e58"),
        ("8574e58-dirty", "8574e58", "8574e58-dirty"),
        ("unknown", "", "unknown"),
        ("", "", "unknown"),
    ],
)
def test_display_label(raw, sha, expected):
    assert display_label(raw, sha) == expected


def test_header_text_defines_and_escapes():
    text = header_text('1.0 "x"', "abc1234", "2026-10-30")
    assert '#define DW3_VERSION_RAW "1.0 \\"x\\""' in text
    assert '#define DW3_VERSION_SHA "abc1234"' in text
    assert '#define DW3_VERSION_BUILD_DATE "2026-10-30"' in text
    assert "#define DW3_VERSION_LABEL" in text


def test_write_header_only_when_changed(tmp_path):
    path = tmp_path / "gen" / "dw3_version_gen.h"
    assert write_if_changed(path, "a\n") is True
    mtime = path.stat().st_mtime_ns
    assert write_if_changed(path, "a\n") is False
    assert path.stat().st_mtime_ns == mtime
    assert write_if_changed(path, "b\n") is True


def test_cli_label_release_tag(monkeypatch, capsys):
    monkeypatch.setenv("RELEASE_TAG", "v0.6.5-preview.57")
    monkeypatch.setenv("GITHUB_SHA", "962462dabcdef")
    monkeypatch.delenv("DW3_NRO_VERSION", raising=False)
    assert mod.main(["--auto", "--format", "label"]) == 0
    assert capsys.readouterr().out.strip() == "dw3-switch v0.6.5-preview.57 (962462d)"
