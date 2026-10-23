#!/usr/bin/env python3
"""Symbolize a dw3-switch crash_report.txt (P5.4) against the matching .elf.

Prints `file:line function` (inlined frames too) for pc, lr, every backtrace
frame and the registers that point into the game's code, using devkitA64's
aarch64-none-elf-addr2line (-f -i -C). Checks that the .elf is the build that
wrote the report (GNU build-id, then version / git strings) and warns loudly
when it is not: a wrong .elf gives plausible but wrong functions.

  tools/symbolize_crash.py crash_report.txt build/switch-nxlink/dw3-switch.elf
  tools/symbolize_crash.py crash_report.txt dw3-switch.elf \\
      --creport 01792722609_05446530aca7e000.log   # older report / creport

Reports from before P5.4 PR 1 have absolute addresses only; pass the
Atmosphere creport log of the same crash (module base from its
`dw3-switch + 0x...` lines) or --base. The creport stack trace is
symbolized too when given.

addr2line: --addr2line PATH, else aarch64-none-elf-addr2line on PATH or under
$DEVKITPRO, else the devkitpro/devkita64 Docker image (as tools/dk.sh uses).
Return addresses (lr, frames) are looked up at addr-4, the call instruction.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import struct
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

DOCKER_IMAGE = os.environ.get("DW3_DK_IMAGE", "devkitpro/devkita64:latest")
ADDR2LINE = "aarch64-none-elf-addr2line"

ADDR_RE = re.compile(r"0x([0-9a-fA-F]+)(?:\s+base\+0x([0-9a-fA-F]+))?")
FRAME_RE = re.compile(r"^#(\d+)\s+(.*)$")
REG_RE = re.compile(r"^(pc|lr|x\d{1,2}):\s+(.*)$")
CREPORT_ADDR_RE = re.compile(
    r"^\s*(PC|LR|ReturnAddress\[(\d+)\]):\s+([0-9a-fA-F]{8,16})\s+\(([^+)]+?)\s*\+\s*0x([0-9a-fA-F]+)\)")


@dataclass
class Addr:
    label: str
    absolute: Optional[int]
    offset: Optional[int]
    is_return: bool  # lr / saved lr: look up the call instruction


@dataclass
class Report:
    fields: Dict[str, str] = field(default_factory=dict)
    regs: List[Addr] = field(default_factory=list)
    frames: List[Addr] = field(default_factory=list)
    base: Optional[int] = None


def _addr(label: str, text: str, is_return: bool) -> Optional[Addr]:
    m = ADDR_RE.search(text)
    if not m:
        return None
    absolute = int(m.group(1), 16)
    offset = int(m.group(2), 16) if m.group(2) else None
    return Addr(label, absolute, offset, is_return)


def parse_report(text: str) -> Report:
    """Fields (`key: value` lines), pc/lr/xN and `#NN` backtrace frames."""
    rep = Report()
    for raw in text.splitlines():
        line = raw.rstrip()
        if line.startswith("--- last log lines"):
            break  # the log tail is free text
        m = FRAME_RE.match(line)
        if m:
            a = _addr(f"#{int(m.group(1)):02d}", m.group(2), True)
            if a:
                rep.frames.append(a)
            continue
        m = REG_RE.match(line)
        if m:
            name = m.group(1)
            a = _addr(name, m.group(2), name == "lr")
            if a:
                rep.regs.append(a)
            continue
        key, sep, val = line.partition(": ")
        if sep and key and " " not in key:
            rep.fields.setdefault(key, val.strip())
    mb = rep.fields.get("module_base", "")
    m = ADDR_RE.match(mb)
    if m and int(m.group(1), 16) != 0:
        rep.base = int(m.group(1), 16)
    return rep


@dataclass
class Creport:
    base: Optional[int]
    module: Optional[str]
    addrs: List[Addr]


def parse_creport(text: str, module: str = "dw3-switch") -> Creport:
    """Atmosphere creport log: module base from any `<abs> (dw3-switch +
    0x<off>)` pair; PC / LR / ReturnAddress[NN] of the crashed thread only
    (the `Crashed Thread Info:` section; the thread report repeats it and
    adds the idle threads)."""
    base = None
    addrs: List[Addr] = []
    section = ""
    pair = re.compile(r"([0-9a-fA-F]{8,16})\s+\(" + re.escape(module) + r"\s*\+\s*0x([0-9a-fA-F]+)\)")
    for line in text.splitlines():
        if line and not line[0].isspace() and line.rstrip().endswith(":"):
            section = line.strip().rstrip(":")
        m = pair.search(line)
        if m and base is None:
            base = int(m.group(1), 16) - int(m.group(2), 16)
        if section != "Crashed Thread Info":
            continue
        m = CREPORT_ADDR_RE.match(line)
        if not m or m.group(4).strip() != module:
            continue
        absolute = int(m.group(3), 16)
        off = int(m.group(5), 16)
        kind = m.group(1)
        label = kind.lower() if m.group(2) is None else f"ret[{int(m.group(2)):02d}]"
        addrs.append(Addr(label, absolute, off, kind != "PC"))
    return Creport(base, module if addrs else None, addrs)


def resolve_offsets(addrs: Sequence[Addr], base: Optional[int]) -> None:
    for a in addrs:
        if a.offset is None and base is not None and a.absolute is not None:
            off = a.absolute - base
            if 0 <= off < 0x10000000:
                a.offset = off


# --- ELF identity ---------------------------------------------------------

def elf_build_id(path: Path) -> Optional[str]:
    """GNU build-id (hex) from a 64-bit little-endian ELF's PT_NOTE /
    SHT_NOTE sections, or None."""
    data = path.read_bytes()
    if data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
        return None
    e_shoff = struct.unpack_from("<Q", data, 0x28)[0]
    e_shentsize, e_shnum = struct.unpack_from("<HH", data, 0x3A)
    for i in range(e_shnum):
        sh = e_shoff + i * e_shentsize
        sh_type = struct.unpack_from("<I", data, sh + 4)[0]
        if sh_type != 7:  # SHT_NOTE
            continue
        off, size = struct.unpack_from("<QQ", data, sh + 0x18)
        pos = off
        while pos + 12 <= off + size:
            namesz, descsz, ntype = struct.unpack_from("<III", data, pos)
            name_start = pos + 12
            desc_start = name_start + ((namesz + 3) & ~3)
            if ntype == 3 and data[name_start:name_start + namesz] == b"GNU\0":
                return data[desc_start:desc_start + descsz].hex()
            pos = desc_start + ((descsz + 3) & ~3)
    return None


def elf_text_symbols(path: Path) -> List[Tuple[int, int, str]]:
    """Sorted (addr, size, name) of FUNC / NOTYPE symbols in executable
    sections, from .symtab. A cross-check for code without DWARF (crt0,
    libnx asm), where addr2line's function can be a neighbour's."""
    data = path.read_bytes()
    if data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
        return []
    e_shoff = struct.unpack_from("<Q", data, 0x28)[0]
    e_shentsize, e_shnum = struct.unpack_from("<HH", data, 0x3A)
    secs = []
    for i in range(e_shnum):
        sh = e_shoff + i * e_shentsize
        _name, sh_type = struct.unpack_from("<II", data, sh)
        sh_flags = struct.unpack_from("<Q", data, sh + 8)[0]
        off, size = struct.unpack_from("<QQ", data, sh + 0x18)
        link = struct.unpack_from("<I", data, sh + 0x28)[0]
        entsize = struct.unpack_from("<Q", data, sh + 0x38)[0]
        secs.append((sh_type, sh_flags, off, size, link, entsize))
    out: List[Tuple[int, int, str]] = []
    for sh_type, _flags, off, size, link, entsize in secs:
        if sh_type != 2 or entsize != 24:  # SHT_SYMTAB
            continue
        str_off = secs[link][2]
        for pos in range(off, off + size, 24):
            st_name, st_info, _other, shndx, value, st_size = struct.unpack_from(
                "<IBBHQQ", data, pos)
            if (st_info & 0xF) not in (0, 2) or shndx == 0 or shndx >= len(secs):
                continue
            if not (secs[shndx][1] & 0x4):  # SHF_EXECINSTR
                continue
            end = data.index(b"\0", str_off + st_name)
            name = data[str_off + st_name:end].decode("ascii", "replace")
            if name and not name.startswith("$"):
                out.append((value, st_size, name))
    out.sort()
    return out


def symbol_at(syms: Sequence[Tuple[int, int, str]], off: int) -> Optional[str]:
    lo, hi = 0, len(syms)
    while lo < hi:
        mid = (lo + hi) // 2
        if syms[mid][0] <= off:
            lo = mid + 1
        else:
            hi = mid
    if lo == 0:
        return None
    addr, _size, name = syms[lo - 1]
    return f"{name}+0x{off - addr:x}"


def identity_checks(rep: Report, elf: Path) -> Tuple[List[str], List[str]]:
    """(ok lines, warnings) about whether elf built the report."""
    ok: List[str] = []
    warn: List[str] = []
    want = rep.fields.get("build_id", "")
    got = elf_build_id(elf)
    if want and want != "unknown":
        if got is None:
            warn.append(f"{elf.name} has no GNU build-id; cannot match it to the report ({want})")
        elif got == want:
            ok.append(f"build_id matches ({got})")
        else:
            warn.append(f"build_id MISMATCH: report {want}, {elf.name} {got} -- "
                        "this is not the .elf of the build that crashed; the "
                        "functions below are likely wrong")
    else:
        warn.append("report has no build_id (written before P5.4 PR 1); the "
                    ".elf match is only checked by version / git strings")
    blob = None
    for key in ("version", "git"):
        val = rep.fields.get(key, "")
        if not val or val in ("unknown", "(none)"):
            continue
        if blob is None:
            blob = elf.read_bytes()
        if val.encode() in blob:
            ok.append(f"{key} string {val!r} found in {elf.name}")
        else:
            warn.append(f"{key} {val!r} not found in {elf.name}")
    return ok, warn


# --- addr2line ------------------------------------------------------------

def addr2line_cmd(elf: Path, explicit: Optional[str], no_docker: bool) -> List[str]:
    if explicit:
        return [explicit]
    found = shutil.which(ADDR2LINE)
    if found:
        return [found]
    dkp = os.environ.get("DEVKITPRO")
    if dkp and (Path(dkp) / "devkitA64/bin" / ADDR2LINE).is_file():
        return [str(Path(dkp) / "devkitA64/bin" / ADDR2LINE)]
    if no_docker or not shutil.which("docker"):
        raise SystemExit(f"no {ADDR2LINE} (pass --addr2line, set DEVKITPRO, or install docker)")
    d = str(elf.resolve().parent)
    return ["docker", "run", "--rm", "-v", f"{d}:{d}:ro", DOCKER_IMAGE,
            f"/opt/devkitpro/devkitA64/bin/{ADDR2LINE}"]


def parse_addr2line(out: str) -> Dict[int, List[str]]:
    """`-a -p -f -i -C` output: `0x...: func at file:line`, then
    ` (inlined by) func at file:line` lines for the same address."""
    res: Dict[int, List[str]] = {}
    cur: Optional[int] = None
    for line in out.splitlines():
        m = re.match(r"^0x([0-9a-fA-F]+):\s*(.*)$", line)
        if m:
            cur = int(m.group(1), 16)
            res[cur] = [m.group(2).strip()]
        elif cur is not None and line.strip().startswith("(inlined by)"):
            res[cur].append(line.strip())
    return res


def run_addr2line(cmd: List[str], elf: Path, offsets: Sequence[int]) -> Dict[int, List[str]]:
    if not offsets:
        return {}
    args = cmd + ["-e", str(elf.resolve()), "-a", "-p", "-f", "-i", "-C"] + [
        f"0x{o:x}" for o in offsets]
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"addr2line failed ({r.returncode}): {r.stderr.strip()}")
    return parse_addr2line(r.stdout)


def short_path(s: str, full: bool) -> str:
    """Drop the build machine's checkout prefix from paths in addr2line output."""
    if full:
        return s

    def cut(m: re.Match) -> str:
        p = m.group(0)
        i = p.find("/build/decomp-src/")
        if i >= 0:
            return "decomp/" + p[i + len("/build/decomp-src/"):]
        for root in ("/platforms/", "/port/", "/vendor/", "/tools/"):
            i = p.find(root)
            if i >= 0:
                return p[i + 1:]
        return p

    return re.sub(r"/[^\s:]+", cut, s)


