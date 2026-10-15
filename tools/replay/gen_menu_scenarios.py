#!/usr/bin/env python3
"""Extend existing title replays with asset-free native Options coverage.

Use with replay_tests.sh --scen-dir OUTPUT. --performance removes screenshots
so capture subprocesses cannot contaminate frame-time samples.
"""
import argparse
from pathlib import Path
from gen_scenarios import EU_LANGS, us_title, eu_language


def extend(rec, performance=False):
    start = int(rec.lines.pop().split()[1])
    events = [(0, 0x40, 'title-options-focus'), (20, 0x4000, 'options-open'),
              (100, 0x4000, 'widescreen-off'), (130, 0x4000, 'widescreen-on'),
              (160, 0x40, 'cursor-turbo'), (190, 0x4000, 'turbo-3x'),
              (220, 0x4000, 'turbo-4x'), (250, 0x4000, 'turbo-off'),
              (280, 0x4000, 'turbo-2x'), (310, 0x40, 'cursor-restart'),
              (340, 0x4000, 'restart-on'), (370, 0x4000, 'restart-off'),
              (400, 0x4000, 'restart-on-again'), (430, 0x10, 'cursor-up-turbo'),
              (460, 0x10, 'cursor-up-widescreen'), (490, 0x1000, 'return-title'),
              (520, 0x4000, 'reopen-options'), (550, 0x1000, 'return-again'),
              (580, 0xC09, 'restart-chord')]
    for offset, mask, label in events:
        rec.press(start + offset, mask)
        rec.shot(start + offset + 15, label)
    rec.shot(start + 680, 'after-restart')
    rec.end(start + 700)
    if performance:
        rec.lines = [line for line in rec.lines if not line.startswith('shot ')]
    return rec


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('out', type=Path)
    p.add_argument('--performance', action='store_true')
    args = p.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    scenarios = [('us-title', us_title())] + [(f'eu-lang-{l}', eu_language(i)) for i, l in enumerate(EU_LANGS)]
    for name, rec in scenarios:
        (args.out / f'{name}.rec').write_text(extend(rec, args.performance).text())


if __name__ == '__main__':
    main()
