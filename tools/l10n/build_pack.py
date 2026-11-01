#!/usr/bin/env python3
"""P6.7 phase 1: build a local text pack for the port's loader (docs/LOCALIZATION.md section 3).

  python3 tools/l10n/build_pack.py --templates tmp/l10n/ptbr/eu --region eu \\
          --out ~/.cache/dw3-switch/runtime/eu/l10n/ptbr
  python3 tools/l10n/build_pack.py --synthetic upper --region eu --out PACKDIR
  python3 tools/l10n/build_pack.py --synthetic stress --region eu --out PACKDIR
  python3 tools/l10n/build_pack.py --templates DIR --keys ...   also write the key variants
  python3 tools/l10n/build_pack.py --verify PACKDIR [--region eu]

Reads the owner's runtime (the English text files, for the structure, the
fallback strings and the source hashes) and a translated template folder
(extract_text.py), and writes a pack folder:

  pack.json      format, region, language, the sha256 of every SOURCE English file,
                 and per output file: id, stem, bytes, sha256, string counts and the first
                 32 keys kept in English (the loader logs them)
  <fileid>.bin   one finished text file per English file id, same container
  <fileid>.k.bin (--keys) the same with every string started by its key, "(TALK03:412) ": the
                 port reads these with DW3_L10N_SHOW_KEYS=1, to find a string seen in the game

An empty "pt" keeps the original English bytes of that string, so a half-finished
translation is playable. The loader refuses a pack whose source hashes do not match
the runtime it runs on (a pack built against another disc dump) and plays English.

--synthetic builds a pack from the English text alone, with a visible transform, so
the loader, the font and the boxes can be tested before any translation exists:

  upper    every string upper-cased, with "AN" -> "ãN", "ON" -> "õN", "CE" -> "çE"
           (lower-case ã õ ç next to capitals: they show the glyphs without a translation)
  proof    English, except two card messages (MEMCRD:29, "Choose MEMORY CARD slot.", and MEMCRD:12,
           "File cannot be found.", shown by Title > Load Game on an empty card): they are the
           glyph proof, the 26 accented letters PT-BR needs and ¡ ¿ ° (29 characters)
  partial  "upper" on two strings in three, the third keeps the English (a half-finished
           translation: the fallback path)
  stress   every line padded with accented letters up to the widest line the other
           four EU languages have at the same index (limit_px), the longest text a
           translator was allowed; for the measuring pass of phase 2

The pack is derived from the game's text. It is never committed and never shipped:
the tool refuses to write inside the repository unless the path is git-ignored.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import extract_text as X  # noqa: E402
import textfmt as T  # noqa: E402

PACK_FORMAT = "dw3-l10n-pack-v1"
SECTOR = 2048
# The loader's sanity limits (port/l10n/l10n_core.h): a pack is at most this many files.
MAX_FILES = 64
KEYS_MAX = 32  # fallback keys listed per file, for the loader's log (port/l10n/l10n_core.h)
LANG_RE = re.compile(r"^[a-z][a-z0-9]{1,15}$")
_TOKEN = re.compile(r"\{[^}]*\}")
SYNTHETIC = ("upper", "partial", "stress", "proof")
PROOF_PAGES = ("ÁÀÂÃÉÊÍÓÔÕÚÇÜ\náàâãéêíóôõúçü\n¡ ¿ °", "AÃ OÕ EÊ OÔ aã oõ eê oô\n¡Hola! ¿Qué? 20°")
PROOF_KEYS = {"MEMCRD:12": PROOF_PAGES[0], "MEMCRD:29": PROOF_PAGES[1]}  # "File cannot be found." and "Choose MEMORY CARD slot."
PROOF_CHARS = 29  # 13 + 13 accented letters and ¡ ¿ °
FILLER = "ãõçéêáíóúÃÕÊÔmw"  # wide letters, with the accents PT-BR needs


class PackError(ValueError):
    """The template or the runtime cannot make a pack."""


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


# --------------------------------------------------------------------------
# the synthetic transforms (no translation needed)

def _split_tokens(text: str):
    """[(is_token, piece)] of an editable text."""
    out = []
    pos = 0
    for m in _TOKEN.finditer(text):
        if m.start() > pos:
            out.append((False, text[pos:m.start()]))
        out.append((True, m.group(0)))
        pos = m.end()
    if pos < len(text):
        out.append((False, text[pos:]))
    return out


def synth_upper(text: str, codec: T.Codec) -> str:
    """Upper-case, plus ã õ ç (see the module doc). Characters the font cannot write stay as they were."""
    out = []
    for is_tok, piece in _split_tokens(text):
        if is_tok:
            out.append(piece)
            continue
        chars = []
        for ch in piece:
            up = ch.upper()
            chars.append(up if len(up) == 1 and codec.can_encode(up) else ch)
        s = "".join(chars)
        s = s.replace("AN", "ãN").replace("ON", "õN").replace("CE", "çE")
        out.append("".join(c if codec.can_encode(c) or c == "\n" else "?" for c in s))
    return "".join(out)


def _line_width(line: str, codec: T.Codec, style: T.Metrics) -> int:
    try:
        return max(T.line_widths(T.encode(line, codec), style) or [0])
    except T.EncodeError:
        return 0


def synth_stress(text: str, limit_px: int | None, codec: T.Codec, style: T.Metrics | None) -> str:
    """Pad every line to limit_px with accented wide letters (a worst case a translator was allowed)."""
    if not limit_px or style is None:
        return text
    lines = text.split("\n")
    for n, line in enumerate(lines):
        parts = _split_tokens(line)
        plain = [i for i, (tok, p) in enumerate(parts) if not tok and p.strip()]
        if not plain:
            continue
        best = max(plain, key=lambda i: len(parts[i][1]))
        cur = parts[best][1].rstrip()
        for i in range(200):
            nxt = cur + (" " if i % 5 == 0 else "") + FILLER[i % len(FILLER)]
            trial = list(parts)
            trial[best] = (False, nxt)
            if _line_width("".join(p for _, p in trial), codec, style) > limit_px:
                break
            cur = nxt
        parts[best] = (False, cur)
        lines[n] = "".join(p for _, p in parts)
    return "\n".join(lines)


def _transform(kind, tpl, n, e, codec, style):
    if kind == "upper":
        return synth_upper(e["en"], codec)
    if kind == "proof":
        return PROOF_KEYS.get(e["key"], "")
    if kind == "partial":
        return synth_upper(e["en"], codec) if n % 3 else ""
    if kind == "stress":
        return synth_stress(e["en"], e.get("limit_px"), codec, style)
    raise PackError("unknown --synthetic %r (%s)" % (kind, ", ".join(SYNTHETIC)))


def synthetic_templates(kind: str, region: str, runtime: Path, decomp: Path) -> dict[str, dict]:
    """Templates (extract_text.py shape) with pt filled by a synthetic transform."""
    try:
        metrics = T.load_metrics(decomp, region)
    except (OSError, T.TextFormatError):
        metrics = None
    if kind == "stress" and metrics is None:
        raise PackError("stress needs the glyph widths: --decomp must hold the decomp source")
    tpls = X.build_template(region, runtime, metrics)
    codec = T.codec_for(region)
    style = metrics[X.DIALOGUE_STYLE] if metrics else None
    for tpl in tpls.values():
        for n, e in enumerate(tpl["entries"]):
            if not e["en"]:
                continue
            e["pt"] = _transform(kind, tpl, n, e, codec, style)
            if e["pt"] and X.buffer_problem(e["key"], e["pt"], codec):
                e["pt"] = ""  # the engine copies this string into a fixed buffer: keep the English
    return tpls


# --------------------------------------------------------------------------
# the builder

def keyed(text: str, key: str) -> str:
    """text with its key in front, after the speaker's {start} when there is one."""
    prefix = "(%s) " % key
    if "{start}" in text:
        return text.replace("{start}", "{start}" + prefix, 1)
    return prefix + text


