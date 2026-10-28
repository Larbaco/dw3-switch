#!/usr/bin/env python3
"""List (and, only with --apply --yes, remove) stale scratch dirs in /mnt/cabal/tmp.

Dry run by default: nothing is deleted unless BOTH --apply and --yes are given.

An entry directly under TMP_ROOT is a deletion candidate when ALL hold:
  * its newest file (looked at two levels deep) is older than --days (default 14);
  * it is not a registered git worktree of this repository, and not the target of
    a worktree's `build` symlink (the NTFS-checkout layout in AGENTS.md);
  * no process has its cwd inside it;
  * it is not protected: dw3-preview<NN>*-out folders, anything named or under
    `evidence` / `assets`, and anything with backup, keep, refs, runtime, disks,
    memcard or ccache in its name are never touched.
Only directories are candidates: an unregistered directory with a `.git` file
pointing at a missing gitdir is an "orphan worktree"; a directory named like our
scratch trees (dw3-*, build*, pr<N>*, p<N>*, *-build, *-src, *-worktree, *-out)
is a "scratch build dir". Loose files and everything else (the owner's
downloads, notes) are never listed.

  tools/cleanup_tmp.py                 dry run, 14 days, /mnt/cabal/tmp
  tools/cleanup_tmp.py --days 30 --no-sizes
  tools/cleanup_tmp.py --apply --yes   really delete the listed entries

See docs/BUILD_SPEED.md.
"""
from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OURS = re.compile(r"^(dw3[-_.]|build|pr\d+|p\d+|.*-(build|src|worktree|wt|out)$)")
PROTECT_NAME = re.compile(
    r"^(dw3-preview\d+.*-out|evidence|assets|\.cache|lost\+found)$"
    r"|backup|keep|refs|runtime|disks|memcard|ccache|replay-nightly")


def registered_worktrees() -> set[Path]:
    """Worktree paths known to git, plus the targets of their `build` symlinks."""
    out = subprocess.run(["git", "-C", str(ROOT), "worktree", "list", "--porcelain"],
                         capture_output=True, text=True)
    paths: set[Path] = set()
    for line in out.stdout.splitlines():
        if line.startswith("worktree "):
            p = Path(line[len("worktree "):])
            paths.add(p)
            b = p / "build"
            try:
                if b.is_symlink():
                    paths.add(Path(os.path.realpath(b)))
            except OSError:
                pass
    return paths


def cwd_users() -> set[Path]:
    """Directories some process currently sits in (or has as root)."""
    used = set()
    for pid in os.listdir("/proc"):
        if not pid.isdigit():
            continue
        for link in ("cwd",):
            try:
                used.add(Path(os.readlink(f"/proc/{pid}/{link}")))
            except OSError:
                pass
    return used


def newest_mtime(path: Path, depth: int = 2) -> float:
    """Newest mtime among the entry and its descendants down to `depth`."""
    newest = 0.0
    try:
        newest = path.lstat().st_mtime
    except OSError:
        return newest
    if depth == 0 or not path.is_dir() or path.is_symlink():
        return newest
    try:
        with os.scandir(path) as it:
            for e in it:
                try:
                    m = e.stat(follow_symlinks=False).st_mtime
                except OSError:
                    continue
                newest = max(newest, m)
                if e.is_dir(follow_symlinks=False) and depth > 1:
                    newest = max(newest, newest_mtime(Path(e.path), depth - 1))
    except OSError:
        pass
    return newest


def tree_kib(path: Path) -> int:
    r = subprocess.run(["du", "-sk", "--", str(path)], capture_output=True, text=True)
    try:
        return int(r.stdout.split()[0])
    except (IndexError, ValueError):
        return 0


def orphan_worktree(path: Path) -> bool:
    g = path / ".git"
    if not g.is_file():
        return False
    m = re.match(r"gitdir:\s*(.+)", g.read_text(errors="replace").strip())
    return bool(m) and not Path(m.group(1)).exists()


def under(child: Path, parent: Path) -> bool:
    try:
        child.relative_to(parent)
        return True
    except ValueError:
        return False


def scan(tmp_root: Path, days: float, sizes: bool):
    now = time.time()
    keep_wt = registered_worktrees()
    busy = cwd_users()
    cands, kept = [], []
    for entry in sorted(tmp_root.iterdir()):
        real = Path(os.path.realpath(entry))
        why = None
        if PROTECT_NAME.search(entry.name) or "evidence" in entry.parts or "assets" in entry.parts:
            why = "protected name"
        elif any(under(real, w) or under(w, real) for w in keep_wt):
            why = "git worktree (or its build dir)"
        elif any(under(b, real) for b in busy):
            why = "a process is working in it"
        else:
            age = (now - newest_mtime(entry)) / 86400
            if age < days:
                why = f"recent ({age:.1f} days)"
        if why:
            kept.append((entry, why))
            continue
        if not entry.is_dir() or entry.is_symlink():
            kept.append((entry, "not a directory"))
            continue
        if orphan_worktree(entry):
            kind = "orphan worktree"
        elif OURS.match(entry.name):
            kind = "scratch build dir"
        else:
            kept.append((entry, "not named like our scratch dirs"))
            continue
        cands.append((entry, kind, tree_kib(entry) if sizes else 0,
                      (now - newest_mtime(entry)) / 86400))
    return cands, kept


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", default="/mnt/cabal/tmp", help="directory to scan")
    ap.add_argument("--days", type=float, default=14, help="minimum age in days (default 14)")
    ap.add_argument("--no-sizes", action="store_true", help="skip du (faster)")
    ap.add_argument("--apply", action="store_true", help="delete the listed entries ...")
    ap.add_argument("--yes", action="store_true", help="... and confirm it")
    ap.add_argument("--show-kept", action="store_true", help="also list what is kept and why")
    a = ap.parse_args()

    tmp_root = Path(a.root)
    if not tmp_root.is_dir():
        print(f"no such directory: {tmp_root}", file=sys.stderr)
        return 2
    cands, kept = scan(tmp_root, a.days, not a.no_sizes)
    total = sum(c[2] for c in cands)
    print(f"{tmp_root}: {len(cands) + len(kept)} entries, {len(cands)} candidate(s) older than "
          f"{a.days:g} days, {len(kept)} kept")
    if a.show_kept:
        for entry, why in kept:
            print(f"  keep   {entry.name:<48} {why}")
    for entry, kind, kib, age in sorted(cands, key=lambda c: -c[2]):
        size = f"{kib / 1048576:7.2f} GiB" if kib >= 1048576 else f"{kib / 1024:7.0f} MiB"
        print(f"  delete {entry.name:<48} {size if not a.no_sizes else '':>11}  {age:5.0f} d  {kind}")
    if not a.no_sizes:
        print(f"would free about {total / 1048576:.1f} GiB")
    if not (a.apply and a.yes):
        print("dry run: nothing was deleted (add --apply --yes to delete the list above)")
        return 0
    for entry, _kind, _kib, _age in cands:
        if entry.is_symlink() or entry.is_file():
            entry.unlink()
        else:
            shutil.rmtree(entry, ignore_errors=True)
        print(f"deleted {entry}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
