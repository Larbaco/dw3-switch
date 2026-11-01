#!/usr/bin/env python3
"""P6.7 text extraction and checking for a local PT-BR pack (docs/LOCALIZATION.md).

Extract (reads the owner's runtime, writes an editable template OUTSIDE git):

  python3 tools/l10n/extract_text.py                        both regions that exist
  python3 tools/l10n/extract_text.py --region eu --refs fr,de,it,es
  python3 tools/l10n/extract_text.py --stats                counts only, writes nothing

  The template is one JSON file per game text file under
  <out>/<region>/<STEM>.json (default <out> = tmp/l10n/ptbr, git-ignored),
  each entry {"key", "en", "pt": "", "limit_px", "max_lines", ...}. The
  translator fills "pt". An empty "pt" means: keep the English string.

Check (validates a translated template; needs no game data):

  python3 tools/l10n/extract_text.py --check tmp/l10n/ptbr/eu
  python3 tools/l10n/extract_text.py --check FILE.json --strict --glossary glossary.json
  python3 tools/l10n/extract_text.py --check DIR --against DIR_OF_FRESH_TEMPLATE
  python3 tools/l10n/extract_text.py --propagate DIR        copy a translation to
                                                           entries with the same English

Never commit the template or the translation: they contain the game's text.
The tool refuses to write inside the repository unless the path is git-ignored.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))

import textfmt as T  # noqa: E402

FORMAT = "dw3-l10n-v1"
DEFAULT_RUNTIME = Path.home() / ".cache" / "dw3-switch" / "runtime"
DEFAULT_OUT = ROOT / "tmp" / "l10n" / "ptbr"
# The row of language directories on the disc, by LANGUAGE id (file.h TEXT_FILE).
EU_LANGS = {"en": ("ENG", 2), "fr": ("FRA", 3), "it": ("ITA", 4), "de": ("GER", 5), "es": ("SPN", 6)}
US_LANG = ("USA", 1)
MAX_LINES_DIALOGUE = 3  # createMessageBox / createTalkBox setLines(3)
DIALOGUE_STYLE = 1  # lines of 14 px; names and menus use styles 2 and 3, measured in 1 for a common scale

CATEGORY = {
    "names": ("DIGNAM", "ITMNAM", "SKLNAM", "SHPNAM", "HTLNAM", "STNAME", "STAREA", "CARDNM"),
    "descriptions": ("DIGINF", "ITMINF", "SKLINF", "CARDST"),
    "menus": ("AMTMAP", "ASKMAP", "CARDGM", "CRDABM", "CRDDEK", "CRDSHP", "DGLABO", "DGNMET",
              "FGTRPT", "ITSHOP", "MEMCRD", "MFIGHT", "NAMEDT", "NAMEET", "STATUS", "SYSTEM", "TRAINI"),
}
CATEGORIES = ("names", "descriptions", "menus", "dialogue_talk", "dialogue_event", "other")


# Strings the engine copies into a fixed buffer (docs/LOCALIZATION.md section 2.6a). A longer one
# overwrites what follows: the card title crashed the first synthetic pack (stack smashing).
# stem, first index, last index (None = every string), limit in bytes of the game's encoding
# (an accented letter is an icon, 2 bytes), what the buffer is.
BUFFER_LIMITS = (
    ("NAMEDT", 11, 11, 23, "the new game's player name, GAME.name[0x18] (initNewGameData)"),
    ("CRDSHP", 22, 24, 21, "a starter deck's name, Deck.name[0x16] (initNewGameData)"),
    ("DIGNAM", 0, None, 23, "a partner's name, PartnerStats.name[0x18] (initNewGameData)"),
)
CARD_TITLE = ("MEMCRD", 39, 32)  # the save file's title: converted to 2-byte Shift-JIS into char title[0x48]
SPEAKER_MAX = 31                  # a talk box's speaker name, between 02 07 codes: char name[0x20]


def buffer_problem(key: str, text: str, codec: T.Codec) -> str | None:
    """Why `text` (the editable text of string `key`, "STEM:index" or "STEM:table:index") would overflow a
    fixed buffer of the engine, or None."""
    parts = key.split(":")
    stem = parts[0]
    try:
        index = int(parts[-1])
        raw = T.encode(text, codec)
    except (ValueError, T.EncodeError):
        return None
    if len(parts) == 2 and stem == CARD_TITLE[0] and index == CARD_TITLE[1]:
        chars = len(re.sub(r"\{[^}]*\}", "", text).replace("\n", ""))
        if chars > CARD_TITLE[2]:
            return f"{chars} characters, the save file's title buffer holds {CARD_TITLE[2]} (STGMCARD_loadFiles)"
        if any(ord(c) > 127 for c in text):
            return "an accented letter has no Shift-JIS code: it is dropped from the save file's title"
    if len(parts) == 2:
        for st, lo, hi, limit, what in BUFFER_LIMITS:
            if stem == st and lo <= index and (hi is None or index <= hi) and len(raw) > limit:
                return f"{len(raw)} bytes, {what} holds {limit}"
    if stem.startswith(("TALK", "DMG")) and "{start}" in text:
        speaker = T.encode(text.split("{start}", 1)[0], codec)
        if len(speaker) > SPEAKER_MAX:
            return f"the speaker's name is {len(speaker)} bytes, the talk box's buffer holds {SPEAKER_MAX}"
    return None


def category_of(stem: str) -> str:
    for cat, stems in CATEGORY.items():
        if stem in stems:
            return cat
    if stem.startswith("TALK"):
        return "dialogue_talk"
    if stem.startswith("DMG"):
        return "dialogue_event"
    return "other"


def stem_of(name: str) -> str:
    """ESITMNAM.BIN, USITMNAM.BIN, SSITMNAM.BIN (Spanish) ... -> ITMNAM (a two-letter language prefix)."""
    base = Path(name).stem
    return base[2:] if len(base) > 4 and base[1] == "S" and base[0] in "EFDISUM" else base


def words_of(text: str) -> int:
    plain = re.sub(r"\{[^}]*\}", " ", text)
    return len([w for w in plain.split() if re.search(r"\w", w)])


# --------------------------------------------------------------------------
# reading the owner's runtime

def read_language(runtime: Path, region: str, lang_dir: str) -> dict[str, bytes]:
    """{STEM: bytes} of one language row of a region's runtime."""
    manifest = json.loads((runtime / region / f"files_{region}.json").read_text())["files"]
    out = {}
    for rec in manifest.values():
        path = rec["path"]
        if f"/COUNTRY/{lang_dir}/" in path:
            f = runtime / region / path
            if f.exists() and f.stat().st_size > 4:
                out[stem_of(path)] = f.read_bytes()
    return out


