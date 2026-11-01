"""Text containers and the font codec of Digimon World 3 (P6.7, docs/LOCALIZATION.md).

Pure functions on bytes. No game data is read here and none is stored: the
caller hands in the bytes of a text file (extract_text.py reads them from the
owner's runtime) or synthetic bytes (the unit tests).

Containers (what FILE_CACHE hands the text engine):

  table    u32 count, then count u32 offsets from the start of the table, then
           the strings (getString() in text_tools.c).
  archive  u32 offsets of tables (the first one is also the header size), each
           table with its own offsets (the DMG event text files).

Strings are byte strings ended by 0 (decodeChar in font.c, text_window.c):

  0           end
  1 n         icon n of the style's icon sheet (punctuation, buttons and, on
              the EU disc, every accented letter)
  2 c args    control code c (length FONT_CODE_LENGTHS[c] with the 2 and c)
  3           a missing character
  4..233      glyph n - 4 of the style's glyph sheet (digits, letters, ...)

The editable form (the template files) is plain text: letters as themselves,
a new line as "\\n", and the rest as {tokens} so that decode/encode round-trips
every byte. See docs/LOCALIZATION.md section 3.
"""
from __future__ import annotations

import re
import struct
from dataclasses import dataclass, field
from pathlib import Path

# Length of each control code with its 0x02 and code bytes (FONT_CODE_LENGTHS).
CODE_LENGTHS = (1, 2, 3, 2, 5, 3, 3, 2, 3, 2, 0)

C_NEWLINE, C_WAIT, C_PAGE, C_IGNORE, C_BUFFER, C_PAUSE, C_START, C_NAME = 1, 2, 3, 4, 5, 6, 7, 8


class TextFormatError(ValueError):
    """Bad container or bad string bytes."""


class EncodeError(ValueError):
    """The text cannot be written with the game's font."""

    def __init__(self, message: str, pos: int = 0):
        super().__init__(message)
        self.pos = pos


# --------------------------------------------------------------------------
# containers

def _u32(data: bytes, pos: int) -> int:
    return struct.unpack_from("<I", data, pos)[0]


def table_offsets(data: bytes, base: int = 0, end: int | None = None) -> list[int] | None:
    """The offsets of a string table at data[base:end], or None if it is not one."""
    end = len(data) if end is None else end
    if end - base < 8:
        return None
    n = _u32(data, base)
    header = 4 * (n + 1)
    if n == 0 or n > 200000 or base + header > end:
        return None
    offs = list(struct.unpack_from("<%dI" % n, data, base + 4))
    if offs[0] != header or any(b < a for a, b in zip(offs, offs[1:])):
        return None
    if base + offs[-1] >= end:
        return None
    return offs


def archive_offsets(data: bytes) -> list[int] | None:
    """The table offsets of an archive of string tables, or None."""
    if len(data) < 12:
        return None
    first = _u32(data, 0)
    if first % 4 or first < 8 or first >= len(data):
        return None
    k = first // 4
    offs = list(struct.unpack_from("<%dI" % k, data, 0))
    if any(b <= a for a, b in zip(offs, offs[1:])) or offs[-1] >= len(data):
        return None
    ends = offs[1:] + [len(data)]
    for o, e in zip(offs, ends):
        if table_offsets(data, o, e) is None:
            return None
    return offs


def scan_string(data: bytes, pos: int) -> int:
    """The end (exclusive, before the 0) of the string at data[pos:]."""
    n = len(data)
    p = pos
    while True:
        if p >= n:
            raise TextFormatError("string at %d runs past the end of the file" % pos)
        c = data[p]
        if c == 0:
            return p
        if c == 1:
            p += 2
        elif c == 2:
            if p + 1 >= n:
                raise TextFormatError("control code at %d is cut" % p)
            code = data[p + 1]
            if code >= len(CODE_LENGTHS) or CODE_LENGTHS[code] == 0:
                raise TextFormatError("control code %d at %d is unknown" % (code, p))
            p += CODE_LENGTHS[code]
        else:
            p += 1


