#!/usr/bin/env python3
"""P6.30 help assets: the baked glyph atlas and the embedded text files.

  python3 tools/generate_help_assets.py          write both .inc files
  python3 tools/generate_help_assets.py --check  validate the texts and fail if
                                                 the text .inc is out of date
  python3 tools/generate_help_assets.py --with-pt   also embed port/help/text/pt.txt (P6.7,
                                                 unused until the C side reads it)
  python3 tools/generate_help_assets.py --release  --check, and fail while a
                                                 language file is a stub or
                                                 the English file is a draft

Outputs (checked in; the game needs no font and no Python at run time):
  port/pages/help_atlas.inc   glyph coverage bitmaps and metrics, three faces
  port/help/help_text.inc     port/help/text/<lang>.txt as C string literals

Requires Pillow and DejaVu Sans (fonts-dejavu-core) for the atlas only. The
validator measures with the same advances the atlas bakes.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FONT_ROOT = Path('/usr/share/fonts/truetype/dejavu')
TEXT_DIR = ROOT / 'port' / 'help' / 'text'
ATLAS_INC = ROOT / 'port' / 'pages' / 'help_atlas.inc'
TEXT_INC = ROOT / 'port' / 'help' / 'help_text.inc'

# The five languages of the base game (EN on the US disc; all five on the EU
# disc). Order = the lettering order of title_overlay_art.h (EN FR DE ES IT).
LANGS = ('en', 'fr', 'de', 'es', 'it')
# P6.7 (docs/LOCALIZATION.md section 6): Brazilian Portuguese. Validated like the other
# languages, but not embedded in help_text.inc until the C side reads it (phase 3):
# `--with-pt` embeds it (as an unused array) and checks that form.
DRAFT_LANGS = ('pt',)

# Faces: (file, pixel size). 0 body, 1 bold (headings, questions), 2 page title.
FACES = (
    ('DejaVuSans.ttf', 24),
    ('DejaVuSans-Bold.ttf', 24),
    ('DejaVuSansCondensed-Bold.ttf', 29),
)

# Latin-1 (FR DE IT ES and the later pt-BR) plus the few symbols the text uses.
CHARSET = (list(range(0x20, 0x7F)) + list(range(0xA0, 0x100)) +
           [0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026,
            0x2190, 0x2191, 0x2192, 0x2193, 0x2212, 0x25B2, 0x25BC, 0xFFFD])

# Limits in pixels (docs/P6.30_HELP.md 4.5).
TITLE_LIMIT_FACE2 = 330
TITLE_TAG_LIMIT_FACE2 = 410  # title plus " (draft)" in fr, de, it, es: the plaque is about 440 px inside; 421 touched its edges
BLURB_LIMIT_FACE0 = 664   # the message strip: 720 px frame less 28 px each side, 24 px body face
LINES_WARN = 120

KINDS = ('title', 'blurb', 'h', 'p', 'li', 'row', 'note', 'warn', 'q', 'a', 'str')
QUALS = ('@us', '@eu', '@switch', '@pc', '!unv')
BTN = ('cross', 'circle', 'triangle', 'square', 'start', 'select', 'l1', 'r1',
       'l2', 'r2', 'l3', 'r3', 'dpad')
TOKENS = {
    'btn': set(BTN), 'ps': set(BTN), 'chord': {'restart'},
    'path': {'root', 'memcard'}, 'build': {'version', 'region'},
}
# Words the screen itself needs (help.ui.str.<name>); English must have all.
UI_STRINGS = ('open', 'back', 'scroll', 'pages', 'page', 'unverified', 'draft')


# --------------------------------------------------------------------------
# text files

def parse_text(path: Path):
    """Return (headers, entries, errors); entries = [(key, quals, value, line)]."""
    headers: dict[str, str] = {}
    entries = []
    errors = []
    for n, raw in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith('#'):
            continue
        if '=' not in line:
            errors.append(f'{path.name}:{n}: missing "="')
            continue
        left, _, value = line.partition('=')
        value = value.strip()
        parts = left.split()
        if not parts:
            errors.append(f'{path.name}:{n}: empty key')
            continue
        key, quals = parts[0], parts[1:]
        if key.startswith('@'):
            headers[key[1:]] = value
            continue
        for q in quals:
            if q not in QUALS:
                errors.append(f'{path.name}:{n}: unknown qualifier {q}')
        segs = key.split('.')
        if len(segs) < 3 or segs[2] not in KINDS:
            errors.append(f'{path.name}:{n}: bad key {key}')
        entries.append((key, tuple(quals), value, n))
    return headers, entries, errors


def expand_chars(value: str):
    """The characters a value draws, with escapes and tokens removed. Returns
    (chars, tokens, problems)."""
    chars = []
    tokens = []
    problems = []
    i = 0
    while i < len(value):
        c = value[i]
        if value.startswith('\\n', i):
            i += 2
        elif value.startswith('{{', i) or value.startswith('}}', i):
            chars.append(c)
            i += 2
        elif c == '{':
            j = value.find('}', i)
            if j < 0 or ':' not in value[i + 1:j]:
                problems.append('bad token')
                break
            kind, _, name = value[i + 1:j].partition(':')
            tokens.append((kind, name))
            i = j + 1
        elif c == '}':
            problems.append('stray }')
            i += 1
        else:
            chars.append(c)
            i += 1
    return ''.join(chars), tokens, problems


# --------------------------------------------------------------------------
# atlas (Pillow)

_fonts = {}


def _font(face: int):
    from PIL import ImageFont
    if face not in _fonts:
        name, size = FACES[face]
        _fonts[face] = ImageFont.truetype(str(FONT_ROOT / name), size)
    return _fonts[face]


def advance(face: int, ch: str) -> int:
    return int(round(_font(face).getlength(ch)))


def text_width(face: int, text: str) -> int:
    return sum(advance(face, c) for c in text)


def build_atlas() -> str:
    from PIL import Image, ImageDraw
    cov = bytearray()
    out = []
    metrics = []
    for face in range(len(FACES)):
        font = _font(face)
        asc, desc = font.getmetrics()
        metrics.append((asc + desc, asc))
        rows = []
        for cp in CHARSET:
            ch = chr(cp)
            adv = advance(face, ch)
            bbox = font.getbbox(ch, anchor='ls')
            x0, y0, x1, y1 = bbox
            w, h = x1 - x0, y1 - y0
            if cp == 0x20 or cp == 0xA0 or w <= 0 or h <= 0:
                rows.append((cp, adv, 0, 0, 0, 0, len(cov)))
                continue
            img = Image.new('L', (w, h), 0)
            ImageDraw.Draw(img).text((-x0, -y0), ch, font=font, fill=255, anchor='ls')
            off = len(cov)
            cov.extend(img.tobytes())
            rows.append((cp, adv, w, h, x0, -y0, off))
        out.append(rows)
    lines = ['/* Generated by tools/generate_help_assets.py. DejaVu font license applies',
             ' * (platforms/pc/src/title_overlay_font_license.txt). Three faces: 0 body 24 px,',
             ' * 1 bold 24 px, 2 page title 29 px condensed bold; Latin-1 and a few symbols. */']
    lines.append('static const unsigned char atlas_cov[%d] = {' % len(cov))
    for i in range(0, len(cov), 32):
        lines.append(','.join(str(b) for b in cov[i:i + 32]) + ',')
    lines.append('};')
    for face, rows in enumerate(out):
        lines.append('static const Dw3AtlasGlyph atlas_g%d[%d] = {' % (face, len(rows)))
        for r in rows:
            lines.append('{%d,%d,%d,%d,%d,%d,%d},' % r)
        lines.append('};')
    lines.append('static const int atlas_metrics[%d][2] = {%s};' % (
        len(metrics), ','.join('{%d,%d}' % m for m in metrics)))
    return '\n'.join(lines) + '\n'


# --------------------------------------------------------------------------
# embedded text

def c_string(text: str) -> str:
    out = []
    for line in text.splitlines(keepends=True):
        s = line.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')
        s = s.replace('\r', '')
        out.append('"%s"' % s)
    return '\n'.join(out) if out else '""'


def build_text_inc(with_pt: bool = False) -> str:
    lines = ['/* Generated by tools/generate_help_assets.py from port/help/text/<lang>.txt.',
             ' * Do not edit. UTF-8. */']
    for lang in LANGS + (DRAFT_LANGS if with_pt else ()):
        path = TEXT_DIR / f'{lang}.txt'
        text = path.read_text(encoding='utf-8') if path.exists() else ''
        unused = ' __attribute__((unused))' if lang in DRAFT_LANGS else ''
        lines.append('static const char%s help_text_%s[] =' % (unused, lang))
        lines.append(c_string(text) + ';')
    return '\n'.join(lines) + '\n'


# --------------------------------------------------------------------------
# validation

def validate(release: bool = False):
    """Return (errors, warnings)."""
    errors: list[str] = []
    warnings: list[str] = []
    charset = {chr(c) for c in CHARSET}
    parsed = {}
    for lang in LANGS + DRAFT_LANGS:
        path = TEXT_DIR / f'{lang}.txt'
        if not path.exists():
            if lang in LANGS:
                errors.append(f'{lang}.txt is missing')
            continue
        headers, entries, errs = parse_text(path)
        errors += errs
        if headers.get('format') != '1':
            errors.append(f'{lang}.txt: @format must be 1')
        if headers.get('lang') != lang:
            errors.append(f'{lang}.txt: @lang must be {lang}')
        parsed[lang] = (headers, entries)
    if 'en' not in parsed:
        return errors, warnings
    en_headers, en_entries = parsed['en']
    en_keys = {}
    for key, quals, value, n in en_entries:
        en_keys.setdefault(key, []).append((quals, n))
    # English: tokens, glyphs, limits, UI words
    pages_seen = []
    for key, quals, value, n in en_entries:
        chars, tokens, problems = expand_chars(value)
        for p in problems:
            errors.append(f'en.txt:{n}: {key}: {p}')
        for kind, name in tokens:
            if name not in TOKENS.get(kind, ()):
                errors.append(f'en.txt:{n}: {key}: unknown token {{{kind}:{name}}}')
        for ch in chars:
            if ch not in charset:
                errors.append(f'en.txt:{n}: {key}: glyph U+{ord(ch):04X} {ch!r} not in the atlas')
        seg = key.split('.')
        page, kind = seg[1], seg[2]
        if not pages_seen or pages_seen[-1] != page:
            if page in pages_seen:
                errors.append(f'en.txt:{n}: {key}: page {page} keys are not together')
            pages_seen.append(page)
        if kind == 'title' and text_width(2, chars) > TITLE_LIMIT_FACE2:
            errors.append(f'en.txt:{n}: {key}: title is {text_width(2, chars)} px (limit {TITLE_LIMIT_FACE2})')
        if kind == 'blurb' and text_width(0, chars) > BLURB_LIMIT_FACE0:
            errors.append(f'en.txt:{n}: {key}: blurb is {text_width(0, chars)} px (limit {BLURB_LIMIT_FACE0})')
    for page in pages_seen:
        if page == 'ui':
            continue
        keys = [k for k, *_ in en_entries if k.split('.')[1] == page]
        kinds = {k.split('.')[2] for k in keys}
        if 'title' not in kinds or 'blurb' not in kinds:
            errors.append(f'en.txt: page {page} needs a title and a blurb')
    ui = {k.split('.')[-1] for k, *_ in en_entries if k.split('.')[1:3] == ['ui', 'str']}
    for name in UI_STRINGS:
        if name not in ui:
            errors.append(f'en.txt: missing help.ui.str.{name}')
    if release and 'draft' in en_headers.get('status', ''):
        errors.append(f'en.txt: @status is "{en_headers["status"]}" (a release needs the owner review)')
    # other languages: stubs warn, partial files and unknown keys are errors
    for lang in LANGS[1:] + DRAFT_LANGS:
        if lang not in parsed:
            continue
        headers, entries = parsed[lang]
        status = headers.get('status', '')
        if not entries and lang in DRAFT_LANGS:
            continue
        if not entries:
            warnings.append(f'{lang}.txt is a stub ({status or "no @status"}): '
                            f'the help falls back to English for {lang} (P6.30 launch gap)')
            if release:
                errors.append(f'{lang}.txt is a stub: the base-game languages need the help at launch')
            continue
        have = {}
        draft_word = next((v for k, _q, v, _n in entries if k == 'help.ui.str.draft'), '(draft)')
        for key, quals, value, n in entries:
            have.setdefault(key, []).append(n)
            if key not in en_keys:
                errors.append(f'{lang}.txt:{n}: {key} is not in en.txt')
            chars, tokens, problems = expand_chars(value)
            for p in problems:
                errors.append(f'{lang}.txt:{n}: {key}: {p}')
            for kind, name in tokens:
                if name not in TOKENS.get(kind, ()):
                    errors.append(f'{lang}.txt:{n}: {key}: unknown token {{{kind}:{name}}}')
            for ch in chars:
                if ch not in charset:
                    errors.append(f'{lang}.txt:{n}: {key}: glyph U+{ord(ch):04X} {ch!r} not in the atlas')
            k = key.split('.')[2]
            if k == 'title' and text_width(2, chars) > TITLE_LIMIT_FACE2:
                errors.append(f'{lang}.txt:{n}: {key}: title too wide')
            if k == 'title' and 'draft' in headers.get('status', '') and \
                    text_width(2, chars + ' ' + draft_word) > TITLE_TAG_LIMIT_FACE2:
                errors.append(f'{lang}.txt:{n}: {key}: title with the draft tag is '
                              f'{text_width(2, chars + " " + draft_word)} px (limit {TITLE_TAG_LIMIT_FACE2})')
            if k == 'blurb' and text_width(0, chars) > BLURB_LIMIT_FACE0:
                errors.append(f'{lang}.txt:{n}: {key}: blurb too wide')
        missing = [k for k in en_keys if k not in have]
        if missing:
            msg = f'{lang}.txt is partial: {len(missing)} of {len(en_keys)} keys fall back to English'
            (errors if release else warnings).append(msg)
    return errors, warnings


def main(argv):
    with_pt = '--with-pt' in argv
    release = '--release' in argv
    check = '--check' in argv or release
    if check:
        errors, warnings = validate(release)
        for w in warnings:
            print('WARNING:', w)
        for e in errors:
            print('ERROR:', e)
        stale = []
        if TEXT_INC.read_text(encoding='utf-8') != build_text_inc(with_pt) if TEXT_INC.exists() else True:
            stale.append(str(TEXT_INC.relative_to(ROOT)))
        if stale:
            print('ERROR: out of date, run tools/generate_help_assets.py:', ', '.join(stale))
        return 1 if errors or stale else 0
    TEXT_INC.write_text(build_text_inc(with_pt), encoding='utf-8')
    ATLAS_INC.write_text(build_atlas(), encoding='utf-8')
    errors, warnings = validate()
    for w in warnings:
        print('WARNING:', w)
    for e in errors:
        print('ERROR:', e)
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
