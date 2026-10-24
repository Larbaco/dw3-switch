"""Regression cases for the guide's entity resolver (no copyrighted fixture)."""
import json
import sys
from html.parser import HTMLParser
from pathlib import Path

import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import guide_to_html as guide


def section(sid, chapters=(), intro=()):
    return dict(id=sid, title=sid.replace('-', ' ').title(), chapters=list(chapters), intro=list(intro))


def chapter(cid, name, items=()):
    return dict(id=cid, title=name, items=list(items))


def fixture():
    return [
        section('partner-digimon', [chapter('partner', '1. Testmon')]),
        section('digimon-evolutions', [chapter('evolution', 'Testmon', [
            ('h', 'techniques', 'TECHNIQUES'),
            ('pre', '   Fire+\n      Type: [Fire], Mag   Level Learned: 1\n'
             '   Fire\n      Type: [Fire], Mag   Level Learned: 2\n'
             '   Fire\n      Type: [Fire], Mag   Level Learned: 3'),
        ])]),
        section('walkthrough', [chapter('walk', '1. Walk', [
            ('h', 'city', 'TEST CITY - INN'),
            ('pre', 'Enemies:\n   Testmon      6 EXP    10 BIT    Power Charge\n\n'
             'Items:\n   Power Charge x2\n   Attack\n\n'
             'Testmon uses Fire+ in Test City. Attack the enemy. Fire is hot.'),
        ])]),
        section('card-battle', intro=[('h', 'booster', 'BOOSTER 1b'), ('pre', 'Testmon\n\nPower Charge')]),
    ]


