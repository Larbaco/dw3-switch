#!/usr/bin/env python3
"""Does a dialogue line fit the box that shows it? Measure it, and rewrap it when it does not.

The text engine never wraps, so a PT line is only inside the box if the translator (or this tool)
broke it to the right width. What counts as "the right width" is the width of the box that
really shows the string (`screens.py`), measured the way the engine measures it:

* the speaker's name (the text between the two `{start}` codes) is **not** part of the line:
  `createTalkBox` measures and draws the text after it, in another window;
* a **talk box** (`createTalkBox`) is as wide as its widest line, clamped to 139 px; the frame
  ends about 150 px from where the text starts, so a line above `TALK_FIT_PX` leaves the box
  (measured on the PC build: docs/LOCALIZATION.md section 7b);
* the **message box** (`createMessageBox`) runs across the screen; the widest dialogue line of
  any EU language is 252 px.

A string that is as wide in the English original is left alone ("never worse than the English").

    python3 tools/l10n/fit.py --templates tmp/l10n/ptbr/eu [--report tmp/l10n/ptbr/fit-report.txt]
    python3 tools/l10n/fit.py --pack ~/.cache/dw3-switch/runtime/eu/l10n/ptbr [--report FILE]

`--templates` measures the translated template folder (what `build_pack.py` would build, after
its automatic rewrap); `--pack` measures the finished pack, byte by byte. Neither writes game
text anywhere but to the `--report` file, which must be git-ignored (tmp/). The standard output
has counts and keys only.
"""
from __future__ import annotations

import argparse
import itertools
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import screens as S  # noqa: E402
import textfmt as T  # noqa: E402

TALK, MESSAGE = S.TALK_BOX, S.MESSAGE_BOX
TALK_TARGET_PX = S.TALK_BOX_MAX_PX  # the box width the engine clamps to: wrap to this when possible
TALK_FIT_PX = 148  # the frame's inner edge, from the text start (PC screenshot, 2026-11-03)
MESSAGE_FIT_PX = 252  # the widest dialogue line of any EU language; the arrow sits at x=292, text at x=18
INFER_MESSAGE_ABOVE_PX = 155  # an event string no code reaches: English wider than this must be a message box
MAX_LINES = 3  # setLines(3) in both boxes
DIALOGUE_STYLE = 1

LIMITS = {TALK: (TALK_TARGET_PX, TALK_FIT_PX), MESSAGE: (MESSAGE_FIT_PX, MESSAGE_FIT_PX)}  # (wrap target, hard limit)


# --------------------------------------------------------------------------
# which box, and the width the engine measures

def speaker_split(raw: bytes) -> tuple[bytes, bytes]:
    """(speaker header, text): the header is `02 07 name 02 07` (code 7 twice), empty when there is none."""
    if raw[:2] == b"\x02\x07":
        p = 2
        while p + 1 < len(raw):
            c = raw[p]
            if c == 1:
                p += 2
            elif c == 2:
                if raw[p + 1] == 7:
                    return raw[:p + 2], raw[p + 2:]
                p += T.CODE_LENGTHS[raw[p + 1]]
            else:
                p += 1
    return b"", raw


def body_widths(raw: bytes, style: T.Metrics) -> list[int]:
    """Width of every line the box shows (the speaker's name excluded)."""
    return T.line_widths(speaker_split(raw)[1], style)


def body_lines_per_page(raw: bytes) -> list[int]:
    return T.page_lines(speaker_split(raw)[1])


class Boxes:
    """key -> the box that shows the string (screens.classify, plus the inference for the few strings
    no stage script reaches in the decomp source)."""

    def __init__(self, decomp: Path):
        self.map = S.classify(decomp)

    def box(self, key: str, en_raw: bytes | None = None, style: T.Metrics | None = None):
        """(box, how): how is "code" (a script or an actor's table), "file" (TALK: always a talk box) or
        "inferred" (an event string no script in the source reaches: judged by its English width)."""
        kinds = self.map.get(key)
        if kinds:
            return (TALK if TALK in kinds else MESSAGE), "code"
        stem = key.split(":")[0]
        if stem.startswith("TALK"):
            return TALK, "file"
        if stem.startswith("DMG"):
            if en_raw is None or style is None:
                return None, ""
            wide = max(body_widths(en_raw, style) or [0])
            return (MESSAGE if wide > INFER_MESSAGE_ABOVE_PX else TALK), "inferred"
        return None, ""


