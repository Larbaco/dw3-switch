"""Changed-area scenario selection for tools/replay_tests.sh --changed.

A pre-PR aid for agents: map the files a branch touched to the replay
scenarios that can notice the change, so a UI/options/cheats change does not
pay for the whole ~245-scenario suite. CI and release checks still run the
full tier. The map is deliberately conservative:

* a file with no rule that is part of the game or the PC host (port/, patches,
  platforms/pc, vendor/, CMake, the runner) selects EVERYTHING (the full tier);
* `platforms/pc/src/main.c` is the PC host entry (main(), boot, the frame hooks)
  and selects EVERYTHING with its own reason, not a generic fallback;
* a CMake file under port/ or platforms/pc/ is read through its diff: if every
  changed line is a source-file entry (or a comment/blank), the selection comes
  from those sources through the rules below; any other change (flags,
  definitions, targets, conditions) or a missing diff selects the full tier;
* docs, tests, other tools and the Switch-only host select nothing by themselves;
* the quick tier and a few smoke scenarios always run.

Rules are (regex on the repo-relative path, scenario-name globs). The first
matching rule wins for that file. Globs are fnmatch patterns on scenario names.
"""
from __future__ import annotations

import fnmatch
import posixpath
import re
from dataclasses import dataclass, field

ALL = "ALL"      # sentinel: run the full tier
BUILD = "BUILD"  # sentinel: a CMake file, selected through the sources its diff adds or removes
HOST = "HOST"    # sentinel: the PC host entry, full tier with a reason that says so
NONE: tuple = ()

# Always run with a --changed selection: one boot-to-field and one battle on
# each region, a save round trip. Cheap, and they catch a broken build/boot.
SMOKE = ("us-newgame-field", "eu-newgame-field", "us-two-battles",
         "us-field-walk", "us-restart-save-continue")

_TITLE = ("*title*", "*lang*", "*return-title*")
# P6.37: other scenarios whose pictures include the title or an Options screen
_SCREENS = ("*restart*", "*load-menu*", "*nintendo*", "*map-rebind*")
_OPTIONS = ("*options*", "*persist*", "*text-*", "*encounter*", "*pacing*",
            "*volume*", "*cheat*", "*autosave*", "*turbo*")

# P10.6: the screens that draw a font button icon (shops, the lab, the gym, the
# field menu's pages, the save screen) and the glyph scenarios themselves.
_ICONS = ("*glyphs*", "*shop*", "*save*", "*lab*", "*gym*", "*menu-screens*", "*items*")

# The field stages and what registers them (port/overlays tables and sections,
# the stage patches): the scenarios that boot or cross a stage. A mistake in one
# table row loads as "file id not in the overlay table", which strict replay
# turns into an abort; tools/tests/test_overlay_table_stages.py checks the
# registry itself against the decomp.
_STAGES = ("*stage*", "*walkthrough-p*", "*newgame*", "*field-walk*", "*underground*",
           "*door*", "*bridge*", "*two-battles*", "*encounter-1x*", "*cheat-warp*")