def lookup_offset(a: Addr) -> Optional[int]:
    if a.offset is None:
        return None
    return a.offset - 4 if a.is_return and a.offset >= 4 else a.offset


def format_rows(addrs: Sequence[Addr], sym: Dict[int, List[str]], full: bool,
                syms: Sequence[Tuple[int, int, str]] = ()) -> List[str]:
    rows = []
    for a in addrs:
        lo = lookup_offset(a)
        where = f"base+0x{a.offset:x}" if a.offset is not None else (
            f"0x{a.absolute:x}" if a.absolute is not None else "?")
        if lo is None:
            rows.append(f"  {a.label:<8} {where:<16} (outside the main module)")
            continue
        lines = sym.get(lo, ["??"])
        note = "  [call site]" if a.is_return else ""
        st = symbol_at(syms, lo) if syms else None
        entry = symbol_at(syms, a.offset) if (syms and a.is_return) else None
        if entry and entry.endswith("+0x0"):
            # A "return address" that is a function's first instruction was
            # never pushed by a call: crt0 enters main with lr = exit (the
            # __wrap_exit stub in the game image), so main's frame names it.
            note += (f"  [return address is the entry of {entry[:-4]}: not a "
                     f"call; crt0 sets main's lr to exit]")
        if st:
            note += f"  {{symtab {st}}}"
            outer = lines[-1].replace("(inlined by)", "").strip().split(" at ")[0]
            if outer != "??" and outer != st.split("+0x")[0]:
                note += "  [addr2line and symtab disagree: no debug info here?]"
        rows.append(f"  {a.label:<8} {where:<16} {short_path(lines[0], full)}{note}")
        for extra in lines[1:]:
            rows.append(f"  {'':<8} {'':<16}   {short_path(extra, full)}")
    return rows


