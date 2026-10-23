"""P5.4 tools/symbolize_crash.py: report / creport parsing, build-id check
and the addr2line plumbing, with a stub addr2line (no devkitA64 needed)."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import symbolize_crash as sc  # noqa: E402

NEW_REPORT = textwrap.dedent("""\
    dw3-switch crash report
    version: 0.6.5-test
    reason: user exception
    mode_file: 0x207 (STSTATUS)
    stage_file: 0x1e4 (WSTAG355)
    disc: EU language: 3 (fr)
    git: 0123456789ab
    build_id: {build_id}
    vsync_pumps: 54068
    last_dt_us: 1525767
    module_base: 0x0000000b697c2000 text_size: 0x59f000
    error_desc: 0x101 (Other)
    pc:  0x0000000b699233ec base+0x1613ec
    lr:  0x0000000b699233ec base+0x1613ec
    sp:  0x000000005d92ecf0
    fp:  0x000000005d92ecf0
    far: 0x000000000000000d
    esr: 0x92000006 (data abort, read, translation fault L2)
    x0:  0x0000000000000000
    x1:  0x0000000b699233e0 base+0x1613e0
    --- backtrace (fp chain; saved lr per frame) ---
    stack: 0x000000005d000000..0x000000005e000000 type 0xb
    #00 0x0000000b69889608 base+0xc7608
    #01 0x0000000b6988c270 base+0xca270
    backtrace_end: 2 frames, end of chain (fp=0)
    --- last log lines ---
    #99 0x1234 base+0x10  (a log line that looks like a frame)
    pc:  0x1 base+0x1
    --- 1 log lines ---
    --- end ---
    """)

OLD_REPORT = textwrap.dedent("""\
    dw3-switch crash report
    version: 0.6.5-preview.30-rc6
    reason: user exception
    mode_file: 0x207
    stage_file: 0x1e4
    pc:  0x0000000b699233ec
    lr:  0x0000000b699233ec
    x1:  0x0000000b699233e0
    --- end ---
    """)

CREPORT = textwrap.dedent("""\
    Atmosphère Crash Report (v1.7):
    Exception Info:
        Address:                     0000000b69d30e20 (dw3-switch + 0x56ee20)
    Crashed Thread Info:
        Thread ID:                   0000000000000bd2
        Registers:
            LR:                      0000000b697c8264 (dw3-switch + 0x6264)
            PC:                      0000000b69d30e1c (dw3-switch + 0x56ee1c)
        Stack Trace:
            ReturnAddress[00]:       0000000b69d36ea4 (dw3-switch + 0x574ea4)
            ReturnAddress[01]:       0000000b69889608 (dw3-switch + 0xc7608)
    Thread Report:
        Thread ID:                   0000000000000bf9
        Stack Trace:
            ReturnAddress[00]:       0000000b6981d6ac (dw3-switch + 0x5b6ac)
    """)


def test_parse_new_report():
    rep = sc.parse_report(NEW_REPORT.format(build_id="aa"))
    assert rep.base == 0xB697C2000
    assert rep.fields["version"] == "0.6.5-test"
    assert rep.fields["mode_file"] == "0x207 (STSTATUS)"
    assert rep.fields["backtrace_end"].startswith("2 frames")
    regs = {r.label: r for r in rep.regs}
    assert regs["pc"].offset == 0x1613EC and not regs["pc"].is_return
    assert regs["lr"].is_return
    assert regs["x1"].offset == 0x1613E0
    assert regs["x0"].offset is None
    assert [f.offset for f in rep.frames] == [0xC7608, 0xCA270]
    assert all(f.is_return for f in rep.frames)  # log tail lines are not frames


def test_old_report_rebased_from_creport():
    rep = sc.parse_report(OLD_REPORT)
    cre = sc.parse_creport(CREPORT)
    assert rep.base is None
    assert cre.base == 0xB697C2000
    assert [a.label for a in cre.addrs] == ["lr", "pc", "ret[00]", "ret[01]"]
    sc.resolve_offsets(rep.regs, cre.base)
    regs = {r.label: r for r in rep.regs}
    assert regs["pc"].offset == 0x1613EC


def test_addr2line_output_parser():
    out = textwrap.dedent("""\
        0x00000000001613ec: getItemCategory at /x/build/decomp-src/src/engine/game/items.c:28
        0x000000000055f6a8: waitSingle at /y/libnx/wait.h:106
         (inlined by) eventWait at /y/libnx/event.c:33
        """)
    got = sc.parse_addr2line(out)
    assert got[0x1613EC] == ["getItemCategory at /x/build/decomp-src/src/engine/game/items.c:28"]
    assert got[0x55F6A8][1] == "(inlined by) eventWait at /y/libnx/event.c:33"
    assert sc.short_path(got[0x1613EC][0], False) == \
        "getItemCategory at decomp/src/engine/game/items.c:28"
    assert sc.short_path("f at /a/b/platforms/switch/x.c:1", False) == \
        "f at platforms/switch/x.c:1"


def test_return_into_a_function_entry_is_flagged():
    syms = [(0x5b38, 0x64, "__wrap___assert_func"), (0x5b9c, 0x64, "__wrap_exit")]
    rows = sc.format_rows([sc.Addr("#03", None, 0x5b9c, True),
                           sc.Addr("#02", None, 0x5b40, True)],
                          {0x5b98: ["??"], 0x5b3c: ["??"]}, False, syms)
    assert "entry of __wrap_exit" in rows[0]
    assert "entry of" not in rows[1]


@pytest.fixture(scope="module")
def host_elf(tmp_path_factory) -> Path:
    d = tmp_path_factory.mktemp("elf")
    src = d / "t.c"
    src.write_text('const char v[] = "0.6.5-test"; const char g[] = "0123456789ab";\n'
                   'int main(void) { return v[0] + g[0]; }\n')
    out = d / "t.elf"
    r = subprocess.run(["gcc", "-Wl,--build-id=sha1", "-o", str(out), str(src)],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    return out


def test_build_id_read_matches_readelf(host_elf):
    bid = sc.elf_build_id(host_elf)
    assert bid and len(bid) == 40
    if shutil.which("readelf"):
        r = subprocess.run(["readelf", "-n", str(host_elf)], capture_output=True, text=True)
        assert bid in r.stdout
    assert any(name == "main" for _a, _s, name in sc.elf_text_symbols(host_elf))


def _stub_addr2line(tmp: Path) -> Path:
    stub = tmp / "fake-addr2line"
    stub.write_text(textwrap.dedent("""\
        #!/usr/bin/env python3
        import sys
        args = sys.argv[1:]
        for a in args:
            if a.startswith("0x"):
                print(f"0x{int(a, 16):016x}: fn_{int(a, 16):x} at /w/platforms/switch/f.c:{int(a, 16) % 97}")
        """))
    stub.chmod(0o755)
    return stub


def test_cli_match_and_mismatch(host_elf, tmp_path):
    stub = _stub_addr2line(tmp_path)
    bid = sc.elf_build_id(host_elf)
    rep = tmp_path / "crash_report.txt"
    rep.write_text(NEW_REPORT.format(build_id=bid))
    cmd = [sys.executable, str(ROOT / "tools/symbolize_crash.py"), str(rep), str(host_elf),
           "--addr2line", str(stub), "--strict"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "build_id matches" in r.stdout
    assert "version string '0.6.5-test' found" in r.stdout
    # pc looked up as is, return addresses at addr-4 (the call)
    assert "fn_1613ec at platforms/switch/f.c" in r.stdout
    assert "fn_c7604 at platforms/switch/f.c" in r.stdout
    assert "fn_1613e8" in r.stdout  # lr
    rep.write_text(NEW_REPORT.format(build_id="00" * 20))
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 3
    assert "build_id MISMATCH" in r.stdout


def test_cli_old_report_needs_base(host_elf, tmp_path):
    stub = _stub_addr2line(tmp_path)
    rep = tmp_path / "crash_report.txt"
    rep.write_text(OLD_REPORT)
    cre = tmp_path / "creport.log"
    cre.write_text(CREPORT)
    base = [sys.executable, str(ROOT / "tools/symbolize_crash.py"), str(rep), str(host_elf),
            "--addr2line", str(stub)]
    r = subprocess.run(base, capture_output=True, text=True)
    assert r.returncode == 0
    assert "module_base: unknown" in r.stdout
    r = subprocess.run(base + ["--creport", str(cre)], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    assert "module_base: 0xb697c2000 (from creport)" in r.stdout
    assert "fn_1613ec" in r.stdout
    assert "creport crashed thread" in r.stdout
    assert "fn_574ea0" in r.stdout and "fn_5b6a8" not in r.stdout  # idle thread skipped
    assert "report has no build_id" in r.stdout
