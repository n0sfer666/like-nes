"""Independent report fixtures, including malformed producer output and exact budget boundaries."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def selftest(judge):
    base = dict(schema=1, frames=3600, intervals=3599, misses=0, work_p50_ns=8000000,
                work_p99_ns=12000000, interval_p50_ns=16666667, interval_p99_ns=16666667,
                underruns=0, save_ms=2.5, save_ok=True, complete=True)
    cases = []

    def fixture(name, changes, passes=False, budget=False):
        cases.append((name, json.dumps({**base, **changes}), budget, passes))

    fixture('ordinary report', {}, True)
    fixture('budget report', {}, True, True)
    fixture('large work and underruns are reported, not an invented threshold',
            dict(work_p99_ns=1000000000, underruns=99999), True, True)
    fixture('zero work and measured zero save are valid', dict(work_p50_ns=0, save_ms=0), True)
    fixture('unknown metadata', dict(device='test device', build='abc'), True)
    fixture('marker within JSON metadata', dict(device='test [frame-stats] device'), True)
    fixture('nonfinite metadata', dict(extra=float('nan')))
    fixture('infinite metadata', dict(extra=float('inf')))
    fixture('schema only permits a slow measured run', dict(misses=999), True)
    fixture('exactly one percent fails', dict(intervals=99, misses=1), False, True)
    fixture('below one percent passes', dict(intervals=100, misses=1), True, True)
    fixture('above one percent fails', dict(intervals=98, misses=1), False, True)
    fixture('small capture valid', dict(frames=2, intervals=1, work_p50_ns=0, work_p99_ns=0), True)
    fixture('small work sample inconsistent', dict(frames=2, intervals=1))
    fixture('small cadence sample inconsistent', dict(intervals=2, interval_p99_ns=33333333))
    fixture('budget arithmetic does not wrap', dict(intervals=(1 << 64) - 1, frames=(1 << 64) - 1,
                                                  misses=(1 << 64) - 1), False, True)
    fixture('empty frames', dict(frames=0))
    fixture('empty intervals', dict(intervals=0))
    fixture('intervals exceed frames', dict(frames=2, intervals=3))
    fixture('unordered work percentiles', dict(work_p50_ns=13000000))
    fixture('unordered interval percentiles', dict(interval_p50_ns=20000000))
    fixture('empty interval percentile', dict(interval_p50_ns=0))
    fixture('incomplete capture', dict(complete=False))
    fixture('missing successful save', dict(save_ok=False))
    for key in base:
        report = base.copy()
        del report[key]
        cases.append(('missing ' + key, json.dumps(report), False, False))
    for key in ('schema', 'frames', 'intervals', 'misses', 'work_p50_ns', 'work_p99_ns',
                'interval_p50_ns', 'interval_p99_ns', 'underruns'):
        for value in (True, -1, 1.25, str(base[key]), None, 1 << 64):
            fixture(f'invalid {key}={value}', {key: value})
    for value in (True, -0.1, '2.5', None, float('nan'), float('inf'), -float('inf')):
        fixture(f'invalid save_ms={value}', dict(save_ms=value))
    for key in ('complete', 'save_ok'):
        for value in (1, 'true', None):
            fixture(f'invalid {key}={value}', {key: value})
    for value in (0, 2, '1', None):
        fixture('wrong schema', dict(schema=value))
    valid = json.dumps(base)
    cases += [
        ('tagged report amid logs', 'before\nI/game [frame-stats] ' + valid + '\nafter', True, True),
        ('multiple reports', '[frame-stats] ' + valid + '\n[frame-stats] ' + valid, False, False),
        ('duplicate required key', valid[:-1] + ', "underruns":0}', False, False),
        ('duplicate metadata key', valid[:-1] + ', "device":"A", "device":"B"}', False, False),
        ('nested duplicate key', valid[:-1] + ', "extra":{"x":1,"x":2}}', False, False),
        ('malformed JSON', '{', False, False),
        ('nonobject JSON', '[]', False, False),
        ('missing JSON', '', False, False),
        ('overflowed JSON float', valid.replace('2.5', '1e400'), False, False),
        ('overflowed metadata float', valid[:-1] + ', "extra":1e400}', False, False),
    ]
    failures = 0
    for name, text, budget, want in cases:
        try:
            judge(text, budget)
            passed = True
        except ValueError:
            passed = False
        if passed != want:
            failures += 1
            print(f'frame-stats selftest: FAIL {name}: accepted={passed}, expected={want}')
    command = [sys.executable, str(Path(__file__).with_name('frame_stats_verdict.py'))]
    cli_cases = [
        ('stdin', ['--budget'], valid, 0, 'frame-stats: PASS'),
        ('missing underruns CLI', [], json.dumps({k: v for k, v in base.items() if k != 'underruns'}),
         1, 'underruns'),
        ('exact budget CLI', ['--budget'], json.dumps({**base, 'intervals': 99, 'misses': 1}),
         1, 'not below 1%'),
        ('duplicate CLI', [], valid[:-1] + ', "underruns":0}', 1, 'duplicate key'),
        ('help', ['--help'], '', 0, '--budget'),
    ]
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory, 'frame stats.json')
        path.write_text('log\n[frame-stats] ' + valid + '\n', encoding='utf-8')
        cli_cases.append(('file with spaces', ['--budget', str(path)], '', 0, 'frame-stats: PASS'))
        for name, args, stdin, code, expected in cli_cases:
            result = subprocess.run(command + args, input=stdin, capture_output=True, encoding='utf-8')
            if result.returncode != code or expected not in result.stdout + result.stderr:
                failures += 1
                print(f'frame-stats selftest: FAIL {name}: exit={result.returncode}, expected={code}')
    count = len(cases) + len(cli_cases)
    print(f"frame-stats selftest: {'PASS' if not failures else 'FAIL'} {count} controls")
    return int(bool(failures))
