"""Offline chapter package assembled from the guide's existing render fragments.

Only the output layout changes: IDs, rendered translations and original runs are
owned by guide_to_html. Classic shared scripts work directly over file://.
"""
import html
import json
from html.parser import HTMLParser
from pathlib import Path, PurePosixPath
import re
from urllib.parse import unquote, urlsplit


class Fragment(HTMLParser):
    VOID = {'area', 'base', 'br', 'col', 'embed', 'hr', 'img', 'input', 'link',
            'meta', 'param', 'source', 'track', 'wbr'}
    SKIP = {'chb', 'pn', 'idx', 'tiles', 'orig', 'nosearch', 'entity-tools'}
    BLOCK = {'p', 'div', 'section', 'article', 'li', 'tr', 'h1', 'h2', 'h3', 'h4', 'h5'}

    def __init__(self, source):
        super().__init__(convert_charrefs=True)
        self.ids, self.links, self.assets, self.text = [], [], [], []
        self.stack, self.anchors, self.nested = [], 0, False
        self.feed(source)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if 'id' in attrs:
            self.ids.append(attrs['id'])
        if tag == 'a':
            self.links.append(attrs.get('href', ''))
            self.nested |= self.anchors > 0
            self.anchors += 1
        if tag == 'script' and attrs.get('src'):
            self.assets.append(attrs['src'])
        if tag == 'link' and attrs.get('rel') == 'stylesheet':
            self.assets.append(attrs.get('href', ''))
        if tag in self.VOID:
            return
        skip = (bool(self.stack and self.stack[-1][1]) or tag in {'script', 'style'}
                or bool(set(attrs.get('class', '').split()) & self.SKIP))
        self.stack.append((tag, skip))
        if not skip and tag in self.BLOCK:
            self.text.append('\n')

    def handle_endtag(self, tag):
        if tag == 'a':
            self.anchors = max(0, self.anchors - 1)
        if self.stack and self.stack[-1][0] == tag:
            _, skip = self.stack.pop()
            if not skip and tag in self.BLOCK:
                self.text.append('\n')

    def handle_data(self, text):
        if not self.stack or not self.stack[-1][1]:
            self.text.append(text)

    def reading_text(self):
        return re.sub(r'\s+', ' ', ''.join(self.text)).strip()


def _filename(page):
    if page['kind'] == 'home':
        return 'index.html'
    if page['kind'] == 'entities':
        return 'entities.html'
    if not re.fullmatch(r'[a-z0-9-]+', page['id']):
        raise ValueError(f'Unsafe page ID: {page["id"]}')
    return ('chapter-' if page['kind'] == 'chapter' else '') + page['id'] + '.html'


def _pager(page_id, order, by_id):
    if page_id not in order:
        children = [p for p in by_id.values() if p['parent'] == page_id]
        following = children[0]['id'] if children else None
        previous = None
    else:
        i = order.index(page_id)
        previous = order[i - 1] if i else None
        following = order[i + 1] if i + 1 < len(order) else None
    def link(key, cls, label, rel):
        if key is None:
            return '<span></span>'
        return (f'<a class="{cls}" rel="{rel}" href="#{key}">'
                f'<span><small>{label}</small>{html.escape(by_id[key]["title"])}</span></a>')
    return ('<nav class="pn page-pager" aria-label="Páginas do guia">'
            + link(previous, 'prev', '← Anterior', 'prev')
            + '<a class="to-top" href="#main">Topo</a>'
            + link(following, 'next', 'Próximo →', 'next') + '</nav>')


def _promote_headings(content, kind):
    # Each physical page gets one h1; chapter areas sit directly beneath it.
    if kind == 'chapter':
        for old, new in [('h3 class="ch"', 'h1 class="ch"'),
                         ('/h3', '/h1'), ('h4', 'h2'), ('h5', 'h3')]:
            # Only the chapter-title closing h3 needs promotion.
            if old == '/h3':
                content = content.replace('</h3>', '</h1>', 1)
            else:
                content = content.replace('<' + old, '<' + new)
                if old in {'h4', 'h5'}:
                    content = content.replace('</' + old + '>', '</' + new + '>')
    elif kind != 'home':
        content = content.replace('<h2>', '<h1 class="section-title">', 1)
        content = content.replace('</h2>', '</h1>', 1)
    return content


