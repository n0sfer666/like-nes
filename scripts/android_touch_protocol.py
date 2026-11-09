"""Protocol-B input device discovery and landscape -> natural-portrait raw coordinates.

Rotation convention follows AOSP TouchInputMapper: ROTATION_90 maps natural (x,y) to
display (y,W-1-x), ROTATION_270 to (H-1-y,x). Raw axes include their min/max endpoints.
"""

import re
from dataclasses import dataclass
from typing import Optional


@dataclass(frozen=True)
class Axis:
    low: int
    high: int


@dataclass(frozen=True)
class Device:
    path: str
    x: Axis
    y: Axis
    slot: Axis
    tracking: Axis
    btn_touch: bool
    pressure: Optional[Axis] = None


def devices(text):
    """Return protocol-B candidates for input on the emulator's default display."""
    out = []
    blocks = re.finditer(r'add device \d+: (/dev/input/event\d+)\s*(.*?)(?=add device \d+:|\Z)',
                         text, re.S)
    for block in blocks:
        path, body = block.groups()
        axes = {}
        for match in re.finditer(r'\b(002f|0035|0036|0039|003a)\s*:\s*value\s+-?\d+,\s*'
                                 r'min\s+(-?\d+),\s*max\s+(-?\d+)', body, re.I):
            code, low, high = match.groups()
            axes[int(code, 16)] = Axis(int(low), int(high))
        if not all(code in axes for code in (0x2f, 0x35, 0x36, 0x39)):
            continue
        x, y, slot, tracking = (axes[code] for code in (0x35, 0x36, 0x2f, 0x39))
        if x.high <= x.low or y.high <= y.low or slot.high - slot.low < 1:
            continue
        if tracking.high < 102 or tracking.low > 101:
            continue
        pressure = axes.get(0x3a)
        if pressure and (pressure.high <= 0 or pressure.low > pressure.high):
            continue
        name = re.search(r'^[ \t]*name:[ \t]+"([^"\r\n]+)"[ \t]*\r?$', body, re.M)
        out.append((name[1] if name else '',
                    Device(path, x, y, slot, tracking, bool(re.search(r'\b014a\b', body)), pressure)))
    # Emulator QEMU registers numbered virtio touch devices at display index id - 1.
    if all(re.fullmatch(r'virtio_input_multi_touch_(?:[1-9]|1[01])', name) for name, _ in out):
        return [device for name, device in out if name == 'virtio_input_multi_touch_1']
    return [device for _, device in out]


def raw_point(device, safe, x, y):
    px, py = x * safe.scale, y * safe.scale
    if safe.rotation not in (1, 3):
        raise ValueError('raw gesture requires landscape rotation 1 or 3')
    nw, nh = safe.fb_h, safe.fb_w
    if not (0 <= px < safe.fb_w and 0 <= py < safe.fb_h) or min(nw, nh) <= 1:
        raise ValueError('gesture point outside the framebuffer')
    nx, ny = (nw - 1 - py, px) if safe.rotation == 1 else (py, nh - 1 - px)
    rx = round(device.x.low + nx * (device.x.high - device.x.low) / (nw - 1))
    ry = round(device.y.low + ny * (device.y.high - device.y.low) / (nh - 1))
    return rx, ry


def gesture_script(device, safe, layout):
    """Only validated event paths and integer arguments reach the remote shell."""
    path, slot = device.path, device.slot.low
    if not re.fullmatch(r'/dev/input/event\d+', path):
        raise ValueError('invalid input device path')
    lines = ['set -e']

    def send(kind, code, value):
        lines.append(f'sendevent {path} {kind} {code} {value}')

    def position(index, x, y, tracking=None):
        rx, ry = raw_point(device, safe, x, y)
        send(3, 0x2f, slot + index)
        if tracking is not None:
            send(3, 0x39, tracking)
            # Android treats zero pressure on an advertised axis as hover, even with a tracking ID.
            if device.pressure:
                send(3, 0x3a, device.pressure.high)
        send(3, 0x35, rx)
        send(3, 0x36, ry)

    def release(index):
        send(3, 0x2f, slot + index)
        if device.pressure:
            send(3, 0x3a, device.pressure.low)
        send(3, 0x39, -1)

    def sync(delay):
        send(0, 0, 0)
        lines.append(f'sleep {delay}')

    sx, sy = safe.width * .25, safe.height * .5
    fx, fy = layout.buttons['a'].probe(safe)
    # Первый A присоединяет P1; отпускание до основной пары отличает вход от атаки.
    # Новое касание после Up вправе повторно использовать тот же tracking id.
    position(1, fx, fy, 102)
    if device.btn_touch:
        send(1, 0x14a, 1)
    sync(.3)
    release(1)
    if device.btn_touch:
        send(1, 0x14a, 0)
    sync(.3)
    position(0, sx, sy, 101)
    if device.btn_touch:
        send(1, 0x14a, 1)
    sync(.3)
    position(0, sx + 30, sy)
    sync(.3)
    position(1, fx, fy, 102)
    sync(.12)
    release(1)
    sync(.3)
    release(0)
    if device.btn_touch:
        send(1, 0x14a, 0)
    sync(.2)
    return '\n'.join(lines) + '\n'