# (regex, globs). Order matters. Patch rules key on the patch's topic name.
RULES: list[tuple[str, object]] = [
    # --- nothing to replay -------------------------------------------------
    (r"^(docs/|notes/|LICENSE|CONTRIBUTING|\.github/|\.gitignore|\.gitmodules)", NONE),
    (r".*\.md$", NONE),
    (r"^tools/tests/", NONE),
    (r"^tools/(mods|guide|memaudit|compare_harness|testdata|audio_|menu_art|docker)", NONE),
    (r"^tools/[^/]*\.(sh|py|c)$", NONE),        # build/dev helpers (not tools/replay/)
    (r"^platforms/switch/", NONE),              # Switch host: Ryubing, not the PC replay
    (r"^port/help/", ("*help*", "*options*", "*walkthrough*")),
    (r"^port/include/port_(help|pages|walkthrough|walkthrough_ui)\.h$", ("*help*", "*walkthrough*", "*options*")),
    (r"^port/include/port_options_groups\.h$", _OPTIONS + _TITLE + ("*help*", "*walkthrough*", "*controls*")),
    # the options registry's header (the enum of the rows and the accessors): an additive change
    # (a row, a getter) alters no behaviour by itself, so it selects what reads the registry and
    # draws its screens, not the full tier (P6.32 phase 4: the Full-screen map row, a text getter)
    (r"^port/include/port_options\.h$", _OPTIONS + _TITLE + ("*help*", "*walkthrough*", "*controls*",
                                                            "*nintendo*", "*map*", "*glyphs*", "*p79*", "*ws-*")),
    # the native Options art and its baked lettering: every screen of the Options shell
    # P6.37: the version line is in every picture of the title and Options, so it also
    # reaches the scenarios that pass them (restart, the load menu, the Nintendo layout)
    (r"^platforms/pc/src/title_overlay_(art|lettering)", _OPTIONS + _TITLE + _SCREENS + ("*help*", "*walkthrough*", "*controls*")),
    (r"^port/pages/", ("*help*", "*walkthrough*", "*options*")),
    # P6.37: the module that writes the version header (before the build-file rule)
    (r"^port/version/version\.cmake$", _OPTIONS + _TITLE + _SCREENS),
    # --- build files: only the source-list entries of the diff count -----------
    (r"^(port|platforms/pc)/(.*/)?(CMakeLists\.txt|[^/]+\.cmake)$", BUILD),
    # main() and the frame hooks (boot, region pick, pad, turbo pace, vsync, options
    # flush) run for every scenario, and a change there is not confined to one block
    # (no area rule can say which scenarios a boot-path edit reaches), so: full tier.
    (r"^platforms/pc/src/main\.c$", HOST),
    # --- areas by directory ------------------------------------------------
    (r"^port/walkthrough/", ("*walkthrough*",)),
    (r"^port/autosave/", ("*autosave*", "*title-continue*")),
    (r"^port/battle_menu/", ("*p79*", "*battle-options*", "*battle-list-bar*")),
    (r"^port/(items_menu/|include/port_items_menu\.h)", ("*items*", "*menu-screens*")),
    (r"^port/(map_view/|include/port_map_view\.h$)", ("*map*",)),
    # the PC overlay of that screen (P6.32 phase 3 touches it for the FIT backdrop)
    (r"^platforms/pc/src/map_view_overlay\.c$", ("*map*",)),
    (r"^port/saves/|^port/memcard/", ("*save*", "xr-*", "*continue*", "*restart*", "*autosave*")),
    (r"^port/pacing/", ("*text-*", "*encounter*", "*pacing*", "*turbo*")),
    (r"^port/region/", ("*lang*", "xr-*", "*title*")),
    (r"^port/sweep/", ("*stage-sweep*",)),
    (r"^port/overlays/overlay_(table_(us|eu)|sections(_eu_extra)?)\.c$", _STAGES),
    (r"^port/patches/\d+-PORT_HOST-wstag", _STAGES),
    (r"^port/file_cache/", ("*file-cache*",)),
    (r"^port/options/cheat", ("*cheat*",)),
    (r"^port/options/lang_select", ("*lang*", "*title*")),
    (r"^port/options/(widescreen|screen_fade)", ("*ws-*", "*iris*", "*title-intro*")),
    (r"^port/options/turbo", ("*turbo*",)),
    (r"^port/options/restart", ("*restart*",)),
    # P9.8: the layout map. Replays bypass it (except the *nintendo* scenarios,
    # which turn it on with DW3_REPLAY_SOURCE=1), so the default is identical by
    # construction; the screens and the scenarios that read buttons are the check.
    (r"^port/options/button_map", _OPTIONS + _TITLE + ("*nintendo*", "*controls*", "*help*", "*map-view*")),
    (r"^port/psyz_missing/psyz_pad_host", _OPTIONS + _TITLE + ("*nintendo*", "*controls*", "*help*", "*map*",
                                                                 "*menu*", "*items*", "*battle*", "*restart*",
                                                                 "*quit*", "*card*", "*shop*", "*lab*", "*deck*")),
    # P10.6: with the option off the FONT.load wrapper does nothing and the text hook is a
    # flag test, so the icon screens, the Options shell and the glyph scenarios are the check.
    (r"^port/options/switch_glyph|^port/include/port_glyphs\.h$",
     _OPTIONS + _TITLE + _ICONS + ("*controls*", "*nintendo*")),
    (r"^port/patches/\d+-PORT_HOST-p106-", _ICONS + ("*text-*",)),
    (r"^port/options/title_", _TITLE),
    # P6.37: the port version text (Options footer, title corner); replays pin it to "replay"
    (r"^port/(version/|include/port_version\.h$)", _OPTIONS + _TITLE + _SCREENS),
    (r"^port/options/(field_options|battle_options)", ("*options*",)),
    (r"^port/options/(port_options|options_cfg|confirm)", _OPTIONS + _TITLE),
    (r"^port/ui/", _OPTIONS + _TITLE + ("*items*", "*menu-screens*")),
    # --- decomp patches, by topic -------------------------------------------
    (r"^port/patches/\d+-PORT_HOST-p635-walkthrough", ("*walkthrough*",)),
    (r"^port/patches/\d+-PORT_HOST-p620-cheats", ("*cheat*",)),
    (r"^port/patches/\d+-PORT_HOST-p631-", ("*autosave*", "*title-continue*")),
    (r"^port/patches/\d+-PORT_HOST-p632-map", ("*map*",)),
    # P6.32 phase 6: the field hotkey's poll sits in the field task's main state (every field
    # scenario runs it, and does nothing unless the key goes down) and shares the function with
    # the Start menu, the cheats and the autosave hooks
    (r"^port/patches/\d+-PORT_HOST-p632-field", _STAGES + ("*map*", "*turbo*", "*encounter*", "*autosave*",
                                                          "*cheat*", "*menu-screens*", "*items*", "*options*",
                                                          "*restart*", "*walkthrough*")),
    (r"^port/patches/\d+-PORT_HOST-p63[34]-", ("*items*", "*menu-screens*")),
    (r"^port/patches/\d+-PORT_HOST-p77-", ("*p77*",)),
    (r"^port/patches/\d+-PORT_HOST-p79-", ("*p79*",)),
    (r"^port/patches/\d+-PORT_HOST-p51-battle", ("*battle-list-bar*", "*p79*", "*p77*", "*two-battles*", "*battle-options*")),
    (r"^port/patches/\d+-PORT_HOST-p61-widescreen-battle-spiral", ("*spiral*",)),
    (r"^port/patches/\d+-PORT_HOST-p61-widescreen-area-change-iris", ("*iris*",)),
    (r"^port/patches/\d+-PORT_HOST-p61-widescreen-map-screen", ("*map*",)),
    (r"^port/patches/\d+-PORT_HOST-p61-widescreen-memcard-slots", ("*save*",)),
    (r"^port/patches/\d+-PORT_HOST-p61-widescreen-(card-|gym|digimon-lab|item-shop)",
     ("*card*", "*deck*", "*gym*", "*lab*", "*shop*")),
    (r"^port/patches/\d+-PORT_HOST-p61-widescreen-(language-select|title-backdrop)", _TITLE),
    (r"^port/patches/\d+-PORT_HOST-p61-language-select", ("*lang*",)),
    (r"^port/patches/\d+-PORT_HOST-p62-eu-country", ("*lang*",)),
    (r"^port/patches/\d+-PORT_HOST-p92-", _TITLE + ("*options*",)),
    (r"^port/patches/\d+-PORT_HOST-p617-", ("*volume*", "*music*", "*options-sound*")),
    (r"^port/patches/\d+-PORT_HOST-p618-", ("*text-*", "*encounter*", "*banner*", "*persist*")),
    (r"^port/patches/\d+-PORT_HOST-p627-", ("*title-quit*", "*return-title*")),
    (r"^port/patches/\d+-PORT_HOST-p69-restart", ("*restart*",)),
    (r"^port/patches/\d+-PORT_HOST-p64-", ("*turbo*",)),
    (r"^port/patches/\d+-PORT_HOST-(p38|p55)-stage-sweep", ("*stage-sweep*",)),
    (r"^port/patches/\d+-PORT_HOST-(items-|p633)", ("*items*",)),
    (r"^port/patches/\d+-PORT_HOST-(field-)?file-cache", ("*file-cache*",)),
    (r"^port/patches/DECOMP_PIN$", ALL),
    # --- the replay infrastructure itself: quick + smoke is the sanity check -
    (r"^tools/replay/scenarios/", "scenario-recording"),
    (r"^tools/replay/(manifest|gen_scenarios|ryubing|areas|map_names_check)\.py$", NONE),
    (r"^tools/(replay_tests\.sh|replay/replay_tests\.py)$", NONE),
]