def has_insert(raw: bytes) -> bool:
    """A work buffer (an item or Digimon name) or the player's name: the word's width is not counted."""
    p = 0
    while p < len(raw):
        c = raw[p]
        if c == 1:
            p += 2
        elif c == 2:
            if raw[p + 1] in (5, 8):
                return True
            p += T.CODE_LENGTHS[raw[p + 1]]
        else:
            p += 1
    return False


def overflow(raw: bytes, box: str, style: T.Metrics, en_raw: bytes | None = None) -> list[int]:
    """Widths of the lines wider than the box holds. A line is not counted when it is no wider than
    the English line of the same string (the original has the same flaw)."""
    limit = LIMITS[box][1]
    if en_raw is not None:
        limit = max(limit, max(body_widths(en_raw, style) or [0]))
    return [w for w in body_widths(raw, style) if w > limit]


NAME_BAR_MARGIN_PX = 10  # the speaker's bar is as wide as the box plus this (PC screenshots; no English name is wider)


def speaker_overflow(raw: bytes, styles: dict[int, T.Metrics]) -> int:
    """Pixels by which the speaker's name (style 2) is wider than the talk box's name bar, 0 if it fits.
    The bar is as wide as the box, the widest line clamped to 95..139 px."""
    head, text = speaker_split(raw)
    if not head:
        return 0
    name = max(T.line_widths(head[2:-2], styles[2]) or [0])
    box = min(TALK_TARGET_PX, max(S.TALK_BOX_MIN_PX, max(T.line_widths(text, styles[1]) or [0])))
    return max(0, name - box - NAME_BAR_MARGIN_PX)


# --------------------------------------------------------------------------
# rewrap

_TAIL = re.compile(r"((?:\{(?:wait|pause):\d+\})*)$")
_NO_MID = re.compile(r"\{(?:wait|pause|raw|start)\b")


def _w(piece: str, codec: T.Codec, style: T.Metrics) -> int:
    return max(T.line_widths(T.encode(piece, codec), style) or [0])


def _wrap_words(words: list[str], widths: list[int], space: int, limit: int, max_lines: int):
    """The fewest lines (up to max_lines) every one within limit, filled as evenly as a typeset
    paragraph (sum of squared slack, last line free). Returns a list of lines or None."""
    n = len(words)
    inf = float("inf")

    def line(a, b):  # words a..b-1
        return sum(widths[a:b]) + space * (b - a - 1)

    for k in range(1, max_lines + 1):
        best = [[inf] * (n + 1) for _ in range(k + 1)]
        cut = [[0] * (n + 1) for _ in range(k + 1)]
        best[0][0] = 0
        for j in range(1, k + 1):
            for i in range(1, n + 1):
                for a in range(i):
                    if best[j - 1][a] == inf:
                        continue
                    w = line(a, i)
                    if w > limit:
                        continue
                    cost = best[j - 1][a] + (0 if i == n else (limit - w) ** 2)
                    if cost < best[j][i]:
                        best[j][i] = cost
                        cut[j][i] = a
        if best[k][n] < inf:
            out, i = [], n
            for j in range(k, 0, -1):
                a = cut[j][i]
                out.append(" ".join(words[a:i]))
                i = a
            return out[::-1]
    return None


