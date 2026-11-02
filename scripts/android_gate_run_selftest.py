#!/usr/bin/env python3
"""Exercise the real gate's assertions against a fake adb transport; no emulator/GPU proof."""

from contextlib import redirect_stdout
import io
import itertools
from pathlib import Path
import struct
import tempfile
from unittest.mock import patch
import zlib

from android_gate_run import APP, ROOT, exercise, last_safe
from android_touch_protocol_selftest import PROBE
from touch_probe_layout import parse_layout
from touch_verdict_selftest import gesture
from rumble_touch_verdict import actor_fixture, selftest as actor_selftest


def png(black=False):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 1, 8, 2, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(b'\0' + (b'\0' * 6 if black else b'\xff\0\0\0\0\xff'))) +
            chunk(b'IEND', b''))


class FakeAvd:
    def __init__(self, scenario):
        self.scenario, self.rotation, self.pids = scenario, 1, 0
        layout = parse_layout((ROOT / 'games/neon-rumble/controls/touch.txt').read_text(encoding='utf-8'))
        self.first, self.second = actor_fixture(gesture(1, layout)), actor_fixture(gesture(3, layout))
        self.calls = []

    def adb(self, *args, **kwargs):
        self.calls.append(args)
        # Model sensorLandscape: user_rotation has no effect, only injected acceleration does.
        if args[:4] == ('emu', 'sensor', 'set', 'acceleration') and self.scenario != 'sensor-ignored':
            self.rotation = 3 if args[-1].startswith('-') else 1
        if args[0] == 'logcat' and '-d' in args:
            backend = 'Gl' if self.scenario == 'gles' else 'Vulkan'
            adapter = 'other' if self.scenario == 'wrong-adapter' else 'SwiftShader Device'
            text = (f'[gpu] {adapter} | {backend} | BC: no\n' +
                    '\n'.join(self.first + (self.second if self.rotation == 3 else [])))
            if self.scenario == 'no-actor-movement':
                text = text.replace('player_x=236', 'player_x=200')
            if self.scenario == 'no-actor-attack':
                text = text.replace('jab_tick=21', 'jab_tick=0')
            return text
        if args[:2] == ('shell', 'pidof'):
            self.pids += 1
            return '102' if self.scenario == 'restart' and self.pids > 1 else '101'
        if args[:3] == ('shell', 'dumpsys', 'window'):
            component = APP + '/android.app.NativeActivity'
            if self.scenario == 'background':
                component = 'launcher/.Home'
            if self.scenario == 'prefix-package':
                component = APP + '.clone/.OtherActivity'
            if self.scenario == 'title-only':
                component = 'other.app/.Activity ' + APP
            return 'mCurrentFocus=Window{0 u0 ' + component + '}'
        if args[:2] == ('shell', 'getevent'):
            return PROBE.replace('0039', '003a') if self.scenario == 'no-protocol-b' else PROBE
        if args[:2] == ('shell', 'sh'):
            assert 'sendevent /dev/input/event4' in kwargs['data']
            assert kwargs['data'].count(' 3 57 -1') == 3
            return ''
        if args[:2] == ('exec-out', 'screencap'):
            assert kwargs.get('binary')
            return png(self.scenario == 'black')
        if args[:3] == ('shell', 'pm', 'clear'):
            return 'Success'
        return ''


def selftest():
    actor_selftest()
    cases = [('normal', None), ('gles', 'Vulkan is required'), ('wrong-adapter', 'SwiftShader'),
             ('restart', 'died/restarted'), ('background', 'lost foreground'),
             ('black', 'black screenshot'), ('no-protocol-b', 'protocol-B touchscreen'),
             ('prefix-package', 'lost foreground'), ('title-only', 'lost foreground'),
             ('sensor-ignored', 'no landscape [safe]'),
             ('no-actor-movement', 'actual jab'), ('no-actor-attack', 'actual jab')]
    with tempfile.TemporaryDirectory(prefix='android-gate-selftest-') as directory:
        for scenario, reason in cases:
            work = Path(directory) / scenario
            work.mkdir()
            avd = FakeAvd(scenario)
            with patch('android_gate_run.time.sleep'), \
                 patch('android_gate_run.time.monotonic', side_effect=itertools.count().__next__), \
                 redirect_stdout(io.StringIO()) as output:
                try:
                    exercise(avd, work, 'R')
                    assert reason is None, f'{scenario} was accepted'
                    assert 'touch PASS:' in output.getvalue()
                except RuntimeError as error:
                    assert reason and reason in str(error), (scenario, str(error))
            assert ('shell', 'am', 'force-stop', APP) in avd.calls
        trace = FakeAvd('normal').first[0]
        assert last_safe('[safe] broken run=OLD\n' + trace, 'R').rotation == 1
        assert last_safe(trace, 'OTHER') is None
    print(f'android-gate-selftest: PASS 1 good + {len(cases) - 1} broken transports (GPU, PID, focus, pixels, input, sensor)')
    return 0


if __name__ == '__main__':
    raise SystemExit(selftest())
