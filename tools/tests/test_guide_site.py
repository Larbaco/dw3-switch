"""Chapter package routing/ownership tests using synthetic, non-game text."""
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import guide_to_html as guide
import guide_site as site
from test_guide_to_html import fixture, chapter


def package():
    sections = fixture()
    sections[2]['chapters'].append(chapter('walk-return', '2. Return', [
        ('h', 'city-return', 'TEST CITY - INN'),
        ('pre', 'Testmon returns to Test City. A unique second chapter sentence.'),
    ]))
    return guide.render('Synthetic author header', sections, site=True)


def shared(files):
    return json.loads(files['assets/guide-data.js'].removeprefix('window.DW3_GUIDE=').strip().removesuffix(';'))


class GuideSiteTest(unittest.TestCase):
    def test_chapters_are_physical_pages_and_routes_keep_source_links(self):
        files = package()
        data = shared(files)
        assert data['routes']['walk'] == 'chapter-walk.html'
        assert data['routes']['city-return'] == 'chapter-walk-return.html'
        assert '<article class="lvl2 walk"' not in files['walkthrough.html']
        assert 'A unique second chapter sentence.' not in files['chapter-walk.html']
        assert 'chapter-walk-return.html#walk-return' in files['chapter-walk.html']
        assert 'chapter-walk.html#walk' in files['chapter-walk-return.html']
        entity = next(x for x in data['palette'] if x['t'] == 'Testmon' and x['k'] == 'ent')
        assert 'entities.html#' + entity['id'] in files['chapter-walk.html']
        assert 'chapter-walk.html#walk--source-' in files['entities.html']
        assert '<h1 class="ch">' in files['chapter-walk.html']
        assert '<h2 id="city" class="subheading loc"' in files['chapter-walk.html']
        assert all(site.Fragment(page).ids.count('walk') <= 1 for name, page in files.items() if name.endswith('.html'))
        site.validate_site(files, data)

    def test_global_search_data_is_shared_and_excludes_original_disclosures(self):
        files = package()
        data = shared(files)
        assert {x['id'] for x in data['search']} >= {'walk', 'walk-return', 'evolution'}
        assert any('A unique second chapter sentence.' in x['text'] for x in data['search'])
        assert site.Fragment('<p>Texto visível.</p><details class="orig"><pre>HIDDEN ENGLISH</pre></details>').reading_text() == 'Texto visível.'
        for name, text in files.items():
            if name.endswith('.html'):
                assert '<script src="assets/guide-data.js"></script>' in text
                assert 'window.DW3_GUIDE=' not in text
                assert 'id="pal-data"' not in text
                assert 'fetch(' not in text and 'type="module"' not in text
                assert text.count('<h1') == 1, name
        assert package() == files

    def test_package_validator_rejects_broken_routes_anchors_assets_and_duplicates(self):
        files = package()
        for fragment in ['<a href="missing.html#x">bad</a>',
                         '<a href="chapter-walk.html#missing">bad</a>',
                         '<p id="main">duplicate</p>',
                         '<a href="#main"><a href="#main">nested</a></a>',
                         '<script src="assets/missing.js"></script>']:
            damaged = dict(files)
            damaged['index.html'] = damaged['index.html'].replace('</main>', fragment + '</main>')
            with self.assertRaises(ValueError):
                site.validate_site(damaged)
        data = copy.deepcopy(shared(files))
        data['routes']['city'] = 'index.html'
        with self.assertRaises(ValueError):
            site.validate_site(files, data)

    def test_writer_regenerates_its_files_and_preserves_unrelated_data(self):
        files = package()
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / 'guide'
            site.write_site(target, files)
            (target / 'personal.txt').write_text('Keep me')
            site.write_site(target, files)
            assert (target / 'personal.txt').read_text() == 'Keep me'
            assert (target / 'chapter-walk.html').read_text() == files['chapter-walk.html']
            (target / 'assets/guide.js').unlink()
            (target / 'assets/guide.js').symlink_to(target / 'personal.txt')
            with self.assertRaises(ValueError):
                site.write_site(target, files)
            assert (target / 'personal.txt').read_text() == 'Keep me'
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp)
            (target / 'index.html').write_text('Unrelated index')
            with self.assertRaises(ValueError):
                site.write_site(target, files)
            assert (target / 'index.html').read_text() == 'Unrelated index'
            with self.assertRaises(ValueError):
                site.write_site(target, {'../outside.txt': 'bad'})

    def test_cli_isolated_mode_and_legacy_single_file(self):
        source = ('<div class="faqtext"><pre id="faqspan-0">Synthetic guide\n'
                  '* * * *\nWalkthrough\n* * * *\n'
                  '--------------------\n1. Example Journey\n--------------------\n'
                  'EXAMPLE CITY\n------------\nA synthetic paragraph for route validation.\n</pre></div>')
        script = Path(guide.__file__)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'source.txt').write_text(source)
            subprocess.run([sys.executable, '-I', str(script), str(root / 'source.txt'),
                            str(root / 'package'), '--format', 'site'], check=True, capture_output=True)
            assert (root / 'package/index.html').exists()
            assert (root / 'package/chapter-1-example-journey.html').exists()
            subprocess.run([sys.executable, '-I', str(script), str(root / 'source.txt'),
                            str(root / 'single.html')], check=True, capture_output=True)
            document = (root / 'single.html').read_text()
            assert 'EXAMPLE CITY' in document and '<script src=' not in document
            guide.validate_links(document)


if __name__ == '__main__':
    unittest.main()