def build_site(document, pages, order):
    """Return a validated in-memory package; write nothing until validation passes."""
    by_id = {p['id']: dict(p, file=_filename(p)) for p in pages}
    if len(by_id) != len(pages) or len({p['file'] for p in by_id.values()}) != len(pages):
        raise ValueError('Duplicate package page IDs or filenames')
    canonical = {p['id']: p['file'] for p in by_id.values()}
    routes, content_ids = dict(canonical), set()
    for p in by_id.values():
        for target in Fragment(p['content']).ids:
            content_ids.add(target)
            # A chapter's lightweight wrapper repeats its parent section ID;
            # links to that ID still belong to the section's overview page.
            if target in canonical:
                continue
            if target in routes:
                raise ValueError(f'Duplicated content destination: {target}')
            routes[target] = p['file']
    shell_ids = set(Fragment(document).ids) - content_ids
    main_open = re.search(r'<main\b[^>]*>', document)
    main_end = document.index('</main>', main_open.end())
    shell = document[:main_open.end()] + '%%PAGE_BODY%%' + document[main_end:]
    palette = json.loads(re.search(r'<script type="application/json" id="pal-data">(.*?)</script>',
                                  shell, re.S)[1])
    shell = re.sub(r'<script type="application/json" id="pal-data">.*?</script>', '', shell, flags=re.S)
    shell = re.sub(r'<style>.*?</style>', '<link rel="stylesheet" href="assets/guide.css">', shell,
                   count=1, flags=re.S)
    shell = re.sub(r'<script>.*?</script>',
                   '<script src="assets/guide-data.js"></script><script src="assets/guide.js"></script>',
                   shell, flags=re.S)
    search = []
    for page in by_id.values():
        if page['kind'] == 'home':
            continue
        fragment = page['content']
        # Catalogue records are independent results, avoiding one huge result.
        entries = re.findall(r'<article class="entity-entry".*?</article>', fragment, re.S)
        records = entries or [fragment]
        for record in records:
            info = Fragment(record)
            entity = re.search(r'<h3>(.*?)</h3>', record) if entries else None
            search.append(dict(id=info.ids[0] if entries else page['id'],
                               title=html.unescape(entity[1]) if entity else page['title'],
                               parent=page['parent'] or page['id'], text=info.reading_text()))
    data = dict(routes=routes, palette=palette, search=search,
                order=[dict(id=key, title=by_id[key]['title']) for key in order])
    assets_dir = Path(__file__).with_name('guide_assets')
    files = {f'assets/guide.{ext}': (assets_dir / f'guide.{ext}').read_text(encoding='utf-8')
             for ext in ['css', 'js']}
    files['assets/guide-data.js'] = ('window.DW3_GUIDE='
        + json.dumps(data, ensure_ascii=False, separators=(',', ':')).replace('</', '<\\/') + ';\n')
    for page in by_id.values():
        content = _promote_headings(page['content'], page['kind'])
        parent = by_id.get(page['parent'])
        breadcrumb = '<nav class="page-breadcrumb" aria-label="Caminho"><a href="#inicio">Guia</a>'
        if parent:
            breadcrumb += f'<span>›</span><a href="#{parent["id"]}">{html.escape(parent["title"])}</a>'
        breadcrumb += '<span>›</span><span>' + html.escape(page['title']) + '</span></nav>'
        if page['kind'] == 'home':
            content += ('<p class="package-note">Pacote offline · ' + str(len(pages))
                        + ' páginas · a busca consulta todo o guia. Abra os capítulos pelo índice.</p>')
        else:
            pager = _pager(page['id'], order, by_id)
            content = re.sub(r'<nav class="pn".*?</nav>', '', content, flags=re.S)
            if page['kind'] == 'chapter':
                content = content.replace('</header>', '</header>' + pager, 1)
                content = breadcrumb + content + pager
            else:
                content = breadcrumb + pager + content + pager
        output = shell.replace('%%PAGE_BODY%%', content)
        output = output.replace('<body>', f'<body data-page-id="{page["id"]}">')
        output = output.replace('<title>Digimon World 3 — Guide and Walkthrough (EmeraldPhoenix)</title>',
                                '<title>' + html.escape(page['title']) + ' · Guia DW3</title>')
        def route(match):
            target = html.unescape(match[1])
            if target in shell_ids:
                return f'href="#{html.escape(target, quote=True)}"'
            filename = routes.get(target)
            if not filename:
                raise ValueError(f'Unknown destination: {target}')
            return f'href="{filename if filename != page["file"] else ""}#{html.escape(target, quote=True)}"'
        output = re.sub(r'href="#([^"]+)"', route, output)
        files[page['file']] = output
    validate_site(files, data)
    files['manifest.json'] = json.dumps(dict(generator='dw3-guide-site', format_version=1,
                                           files=sorted(files)), indent=2) + '\n'
    files['LEIA-ME.txt'] = ('GUIA OFFLINE — DIGIMON WORLD 3\n\n'
        'Abra index.html no navegador. Copie esta pasta inteira, incluindo assets/.\n'
        'Nao e necessario instalar nada, usar internet ou iniciar um servidor.\n'
        'Cada capitulo abre em um arquivo proprio; as areas mantem links diretos.\n'
        'A busca global e Ctrl+K precisam de JavaScript. Texto e links funcionam sem ele.\n'
        'O texto em ingles acompanha os blocos PT-BR em "ver texto original".\n'
        'Texto original: Emerald Phoenix / GameFAQs. Apenas para consulta local.\n')
    return files


