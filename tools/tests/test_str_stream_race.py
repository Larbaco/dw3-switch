"""The STR file bridge must serve the sector that was asked for.

port/movies/str_stream.c has two callers: the video ring, fed from
StCdInterrupt on the game/main thread (CD_ready -> disk_read_cb, no PSY-Z lock
held), and the XA decoder on the audio thread (xa_pull_samples holds
Psyz_AudioLock). They share one FILE* with a non-atomic open/seek/read, so
without a lock one reader's fseek lands between the other's fseek and fread.
StCdInterrupt then sees a secCount mismatch, resets the ring, and the whole
partial STR frame is lost -- which is what made the opening movie play at
~9.5 fps instead of 15.

The helper hammers dw3_str_cd_read_cb from two threads over two disjoint LBA
ranges, so each call also reopens the shared FILE*. It is synthesized data
only: every sector encodes its own LBA.
"""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]

SOURCES = [
    ROOT / "tools" / "tests" / "str_stream_race_main.c",
    ROOT / "port" / "movies" / "str_stream.c",
]

INCLUDES = [
    f"-I{ROOT / 'port' / 'movies'}",
    f"-I{ROOT / 'port' / 'cdfs'}",
    f"-I{ROOT / 'vendor' / 'psyz' / 'psyz' / 'include'}",
    f"-I{ROOT / 'vendor' / 'psyz' / 'decomp' / 'include'}",
]


@pytest.fixture(scope="module")
def race_output(tmp_path_factory) -> dict[str, str]:
    work = tmp_path_factory.mktemp("str_stream_race")
    exe = work / "str_stream_race"
    cmd = [
        "gcc",
        "-std=c11",
        # pthread_mutexattr_settype / PTHREAD_MUTEX_RECURSIVE are POSIX, not
        # ISO C, so the strict -std=c11 above needs the feature macro.
        "-D_GNU_SOURCE",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wno-comment",
        "-O2",
        "-pthread",
        "-o",
        str(exe),
        *[str(s) for s in SOURCES],
        *INCLUDES,
    ]
    build = subprocess.run(cmd, capture_output=True, text=True)
    assert build.returncode == 0, build.stderr + build.stdout

    scratch = work / "data"
    scratch.mkdir()
    run = subprocess.run([str(exe), str(scratch)], capture_output=True, text=True)
    assert run.returncode == 0, run.stdout + run.stderr

    parsed: dict[str, str] = {}
    for line in run.stdout.splitlines():
        key, _, value = line.partition("=")
        parsed[key.strip()] = value.strip()
    return parsed


def test_every_read_returns_the_requested_sector(race_output):
    assert race_output["mismatches"] == "0"
    assert race_output["errors"] == "0"


def test_the_two_threads_really_overlapped(race_output):
    # Guards the test itself: with no contention at all, a mismatch-free run
    # would say nothing about the lock. The counter is bumped before the lock
    # is taken, so it stays non-zero after a fix.
    assert int(race_output["overlaps"]) > 0
    assert int(race_output["reads"]) > int(race_output["overlaps"])
