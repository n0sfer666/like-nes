#!/usr/bin/env python3
"""Средний цвет PNG — по числу на канал, в формате, который читает шелл.

Существует ради гейта 8 спеки #13: «фон стал красным» обязано быть ЧИСЛОМ, иначе доказательство
сводится к «я посмотрел», а именно это гейт и должен снять с владельца. Декодер здесь свой и
минимальный, потому что тащить Pillow в проверку, которую гоняют на свежей машине трёх ОС, значит
менять зависимость PNG на зависимость потяжелее — и получать в отчёте `pip: command not found`
вместо ответа про цвет.

Поддержано ровно то, что пишет stb_image_write в этом дереве: 8 бит на канал, RGB/RGBA, без
чересстрочности. Всё остальное — явная ошибка, а не тихо неверное среднее.
"""
import sys
import zlib

SIG = b"\x89PNG\r\n\x1a\n"


def chunks(data):
    pos = len(SIG)
    while pos + 8 <= len(data):
        size = int.from_bytes(data[pos:pos + 4], "big")
        kind = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + size]
        yield kind, body
        pos += 12 + size   # длина + тип + тело + CRC


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def unfilter(raw, w, h, bpp):
    stride = w * bpp
    out = bytearray(stride * h)
    prev = bytearray(stride)
    pos = 0
    for y in range(h):
        ft = raw[pos]
        line = bytearray(raw[pos + 1:pos + 1 + stride])
        pos += 1 + stride
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if ft == 1:
                line[i] = (line[i] + a) & 0xFF
            elif ft == 2:
                line[i] = (line[i] + b) & 0xFF
            elif ft == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xFF
            elif ft == 4:
                line[i] = (line[i] + paeth(a, b, c)) & 0xFF
            elif ft != 0:
                raise ValueError("неизвестный фильтр строки: %d" % ft)
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return out


def mean_rgb(path):
    data = open(path, "rb").read()
    if not data.startswith(SIG):
        raise ValueError("не PNG: %s" % path)
    w = h = 0
    bpp = 0
    idat = bytearray()
    for kind, body in chunks(data):
        if kind == b"IHDR":
            w = int.from_bytes(body[0:4], "big")
            h = int.from_bytes(body[4:8], "big")
            depth, color, _comp, _filt, interlace = body[8], body[9], body[10], body[11], body[12]
            if depth != 8 or color not in (2, 6) or interlace != 0:
                raise ValueError("поддержаны только 8-бит RGB/RGBA без чересстрочности: %s" % path)
            bpp = 3 if color == 2 else 4
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    if not (w and h and bpp):
        raise ValueError("нет IHDR: %s" % path)
    px = unfilter(zlib.decompress(bytes(idat)), w, h, bpp)
    n = w * h
    sums = [0, 0, 0]
    for c in range(3):
        sums[c] = sum(px[c::bpp])
    return [s / n for s in sums]


def main(argv):
    if len(argv) != 2:
        print("usage: png_mean.py <file.png>   # печатает 'R G B' в шкале 0..255", file=sys.stderr)
        return 2
    r, g, b = mean_rgb(argv[1])
    print("%.3f %.3f %.3f" % (r, g, b))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
