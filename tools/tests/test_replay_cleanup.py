"""The replay runner leaves no Xvfb behind: normal stop, SIGTERM/SIGINT of the
runner, and SIGKILL of the runner (PR_SET_PDEATHSIG). Needs Xvfb (skipped without it);
Pillow is not needed (replay_tests imports it lazily); no game data."""
from __future__ import annotations

import os
import shutil
import signal
import subprocess
import sys
import textwrap
import time
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
REPLAY = ROOT / "tools" / "replay"

pytestmark = pytest.mark.skipif(shutil.which("Xvfb") is None or not sys.platform.startswith("linux"),
                                reason="needs Xvfb on Linux")


def _alive(pid):
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    try:   # a zombie is gone for our purposes
        return Path(f"/proc/{pid}/stat").read_text().split(")")[-1].split()[0] != "Z"
    except OSError:
        return False


def _gone(pid, secs=10.0):
    end = time.time() + secs
    while time.time() < end:
        if not _alive(pid):
            return True
        time.sleep(0.1)
    return False


def _runner(tmp_path, body):
    """A child python that starts an Xvfb through replay_tests, prints its pid and runs `body`."""
    script = tmp_path / "child.py"
    script.write_text(textwrap.dedent(f"""
        import sys, time
        sys.path.insert(0, {str(REPLAY)!r})
        import replay_tests as rt
        rt.install_cleanup()
        p, disp = rt.start_xvfb(__import__("pathlib").Path({str(tmp_path)!r}))
        print(p.pid, flush=True)
        {body}
    """))
    proc = subprocess.Popen([sys.executable, "-I", str(script)], stdout=subprocess.PIPE, text=True)
    line = proc.stdout.readline()
    assert line.strip().isdigit(), f"child runner failed: {proc.wait(10)} {line!r}"
    xpid = int(line)
    assert _alive(xpid)
    return proc, xpid


def test_stop_xvfb_removes_it(tmp_path):
    sys.path.insert(0, str(REPLAY))
    import replay_tests as rt
    p, disp = rt.start_xvfb(tmp_path)
    assert disp.startswith(":") and p in rt._LIVE_XVFB
    rt.stop_xvfb(p)
    assert p not in rt._LIVE_XVFB and not _alive(p.pid)


@pytest.mark.parametrize("sig", [signal.SIGTERM, signal.SIGINT, signal.SIGHUP])
def test_signal_to_the_runner_reaps_xvfb(tmp_path, sig):
    proc, xpid = _runner(tmp_path, "time.sleep(60)")
    try:
        proc.send_signal(sig)
        proc.wait(10)
        assert proc.returncode == 128 + sig
        assert _gone(xpid), "Xvfb survived the signal"
    finally:
        proc.kill()
        proc.wait()


def test_normal_exit_reaps_xvfb(tmp_path):
    proc, xpid = _runner(tmp_path, "pass")   # no stop_xvfb: atexit must do it
    proc.wait(10)
    assert _gone(xpid), "Xvfb survived a normal exit"


def test_exception_reaps_xvfb(tmp_path):
    proc, xpid = _runner(tmp_path, "raise RuntimeError('boom')")
    proc.wait(10)
    assert proc.returncode != 0 and _gone(xpid)


def test_sigkill_of_the_runner_reaps_xvfb(tmp_path):
    proc, xpid = _runner(tmp_path, "time.sleep(60)")
    try:
        proc.kill()      # what a timeout does: no handler, no atexit
        proc.wait(10)
        assert _gone(xpid), "Xvfb survived SIGKILL of the runner"
    finally:
        if _alive(xpid):
            os.kill(xpid, signal.SIGKILL)