class GuideLinksTest(unittest.TestCase):
    def test_routes_keep_repeated_visits_and_semantic_area_headings(self):
        sections = fixture()
        walk = sections[2]['chapters'][0]
        walk['items'] += [('h', 'city-return', 'TEST CITY - INN'), ('pre', 'Return to Test City.')]
        document = guide.render('', sections)
        return_visit = document.split('href="#city-return">', 1)[1].split('</a>', 1)[0]
        assert return_visit.endswith(' · visita 2')
        assert '<section class="guide-area" aria-labelledby="city-return"><h4' in document
        assert '<h4 id="city-return" class="subheading loc"' in document
        guide.validate_links(document)

    def test_npc_and_quest_roles_require_explicit_source_records(self):
        sections = fixture()
        sections.append(section('side-quests', [chapter('test-quest', '3. Test Quest')]))
        sections[2]['chapters'][0]['items'].append(('pre',
            ' ------------------------------\n'
            '|CARD BATTLE: Card Master       |\n'
            ' ------------------------------\n'
            '|Testmon   6 EXP 10 BIT         |\n'
            ' ------------------------------\n\n'
            'An Unnamed Person talks about an Imaginary Quest.'))
        entities = guide.EntityIndex(sections)
        assert set(entities.entries['card master']['kinds']) == {'NPC'}
        assert set(entities.entries['test quest']['kinds']) == {'Quest'}
        assert 'unnamed person' not in entities.entries
        assert 'imaginary quest' not in entities.entries
        assert '<a ' in entities.link('Meet Card Master for Test Quest.', 'walk')
        guide.validate_links(guide.render('', sections))

    def test_standalone_assets_and_accessible_controls(self):
        document = guide.render('', fixture())
        assert '%%' not in document
        assert '<script src=' not in document and '<link rel="stylesheet"' not in document
        assert 'role="combobox"' in document
        assert 'id="search-scope"' in document and 'id="entity-kind"' in document
        assert '<th scope="col"' in document
        assert '<noscript>' in document
        # Supplementary definitions remain in the file when collapsed.
        many = fixture()
        many[0]['chapters'] += [chapter('extra-a', 'Testmon'), chapter('extra-b', 'Testmon')]
        generated = guide.render('', many)
        assert 'Mais 2 fontes de Digimon' in generated
        assert 'href="#extra-b"' in generated
        guide.validate_links(generated)

    def test_palette_entities_keep_all_roles_and_destinations(self):
        sections = fixture()
        entities = guide.EntityIndex(sections)
        items = json.loads(guide.palette_json(sections, entities))
        results = {item['id']: item for item in items if item['k'] == 'ent'}
        assert set(results) == {entry['id'] for entry in entities.entries.values()}
        assert not any(item['k'] == 'dig' for item in items)
        roles = set()
        for entry in entities.entries.values():
            result = results[entry['id']]
            assert result['t'] == entry['name']
            assert result['g'].split(' / ') == sorted(entry['kinds'])
            roles.update(entry['kinds'])
        assert roles == {'Digimon', 'Inimigo', 'Item', 'Local', 'Técnica', 'Booster', 'Carta'}
        assert results[entities.entries['testmon']['id']]['g'] == 'Carta / Digimon / Inimigo'
        assert results[entities.entries['power charge']['id']]['g'] == 'Carta / Item'
        checked = guide.validate_links(guide.render('', sections))
        assert all(item['id'] in checked.ids for item in items)


    def test_homonyms_keep_every_definition_and_role(self):
        entities = guide.EntityIndex(fixture())
        entry = entities.entries['testmon']
        assert {t for t, _ in entry['kinds']['Digimon']} == {'partner', 'evolution'}
        assert set(entry['kinds']) == {'Digimon', 'Inimigo', 'Carta'}
        assert set(entities.entries['power charge']['kinds']) == {'Item', 'Carta'}
        assert 'inn' in entities.entries  # explicit location, but no prose auto-link
        assert entities.link('The inn has room.', 'walk') == 'The inn has room.'
        assert entities.link('Attack the enemy. Fire is hot.', 'walk') == 'Attack the enemy. Fire is hot.'
        assert '<a ' in entities.link('Attack', 'walk')  # exact structured cell


    def test_markup_escaping_boundaries_and_ascii_survive(self):
        entities = guide.EntityIndex(fixture())
        text = '<a title="Testmon" href="#partner">Testmon</a> <span title="Testmon">Testmon</span>'
        linked = entities.link(text, 'walk')
        assert linked.startswith('<a title="Testmon" href="#partner">Testmon</a>')
        assert '<span title="Testmon"><a ' in linked
        assert entities.link('SuperTestmon Testmon2 Testmon-like', 'walk') == 'SuperTestmon Testmon2 Testmon-like'
        original = ' | Testmon    & <unknown> |\n +-----------------------+'
        linked = entities.link(guide.esc(original), 'walk')

        class Text(HTMLParser):
            def __init__(self):
                super().__init__()
                self.text = ''

            def handle_data(self, data):
                self.text += data

        parser = Text()
        parser.feed(linked)
        assert parser.text == original


    def test_escaped_entity_name_and_stable_collision_ids(self):
        one = section('partner-digimon', [chapter('a', 'A & Bmon'), chapter('b', 'A-Bmon')])
        entities = guide.EntityIndex([one])
        assert '<a ' in entities.link(guide.esc('A & Bmon'), 'a')
        before = {k: e['id'] for k, e in entities.entries.items()}
        one['chapters'].reverse()
        after = guide.EntityIndex([one])
        assert before == {k: e['id'] for k, e in after.entries.items()}
        assert len(set(before.values())) == 2


    def test_render_integrity_backlinks_and_duplicate_techniques(self):
        document = guide.render('', fixture())
        checked = guide.validate_links(document)
        assert 'evolution--tech-fire' in checked.ids
        assert 'evolution--tech-fire-2' in checked.ids
        assert 'evolution--tech-fire-3' in checked.ids
        index = document.split('<h2>Índice de entidades</h2>', 1)[1]
        assert 'Referências:' in index
        assert 'href="#walk--source-' in index
        assert guide.render('', fixture()) == document


    def test_integrity_rejects_broken_html(self):
        for document in [
            '<div id="x"></div><p id="x"></p>',
            '<a href="#missing">missing</a>',
            '<a href="#x"><a href="#x">nested</a></a><p id="x"></p>',
        ]:
            with unittest.TestCase().assertRaises(ValueError):
                guide.validate_links(document)

if __name__ == '__main__':
    unittest.main()