def main(argv: Optional[Sequence[str]] = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("report", type=Path, help="crash_report.txt (or crash_report_prev.txt)")
    ap.add_argument("elf", type=Path, help="unstripped dw3-switch.elf of the same build")
    ap.add_argument("--creport", type=Path, help="Atmosphere crash_reports/*.log of the same crash")
    ap.add_argument("--base", help="module base (hex) for reports without base+ offsets")
    ap.add_argument("--addr2line", help="addr2line binary (default: devkitA64, else Docker)")
    ap.add_argument("--no-docker", action="store_true")
    ap.add_argument("--full-paths", action="store_true")
    ap.add_argument("--strict", action="store_true",
                    help="exit 3 when the .elf does not match the report")
    a = ap.parse_args(argv)

    rep = parse_report(a.report.read_text(errors="replace"))
    cre = parse_creport(a.creport.read_text(errors="replace")) if a.creport else None
    base = int(a.base, 16) if a.base else rep.base
    if base is None and cre is not None:
        base = cre.base
    # Old reports print absolute values only: rebase pc / lr / frames. The xN
    # registers keep the report's own base+ marks (code pointers only).
    resolve_offsets([r for r in rep.regs if r.label in ("pc", "lr")] + rep.frames, base)

    print(f"report:  {a.report}")
    for key in ("version", "git", "build_id", "reason", "mode_file", "stage_file", "disc"):
        if key in rep.fields:
            print(f"  {key}: {rep.fields[key]}")
    print(f"elf:     {a.elf}  build_id {elf_build_id(a.elf) or 'none'}")
    ok, warn = identity_checks(rep, a.elf)
    for line in ok:
        print(f"  ok: {line}")
    for line in warn:
        print(f"  WARNING: {line}", file=sys.stderr)
        print(f"  WARNING: {line}")
    if base is not None:
        src = "--base" if a.base else ("report" if rep.base else "creport")
        print(f"module_base: 0x{base:x} (from {src})")
    else:
        print("module_base: unknown (old report without base+ offsets: pass --creport or --base)")

    code_regs = [r for r in rep.regs if r.label in ("pc", "lr") or r.offset is not None]
    every = code_regs + rep.frames + (cre.addrs if cre else [])
    offsets = sorted({o for o in (lookup_offset(x) for x in every) if o is not None})
    sym = run_addr2line(addr2line_cmd(a.elf, a.addr2line, a.no_docker), a.elf, offsets) if offsets else {}
    syms = elf_text_symbols(a.elf)

    print("\nregisters:")
    for row in format_rows(code_regs, sym, a.full_paths, syms):
        print(row)
    print("\nbacktrace (frame records):")
    if rep.frames:
        for row in format_rows(rep.frames, sym, a.full_paths, syms):
            print(row)
    else:
        print("  (none in the report)")
    if "backtrace_end" in rep.fields:
        print(f"  end: {rep.fields['backtrace_end']}")
    if cre is not None:
        print(f"\ncreport crashed thread ({a.creport.name}):")
        if cre.addrs:
            for row in format_rows(cre.addrs, sym, a.full_paths, syms):
                print(row)
        else:
            print("  (no dw3-switch + offset lines)")
    if warn and a.strict:
        return 3
    return 0


if __name__ == "__main__":
    sys.exit(main())