@dataclass
class Container:
    kind: str  # "table" or "archive"
    tables: list[list[bytes]] = field(default_factory=list)  # strings without the final 0

    def strings(self):
        """(table index, string index, bytes) in file order."""
        for t, table in enumerate(self.tables):
            for i, s in enumerate(table):
                yield t, i, s


def parse_container(data: bytes) -> Container:
    offs = table_offsets(data)
    if offs is not None:
        return Container("table", [[data[o:scan_string(data, o)] for o in offs]])
    aoffs = archive_offsets(data)
    if aoffs is None:
        raise TextFormatError("not a string table or an archive of string tables")
    tables = []
    for o in aoffs:
        toffs = table_offsets(data, o)
        tables.append([data[o + t:scan_string(data, o + t)] for t in toffs])
    return Container("archive", tables)


def build_table(strings: list[bytes]) -> bytes:
    """A string table of `strings` (each without its 0; strings are 4-aligned)."""
    n = len(strings)
    header = 4 * (n + 1)
    body = bytearray()
    offs = []
    for s in strings:
        offs.append(header + len(body))
        body += s + b"\0"
        while len(body) % 4:
            body += b"\0"
    return struct.pack("<I", n) + struct.pack("<%dI" % n, *offs) + bytes(body)


def build_container(c: Container) -> bytes:
    if c.kind == "table":
        return build_table(c.tables[0])
    blobs = [build_table(t) for t in c.tables]
    pos = 4 * len(blobs)
    offs = []
    for b in blobs:
        offs.append(pos)
        pos += len(b)
    return struct.pack("<%dI" % len(offs), *offs) + b"".join(blobs)


# --------------------------------------------------------------------------
# the font: which byte is which character

# Icons that are text punctuation, from FONT_ICON_MAP's Shift-JIS codes and the
# way the English text uses them (docs/LOCALIZATION.md section 2.2).
TEXT_ICONS = {1: " ", 4: ",", 5: ".", 7: ":", 8: ";", 15: "/", 18: "…", 19: "'",
              20: '"', 21: "(", 22: ")", 25: "+", 26: "-", 28: "=", 42: "%", 43: "&"}
# The EU disc's accented letters, icons 59..114 (identified from the glyph
# sheet and checked against the French, Spanish, Italian and German text).
# 60, 61 and 62 are not accents (pound, section sign, micro) and unverified.
EU_ICONS = {
    59: "¡", 60: "£", 61: "§", 62: "µ", 63: "¿",
    64: "Ã", 65: "Ä", 66: "Å", 67: "Æ", 68: "Ç", 69: "Ñ",
    70: "Õ", 71: "Ö", 72: "Ø", 73: "Ü", 74: "ß",
    75: "à", 76: "â", 77: "ã", 78: "ä", 79: "ç", 80: "è",
    81: "é", 82: "ê", 83: "ë", 84: "ì", 85: "î", 86: "ï",
    87: "ñ", 88: "ò", 89: "ô", 90: "õ", 91: "ö", 92: "ù",
    93: "û", 94: "ü", 95: "á", 96: "í", 97: "ó", 98: "ú",
    99: "Â", 100: "Ê", 101: "Ô", 102: "Ë", 103: "Ï",
    104: "Á", 105: "É", 106: "Í", 107: "Ó", 108: "Ú",
    109: "À", 110: "È", 111: "Ì", 112: "Ò", 113: "Ù", 114: "°",
}
# Glyph bytes (4..233): digits, A-Z, a-z, then '?' and '!' (the sheet's last
# punctuation); 66..228 and the rest are Japanese kana and stay {g:n}.
GLYPH_CHARS = {}
for _i in range(10):
    GLYPH_CHARS[4 + _i] = chr(48 + _i)
for _i in range(26):
    GLYPH_CHARS[14 + _i] = chr(65 + _i)
    GLYPH_CHARS[40 + _i] = chr(97 + _i)
GLYPH_CHARS[230] = "?"
GLYPH_CHARS[231] = "!"

# What PT-BR needs and the EU font has (checked by tests and by --check).
PTBR_ACCENTS = "áàâãéêíóôõúçü" \
               "ÁÀÂÃÉÊÍÓÔÕÚÇÜ"


