"""Independent touch.txt oracle shared by gesture drivers and the log verdict."""

import math
from dataclasses import dataclass


@dataclass(frozen=True)
class Button:
    corner: str
    dx: float
    dy: float
    radius: float

    def center(self, safe):
        x = safe.left + self.dx
        y = safe.top + self.dy
        if self.corner.endswith('right'):
            x = safe.width - safe.right - self.dx
        if self.corner.startswith('bottom'):
            y = safe.height - safe.bottom - self.dy
        return x, y

    def contains(self, safe, x, y):
        cx, cy = self.center(safe)
        return math.hypot(x - cx, y - cy) <= self.radius + .01

    def probe(self, safe):
        """Near the inward edge: ignoring a nonzero safe inset must miss the circle."""
        x, y = self.center(safe)
        right, bottom = self.corner.endswith('right'), self.corner.startswith('bottom')
        horizontal = safe.right if right else safe.left
        vertical = safe.bottom if bottom else safe.top
        if horizontal > 2:
            x += (1 - self.radius) if right else (self.radius - 1)
        elif vertical > 2:
            y += (1 - self.radius) if bottom else (self.radius - 1)
        return x, y


@dataclass(frozen=True)
class Layout:
    stick_radius: float
    buttons: dict

    @property
    def controls(self):
        return ('stick', *self.buttons)


def scalar(text, low, high):
    value = float(text)
    if not math.isfinite(value) or not low <= value <= high:
        raise ValueError('layout number outside range')
    return value


def parse_layout(text):
    stick, buttons = None, {}
    for line in text.splitlines():
        line = line.split('#', 1)[0].strip()
        if not line:
            continue
        fields = [part.strip() for part in line.split('|')]
        if len(fields) == 2 and fields[0] == 'stick' and stick is None:
            stick = scalar(fields[1], 1 / 65536, 256)
        elif len(fields) == 6 and fields[0] == 'button':
            _, name, corner, dx, dy, radius = fields
            if name not in 'abcde' or len(name) != 1 or name in buttons:
                raise ValueError('invalid or duplicate button')
            if corner not in ('top-left', 'top-right', 'bottom-left', 'bottom-right'):
                raise ValueError('invalid button corner')
            buttons[name] = Button(corner, scalar(dx, 0, 2048), scalar(dy, 0, 2048),
                                   scalar(radius, 1 / 65536, 256))
        else:
            raise ValueError('invalid or duplicate layout directive')
    if stick is None or 'a' not in buttons:
        raise ValueError('the gate requires stick and button a')
    return Layout(stick, buttons)