@dataclass
class Selection:
    names: set = field(default_factory=set)
    everything: bool = False
    reasons: list = field(default_factory=list)   # (file, why)
    unmapped_globs: list = field(default_factory=list)


def _phase_owners(scenarios):
    out = {}
    for sc in scenarios:
        for p in sc.phases:
            out.setdefault(p, set()).add(sc.name)
    return out


_SRC_EXT = r"(?:c|h|cc|cpp|S|inc)"
_SRC_LINE = re.compile(
    r'^"?(?:\$\{(?P<var>[A-Za-z0-9_]+)\}/)?(?P<path>[\w./+-]+\.' + _SRC_EXT + r')"?\s*(?:#.*)?$')
# P6.37: the version wiring of a host target (an include of the module and one call)
# adds port/version/port_version.c to it, so it counts as a source entry.
_VERSION_WIRING = re.compile(
    r'^(?:include\(.*port/version/version\.cmake\)|dw3_version_setup\([\w-]+\))\s*(?:#.*)?$')
_CMAKE_ROOTS = {"CMAKE_CURRENT_SOURCE_DIR": None, "CMAKE_SOURCE_DIR": "platforms/pc", "DW3_ROOT": ""}


def build_file_sources(path, diff_text):
    """Sources added or removed by a CMake file's diff (zero-context unified diff).

    Returns the repo-relative source paths when EVERY changed line is a source
    entry (`"${CMAKE_CURRENT_SOURCE_DIR}/x/y.c"`, `src/y.c`, ...), a comment or
    blank; None when anything else changed or an entry cannot be resolved, which
    the caller treats as "unknown: full tier"."""
    here = path.rsplit("/", 1)[0] if "/" in path else ""
    out = []
    for line in diff_text.splitlines():
        if line.startswith(("+++", "---", "@@", "diff ", "index ", "\\")):
            continue
        if not line.startswith(("+", "-")):
            return None   # context lines are not expected with -U0
        body = line[1:].strip()
        if not body or body.startswith("#"):
            continue
        if _VERSION_WIRING.match(body):
            out.append("port/version/port_version.c")
            continue
        m = _SRC_LINE.match(body)
        if not m:
            return None
        var, rel = m.group("var"), m.group("path")
        if var is not None and var not in _CMAKE_ROOTS:
            return None
        base = here if var in (None, "CMAKE_CURRENT_SOURCE_DIR") else _CMAKE_ROOTS[var]
        full = posixpath.normpath(posixpath.join(base, rel))
        if full.startswith(("..", "/")):
            return None
        out.append(full)
    return sorted(set(out))


