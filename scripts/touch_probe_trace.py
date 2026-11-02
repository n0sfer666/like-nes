"""Nonce-scoped mobile probe records. Coordinates and insets are logical points."""

import math
import re
from dataclasses import dataclass


@dataclass(frozen=True)
class Safe:
    fb_w: int
    fb_h: int
    scale: float
    left: float
    top: float
    right: float
    bottom: float
    rotation: int

    @property
    def width(self):
        return self.fb_w / self.scale

    @property
    def height(self):
        return self.fb_h / self.scale


@dataclass
class Event:
    phase: str
    tid: int
    id32: int
    x: float
    y: float
    owners: dict
    knob: tuple
    safe: Safe
    epoch: int
    fields: dict


def number(fields, key):
    value = float(fields[key])
    if not math.isfinite(value):
        raise ValueError(f"nonfinite {key}")
    return value


def safe_record(f):
    s = Safe(int(f['fb_w']), int(f['fb_h']), number(f, 'scale'),
             *(number(f, k) for k in ('left', 'top', 'right', 'bottom')),
             int(f['rotation']))
    if s.fb_w <= 0 or s.fb_h <= 0 or s.scale <= 0 or s.rotation not in range(4):
        raise ValueError('invalid framebuffer, scale or rotation')
    if not (0 < s.width <= 32767 and 0 < s.height <= 32767):
        raise ValueError('logical dimensions outside fix32 range')
    if min(s.left, s.top, s.right, s.bottom) < 0:
        raise ValueError('negative inset')
    if s.left + s.right >= s.width or s.top + s.bottom >= s.height:
        raise ValueError('insets consume the view')
    return s


def parse(text, run, controls):
    events, safes = [], []
    current, epoch = None, 0
    for lineno, line in enumerate(text.splitlines(), 1):
        match = re.search(r'\[(safe|touch)\]\s+(.*)', line)
        if not match:
            continue
        kind, body = match.groups()
        # Ignore another run before interpreting any of its payload, including bad numbers.
        if f'run={run}' not in body.split():
            continue
        words = body.split()
        phase = words.pop(0) if kind == 'touch' else None
        fields = {}
        try:
            for word in words:
                key, value = word.split('=', 1)
                if key in fields:
                    raise ValueError(f'duplicate {key}')
                fields[key] = value
            if fields['run'] != run:
                raise ValueError('ambiguous run')
            if kind == 'safe':
                updated = safe_record(fields)
                epoch += updated != current
                current = updated
                safes.append(current)
                continue
            if phase not in ('down', 'move', 'up'):
                raise ValueError('unknown touch phase')
            if current is None:
                raise ValueError('touch before [safe]')
            tid, id32 = int(fields['id']), int(fields['id32'])
            if not -(2**63) <= tid < 2**63 or tid == -1:
                raise ValueError('invalid touch id')
            if not -(2**31) <= id32 < 2**31:
                raise ValueError('invalid id32')
            owners = {key: int(fields[key]) for key in controls}
            event = Event(phase, tid, id32, number(fields, 'x'), number(fields, 'y'),
                          owners, (number(fields, 'knob_x'), number(fields, 'knob_y')), current, epoch, fields)
            if not (0 <= event.x <= current.width and 0 <= event.y <= current.height):
                raise ValueError('touch outside view')
            events.append(event)
        except (KeyError, ValueError, OverflowError) as error:
            raise ValueError(f'probe line {lineno}: {error}') from error
    return events, safes
