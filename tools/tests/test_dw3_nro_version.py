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


@pytest.mark.parametrize(
    "full,expected",
    [
        # SemVer series (owner decision 2026-10-31)
        ("v0.9.0-preview.1", "0.9.0-preview.1"),  # 15, fits as is
        ("v0.9.0-preview.9", "0.9.0-preview.9"),
        ("v0.9.0-preview.12", "0.9.0-p.12"),
        ("0.9.0-preview.123", "0.9.0-p.123"),
        ("v1.0.0-rc.3", "1.0.0-rc.3"),
        ("v1.0.0-rc.12", "1.0.0-rc.12"),
        ("v0.9.0", "0.9.0"),
        ("v1.0.0", "1.0.0"),
        # git describe past each kind of tag
        ("v0.9.0-preview.12-3-gabcdef1", "0.9.0-p.12+abcd"),
        ("v1.0.0-rc.3-13-ga0e4973", "1.0.0-rc.3+a0e4"),
        ("v0.9.0-13-ga0e4973", "0.9.0+a0e4973"),
        ("v1.0.0-5-ga0e4973", "1.0.0+a0e4973"),
        # dirty trees
        ("0.9.0-preview.12-dirty", "0.9.0-p.12+d"),
        ("1.0.0-rc.3-dirty", "1.0.0-rc.3+d"),
        ("0.9.0-dirty", "0.9.0+d"),
        # a long preview number with a describe: the version prefix goes first
        ("0.9.0-preview.100-3-gabcdef1", "p.100+abcdef1"),
    ],
)
def test_nacp_version_semver_series(full, expected):
    out = nacp_version(full)
    assert len(out) <= NACP_VERSION_MAX
    assert out == expected


def test_nacp_version_never_exceeds_the_cap():
    for major in (0, 1, 10):
        for kind in ("preview", "rc"):
            for n in (1, 9, 10, 99, 100, 1234):
                for tail in ("", "-3-gabcdef1", "-dirty", "-30-gabcdef1-dirty"):
                    full = f"v{major}.9.0-{kind}.{n}{tail}"
                    assert len(nacp_version(full)) <= NACP_VERSION_MAX, full


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


@pytest.mark.parametrize(
    "tag,expected",
    [
        ("v0.9.0-preview.1", "0.9.0-preview.1"),
        ("v1.0.0-rc.2", "1.0.0-rc.2"),
        ("v0.9.0", "0.9.0"),
        ("v1.0.0", "1.0.0"),
    ],
)
def test_format_from_github_new_tags(monkeypatch, tag, expected):
    monkeypatch.setenv("RELEASE_TAG", tag)
    monkeypatch.delenv("GITHUB_REF", raising=False)
    monkeypatch.delenv("GITHUB_SHA", raising=False)
    assert format_from_github() == expected


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
        # SemVer series
        ("0.9.0-preview.1", "abc1234", "v0.9.0-preview.1 (abc1234)"),
        ("v1.0.0-rc.2", "abc1234", "v1.0.0-rc.2 (abc1234)"),
        ("0.9.0", "abc1234", "v0.9.0 (abc1234)"),
        ("1.0.0-dirty", "abc1234", "v1.0.0-dirty (abc1234)"),
        ("0.9.0-preview.1-4-gabc1234", "abc1234", "v0.9.0-preview.1-4-gabc1234"),
        ("1.0.0-rc.2-4-gabc1234-dirty", "abc1234", "v1.0.0-rc.2-4-gabc1234-dirty"),
        ("0.9.0-13-gabc1234", "abc1234", "v0.9.0-13-gabc1234"),
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


# --- release tags: what the release workflow publishes -----------------------

release_info = mod.release_info


