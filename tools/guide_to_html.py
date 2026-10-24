#!/usr/bin/env python3
"""Convert the saved GameFAQs page of the Digimon World 3 guide (EmeraldPhoenix)
into a single navigable HTML file (sidebar, scroll-spy, full-text search).

Usage: python3 -I tools/guide_to_html.py <saved-gamefaqs-page.txt> <out.html>

The guide body is plain text inside <pre id="faqspan-N"> blocks; headings are
ASCII banners (`* * *` boxes), dash boxes and dash underlines.
"""
import html
import json
import re
import sys

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
    return html.escape(s, quote=False)


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
    th = "".join(f'<th{" class=num" if i in num_cols else ""}>{h}</th>' for i, h in enumerate(head))
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
        f'<span class="pill ghost">{esc(mt.group(3))}</span>'
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
                 f'<span class="n">{esc(d.get("Bit", ""))}</span>', esc(d.get("Loc", "")), fmt_item(d.get("Item", ""), link)]
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
    "walkthrough": "Passo a passo do início ao fim, por capítulo e local.",
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


def cover_html(header, sections):
    ver = re.search(r"Version (\S+)", header)
    date = re.search(r"\d\d/\d\d/\d\d", header)
    credit = f"by Emerald Phoenix · v{ver.group(1) if ver else '?'} · {date.group(0) if date else ''}"
    tiles = "".join(
        f'<a class="tile" href="#{s["id"]}"><span class="ti">{svg(SEC_ICON.get(s["id"], "list"))}</span>'
        + (f'<span class="tc">{esc(count_label(s))}</span>' if count_label(s) else "")
        + f'<span class="tt">{esc(s["title"])}</span><span class="td">{esc(SEC_DESC.get(s["id"], ""))}</span></a>'
        for s in sections
    )
    return (
        '<header class="hero" id="inicio">'
        '<p class="eyebrow">Guia do jogo</p>'
        '<h1>Digimon World 3</h1>'
        '<p class="sub">Guide &amp; Walkthrough · PlayStation</p>'
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
    for s in sections:
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


def palette_json(sections, names):
    """Items for the Ctrl+K palette: sections, chapters, L3 headings and Digimon names."""
    items = []
    for s in sections:
        items.append({"t": s["title"], "g": "Seção", "id": s["id"], "k": "sec"})
        for c in s["chapters"]:
            items.append({"t": c["title"], "g": s["title"], "id": c["id"], "k": "cap"})
            items += [{"t": it[2], "g": c["title"], "id": it[1], "k": "loc"} for it in c["items"] if it[0] == "h"]
        items += [{"t": it[2], "g": s["title"], "id": it[1], "k": "loc"} for it in s["intro"] if it[0] == "h"]
    items += [{"t": n, "g": "Digimon", "id": cid, "k": "dig"} for n, cid in names.items()]
    return json.dumps(items, ensure_ascii=False).replace("</", "<\\/")


CARD_SECS = {"disclaimer", "introduction", "things-left-undone", "credits"}
DATA_SECS = {"digimon-evolutions", "partner-digimon"}


def banner_html(c, s):
    """Chapter banner: faded numeral, title, and chips linking to the L3 headings of the chapter."""
    seen = {}
    for it in c["items"]:
        if it[0] == "h":
            seen.setdefault(it[2], it[1])
    m = re.match(r"^(\d+)\.", c["title"])
    big = m.group(1).zfill(2) if m else c["title"][:1].upper()
    chips = "".join(f'<li><a href="#{v}">{svg("pin", "ic sm")}{esc(k)}</a></li>' for k, v in seen.items())
    chips = f'<ul class="chips locs" aria-label="Locais deste capítulo">{chips}</ul>' if chips else ""
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
    n = name.strip()
    if n in cx["names"] and cx["names"][n] != cx["cid"]:
        return f'<a class="x" href="#{cx["names"][n]}">{esc(n)}</a>'
    return esc(n)


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


def chapter_techs(c):
    """Technique names of a chapter (TECHNIQUES block), so level-chart names can link to their cards."""
    items = c["items"]
    for i, it in enumerate(items):
        if it[0] == "h" and it[2].upper() == "TECHNIQUES" and i + 1 < len(items) and items[i + 1][0] == "pre":
            return set(n.strip() for n in re.findall(r"^   ([^\s:][^:]*?):?\s*$", items[i + 1][1], re.M))
    return set()


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
        nm = f'<a href="#{cx["cid"]}--tech-{slug(name)}">{esc(name)}</a>' if name in cx["techs"] else esc(name)
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
        f'<article class="tcard" id="{cx["cid"]}--tech-{slug(t["name"])}">'
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
            esc(d.get("Loc", "")),
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


def render(header, sections):
    # Digimon names -> chapter ids (partners + evolutions), for cross links
    names = {}
    for s in sections:
        if s["title"] in ("Partner Digimon", "Digimon Evolutions"):
            for c in s["chapters"]:
                names[re.sub(r"^\d+\.\s*", "", c["title"])] = c["id"]
    name_re = re.compile(r"(?<![A-Za-z])(" + "|".join(sorted(map(re.escape, names), key=len, reverse=True)) + r")(?![A-Za-z])")

    def make_link(self_id):
        def link(escaped):
            seen = set()

            def sub(m):
                n = m.group(1)
                if n in seen or names[n] == self_id:
                    return n
                seen.add(n)
                return f'<a class="x" href="#{names[n]}">{n}</a>'

            return name_re.sub(sub, escaped)

        return link

    nav = nav_html(sections)

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
        return f'<nav class="pn" aria-label="Capítulos">{p}{n}</nav>'

    def items_html(items, self_id, sec_id, techs=frozenset()):
        out = []
        link = make_link(self_id)
        cx = {"names": names, "cid": self_id, "techs": techs, "link": link}
        head = ""
        walk = sec_id == "walkthrough"
        for it in items:
            if it[0] == "pre":
                out.append(special_html(sec_id, head, it[1], cx) or blocks_html(it[1], link))
            else:
                head = it[2]
                pin = svg("pin", "ic sm") if walk else ""
                cls = ' class="loc"' if walk else ""
                out.append(f'<h3 id="{it[1]}"{cls}><a href="#{it[1]}">{pin}{esc(it[2])}</a></h3>')
        return "\n".join(out)

    main = []
    for s in sections:
        leaf = "" if s["chapters"] else ' data-leaf="1"'
        parts = [f'<section class="lvl1" id="{s["id"]}" data-title="{esc(s["title"])}"{leaf}>',
                 f'<h2>{esc(s["title"])}</h2>']
        card = " card" if s["id"] in CARD_SECS else ""
        parts.append(f'<div class="intro{card}">{items_html(s["intro"], s["id"], s["id"])}</div>')
        if not s["chapters"]:
            parts.append(pn(s["id"]))
        if s["chapters"]:
            idx = "".join(f'<li><a href="#{c["id"]}">{esc(c["title"])}</a></li>' for c in s["chapters"])
            parts.append(f'<ul class="idx">{idx}</ul>')
        for c in s["chapters"]:
            kind = "walk" if s["id"] == "walkthrough" else "data" if s["id"] in DATA_SECS else "plain"
            parts.append(
                f'<article class="lvl2 {kind}" id="{c["id"]}" data-title="{esc(c["title"])}" data-parent="{s["id"]}">'
                f'{banner_html(c, s)}\n'
                f"{items_html(c['items'], c['id'], s['id'], chapter_techs(c))}\n{pn(c['id'])}</article>"
            )
        parts.append("</section>")
        main.append("\n".join(parts))

    tpl = re.sub(r"%%ICON:(\w+)%%", lambda m: svg(m.group(1)), TEMPLATE)
    return (tpl.replace("%%COVER%%", cover_html(header, sections))
               .replace("%%NAV%%", "\n".join(nav))
               .replace("%%MAIN%%", "\n".join(main))
               .replace("%%PAL%%", palette_json(sections, names)))


TEMPLATE = r"""<!DOCTYPE html>
<html lang="pt-BR" data-theme="dark">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="theme-color" content="#0b1020">
<meta name="color-scheme" content="dark light">
<title>Digimon World 3 — Guide and Walkthrough (EmeraldPhoenix)</title>
<style>
:root{color-scheme:dark;
 --bg:#0b1020;--pn:#121a30;--pn2:#172040;--bg2:#0e1528;--fg:#e2e8f0;--mut:#94a3b8;--bd:#22305a;
 --ac:#22d3ee;--ac2:#fb923c;--boss:#fb7185;--ok:#4ade80;--mark:rgba(250,204,21,.38);--hov:rgba(34,211,238,.08);
 --glow:0 0 0 1px rgba(34,211,238,.16),0 0 24px rgba(34,211,238,.07);
 --fire:#ef4444;--water:#3b82f6;--ice:#67e8f9;--wind:#4ade80;--thunder:#facc15;--machine:#94a3b8;--dark:#a855f7;--heal:#f472b6;--neutral:#a1a1aa;
 --sans:system-ui,-apple-system,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif;
 --mono:ui-monospace,"SFMono-Regular",Menlo,Consolas,"Liberation Mono",monospace;
 --bh:56px;--side:300px;--fs:16px}
:root[data-theme=light]{color-scheme:light;
 --bg:#f6f2e8;--pn:#fffdf7;--pn2:#f3eedf;--bg2:#ece5d3;--fg:#1c1917;--mut:#57534e;--bd:#d8cfbb;
 --ac:#0e7490;--ac2:#c2410c;--boss:#be123c;--ok:#15803d;--mark:#fde68a;--hov:rgba(14,116,144,.08);
 --glow:0 1px 2px rgba(60,40,10,.08),0 8px 24px rgba(60,40,10,.06);
 --fire:#b91c1c;--water:#1d4ed8;--ice:#0e7490;--wind:#15803d;--thunder:#a16207;--machine:#475569;--dark:#7e22ce;--heal:#be185d;--neutral:#52525b}

*{box-sizing:border-box;scrollbar-width:thin;scrollbar-color:var(--bd) transparent}
::-webkit-scrollbar{width:10px;height:10px}::-webkit-scrollbar-thumb{background:var(--bd);border-radius:10px;border:3px solid transparent;background-clip:content-box}
::selection{background:color-mix(in srgb,var(--ac) 32%,transparent)}
:focus-visible{outline:2px solid var(--ac);outline-offset:2px;border-radius:4px}
[hidden]{display:none!important}
html{scroll-behavior:smooth;scroll-padding-top:calc(var(--bh) + 14px)}
body{margin:0;min-height:100vh;background:var(--bg);color:var(--fg);font-family:var(--sans);font-size:var(--fs);line-height:1.6;
 background-image:radial-gradient(1100px 520px at 78% -12%,color-mix(in srgb,var(--ac) 10%,transparent),transparent 62%),radial-gradient(760px 480px at -8% 14%,color-mix(in srgb,var(--ac2) 7%,transparent),transparent 62%);background-attachment:fixed}
a{color:var(--ac);text-decoration:none}a:hover{text-decoration:underline}
kbd{font:12px var(--mono);color:var(--mut);background:var(--bg2);border:1px solid var(--bd);border-bottom-width:2px;border-radius:5px;padding:0 5px;line-height:18px}
.ic{width:20px;height:20px;flex:none}.ic.sm{width:14px;height:14px;vertical-align:-2px}
.sr{position:absolute;width:1px;height:1px;overflow:hidden;clip:rect(0 0 0 0);white-space:nowrap}
.skip{position:absolute;left:12px;top:-60px;z-index:50;background:var(--ac);color:#06121a;padding:8px 12px;border-radius:8px;font-weight:600}
.skip:focus{top:10px}

/* top bar */
#bar{position:fixed;inset:0 0 auto 0;height:var(--bh);display:flex;align-items:center;gap:10px;padding:0 14px;z-index:30;
 background:color-mix(in srgb,var(--bg) 80%,transparent);backdrop-filter:blur(14px) saturate(1.3);-webkit-backdrop-filter:blur(14px) saturate(1.3);border-bottom:1px solid var(--bd)}
#prog{position:absolute;left:0;bottom:-1px;height:2px;width:0;background:linear-gradient(90deg,var(--ac),var(--ac2));box-shadow:0 0 8px var(--ac);transition:width .1s linear}
.brand{display:flex;align-items:center;gap:8px;font-weight:700;color:var(--fg);white-space:nowrap;letter-spacing:.01em}
.brand .ic{color:var(--ac)}.brand:hover{text-decoration:none}
.crumb{flex:1;min-width:0;display:flex;align-items:center;gap:6px;font-size:14px;color:var(--mut);white-space:nowrap;overflow:hidden}
.crumb span,.crumb b{overflow:hidden;text-overflow:ellipsis}.crumb b{color:var(--fg);font-weight:600}.crumb i{font-style:normal;opacity:.6}
.sf{position:relative;width:min(280px,26vw);margin:0}
.sf input{width:100%;font:inherit;font-size:14px;color:var(--fg);background:var(--pn);border:1px solid var(--bd);border-radius:9px;padding:6px 44px 6px 12px;height:34px}
.sf input:focus{outline:none;border-color:var(--ac);box-shadow:0 0 0 3px color-mix(in srgb,var(--ac) 22%,transparent)}
.sf kbd{position:absolute;right:8px;top:50%;transform:translateY(-50%)}
.ib{display:inline-flex;align-items:center;gap:6px;height:34px;padding:0 10px;border:1px solid var(--bd);border-radius:9px;background:var(--pn);color:var(--fg);font:inherit;font-size:14px;font-weight:600;cursor:pointer;white-space:nowrap;transition:border-color .15s,background .15s,color .15s}
.ib:hover{border-color:var(--ac);background:var(--pn2);color:var(--ac)}
.ib.icon{width:34px;padding:0;justify-content:center}
.ib .ic{width:18px;height:18px}
#bar .kbd-hint{font-size:12px}

/* sidebar */
#side{position:fixed;top:var(--bh);bottom:0;left:0;width:var(--side);overflow:auto;padding:12px 10px 40px;font-size:14px;z-index:20;
 background:color-mix(in srgb,var(--pn) 94%,transparent);border-right:1px solid var(--bd)}
#tree details.grp{margin:3px 0}
details.grp>summary{position:relative;display:flex;align-items:center;gap:9px;padding:7px 8px 8px;border-radius:9px;cursor:pointer;list-style:none}
details.grp>summary::-webkit-details-marker{display:none}
details.grp>summary:hover{background:var(--hov)}
details.grp>summary>.ic{color:var(--ac)}
details.grp>summary>a{flex:1;min-width:0;color:var(--fg);font-weight:600;overflow:hidden;text-overflow:ellipsis}
details.grp>summary>a:hover{text-decoration:none;color:var(--ac)}
details.grp>summary .n{font:12px var(--mono);color:var(--mut)}
details.grp>summary::after{content:"";width:6px;height:6px;border-right:2px solid var(--mut);border-bottom:2px solid var(--mut);transform:rotate(-45deg);margin:0 2px 0 2px;transition:transform .15s;flex:none}
details.grp[open]>summary::after{transform:rotate(45deg)}
.gp{position:absolute;left:8px;right:8px;bottom:2px;height:2px;border-radius:2px;background:var(--bd);overflow:hidden}
.gp::after{content:"";position:absolute;inset:0;width:calc(var(--p,0)*100%);background:linear-gradient(90deg,var(--ac),var(--ac2));transition:width .3s}
#side ul{list-style:none;margin:2px 0 8px 17px;padding:0 0 0 9px;border-left:1px solid var(--bd)}
#side li a{display:block;padding:4px 8px;border-radius:7px;color:var(--mut);line-height:1.35}
#side li a:hover{color:var(--fg);background:var(--hov);text-decoration:none}
#side li a.active{color:var(--ac);background:var(--hov);font-weight:600;box-shadow:inset 3px 0 0 var(--ac)}
#side a.top{display:flex;align-items:center;gap:9px;padding:8px 8px;margin:3px 0;border-radius:9px;color:var(--fg);font-weight:600}
#side a.top:hover{background:var(--hov);text-decoration:none}
#side a.top.active{color:var(--ac);background:var(--hov);box-shadow:inset 3px 0 0 var(--ac)}
#side a.top>.ic{color:var(--ac)}
#results .cnt{color:var(--mut);padding:6px 8px;font-size:13px}
#results .r{display:block;padding:8px;border-radius:8px;color:var(--fg);border:1px solid transparent}
#results .r:hover{background:var(--hov);border-color:var(--bd);text-decoration:none}
#results .r b{display:block;color:var(--ac)}
#results .r .n{color:var(--mut);font-weight:400}
#results .r small{color:var(--mut);display:block;font:12px/1.5 var(--mono);white-space:pre-wrap;overflow-wrap:anywhere;margin-top:3px}

/* main */
main{margin-left:var(--side);padding:calc(var(--bh) + 28px) clamp(16px,3vw,44px) 96px;min-width:0}
main>*{max-width:1040px}
mark{background:var(--mark);color:inherit;border-radius:3px;padding:0 1px}

/* home */
.hero{position:relative;overflow:hidden;border:1px solid var(--bd);border-radius:18px;padding:44px 40px 30px;background:linear-gradient(140deg,var(--pn),var(--pn2));box-shadow:var(--glow)}
.hero::before{content:"";position:absolute;inset:0;pointer-events:none;
 background-image:linear-gradient(color-mix(in srgb,var(--ac) 13%,transparent) 1px,transparent 1px),linear-gradient(90deg,color-mix(in srgb,var(--ac) 13%,transparent) 1px,transparent 1px);
 background-size:38px 38px;-webkit-mask-image:radial-gradient(ellipse 80% 70% at 85% 0%,#000 8%,transparent 72%);mask-image:radial-gradient(ellipse 80% 70% at 85% 0%,#000 8%,transparent 72%)}
.hero::after{content:"";position:absolute;left:0;right:0;top:-160px;height:160px;pointer-events:none;background:linear-gradient(transparent,color-mix(in srgb,var(--ac) 16%,transparent) 70%,transparent);animation:scan 7s linear infinite}
@keyframes scan{to{transform:translateY(calc(100% + 320px))}}
.hero>*{position:relative}
.eyebrow{margin:0;font:600 12px var(--mono);letter-spacing:.22em;text-transform:uppercase;color:var(--ac)}
.hero h1{margin:8px 0 2px;font-size:40px;line-height:1.1;letter-spacing:-.015em;color:var(--fg);
 background:linear-gradient(90deg,var(--fg) 30%,var(--ac));-webkit-background-clip:text;background-clip:text;-webkit-text-fill-color:transparent}
.hero .sub{margin:0;font-size:18px;color:var(--fg);opacity:.9}
.hero .credit{margin:10px 0 0;font:14px var(--mono);color:var(--ac2)}
.hero .links{margin:14px 0 0;font-size:14px;color:var(--mut);display:flex;flex-wrap:wrap;gap:6px 14px;align-items:center}
.hero .links a{display:inline-flex;align-items:center;gap:5px}
.hero details.orig{margin-top:16px}
details.orig>summary{cursor:pointer;color:var(--mut);font-size:14px;width:max-content}
details.orig>pre{margin-top:10px}
.home{margin-top:26px}
.tiles{display:grid;grid-template-columns:repeat(auto-fill,minmax(232px,1fr));gap:12px}
.tile{position:relative;display:flex;flex-direction:column;gap:5px;padding:16px 16px 14px;border:1px solid var(--bd);border-radius:14px;background:var(--pn);color:var(--fg);transition:border-color .15s,transform .15s,box-shadow .15s}
.tile:hover{border-color:var(--ac);transform:translateY(-2px);box-shadow:var(--glow);text-decoration:none}
.tile .ti{width:40px;height:40px;border-radius:11px;display:grid;place-items:center;color:var(--ac);background:color-mix(in srgb,var(--ac) 13%,transparent);margin-bottom:4px}
.tile .ti .ic{width:22px;height:22px}
.tile .tt{font-weight:700;font-size:16px;line-height:1.3}
.tile .tc{position:absolute;top:14px;right:14px;font:12px var(--mono);color:var(--ac2)}
.tile .td{color:var(--mut);font-size:14px;line-height:1.45}

/* sections and chapters */
section.lvl1{margin-top:52px;scroll-margin-top:calc(var(--bh) + 14px)}
section.lvl1>h2{font-size:28px;line-height:1.2;margin:0 0 14px;padding-bottom:10px;border-bottom:1px solid var(--bd);position:relative;letter-spacing:-.01em}
section.lvl1>h2::after{content:"";position:absolute;left:0;bottom:-1px;width:72px;height:2px;background:linear-gradient(90deg,var(--ac),var(--ac2))}
article.lvl2{margin-top:40px;padding-top:4px;scroll-margin-top:calc(var(--bh) + 14px)}
h3.ch{font-size:22px;line-height:1.25;margin:0 0 10px;text-transform:none;letter-spacing:0;color:var(--fg)}
h3.ch a{color:var(--fg)}
h3{font-size:15px;margin:26px 0 6px;color:var(--ac);text-transform:uppercase;letter-spacing:.07em;scroll-margin-top:calc(var(--bh) + 14px)}
h3 a{color:inherit}
main p{max-width:72ch;line-height:1.72;margin:.7em 0}
main p strong{color:var(--ac2)}
a.x{color:inherit;border-bottom:1px dotted var(--ac)}a.x:hover{color:var(--ac);text-decoration:none}
ul.idx{columns:2;list-style:none;padding:0;margin:10px 0}ul.idx li{padding:3px 0;break-inside:avoid}
pre{margin:8px 0;padding:12px 14px;background:var(--bg2);border:1px solid var(--bd);border-radius:10px;overflow-x:auto;font:13.5px/1.45 var(--mono);tab-size:4;color:var(--fg)}
.pn{display:flex;justify-content:space-between;gap:12px;margin:18px 0 0;font-size:14px;padding-top:12px;border-top:1px solid var(--bd)}
.pn a{display:inline-flex;align-items:center;gap:6px;max-width:48%}
.pn .next{margin-left:auto;text-align:right}

/* chapter banner */
article.lvl2{position:relative}
.chb{position:relative;overflow:hidden;padding:18px 22px 16px;margin:0 0 10px;border:1px solid var(--bd);border-radius:16px;background:linear-gradient(135deg,var(--pn),var(--pn2))}
.chb::before{content:"";position:absolute;left:0;top:0;bottom:0;width:4px;background:linear-gradient(var(--ac),var(--ac2))}
.chb .num{position:absolute;right:14px;top:-18px;font:800 120px/1 var(--mono);letter-spacing:-.05em;color:var(--ac);opacity:.09;pointer-events:none;user-select:none}
.chb .kick{margin:0 0 4px;font:600 12px var(--mono);letter-spacing:.16em;text-transform:uppercase;color:var(--ac)}
.chb h3.ch{margin:0;font-size:24px}
.chb .cnt{margin:6px 0 0;font-size:13px;color:var(--mut)}
.chb .locs{margin-top:12px}
.chb .locs a{display:inline-flex;align-items:center;gap:6px;padding:3px 11px;border-radius:999px;font-size:13px;color:var(--fg);border:1px solid var(--bd);background:var(--bg2);transition:border-color .15s,color .15s}
.chb .locs a:hover{border-color:var(--ac);color:var(--ac);text-decoration:none}
.chb .locs .ic{color:var(--ac)}
.chb .locs li{background:none;border:0;padding:0}
.ic.sm{width:14px;height:14px;vertical-align:-2px}

/* location bars (walkthrough L3 headings) */
article.walk h3.loc{display:flex;align-items:center;gap:8px;margin:30px 0 8px;padding:8px 12px;border-radius:10px;border-left:3px solid var(--ac);background:linear-gradient(90deg,color-mix(in srgb,var(--ac) 13%,transparent),transparent 70%);font-size:15px;color:var(--fg)}
article.walk h3.loc a{color:inherit;display:inline-flex;align-items:center;gap:8px}
article.walk h3.loc .ic{color:var(--ac)}
article.walk h3.loc:hover a{text-decoration:none}

/* callouts */
.callout{display:grid;grid-template-columns:auto 1fr;gap:10px;margin:16px 0;padding:12px 14px;border-radius:12px;border:1px solid var(--bd);border-left:4px solid var(--c,var(--ac));background:color-mix(in srgb,var(--c,var(--ac)) 8%,var(--pn))}
.callout .ic{color:var(--c,var(--ac));margin-top:2px}
.callout p{margin:0;max-width:none}
.callout.info{--c:var(--ac)}.callout.tip{--c:var(--ok)}.callout.warn,.callout.boss{--c:var(--ac2)}

/* battle cards */
.battle{position:relative;margin:20px 0;padding:14px 18px 10px;border:1px solid var(--bd);border-left:4px solid var(--c);border-radius:14px;background:linear-gradient(160deg,var(--pn),var(--pn2));--c:var(--water);box-shadow:var(--glow)}
.battle.digimon{--c:var(--water)}
.battle.card{--c:var(--wind)}
.battle.boss{--c:var(--boss);border-color:color-mix(in srgb,var(--boss) 40%,var(--bd));box-shadow:0 0 0 1px color-mix(in srgb,var(--boss) 22%,transparent),0 0 28px color-mix(in srgb,var(--boss) 18%,transparent)}
.battle.boss::before{content:"";position:absolute;left:0;right:0;top:0;height:46px;border-radius:14px 14px 0 0;background:linear-gradient(90deg,color-mix(in srgb,var(--boss) 22%,transparent),transparent 75%);pointer-events:none}
.battle .bh{position:relative;display:flex;gap:10px;align-items:center;flex-wrap:wrap;margin-bottom:6px}
.battle .bk{display:inline-flex;align-items:center;gap:6px;font:700 11px var(--mono);letter-spacing:.12em;text-transform:uppercase;color:var(--c);padding:3px 9px;border-radius:999px;background:color-mix(in srgb,var(--c) 14%,transparent)}
.battle .bt{font-weight:700;font-size:20px;line-height:1.25;color:var(--fg)}
.battle .bt a{color:inherit}
.battle .meta{display:flex;gap:6px;flex-wrap:wrap;margin:6px 0 4px}
.pill{display:inline-flex;align-items:baseline;gap:4px;padding:2px 10px;border-radius:999px;border:1px solid var(--bd);font:13px var(--mono);color:var(--fg);background:var(--bg2);white-space:nowrap}
.pill b{font-weight:700}
.pill.ex{color:var(--ac);border-color:color-mix(in srgb,var(--ac) 35%,var(--bd))}
.pill.bit{color:var(--ac2);border-color:color-mix(in srgb,var(--ac2) 35%,var(--bd))}
.pill.lv{color:var(--ok);border-color:color-mix(in srgb,var(--ok) 40%,var(--bd))}
.pill.ghost{color:var(--mut)}
.battle .row{display:flex;gap:10px;align-items:center;flex-wrap:wrap;padding:7px 0;border-top:1px dashed var(--bd)}
.battle .who{flex:1;min-width:12ch;font-size:17px;font-weight:600}
.battle .who a{color:var(--fg)}
.battle .strat{margin-top:8px;border-top:1px solid var(--bd);padding-top:6px}
.battle .strat>summary{cursor:pointer;color:var(--mut);font-size:13px;letter-spacing:.06em;text-transform:uppercase;font-weight:600;list-style:none}
.battle .strat>summary::-webkit-details-marker{display:none}
.battle .strat>summary::before{content:"▸ ";color:var(--c)}
.battle .strat[open]>summary::before{content:"▾ "}
.battle p{margin:.5em 0;font-size:.97em;max-width:none}
.battle pre{margin:.5em 0}

/* labeled lists and tables */
.lab{margin:18px 0}
.lab h4{margin:0 0 6px;font-size:12px;letter-spacing:.09em;text-transform:uppercase;color:var(--mut);font-weight:700}
.chips{list-style:none;margin:0;padding:0;display:flex;flex-wrap:wrap;gap:6px}
.chips li{background:var(--pn);border:1px solid var(--bd);border-radius:999px;padding:3px 12px;font-size:14px}
.tag{font:700 10px var(--mono);letter-spacing:.06em;background:var(--boss);color:#fff;border-radius:4px;padding:1px 5px;margin-left:4px;vertical-align:2px}
.tw{overflow-x:auto;margin:8px 0;border:1px solid var(--bd);border-radius:12px;background:var(--pn)}
table.grid{border-collapse:separate;border-spacing:0;font-size:14.5px;min-width:min(100%,480px);width:100%}
table.grid th,table.grid td{padding:8px 13px;border-bottom:1px solid var(--bd);text-align:left;vertical-align:middle}
table.grid tbody tr:last-child td{border-bottom:0}
table.grid th{font-size:11.5px;letter-spacing:.08em;text-transform:uppercase;color:var(--mut);background:var(--pn2);position:sticky;top:0;font-weight:700;white-space:nowrap}
table.grid tbody tr:nth-child(even){background:color-mix(in srgb,var(--fg) 3%,transparent)}
table.grid tbody tr:hover{background:var(--hov)}
table.grid td.num,table.grid th.num{text-align:right;font-family:var(--mono);font-variant-numeric:tabular-nums}
table.grid td small{color:var(--mut)}
table.grid td .n{font-family:var(--mono);font-variant-numeric:tabular-nums}
table.grid .bar{display:block;height:4px;border-radius:4px;margin:4px 0 0 auto;min-width:2px;width:var(--w,0%);background:linear-gradient(90deg,var(--ac),var(--ac2));opacity:.85}
table.grid td.num{min-width:110px}

/* version timeline */
ol.tl{list-style:none;margin:8px 0 0;padding:0 0 0 22px;position:relative}
ol.tl::before{content:"";position:absolute;left:6px;top:8px;bottom:8px;width:2px;background:linear-gradient(var(--ac),var(--ac2) 70%,transparent)}
ol.tl>li.ver{position:relative;padding:4px 0 26px}
ol.tl>li.ver::before{content:"";position:absolute;left:-22px;top:9px;width:14px;height:14px;border-radius:50%;background:var(--bg);border:3px solid var(--ac);box-shadow:0 0 0 4px color-mix(in srgb,var(--ac) 15%,transparent)}
ol.tl .vh{display:flex;gap:10px;align-items:center;flex-wrap:wrap}
ol.tl .vb{font:700 13px var(--mono);color:var(--bg);background:var(--ac);padding:2px 10px;border-radius:7px}
ol.tl .vd{font:13px var(--mono);color:var(--ac2)}
ol.tl .vt{margin:6px 0 4px;font-weight:600;max-width:none}
ol.tl ul{margin:4px 0 0;padding-left:18px;color:var(--fg)}
ol.tl ul li{margin:3px 0;max-width:72ch}

/* card-style intro sections */
.intro.card{padding:18px 24px 14px;border:1px solid var(--bd);border-radius:16px;background:var(--pn);max-width:820px}
.intro.card p{max-width:none}
ul.credits{list-style:none;margin:10px 0 14px;padding:0;display:grid;gap:8px}
ul.credits li{display:grid;grid-template-columns:minmax(150px,max-content) 1fr;gap:4px 16px;padding:10px 14px;border-radius:12px;background:var(--bg2);border:1px solid var(--bd)}
ul.credits b{color:var(--ac)}
ul.credits span{color:var(--fg)}

/* evolution and partner data */
.muted{color:var(--mut)}
details.orig{margin:8px 0 14px}
details.orig>summary{cursor:pointer;color:var(--mut);font-size:12.5px;width:max-content;padding:2px 0}
details.orig>summary:hover{color:var(--ac)}
details.orig>pre{margin-top:6px;font-size:12.5px}
.reqs{display:inline-flex;flex-wrap:wrap;gap:5px;vertical-align:middle}
.rq{display:inline-flex;align-items:center;gap:5px;padding:2px 9px;border-radius:7px;font:13px var(--mono);background:var(--bg2);border:1px solid var(--bd);color:var(--fg);white-space:nowrap}
.rq b{font-weight:700}
.rq.lv{border-color:color-mix(in srgb,var(--ac) 40%,var(--bd))}
.rq .lvn{color:var(--ac2);font-weight:700}
.rq.st b{color:var(--ok)}
.rq.el{color:var(--c);border-color:color-mix(in srgb,var(--c) 50%,var(--bd))}
.rq.el b{color:var(--fg)}
.acqlist{display:grid;gap:8px;margin:10px 0}
.acq{display:grid;grid-template-columns:minmax(150px,max-content) 1fr;gap:12px;align-items:center;padding:10px 14px;border:1px solid var(--bd);border-radius:12px;background:var(--pn)}
.acq .who{font-weight:700;font-size:16px}
.acq .who a{color:var(--ac)}
.stats{display:grid;grid-template-columns:1fr 1fr;gap:20px;margin:12px 0;padding:14px 16px;border:1px solid var(--bd);border-radius:14px;background:var(--pn)}
.sgrp h4,.kv h4{margin:0 0 6px;font-size:12px;letter-spacing:.09em;text-transform:uppercase;color:var(--mut)}
.srow{display:grid;grid-template-columns:104px 1fr 52px;gap:10px;align-items:center;padding:4px 0;font-size:14px}
.srow .sl{color:var(--fg)}
.sbar{height:10px;background:var(--bg2);border:1px solid var(--bd);border-radius:999px;overflow:hidden}
.sbar i{display:block;height:100%;width:var(--w,0%);border-radius:999px;background:linear-gradient(90deg,color-mix(in srgb,var(--c,var(--ac)) 55%,transparent),var(--c,var(--ac)))}
.srow .sv{font:13px var(--mono);text-align:right;color:var(--ok)}
.srow.neg .sv{color:var(--boss)}
.srow.neg .sbar i{background:var(--boss)}
.srow.zero .sv{color:var(--mut)}
.sstrip{display:flex;gap:8px;flex-wrap:wrap;margin-top:12px}
.sc{padding:4px 11px;border-radius:999px;font-size:13px;border:1px solid color-mix(in srgb,var(--c,var(--ac)) 50%,var(--bd));background:color-mix(in srgb,var(--c,var(--ac)) 12%,transparent)}
.sc b{color:var(--ok);font-family:var(--mono)}
.kvwrap{display:grid;grid-template-columns:1fr 1fr;gap:16px;margin:12px 0}
.kv dl{display:grid;grid-template-columns:1fr auto;margin:0;border:1px solid var(--bd);border-radius:12px;overflow:hidden;background:var(--pn)}
.kv dl>div{display:contents}
.kv dt,.kv dd{margin:0;padding:7px 12px;border-bottom:1px solid var(--bd);align-self:center}
.kv dd{font:14px var(--mono);text-align:right}
.kv dl>div:last-child dt,.kv dl>div:last-child dd{border-bottom:0}
ol.lvgrid{list-style:none;display:grid;grid-template-columns:repeat(auto-fill,minmax(168px,1fr));gap:6px;margin:10px 0;padding:0}
.lc{display:grid;grid-template-columns:auto 1fr;gap:3px 8px;padding:7px 10px;border-radius:10px;border:1px solid var(--bd);background:var(--pn);font-size:14px;align-items:center;min-width:0}
.lc .lv{font:600 12px var(--mono);color:var(--mut)}
.lc .tn{overflow:hidden;text-overflow:ellipsis;white-space:nowrap;min-width:0}
.lc .bd{grid-column:1/-1;justify-self:start;font:700 10px var(--mono);letter-spacing:.1em;padding:1px 7px;border-radius:5px}
.lc.learn{border-color:color-mix(in srgb,var(--ac) 45%,var(--bd))}
.lc.learn .bd{background:color-mix(in srgb,var(--ac) 18%,transparent);color:var(--ac)}
.lc.load{border-color:color-mix(in srgb,var(--ac2) 45%,var(--bd))}
.lc.load .bd{background:color-mix(in srgb,var(--ac2) 18%,transparent);color:var(--ac2)}
.lc.empty{opacity:.42;border-style:dashed;background:transparent;grid-template-columns:1fr}
.tgrid{display:grid;grid-template-columns:repeat(auto-fill,minmax(290px,1fr));gap:12px;margin:12px 0}
.tcard{display:flex;flex-direction:column;gap:9px;padding:14px 16px;border:1px solid var(--bd);border-radius:14px;background:linear-gradient(160deg,var(--pn),var(--pn2));min-width:0}
.tcard header{display:flex;justify-content:space-between;align-items:flex-start;gap:8px;flex-wrap:wrap}
.tcard h4{margin:0;font-size:17px;line-height:1.25;color:var(--fg);text-transform:none;letter-spacing:0}
.tbadges{display:flex;gap:5px;flex-wrap:wrap}
.el{font:700 11px var(--mono);letter-spacing:.05em;text-transform:uppercase;color:var(--c,var(--neutral));padding:2px 8px;border-radius:6px;background:color-mix(in srgb,var(--c,var(--neutral)) 15%,transparent);border:1px solid color-mix(in srgb,var(--c,var(--neutral)) 45%,transparent)}
.tg{font:700 11px var(--mono);color:var(--mut);border:1px solid var(--bd);padding:2px 6px;border-radius:6px}
.tst{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;margin:0}
.tst>div{background:var(--bg2);border:1px solid var(--bd);border-radius:9px;padding:6px 8px;min-width:0}
.tst dt{font-size:10.5px;color:var(--mut);letter-spacing:.05em;text-transform:uppercase;white-space:nowrap}
.tst dd{margin:0;font:600 15px var(--mono)}
.tst .lrn dd{color:var(--ac)}
.tst .ldd dd{color:var(--ac2)}
.tst dd.na{color:var(--mut);font-weight:400}
.tcard .gt,.tcard .ds{margin:0;font-size:14px;line-height:1.5;max-width:none}
.cap{display:block;font:700 10px var(--mono);letter-spacing:.1em;text-transform:uppercase;color:var(--mut);margin-bottom:2px}
ul.dtree details>summary{list-style:none;cursor:pointer}
ul.dtree details>summary::-webkit-details-marker{display:none}
.dtwrap{margin:12px 0}
.dtools{display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap}
.dtools button{font:inherit;font-size:13px;padding:5px 11px;border-radius:8px;border:1px solid var(--bd);background:var(--pn);color:var(--fg);cursor:pointer}
.dtools button:hover{border-color:var(--ac);color:var(--ac)}
.droot{display:inline-block;font-weight:800;font-size:18px;padding:6px 14px;border-radius:10px;border:1px solid var(--ac);color:var(--ac);background:color-mix(in srgb,var(--ac) 10%,transparent);margin-bottom:8px}
.droot a{color:inherit}
ul.dtree{list-style:none;margin:0 0 0 12px;padding:4px 0 0 24px;border-left:1px dashed color-mix(in srgb,var(--ac) 45%,var(--bd))}
ul.dtree.top{margin-left:12px}
ul.dtree>li{position:relative;padding:3px 0}
ul.dtree>li::before{content:"";position:absolute;left:-24px;top:19px;width:20px;border-top:1px dashed color-mix(in srgb,var(--ac) 45%,var(--bd))}
ul.dtree .leaf,ul.dtree summary{display:flex;align-items:center;gap:8px;flex-wrap:wrap;padding:6px 10px;border-radius:9px;border:1px solid transparent}
ul.dtree summary{cursor:pointer}
ul.dtree summary:hover,ul.dtree .leaf:hover{background:var(--hov);border-color:var(--bd)}
ul.dtree details>summary::before{content:"▸";color:var(--ac);width:10px;transition:transform .15s}
ul.dtree details[open]>summary::before{transform:rotate(90deg)}
.dn{font-weight:700}
.dn a{color:var(--fg)}
.dn a:hover{color:var(--ac)}
.dvgrid{display:grid;grid-template-columns:repeat(auto-fill,minmax(250px,1fr));gap:10px;margin:12px 0}
.dvc{padding:11px 13px;border:1px solid var(--bd);border-radius:12px;background:var(--pn);min-width:0}
.dvc h4{margin:0 0 7px;font-size:15px;color:var(--fg);text-transform:none;letter-spacing:0}
.dvc h4 a{color:var(--fg)}
.tw.exp{max-height:440px;overflow:auto}
.tw.exp table{min-width:0}
@media(max-width:700px){
 .stats,.kvwrap{grid-template-columns:1fr}
 .acq{grid-template-columns:1fr}
 ul.dtree{margin-left:4px;padding-left:12px}
 ul.dtree.top{margin-left:0}
 ul.dtree>li::before{left:-12px;width:10px}
 .rq{white-space:normal}
 .reqs{max-width:100%}
 .droot{font-size:16px}
}

/* card battle, enemies, sortable tables */
.tbar{margin:6px 0 10px}
.tbar input{width:100%;max-width:380px;font:inherit;font-size:14px;color:var(--fg);background:var(--pn);border:1px solid var(--bd);border-radius:9px;padding:7px 12px;height:36px}
.tbar input:focus{outline:none;border-color:var(--ac);box-shadow:0 0 0 3px color-mix(in srgb,var(--ac) 22%,transparent)}
.tall{max-height:560px;overflow:auto;border-radius:12px}
.tall table.grid th{top:0}
.bgrp{margin:14px 0 18px}
.bgrp h4{margin:0 0 8px;font-size:12px;letter-spacing:.09em;text-transform:uppercase;color:var(--ac);font-weight:700}
.bgrp.prog h4{color:var(--ac2)}
ul.cgrid{list-style:none;margin:0;padding:0;display:grid;grid-template-columns:repeat(auto-fill,minmax(160px,1fr));gap:6px}
ul.cgrid li{padding:6px 10px;border-radius:9px;border:1px solid var(--bd);background:var(--pn);font-size:14px;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
ul.cgrid li:hover{border-color:var(--ac)}
.bgrp.prog ul.cgrid li{border-left:3px solid var(--ac2)}
.bgrp:not(.prog) ul.cgrid li{border-left:3px solid var(--ac)}
[hidden]{display:none!important}
table.grid th[aria-sort]{cursor:pointer;user-select:none}
table.grid th[aria-sort]:hover{color:var(--ac)}
table.grid th[aria-sort=ascending]::after{content:" ▲";color:var(--ac)}
table.grid th[aria-sort=descending]::after{content:" ▼";color:var(--ac)}
table.grid th:focus-visible{outline:2px solid var(--ac);outline-offset:-2px}
section.lvl1.enter{animation:fadeIn .45s ease both}
@keyframes fadeIn{from{opacity:0;transform:translateY(6px)}to{opacity:1;transform:none}}

/* mobile dock */
#dock{display:none}

/* command palette, shortcuts */
.modal{position:fixed;inset:0;z-index:60;background:color-mix(in srgb,var(--bg) 60%,transparent);backdrop-filter:blur(4px);-webkit-backdrop-filter:blur(4px);display:flex;align-items:flex-start;justify-content:center;padding:12vh 16px 16px}
.mbox{width:min(640px,100%);background:var(--pn);border:1px solid var(--bd);border-radius:16px;box-shadow:0 30px 80px rgba(0,0,0,.35),var(--glow);overflow:hidden}
.mbox.sm{padding:20px 22px;width:min(460px,100%)}
.mbox.sm h2{margin:0 0 10px;font-size:18px}
.keys{display:grid;grid-template-columns:auto 1fr;gap:8px 16px;margin:0;align-items:center}
.keys dt{white-space:nowrap}.keys dd{margin:0;color:var(--mut)}
#pq{width:100%;font:inherit;font-size:16px;color:var(--fg);background:transparent;border:0;border-bottom:1px solid var(--bd);padding:16px 18px;outline:none}
#pl{list-style:none;margin:0;padding:6px;max-height:min(56vh,480px);overflow:auto}
#pl li{display:grid;grid-template-columns:auto 1fr;gap:2px 10px;padding:9px 12px;border-radius:9px;cursor:pointer;align-items:baseline}
#pl li.on{background:color-mix(in srgb,var(--ac) 14%,transparent);box-shadow:inset 3px 0 0 var(--ac)}
#pl .pk{font:600 11px var(--mono);letter-spacing:.06em;text-transform:uppercase;color:var(--ac);min-width:70px}
#pl .pt{font-weight:600;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
#pl .pg{grid-column:2;font-size:12px;color:var(--mut);overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
#pl .none{display:block;color:var(--mut);cursor:default}
.mfoot{margin:0;padding:8px 14px;border-top:1px solid var(--bd);font-size:12px;color:var(--mut)}
#top{position:fixed;right:18px;bottom:18px;width:42px;height:42px;border-radius:12px;border:1px solid var(--bd);background:var(--pn);color:var(--ac);cursor:pointer;z-index:25;display:grid;place-items:center;opacity:0;pointer-events:none;transition:opacity .15s,border-color .15s}
#top.show{opacity:1;pointer-events:auto}
#top:hover{border-color:var(--ac)}

@media(max-width:900px){
 :root{--bh:104px}
 #bar{flex-wrap:wrap;align-content:flex-start;padding-top:8px;row-gap:8px}
 #bar .crumb,#bar .brand span,#pbtn span,#bar .kbd-hint{display:none}
 #bar .sf{order:9;width:100%}
 #bar .sf kbd{display:none}
 #menu{display:inline-flex}
 #side{transform:translateX(-102%);transition:transform .22s ease;width:min(86vw,340px);bottom:58px;box-shadow:none;background:var(--pn)}
 body.open #side{transform:none;box-shadow:0 0 40px rgba(0,0,0,.5)}
 body.open::after{content:"";position:fixed;inset:0;background:rgba(0,0,0,.35);z-index:15}
 main{margin-left:0;padding:calc(var(--bh) + 16px) 14px 110px}
 .hero{padding:30px 22px 22px}
 .hero h1{font-size:34px}
 .tiles{grid-template-columns:1fr}
 ul.idx{columns:1}
 section.lvl1>h2{font-size:24px}
 #dock{display:flex;position:fixed;left:0;right:0;bottom:0;height:58px;z-index:28;justify-content:space-around;align-items:center;gap:6px;padding:0 12px;background:color-mix(in srgb,var(--bg) 88%,transparent);backdrop-filter:blur(14px);-webkit-backdrop-filter:blur(14px);border-top:1px solid var(--bd)}
 #dock .dk{flex:1;max-width:96px;height:42px;display:grid;place-items:center;border-radius:11px;border:1px solid var(--bd);background:var(--pn);color:var(--fg);cursor:pointer;text-decoration:none}
 #dock .dk:hover{text-decoration:none;color:var(--ac)}
 #dock .dk[aria-disabled=true]{opacity:.4;pointer-events:none}
 #top{bottom:72px;right:12px}
 .sf input{font-size:16px}
}
@media(max-width:420px){#fs{display:none}}

@media (prefers-reduced-motion: reduce){
 *,*::before,*::after{animation:none!important;transition:none!important}
 html{scroll-behavior:auto}
}
@media print{
 :root,:root[data-theme]{--bg:#fff;--pn:#fff;--pn2:#fff;--bg2:#fff;--fg:#000;--mut:#333;--bd:#bbb;--ac:#000;--ac2:#000;--boss:#000;--ok:#000;--hov:transparent;--glow:none}
 body{background:#fff;color:#000}
 #bar,#side,#dock,#top,.modal,.pn,.hero::before,.hero::after,details.orig,.tiles{display:none!important}
 main{margin:0;padding:0}
 section.lvl1{break-before:page}
 .hero{border:0;box-shadow:none;background:none;padding:0}
 pre,.battle,.tile{break-inside:avoid}
}
</style>
</head>
<body>
<a class="skip" href="#main">Pular para o conteúdo</a>
<header id="bar">
 <button id="menu" class="ib icon" aria-label="Abrir índice" aria-controls="side" aria-expanded="false" title="Índice">%%ICON:menu%%</button>
 <a class="brand" href="#inicio">%%ICON:book%%<span>Digimon World 3</span></a>
 <nav class="crumb" id="crumb" aria-label="Você está em"><span>Início</span></nav>
 <form class="sf" role="search" id="sform"><input id="q" type="search" placeholder="Buscar no guia" aria-label="Buscar no guia" autocomplete="off"><kbd title="Atalho: /">/</kbd></form>
 <span id="fs"><button id="fsm" class="ib icon" aria-label="Diminuir fonte" title="Diminuir fonte">A−</button><button id="fsp" class="ib icon" aria-label="Aumentar fonte" title="Aumentar fonte">A+</button></span>
 <button id="pbtn" class="ib" aria-label="Ir para… (Ctrl+K)" title="Ir para… (Ctrl+K)" aria-haspopup="dialog">%%ICON:cmd%%<span>Ir para…</span></button>
 <button id="theme" class="ib icon" aria-label="Alternar tema claro/escuro" title="Alternar tema">%%ICON:sun%%</button>
 <div id="prog" aria-hidden="true"></div>
</header>
<nav id="side" aria-label="Índice do guia">
 <div id="tree">
%%NAV%%
 </div>
 <div id="results" hidden></div>
</nav>
<main id="main" tabindex="-1">
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
  <input id="pq" type="text" placeholder="Ir para capítulo, seção, local, boss ou Digimon…" aria-label="Ir para" aria-controls="pl" autocomplete="off">
  <ul id="pl" role="listbox" aria-label="Resultados"></ul>
  <p class="mfoot"><kbd>↑</kbd> <kbd>↓</kbd> navegar · <kbd>Enter</kbd> ir · <kbd>Esc</kbd> fechar</p>
 </div>
</div>
<div id="kbd" class="modal" hidden>
 <div class="mbox sm" role="dialog" aria-modal="true" aria-label="Atalhos de teclado">
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
<script>
(()=>{
'use strict';
const $=(s,r=document)=>r.querySelector(s),$$=(s,r=document)=>[...r.querySelectorAll(s)],$i=id=>document.getElementById(id);
const root=document.documentElement,body=document.body,main=$i('main');
const store=(k,v)=>{try{if(v===undefined)return localStorage.getItem(k);localStorage.setItem(k,v)}catch(e){return null}};
const esc=s=>s.replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));
const mobile=()=>matchMedia('(max-width:900px)').matches;
// theme: saved choice > dark (default)
root.dataset.theme=store('dw3-theme')||'dark';
$i('theme').onclick=()=>{const t=root.dataset.theme==='light'?'dark':'light';root.dataset.theme=t;store('dw3-theme',t)};
// font size
let fs=+store('dw3-fs')||16;
const setFs=v=>{fs=Math.max(12,Math.min(24,v));root.style.setProperty('--fs',fs+'px');store('dw3-fs',fs)};
setFs(fs);$i('fsm').onclick=()=>setFs(fs-1);$i('fsp').onclick=()=>setFs(fs+1);
// mobile menu
const menu=$i('menu');
const setOpen=o=>{body.classList.toggle('open',o);menu.setAttribute('aria-expanded',String(o))};
menu.onclick=()=>setOpen(!body.classList.contains('open'));
$i('dmenu').onclick=()=>setOpen(!body.classList.contains('open'));
$i('side').addEventListener('click',e=>{if(e.target.closest('a'))setOpen(false)});
main.addEventListener('click',()=>{if(body.classList.contains('open'))setOpen(false)});
// sidebar groups remembered
const gstate=(()=>{try{return JSON.parse(store('dw3-groups')||'{}')}catch(e){return {}}})();
const groups=$$('#tree details.grp');
groups.forEach(d=>{const k=d.dataset.sec;if(k in gstate)d.open=!!gstate[k];
 d.addEventListener('toggle',()=>{gstate[k]=d.open;store('dw3-groups',JSON.stringify(gstate))})});
// scroll-spy, breadcrumb, progress, dock
const links=new Map();$$('#tree a[href^="#"]').forEach(a=>links.set(a.getAttribute('href').slice(1),a));
const heads=$$('section.lvl1,article.lvl2');
const navs=$$('article.lvl2,section.lvl1[data-leaf]');
const crumb=$i('crumb'),pbar=$i('prog'),topb=$i('top'),dprev=$i('dprev'),dnext=$i('dnext');
let cur=undefined;
function setActive(id){
 if(cur===id)return;cur=id;
 $$('#tree a.active').forEach(a=>a.classList.remove('active'));
 if(!id){crumb.innerHTML='<span>Início</span>'}
 else{
  const el=$i(id),isCh=el.classList.contains('lvl2'),sec=isCh?$i(el.dataset.parent):el;
  crumb.innerHTML=`<span>${esc(sec.dataset.title)}</span>`+(isCh?`<i>›</i><b>${esc(el.dataset.title)}</b>`:'');
 }
 const a=id&&links.get(id);
 if(a){
  a.classList.add('active');
  const d=a.closest('details');if(d)d.open=true;
  const r=a.getBoundingClientRect(),s=$i('side').getBoundingClientRect();
  if($i('results').hidden&&(r.top<s.top+40||r.bottom>s.bottom-40))a.scrollIntoView({block:'center'});
 }
 groups.forEach(d=>{const ls=$$('li a',d);const i=ls.findIndex(x=>x.classList.contains('active'));d.style.setProperty('--p',i<0?0:(i+1)/ls.length)});
 const ix=navs.findIndex(e=>e.id===id);
 const pv=navs[ix-1],nx=ix<0?navs[0]:navs[ix+1];
 dprev.href=pv?'#'+pv.id:'#inicio';dprev.setAttribute('aria-disabled',String(!pv));
 dnext.href=nx?'#'+nx.id:'#inicio';dnext.setAttribute('aria-disabled',String(!nx));
}
function frame(){
 tick=false;
 let id=null;
 if(scrollY>200)for(const h of heads){if(h.getBoundingClientRect().top<=150)id=h.id;else break}
 setActive(id);
 const max=document.documentElement.scrollHeight-innerHeight;
 pbar.style.width=(max>0?Math.min(100,scrollY/max*100):0)+'%';
 topb.classList.toggle('show',scrollY>800);
}
let tick=false;
addEventListener('scroll',()=>{if(!tick){tick=true;setTimeout(frame,60)}},{passive:true});
topb.onclick=()=>scrollTo({top:0});
// keyboard: j/k move between chapters
function go(dir){
 const i=navs.findIndex(e=>e.id===cur);
 const el=i<0?navs[dir>0?0:navs.length-1]:navs[i+dir];
 if(el)el.scrollIntoView({block:'start'});
}
// search
const SKIP='.chb,.pn,ul.idx,h3.ch,h2,.tiles,details.orig,.nosearch';
const txt=n=>n.nodeType===3?n.data:(n.nodeType!==1||n.matches(SKIP)?'':[...n.childNodes].map(txt).join(''));
const secs=$$('article.lvl2,section.lvl1').map(el=>{
 const lvl1=el.classList.contains('lvl1');
 const own=[...el.children].filter(n=>!n.matches(SKIP+(lvl1?',article':'')));
 return {el,id:el.id,title:el.dataset.title,parent:lvl1?null:el.dataset.parent,text:own.map(txt).join('\n')};
}).filter(s=>s.text.trim());
const input=$i('q'),res=$i('results'),tree=$i('tree');
let timer;
$i('sform').addEventListener('submit',e=>e.preventDefault());
input.addEventListener('input',()=>{clearTimeout(timer);timer=setTimeout(run,150)});
function run(){
 clearMarks();
 const q=input.value.trim();
 if(q.length<2){res.hidden=true;tree.hidden=false;return}
 if(mobile())body.classList.add('open');
 const lq=q.toLowerCase();let total=0,html='';
 for(const s of secs){
  const lt=s.text.toLowerCase();let i=lt.indexOf(lq),n=0,first=-1;
  while(i>=0){n++;if(first<0)first=i;i=lt.indexOf(lq,i+lq.length)}
  if(!n)continue;total+=n;
  const a=Math.max(0,first-50),b=Math.min(s.text.length,first+lq.length+70);
  const snip=(a?'…':'')+s.text.slice(a,b).replace(/\s+/g,' ')+(b<s.text.length?'…':'');
  html+=`<a class="r" href="#${s.id}" data-q="${esc(q)}"><b>${esc(s.title)} <span class="n">(${n})</span></b><small>${esc(snip)}</small></a>`;
 }
 res.innerHTML=`<div class="cnt">${total} ocorrência(s) para “${esc(q)}”</div>`+html;
 res.hidden=false;tree.hidden=true;
}
res.addEventListener('click',e=>{const a=e.target.closest('a.r');if(!a)return;
 setTimeout(()=>highlight(a.getAttribute('href').slice(1),a.dataset.q),0)});
function clearMarks(){$$('mark').forEach(m=>{const p=m.parentNode;p.replaceChild(document.createTextNode(m.textContent),m);p.normalize()})}
function highlight(id,q){
 clearMarks();const el=$i(id);if(!el)return;
 const lq=q.toLowerCase();const w=document.createTreeWalker(el,NodeFilter.SHOW_TEXT);const nodes=[];
 while(w.nextNode())nodes.push(w.currentNode);
 let first=null;
 for(const t of nodes){
  if(t.parentNode.closest('.chb,.pn,ul.idx,h3.ch,details.orig,button,script,style'))continue;
  const lt=t.data.toLowerCase();let i=lt.indexOf(lq);if(i<0)continue;
  const frag=document.createDocumentFragment();let last=0;
  while(i>=0){frag.append(t.data.slice(last,i));const m=document.createElement('mark');
   m.textContent=t.data.slice(i,i+lq.length);frag.append(m);if(!first)first=m;
   last=i+lq.length;i=lt.indexOf(lq,last)}
  frag.append(t.data.slice(last));t.replaceWith(frag);
 }
 if(first)first.scrollIntoView({block:'center'});
}
// command palette
const PAL=JSON.parse($i('pal-data').textContent);
const pal=$i('pal'),pq=$i('pq'),pl=$i('pl');
const KLAB={sec:'Seção',cap:'Capítulo',loc:'Local',dig:'Digimon'};
let psel=0,plist=[];
function fuzzy(t,q){const i=t.indexOf(q);if(i>=0)return i;let p=0,last=-1,gap=0;
 for(const ch of q){const j=t.indexOf(ch,p);if(j<0)return -1;if(last>=0)gap+=j-last-1;last=j;p=j+1}
 return 1000+gap}
function palRender(){
 const q=pq.value.trim().toLowerCase();
 plist=!q?PAL.filter(x=>x.k==='sec'||x.k==='cap').slice(0,40)
  :PAL.map(x=>[fuzzy((x.t+' · '+x.g).toLowerCase(),q),x]).filter(a=>a[0]>=0).sort((a,b)=>a[0]-b[0]).slice(0,60).map(a=>a[1]);
 psel=Math.max(0,Math.min(psel,plist.length-1));
 pl.innerHTML=plist.map((x,i)=>`<li role="option" id="po${i}" aria-selected="${i===psel}" class="${i===psel?'on':''}" data-i="${i}"><span class="pk">${KLAB[x.k]}</span><span class="pt">${esc(x.t)}</span><span class="pg">${esc(x.g)}</span></li>`).join('')
  ||'<li class="none">Nada encontrado</li>';
 const on=$i('po'+psel);if(on)on.scrollIntoView({block:'nearest'});
}
function openPal(){pal.hidden=false;pq.value='';psel=0;palRender();setTimeout(()=>pq.focus(),0)}
function closePal(){pal.hidden=true}
function jump(x){closePal();const el=$i(x.id);if(el){el.scrollIntoView({block:'start'});history.replaceState(null,'','#'+x.id)}}
$i('pbtn').onclick=openPal;
pq.addEventListener('input',()=>{psel=0;palRender()});
pq.addEventListener('keydown',e=>{
 if(e.key==='ArrowDown'){e.preventDefault();psel=Math.min(plist.length-1,psel+1);palRender()}
 else if(e.key==='ArrowUp'){e.preventDefault();psel=Math.max(0,psel-1);palRender()}
 else if(e.key==='Enter'){e.preventDefault();if(plist[psel])jump(plist[psel])}
});
pl.addEventListener('click',e=>{const li=e.target.closest('li[data-i]');if(li)jump(plist[+li.dataset.i])});
pal.addEventListener('click',e=>{if(e.target===pal)closePal()});
// shortcuts overlay
const kbdm=$i('kbd');
kbdm.addEventListener('click',e=>{if(e.target===kbdm)kbdm.hidden=true});
// global keys
addEventListener('keydown',e=>{
 if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==='k'){e.preventDefault();openPal();return}
 const typing=/^(INPUT|TEXTAREA|SELECT)$/.test(document.activeElement.tagName);
 if(e.key==='Escape'){
  if(!pal.hidden){closePal();return}
  if(!kbdm.hidden){kbdm.hidden=true;return}
  if(body.classList.contains('open')&&mobile()){setOpen(false);return}
  if(document.activeElement===input){input.value='';run();input.blur()}
  return;
 }
 if(typing||!pal.hidden||!kbdm.hidden||e.altKey||e.ctrlKey||e.metaKey)return;
 if(e.key==='/'){e.preventDefault();input.focus();input.select()}
 else if(e.key==='?'){e.preventDefault();kbdm.hidden=false}
 else if(e.key==='j'){go(1)}
 else if(e.key==='k'){go(-1)}
});
$i('dsearch').onclick=()=>{input.focus();input.select()};
// digivolution tree: expand / collapse all
document.addEventListener('click',e=>{const b=e.target.closest('.dtools button');if(!b)return;
 const open=b.dataset.act==='open';b.closest('.dtwrap').querySelectorAll('details').forEach(d=>d.open=open)});
// strategy notes start collapsed on small screens
if(mobile())$$('details.strat').forEach(d=>d.open=false);
// sortable tables (numeric aware), live filters, section fade-in
function sortTable(th){
 const table=th.closest('table'),idx=[...th.parentNode.children].indexOf(th);
 const asc=th.getAttribute('aria-sort')!=='ascending';
 table.querySelectorAll('thead th').forEach(h=>h.removeAttribute('aria-sort'));
 th.setAttribute('aria-sort',asc?'ascending':'descending');
 const tb=table.tBodies[0],rows=[...tb.rows];
 const key=r=>{const c=r.cells[idx];if(!c)return '';const t=(c.dataset.v!==undefined?c.dataset.v:c.textContent).trim();
  const n=parseFloat(t.replace(/,/g,''));return /^[-+]?\d/.test(t)&&!isNaN(n)?n:t.toLowerCase()};
 rows.sort((a,b)=>{const x=key(a),y=key(b);
  if(typeof x==='number'&&typeof y==='number')return asc?x-y:y-x;
  return asc?String(x).localeCompare(String(y)):String(y).localeCompare(String(x))});
 rows.forEach(r=>tb.appendChild(r));
}
$$('table.grid thead th').forEach(th=>{th.tabIndex=0;th.title='Clique para ordenar'});
document.addEventListener('click',e=>{const th=e.target.closest('table.grid thead th');if(th)sortTable(th)});
document.addEventListener('keydown',e=>{if(e.key!=='Enter')return;const th=e.target.closest&&e.target.closest('table.grid thead th');if(th)sortTable(th)});
document.addEventListener('input',e=>{const inp=e.target.closest('input.tfilt');if(!inp)return;
 const q=inp.value.trim().toLowerCase(),root=inp.closest('.fbox');
 root.querySelectorAll('tbody tr, ul.cgrid li').forEach(el=>{el.hidden=!!q&&!el.textContent.toLowerCase().includes(q)})});
if('IntersectionObserver' in window){
 const io=new IntersectionObserver(es=>es.forEach(en=>{if(en.isIntersecting){en.target.classList.add('enter');io.unobserve(en.target)}}),{rootMargin:'0px 0px -10% 0px'});
 $$('section.lvl1').forEach(sx=>io.observe(sx));
}
// deep link
if(location.hash){const el=$i(decodeURIComponent(location.hash.slice(1)));if(el)setTimeout(()=>el.scrollIntoView(),0)}
frame();
})();
</script>
</body>
</html>
"""


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    with open(sys.argv[1], encoding="utf-8") as f:
        lines = extract_text(f.read())
    header, sections = parse(lines)
    with open(sys.argv[2], "w", encoding="utf-8") as f:
        f.write(render(header, sections))
    for s in sections:
        print(f"{s['title']}: {len(s['chapters'])} chapters")
    print(f"fallback blocks: {STATS['fallback']}", file=sys.stderr)
    tot, bad = STATS.get("special", 0), STATS.get("special_fail", 0)
    print(f"evolution/partner blocks structured: {tot - bad}/{tot}", file=sys.stderr)
    print(f"card-battle boosters structured: {STATS.get('booster', 0) - STATS.get('booster_fail', 0)}/{STATS.get('booster', 0)}", file=sys.stderr)
    print(f"enemy list structured: {'no' if STATS.get('enemy_fail') else 'yes'}", file=sys.stderr)


if __name__ == "__main__":
    main()
