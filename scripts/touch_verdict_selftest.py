"""Positive traces and independent broken histories for the shared mobile gate."""

from pathlib import Path
import subprocess
import sys
import tempfile

from touch_probe_layout import parse_layout
from touch_probe_trace import Safe
from touch_verdict import verdict

LAYOUT = 'stick | 48\nbutton | a | bottom-right | 68 | 68 | 50'


def safe(rotation):
    left, right = (44, 0) if rotation == 1 else (0, 44)
    return Safe(2532, 1170, 3, left, 0, right, 21, rotation)


def safe_line(s):
    return ('[safe] ' + ' '.join(f'{k}={v}' for k, v in vars(s).items()) + ' run=R')


def gesture(rotation, layout=None):
    layout = layout or parse_layout(LAYOUT)
    s = safe(rotation)
    fx, fy = layout.buttons['a'].probe(s)

    def event(phase, tid, stick, fire, x, y, knob=230):
        owners = {'stick': stick, **{name: fire if name == 'a' else -1 for name in layout.buttons}}
        fields = ' '.join(f'{k}={v}' for k, v in owners.items())
        return (f'[touch] {phase} id={tid} id32={tid} x={x} y={y} {fields} '
                f'knob_x={knob} knob_y=190 run=R')

    return [safe_line(s), event('down', 11, 11, -1, 200, 190, 200),
            event('move', 11, 11, -1, 230, 190), event('down', 22, 11, 22, fx, fy),
            event('up', 22, 11, -1, fx, fy), event('up', 11, -1, -1, 230, 190)]


