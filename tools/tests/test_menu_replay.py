"""Validate new recording bounds and screenshot-free timing inputs."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / 'tools/replay/gen_menu_scenarios.py'


def generate(out, *args):
    subprocess.run([sys.executable, str(GEN), str(out), *args], check=True)
    return {p.name: p.read_text().splitlines() for p in out.glob('*.rec')}


def test_stress_inputs_match_capture_inputs(tmp_path):
    captures = generate(tmp_path / 'captures', '--cycles', '10')
    perf = generate(tmp_path / 'perf', '--cycles', '10', '--performance')
    assert len(captures) == 6
    for name, lines in captures.items():
        assert [l for l in lines if not l.startswith('shot ')] == perf[name]
        shots = [l.split() for l in lines if l.startswith('shot ')]
        assert len({s[2] for s in shots}) == len(shots)
        frames = [int(s[1]) for s in shots]
        assert frames == sorted(set(frames))
        end = int(lines[-1].split()[1])
        assert max(frames) < end
        assert sum('cycle-' in s[2] for s in shots) == 80


def test_odd_cycles_cannot_silently_disable_restart(tmp_path):
    result = subprocess.run([sys.executable, str(GEN), str(tmp_path), '--cycles', '1'],
                            capture_output=True, text=True)
    assert result.returncode != 0
    assert 'even' in result.stderr
