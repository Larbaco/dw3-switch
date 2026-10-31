#!/usr/bin/env python3
"""Compute Switch NRO / NACP version strings for hbmenu and boot logs.

libnx NacpStruct.display_version is char[0x10] (15 printable chars + NUL).
nacptool truncates longer strings, so a naive `0.6.5-preview.18` becomes
`0.6.5-preview.1` and loses the preview number. This helper compresses first.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import time
from pathlib import Path

# display_version[0x10] in libnx nacp.h — leave room for the NUL.
NACP_VERSION_MAX = 15

_GIT_DESCRIBE_RE = re.compile(
    r"^(?P<tag>.+?)-(?P<n>\d+)-g(?P<sha>[0-9a-f]+)(?P<dirty>-dirty)?$"
)


def strip_v_prefix(s: str) -> str:
    if s.startswith("v") and len(s) > 1 and s[1].isdigit():
        return s[1:]
    return s


def compress_preview(label: str) -> str:
    """Shorten preview tags so the preview number survives the 15-char cap."""
    return label.replace("-preview.", "-p.")


def nacp_version(full: str, max_len: int = NACP_VERSION_MAX) -> str:
    """Return a display_version-safe string (≤ max_len)."""
    s = strip_v_prefix(full.strip())
    dirty = False
    if s.endswith("-dirty"):
        dirty = True
        s = s[: -len("-dirty")]
    if not s:
        s = "0.0.0"

    def finish(base: str) -> str:
        if dirty:
            for suffix in ("+d", "*"):
                if len(base) + len(suffix) <= max_len:
                    return base + suffix
        if len(base) <= max_len:
            return base
        return base[-max_len:]

    if len(s) <= max_len:
        return finish(s)

    compressed = compress_preview(s)
    if len(compressed) <= max_len:
        return finish(compressed)

    m = _GIT_DESCRIBE_RE.match(s) or _GIT_DESCRIBE_RE.match(compressed)
    if m:
        base = compress_preview(strip_v_prefix(m.group("tag")))
        sha = m.group("sha")[:7]
        for sha_len in range(min(7, len(sha)), 3, -1):
            candidate = f"{base}+{sha[:sha_len]}"
            if len(candidate) <= max_len:
                return finish(candidate)
        # Drop the 0.6.5- prefix; keep preview token + sha.
        short_base = base
        if short_base.startswith("0.6.5-"):
            short_base = short_base[len("0.6.5-") :]
        for sha_len in range(min(7, len(sha)), 3, -1):
            candidate = f"{short_base}+{sha[:sha_len]}"
            if len(candidate) <= max_len:
                return finish(candidate)

    return finish(compressed)


def resolve_raw_version(
    *,
    explicit: str | None = None,
    repo: Path | None = None,
) -> str:
    """Pick the raw version label before NACP compression.

    Priority:
      1. explicit / DW3_NRO_VERSION env
      2. git describe --tags --dirty --always (when repo has git metadata)
      3. short HEAD sha / unknown
    """
    if explicit:
        return explicit.strip()
    env = os.environ.get("DW3_NRO_VERSION", "").strip()
    if env:
        return env

    root = repo or Path.cwd()
    try:
        described = subprocess.run(
            ["git", "describe", "--tags", "--dirty", "--always"],
            cwd=root,
            check=False,
            capture_output=True,
            text=True,
        )
        if described.returncode == 0:
            out = described.stdout.strip()
            if out:
                return out
    except OSError:
        pass

    try:
        sha = subprocess.run(
            ["git", "rev-parse", "--short=7", "HEAD"],
            cwd=root,
            check=False,
            capture_output=True,
            text=True,
        )
        if sha.returncode == 0 and sha.stdout.strip():
            return sha.stdout.strip()
    except OSError:
        pass

    return "unknown"


def format_from_github(
    *,
    ref: str | None = None,
    ref_name: str | None = None,
    sha: str | None = None,
    release_tag: str | None = None,
) -> str | None:
    """CI helpers: release tag when building a tagged preview; short SHA on PRs.

    Recognizes:
      - RELEASE_TAG / DW3_RELEASE_TAG (switch-release.yml sets this)
      - refs/tags/v* via GITHUB_REF
      - otherwise GITHUB_SHA short form for PR / branch CI
    """
    release_tag = (
        release_tag
        or os.environ.get("DW3_RELEASE_TAG", "")
        or os.environ.get("RELEASE_TAG", "")
    ).strip()
    if release_tag:
        return strip_v_prefix(release_tag)

    ref = (ref or os.environ.get("GITHUB_REF", "")).strip()
    ref_name = (ref_name or os.environ.get("GITHUB_REF_NAME", "")).strip()
    sha = (sha or os.environ.get("GITHUB_SHA", "")).strip()

    if ref.startswith("refs/tags/") and ref_name:
        return strip_v_prefix(ref_name)

    if sha:
        return sha[:7]
    return None


_HEX_SHA_RE = re.compile(r"^[0-9a-f]{7,40}(-dirty)?$")
_TAGLIKE_RE = re.compile(r"^\d+\.\d+")

# P6.37: the label the game shows (Options footer, title corner, dw3-version:
# log line, PC window title). Prefixed by the port name there.
PORT_NAME = "dw3-switch"


def _git(root: Path, *args: str) -> str:
    try:
        r = subprocess.run(
            ["git", *args], cwd=root, check=False, capture_output=True, text=True
        )
    except OSError:
        return ""
    return r.stdout.strip() if r.returncode == 0 else ""


def commit_sha(repo: Path | None = None) -> str:
    """Short (7) commit id: GITHUB_SHA in CI, else git HEAD, else empty."""
    sha = os.environ.get("GITHUB_SHA", "").strip()
    if sha:
        return sha[:7]
    return _git(repo or Path.cwd(), "rev-parse", "--short=7", "HEAD")


def display_label(raw: str, sha: str = "") -> str:
    """The human label for a raw version (after strip_v_prefix).

    - release tag (`0.6.5-preview.57`, maybe `-dirty`): `v0.6.5-preview.57 (962462d)`
    - describe past a tag (`0.6.5-preview.56-12-g8574e58`): `v0.6.5-preview.56-12-g8574e58`
    - bare commit id (`8574e58`, `8574e58-dirty`): as is
    - anything else (`unknown`, a hand-set string): as is
    """
    raw = strip_v_prefix(raw.strip()) or "unknown"
    if _HEX_SHA_RE.match(raw):
        return raw
    if _TAGLIKE_RE.match(raw):
        if _GIT_DESCRIBE_RE.match(raw):
            return "v" + raw
        return f"v{raw} ({sha})" if sha else "v" + raw
    return raw


def build_date(raw: str, repo: Path | None = None) -> str:
    """YYYY-MM-DD: SOURCE_DATE_EPOCH, else the commit date of a clean tree,
    else today (UTC) for a dirty tree or a build without git."""
    epoch = os.environ.get("SOURCE_DATE_EPOCH", "").strip()
    if epoch.isdigit():
        return time.strftime("%Y-%m-%d", time.gmtime(int(epoch)))
    if not raw.endswith("-dirty"):
        d = _git(repo or Path.cwd(), "show", "-s", "--format=%cs", "HEAD")
        if re.match(r"^\d{4}-\d{2}-\d{2}$", d):
            return d
    return time.strftime("%Y-%m-%d", time.gmtime())


def _c_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


def header_text(full: str, sha: str, date: str) -> str:
    label = display_label(full, sha)
    return (
        "/* Generated by tools/dw3_nro_version.py --write-header; do not edit.\n"
        " * P6.37: the port version shown in game and logged (port/version). */\n"
        "#ifndef DW3_VERSION_GEN_H\n"
        "#define DW3_VERSION_GEN_H\n"
        f'#define DW3_VERSION_RAW "{_c_escape(full)}"\n'
        f'#define DW3_VERSION_LABEL "{_c_escape(label)}"\n'
        f'#define DW3_VERSION_SHA "{_c_escape(sha)}"\n'
        f'#define DW3_VERSION_BUILD_DATE "{_c_escape(date)}"\n'
        "#endif\n"
    )


def write_if_changed(path: Path, text: str) -> bool:
    """Write only when the text differs, so make/ninja do not rebuild."""
    try:
        if path.read_text() == text:
            return False
    except OSError:
        pass
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    return True


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo",
        type=Path,
        default=None,
        help="git repo root (default: cwd)",
    )
    parser.add_argument(
        "--version",
        default=None,
        help="override raw version (else DW3_NRO_VERSION / git describe)",
    )
    parser.add_argument(
        "--github",
        action="store_true",
        help="prefer GITHUB_REF_NAME tag / GITHUB_SHA when set",
    )
    parser.add_argument(
        "--format",
        choices=("full", "nacp", "export", "label", "header"),
        default="export",
        help="full label, nacp-safe only, KEY=value lines for shells, the "
        "in-game label, or the C header (P6.37)",
    )
    parser.add_argument(
        "--auto",
        action="store_true",
        help="like --github when RELEASE_TAG / DW3_RELEASE_TAG / GITHUB_SHA "
        "is set (CI), else git describe (CMake of the PC host)",
    )
    parser.add_argument(
        "--write-header",
        type=Path,
        default=None,
        metavar="PATH",
        help="write the C header (dw3_version_gen.h) when its text changed",
    )
    args = parser.parse_args(argv)

    if args.auto and not args.github and not args.version:
        if (
            os.environ.get("DW3_RELEASE_TAG")
            or os.environ.get("RELEASE_TAG")
            or os.environ.get("GITHUB_SHA")
        ) and not os.environ.get("DW3_NRO_VERSION"):
            args.github = True

    raw = None
    if args.github:
        raw = format_from_github()
    if raw is None:
        raw = resolve_raw_version(explicit=args.version, repo=args.repo)

    full = strip_v_prefix(raw)
    nacp = nacp_version(full)

    sha = commit_sha(args.repo)
    date = build_date(full, args.repo)
    if args.write_header is not None:
        write_if_changed(args.write_header, header_text(full, sha, date))

    if args.format == "full":
        print(full)
    elif args.format == "nacp":
        print(nacp)
    elif args.format == "label":
        print(f"{PORT_NAME} {display_label(full, sha)}")
    elif args.format == "header":
        sys.stdout.write(header_text(full, sha, date))
    else:
        print(f"DW3_NRO_VERSION={full}")
        print(f"DW3_NACP_VERSION={nacp}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
