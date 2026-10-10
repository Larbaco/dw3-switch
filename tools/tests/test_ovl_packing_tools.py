"""P4.7 cross-compile guard: overlay packing must use *target* binutils.

`tools/rename_ovl_sections.py` and `tools/prefix_stage_obj.py` run `ld -r`,
`objcopy` and `nm` over target objects. A cross build (Switch/aarch64) has to be
able to point them at the target binutils — a host `ld` cannot merge AArch64
objects, and the failure is silent until the link. The PC build must keep
resolving the bare names exactly as before.

Resolution contract (see `_tool()` in both scripts):

    DW3_<tool>            explicit per-tool override
    DW3_BINUTILS_PREFIX   toolchain prefix, e.g. "aarch64-none-elf-"
    (neither set)         the bare tool name
"""

from __future__ import annotations

import importlib.util
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = {
    "rename": ROOT / "tools" / "rename_ovl_sections.py",
    "prefix": ROOT / "tools" / "prefix_stage_obj.py",
}


def _load(path: Path):
    spec = importlib.util.spec_from_file_location(path.stem, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _env_without_overrides() -> dict:
    return {k: v for k, v in os.environ.items() if not k.startswith("DW3_")}


def test_defaults_are_bare_tool_names(monkeypatch):
    for key in ("DW3_LD", "DW3_OBJCOPY", "DW3_NM", "DW3_BINUTILS_PREFIX"):
        monkeypatch.delenv(key, raising=False)

    for name, path in SCRIPTS.items():
        module = _load(path)
        assert [module._tool(t) for t in ("ld", "objcopy", "nm")] == [
            "ld",
            "objcopy",
            "nm",
        ], f"{name} must keep resolving bare names when the env is clean"


def test_prefix_is_applied(monkeypatch):
    monkeypatch.delenv("DW3_LD", raising=False)
    monkeypatch.setenv("DW3_BINUTILS_PREFIX", "aarch64-none-elf-")

    module = _load(SCRIPTS["rename"])
    assert module._tool("ld") == "aarch64-none-elf-ld"


def test_per_tool_override_wins_over_prefix(monkeypatch):
    monkeypatch.setenv("DW3_BINUTILS_PREFIX", "aarch64-none-elf-")
    monkeypatch.setenv("DW3_OBJCOPY", "/opt/bin/objcopy-real")

    module = _load(SCRIPTS["prefix"])
    assert module._tool("objcopy") == "/opt/bin/objcopy-real"
    assert module._tool("ld") == "aarch64-none-elf-ld"


def test_prefix_reaches_the_real_exec(tmp_path):
    """The env must actually select the binary that gets exec'd."""
    src = tmp_path / "stage.c"
    src.write_text(
        "int g_counter = 7;\nint g_table[16];\n"
        "static int s_private = 3;\n"
        "int stage_tick(void) { g_table[0] = s_private; return g_counter; }\n"
    )
    obj = tmp_path / "stage.o"
    r = subprocess.run(
        ["gcc", "-c", "-O0", "-o", str(obj), str(src)],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stderr

    out = tmp_path / "host_ovl" / "fieldstg.host.o"
    r = subprocess.run(
        [
            "python3",
            str(SCRIPTS["rename"]),
            "fieldstg",
            str(out),
            str(obj),
        ],
        capture_output=True,
        text=True,
        env=_env_without_overrides(),
    )
    assert r.returncode == 0, r.stderr
    assert out.is_file()

    # A prefix that does not exist must surface as the *prefixed* name, proving
    # the variable is what selected the tool rather than a stray PATH hit.
    r = subprocess.run(
        [
            "python3",
            str(SCRIPTS["rename"]),
            "fieldstg",
            str(tmp_path / "host_ovl" / "bogus.o"),
            str(obj),
        ],
        capture_output=True,
        text=True,
        env={**_env_without_overrides(), "DW3_BINUTILS_PREFIX": "bogus-nonexistent-"},
    )
    assert r.returncode != 0
    assert "bogus-nonexistent-ld" in (r.stderr + r.stdout)
