#!/usr/bin/env python3
"""Judge both landscape touch gestures: touch_verdict.py LOG NONCE LAYOUT, or --selftest."""

import sys
from pathlib import Path

import py_utf8
from touch_history import NO_LINES, judge
from touch_probe_layout import parse_layout
from touch_probe_trace import parse


def verdict(text, run, layout, *, cutout_swap=True):
    try:
        events, _ = parse(text, run, layout.controls)
    except ValueError as error:
        return [str(error)], ''
    if not events:
        return [f'{NO_LINES} for run {run}'], ''
    # A changed safe snapshot starts a new epoch even when dimensions are unchanged (180°).
    # Identical snapshots are harmless; never stitch two incomplete epochs into a good gesture.
    epochs = []
    for event in events:
        if not epochs or epochs[-1][0].epoch != event.epoch:
            epochs.append([])
        epochs[-1].append(event)
    bad, sides, rotations = [], {}, set()
    for epoch in epochs:
        safe = epoch[0].safe
        prefix = f'rotation {safe.rotation}: '
        if safe.rotation not in (1, 3) or safe.width <= safe.height:
            bad.append(prefix + 'gesture is not landscape')
            continue
        rotations.add(safe.rotation)
        if max(safe.left, safe.right) <= 0:
            bad.append(prefix + 'no horizontal safe inset')
        if cutout_swap and safe.left == safe.right:
            bad.append(prefix + 'no distinct cutout side')
        side = 'left' if safe.left > safe.right else 'right'
        sides.setdefault(safe.rotation, set()).add(side)
        bad.extend(prefix + reason for reason in judge(epoch, layout))
    if rotations != {1, 3}:
        bad.append('both landscape rotations 1 and 3 are required')
    if cutout_swap and len(sides) == 2 and (any(len(v) != 1 for v in sides.values()) or sides[1] == sides[3]):
        bad.append('cutout did not swap sides between landscape rotations')
    ids32 = {e.id32 for e in events if e.phase == 'down'}
    ids = {e.tid for e in events if e.phase == 'down'}
    summary = (f'both landscapes; {len(events)} events; {len(epochs)} safe epochs; '
               f'int-truncated ids {"distinct" if len(ids32) == len(ids) else "COLLIDE"}')
    return list(dict.fromkeys(bad)), summary


def main(argv, *, cutout_swap=True):
    py_utf8.enable()
    if argv[1:] == ['--selftest']:
        from touch_verdict_selftest import selftest
        return selftest()
    if len(argv) != 4:
        print(__doc__)
        return 2
    try:
        layout = parse_layout(Path(argv[3]).read_text(encoding='utf-8'))
        bad, summary = verdict(Path(argv[1]).read_text(encoding='utf-8', errors='replace'), argv[2],
                               layout, cutout_swap=cutout_swap)
    except (OSError, ValueError) as error:
        bad, summary = [str(error)], ''
    for reason in bad:
        print(f'[touch] FAIL: {reason}')
    if not bad:
        print(f'[touch] PASS: {summary}')
    return int(bool(bad))


if __name__ == '__main__':
    sys.exit(main(sys.argv))