def file_ids(runtime: Path, region: str, lang_dir: str) -> dict[str, int]:
    manifest = json.loads((runtime / region / f"files_{region}.json").read_text())["files"]
    return {stem_of(r["path"]): int(k) for k, r in manifest.items() if f"/COUNTRY/{lang_dir}/" in r["path"]}


def build_template(region: str, runtime: Path, metrics: dict[int, T.Metrics] | None,
                   refs: tuple[str, ...] = ()) -> dict[str, dict]:
    """{STEM: template dict} for one region (English, with limits from the other EU languages)."""
    codec = T.codec_for(region)
    lang_dir = EU_LANGS["en"][0] if region == "eu" else US_LANG[0]
    ids = file_ids(runtime, region, lang_dir)
    en = read_language(runtime, region, lang_dir)
    others = {}
    if region == "eu":
        for code, (d, _) in EU_LANGS.items():
            if code != "en":
                others[code] = read_language(runtime, region, d)
    style = metrics[DIALOGUE_STYLE] if metrics else None
    result = {}
    for stem in sorted(en):
        cont = T.parse_container(en[stem])
        parsed_others = {}
        for code, files in others.items():
            if stem in files:
                try:
                    parsed_others[code] = T.parse_container(files[stem])
                except T.TextFormatError:
                    pass
        entries = []
        for t, i, raw in cont.strings():
            key = "%s:%d:%d" % (stem, t, i) if cont.kind == "archive" else "%s:%d" % (stem, i)
            text = T.decode(raw, codec)
            e = {"key": key, "en": text}
            if style is not None and region == "eu":
                widths = [max(T.line_widths(raw, style))]
                lines = [max(T.page_lines(raw))]
                for code, oc in parsed_others.items():
                    try:
                        oraw = oc.tables[t][i]
                    except IndexError:
                        continue
                    widths.append(max(T.line_widths(oraw, style)))
                    lines.append(max(T.page_lines(oraw)))
                e["limit_px"] = max(widths)
                e["max_lines"] = max(lines)
            elif style is not None:
                e["limit_px"] = None
                e["max_lines"] = max(T.page_lines(raw))
            for code in refs:
                oc = parsed_others.get(code)
                if oc is not None:
                    try:
                        e[code] = T.decode(oc.tables[t][i], codec)
                    except (IndexError, T.TextFormatError):
                        pass
            e["pt"] = ""
            entries.append(e)
        result[stem] = {"format": FORMAT, "region": region, "stem": stem, "file_id": ids.get(stem),
                        "container": cont.kind, "category": category_of(stem), "entries": entries}
    return result


