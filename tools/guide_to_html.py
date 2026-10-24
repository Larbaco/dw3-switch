#!/usr/bin/env python3
"""Convert the saved GameFAQs page of the Digimon World 3 guide (EmeraldPhoenix)
into a single navigable HTML file (sidebar, scroll-spy, full-text search).

Usage: python3 -I tools/guide_to_html.py <saved-gamefaqs-page.txt> <out.html>

The guide body is plain text inside <pre id="faqspan-N"> blocks; headings are
ASCII banners (`* * *` boxes), dash boxes and dash underlines.
"""
import html
import copy
from pathlib import Path
import hashlib
from collections import defaultdict
from html.parser import HTMLParser
import json
import re
import sys
import argparse
import importlib.util


def site_module():
    # An explicit sibling path also works with Python's isolated (-I) mode.
    spec = importlib.util.spec_from_file_location('dw3_guide_site', Path(__file__).with_name('guide_site.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

STAR = re.compile(r"^\* \* \* \*[ *]*$")
DASH = re.compile(r"^-{15,}\s*$")
UNDERLINE = re.compile(r"^-{3,}\s*$")
SMALL = {"of", "the", "and", "a"}


def extract_text(page):
    start = page.index('<div class="faqtext')
    end = page.rindex("</pre>")
    body = page[start:end]
    body = re.sub(r"</?(?:pre|div|button|p)\b[^>]*>", "", body)
    text = html.unescape(body)
    text = text.replace("\r", "").replace("\t", "    ")
    text = re.sub(r"\n?Restore Page\s*$", "", text)
    return text.split("\n")


def despace(title):
    t = title.strip()
    if re.fullmatch(r"(\S )+\S(  (\S )*\S)*", t):
        words = [w.replace(" ", "") for w in re.split(r" {2,}", t)]
        return " ".join(w.lower() if w.lower() in SMALL else w.capitalize() for w in words)
    return " ".join(t.split())


def slug(s):
    s = re.sub(r"[^a-z0-9]+", "-", s.lower()).strip("-")
    return s or "x"


def clean_text(lines):
    while lines and not lines[0].strip():
        lines = lines[1:]
    while lines and not lines[-1].strip():
        lines = lines[:-1]
    out, blank = [], 0
    for l in lines:
        blank = blank + 1 if not l.strip() else 0
        if blank <= 3:
            out.append(l.rstrip())
    return "\n".join(out)


def parse(lines):
    # --- top-level banners ---------------------------------------------
    banners = []  # (line index of first star line, title)
    for i in range(len(lines) - 2):
        if STAR.match(lines[i]) and STAR.match(lines[i + 2]) and lines[i + 1].strip():
            banners.append((i, despace(lines[i + 1])))
    header = clean_text(lines[: banners[0][0]])
    sections = []
    used = set()

    def uid(base):
        n, cand = 2, base
        while cand in used:
            cand = f"{base}-{n}"
            n += 1
        used.add(cand)
        return cand

    for b, (start, title) in enumerate(banners):
        if title.lower() == "table of contents":
            continue
        end = banners[b + 1][0] if b + 1 < len(banners) else len(lines)
        body = lines[start + 3 : end]
        sec = {"id": uid(slug(title)), "title": title, "intro": [], "chapters": []}
        cur = None  # current chapter (L2)
        buf = []  # (kind, ...) items for the current container
        j = 0
        pending = []

        def flush():
            txt = clean_text(pending)
            if txt:
                (cur["items"] if cur else sec["intro"]).append(("pre", txt))
            pending.clear()

        while j < len(body):
            l = body[j]
            if (
                DASH.match(l)
                and j + 2 < len(body)
                and body[j + 1].strip()
                and DASH.match(body[j + 2])
            ):
                flush()
                t = " ".join(body[j + 1].split())
                t = re.sub(r"^xx\.\s*", "", t)
                cur = {"id": uid(slug(t)), "title": t, "items": []}
                sec["chapters"].append(cur)
                j += 3
                continue
            if (
                l.strip()
                and not l.startswith(" ")
                and j + 1 < len(body)
                and UNDERLINE.match(body[j + 1])
                and abs(len(body[j + 1].rstrip()) - len(l.rstrip())) <= 2
                and (j == 0 or not body[j - 1].strip())
            ):
                flush()
                t = " ".join(l.split())
                owner = cur or sec
                (cur["items"] if cur else sec["intro"]).append(("h", uid(f"{owner['id']}--{slug(t)}"), t))
                j += 2
                continue
            pending.append(l)
            j += 1
        flush()
        sections.append(sec)
    return header, sections


BOX_BORDER = re.compile(r"^ -{20,}\s*$")
EXPBIT = re.compile(r"(\d[\d,]*)\s*EXP\s+(\d[\d,]*)\s*BIT")
LABELED = re.compile(r"^(Enemies|Items|Digimon Battles?):\s*(.*?)\s*$")
ENEMY_ENTRY = re.compile(r"^[A-Za-z][A-Za-z0-9 ()'.&-]*:?$")
ENEMY_FIELD = re.compile(r"^\s+(Exp|Bit|Loc|Item):\s*(.*)$")
BOX_KINDS = {"DIGIMON BATTLE": "digimon", "BOSS BATTLE": "boss", "CARD BATTLE": "card"}
STATS = {"fallback": 0}


def esc(s):
    return html.escape(s, quote=True)


def is_prose(ls):
    if any(re.search(r"\S {3,}\S", l) or "\t" in l or re.match(r"^\s*[|+]", l) for l in ls):
        return False
    if len(ls) == 1:
        l = ls[0]
        return len(l.strip()) >= 35 and not l.rstrip().endswith(":") and not l.startswith("   ")
    if ls[0].startswith("   "):
        return False
    return all(len(l) >= 45 for l in ls[:-1])


CALLOUT = re.compile(r"^(NOTE|Note|Tip|TIP|Warning|WARNING|Important|IMPORTANT)\s*:")
CALLOUT_KIND = {"note": ("info", "info"), "tip": ("tip", "ok"), "warning": ("warn", "boss"), "important": ("warn", "boss")}


def prose_html(ls, link):
    t = " ".join(" ".join(ls).split())
    cm = CALLOUT.match(t)
    if cm:
        icon, cls = CALLOUT_KIND[cm.group(1).lower()]
        return f'<aside class="callout {cls}">{svg(icon)}<p>{link(esc(t))}</p></aside>'
    m = re.match(r"^([A-Z][A-Za-z' ]{2,24}):(\s|$)", t)
    body = link(esc(t))
    if m:
        body = "<strong>" + esc(m.group(1)) + ":</strong>" + link(esc(t[m.end(1) + 1 :]))
    return f"<p>{body}</p>"


def num(x):
    return x.replace(",", "")


def numcell(v, mx):
    """Mono number with a small inline bar scaled to the table max (v and mx are digit strings)."""
    try:
        pct = 100 * int(v) / mx if mx else 0
    except ValueError:
        return esc(v)
    return f'<span class="n">{v}</span><i class="bar" style="--w:{pct:.1f}%" aria-hidden="true"></i>'


def table(head, rows, cls, num_cols=()):
    th = "".join(f'<th scope="col"{" class=num" if i in num_cols else ""}>{h}</th>' for i, h in enumerate(head))
    tr = "".join(
        "<tr>" + "".join(f'<td{" class=num" if i in num_cols else ""}>{c}</td>' for i, c in enumerate(r)) + "</tr>"
        for r in rows
    )
    return f'<div class="tw"><table class="{cls}"><thead><tr>{th}</tr></thead><tbody>{tr}</tbody></table></div>'


def fmt_item(s, link):
    s = esc(s)
    s = re.sub(r"\(BOSS\)", '<span class="tag">BOSS</span>', s)
    return link(s)


def labeled_html(ls, link):
    m = LABELED.match(ls[0])
    kind = "Digimon Battles" if m.group(1).startswith("Digimon Battle") else m.group(1)
    rows = ([" " + m.group(2)] if m.group(2) else []) + [l for l in ls[1:] if l.strip()]
    if not rows or not all(l.startswith(" ") for l in rows):
        return None
    if kind == "Items":
        lis = "".join(f"<li>{fmt_item(l.strip(), link)}</li>" for l in rows)
        return f'<div class="lab"><h4>Items</h4><ul class="chips">{lis}</ul></div>'
    base = len(rows[0]) - len(rows[0].lstrip())
    out, notes = [], {}
    for l in rows:
        if len(l) - len(l.lstrip()) >= base + 2 and out and kind != "Items":
            notes[len(out) - 1] = l.strip()
            continue
        cols = re.split(r"\s{2,}", l.strip())
        k = next((i for i, c in enumerate(cols) if re.fullmatch(r"[\d,]+ [ER]XP", c)), -1)
        if k < 0 or k + 1 >= len(cols) or not re.fullmatch(r"[\d,]+ BIT", cols[k + 1], re.I):
            return None
        out.append((cols[:k], num(cols[k][:-4]), num(cols[k + 1][:-4]), cols[k + 2 :]))
    nlead = {len(r[0]) for r in out}
    if len(nlead) != 1 or nlead.pop() not in (1, 2):
        return None
    battles = len(out[0][0]) == 2
    head = ["Tamer", "Digimon"] if battles else ["Digimon"]
    head += ["EXP", "BIT"]
    drops = any(r[3] for r in out)
    if drops:
        head.append("Drop")
    rows_html = []
    mxe = max(int(e) for _, e, _, _ in out if e.isdigit()) if any(e.isdigit() for _, e, _, _ in out) else 0
    k = len(out[0][0])
    for i, (lead, e, b, extra) in enumerate(out):
        cells = [link(esc(c)) for c in lead]
        cells = [c.replace("(BOSS)", '<span class="tag">BOSS</span>') for c in cells]
        cells += [numcell(e, mxe), f'<span class="n">{b}</span>']
        if drops:
            cells.append(fmt_item(" ".join(extra), link) + (f' <small>{esc(notes[i])}</small>' if i in notes else ""))
        elif i in notes:
            cells[-1] += f' <small>{esc(notes[i])}</small>'
        rows_html.append(cells)
    title = "Digimon Battles" if battles else "Enemies"
    return f'<div class="lab"><h4>{title}</h4>{table(head, rows_html, "grid", (k, k + 1))}</div>'


def enemy_entry(ls):
    if not (len(ls) >= 2 and ENEMY_ENTRY.match(ls[0]) and all(ENEMY_FIELD.match(l) for l in ls[1:])):
        return None
    d = {m.group(1): m.group(2).strip() for m in map(ENEMY_FIELD.match, ls[1:])}
    return ls[0].rstrip(":"), d


BOX_META = re.compile(r"^\s*(LV \d+)\s+(.*?)\s{2,}(Booster \S+)\s*$")
BOX_ICON = {"digimon": "swords", "boss": "skull", "card": "cards"}


def box_html(ls, link):
    if len(ls) < 5 or not BOX_BORDER.match(ls[0]) or not BOX_BORDER.match(ls[-1]):
        return None
    inner = []
    for l in ls[1:-1]:
        if BOX_BORDER.match(l):
            inner.append(None)
            continue
        m = re.match(r"^\|(.*)\|\s*$", l)
        if not m:
            return None
        inner.append(m.group(1).rstrip())
    if len(inner) < 2 or inner[1] is not None:
        return None
    m = re.match(r"^\s*(DIGIMON BATTLE|BOSS BATTLE|CARD BATTLE):?\s*(.*?)\s*$", inner[0])
    if not m:
        return None
    kind, label, title = BOX_KINDS[m.group(1)], m.group(1).title(), m.group(2)
    body = inner[2:]
    if None in body:
        return None
    rows, paras, cur, meta = [], [], [], []

    def flush():
        if cur:
            paras.append(list(cur))
            cur.clear()

    for l in body:
        mm = EXPBIT.search(l)
        mt = BOX_META.match(l)
        if mt and not mm:
            flush()
            meta.append(mt)
        elif mm:
            flush()
            rows.append((l[: mm.start()].strip(), num(mm.group(1)), num(mm.group(2))))
        elif not l.strip():
            flush()
        else:
            cur.append(l)
    flush()
    metahtml = "".join(
        f'<span class="pill lv">{esc(mt.group(1))}</span><span class="pill">{esc(mt.group(2))}</span>'
        f'<span class="pill ghost">{link(esc(mt.group(3)))}</span>'
        for mt in meta
    )
    rows_html = ""
    for name, e, b in rows:
        who = f'<span class="who">{link(esc(name))}</span>' if name else '<span class="who"></span>'
        rows_html += (f'<div class="row">{who}<span class="pill ex"><b>{e}</b> EXP</span>'
                      f'<span class="pill bit"><b>{b}</b> BIT</span></div>')
    notes = ""
    for pl in paras:
        ind = min(len(x) - len(x.lstrip()) for x in pl)
        pl = [x[ind:] for x in pl]
        if any(re.search(r"\S {3,}\S", x) or x.startswith("  ") for x in pl):
            notes += f"<pre>{link(esc(chr(10).join(pl)))}</pre>"
        else:
            notes += f"<p>{link(esc(' '.join(' '.join(pl).split())))}</p>"
    if notes:
        notes = f'<details class="strat" open><summary>Estratégia</summary>{notes}</details>'
    head = (f'<div class="bh"><span class="bk">{svg(BOX_ICON[kind], "ic sm")}{label}</span>'
            f'<span class="bt">{link(esc(title))}</span></div>')
    metarow = f'<div class="meta">{metahtml}</div>' if metahtml else ""
    return f'<aside class="battle {kind}">{head}{metarow}{rows_html}{notes}</aside>'


VER_HEAD = re.compile(r"^Version (\S+)\s+-\s+(.*)$")
VER_DATE = re.compile(r"^\s+(\d{1,2}/\d{1,2}/\d{2,4})\s+(.*)$")
BULLET = re.compile(r"^\s*-\s+(.*)$")
CREDIT = re.compile(r"^([A-Za-z][\w .'&/-]{0,40}?) - (.+)$")


def version_html(text, link):
    """Version History as a timeline; None (-> <pre>) if any block does not match."""
    out = []
    for blk in re.split(r"\n{2,}", text.strip("\n")):
        ls = blk.split("\n")
        m = VER_HEAD.match(ls[0])
        if not m:
            return None
        ver, title = m.group(1), m.group(2)
        j = 1
        while j < len(ls) and not VER_DATE.match(ls[j]):
            if not ls[j].startswith(" "):
                return None
            title += " " + ls[j].strip()
            j += 1
        if j >= len(ls):
            return None
        dm = VER_DATE.match(ls[j])
        date, rest = dm.group(1), dm.group(2)
        bullets = []
        if rest.startswith("- "):
            bullets.append(rest[2:])
        elif rest:
            title += " " + rest
        for l in ls[j + 1 :]:
            bm = BULLET.match(l)
            if bm:
                bullets.append(bm.group(1))
            elif bullets:
                bullets[-1] += " " + l.strip()
            else:
                title += " " + l.strip()
        lis = "".join(f"<li>{link(esc(b.strip()))}</li>" for b in bullets)
        out.append(
            f'<li class="ver"><div class="vh"><span class="vb">Version {esc(ver)}</span>'
            f'<span class="vd">{esc(date)}</span></div>'
            f'<p class="vt">{link(esc(" ".join(title.split())))}</p>'
            f'{"<ul>" + lis + "</ul>" if lis else ""}</li>'
        )
    if not out:
        return None
    return f'<ol class="tl">{"".join(out)}</ol>'


def credits_html(text, link):
    """Credits: 'Name - text' blocks become a list; anything else is read as prose/pre."""
    parts, lis = [], []
    for blk in re.split(r"\n{2,}", text.strip("\n")):
        ls = blk.split("\n")
        m = CREDIT.match(ls[0])
        if m and all(l.startswith(" ") for l in ls[1:]):
            desc = " ".join(" ".join([m.group(2)] + [l.strip() for l in ls[1:]]).split())
            lis.append(f"<li><b>{esc(m.group(1))}</b><span>{link(esc(desc))}</span></li>")
        else:
            if lis:
                parts.append(f'<ul class="credits">{"".join(lis)}</ul>')
                lis = []
            parts.append(blocks_html(blk, link))
    if lis:
        parts.append(f'<ul class="credits">{"".join(lis)}</ul>')
    return "\n".join(parts)


def blocks_html(text, link):
    """Turn one text run into HTML: prose reflowed, boxes/lists/tables structured, rest <pre>."""
    out, pre = [], []

    def flush_pre():
        if pre:
            out.append(f"<pre>{link(esc(chr(10).join(pre)))}</pre>")
            pre.clear()

    enemies = []

    def flush_enemies():
        if enemies:
            mxe = max([int(d["Exp"]) for _, d in enemies if d.get("Exp", "").isdigit()] or [0])
            rows = [
                [link(esc(n)), numcell(d.get("Exp", ""), mxe) if d.get("Exp", "").isdigit() else esc(d.get("Exp", "")),
                 f'<span class="n">{esc(d.get("Bit", ""))}</span>', link(esc(d.get("Loc", ""))), fmt_item(d.get("Item", ""), link)]
                for n, d in enemies
            ]
            out.append(table(["Digimon", "EXP", "BIT", "Location", "Drop"], rows, "grid", (1, 2)))
            enemies.clear()

    for blk in re.split(r"\n{2,}", text):
        ls = blk.split("\n")
        ent = enemy_entry(ls)
        if ent:
            flush_pre()
            enemies.append(ent)
            continue
        flush_enemies()
        h = None
        if BOX_BORDER.match(ls[0]):
            h = box_html(ls, link)
        elif LABELED.match(ls[0]):
            h = labeled_html(ls, link)
        elif is_prose(ls):
            h = prose_html(ls, link)
        if h:
            flush_pre()
            out.append(h)
        else:
            if BOX_BORDER.match(ls[0]) or LABELED.match(ls[0]):
                STATS["fallback"] += 1
                print("fallback:", ls[0][:60].strip(), "|", ls[1][:50] if len(ls) > 1 else "", file=sys.stderr)
            if pre:
                pre.append("")
            pre.extend(ls)
    flush_enemies()
    flush_pre()
    return "\n".join(out)


ICONS = {
    "book": '<path d="M4 5a2 2 0 0 1 2-2h5v16H6a2 2 0 0 0-2 2z"/><path d="M20 5a2 2 0 0 0-2-2h-5v16h5a2 2 0 0 1 2 2z"/>',
    "map": '<path d="M9 4 3 6.5v13L9 17l6 2.5 6-2.5v-13L15 6.5z"/><path d="M9 4v13M15 6.5V19"/>',
    "swords": '<path d="M14 4.5h5.5V10L10 19.5 4.5 14z"/><path d="M4 20l3.5-3.5M8 16l-2.5 2.5"/>',
    "sparkle": '<path d="M12 3c.6 4.2 2.8 6.4 7 7-4.2.6-6.4 2.8-7 7-.6-4.2-2.8-6.4-7-7 4.2-.6 6.4-2.8 7-7z"/>',
    "evo": '<path d="M6 4v6a4 4 0 0 0 4 4h8"/><path d="m14 10 4 4-4 4"/>',
    "cards": '<rect x="3" y="5" width="12" height="16" rx="2"/><path d="M8 3h9a2 2 0 0 1 2 2v13"/>',
    "skull": '<path d="M12 3a7 7 0 0 0-7 7v3l2 2v3h2v2h6v-2h2v-3l2-2v-3a7 7 0 0 0-7-7z"/><path d="M9 11h.01M15 11h.01"/>',
    "scroll": '<path d="M7 4h11v13a3 3 0 0 0 3 3H8a3 3 0 0 1-3-3V7a3 3 0 0 1 2-3z"/><path d="M10 9h5M10 13h5"/>',
    "list": '<path d="M9 6h11M9 12h11M9 18h11"/><path d="M4 6h.01M4 12h.01M4 18h.01"/>',
    "pin": '<path d="M12 21s-6.5-5.6-6.5-11a6.5 6.5 0 0 1 13 0c0 5.4-6.5 11-6.5 11z"/><circle cx="12" cy="10" r="2.3"/>',
    "search": '<circle cx="11" cy="11" r="6.5"/><path d="m16 16 4.5 4.5"/>',
    "cmd": '<path d="M4 7h16M4 12h11M4 17h7"/>',
    "menu": '<path d="M4 7h16M4 12h16M4 17h16"/>',
    "sun": '<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/>',
    "info": '<circle cx="12" cy="12" r="9"/><path d="M12 11v5M12 8h.01"/>',
    "warn": '<path d="M12 4 2.5 20h19z"/><path d="M12 10v4M12 17h.01"/>',
    "tip": '<path d="M12 3c.6 4.2 2.8 6.4 7 7-4.2.6-6.4 2.8-7 7-.6-4.2-2.8-6.4-7-7 4.2-.6 6.4-2.8 7-7z"/>',
    "ext": '<path d="M14 4h6v6M20 4 10 14M19 14v5a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V6a1 1 0 0 1 1-1h5"/>',
    "keys": '<rect x="2" y="6" width="20" height="12" rx="2"/><path d="M6 10h.01M10 10h.01M14 10h.01M18 10h.01M7 14h10"/>',
    "up": '<path d="M12 19V5M5 12l7-7 7 7"/>',
    "prev": '<path d="m15 6-6 6 6 6"/>',
    "next": '<path d="m9 6 6 6-6 6"/>',
    "close": '<path d="M6 6l12 12M18 6 6 18"/>',
}

SEC_ICON = {
    "disclaimer": "scroll", "introduction": "book", "version-history": "list",
    "partner-digimon": "sparkle", "walkthrough": "map", "side-quests": "swords",
    "digimon-evolutions": "evo", "enemy-digimon": "skull", "card-battle": "cards",
    "things-left-undone": "list", "credits": "scroll",
}
SEC_DESC = {
    "disclaimer": "Direitos de uso e distribuição do texto.",
    "introduction": "Por que o guia existe e como foi feito.",
    "version-history": "Histórico de versões e mudanças.",
    "partner-digimon": "Kotemon, Patamon e Renamon: stats, ataques e digievolução.",
    "walkthrough": "Do início ao fim, organizado por capítulos e locais.",
    "side-quests": "Missões paralelas e DRI Agents.",
    "digimon-evolutions": "Evoluções: como obter, atributos, níveis e técnicas.",
    "enemy-digimon": "Inimigos com EXP, BIT, localização e drops.",
    "card-battle": "Boosters, cartas e probabilidades do jogo de cartas.",
    "things-left-undone": "O que o autor ainda planejava para o guia.",
    "credits": "Quem tornou o guia possível.",
}


def svg(name, cls="ic"):
    return (f'<svg class="{cls}" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" '
            f'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">{ICONS[name]}</svg>')


def count_label(s):
    """Short count shown on the home tiles."""
    if s["chapters"]:
        n = len(s["chapters"])
        return f'{n} capítulo' + ("s" if n != 1 else "")
    if s["id"] == "card-battle":
        return f'{sum(1 for i in s["intro"] if i[0] == "h")} boosters'
    n = sum(1 for i in s["intro"] if i[0] == "h")
    if n:
        return f"{n} seções"
    txt = "\n".join(i[1] for i in s["intro"] if i[0] == "pre")
    e = len(re.findall(r"^   Exp:", txt, re.M))
    if e:
        return f"{e} inimigos"
    v = len(re.findall(r"^Version ", txt, re.M))
    return f"{v} versões" if v else ""


def browse_sections(sections):
    """Prioritize play/reference navigation without reordering the source text."""
    order = ['walkthrough', 'side-quests', 'partner-digimon', 'digimon-evolutions',
             'enemy-digimon', 'card-battle', 'introduction', 'disclaimer',
             'version-history', 'things-left-undone', 'credits']
    return sorted(sections, key=lambda s: order.index(s['id']) if s['id'] in order else len(order))


def cover_html(header, sections):
    ver = re.search(r"Version (\S+)", header)
    date = re.search(r"\d\d/\d\d/\d\d", header)
    credit = f"by Emerald Phoenix · v{ver.group(1) if ver else '?'} · {date.group(0) if date else ''}"
    tiles = "".join(
        f'<a class="tile" href="#{s["id"]}"><span class="ti">{svg(SEC_ICON.get(s["id"], "list"))}</span>'
        + (f'<span class="tc">{esc(count_label(s))}</span>' if count_label(s) else "")
        + f'<span class="tt">{esc(s["title"])}</span><span class="td">{esc(SEC_DESC.get(s["id"], ""))}</span></a>'
        for s in browse_sections(sections)
    )
    return (
        '<header class="hero" id="inicio">'
        '<p class="eyebrow">Guia do jogo</p>'
        '<h1>Digimon World 3</h1>'
        '<p class="sub">Guide &amp; Walkthrough · PlayStation</p>'
        '<p class="hero-note">Seu diário de viagem pelo Digital World. '
        'Siga a história em português ou consulte locais, evoluções e recompensas.</p>'
        '<nav class="hero-actions" aria-label="Começar a consulta">'
        + ''.join(f'<a href="#{s["id"]}">{label}</a>' for key, label in
                  [('walkthrough', 'Começar o passo a passo'), ('digimon-evolutions', 'Consultar evoluções')]
                  for s in sections if s['id'] == key) + '</nav>'
        '<a id="resume" hidden>Continuar leitura</a>'
        f'<p class="credit">{esc(credit)}</p>'
        '<p class="links"><a href="https://gamefaqs.gamespot.com/ps/562323-digimon-world-3/faqs/18310" '
        'target="_blank" rel="noopener">Página original no GameFAQs ' + svg("ext", "ic sm") + '</a>'
        '<span>Texto © Emerald Phoenix — exibição local</span></p>'
        f'<details class="orig"><summary>Cabeçalho original</summary><pre>{esc(header)}</pre></details>'
        '</header>'
        f'<section class="home" aria-labelledby="secoes"><h2 id="secoes" class="sr">Seções do guia</h2>'
        f'<div class="tiles">{tiles}</div></section>'
    )


def nav_html(sections):
    nav = []
    for s in browse_sections(sections):
        ic = svg(SEC_ICON.get(s["id"], "list"))
        if s["chapters"]:
            kids = "".join(
                f'<li><a href="#{c["id"]}" data-t="{esc(c["title"].lower())}">{esc(c["title"])}</a></li>'
                for c in s["chapters"]
            )
            nav.append(
                f'<details class="grp" data-sec="{s["id"]}"><summary>{ic}<a href="#{s["id"]}">{esc(s["title"])}</a>'
                f'<span class="n">{len(s["chapters"])}</span><i class="gp" aria-hidden="true"></i></summary>'
                f'<ul>{kids}</ul></details>'
            )
        else:
            nav.append(f'<a class="top" href="#{s["id"]}" data-sec="{s["id"]}">{ic}{esc(s["title"])}</a>')
    return nav


def palette_json(sections, entities):
    """Items for the Ctrl+K palette: headings and every indexed entity/role."""
    items = []
    for s in sections:
        items.append({"t": s["title"], "g": "Seção", "id": s["id"], "k": "sec"})
        for c in s["chapters"]:
            items.append({"t": c["title"], "g": s["title"], "id": c["id"], "k": "cap", "en": c.get("original_title", "")})
            items += [{"t": it[2], "g": c["title"], "id": it[1], "k": "loc"} for it in c["items"] if it[0] == "h"]
        items += [{"t": it[2], "g": s["title"], "id": it[1], "k": "loc"} for it in s["intro"] if it[0] == "h"]
    items.append({"t": "Índice de entidades", "g": "Guia", "id": entities.index_id, "k": "sec"})
    items += [{"t": entry["name"], "g": " / ".join(sorted(entry["kinds"])),
               "id": entry["id"], "k": "ent"} for entry in entities.entries.values()]
    return json.dumps(items, ensure_ascii=False).replace("</", "<\\/")


CARD_SECS = {"disclaimer", "introduction", "things-left-undone", "credits"}
DATA_SECS = {"digimon-evolutions", "partner-digimon"}


def banner_html(c, s):
    """Chapter banner: faded numeral, title, and chips linking to the L3 headings of the chapter."""
    # A return visit is a different part of the route, with its own stable anchor.
    seen = [(it[2], it[1]) for it in c["items"] if it[0] == "h"]
    visits = defaultdict(int)
    counts = defaultdict(int)
    for title, _ in seen:
        counts[title] += 1
    def visit_label(title):
        visits[title] += 1
        return title + (f' · visita {visits[title]}' if counts[title] > 1 else '')
    m = re.match(r"^(\d+)\.", c["title"])
    big = m.group(1).zfill(2) if m else c["title"][:1].upper()
    chips = "".join(f'<li><a href="#{v}">{esc(visit_label(k))}</a></li>' for k, v in seen)
    label = 'Roteiro de áreas' if s['id'] == 'walkthrough' else 'Neste capítulo'
    chips = (f'<details class="chapter-route"><summary>{label} · {len(seen)} paradas</summary>'
             f'<ol class="route-links">{chips}</ol></details>') if chips else ''
    n = len(seen)
    cnt = f'<p class="cnt">{n} {"local" if n == 1 else "seções"}</p>' if n else ""
    strip = stat_strip(c) if s["id"] == "digimon-evolutions" else ""
    return (f'<header class="chb"><span class="num" aria-hidden="true">{esc(big)}</span>'
            f'<p class="kick">{esc(s["title"])}</p>'
            f'<h3 class="ch"><a href="#{c["id"]}">{esc(c["title"])}</a></h3>{cnt}{chips}{strip}</header>')


ATTR_NAMES = ("Strength", "Defense", "Spirit", "Wisdom", "Speed", "Charisma")
ELEM_NAMES = ("Fire", "Water", "Ice", "Wind", "Thunder", "Machine", "Dark")
ELEM_VAR = {"Fire": "fire", "Water": "water", "Ice": "ice", "Wind": "wind", "Thunder": "thunder",
            "Lightning": "thunder", "Machine": "machine", "Dark": "dark", "Healing": "heal", "Neutral": "neutral"}
STAT_ABBR = {"STR": "Strength", "DEF": "Defense", "SPR": "Spirit", "WIS": "Wisdom", "SPD": "Speed", "CHA": "Charisma"}
PAIR = re.compile(r"([A-Za-z]+)(?: Tolerance)?:\s*([+-]?)\s*(\d+)")
TECH_NAME = re.compile(r"^   (?!\s)(?:([^\s:][^:]*?))?:?\s*$")
TF_TYPE = re.compile(r"^      Type:\s*(.*?)\s*Level [Ll]earned:\s*(.*?)\s*$")
TF_COST = re.compile(r"^      Cost:\s*(.*?)\s*Level [Ll]oaded:\s*(.*?)\s*$")
TF_TEXT = re.compile(r"^      (Game Text|Description):\s*(.*?)\s*$")
TF_CONT = re.compile(r"^ {8,}(\S.*?)\s*$")
LVL_ROW = re.compile(r"^\s*Level (\d+)(.*?)(?:\s+Level (\d+)(.*))?\s*$")
TREE_NODE = re.compile(r"^(\s*)-> (.+?)(?:\s+\((.*)\))?\s*$")
EXP_TOK = re.compile(r"Level (\d+):\s*([\d,]*)")


def orig_html(text):
    """Collapsed copy of the guide's ASCII original, kept next to every structured block."""
    return f'<details class="orig"><summary>ver texto original</summary><pre>{esc(text)}</pre></details>'


def dig_link(name, cx):
    return cx["link"](esc(name.strip()))


def req_chip(r, cx):
    """One requirement: 'Name LV 5', 'WIS 180', '[Wind] 120' or raw text."""
    m = re.match(r"^(.+?)\s+LV\s+(\d+)$", r)
    if m:
        return f'<span class="rq lv">{dig_link(m.group(1), cx)}<span class="lvn">LV {m.group(2)}</span></span>'
    m = re.match(r"^([A-Z]{3})\s+(\d+)$", r)
    if m and m.group(1) in STAT_ABBR:
        return f'<span class="rq st" title="{STAT_ABBR[m.group(1)]}">{m.group(1)} <b>{m.group(2)}</b></span>'
    m = re.match(r"^\[?([A-Za-z]+)\]?\s+(\d+)$", r)
    if m:
        var = ELEM_VAR.get(m.group(1))
        style = f' style="--c:var(--{var})"' if var else ""
        return f'<span class="rq el"{style}>{esc(m.group(1))} <b>{m.group(2)}</b></span>'
    return f'<span class="rq">{esc(r)}</span>'


def stat_pairs(text):
    """'Name: +  0      Fire Tolerance: + 15' lines -> [(name, value, is_element)], None on any oddity."""
    out = []
    for l in text.split("\n"):
        if not l.strip():
            continue
        if not l.startswith("   "):
            return None
        found = list(PAIR.finditer(l))
        if not found or l.count(":") != len(found):
            return None
        for m in found:
            name = m.group(1)
            if name not in ATTR_NAMES and name not in ELEM_NAMES:
                return None
            v = int(m.group(3)) * (-1 if m.group(2) == "-" else 1)
            out.append((name, v, name in ELEM_NAMES))
    return out or None


def stat_strip(c):
    """Chapter summary chips: the three largest positive stat gains of an evolution chapter."""
    items = c["items"]
    for i, it in enumerate(items):
        if it[0] == "h" and it[2].upper() == "STAT CHANGES" and i + 1 < len(items) and items[i + 1][0] == "pre":
            pairs = stat_pairs(items[i + 1][1]) or []
            top = sorted((p for p in pairs if p[1] > 0), key=lambda p: -p[1])[:3]
            if not top:
                return ""
            chips = ""
            for n, v, el in top:
                style = f' style="--c:var(--{ELEM_VAR.get(n, "ac")})"' if el else ""
                chips += f'<span class="sc"{style}>{esc(n)} <b>+{v}</b></span>'

            return f'<div class="sstrip" aria-label="Maiores ganhos">{chips}</div>'
    return ""


def acquired_html(text, cx):
    rows = []
    for l in text.split("\n"):
        if not l.strip():
            continue
        m = re.match(r"^   (\S[^:]*):\s+(.+)$", l)
        if not m:
            return None
        reqs = "".join(req_chip(r.strip(), cx) for r in m.group(2).split(", "))
        rows.append(f'<div class="acq"><div class="who">{dig_link(m.group(1), cx)}</div><div class="reqs">{reqs}</div></div>')
    return f'<div class="acqlist">{"".join(rows)}</div>' if rows else None


def stat_bars(pairs, mx):
    """Bars for a list of (name, value, is_element); negatives red, scale shared by the whole chapter."""
    rows = ""
    for n, v, el in pairs:
        pct = 100 * abs(v) / mx
        cls = "neg" if v < 0 else "pos" if v > 0 else "zero"
        var = f' style="--c:var(--{ELEM_VAR.get(n, "ac")})"' if el else ""
        sign = f"+{v}" if v > 0 else str(v)
        rows += (f'<div class="srow {cls}"{var}><span class="sl">{esc(n)}</span>'
                 f'<span class="sbar"><i style="--w:{pct:.1f}%"></i></span><span class="sv">{sign}</span></div>')
    return rows


def stats_html(text, cx):
    pairs = stat_pairs(text)
    if not pairs:
        return None
    mx = max(abs(v) for _, v, _ in pairs) or 1
    attrs = [p for p in pairs if not p[2]]
    elems = [p for p in pairs if p[2]]
    return (f'<div class="stats"><div class="sgrp"><h4>Atributos</h4>{stat_bars(attrs, mx)}</div>'
            f'<div class="sgrp"><h4>Resistências</h4>{stat_bars(elems, mx)}</div></div>')


def start_stats_html(text, cx):
    pairs = stat_pairs(text)
    if not pairs:
        return None

    def col(group, title):
        dl = "".join(f"<div><dt>{esc(n)}</dt><dd>{v}</dd></div>" for n, v, _ in group)
        return f'<div class="kv"><h4>{title}</h4><dl>{dl}</dl></div>'

    return f'<div class="kvwrap">{col([p for p in pairs if not p[2]], "Atributos")}{col([p for p in pairs if p[2]], "Resistências")}</div>'


def level_cell(s):
    s = s.strip()
    if not s or s == "-----":
        return None
    m = re.match(r"^TECH:\s*(.+)$", s)
    if m:
        return ("learn", m.group(1).strip())
    m = re.match(r"^(?:LOAD|LD):?\s*(.+)$", s)
    if m:
        return ("load", m.group(1).strip())
    return ("learn", s)


def level_html(text, cx):
    cells = {}
    for l in text.split("\n"):
        if not l.strip():
            continue
        m = LVL_ROW.match(l)
        if not m:
            return None
        for lv, content in ((m.group(1), m.group(2)), (m.group(3), m.group(4))):
            if lv is None:
                continue
            if lv in cells:
                return None
            cells[lv] = level_cell(content or "")
    if not cells:
        return None
    out = []
    for lv in sorted(cells, key=int):
        c = cells[lv]
        if c is None:
            out.append(f'<li class="lc empty"><span class="lv">Lv {lv}</span></li>')
            continue
        kind, name = c
        nm = cx["link"](esc(name))
        badge = "APRENDE" if kind == "learn" else "CARREGA"
        out.append(f'<li class="lc {kind}"><span class="lv">Lv {lv}</span><span class="tn">{nm}</span>'
                   f'<span class="bd">{badge}</span></li>')
    return f'<ol class="lvgrid">{"".join(out)}</ol>'


def lvcell(v):
    v = v.strip()
    if not v or v in ("??", "N/A"):
        return f'<dd class="na">{esc(v) if v else "—"}</dd>'
    return f"<dd>{esc(v)}</dd>"


def tcard(t, cx):
    typ = t.get("type", "")
    parts = [p.strip() for p in typ.split(",")] if typ else []
    elem = parts[0].strip("[]") if parts else ""
    kind = parts[1] if len(parts) > 1 else ""
    low = (elem + " " + kind).lower()
    badges = ""
    if not elem.lower().startswith(("phy", "mag")) and elem:
        var = ELEM_VAR.get(elem)
        style = f' style="--c:var(--{var})"' if var else ' style="--c:var(--neutral)"'
        badges += f'<span class="el"{style}>{esc(elem)}</span>'
    if "phy" in low:
        badges += '<span class="tg">Phys</span>'
    if "mag" in low:
        badges += '<span class="tg">Mag</span>'
    cost = t.get("cost", "").strip()
    return (
        f'<article class="tcard" id="{cx["entities"].uid(cx["cid"] + "--tech-" + slug(t["name"]))}">'
        f'<header><h4>{esc(t["name"])}</h4><div class="tbadges">{badges}</div></header>'
        f'<dl class="tst"><div><dt>Custo</dt><dd>{esc(cost) if cost else "—"}</dd></div>'
        f'<div class="lrn"><dt>Aprende Lv</dt>{lvcell(t.get("learned", ""))}</div>'
        f'<div class="ldd"><dt>Carrega Lv</dt>{lvcell(t.get("loaded", ""))}</div></dl>'
        + (f'<p class="gt"><span class="cap">Texto do jogo</span> {esc(t["game"])}</p>' if t.get("game") else "")
        + (f'<p class="ds"><span class="cap">Descrição</span> {esc(t["desc"])}</p>' if t.get("desc") else "")
        + '</article>'
    )


def techniques_html(text, cx):
    blocks, cur = [], None
    for l in text.split("\n"):
        if not l.strip():
            continue
        m = TECH_NAME.match(l)
        if m:
            cur = {"name": (m.group(1) or "").strip(), "last": None}
            blocks.append(cur)
            continue
        if cur is None:
            return None
        m = TF_TYPE.match(l)
        if m:
            cur.update(type=m.group(1), learned=m.group(2))
            cur["last"] = None
            continue
        m = TF_COST.match(l)
        if m:
            cur.update(cost=m.group(1), loaded=m.group(2))
            cur["last"] = None
            continue
        m = TF_TEXT.match(l)
        if m:
            key = "game" if m.group(1) == "Game Text" else "desc"
            cur[key] = m.group(2)
            cur["last"] = key
            continue
        m = TF_CONT.match(l)
        if m and cur["last"]:
            cur[cur["last"]] = (cur[cur["last"]] + " " + m.group(1)).strip()
            continue
        return None
    cards = "".join(tcard(t, cx) for t in blocks if t["name"])
    if not cards:
        return '<p class="muted">Nenhuma técnica documentada neste bloco do guia.</p>'
    return f'<div class="tgrid">{cards}</div>'


def exp_html(text, cx):
    rows = []
    for l in text.split("\n"):
        if not l.strip():
            continue
        toks = EXP_TOK.findall(l)
        if not toks or len(toks) != l.count("Level "):
            return None
        rows += [(int(lv), v) for lv, v in toks if v]
    if not rows:
        return None
    body = "".join(f'<tr><td class="num">{lv:02d}</td><td class="num">{v}</td></tr>' for lv, v in sorted(rows))
    return (f'<div class="tw exp"><table class="grid"><thead><tr><th class="num">Nível</th><th class="num">EXP</th>'
            f'</tr></thead><tbody>{body}</tbody></table></div>')


def tree_node(node, cx):
    reqs = "".join(req_chip(r, cx) for r in node["r"])
    head = f'<span class="dn">{dig_link(node["n"], cx)}</span>' + (f'<span class="reqs">{reqs}</span>' if reqs else "")
    if not node["c"]:
        return f'<div class="leaf">{head}</div>'
    kids = "".join(f"<li>{tree_node(c, cx)}</li>" for c in node["c"])
    return f'<details open><summary>{head}</summary><ul class="dtree">{kids}</ul></details>'


def tree_html(text, cx):
    lines = [l for l in text.split("\n") if l.strip()]
    if not lines or lines[0].startswith(" ") or "->" in lines[0]:
        return None
    root = {"n": lines[0].strip(), "r": [], "c": []}
    stack = [(-1, root)]
    for l in lines[1:]:
        m = TREE_NODE.match(l)
        if not m:
            return None
        ind = len(m.group(1))
        node = {"n": m.group(2).strip(), "r": [x.strip() for x in m.group(3).split(",")] if m.group(3) else [], "c": []}
        while stack[-1][0] >= ind:
            stack.pop()
        stack[-1][1]["c"].append(node)
        stack.append((ind, node))
    kids = "".join(f"<li>{tree_node(c, cx)}</li>" for c in root["c"])
    return (f'<div class="dtwrap"><div class="dtools"><button type="button" data-act="open">Expandir tudo</button>'
            f'<button type="button" data-act="close">Recolher tudo</button></div>'
            f'<div class="droot">{dig_link(root["n"], cx)}</div>'
            f'<ul class="dtree top">{kids}</ul></div>')


def dv_html(text, cx):
    entries = []
    for l in text.split("\n"):
        if not l.strip():
            continue
        if l.startswith("   ") and not l.startswith("      "):
            entries.append([l.strip().rstrip(":"), []])
        elif l.startswith("      ") and entries:
            entries[-1][1].append(l.strip())
        else:
            return None
    if not entries:
        return None
    cards = ""
    for name, rl in entries:
        chips = "".join(req_chip(r.strip(), cx) for r in " ".join(rl).split(", ") if r.strip())
        cards += f'<div class="dvc"><h4>{dig_link(name, cx)}</h4><div class="reqs">{chips}</div></div>'
    return f'<div class="dvgrid">{cards}</div>'


SPECIAL_H = {
    "ACQUIRED": acquired_html, "STAT CHANGES": stats_html, "LEVEL CHART": level_html,
    "TECHNIQUES": techniques_html, "SIGNATURE ATTACK": techniques_html, "STARTING STATS": start_stats_html,
    "EXPEIENCE TABLE": exp_html, "EXPERIENCE TABLE": exp_html, "DIGIVOLUTION CHART": tree_html,
    "CHAMPION DIGIVOLUTIONS": dv_html, "ULTIMATE DIGIVOLUTIONS": dv_html, "MEGA DIGIVOLUTIONS": dv_html,
}


PCT_ROW = re.compile(r"^\s*(\d+)/(\d+)\s+([\d.]+)%\s+(.+?)\s*$")


def filter_box(body, placeholder, cls=""):
    """Wrapper with a live text filter; JS hides rows/chips whose text does not match."""
    return (f'<div class="fbox{cls}"><div class="tbar"><input type="search" class="tfilt" '
            f'placeholder="{esc(placeholder)}" aria-label="{esc(placeholder)}"></div>{body}</div>')


def booster_html(text, cx):
    """Booster block: Digimon list, blank line, program list; percentage rows become a sortable table."""
    groups = [g for g in re.split(r"\n\s*\n", text.strip("\n")) if g.strip()]
    if not groups or len(groups) > 2:
        return None
    body = ""
    for i, g in enumerate(groups):
        ls = [l.strip() for l in g.split("\n") if l.strip()]
        label = "Digimon" if i == 0 else "Programa"
        pcts = [PCT_ROW.match(l) for l in ls]
        if all(pcts):
            top = max(float(m.group(3)) for m in pcts) or 1
            rows = [[cx["link"](esc(m.group(4))),
                     f'{m.group(1)}/{m.group(2)}',
                     f'<span data-v="{m.group(3)}">{m.group(3)}%</span>'
                     f'<i class="bar" style="--w:{100 * float(m.group(3)) / top:.1f}%" aria-hidden="true"></i>']
                    for m in pcts]
            content = table(["Carta", "Ocorrência", "Probabilidade"], rows, "grid sortable", (1, 2))
        elif any(pcts):
            return None
        else:
            content = '<ul class="cgrid">' + "".join(f"<li>{cx['link'](esc(l))}</li>" for l in ls) + "</ul>"
        body += f'<div class="bgrp{" prog" if i else ""}"><h4>{label}</h4>{content}</div>'
    return filter_box(body, "Filtrar cartas deste booster…", " booster")


def enemy_html(text, cx):
    """Enemy Digimon list (all blocks are 'Name:' + Exp/Bit/Loc/Item) as a sortable, filterable table."""
    ents = []
    for blk in re.split(r"\n{2,}", text.strip("\n")):
        ent = enemy_entry(blk.split("\n"))
        if not ent:
            return None
        ents.append(ent)
    if not ents:
        return None
    mxe = max([int(d["Exp"]) for _, d in ents if d.get("Exp", "").isdigit()] or [0])
    rows = []
    for n, d in ents:
        e = d.get("Exp", "")
        rows.append([
            dig_link(n, cx),
            numcell(e, mxe) if e.isdigit() else esc(e),
            f'<span class="n">{esc(d.get("Bit", ""))}</span>',
            cx["link"](esc(d.get("Loc", ""))),
            fmt_item(d.get("Item", ""), cx["link"]),
        ])
    tbl = table(["Digimon", "EXP", "BIT", "Location", "Drop"], rows, "grid sortable", (1, 2))
    return filter_box(f'<div class="tall">{tbl}</div>', "Filtrar inimigos…", " enemies")


def special_html(sec_id, head, text, cx):
    """Structured renderers for specific sections/headings; None means use the generic blocks path."""
    if sec_id == "version-history":
        return version_html(text, cx["link"])
    if sec_id == "credits":
        return credits_html(text, cx["link"])
    if sec_id == "enemy-digimon":
        out = enemy_html(text, cx)
        STATS["enemy"] = STATS.get("enemy", 0) + 1
        if not out:
            STATS["enemy_fail"] = 1
            print("fallback: enemy list [enemy-digimon] |", text.strip().split("\n")[0][:50], file=sys.stderr)
            return None
        return out + orig_html(text)
    if sec_id == "card-battle" and head:
        out = booster_html(text, cx)
        STATS["booster"] = STATS.get("booster", 0) + 1
        if not out:
            STATS["booster_fail"] = STATS.get("booster_fail", 0) + 1
            print(f"fallback: booster [{head}] |", text.strip().split("\n")[0][:50], file=sys.stderr)
            return None
        return out + orig_html(text)
    if sec_id in DATA_SECS and head.upper() in SPECIAL_H:
        out = SPECIAL_H[head.upper()](text, cx)
        STATS["special"] = STATS.get("special", 0) + 1
        if not out:
            STATS["special_fail"] = STATS.get("special_fail", 0) + 1
            print(f"fallback: {head} [{cx['cid']}] |", text.strip().split("\n")[0][:50], file=sys.stderr)
            return None
        return out + orig_html(text)
    return None


def entity_key(name):
    return " ".join(name.split()).casefold()


class EntityIndex:
    """Evidence-only names; all homonyms share an explicit, lossless destination.

    A destination lists every role/definition instead of choosing the last one.
    No fuzzy matching, singularization, or inferred color/number variants.
    """

    def __init__(self, sections):
        self.entries = {}
        self.blocks = {}
        self.labels = {}
        self.references = defaultdict(set)
        self.used = set(re.findall(r'\bid="([^"]+)"', TEMPLATE))
        for sec in sections:
            self.used.add(sec["id"])
            for owner, items in [(sec, sec["intro"])] + [(c, c["items"]) for c in sec["chapters"]]:
                self.used.add(owner["id"])
                self.used.update(it[1] for it in items if it[0] == "h")
        self.index_id = self.uid("guide-entities")
        for sec in sections:
            for owner, items in [(sec, sec["intro"])] + [(c, c["items"]) for c in sec["chapters"]]:
                cid = owner["id"]
                title = owner["title"]
                self.labels[cid] = f'{sec["title"]} / {title}'
                if sec['id'] == 'side-quests' and owner is not sec:
                    self.add(re.sub(r'^\d+\.\s*', '', title), 'Quest', cid, title)
                if sec["id"] in DATA_SECS and owner is not sec:
                    self.add(re.sub(r"^\d+\.\s*", "", title), "Digimon", cid, title)
                head = ""
                for i, it in enumerate(items):
                    if it[0] == "h":
                        head = it[2]
                        self.labels[it[1]] = f'{title} / {head}'
                        if sec["id"] == "walkthrough":
                            # Full headings and their city/room components are explicit evidence.
                            for place in [head] + head.split(" - "):
                                self.add(place.title(), "Local", it[1], self.labels[it[1]])
                        if sec["id"] == "card-battle" and "booster" in head.lower():
                            self.add(head.title(), "Booster", it[1], head)
                        continue
                    text = it[1]
                    digest = hashlib.sha256(text.encode()).hexdigest()[:10]
                    source = self.uid(f'{cid}--source-{digest}')
                    self.blocks[(cid, i)] = source
                    self.labels[source] = f'{title} / {head}' if head else title
                    if sec["id"] in DATA_SECS and head.upper() in ("TECHNIQUES", "SIGNATURE ATTACK"):
                        for line in text.splitlines():
                            m = TECH_NAME.match(line)
                            if m and m.group(1):
                                self.add(m.group(1).strip(), "Técnica", source, title)
                    if sec["id"] == "card-battle" and "booster" in head.lower():
                        for line in text.splitlines():
                            if line.strip():
                                m = PCT_ROW.match(line)
                                self.add(m.group(4) if m else line.strip(), "Carta", source, head)
                    for block in re.split(r"\n{2,}", text):
                        lines = block.splitlines()
                        # Explicit battle boxes supply bosses, roster names and booster rewards.
                        if lines and BOX_BORDER.match(lines[0]):
                            for line in lines[1:]:
                                inner = re.match(r"^\|(.*)\|\s*$", line)
                                if not inner:
                                    continue
                                cell = inner.group(1).strip()
                                boss = re.match(r"^BOSS BATTLE:\s*(.+)$", cell)
                                if boss:
                                    self.add(boss.group(1), "Inimigo", source, self.labels[source])
                                trainer = re.match(r'^(?:CARD|DIGIMON) BATTLE:\s*(.+)$', cell)
                                if trainer:
                                    self.add(trainer.group(1), 'NPC', source, self.labels[source])
                                reward = BOX_META.match(inner.group(1))
                                if reward:
                                    self.add(reward.group(3), "Booster", source, self.labels[source])
                                xp = EXPBIT.search(cell)
                                if xp:
                                    for name in cell[:xp.start()].split(","):
                                        name = name.strip()
                                        if name and ENEMY_ENTRY.fullmatch(name):
                                            self.add(name, "Inimigo", source, self.labels[source])
                        ent = enemy_entry(lines)
                        if ent:
                            name, fields = ent
                            self.add(name, "Inimigo", source, self.labels[source])
                            self.add_items(fields.get("Item", ""), source, f'Drop: {name}')
                        if not lines:
                            continue
                        m = LABELED.match(lines[0])
                        if not m:
                            continue
                        rows = ([m.group(2)] if m.group(2) else []) + lines[1:]
                        if m.group(1) == "Items":
                            for row in rows:
                                self.add_items(row.strip(), source, self.labels[source])
                        else:
                            for row in rows:
                                # Only complete EXP/BIT rows define enemies/drops.
                                match = re.match(r"^\s*(.+?)\s{2,}[\d,]+ EXP\s+[\d,]+ BIT(?:\s+(.*))?$", row)
                                if match:
                                    self.add(re.split(r"\s{2,}", match.group(1))[-1].replace("(BOSS)", "").strip().lstrip("* "),
                                             "Inimigo", source, self.labels[source])
                                    self.add_items(match.group(2) or "", source, f'Drop: {match.group(1)}')
                    if sec["id"] in DATA_SECS and head.upper() in ("WEAPONS", "ARMOR"):
                        for line in text.splitlines():
                            m = re.match(r"^\s+(.+?)\s{2,}(?:STR|DEF|SPR|WIS|SPD|CHA)\s", line)
                            if m:
                                self.add_items(m.group(1), source, self.labels[source])
        aliases = {}
        for key, entry in self.entries.items():
            for alias in entry["aliases"]:
                # Single common words remain unlinked in prose (e.g. Speed, Attack).
                if " " in alias or re.search(r"mon(?:\b|$)", alias, re.I) or "Booster" in entry["kinds"]:
                    aliases[esc(alias)] = key
        self.aliases = aliases
        self.pattern = (re.compile(r"(?<![\w'-])(?:" + "|".join(
            re.escape(a) for a in sorted(aliases, key=lambda a: (-len(a), a))) + r")(?![\w'-])", re.I)
            if aliases else None)
        self.folded_aliases = {a.casefold(): key for a, key in aliases.items()}

    def uid(self, base):
        candidate, n = base, 2
        while candidate in self.used:
            candidate = f"{base}-{n}"
            n += 1
        self.used.add(candidate)
        return candidate

    def add(self, name, kind, target, label):
        name = " ".join(name.split()).strip().rstrip(":")
        if not name or not re.search(r"[A-Za-z]", name):
            return
        key = entity_key(name)
        if key not in self.entries:
            digest = hashlib.sha256(key.encode()).hexdigest()[:10]
            self.entries[key] = {"name": name, "id": self.uid(f'entity-{slug(name)}-{digest}'),
                                 "aliases": set(), "kinds": defaultdict(dict)}
        entry = self.entries[key]
        entry["aliases"].add(name)
        entry["kinds"][kind][(target, label)] = None

    def add_items(self, text, source, label):
        for item in text.split(","):
            item = re.sub(r"\s*\(BOSS\)|\s+x\d+\s*$", "", item).strip()
            if not item or re.fullmatch(r"[\d,]+ BIT|None|N/A|\?+", item, re.I):
                continue
            if re.fullmatch(r"[A-Za-z][A-Za-z0-9 '&()./-]*", item):
                self.add(item, "Item", source, label)

    def link(self, escaped, source):
        """Link only text tokens, never attributes or existing anchors; retain bytes/spaces."""
        out, protected, seen = [], [], set()
        # Callers supply escaped text or renderer-created markup, never raw source HTML.
        tokens = re.split(r"(<[^>]*>)", escaped)
        for token in tokens:
            if token.startswith("<"):
                tag = re.match(r"</?([\w-]+)", token)
                if tag and tag.group(1).lower() in {"a", "script", "style", "code"}:
                    if token.startswith("</"):
                        if protected:
                            protected.pop()
                    elif not token.endswith("/>"):
                        protected.append(tag.group(1).lower())
                out.append(token)
                continue
            if protected:
                out.append(token)
                continue

            def sub(match):
                key = self.folded_aliases[match.group().casefold()]
                if key in seen:
                    return match.group()
                seen.add(key)
                self.references[key].add(source)
                return f'<a class="x" href="#{self.entries[key]["id"]}">{match.group()}</a>'

            # Whole-cell exact matching also permits common single-word item/tech names.
            key = entity_key(html.unescape(token))
            if len(tokens) == 1 and key in self.entries:
                self.references[key].add(source)
                out.append(f'<a class="x" href="#{self.entries[key]["id"]}">{token}</a>')
            else:
                out.append(self.pattern.sub(sub, token) if self.pattern else token)
        return "".join(out)

    def index_html(self):
        rows = []
        for key, entry in sorted(self.entries.items()):
            roles = []
            for kind, definitions in sorted(entry["kinds"].items()):
                links = [f'<a href="#{target}">{esc(label)}</a>' for target, label in sorted(definitions)]
                count = len(links) - 2
                noun = 'fonte' if count == 1 else 'fontes'
                extra = (f'<details class="backlinks"><summary>Mais {count} {noun} de {esc(kind)}</summary>'
                         '<p>' + ' · '.join(links[2:]) + '</p></details>') if len(links) > 2 else ''
                roles.append(f'<p><b>{esc(kind)}</b>: {" · ".join(links[:2])}</p>' + extra)
            refs = sorted(self.references[key], key=lambda r: (self.labels[r], r))
            back = " · ".join(f'<a href="#{r}">{esc(self.labels[r])}</a>' for r in refs)
            badges = ''.join(f'<span>{esc(k)}</span>' for k in sorted(entry['kinds']))
            rows.append(f'<article class="entity-entry" data-roles="{esc("|".join(entry["kinds"]))}" '
                        f'id="{entry["id"]}"><h3>{esc(entry["name"])}</h3><div class="entity-roles">{badges}</div>'
                        + "".join(roles) + (f'<details class="backlinks"><summary>Referências: {len(refs)}</summary><p>{back}</p></details>' if back else "") + '</article>')
        print(f'entity index: {len(rows)} names; '
              f'{sum(len(e["kinds"]) > 1 for e in self.entries.values())} multi-role names; '
              f'{sum(map(len, self.references.values()))} source references', file=sys.stderr)
        kinds = sorted({k for entry in self.entries.values() for k in entry['kinds']})
        filters = ('<div class="entity-tools nosearch"><label>Nome ou fonte '
                   '<input id="entity-query" type="search" placeholder="Ex.: Renamon, Power Charge…"></label>'
                   '<label>Tipo <select id="entity-kind"><option value="">Todos os tipos</option>'
                   + ''.join(f'<option>{esc(k)}</option>' for k in kinds) + '</select></label>'
                   '<button id="entity-clear" type="button">Limpar filtros</button>'
                   f'<p id="entity-count" role="status">{len(rows)} entidades</p></div>')
        return (f'<section class="lvl1" data-leaf="1" id="{self.index_id}" data-title="Índice de entidades">'
                '<h2>Índice de entidades</h2><p>Nomes iguais preservam todos os papéis e fontes. '
                'Variantes, cores e números não são inferidos. As referências levam aos trechos do guia; '
                'recompensas mantêm os valores da fonte.</p>' + filters + '<div class="entity-grid">'
                + "".join(rows) + '</div></section>')


class LinkIntegrity(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids, self.hrefs, self.duplicates = set(), [], []
        self.anchors = 0
        self.nested = False

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if "id" in attrs:
            if attrs["id"] in self.ids:
                self.duplicates.append(attrs["id"])
            self.ids.add(attrs["id"])
        href = attrs.get("href", "")
        if href.startswith("#"):
            self.hrefs.append(href[1:])
        if tag == "a":
            self.nested |= self.anchors > 0
            self.anchors += 1

    def handle_endtag(self, tag):
        if tag == "a":
            self.anchors = max(0, self.anchors - 1)


def validate_links(document):
    check = LinkIntegrity()
    check.feed(document)
    missing = set(check.hrefs) - check.ids
    if check.duplicates or missing or check.nested:
        raise ValueError(f'Invalid internal links: duplicates={check.duplicates}, '
                         f'missing={sorted(missing)}, nested anchors={check.nested}')
    print(f'link integrity: {len(check.ids)} unique IDs; {len(check.hrefs)} internal hrefs; '
          'no missing targets or nested anchors', file=sys.stderr)
    return check



# Translation units are exact UTF-8 text after rendering/reflow, never fuzzy matches.
def translation_key(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def ascii_prose_units(text):
    """Editorial prose embedded in an ASCII fallback; leave data/layout untouched."""
    for paragraph in text.split("\n\n"):
        first = paragraph.splitlines()[0] if paragraph else ""
        prose = (not first.startswith(" ")
                 and not re.match(r"^(Card Battles|Digibattles|Digimobattles|Digimon Battles|Enemies|Items|Inn|\?\?\?|EAST SECTOR)", first)
                 and len(paragraph.split()) >= 5)
        if prose or re.match(r'^   "', first) or ' Pack - "' in first:
            yield " ".join(paragraph.split())


class WalkthroughTranslation:
    LABELS = {"Items": "Itens", "Enemies": "Inimigos",
              "Digimon Battles": "Batalhas de Digimon", "Digimon Battle": "Batalha de Digimon",
              "Boss Battle": "Batalha de chefe", "Card Battle": "Batalha de cartas",
              "Location": "Local", "Drop": "Item deixado"}

    def __init__(self, path=None):
        path = path or Path(__file__).with_name("guide_walkthrough_ptbr.json")
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
        if data.get("format_version") != 1 or data.get("language") != "pt-BR":
            raise ValueError("Unsupported walkthrough translation catalog")
        self.entries = data["translations"]
        if not all(re.fullmatch(r"[0-9a-f]{64}", k) and isinstance(v, str) and v.strip()
                   for k, v in self.entries.items()):
            raise ValueError("Invalid walkthrough translation entry")
        self.seen, self.missing = set(), set()
        self.occurrences = 0

    def text(self, original):
        key = translation_key(original)
        self.seen.add(key)
        self.occurrences += 1
        if key not in self.entries:
            self.missing.add(key)
            return original
        return self.entries[key]

    def heading(self, original):
        text = self.text(original)
        return text + " [sem tradução · inglês]" if translation_key(original) in self.missing else text

    def paragraph(self, original, link):
        translated = self.text(original)
        missing = translation_key(original) in self.missing
        label = '<small class="translation-missing">Sem tradução · original em inglês</small>' if missing else ""
        return label + f'<p lang="{"en" if missing else "pt-BR"}">{link(esc(translated))}</p>'

    def fragment(self, body, original, link):
        # Rebuild only prose paragraphs. Tables, values, names and ASCII are never parsed as Portuguese.
        def paragraph(m):
            text = html.unescape(re.sub(r"<[^>]*>", "", m.group(1)))
            return self.paragraph(text, link)
        body = re.sub(r"<p>(.*?)</p>", paragraph, body, flags=re.S)

        def ascii_block(m):
            text = html.unescape(re.sub(r"<[^>]*>", "", m.group(1)))
            units = list(ascii_prose_units(text))
            if not units:
                return m.group(0)
            translated = "".join(self.paragraph(t, link) for t in units)
            return translated + '<details class="orig" lang="en"><summary>ver ASCII original</summary>' + m.group(0) + '</details>'
        body = re.sub(r"<pre>(.*?)</pre>", ascii_block, body, flags=re.S)
        for tag in ("h4", "th"):
            body = re.sub(rf"(<{tag}(?: [^>]*)?>)([^<]*)(</{tag}>)",
                          lambda m: m[1] + esc(self.LABELS.get(html.unescape(m[2]), html.unescape(m[2]))) + m[3], body)
        body = re.sub(r'(<span class="bk">.*?</svg>)([^<]*)(</span>)',
                      lambda m: m[1] + esc(self.LABELS.get(m[2], m[2])) + m[3], body)
        return '<div class="walk-translation" lang="pt-BR">' + body + '</div>' + orig_html(original).replace('<details ', '<details lang="en" ', 1)

    def report(self):
        return {"units": len(self.seen), "translated": len(self.seen - self.missing),
                "missing": len(self.missing), "occurrences": self.occurrences,
                "unused_catalog_keys": len(self.entries.keys() - self.seen)}

def render(header, sections, translation=None, *, site=False):
    entities = EntityIndex(sections)
    translation = translation or WalkthroughTranslation()
    original_sections = sections
    sections = copy.deepcopy(sections)
    for section in sections:
        if section["id"] != "walkthrough":
            continue
        section["title"] = "Passo a passo"
        for chapter in section["chapters"]:
            chapter["original_title"] = chapter["title"]
            chapter["title"] = translation.heading(chapter["title"])
            chapter["items"] = [(it[0], it[1], translation.heading(it[2])) if it[0] == "h" else it
                                for it in chapter["items"]]

    def make_link(source):
        return lambda escaped: entities.link(escaped, source)

    nav = nav_html(sections)
    nav.append(f'<a class="top" href="#{entities.index_id}">Índice de entidades</a>')

    flat = []  # ordered list of (id, title) for prev/next
    for s in sections:
        if s["chapters"]:
            flat += [(c["id"], c["title"]) for c in s["chapters"]]
        else:
            flat.append((s["id"], s["title"]))
    pos = {k: i for i, (k, _) in enumerate(flat)}

    def pn(key):
        i = pos[key]
        p = f'<a class="prev" href="#{flat[i-1][0]}">{svg("prev", "ic sm")} {esc(flat[i-1][1])}</a>' if i else "<span></span>"
        n = f'<a class="next" href="#{flat[i+1][0]}">{esc(flat[i+1][1])} {svg("next", "ic sm")}</a>' if i + 1 < len(flat) else "<span></span>"
        return f'<nav class="pn" aria-label="Capítulos">{p}<a class="to-top" href="#inicio">Topo</a>{n}</nav>'

    def items_html(items, self_id, sec_id):
        if sec_id == "walkthrough":
            original_section = next(s for s in original_sections if s["id"] == sec_id)
            owner = next((c for c in original_section["chapters"] if c["id"] == self_id), original_section)
            items = owner.get("items", owner.get("intro", []))
        out = []
        in_area = False
        link = make_link(self_id)
        cx = {"cid": self_id, "link": link, "entities": entities}
        head = ""
        walk = sec_id == "walkthrough"
        for i, it in enumerate(items):
            if it[0] == "pre":
                source = entities.blocks[(self_id, i)]
                link = make_link(source)
                cx["link"] = link
                cx["source"] = source
                body = special_html(sec_id, head, it[1], cx) or blocks_html(it[1], link)
                if walk:
                    body = translation.fragment(body, it[1], link)
                if self_id != sec_id:
                    body = body.replace('<h4', '<h5').replace('</h4>', '</h5>')
                out.append(f'<div class="source-block" id="{source}">{body}</div>')
            else:
                if in_area:
                    out.append('</section>')
                in_area = True
                head = it[2]
                pin = svg("pin", "ic sm") if walk else ""
                original_title = f' title="Original: {esc(it[2])}"' if walk else ""
                tag = 'h4' if self_id != sec_id else 'h3'
                out.append(f'<section class="guide-area" aria-labelledby="{it[1]}">'
                           f'<{tag} id="{it[1]}" class="subheading{" loc" if walk else ""}"{original_title}>'
                           f'<a href="#{it[1]}">{pin}{esc(translation.entries.get(translation_key(it[2]), it[2] + " [sem tradução · inglês]") if walk else it[2])}</a></{tag}>')
        if in_area:
            out.append('</section>')
        return "\n".join(out)

    main, pages = [], []
    for s in sections:
        leaf = "" if s["chapters"] else ' data-leaf="1"'
        parts = [f'<section class="lvl1" id="{s["id"]}" data-title="{esc(s["title"])}"{leaf}>',
                 f'<h2>{esc(s["title"])}</h2>']
        card = " card" if s["id"] in CARD_SECS else ""
        if s["id"] == "walkthrough":
            parts.append('<p class="translation-status" lang="pt-BR">Português brasileiro · '
                         '%%WALK_COVERAGE%% unidades editoriais disponíveis · nomes oficiais e dados preservados. O original em inglês acompanha cada bloco; '
                         'lacunas são indicadas em inglês.</p>')
        parts.append(f'<div class="intro{card}">{items_html(s["intro"], s["id"], s["id"])}</div>')
        if not s["chapters"]:
            parts.append(pn(s["id"]))
        if s["chapters"]:
            idx = "".join(f'<li><a href="#{c["id"]}">{esc(c["title"])}</a></li>' for c in s["chapters"])
            parts.append(f'<ul class="idx">{idx}</ul>')
        overview = list(parts)
        if s['chapters']:
            overview = parts[:2] + [parts[-1]] + parts[2:-1]
            overview = [('<details class="overview-notes"><summary>Notas de leitura e orientação</summary>'
                         + part + '</details>') if part.startswith('<div class="intro') else part
                        for part in overview]
        pages.append(dict(id=s['id'], title=s['title'], kind='section', parent=None,
                          content='\n'.join(overview) + '</section>'))
        for c in s["chapters"]:
            kind = "walk" if s["id"] == "walkthrough" else "data" if s["id"] in DATA_SECS else "plain"
            article = (
                f'<article class="lvl2 {kind}" id="{c["id"]}" data-title="{esc(c["title"])}" data-parent="{s["id"]}">'
                f'{banner_html(c, s)}\n'
                + (f'<details class="orig" lang="en"><summary>Título original em inglês</summary><p>{esc(c["original_title"])}</p></details>' if "original_title" in c else "") +
                f"{items_html(c['items'], c['id'], s['id'])}\n{pn(c['id'])}</article>"
            )
            parts.append(article)
            wrapper = (f'<section class="lvl1" id="{s["id"]}" data-title="{esc(s["title"])}">'
                       f'<p class="page-group">{esc(s["title"])}</p>')
            pages.append(dict(id=c['id'], title=c['title'], kind='chapter', parent=s['id'],
                              content=wrapper + article + '</section>'))
        parts.append("</section>")
        main.append("\n".join(parts))

    entity_html = entities.index_html()
    main.append(entity_html)
    pages.append(dict(id=entities.index_id, title='Índice de entidades', kind='entities',
                      parent=None, content=entity_html))
    pages.insert(0, dict(id='inicio', title='Digimon World 3', kind='home', parent=None,
                         content=cover_html(header, sections)))
    scopes = ''.join(f'<option value="{s["id"]}">{esc(s["title"])}</option>' for s in sections)
    tpl = re.sub(r"%%ICON:(\w+)%%", lambda m: svg(m.group(1)), TEMPLATE)
    document = (tpl.replace("%%COVER%%", pages[0]['content'])
               .replace("%%NAV%%", "\n".join(nav))
               .replace("%%MAIN%%", "\n".join(main))
               .replace("%%SCOPES%%", scopes)
               .replace("%%PAL%%", palette_json(sections, entities))
               .replace("%%CSS%%", Path(__file__).with_name("guide_assets").joinpath("guide.css").read_text(encoding="utf-8"))
               .replace("%%JS%%", Path(__file__).with_name("guide_assets").joinpath("guide.js").read_text(encoding="utf-8"))
               .replace("%%WALK_COVERAGE%%", f"{len(translation.seen - translation.missing)}/{len(translation.seen)}"))
    if site:
        order = [c['id'] for s in browse_sections(sections)
                 for c in (s['chapters'] or [s])] + [entities.index_id]
        coverage = f'{len(translation.seen - translation.missing)}/{len(translation.seen)}'
        for page in pages:
            page['content'] = page['content'].replace('%%WALK_COVERAGE%%', coverage)
        return site_module().build_site(document, pages, order)
    return document


TEMPLATE = r"""<!DOCTYPE html>
<html lang="pt-BR" data-theme="dark">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="theme-color" content="#0b1020">
<meta name="color-scheme" content="dark light">
<title>Digimon World 3 — Guide and Walkthrough (EmeraldPhoenix)</title>
<style>%%CSS%%</style>
<noscript><style>
#bar,#dock,#top,.entity-tools,.search-tools{display:none!important}
#side{position:static;transform:none;width:auto;max-height:none;border:0}
#tree{max-height:none}main{margin:0;padding-top:16px}
</style></noscript>
</head>
<body>
<a class="skip" href="#main">Pular para o conteúdo</a>
<header id="bar">
 <button id="menu" class="ib icon" aria-label="Abrir índice" aria-controls="side" aria-expanded="false" title="Índice">%%ICON:menu%%</button>
 <a class="brand" href="#inicio" aria-label="Digimon World 3 — início">%%ICON:book%%<span>Digimon World 3</span></a>
 <nav class="crumb" id="crumb" aria-label="Você está em"><span>Início</span></nav>
 <form class="sf" role="search" id="sform"><input id="q" type="search" placeholder="Buscar no guia" aria-label="Buscar no guia" autocomplete="off"><kbd title="Atalho: /">/</kbd></form>
 <span id="fs"><button id="fsm" class="ib icon" aria-label="Diminuir fonte" title="Diminuir fonte">A−</button><button id="fsp" class="ib icon" aria-label="Aumentar fonte" title="Aumentar fonte">A+</button></span>
 <button id="pbtn" class="ib" aria-label="Ir para… (Ctrl+K)" title="Ir para… (Ctrl+K)" aria-haspopup="dialog">%%ICON:cmd%%<span>Ir para…</span></button>
 <button id="theme" class="ib icon" aria-label="Alternar tema claro/escuro" title="Alternar tema">%%ICON:sun%%</button>
 <div id="prog" aria-hidden="true"></div>
</header>
<nav id="side" aria-label="Índice do guia">
 <div class="side-label">EXPLORAR O GUIA <span>DW3 / 2003</span></div>
 <div class="search-tools" hidden>
  <label for="search-scope">Buscar em</label>
  <select id="search-scope"><option value="">Todo o guia</option>%%SCOPES%%</select>
  <button id="search-clear" type="button">Limpar busca</button>
 </div>
 <section id="context" class="nosearch" hidden aria-label="Neste capítulo">
  <p class="side-label">NESTE CAPÍTULO</p>
  <p id="reading-status"></p><progress id="chapter-progress" max="100" value="0" aria-label="Progresso de leitura do capítulo"></progress>
  <nav id="area-nav" aria-label="Áreas deste capítulo"></nav>
 </section>
 <div id="tree">
%%NAV%%
 </div>
 <p id="search-status" class="sr" role="status"></p>
 <div id="results" hidden></div>
</nav>
<main id="main" tabindex="-1">
<noscript><p>JavaScript está desativado. O texto e os links continuam disponíveis; busca, filtros e progresso precisam de JavaScript.</p></noscript>
%%COVER%%
%%MAIN%%
</main>
<nav id="dock" aria-label="Navegação rápida">
 <a id="dprev" class="dk" href="#inicio" aria-label="Capítulo anterior">%%ICON:prev%%</a>
 <button id="dmenu" class="dk" aria-label="Abrir índice">%%ICON:menu%%</button>
 <button id="dsearch" class="dk" aria-label="Buscar no guia">%%ICON:search%%</button>
 <a id="dnext" class="dk" href="#inicio" aria-label="Próximo capítulo">%%ICON:next%%</a>
</nav>
<button id="top" aria-label="Voltar ao topo" title="Voltar ao topo">%%ICON:up%%</button>
<div id="pal" class="modal" hidden>
 <div class="mbox" role="dialog" aria-modal="true" aria-label="Ir para">
  <button type="button" class="modal-close" aria-label="Fechar navegação">Fechar ×</button>
  <input id="pq" type="text" role="combobox" aria-autocomplete="list" aria-expanded="true" placeholder="Ir para capítulo, seção, local, boss ou Digimon…" aria-label="Ir para" aria-controls="pl" autocomplete="off">
  <ul id="pl" role="listbox" aria-label="Resultados"></ul>
  <p class="mfoot"><kbd>↑</kbd> <kbd>↓</kbd> navegar · <kbd>Enter</kbd> ir · <kbd>Esc</kbd> fechar</p>
 </div>
</div>
<div id="kbd" class="modal" hidden>
 <div class="mbox sm" role="dialog" aria-modal="true" aria-label="Atalhos de teclado">
  <button type="button" class="modal-close" aria-label="Fechar atalhos">Fechar ×</button>
  <h2>Atalhos</h2>
  <dl class="keys">
   <dt><kbd>/</kbd></dt><dd>buscar no guia</dd>
   <dt><kbd>Ctrl</kbd> + <kbd>K</kbd></dt><dd>ir para…</dd>
   <dt><kbd>j</kbd> / <kbd>k</kbd></dt><dd>próximo / capítulo anterior</dd>
   <dt><kbd>?</kbd></dt><dd>mostrar esta ajuda</dd>
   <dt><kbd>Esc</kbd></dt><dd>fechar janelas e limpar a busca</dd>
  </dl>
 </div>
</div>
<script type="application/json" id="pal-data">%%PAL%%</script>
<script>%%JS%%</script>
</body>
</html>
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', help='Locally saved GameFAQs HTML')
    parser.add_argument('output', help='HTML file, or package directory for --format site')
    parser.add_argument('--format', choices=['single', 'site'], default='single',
                        help='site: recommended chapter package; single: compatible offline file (default)')
    args = parser.parse_args()
    with open(args.source, encoding="utf-8") as f:
        lines = extract_text(f.read())
    header, sections = parse(lines)
    translation = WalkthroughTranslation()
    rendered = render(header, sections, translation, site=args.format == 'site')
    document = rendered if isinstance(rendered, str) else ''.join(
        value for name, value in rendered.items() if name.endswith('.html'))
    print("walkthrough translation: " + json.dumps(translation.report(), sort_keys=True))
    if args.format == 'single':
        validate_links(document)
    # The entire parsed English source run must survive, including tables/ASCII.
    walk = next(s for s in sections if s["id"] == "walkthrough")
    original_runs = [it[1] for owner in [walk] + walk["chapters"]
                     for it in owner.get("items", owner.get("intro", [])) if it[0] == "pre"]
    preserved = [html.unescape(re.sub(r"<[^>]*>", "", t))
                 for t in re.findall(r"<pre>(.*?)</pre>", document, re.S)]
    from collections import Counter
    if Counter(original_runs) - Counter(preserved):
        raise ValueError("Walkthrough English originals were not preserved")
    print(f"walkthrough original runs preserved: {len(original_runs)}/{len(original_runs)}")
    if args.format == 'site':
        site_module().write_site(Path(args.output), rendered)
    else:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(document)
    for s in sections:
        print(f"{s['title']}: {len(s['chapters'])} chapters")
    print(f"fallback blocks: {STATS['fallback']}", file=sys.stderr)
    tot, bad = STATS.get("special", 0), STATS.get("special_fail", 0)
    print(f"evolution/partner blocks structured: {tot - bad}/{tot}", file=sys.stderr)
    print(f"card-battle boosters structured: {STATS.get('booster', 0) - STATS.get('booster_fail', 0)}/{STATS.get('booster', 0)}", file=sys.stderr)
    print(f"enemy list structured: {'no' if STATS.get('enemy_fail') else 'yes'}", file=sys.stderr)


if __name__ == "__main__":
    main()
