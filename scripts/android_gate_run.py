#!/usr/bin/env python3
"""Boot an owned Android AVD, build Neon Rumble, inject protocol-B gestures, judge evidence."""

import argparse
from pathlib import Path
import re
import subprocess
import sys
import time
import uuid

from android_touch_protocol import devices, gesture_script
from png_mean import mean_rgb
from touch_probe_layout import parse_layout
from touch_probe_trace import safe_record
from touch_verdict import verdict
from rumble_touch_verdict import actor_verdict

APP = 'com.likenes.neonrumble'
ROOT = Path(__file__).resolve().parent.parent


def last_safe(text, run):
    latest = None
    for line in text.splitlines():
        if '[safe] ' not in line:
            continue
        words = line.split('[safe] ', 1)[1].split()
        if f'run={run}' not in words:
            continue
        fields = dict(word.split('=', 1) for word in words)
        if fields.get('run') == run:
            latest = safe_record(fields)
    return latest


def wait_safe(avd, run, rotation, work):
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        text = avd.adb('logcat', '-d', '-v', 'raw', '-s', 'like-nes')
        (work / 'touch.log').write_text(text, encoding='utf-8')
        safe = last_safe(text, run)
        if (safe and (rotation is None or safe.rotation == rotation) and safe.rotation in (1, 3)
                and safe.width > safe.height and max(safe.left, safe.right) > 0):
            return safe
        time.sleep(.5)
    raise RuntimeError(f'no landscape [safe] for rotation {rotation}: {work / "touch.log"}')


def focused_app(text):
    components = re.findall(r'^\s*mCurrentFocus=Window\{\S+\s+u\d+\s+'
                            r'([A-Za-z0-9_.]+/[A-Za-z0-9_.$]+)\}', text, re.M)
    return len(components) == 1 and components[0].split('/')[0] == APP


def exercise(avd, work, run):
    avd.adb('install', '-r', str(work / 'game/neon_rumble.apk'), timeout=60)
    avd.adb('shell', 'am', 'force-stop', APP)
    cleared = avd.adb('shell', 'pm', 'clear', APP)
    if 'Success' not in cleared:
        raise RuntimeError(f'cannot reset task app: {cleared}')
    avd.adb('shell', 'cmd', 'overlay', 'enable', 'com.android.internal.display.cutout.emulation.corner')
    avd.adb('shell', 'settings', 'put', 'system', 'accelerometer_rotation', '0')
    avd.adb('shell', 'settings', 'put', 'system', 'user_rotation', '1')
    avd.adb('logcat', '-c')
    avd.adb('shell', 'input', 'keyevent', 'KEYCODE_WAKEUP')
    avd.adb('shell', 'wm', 'dismiss-keyguard')
    avd.adb('shell', 'am', 'start', '-W', '-n', APP + '/android.app.NativeActivity', '--es', 'probe', run)
    pid = avd.adb('shell', 'pidof', APP).strip()
    if not re.fullmatch(r'\d+', pid):
        raise RuntimeError('the game did not launch as one process')
    probe = avd.adb('shell', 'getevent', '-p')
    (work / 'getevent.txt').write_text(probe, encoding='utf-8')
    inputs = devices(probe)
    if len(inputs) != 1:
        raise RuntimeError(f'need one protocol-B touchscreen for the default display, '
                           f'found {len(inputs)} candidates: {work / "getevent.txt"}')
    layout = parse_layout((ROOT / 'games/neon-rumble/controls/touch.txt').read_text(encoding='utf-8'))
    previous_rotation = None
    for requested, gravity in ((1, '9.81:0:0'), (3, '-9.81:0:0')):
        avd.adb('shell', 'settings', 'put', 'system', 'user_rotation', str(requested))
        # sensorLandscape prefers the sensor to user_rotation. Exercise opposite gravity poles;
        # use the actual Display rotation rather than guessing the emulator sensor's sign.
        sensor = avd.adb('emu', 'sensor', 'set', 'acceleration', gravity)
        if 'KO:' in sensor:
            raise RuntimeError(f'cannot rotate the emulated accelerometer: {sensor}')
        time.sleep(1)
        expected = None if previous_rotation is None else 4 - previous_rotation
        safe = wait_safe(avd, run, expected, work)
        rotation = previous_rotation = safe.rotation
        script = gesture_script(inputs[0], safe, layout)
        (work / f'gesture-{rotation}.sh').write_text(script, encoding='utf-8')
        avd.adb('shell', 'sh', data=script, timeout=20)
        time.sleep(.5)
        if avd.adb('shell', 'pidof', APP).strip() != pid:
            raise RuntimeError('the game died/restarted during the gesture')
        focus = avd.adb('shell', 'dumpsys', 'window', 'windows')
        (work / f'window-{rotation}.txt').write_text(focus, encoding='utf-8')
        if not focused_app(focus):
            raise RuntimeError('the game lost foreground during the gesture')
        shot = work / f'landscape-{rotation}.png'
        shot.write_bytes(avd.adb('exec-out', 'screencap', '-p', binary=True))
        if sum(mean_rgb(shot)) < 30:
            raise RuntimeError(f'black screenshot: {shot}')
    text = avd.adb('logcat', '-d', '-v', 'raw', '-s', 'like-nes')
    (work / 'touch.log').write_text(text, encoding='utf-8')
    gpu = re.findall(r'^\[gpu\] (.+) \| ([^|]+) \| BC:', text, re.M)
    if not gpu or any(backend.strip() != 'Vulkan' for _, backend in gpu):
        raise RuntimeError(f'Vulkan is required, no GLES fallback: observed {gpu}')
    if not any('swiftshader' in adapter.lower() for adapter, _ in gpu):
        raise RuntimeError(f'the gate requires SwiftShader, observed {gpu}')
    bad, summary = verdict(text, run, layout)
    bad += actor_verdict(text, run, layout)
    if bad:
        raise RuntimeError('; '.join(bad))
    print(f'[android-emu] touch PASS: {summary}; actual P1 movement/jab; Vulkan/SwiftShader; 2 nonblack frames')


def main():
    from android_gate_avd import Avd
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', required=True)
    parser.add_argument('--work', required=True)
    parser.add_argument('--image', required=True)
    parser.add_argument('--avd-cache')
    args = parser.parse_args()
    work = Path(args.work).resolve()
    work.mkdir(parents=True, exist_ok=True)
    avd = Avd(args.sdk, work, args.image, args.avd_cache)
    try:
        avd.start()
        abi = args.image.split(';')[-1]
        subprocess.run(['bash', str(ROOT / 'scripts/android_gate_build.sh'), str(work), abi], check=True)
        exercise(avd, work, uuid.uuid4().hex)
        avd.adb('shell', 'am', 'force-stop', APP)
        avd.save_snapshot()
        print(f'[android-emu] PASS: evidence {work}')
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f'[android-emu] FAIL: {error}; evidence {work}', file=sys.stderr)
        return 1
    finally:
        avd.close()


if __name__ == '__main__':
    sys.exit(main())