@dataclass
class Codec:
    """Byte <-> character tables of one region's font."""
    region: str  # "eu" or "us"
    icons: dict[int, str]
    glyphs: dict[int, str] = field(default_factory=lambda: dict(GLYPH_CHARS))

    def __post_init__(self):
        self.char_to_bytes: dict[str, bytes] = {}
        for g, ch in self.glyphs.items():
            self.char_to_bytes[ch] = bytes((g,))
        for n, ch in self.icons.items():
            self.char_to_bytes.setdefault(ch, bytes((1, n)))

    def can_encode(self, ch: str) -> bool:
        return ch in self.char_to_bytes


def codec_for(region: str) -> Codec:
    icons = dict(TEXT_ICONS)
    if region == "eu":
        icons.update(EU_ICONS)
    elif region != "us":
        raise ValueError("region must be eu or us")
    return Codec(region, icons)


_TOKEN = re.compile(r"\{([a-z]+)(?::([0-9A-Fa-f ]*))?\}")


def decode(raw: bytes, codec: Codec) -> str:
    """The editable text of one string (bytes without the final 0)."""
    out = []
    p = 0
    n = len(raw)
    while p < n:
        c = raw[p]
        if c == 1:
            if p + 1 >= n:
                raise TextFormatError("icon cut at the end of a string")
            ch = codec.icons.get(raw[p + 1])
            out.append(ch if ch else "{i:%d}" % raw[p + 1])
            p += 2
        elif c == 2:
            code = raw[p + 1]
            ln = CODE_LENGTHS[code]
            args = raw[p + 2:p + ln]
            if len(args) != ln - 2:
                raise TextFormatError("control code cut at the end of a string")
            if code == C_NEWLINE:
                out.append("\n")
            elif code == C_PAGE:
                out.append("{pg}")
            elif code == C_START:
                out.append("{start}")
            elif code in (C_WAIT, C_BUFFER, C_PAUSE, C_NAME):
                name = {C_WAIT: "wait", C_BUFFER: "buf", C_PAUSE: "pause", C_NAME: "name"}[code]
                out.append("{%s:%d}" % (name, args[0]))
            else:
                out.append("{raw:%s}" % raw[p:p + ln].hex())
            p += ln
        elif c == 3:
            out.append("{x}")
            p += 1
        else:
            ch = codec.glyphs.get(c)
            out.append(ch if ch else "{g:%d}" % c)
            p += 1
    return "".join(out)


def encode(text: str, codec: Codec) -> bytes:
    """The game's bytes of an editable text (no final 0). Raises EncodeError."""
    out = bytearray()
    p = 0
    n = len(text)
    while p < n:
        ch = text[p]
        if ch == "\n":
            out += b"\x02\x01"
            p += 1
        elif ch == "{":
            m = _TOKEN.match(text, p)
            if not m:
                raise EncodeError("unclosed or unknown {token}", p)
            name, arg = m.group(1), m.group(2)
            out += _encode_token(name, arg, p)
            p = m.end()
        elif ch in codec.char_to_bytes:
            out += codec.char_to_bytes[ch]
            p += 1
        else:
            raise EncodeError("%r (U+%04X) is not in the %s font" % (ch, ord(ch), codec.region.upper()), p)
    return bytes(out)


def _encode_token(name: str, arg: str | None, pos: int) -> bytes:
    simple = {"pg": b"\x02\x03", "start": b"\x02\x07", "x": b"\x03"}
    if name in simple and arg is None:
        return simple[name]
    if name in ("wait", "buf", "pause", "name", "i", "g") and arg is not None and arg.strip().isdigit():
        v = int(arg)
        if not 0 <= v <= 255:
            raise EncodeError("{%s:%s} is out of range" % (name, arg), pos)
        code = {"wait": C_WAIT, "buf": C_BUFFER, "pause": C_PAUSE, "name": C_NAME}.get(name)
        if code is not None:
            return bytes((2, code, v))
        return bytes((1, v)) if name == "i" else bytes((v,))
    if name == "raw" and arg is not None:
        try:
            raw = bytes.fromhex(arg.replace(" ", ""))
        except ValueError:
            raise EncodeError("{raw:} needs hex bytes", pos) from None
        if len(raw) < 2 or raw[0] != 2 or raw[1] >= len(CODE_LENGTHS) or len(raw) != CODE_LENGTHS[raw[1]]:
            raise EncodeError("{raw:%s} is not a whole control code" % arg, pos)
        return raw
    raise EncodeError("unknown token {%s%s}" % (name, ":" + arg if arg is not None else ""), pos)


