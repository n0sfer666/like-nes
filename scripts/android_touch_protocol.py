"""Protocol-B input device discovery and landscape -> natural-portrait raw coordinates.

Rotation convention follows AOSP TouchInputMapper: ROTATION_90 maps natural (x,y) to
display (y,W-1-x), ROTATION_270 to (H-1-y,x). Raw axes include their min/max endpoints.
"""

import re
from dataclasses import dataclass


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


def devices(text):
    out = []
    blocks = re.finditer(r'add device \d+: (/dev/input/event\d+)\s*(.*?)(?=add device \d+:|\Z)',
                         text, re.S)
    for block in blocks:
        path, body = block.groups()
        axes = {}
        for match in re.finditer(r'\b(002f|0035|0036|0039)\s*:\s*value\s+-?\d+,\s*'
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
        out.append(Device(path, x, y, slot, tracking, bool(re.search(r'\b014a\b', body))))
    return out


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
        send(3, 0x35, rx)
        send(3, 0x36, ry)

    def sync(delay):
        send(0, 0, 0)
        lines.append(f'sleep {delay}')

    sx, sy = safe.width * .25, safe.height * .5
    fx, fy = layout.buttons['a'].probe(safe)
    position(0, sx, sy, 101)
    if device.btn_touch:
        send(1, 0x14a, 1)
    sync(.3)
    position(0, sx + 30, sy)
    sync(.3)
    position(1, fx, fy, 102)
    sync(.5)
    send(3, 0x2f, slot + 1)
    send(3, 0x39, -1)
    sync(.3)
    send(3, 0x2f, slot)
    send(3, 0x39, -1)
    if device.btn_touch:
        send(1, 0x14a, 0)
    sync(.2)
    return '\n'.join(lines) + '\n'