def select(files, scenarios, diff_for=None) -> Selection:
    """files: repo-relative paths. scenarios: manifest.Scenario list.
    diff_for(path) -> zero-context diff text of a CMake file (or None): lets a
    build file select through the sources it adds or removes.
    Returns the names to run on top of the always-on quick tier + SMOKE."""
    sel = Selection()
    full_tier = {s.name for s in scenarios if s.tier in ("quick", "full")}
    owners = _phase_owners(scenarios)
    for path in files:
        for rx, what in RULES:
            if not re.search(rx, path):
                continue
            if what is ALL:
                sel.everything = True
                sel.reasons.append((path, "pin/engine-wide: full tier"))
            elif what is HOST:
                sel.everything = True
                sel.reasons.append((path, "PC host entry (main(), boot, frame loop, options "
                                          "wiring): full tier"))
            elif what is BUILD:
                diff = diff_for(path) if diff_for else None
                srcs = None if diff is None else build_file_sources(path, diff)
                if srcs is None:
                    sel.everything = True
                    sel.reasons.append((path, "build file changed beyond its source lists "
                                              "(or no diff): full tier"))
                else:
                    sub = select(srcs, scenarios)
                    sel.names |= sub.names
                    sel.everything |= sub.everything
                    sel.unmapped_globs += sub.unmapped_globs
                    sel.reasons.append((path, "source list only: " + (", ".join(srcs) or "comments")))
            elif what == "scenario-recording":
                phase = path.rsplit("/", 1)[-1].removesuffix(".rec")
                got = owners.get(phase, set()) & full_tier  # nx and both scenarios run elsewhere
                sel.names |= got
                sel.reasons.append((path, "recording of " + (", ".join(sorted(got)) or "no scenario")))
            elif what:
                hit = set()
                for g in what:
                    m = {n for n in full_tier if fnmatch.fnmatchcase(n, g)}
                    if not m:
                        sel.unmapped_globs.append(g)
                    hit |= m
                sel.names |= hit
                sel.reasons.append((path, f"{len(hit)} scenario(s)"))
            else:
                sel.reasons.append((path, "no replay impact"))
            break
        else:
            # Unknown file: if it can reach the PC game, be safe.
            if re.match(r"^(port/|platforms/pc/|platforms/switch/launcher/|vendor/|CMake|tools/replay/)", path):
                sel.everything = True
                sel.reasons.append((path, "no rule: full tier"))
            else:
                sel.reasons.append((path, "outside the game: no replay impact"))
    if sel.everything:
        sel.names = set(full_tier)
    sel.names |= {s.name for s in scenarios if s.tier == "quick"}
    sel.names |= {n for n in SMOKE if n in {s.name for s in scenarios}}
    return sel



def scenario_area(name):
    """The area of a scenario: the regex of the first RULES entry whose globs
    match its name (rules with no globs are skipped); a scenario no rule matches
    is its own area."""
    for rx, what in RULES:
        if isinstance(what, tuple) and what and any(fnmatch.fnmatchcase(name, g) for g in what):
            return rx
    return "own:" + name


def both_subset(scenarios, override_names=()):
    """The both-binary subset (docs/P6.15_REPLAY_TESTS.md): one PC scenario per
    (area, region), plus every scenario with a `both/` refs override.

    Pool: tiers quick, full and both (not nx, single or soak). The representative
    prefers tier `both`, then `quick`, then the name order."""
    pool = [s for s in scenarios if s.tier in ("quick", "full", "both")]
    groups: dict = {}
    for s in pool:
        groups.setdefault((scenario_area(s.name), s.version), []).append(s)
    chosen = {min(m, key=lambda s: (s.tier != "both", s.tier != "quick", s.name)).name
              for m in groups.values()}
    chosen |= set(override_names) & {s.name for s in pool}
    return sorted(chosen)