def selftest():
    layout = parse_layout(LAYOUT)
    first, second = gesture(1), gesture(3)
    good = '\n'.join(first + second)
    passes = [good, good.replace('id32=22', 'id32=11'),
              '\n'.join(first + first + second),
              good + '\n[safe] fb_w=broken run=OTHER\n[touch] broken run=OTHER',
              '\n'.join(first[:4] + first[4:5] + first[3:] + second)]
    # Last positive reuses a released native id; it is not two simultaneous fingers sharing one.
    fixtures = [
        ('empty', '', 'no [touch] lines'),
        ('foreign nonce', good.replace('run=R', 'run=OLD'), 'no [touch] lines'),
        ('one orientation', '\n'.join(first), 'both landscape'),
        ('one finger', '\n'.join([first[i] for i in (0, 1, 2, 5)] + second), 'only one touch'),
        ('collapsed id', good.replace('id=22', 'id=11').replace('a=22', 'a=11'), 'two touches share'),
        ('fire not owned', good.replace('a=22', 'a=-1'), 'never held at once'),
        ('no move', '\n'.join(e for e in (first + second) if ' move ' not in e), 'never moved'),
        ('move not routed', good.replace('knob_x=230', 'knob_x=200'), 'never moved'),
        ('stationary move', good.replace('x=230 y=', 'x=200 y='), 'never moved'),
        ('lost stick', good.replace(
            'stick=11 a=-1 knob_x=230', 'stick=-1 a=-1 knob_x=230'), 'releasing fire'),
        ('unclosed id', good.replace('[touch] up id=11', '[touch] move id=11'), 'still held'),
        ('stale owner', good.replace('stick=-1 a=-1', 'stick=11 a=-1'), 'inactive touch'),
        ('no independent up', good.replace('[touch] up id=22', '[touch] move id=22'),
         'fire was never released independently'),
        ('same cutout side', good.replace('left=0 top=0 right=44', 'left=44 top=0 right=0'),
         'cutout did not swap'),
        ('no cutout', good.replace('left=44', 'left=0').replace('right=44', 'right=0'),
         'no distinct cutout'),
        ('bad scale', good.replace('scale=3', 'scale=0'), 'invalid framebuffer'),
        ('bad safe', good.replace('left=44', 'left=900'), 'insets consume'),
        ('nan', good.replace('x=230 y=', 'x=nan y='), 'nonfinite'),
        ('missing knob', good.replace('knob_x=230 ', ''), 'knob_x'),
        ('duplicate field', good.replace('id32=22', 'id32=22 id32=22'), 'duplicate'),
        ('missing safe', '\n'.join(first[1:] + second), 'before [safe]'),
        ('outside button', good.replace('x=776.0', 'x=840.0'), 'outside its layout'),
        ('bad id32', good.replace('id32=22', 'id32=2147483648'), 'invalid id32'),
        ('inactive move', '\n'.join([first[0], first[2]] + first[1:] + second), 'without an active'),
    ]
    # A silent intermediate geometry change must not join incomplete gestures into a pass.
    interrupted = first[:3] + [safe_line(safe(3)), safe_line(safe(1))] + first[3:] + second
    fixtures.append(('interrupted epoch', '\n'.join(interrupted), 'only one touch'))
    # Neither attempt proves the whole gesture. Reusing native ids must not merge lifetimes.
    first_partial = first[:4] + [first[5], first[4].replace('stick=11', 'stick=-1')]
    second_partial = [e.replace('x=230 y=', 'x=200 y=').replace('knob_x=230', 'knob_x=200')
                      for e in first[1:]]
    for reuse in (True, False):
        partial = second_partial if reuse else [e.replace('11', '33').replace('22', '44')
                                               for e in second_partial]
        fixtures.append((f'split evidence reuse={reuse}', '\n'.join(first_partial + partial + second),
                         'no single touch pair'))
    fails = []
    for i, trace in enumerate(passes):
        bad, _ = verdict(trace, 'R', layout)
        if bad:
            fails.append(f'positive {i}: {bad}')
    for name, trace, reason in fixtures:
        bad, _ = verdict(trace, 'R', layout)
        if not any(reason in message for message in bad):
            fails.append(f'{name}: expected {reason!r}, got {bad}')
    symmetric = good.replace('left=44 top=0 right=0', 'left=44 top=0 right=44')
    symmetric = symmetric.replace('left=0 top=0 right=44', 'left=44 top=0 right=44')
    # Adjust the first gesture from the old center776 to the symmetric center732/probe683.
    symmetric = symmetric.replace('x=776.0 y=252.0', 'x=683.0 y=301.0')
    if verdict(symmetric, 'R', layout, cutout_swap=False)[0]:
        fails.append('iOS symmetric nonzero safe area was refused')
    if not any('no distinct cutout' in e for e in verdict(symmetric, 'R', layout)[0]):
        fails.append('Android symmetric cutout overlay was accepted')
    if not verdict(good.replace('left=44', 'left=0').replace('right=44', 'right=0'),
                   'R', layout, cutout_swap=False)[0]:
        fails.append('iOS zero safe area was accepted')
    bad_layouts = [LAYOUT + '\nstick|48', LAYOUT.replace('| a |', '| f |'),
                   LAYOUT.replace('50', 'nan'), LAYOUT.replace('bottom-right', 'middle'),
                   LAYOUT.replace('68', '-1'), '\ufeff' + LAYOUT]
    for text in bad_layouts:
        try:
            parse_layout(text)
            fails.append(f'bad layout accepted: {text!r}')
        except ValueError:
            pass
    root = Path(__file__).resolve().parent.parent
    with tempfile.TemporaryDirectory(prefix='touch-verdict-') as work:
        trace = Path(work) / 'touch.log'
        trace.write_text(symmetric, encoding='utf-8')
        for script, extra, expected in [('ios_touch_verdict.py', [], 0),
                                        ('touch_verdict.py', [str(root / 'example_ugly_game/assets/touch.txt')], 1)]:
            result = subprocess.run([sys.executable, str(root / 'scripts' / script), str(trace), 'R', *extra],
                                    capture_output=True, text=True, encoding='utf-8')
            if result.returncode != expected or ('PASS' if expected == 0 else 'FAIL') not in result.stdout:
                fails.append(f'CLI {script}: {result.returncode}, {result.stdout}, {result.stderr}')
    for path in ('example_ugly_game/assets/touch.txt', 'games/neon-rumble/controls/touch.txt'):
        actual = parse_layout((root / path).read_text(encoding='utf-8'))
        bad, _ = verdict('\n'.join(gesture(1, actual) + gesture(3, actual)), 'R', actual)
        if bad:
            fails.append(f'{path}: {bad}')
    for failure in fails:
        print(f'[touch] selftest FAIL: {failure}')
    print(f'[touch] selftest: {len(passes) + 3} pass + {len(fixtures) + 2} broken traces + '
          f'{len(bad_layouts)} broken layouts + 2 CLI policies; {len(fails)} failure(s)')
    return int(bool(fails))