def build_file(src: bytes, tpl: dict, codec: T.Codec, with_keys: bool = False):
    """(output bytes, strings, translated, fallback keys, errors, key variant or None) for one text file.

    src is the English file of the runtime. A string with a non-empty pt is encoded; any other
    keeps its original bytes. The container (table or archive of tables) is rebuilt with new offsets."""
    cont = T.parse_container(src)
    if tpl.get("container") not in (None, cont.kind):
        raise PackError(f"{tpl.get('stem')}: template says {tpl.get('container')}, the file is a {cont.kind}")
    by_key = {}
    for e in tpl.get("entries", []):
        if e["key"] in by_key:
            raise PackError(f"{tpl.get('stem')}: duplicate key {e['key']}")
        by_key[e["key"]] = e
    stem = tpl.get("stem")
    translated = 0
    fallback: list[str] = []
    errors: list[str] = []
    new_tables: list[list[bytes]] = []
    key_tables: list[list[bytes]] = []
    seen = set()
    for t, table in enumerate(cont.tables):
        new_table = []
        key_table = []
        for i, raw in enumerate(table):
            key = "%s:%d:%d" % (stem, t, i) if cont.kind == "archive" else "%s:%d" % (stem, i)
            seen.add(key)
            e = by_key.get(key)
            pt = e.get("pt") if e else ""
            if not e:
                errors.append(f"{key}: not in the template")
            if pt:
                if e.get("en") is not None and e["en"] != T.decode(raw, codec):
                    errors.append(f"{key}: the English changed since the template was made (another disc dump?)")
                    new_table.append(raw)
                    key_table.append(raw)
                    continue
                try:
                    new_table.append(T.encode(pt, codec))
                    translated += 1
                    key_table.append(T.encode(keyed(pt, key), codec) if with_keys else b"")
                except T.EncodeError as ex:
                    errors.append(f"{key}: {ex}")
                    new_table.append(raw)
                    key_table.append(raw)
            else:
                new_table.append(raw)
                key_table.append(T.encode(keyed(T.decode(raw, codec), key), codec) if with_keys and raw else raw)
                if raw:  # an empty string has nothing to translate
                    fallback.append(key)
        new_tables.append(new_table)
        key_tables.append(key_table)
    for key in by_key:
        if key not in seen:
            errors.append(f"{key}: the file has no such string")
    out = T.build_container(T.Container(cont.kind, new_tables))
    kout = T.build_container(T.Container(cont.kind, key_tables)) if with_keys else None
    return out, sum(len(t) for t in new_tables), translated, fallback, errors, kout


