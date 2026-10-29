"""Changed-area scenario selection for tools/replay_tests.sh --changed.

A pre-PR aid for agents: map the files a branch touched to the replay
scenarios that can notice the change, so a UI/options/cheats change does not
pay for the whole ~245-scenario suite. CI and release checks still run the
full tier. The map is deliberately conservative:

* a file with no rule that is part of the game or the PC host (port/, patches,
  platforms/pc, vendor/, CMake, the runner) selects EVERYTHING (the full tier);
* docs, tests, other tools and the Switch-only host select nothing by themselves;
* the quick tier and a few smoke scenarios always run.

Rules are (regex on the repo-relative path, scenario-name globs). The first
matching rule wins for that file. Globs are fnmatch patterns on scenario names.
"""
from __future__ import annotations

import fnmatch
import re
from dataclasses import dataclass, field

ALL = "ALL"      # sentinel: run the full tier
NONE: tuple = ()

# Always run with a --changed selection: one boot-to-field and one battle on
# each region, a save round trip. Cheap, and they catch a broken build/boot.
SMOKE = ("us-newgame-field", "eu-newgame-field", "us-two-battles",
         "us-field-walk", "us-restart-save-continue")

_TITLE = ("*title*", "*lang*", "*return-title*")
_OPTIONS = ("*options*", "*persist*", "*text-*", "*encounter*", "*pacing*",
            "*volume*", "*cheat*", "*autosave*", "*turbo*")

# (regex, globs). Order matters. Patch rules key on the patch's topic name.
RULES: list[tuple[str, object]] = [
    # --- nothing to replay -------------------------------------------------
    (r"^(docs/|notes/|LICENSE|CONTRIBUTING|\.github/|\.gitignore|\.gitmodules)", NONE),
    (r".*\.md$", NONE),
    (r"^tools/tests/", NONE),
    (r"^tools/(mods|guide|memaudit|compare_harness|testdata|audio_|menu_art|docker)", NONE),
    (r"^tools/[^/]*\.(sh|py|c)$", NONE),        # build/dev helpers (not tools/replay/)
    (r"^platforms/switch/", NONE),              # Switch host: Ryubing, not the PC replay
    (r"^port/help/", ("*help*", "*options*")),
    (r"^port/pages/", ("*help*", "*walkthrough*", "*options*")),
    # --- areas by directory ------------------------------------------------
    (r"^port/walkthrough/", ("*walkthrough*",)),
    (r"^port/autosave/", ("*autosave*", "*title-continue*")),
    (r"^port/battle_menu/", ("*p79*", "*battle-options*", "*battle-list-bar*")),
    (r"^port/(items_menu/|include/port_items_menu\.h)", ("*items*", "*menu-screens*")),
    (r"^port/map_view/", ("*map*",)),
    (r"^port/saves/|^port/memcard/", ("*save*", "xr-*", "*continue*", "*restart*", "*autosave*")),
    (r"^port/pacing/", ("*text-*", "*encounter*", "*pacing*", "*turbo*")),
    (r"^port/region/", ("*lang*", "xr-*", "*title*")),
    (r"^port/sweep/", ("*stage-sweep*",)),
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
    (r"^port/options/title_", _TITLE),
    (r"^port/options/(field_options|battle_options)", ("*options*",)),
    (r"^port/options/(port_options|options_cfg|confirm)", _OPTIONS + _TITLE),
    (r"^port/ui/", _OPTIONS + _TITLE + ("*items*", "*menu-screens*")),
    # --- decomp patches, by topic -------------------------------------------
    (r"^port/patches/\d+-PORT_HOST-p635-walkthrough", ("*walkthrough*",)),
    (r"^port/patches/\d+-PORT_HOST-p620-cheats", ("*cheat*",)),
    (r"^port/patches/\d+-PORT_HOST-p631-", ("*autosave*", "*title-continue*")),
    (r"^port/patches/\d+-PORT_HOST-p632-map", ("*map*",)),
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
    (r"^tools/replay/(manifest|gen_scenarios|ryubing|areas)\.py$", NONE),
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


def select(files, scenarios) -> Selection:
    """files: repo-relative paths. scenarios: manifest.Scenario list.
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