def validate_site(files, data=None):
    """Check physical files, fragment targets, local resources and shared routes."""
    pages = {name: Fragment(value) for name, value in files.items() if name.endswith('.html')}
    count = 0
    for filename, page in pages.items():
        if len(page.ids) != len(set(page.ids)) or page.nested:
            raise ValueError(f'Duplicate IDs or nested anchors in {filename}')
        for value in page.links + page.assets:
            url = urlsplit(value)
            if url.scheme or url.netloc or not value:
                continue
            target = unquote(url.path) or filename
            if target not in files:
                raise ValueError(f'Missing file from {filename}: {value}')
            if url.fragment and (target not in pages or unquote(url.fragment) not in pages[target].ids):
                raise ValueError(f'Missing fragment from {filename}: {value}')
            count += 1
    if data:
        for key, filename in data['routes'].items():
            if filename not in pages or key not in pages[filename].ids:
                raise ValueError(f'Invalid shared route: {key} -> {filename}')
        for result in data['palette'] + data['search'] + data['order']:
            if result['id'] not in data['routes']:
                raise ValueError(f'Unroutable shared entry: {result["id"]}')
    print(f'package integrity: {len(pages)} HTML pages; {count} local links/resources; all targets resolve')
    return dict(pages=len(pages), links=count)


def write_site(directory, files):
    """Write generated files, refusing to overwrite unrelated local data."""
    directory = Path(directory)
    manifest = directory / 'manifest.json'
    owned = set()
    if manifest.exists():
        previous = json.loads(manifest.read_text(encoding='utf-8'))
        if previous.get('generator') != 'dw3-guide-site' or previous.get('format_version') != 1:
            raise ValueError('Unsupported existing package manifest')
        owned = set(previous['files']) | {'manifest.json', 'LEIA-ME.txt'}
    for name in set(files) | owned:
        path = PurePosixPath(name)
        if path.is_absolute() or '..' in path.parts:
            raise ValueError(f'Unsafe package path: {name}')
        target = directory / name
        if target.is_symlink() or any(p.is_symlink() for p in target.parents if p != directory.parent):
            raise ValueError(f'Symlink in package path: {target}')
        if name in files and target.exists() and name not in owned:
            raise ValueError(f'Refusing to overwrite unrelated file: {target}')
    for name, value in files.items():
        target = directory / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(value, encoding='utf-8')
    for stale in owned - files.keys():
        target = directory / stale
        if target.is_file():
            target.unlink()
    print(f'offline package: {directory / "index.html"} ({len(files)} files)')
