"""Output filter (Options > Display > Output filter, docs/OUTPUT_FILTER.md).

The GPU check compiles the real filtered shaders of platforms/switch/psyz_backend
(nx_output_filter.h, nx_present16.h) on a GL ES 3 context (Mesa, surfaceless EGL)
and compares what they draw with a CPU reference of the same arithmetic.
Skipped when this machine has no EGL/GLES3 or no usable context.
"""
import os
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
GPU_TEST_C = ROOT / "tools" / "output_filter_gpu_test.c"
BACKEND = ROOT / "platforms" / "switch" / "psyz_backend"


def test_output_filter_shaders_match_the_reference(tmp_path_factory):
    out = tmp_path_factory.mktemp("output_filter_gpu") / "output_filter_gpu_test"
    cmd = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2", "-o", str(out),
           str(GPU_TEST_C), f"-I{BACKEND}", "-lEGL", "-lGLESv2", "-lm"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        pytest.skip("no EGL/GLES3 development files: " + r.stderr[-200:])
    env = dict(os.environ, EGL_PLATFORM="surfaceless", LIBGL_ALWAYS_SOFTWARE="1")
    r = subprocess.run([str(out)], capture_output=True, text=True, env=env)
    if r.returncode == 77:
        pytest.skip("no surfaceless GL ES 3 context: " + r.stderr[-200:])
    assert r.returncode == 0, r.stderr + r.stdout
    assert "OK: the output filter programs match the CPU reference" in r.stdout
    # Sharp at an integer scale (the handheld 3x) is the Nearest picture.
    assert "OK: sharp at the integer scale 3x is identical to nearest" in r.stdout
    # At the docked 4.5x the seams are about one output pixel wide.
    assert "OK: sharp seams at 4.50x (prescale 4)" in r.stdout
