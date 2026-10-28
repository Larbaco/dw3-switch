"""P6.24: tools/localize_region_obj.py packs one game copy into an object whose
only global is its renamed dw3_game_api, with its own restore sections, so two
copies built from the same sources link into one program.

The fixture is two copies of the same C file (REGION 1 and 2) that define the
same globals, the same writable overlay section and use __start_/__stop_ of it,
plus a host that calls into them through the table. Linking the copies raw must
fail (that is the problem); after localize_region_obj.py it must link and the
copies must not see each other."""

from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools" / "localize_region_obj.py"

GAME_C = r"""
typedef struct Api {
    int region;
    int (*get)(void);
    void (*bump)(void);
    long (*span)(void);
    int (*call_host)(void);
} Api;

extern int host_note(int);           /* host symbol, stays undefined */

int shared_name = REGION * 100;      /* same global name in both copies */
static int counter;
int dw3_internal_helper(void) { return REGION * 7; }

/* a restore section, as tools/rename_ovl_sections.py makes them */
__attribute__((section("dw3data_fieldstg"))) int overlay_word = REGION;
__attribute__((section("dw3bss_fieldstg"))) int overlay_bss;
extern char __start_dw3data_fieldstg[], __stop_dw3data_fieldstg[];
extern char __start_dw3bss_fieldstg[], __stop_dw3bss_fieldstg[];

static int get(void) { return shared_name + counter + dw3_internal_helper(); }
static void bump(void) { counter++; overlay_bss += 2; }
static long span(void) {
    return (__stop_dw3data_fieldstg - __start_dw3data_fieldstg) * 1000 +
           (__stop_dw3bss_fieldstg - __start_dw3bss_fieldstg);
}
static int call_host(void) { return host_note(REGION); }

const Api dw3_game_api = {REGION, get, bump, span, call_host};
"""

HOST_C = r"""
#include <stdio.h>
typedef struct Api {
    int region;
    int (*get)(void);
    void (*bump)(void);
    long (*span)(void);
    int (*call_host)(void);
} Api;
extern const Api us_dw3_game_api, eu_dw3_game_api;
int host_note(int v) { return v + 1000; }
int main(void) {
    const Api *us = &us_dw3_game_api, *eu = &eu_dw3_game_api;
    us->bump();
    us->bump();
    eu->bump();
    /* get = shared_name + counter + helper; span = data bytes * 1000 + bss bytes */
    printf("us get=%d eu get=%d\n", us->get(), eu->get());
    printf("us span=%ld eu span=%ld\n", us->span(), eu->span());
    printf("us host=%d eu host=%d\n", us->call_host(), eu->call_host());
    return 0;
}
"""


def run(cmd, **kw):
    r = subprocess.run([str(c) for c in cmd], capture_output=True, text=True, **kw)
    return r


@pytest.fixture(scope="module")
def parts(tmp_path_factory) -> dict:
    d = tmp_path_factory.mktemp("localize")
    (d / "game.c").write_text(GAME_C)
    (d / "host.c").write_text(HOST_C)
    out = {"dir": d}
    for name, region in (("us", 1), ("eu", 2)):
        o = d / f"game_{name}.o"
        r = run(["gcc", "-c", "-fno-common", f"-DREGION={region}", "-o", o, d / "game.c"])
        assert r.returncode == 0, r.stderr
        a = d / f"libgame_{name}.a"
        r = run(["ar", "rcs", a, o])
        assert r.returncode == 0, r.stderr
        out[name] = a
        out[name + "_o"] = o
    r = run(["gcc", "-c", "-o", d / "host.o", d / "host.c"])
    assert r.returncode == 0, r.stderr
    out["host"] = d / "host.o"
    return out


def localize(parts, name: str):
    out = parts["dir"] / f"packed_{name}.o"
    r = run(["python3", TOOL, "--region", name, "--out", out, "--archive", parts[name]])
    return r, out


def test_raw_link_of_two_copies_fails(parts):
    """The problem the tool solves: same globals, same sections."""
    r = run(["gcc", "-o", parts["dir"] / "raw", parts["host"], parts["us_o"], parts["eu_o"]])
    assert r.returncode != 0
    assert "multiple definition" in r.stderr or "undefined reference" in r.stderr


def test_two_localized_copies_link_and_stay_apart(parts):
    outs = []
    for name in ("us", "eu"):
        r, out = localize(parts, name)
        assert r.returncode == 0, r.stderr
        assert "1 export(s)" in r.stderr
        outs.append(out)
        # the only global that is left is the renamed table
        g = run(["nm", "-g", "--defined-only", out]).stdout.split()
        assert g[-1] == f"{name}_dw3_game_api" and len(g) == 3, g
        # the host's symbol is still undefined and global
        assert "host_note" in run(["nm", "-g", "--undefined-only", out]).stdout
        # sections and __start_/__stop_ references carry the region
        secs = run(["objdump", "-h", out]).stdout
        assert f"dw3data_{name}_fieldstg" in secs and f"dw3bss_{name}_fieldstg" in secs
        und = run(["nm", "-u", out]).stdout
        assert f"__start_dw3data_{name}_fieldstg" in und
        assert "__start_dw3data_fieldstg" not in und.replace(f"__start_dw3data_{name}_fieldstg", "")
    exe = parts["dir"] / "both"
    r = run(["gcc", "-o", exe, parts["host"], *outs])
    assert r.returncode == 0, r.stderr
    r = run([exe])
    assert r.returncode == 0, r.stderr
    # us: 100 + 2 bumps + 7 ; eu: 200 + 1 bump + 14. Each copy's restore section
    # holds only its own int (4 bytes) and bss int (4 bytes): 4004, not 8008.
    assert "us get=109 eu get=215" in r.stdout, r.stdout
    assert "us span=4004 eu span=4004" in r.stdout, r.stdout
    assert "us host=1001 eu host=1002" in r.stdout, r.stdout


def test_missing_export_is_an_error(parts):
    out = parts["dir"] / "bad.o"
    r = run(["python3", TOOL, "--region", "us", "--out", out, "--export", "nope",
             "--archive", parts["us"], "--root", "dw3_game_api"])
    assert r.returncode != 0
