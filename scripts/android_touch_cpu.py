#!/usr/bin/env python3
"""Replay the actual sendevent generator through an independent raw decoder and real MobileRumble."""

from pathlib import Path
import subprocess
import sys

from android_touch_protocol import Axis, Device, gesture_script
from touch_probe_layout import parse_layout
from touch_probe_trace import Safe
from touch_verdict import verdict
from rumble_touch_verdict import actor_verdict


def decode(script, device, safe):
    """Model Linux protocol-B frames and AOSP's forward rotation, independently of raw_point."""
    slot, slots, previous = 0, {}, {}
    out = []
    for line in script.splitlines():
        fields = line.split()
        if fields and fields[0] == 'sleep':
            out.append(f'frames {round(float(fields[1]) * 60)}')
            continue
        if not fields or fields[0] != 'sendevent':
            continue
        _, _, kind, code, value = fields
        kind, code, value = int(kind), int(code), int(value)
        if kind == 3:
            if code == 47:
                slot = value
            else:
                slots.setdefault(slot, {})[code] = value
        if (kind, code) != (0, 0):
            continue
        for sid, state in slots.items():
            before = previous.get(sid, {})
            held, was_held = state.get(57, -1) >= 0, before.get(57, -1) >= 0
            if not held and not was_held:
                continue
            if held and was_held and state == before:
                continue
            rx, ry = state[53], state[54]
            nx = (rx - device.x.low) / (device.x.high - device.x.low) * (safe.fb_h - 1)
            ny = (ry - device.y.low) / (device.y.high - device.y.low) * (safe.fb_w - 1)
            if safe.rotation == 1:
                px, py = ny, safe.fb_h - 1 - nx
            else:
                px, py = safe.fb_w - 1 - ny, nx
            phase = 'up' if not held else 'move' if was_held else 'down'
            out.append(f'{phase} {sid} {px / safe.scale:.6f} {py / safe.scale:.6f}')
        previous = {sid: dict(state) for sid, state in slots.items()}
    return out


def check(exe, assets):
    root = Path(__file__).resolve().parent.parent
    layout = parse_layout((root / 'games/neon-rumble/controls/touch.txt').read_text(encoding='utf-8'))
    device = Device('/dev/input/event4', Axis(100, 32867), Axis(200, 32967), Axis(0, 9), Axis(0, 65535), True)
    commands = []
    for rotation in (1, 3):
        s = Safe(2532, 1170, 3, 44 if rotation == 1 else 0, 0, 44 if rotation == 3 else 0, 0, rotation)
        commands.append(f'safe {s.fb_w} {s.fb_h} {s.left} {s.top} {s.right} {s.bottom} {s.scale} {s.rotation}')
        commands.extend(decode(gesture_script(device, s, layout), device, s))
    result = subprocess.run([exe, assets, 'probe'], input='\n'.join(commands) + '\n',
                            capture_output=True, text=True, encoding='utf-8', timeout=60)
    bad, summary = verdict(result.stdout, 'cpu', layout)
    bad += actor_verdict(result.stdout, 'cpu', layout)
    if result.returncode or 'mobile-cpu: PASS' not in result.stdout or bad:
        raise RuntimeError(f'raw axes -> real MobileRumble failed: {bad}\n{result.stdout}\n{result.stderr}')
    print(f'android-touch-cpu: PASS raw protocol B -> actual source -> shared verdict; {summary}')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('usage: android_touch_cpu.py MOBILE_CPU ASSETS')
        sys.exit(2)
    try:
        check(*sys.argv[1:])
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        print(f'android-touch-cpu: FAIL {error}')
        sys.exit(1)