def tokens(text: str) -> list[str]:
    """The {tokens} of a text, in order (a new line is not one)."""
    return [m.group(0) for m in _TOKEN.finditer(text)]


# --------------------------------------------------------------------------
# widths (measureText in text_tools.c: the sum of advance + dx per glyph)

@dataclass
class Metrics:
    """Advance + dx of every glyph and icon of one style (lists indexed by sheet position)."""
    glyphs: list[int]  # glyph n (byte n + 4)
    icons: list[int]  # icon n (n - 1)

    def char_width(self, raw: bytes, p: int) -> tuple[int, int]:
        """(width, bytes used) of the character at raw[p]."""
        c = raw[p]
        if c == 1:
            n = raw[p + 1] if p + 1 < len(raw) else 0
            return (self.icons[n - 1] if 0 < n <= len(self.icons) else 0), 2
        if c == 2:
            return 0, CODE_LENGTHS[raw[p + 1]] if p + 1 < len(raw) else 2
        if c == 3:
            return self.glyphs[0], 1
        g = c - 4
        return (self.glyphs[g] if 0 <= g < len(self.glyphs) else 0), 1


def line_widths(raw: bytes, m: Metrics) -> list[int]:
    """Pixel width of every line (new lines, page breaks and waits end one)."""
    widths = []
    w = 0
    p = 0
    while p < len(raw):
        c = raw[p]
        if c == 2 and p + 1 < len(raw) and raw[p + 1] in (C_NEWLINE, C_PAGE):
            widths.append(w)
            w = 0
            p += CODE_LENGTHS[raw[p + 1]]
            continue
        cw, used = m.char_width(raw, p)
        w += cw
        p += used
    widths.append(w)
    return widths


def page_lines(raw: bytes) -> list[int]:
    """Lines on each page (a page break starts a new page)."""
    pages = []
    lines = 1
    p = 0
    while p < len(raw):
        c = raw[p]
        if c == 1:
            p += 2
        elif c == 2:
            code = raw[p + 1]
            if code == C_NEWLINE:
                lines += 1
            elif code == C_PAGE:
                pages.append(lines)
                lines = 1
            p += CODE_LENGTHS[code]
        else:
            p += 1
    pages.append(lines)
    return pages


# --------------------------------------------------------------------------
# glyph sheet metrics from the decomp source (text_tools.c, one version's branch)

def _glyph_array(src: str, name: str, size: int, version: str) -> list[tuple[int, ...]]:
    m = re.search(r"Glyph %s_%d\[(\d+)\] = \{(.*?)\n\};" % (name, size), src, re.S)
    if not m:
        raise TextFormatError("%s_%d not found in the decomp source" % (name, size))
    cells: list[tuple[int, ...]] = []
    active = True
    for line in m.group(2).split("\n"):
        t = line.strip()
        if t.startswith(("#if", "#elif")):
            active = t.split()[1] == "VERSION_" + version.upper()
            continue
        if t.startswith("#endif"):
            active = True
            continue
        if active:
            cells += [tuple(int(x) for x in g.split(",")) for g in re.findall(r"\{([^{}]+)\}", line)]
    if len(cells) != int(m.group(1)) and name == "FONT_ICONS":
        raise TextFormatError("%s_%d: %d cells, expected %s" % (name, size, len(cells), m.group(1)))
    return cells


def load_metrics(decomp: Path, version: str) -> dict[int, Metrics]:
    """{style 1..3: Metrics} for "eu" or "us" from vendor/dw3_decomp."""
    src = (Path(decomp) / "src/engine/text/text_tools.c").read_text()
    out = {}
    for size in (1, 2, 3):
        g = _glyph_array(src, "FONT_GLYPHS", size, version)
        i = _glyph_array(src, "FONT_ICONS", size, version)
        out[size] = Metrics([c[9] + c[7] for c in g], [c[9] + c[7] for c in i])
    return out
