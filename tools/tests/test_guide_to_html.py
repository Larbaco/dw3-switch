"""Regression cases for the guide's entity resolver (no copyrighted fixture)."""
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