def rewrap_page(page: str, box: str, codec: T.Codec, style: T.Metrics, limits: tuple[int, ...] | None = None):
    """(new page text, reason): reason is "" when it was rewrapped, else why it could not be. limits: the
    widths to try, in order (default: the box's wrap target, then its hard limit)."""
    tail = _TAIL.search(page).group(1)
    core = page[:len(page) - len(tail)]
    if _NO_MID.search(core):
        return None, "a wait, pause or raw code in the middle of the page"
    pieces = core.split("\n")
    text = pieces[0]
    for nxt in pieces[1:]:
        # a word the translator cut with a hyphen at the end of a line: join it back
        if text.endswith("-") and len(text) > 1 and text[-2].isalpha() and nxt[:1].islower():
            text += nxt
        else:
            text += " " + nxt
    words = [x for x in text.split(" ") if x]
    if not words:
        return None, "empty"
    widths = [_w(x, codec, style) for x in words]
    space = _w("a a", codec, style) - 2 * _w("a", codec, style)
    target, hard = LIMITS[box]
    for limit in dict.fromkeys(limits or (target, hard)):
        lines = _wrap_words(words, widths, space, limit, MAX_LINES)
        if lines is not None:
            return "\n".join(lines) + tail, ""
    if max(widths) > hard:
        return None, "one word is wider than the box"
    return None, f"does not fit in {MAX_LINES} lines of {hard} px"


def autofit(pt: str, box: str, codec: T.Codec, style: T.Metrics, en_raw: bytes | None = None,
            styles: dict[int, T.Metrics] | None = None):
    """(text, status, reason). status: "ok" (nothing wider than the box), "rewrapped", or "unfit" (the
    original text is returned). Only a page that has a too-wide line is touched. The talk box is as wide
    as its widest line, so a tighter wrap can shrink the speaker's name bar: with `styles` (to measure the
    name) the wider wrap is used when the tighter one would make the name overflow."""
    def bad(raw: bytes) -> bool:
        return bool(overflow(raw, box, style, en_raw))

    try:
        raw = T.encode(pt, codec)
    except T.EncodeError:
        return pt, "ok", ""
    if not bad(raw):
        return pt, "ok", ""
    head = ""
    body = pt
    if pt.startswith("{start}"):
        end = pt.find("{start}", 7)
        if end >= 0:
            head, body = pt[:end + 7], pt[end + 7:]
    target, hard = LIMITS[box]
    tries = [(target, hard)] if target == hard else [(target, hard), (hard,)]
    before = speaker_overflow(raw, styles) if styles and box == TALK else 0
    first = None
    for limits in tries:
        out, reason = [], ""
        for page in body.split("{pg}"):
            if not bad(T.encode(page, codec)):
                out.append(page)
                continue
            new, why = rewrap_page(page, box, codec, style, limits)
            if new is None:
                reason = why
                out.append(page)
            else:
                out.append(new)
        text = head + "{pg}".join(out)
        try:
            new_raw = T.encode(text, codec)
        except T.EncodeError:
            return pt, "unfit", "encode"
        if Counter(T.tokens(text)) != Counter(T.tokens(pt)) or len(T.page_lines(new_raw)) != len(T.page_lines(raw)):
            return pt, "unfit", "the rewrap changed the control codes"  # cannot happen; a guard
        if bad(new_raw) or max(body_lines_per_page(new_raw)) > MAX_LINES:
            continue
        if first is None:
            first = text
        if not (styles and box == TALK) or speaker_overflow(new_raw, styles) <= before:
            return text, "rewrapped", ""
    if first is not None:
        return first, "rewrapped", ""
    return pt, "unfit", reason or "still too wide"


# --------------------------------------------------------------------------
# the measuring pass