@pytest.mark.parametrize(
    "tag,kind,pre,latest,title",
    [
        ("v0.9.0-preview.1", "preview", "true", "false", "dw3-switch v0.9.0-preview.1 preview"),
        ("v0.9.0-preview.12", "preview", "true", "false", "dw3-switch v0.9.0-preview.12 preview"),
        ("v1.0.0-rc.1", "rc", "true", "false", "dw3-switch v1.0.0-rc.1 release candidate"),
        ("v0.9.0", "release", "false", "true", "dw3-switch v0.9.0 public beta"),
        ("v1.0.0", "release", "false", "true", "dw3-switch v1.0.0"),
        ("v1.2.3", "release", "false", "true", "dw3-switch v1.2.3"),
        # the old series stays valid for history and re-runs
        ("v0.6.5-preview.57", "preview", "true", "false", "dw3-switch v0.6.5-preview.57 preview"),
        ("v0.6.5-preview.1", "preview", "true", "false", "dw3-switch v0.6.5-preview.1 preview"),
    ],
)
def test_release_info(tag, kind, pre, latest, title):
    assert release_info(tag) == {
        "kind": kind,
        "prerelease": pre,
        "latest": latest,
        "title": title,
    }


@pytest.mark.parametrize(
    "tag",
    [
        "",
        "v1",
        "v1.0",
        "0.9.0",  # the v is required
        "v0.9.0-preview",
        "v0.9.0-preview.",
        "v0.9.0-preview.0",
        "v0.9.0-preview.x",
        "v0.9.0-beta.1",
        "v1.0.0-rc",
        "v1.0.0-rc.1.2",
        "v1.0.0+build",
        "v01.0.0",
        "v1.0.0-preview.1-dirty",
        "vfoo",
        "main",
    ],
)
def test_release_info_rejects(tag):
    assert release_info(tag) is None


def test_cli_release_kind_prerelease(capsys):
    assert mod.main(["--release-kind", "v0.9.0-preview.1"]) == 0
    out = dict(l.split("=", 1) for l in capsys.readouterr().out.splitlines())
    assert out["RELEASE_KIND"] == "preview"
    assert out["RELEASE_PRERELEASE"] == "true"
    assert out["RELEASE_LATEST"] == "false"
    assert out["RELEASE_TITLE"] == "dw3-switch v0.9.0-preview.1 preview"


def test_cli_release_kind_normal_release(capsys):
    assert mod.main(["--release-kind", "v0.9.0"]) == 0
    out = dict(l.split("=", 1) for l in capsys.readouterr().out.splitlines())
    assert out["RELEASE_PRERELEASE"] == "false"
    assert out["RELEASE_LATEST"] == "true"


def test_cli_release_kind_rejects_a_bad_tag(capsys):
    assert mod.main(["--release-kind", "v0.9"]) == 1
    captured = capsys.readouterr()
    assert captured.out == ""
    assert "v0.9.0-preview.1" in captured.err


def test_cli_label_new_series(monkeypatch, capsys):
    monkeypatch.setenv("RELEASE_TAG", "v0.9.0-preview.1")
    monkeypatch.setenv("GITHUB_SHA", "962462dabcdef")
    monkeypatch.delenv("DW3_NRO_VERSION", raising=False)
    assert mod.main(["--auto", "--format", "label"]) == 0
    assert capsys.readouterr().out.strip() == "dw3-switch v0.9.0-preview.1 (962462d)"
    assert mod.main(["--auto", "--format", "nacp"]) == 0
    assert capsys.readouterr().out.strip() == "0.9.0-preview.1"


def test_cli_nacp_for_a_final_release(monkeypatch, capsys):
    monkeypatch.setenv("RELEASE_TAG", "v1.0.0")
    monkeypatch.setenv("GITHUB_SHA", "962462dabcdef")
    monkeypatch.delenv("DW3_NRO_VERSION", raising=False)
    assert mod.main(["--auto", "--format", "nacp"]) == 0
    assert capsys.readouterr().out.strip() == "1.0.0"
    assert mod.main(["--auto", "--format", "label"]) == 0
    assert capsys.readouterr().out.strip() == "dw3-switch v1.0.0 (962462d)"
