#!/usr/bin/env python3
"""Run screenshot-free menu recordings sequentially and summarize CPU telemetry.

Consumes Luna's DW3_MENU_PROFILE_CSV schema. GPU elapsed -1 is unavailable,
never interpreted as zero. Present intervals are callback intervals, not GPU
execution times or input-to-photon latency. Captures must run separately.
"""
import argparse
import csv
import json
import math
import os
from pathlib import Path
import statistics
import time
import replay_tests as replay

FIELDS = ('raster_us', 'staging_us', 'submit_us', 'wait_us', 'overlay_cpu_us',
          'present_interval_us', 'state_update_to_draw_us', 'upload_bytes', 'allocations', 'rebuilds',
          'dirty_pixels', 'dirty_copies')


def stats(values):
    values = sorted(values)
    return {'n': len(values), 'median': statistics.median(values),
            'p95': values[math.floor(.95 * (len(values) - 1))], 'max': max(values)}


def summarize(path):
    rows = list(csv.DictReader(path.open()))
    result = {'gpu_execution_time': 'unavailable', 'events': {}}
    for event in sorted({r['event'] for r in rows}):
        group = [r for r in rows if r['event'] == event]
        result['events'][event] = {key: stats([float(r[key]) for r in group])
                                   for key in FIELDS if key in group[0]}
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bin-us', type=Path, required=True)
    p.add_argument('--bin-eu', type=Path, required=True)
    p.add_argument('--scen-dir', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--runtime-us', type=Path, required=True)
    p.add_argument('--runtime-eu', type=Path, required=True)
    p.add_argument('--repeat', type=int, default=3)
    p.add_argument('--mode', action='append', choices=['reference','cpu','gpu'])
    p.add_argument('--only', action='append')
    args = p.parse_args()
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=True)
    replay.SCEN_DIR = args.scen_dir.resolve()
    for rec in replay.SCEN_DIR.glob('*.rec'):
        if any(l.startswith('shot ') for l in rec.read_text().splitlines()):
            p.error('performance recordings must not contain shot directives')
    xvfb, display = replay.start_xvfb(args.out)
    results = []
    try:
        for repeat in range(args.repeat):
            for mode in args.mode or ['reference', 'cpu', 'gpu']:
                for name, version in replay.SCENARIOS:
                    if args.only and name not in args.only:
                        continue
                    out = args.out / f'{repeat}-{mode}-{name}'
                    out.mkdir()
                    profile = out / 'profile.csv'
                    os.environ['DW3_MENU_COMPOSITION'] = mode
                    os.environ['DW3_MENU_PROFILE_CSV'] = str(profile)
                    rc, elapsed = replay.run_scenario(name, version,
                        getattr(args, f'bin_{version}').resolve(),
                        getattr(args, f'runtime_{version}').resolve(), display,
                        out, time.strftime('%Y%m%d-%H%M%S'), 180)
                    row = {'repeat':repeat,'mode':mode,'scenario':name,'rc':rc,
                           'wall_seconds':elapsed,'csv':str(profile)}
                    if profile.exists():
                        row['profile'] = summarize(profile)
                    results.append(row)
                    (args.out / 'summary.json').write_text(json.dumps(results, indent=2)+'\n')
                    print(f'{repeat} {mode} {name}: rc={rc}, profile={profile.exists()}', flush=True)
                    if rc != 0 or not profile.exists():
                        raise RuntimeError('run failed or profiling unavailable')
    finally:
        xvfb.terminate()
        xvfb.wait(timeout=5)


if __name__ == '__main__':
    main()
