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