class Stats:
    """Counts by box, for a report with no game text in it."""

    def __init__(self):
        self.strings = Counter()
        self.bad_strings = Counter()
        self.bad_lines = Counter()
        self.inserts = Counter()
        self.how = Counter()
        self.keys = defaultdict(list)  # box -> [(key, widths)]
        self.speakers = []  # [(key, px over)] talk boxes whose speaker's name is wider than the name bar

    def add(self, box, how, key, widths, insert):
        self.strings[box] += 1
        self.how[(box, how)] += 1
        if insert:
            self.inserts[box] += 1
        if widths:
            self.bad_strings[box] += 1
            self.bad_lines[box] += len(widths)
            self.keys[box].append((key, widths))

    def add_speaker(self, key: str, over: int):
        if over:
            self.speakers.append((key, over))

    def lines(self, title: str) -> list[str]:
        out = [title]
        for box, label in ((TALK, "talk box (limit %d px)" % TALK_FIT_PX), (MESSAGE, "message box (limit %d px)" % MESSAGE_FIT_PX)):
            how = ", ".join(f"{n} {h}" for (b, h), n in sorted(self.how.items()) if b == box)
            out.append(f"  {label}: {self.strings[box]} strings ({how}); {self.bad_strings[box]} wider than the box, "
                       f"{self.bad_lines[box]} lines; {self.inserts[box]} carry a name or item word (not measured)")
        out.append(f"  talk box speaker bar: {len(self.speakers)} names wider than the bar")
        return out


def apply_autofit(templates: dict[str, dict], decomp: Path, region: str):
    """(templates with the rewrapped pt, {key: (before, after, status, reason, box)} of every string that
    needed it). The input is not modified; only the dialogue files are looked at."""
    style = T.load_metrics(decomp, region)[DIALOGUE_STYLE]
    codec = T.codec_for(region)
    boxes = Boxes(decomp)
    styles = T.load_metrics(decomp, region)
    out, changes = {}, {}
    for stem, tpl in templates.items():
        if not stem.startswith(("TALK", "DMG")):
            out[stem] = tpl
            continue
        entries = []
        for e in tpl.get("entries", []):
            pt, en = e.get("pt") or "", e.get("en") or ""
            if pt and en:
                en_raw = T.encode(en, codec)
                box, _ = boxes.box(e["key"], en_raw, style)
                if box is not None:
                    new, status, reason = autofit(pt, box, codec, style, en_raw, styles)
                    if status != "ok":
                        changes[e["key"]] = (pt, new, status, reason, box)
                    if status == "rewrapped":
                        e = {**e, "pt": new}
            entries.append(e)
        out[stem] = {**tpl, "entries": entries}
    return out, changes


def measure_templates(templates: dict[str, dict], decomp: Path, region: str, autofit_on: bool = False):
    """(Stats, changes). With autofit_on, the strings are measured after the rewrap `build_pack.py`
    applies, and `changes` lists what it did."""
    style = T.load_metrics(decomp, region)[DIALOGUE_STYLE]
    codec = T.codec_for(region)
    boxes = Boxes(decomp)
    changes = {}
    if autofit_on:
        templates, changes = apply_autofit(templates, decomp, region)
    stats = Stats()
    styles = T.load_metrics(decomp, region)
    for stem, tpl in sorted(templates.items()):
        if not stem.startswith(("TALK", "DMG")):
            continue
        for e in tpl.get("entries", []):
            pt, en = e.get("pt") or "", e.get("en") or ""
            if not pt or not en:
                continue
            en_raw = T.encode(en, codec)
            box, how = boxes.box(e["key"], en_raw, style)
            if box is None:
                continue
            raw = T.encode(pt, codec)
            stats.add(box, how, e["key"], overflow(raw, box, style, en_raw), has_insert(raw))
            if box == TALK:
                stats.add_speaker(e["key"], speaker_overflow(raw, styles))
    return stats, changes