def source_files(runtime: Path, region: str) -> dict[str, tuple[int, bytes]]:
    """{STEM: (file id, English bytes)} of the runtime (the ENG row, or USA on the US disc)."""
    lang_dir = X.EU_LANGS["en"][0] if region == "eu" else X.US_LANG[0]
    try:
        ids = X.file_ids(runtime, region, lang_dir)
        data = X.read_language(runtime, region, lang_dir)
    except (OSError, ValueError, KeyError):
        return {}
    return {stem: (ids[stem], data[stem]) for stem in data if stem in ids}


def build_pack(templates: dict[str, dict], runtime: Path, region: str, lang: str, out: Path,
               synthetic: str | None = None, with_keys: bool = False) -> dict:
    """Write the pack folder. Returns the pack.json dict. Raises PackError on a bad input."""
    if not LANG_RE.match(lang):
        raise PackError(f"bad language code {lang!r}")
    codec = T.codec_for(region)
    sources = source_files(runtime, region)
    if not sources:
        raise PackError(f"no English text files under {runtime / region}")
    files = []
    errors: list[str] = []
    fb_total = 0
    blobs: dict[int, bytes] = {}
    kblobs: dict[int, bytes] = {}
    for stem in sorted(sources):
        fid, src = sources[stem]
        tpl = templates.get(stem)
        if tpl is None:
            raise PackError(f"the template folder has no {stem}.json")
        data, n, tr, fb, errs, kdata = build_file(src, tpl, codec, with_keys)
        errors += [f"{stem} {e}" for e in errs]
        fb_total += len(fb)
        blobs[fid] = data
        if kdata is not None:
            kblobs[fid] = kdata
        files.append({
            "id": fid, "stem": stem, "file": f"{fid}.bin", "bytes": len(data), "sectors": -(-len(data) // SECTOR),
            "sha256": sha256(data), "src_bytes": len(src), "src_sha256": sha256(src),
            "strings": n, "translated": tr, "fallback": len(fb), "fallback_keys": fb[:KEYS_MAX],
        })
        if kdata is not None:
            files[-1].update(keys_bytes=len(kdata), keys_sectors=-(-len(kdata) // SECTOR))
        if len(files) > MAX_FILES:
            raise PackError("too many files")
    if errors:
        raise PackError("%d errors, first: %s" % (len(errors), "; ".join(errors[:5])))
    files.sort(key=lambda f: f["id"])
    pack = {
        "format": PACK_FORMAT, "region": region, "language": lang, "synthetic": synthetic,
        **({"proof_chars": PROOF_CHARS} if synthetic == "proof" else {}),
        "files": files,
        "totals": {"files": len(files), "strings": sum(f["strings"] for f in files),
                   "translated": sum(f["translated"] for f in files), "fallback": fb_total},
    }
    out.mkdir(parents=True, exist_ok=True)
    for f in out.glob("*.bin"):
        f.unlink()
    for fid, data in blobs.items():
        (out / f"{fid}.bin").write_bytes(data)
    for fid, data in kblobs.items():
        (out / f"{fid}.k.bin").write_bytes(data)
    # one file entry per line: the loader's reader is a small scanner (port/l10n/l10n_core.c)
    head = {k: v for k, v in pack.items() if k != "files"}
    text = "{\n" + ",\n".join(f" {json.dumps(k)}: {json.dumps(v)}" for k, v in head.items() if k != "totals")
    text += ",\n \"files\": [\n" + ",\n".join("  " + json.dumps(f, separators=(", ", ": ")) for f in files)
    text += "\n ],\n \"totals\": " + json.dumps(pack["totals"]) + "\n}\n"
    (out / "pack.json").write_text(text, encoding="utf-8")
    return pack


# --------------------------------------------------------------------------
# verify: decode every output file and compare it with its source, string by string

def verify_pack(pack_dir: Path, runtime: Path | None, region: str | None) -> list[str]:
    """Problems of a pack folder (empty list = ok). With a runtime, also checks the source hashes
    and that every string of the pack decodes and keeps the control codes of the English."""
    problems: list[str] = []
    try:
        pack = json.loads((pack_dir / "pack.json").read_text(encoding="utf-8"))
    except (OSError, ValueError) as ex:
        return [f"pack.json: {ex}"]
    if pack.get("format") != PACK_FORMAT:
        return [f"pack.json: format is not {PACK_FORMAT}"]
    reg = region or pack.get("region")
    if pack.get("region") != reg:
        problems.append(f"pack.json: region {pack.get('region')} != {reg}")
    sources = source_files(runtime, reg) if runtime else {}
    codec = T.codec_for(reg)
    for f in pack.get("files", []):
        path = pack_dir / f["file"]
        try:
            data = path.read_bytes()
        except OSError as ex:
            problems.append(f"{f['file']}: {ex}")
            continue
        if len(data) != f["bytes"] or sha256(data) != f["sha256"]:
            problems.append(f"{f['file']}: size or hash differs from pack.json")
            continue
        try:
            out_c = T.parse_container(data)
        except T.TextFormatError as ex:
            problems.append(f"{f['file']}: {ex}")
            continue
        if f.get("keys_bytes"):
            kpath = pack_dir / f"{f['id']}.k.bin"
            try:
                kdata = kpath.read_bytes()
                if len(kdata) != f["keys_bytes"]:
                    problems.append(f"{kpath.name}: size differs from pack.json")
                else:
                    kc = T.parse_container(kdata)
                    if [len(t) for t in kc.tables] != [len(t) for t in out_c.tables]:
                        problems.append(f"{kpath.name}: string counts differ from {f['file']}")
            except (OSError, T.TextFormatError) as ex:
                problems.append(f"{kpath.name}: {ex}")
        src_entry = sources.get(f["stem"]) if sources else None
        if sources and src_entry is None:
            problems.append(f"{f['stem']}: not in the runtime")
            continue
        if src_entry:
            fid, src = src_entry
            if fid != f["id"] or sha256(src) != f["src_sha256"]:
                problems.append(f"{f['stem']}: the source file is not the one the pack was built from")
                continue
            src_c = T.parse_container(src)
            if [len(t) for t in src_c.tables] != [len(t) for t in out_c.tables]:
                problems.append(f"{f['stem']}: string counts differ from the English")
                continue
            for (t, i, a), (_, _, b) in zip(src_c.strings(), out_c.strings()):
                ta, tb = T.decode(a, codec), T.decode(b, codec)
                if T.tokens(ta) != T.tokens(tb) and a != b:
                    problems.append(f"{f['stem']}:{t}:{i}: control codes differ from the English")
                    break
    return problems


# --------------------------------------------------------------------------

def load_templates(folder: Path) -> dict[str, dict]:
    out = {}
    for f in sorted(folder.glob("*.json")):
        if f.name.startswith("_"):
            continue
        d = json.loads(f.read_text(encoding="utf-8"))
        if d.get("format") != X.FORMAT:
            raise PackError(f"{f.name}: not a {X.FORMAT} template")
        out[d["stem"]] = d
    if not out:
        raise PackError(f"no template files in {folder}")
    return out


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--runtime", type=Path, default=X.DEFAULT_RUNTIME, help="folder holding eu/ and us/")
    ap.add_argument("--region", choices=("eu", "us"), default="eu")
    ap.add_argument("--lang", default="ptbr", help="language code of the pack folder (default ptbr)")
    ap.add_argument("--templates", type=Path, help="translated template folder (extract_text.py output)")
    ap.add_argument("--synthetic", choices=SYNTHETIC, help="build from the English with a visible transform")
    ap.add_argument("--keys", action="store_true", help="also write <id>.k.bin, every string started by its key (DW3_L10N_SHOW_KEYS=1)")
    ap.add_argument("--out", type=Path, help="pack folder (default <runtime>/<region>/l10n/<lang>)")
    ap.add_argument("--decomp", type=Path, default=X.ROOT / "vendor" / "dw3_decomp", help="decomp source, for the glyph widths")
    ap.add_argument("--verify", type=Path, metavar="PACKDIR", help="check a pack folder (and its source hashes with the runtime)")
    a = ap.parse_args(argv)

    if a.verify:
        problems = verify_pack(a.verify, a.runtime if (a.runtime / a.region / f"files_{a.region}.json").exists() else None,
                               a.region if a.region else None)
        for p in problems:
            print("error:", p)
        print("pack %s: %s" % (a.verify, "ok" if not problems else "%d problems" % len(problems)))
        return 1 if problems else 0

    if bool(a.templates) == bool(a.synthetic):
        ap.error("give exactly one of --templates and --synthetic")
    out = a.out or (a.runtime / a.region / "l10n" / a.lang)
    if not X.is_git_ignored(out):
        print(f"refusing to write a text pack to {out}: not git-ignored (use tmp/ or a path outside the repo)", file=sys.stderr)
        return 2
    try:
        if a.synthetic:
            tpls = synthetic_templates(a.synthetic, a.region, a.runtime, a.decomp)
        else:
            tpls = load_templates(a.templates)
        pack = build_pack(tpls, a.runtime, a.region, a.lang, out, a.synthetic, a.keys)
    except (PackError, OSError, T.TextFormatError, KeyError) as ex:
        print(f"error: {ex}", file=sys.stderr)
        return 1
    t = pack["totals"]
    print("pack %s: %d files, %d strings, %d translated, %d kept in English -> %s"
          % (a.lang, t["files"], t["strings"], t["translated"], t["fallback"], out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