def stats(templates: dict[str, dict]) -> dict:
    """Counts per category and in total (strings, non-empty strings, unique, words, characters)."""
    cats: dict[str, Counter] = defaultdict(Counter)
    seen: dict[str, set] = defaultdict(set)
    for tpl in templates.values():
        c = cats[tpl["category"]]
        c["files"] += 1
        for e in tpl["entries"]:
            c["strings"] += 1
            if e["en"].strip():
                c["nonempty"] += 1
                c["words"] += words_of(e["en"])
                c["chars"] += len(re.sub(r"\{[^}]*\}", "", e["en"]))
                seen[tpl["category"]].add(e["en"])
    for cat, s in seen.items():
        cats[cat]["unique"] = len(s)
    total: Counter = Counter()
    unique_all: set = set()
    for cat, c in cats.items():
        total.update(c)
    for s in seen.values():
        unique_all |= s
    total["unique"] = len(unique_all)
    return {"categories": {k: dict(v) for k, v in sorted(cats.items())}, "total": dict(total)}


def format_stats(st: dict, region: str) -> str:
    cols = ("files", "strings", "nonempty", "unique", "words", "chars")
    lines = ["%-15s" % region + "".join("%10s" % c for c in cols)]
    for cat in CATEGORIES:
        c = st["categories"].get(cat)
        if c:
            lines.append("%-15s" % cat + "".join("%10d" % c.get(k, 0) for k in cols))
    lines.append("%-15s" % "TOTAL" + "".join("%10d" % st["total"].get(k, 0) for k in cols))
    return "\n".join(lines)


# --------------------------------------------------------------------------
# writing (never inside git)

def is_git_ignored(path: Path) -> bool:
    try:
        path.resolve().relative_to(ROOT)
    except ValueError:
        return True  # outside the repository
    probe = path / ".probe"
    return subprocess.run(["git", "check-ignore", "-q", str(probe)], cwd=ROOT).returncode == 0