def measure_pack(pack_dir: Path, runtime: Path | None, decomp: Path, region: str):
    """Stats of the finished pack. The English comes from the runtime (so a string with no code path
    is judged, and the original's own flaws are not counted); without one, the pack's string stands in."""
    import json

    import build_pack as B

    styles = T.load_metrics(decomp, region)
    style = styles[DIALOGUE_STYLE]
    codec = T.codec_for(region)
    boxes = Boxes(decomp)
    pack = json.loads((pack_dir / "pack.json").read_text(encoding="utf-8"))
    sources = B.source_files(runtime, region) if runtime else {}
    stats = Stats()
    for f in pack["files"]:
        if not f["stem"].startswith(("TALK", "DMG")):
            continue
        out_c = T.parse_container((pack_dir / f["file"]).read_bytes())
        src_c = T.parse_container(sources[f["stem"]][1]) if f["stem"] in sources else None
        for (t, i, raw), en in itertools.zip_longest(out_c.strings(), src_c.strings() if src_c else ()):
            if raw is None or not raw:
                continue
            en_raw = en[2] if en else None
            key = "%s:%d:%d" % (f["stem"], t, i) if out_c.kind == "archive" else "%s:%d" % (f["stem"], i)
            box, how = boxes.box(key, en_raw or raw, style)
            if box is None:
                continue
            stats.add(box, how, key, overflow(raw, box, style, en_raw), has_insert(raw))
            if box == TALK:
                stats.add_speaker(key, speaker_overflow(raw, styles))
    return stats


def write_report(path: Path, stats: Stats, changes: dict | None, title: str):
    """A local report: keys, widths, and the before and after text of every rewrap (game text: tmp/ only)."""
    import extract_text as X

    if not X.is_git_ignored(path):
        raise SystemExit(f"refusing to write a report with game text to {path}: not git-ignored (use tmp/)")
    out = stats.lines(title) + [""]
    for box in (TALK, MESSAGE):
        out.append(f"== {box} box: strings still wider than the box ({len(stats.keys[box])}) ==")
        for key, widths in sorted(stats.keys[box]):
            out.append(f"{key}\t{max(widths)} px\t{len(widths)} line(s)")
        out.append("")
    out.append(f"== talk box speaker names wider than the name bar ({len(stats.speakers)}) ==")
    for key, over in sorted(stats.speakers):
        out.append(f"{key}\t{over} px over")
    out.append("")
    if changes:
        out.append(f"== rewrapped or unfit by the automatic fit ({len(changes)}) ==")
        for key, (before, after, status, reason, box) in sorted(changes.items()):
            out.append(f"{key}\t{status}\t{box}\t{reason}")
            out.append("  before: " + before.replace("\n", "\\n"))
            if status == "rewrapped":
                out.append("  after:  " + after.replace("\n", "\\n"))
    path.write_text("\n".join(out) + "\n", encoding="utf-8")


def main(argv=None) -> int:
    import build_pack as B
    import extract_text as X

    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--templates", type=Path, help="translated template folder")
    ap.add_argument("--pack", type=Path, help="a finished pack folder")
    ap.add_argument("--no-autofit", action="store_true", help="with --templates: measure the text as written, no rewrap")
    ap.add_argument("--region", choices=("eu", "us"), default="eu")
    ap.add_argument("--runtime", type=Path, default=X.DEFAULT_RUNTIME)
    ap.add_argument("--decomp", type=Path, default=X.ROOT / "vendor" / "dw3_decomp")
    ap.add_argument("--report", type=Path, help="local report (keys, widths, before/after); must be git-ignored")
    a = ap.parse_args(argv)
    if bool(a.templates) == bool(a.pack):
        ap.error("give exactly one of --templates and --pack")
    changes = None
    if a.templates:
        stats, changes = measure_templates(B.load_templates(a.templates), a.decomp, a.region, not a.no_autofit)
        title = "templates %s (%s)" % (a.templates, "as written" if a.no_autofit else "after the automatic rewrap")
        if changes is not None and not a.no_autofit:
            c = Counter(v[2] for v in changes.values())
            title += f": {c['rewrapped']} rewrapped, {c['unfit']} cannot be fitted"
    else:
        rt = a.runtime if (a.runtime / a.region / f"files_{a.region}.json").exists() else None
        stats = measure_pack(a.pack, rt, a.decomp, a.region)
        title = f"pack {a.pack}"
    print("\n".join(stats.lines(title)))
    if a.report:
        write_report(a.report, stats, changes, title)
    return 1 if sum(stats.bad_strings.values()) or stats.speakers else 0


if __name__ == "__main__":
    raise SystemExit(main())
