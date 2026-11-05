#!/usr/bin/env python3
"""Validate measured frame statistics; --budget applies the owner's <1% rule."""
import argparse
import json
import math
import sys
from pathlib import Path

UINT64_MAX = (1 << 64) - 1
COUNTS = ('frames', 'intervals', 'misses', 'work_p50_ns', 'work_p99_ns',
          'interval_p50_ns', 'interval_p99_ns', 'underruns')


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f'duplicate key: {key}')
        result[key] = value
    return result


def nonfinite(token):
    raise ValueError(f'nonfinite JSON number: {token}')


def finite_float(token):
    value = float(token)
    if not math.isfinite(value):
        nonfinite(token)
    return value


def report_from_text(text):
    # logcat and simulator logs may put their own timestamp/process prefix before our tag.
    raw = text.strip()
    if not raw.startswith('{'):
        reports = [line.split('[frame-stats]', 1)[1].strip()
                   for line in text.splitlines() if '[frame-stats]' in line]
        if len(reports) > 1:
            raise ValueError('expected exactly one frame-stats report')
        raw = reports[0] if reports else raw
    try:
        report = json.loads(raw, object_pairs_hook=unique_object, parse_constant=nonfinite,
                            parse_float=finite_float)
    except json.JSONDecodeError as error:
        raise ValueError(f'invalid report JSON: {error.msg}') from error
    if type(report) is not dict:
        raise ValueError('report must be a JSON object')
    return report


def validate(report, budget=False):
    if type(report.get('schema')) is not int or report['schema'] != 1:
        raise ValueError('schema must be integer 1')
    for key in COUNTS:
        value = report.get(key)
        if type(value) is not int or not 0 <= value <= UINT64_MAX:
            raise ValueError(f'{key} must be a uint64 integer')
    if report.get('complete') is not True:
        raise ValueError('measurement is incomplete')
    if report.get('save_ok') is not True:
        raise ValueError('no successful measured save')
    save_ms = report.get('save_ms')
    try:
        valid_save = type(save_ms) in (int, float) and save_ms >= 0 and math.isfinite(save_ms)
    except OverflowError:
        valid_save = False
    if not valid_save:
        raise ValueError('save_ms must be a finite nonnegative number')
    if not report['frames'] or not 0 < report['intervals'] <= report['frames']:
        raise ValueError('empty or inconsistent frame/interval counts')
    if not 0 < report['interval_p50_ns'] <= report['interval_p99_ns']:
        raise ValueError('invalid interval percentiles')
    if report['work_p50_ns'] > report['work_p99_ns']:
        raise ValueError('invalid work percentiles')
    if report['frames'] <= 2 and report['work_p50_ns'] != report['work_p99_ns']:
        raise ValueError('inconsistent small work sample percentiles')
    if report['intervals'] <= 2 and report['interval_p50_ns'] != report['interval_p99_ns']:
        raise ValueError('inconsistent small interval sample percentiles')
    # Python integers keep this exact even for uint64 counters. No work/underrun limit is implied.
    if budget and 100 * report['misses'] >= report['intervals'] + report['misses']:
        raise ValueError('missed intervals are not below 1%')
    return report


def validate_text(text, budget=False):
    return validate(report_from_text(text), budget)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report', nargs='?', default='-', help='JSON or log file; - reads stdin')
    parser.add_argument('--budget', action='store_true', help='also require <1%% missed intervals')
    parser.add_argument('--selftest', action='store_true')
    args = parser.parse_args()
    if args.selftest:
        from frame_stats_verdict_selftest import selftest
        return selftest(validate_text)
    try:
        text = sys.stdin.read() if args.report == '-' else Path(args.report).read_text(encoding='utf-8')
        validate_text(text, args.budget)
    except (OSError, UnicodeError, ValueError) as error:
        print(f'frame-stats: FAIL {error}', file=sys.stderr)
        return 1
    print('frame-stats: PASS schema and <1% missed intervals' if args.budget else 'frame-stats: PASS schema')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