def write_templates(out: Path, region: str, templates: dict[str, dict], st: dict) -> None:
    d = out / region
    d.mkdir(parents=True, exist_ok=True)
    for stem, tpl in templates.items():
        (d / f"{stem}.json").write_text(json.dumps(tpl, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    index = {"format": FORMAT, "region": region, "stats": st, "files": sorted(templates)}
    (d / "_index.json").write_text(json.dumps(index, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")


# --------------------------------------------------------------------------
# checking

class Report:
    def __init__(self):
        self.errors: list[str] = []
        self.warnings: list[str] = []
        self.counts: Counter = Counter()

    def error(self, where: str, msg: str):
        self.errors.append(f"{where}: {msg}")

    def warn(self, where: str, msg: str):
        self.warnings.append(f"{where}: {msg}")


def load_glossary(path: Path | None) -> list[tuple[str, str]]:
    if not path:
        return []
    data = json.loads(path.read_text(encoding="utf-8"))
    return [(k, v) for k, v in data.items() if not k.startswith("_")]


def check_file(path: Path, rep: Report, *, metrics: dict[int, T.Metrics] | None, max_width: int | None,
               glossary: list[tuple[str, str]], against: dict | None) -> None:
    where = path.name
    try:
        doc = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as ex:
        rep.error(where, f"cannot read: {ex}")
        return
    if doc.get("format") != FORMAT or not isinstance(doc.get("entries"), list):
        rep.error(where, f"not a {FORMAT} template file")
        return
    region = doc.get("region")
    if region not in ("eu", "us"):
        rep.error(where, f"unknown region {region!r}")
        return
    codec = T.codec_for(region)
    style = metrics[DIALOGUE_STYLE] if metrics else None
    dialogue = doc.get("category") in ("dialogue_talk", "dialogue_event")
    seen: set[str] = set()
    for e in doc["entries"]:
        key = e.get("key", "?")
        w = f"{where} {key}"
        if key in seen:
            rep.error(w, "duplicate key")
        seen.add(key)
        rep.counts["entries"] += 1
        en, pt = e.get("en", ""), e.get("pt", "")
        if not isinstance(pt, str):
            rep.error(w, "pt is not a string")
            continue
        if not pt:
            rep.counts["untranslated"] += 1
            continue
        rep.counts["translated"] += 1
        try:
            raw = T.encode(pt, codec)
        except T.EncodeError as ex:
            rep.error(w, f"{ex} (at {ex.pos}): {pt[max(0, ex.pos - 8):ex.pos + 8]!r}")
            continue
        bad = buffer_problem(key, pt, codec)
        if bad:
            rep.error(w, f"fixed buffer: {bad}")
        # placeholders and control codes must match the English (new lines may move)
        if Counter(T.tokens(en)) != Counter(T.tokens(pt)):
            rep.error(w, f"control codes differ: en {T.tokens(en)} vs pt {T.tokens(pt)}")
        elif [t for t in T.tokens(en) if t.startswith("{buf")] != [t for t in T.tokens(pt) if t.startswith("{buf")] \
                and len([t for t in T.tokens(en) if t.startswith("{buf")]) > 1:
            rep.warn(w, "work-buffer placeholders in a different order (check the sentence still works)")
        max_lines = e.get("max_lines") or (MAX_LINES_DIALOGUE if dialogue else None)
        if max_lines:
            pages = T.page_lines(raw)
            if max(pages) > max_lines:
                rep.error(w, f"{max(pages)} lines on a page, limit {max_lines}")
        if style is not None:
            wid = max(T.line_widths(raw, style))
            lim = e.get("limit_px")
            if lim and wid > lim:
                rep.warn(w, f"line is {wid} px, wider than every other EU language ({lim} px)")
            if max_width and wid > max_width:
                rep.error(w, f"line is {wid} px, limit {max_width} px")
        for term, want in glossary:
            if re.search(r"\b%s\b" % re.escape(term), en, re.I) and not re.search(re.escape(want), pt, re.I):
                rep.warn(w, f"glossary: '{term}' should be '{want}'")
    if against is not None:
        ref = against.get(doc.get("stem"))
        if ref is None:
            rep.error(where, "no matching file in --against")
            return
        have = {e["key"]: e for e in doc["entries"]}
        for e in ref["entries"]:
            mine = have.pop(e["key"], None)
            if mine is None:
                rep.error(where, f"missing key {e['key']}")
            elif mine.get("en") != e["en"]:
                rep.error(where, f"{e['key']}: English changed since the template was made")
        for k in have:
            rep.error(where, f"unknown key {k}")


def check_paths(paths: list[Path], *, strict: bool, decomp: Path, max_width: int | None,
                glossary: Path | None, against: Path | None, with_widths: bool = True) -> Report:
    rep = Report()
    files: list[Path] = []
    for p in paths:
        if p.is_dir():
            files += sorted(f for f in p.glob("*.json") if not f.name.startswith("_"))
        else:
            files.append(p)
    if not files:
        rep.error(str(paths[0]) if paths else "?", "no template files found")
        return rep
    metrics = None
    if with_widths:
        try:
            metrics = T.load_metrics(decomp, "eu")
        except (OSError, T.TextFormatError) as ex:
            rep.warn("metrics", f"widths not checked: {ex}")
    ref_docs = None
    if against is not None:
        ref_docs = {}
        for f in sorted(against.glob("*.json")):
            if not f.name.startswith("_"):
                d = json.loads(f.read_text(encoding="utf-8"))
                ref_docs[d.get("stem")] = d
    gl = load_glossary(glossary)
    for f in files:
        check_file(f, rep, metrics=metrics, max_width=max_width, glossary=gl, against=ref_docs)
    if strict:
        rep.errors += [f"(strict) {w}" for w in rep.warnings]
        rep.warnings = []
        if rep.counts["untranslated"]:
            rep.errors.append(f"(strict) {rep.counts['untranslated']} entries are not translated yet")
    return rep


def propagate(paths: list[Path]) -> int:
    """Copy a translation to every empty entry with the same English text."""
    docs = []
    for p in paths:
        for f in sorted(p.glob("*.json")) if p.is_dir() else [p]:
            if not f.name.startswith("_"):
                docs.append((f, json.loads(f.read_text(encoding="utf-8"))))
    known: dict[str, str] = {}
    for _, d in docs:
        for e in d["entries"]:
            if e.get("pt") and e["en"].strip():
                known.setdefault(e["en"], e["pt"])
    n = 0
    for f, d in docs:
        changed = False
        for e in d["entries"]:
            if not e.get("pt") and e["en"] in known:
                e["pt"] = known[e["en"]]
                n += 1
                changed = True
        if changed:
            f.write_text(json.dumps(d, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    return n


# --------------------------------------------------------------------------

def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--runtime", type=Path, default=DEFAULT_RUNTIME, help="folder holding eu/ and us/ (default ~/.cache/dw3-switch/runtime)")
    ap.add_argument("--region", choices=("eu", "us", "both"), default="both")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT, help="template folder (default tmp/l10n/ptbr; refused inside git unless ignored)")
    ap.add_argument("--decomp", type=Path, default=ROOT / "vendor" / "dw3_decomp", help="decomp source, for the glyph widths")
    ap.add_argument("--refs", default="", help="also write these EU languages next to the English (fr,de,it,es)")
    ap.add_argument("--stats", action="store_true", help="print the counts and write nothing")
    ap.add_argument("--check", nargs="+", type=Path, metavar="PATH", help="validate translated template file(s) or folder(s)")
    ap.add_argument("--strict", action="store_true", help="with --check: warnings and untranslated entries are errors")
    ap.add_argument("--against", type=Path, help="with --check: a fresh template folder; keys and English must match")
    ap.add_argument("--glossary", type=Path, help="with --check: JSON {english term: PT-BR term}")
    ap.add_argument("--max-width", type=int, help="with --check: absolute line limit in px (style 1)")
    ap.add_argument("--propagate", nargs="+", type=Path, metavar="PATH", help="fill empty entries from the same English elsewhere")
    a = ap.parse_args(argv)

    if a.propagate:
        print("propagated %d entries" % propagate(a.propagate))
        return 0
    if a.check:
        rep = check_paths(a.check, strict=a.strict, decomp=a.decomp, max_width=a.max_width,
                          glossary=a.glossary, against=a.against)
        for m in rep.warnings:
            print("warning:", m)
        for m in rep.errors:
            print("error:", m)
        c = rep.counts
        print("%d entries, %d translated, %d untranslated; %d errors, %d warnings"
              % (c["entries"], c["translated"], c["untranslated"], len(rep.errors), len(rep.warnings)))
        return 1 if rep.errors else 0

    regions = ("eu", "us") if a.region == "both" else (a.region,)
    refs = tuple(x for x in a.refs.split(",") if x)
    if not a.stats and not is_git_ignored(a.out):
        print(f"refusing to write game text to {a.out}: not git-ignored (use tmp/ or a path outside the repo)", file=sys.stderr)
        return 2
    done = 0
    for region in regions:
        if not (a.runtime / region / f"files_{region}.json").exists():
            print(f"{region}: no runtime under {a.runtime / region}, skipped", file=sys.stderr)
            continue
        try:
            metrics = T.load_metrics(a.decomp, region)
        except (OSError, T.TextFormatError) as ex:
            print(f"{region}: widths unavailable ({ex}); limit_px not written", file=sys.stderr)
            metrics = None
        tpls = build_template(region, a.runtime, metrics, refs)
        st = stats(tpls)
        print(format_stats(st, region))
        if not a.stats:
            write_templates(a.out, region, tpls, st)
            print(f"wrote {len(tpls)} files to {a.out / region}")
        done += 1
    if not done:
        print("nothing extracted: no runtime found", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
