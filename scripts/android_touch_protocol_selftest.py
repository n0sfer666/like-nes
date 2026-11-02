#!/usr/bin/env python3
"""Independent axis endpoint/orientation controls for the Android sendevent driver."""

import sys

from android_touch_protocol import Axis, Device, devices, gesture_script, raw_point
from touch_probe_layout import parse_layout
from touch_probe_trace import Safe

PROBE = '''add device 1: /dev/input/event0
  name: "keys"
  events: KEY (0001): 0074
add device 2: /dev/input/event4
  name: "virtio multi touch"
  events:
    KEY (0001): 014a
    ABS (0003): 002f : value 0, min 0, max 9, fuzz 0
                0035 : value 0, min 100, max 1099, fuzz 0
                0036 : value 0, min 200, max 2199, fuzz 0
                0039 : value -1, min 0, max 65535, fuzz 0
'''


def selftest():
    failures = []

    def check(ok, why):
        if not ok:
            failures.append(why)

    candidates = devices(PROBE)
    check(len(candidates) == 1, 'selects protocol B, ignores a keyboard')
    if not candidates:
        return 1
    device = candidates[0]
    check(device == Device('/dev/input/event4', Axis(100, 1099), Axis(200, 2199),
                           Axis(0, 9), Axis(0, 65535), True), 'axes and BTN_TOUCH read from numeric getevent')
    left = Safe(800, 400, 2, 20, 0, 0, 0, 1)
    right = Safe(800, 400, 2, 0, 0, 20, 0, 3)
    check(raw_point(device, left, 0, 0) == (1099, 200), 'rotation90 top-left endpoint')
    check(raw_point(device, left, 399.5, 199.5) == (100, 2199), 'rotation90 bottom-right endpoint')
    check(raw_point(device, right, 0, 0) == (100, 2199), 'rotation270 top-left endpoint')
    check(raw_point(device, right, 399.5, 199.5) == (1099, 200), 'rotation270 bottom-right endpoint')
    check(raw_point(device, left, 50, 70) == (748, 450), 'rotation90 interior/nonzero raw min')
    check(raw_point(device, right, 50, 70) == (451, 1949), 'rotation270 interior/nonzero raw min')
    layout = parse_layout('stick|48\nbutton|a|bottom-right|68|68|30')
    for safe in (left, right):
        script = gesture_script(device, safe, layout)
        check('sendevent /dev/input/event4 3 57 101' in script and
              'sendevent /dev/input/event4 3 57 102' in script, 'two tracking ids')
        check(script.count('sendevent /dev/input/event4 3 57 -1') == 3, 'join tap and both slots released')
        check(script.count('sendevent /dev/input/event4 0 0 0') == 7, 'seven SYN_REPORT frames')
        check('sendevent /dev/input/event4 1 330 1' in script and
              'sendevent /dev/input/event4 1 330 0' in script, 'BTN_TOUCH begin/end')
    for broken in (PROBE.replace('max 9', 'max 0'), PROBE.replace('0039', '003a'),
                   PROBE.replace('max 1099', 'max 100'), PROBE.replace('max 65535', 'max 1')):
        check(not devices(broken), 'rejects incomplete/tiny protocol-B axes')
    for xy in ((-1, 0), (400, 0), (0, 200), (float('nan'), 0)):
        try:
            raw_point(device, left, *xy)
            check(False, f'refused point {xy}')
        except ValueError:
            pass
    for failure in failures:
        print(f'android-touch-protocol: FAIL {failure}')
    print(f'android-touch-protocol: {"FAIL" if failures else "PASS"} (endpoints, interior, '
          'rotation1/3, two slots, release, malformed devices)')
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(selftest())
