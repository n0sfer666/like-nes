#!/usr/bin/env python3
"""Синтетический PE с ЗАДАННЫМ списком импортируемых DLL — фикстура для утверждений вертикали 5.

Настоящего бинаря Windows на машине владельца взяться неоткуда: `--only windows` уходит в CI, а
пакет оттуда приезжает готовым. Поэтому подмены («в пакете появился импорт msvcp140.dll»,
«зависимость пришла отложенной загрузкой», «таблицы импортов нет вовсе») строятся здесь.

Собственный генератор не доказывает, что читатель понимает формат Microsoft: он доказывает, что
читатель понимает ЭТОТ генератор. Вторая половина позитивного контроля — якорь на настоящем
`wgpu_native.dll` из дистрибуции (см. assert_reader_anchor в release_crt_lib.sh).
"""
import struct
import sys

SEC_RVA = 0x1000
SEC_OFF = 0x200
IMAGE_BASE64 = 0x180000000
IMAGE_BASE32 = 0x10000000


def _noterm_blob(plain):
    """Таблица импортов БЕЗ нулевого дескриптора, дотянутая до самого конца секции.

    Имена кладутся первыми, дескрипторы — за ними и добиваются копиями валидной записи, пока длина
    не сядет на границу выравнивания: иначе нулевая набивка секции сама сыграла бы терминатором, и
    фикстура проверяла бы набивку вместо читателя.
    """
    names = bytearray()
    rva = {}
    for n in plain:
        if n not in rva:
            rva[n] = SEC_RVA + len(names)
            names += n.encode('latin1') + b'\0'
    # Имена выравниваются до 4 байт: дескриптор длиной 20 сдвигает сумму только кратно четырём, и
    # из остатка, не кратного ей, добивка не сошлась бы на границе выравнивания никогда.
    names += b'\0' * ((-len(names)) % 4)
    one = struct.pack('<IIIII', 0, 0, 0, rva[plain[-1]], 0)
    desc = b''.join(struct.pack('<IIIII', 0, 0, 0, rva[n], 0) for n in plain)
    while (len(names) + len(desc)) % 0x200:
        desc += one
    return bytes(names) + desc, SEC_RVA + len(names)


def build(plain, delay, pe32=False, short_size=False, unmapped=False, noterm=False, nrva=16):
    blob = bytearray()
    # Смещения имён считаются ПОСЛЕ обеих таблиц: длина каждой известна заранее (по записи на
    # библиотеку плюс нулевой терминатор), и имена ложатся общим хвостом.
    imp_sz = (len(plain) + 1) * 20
    dly_sz = (len(delay) + 1) * 32 if delay else 0
    names_at = imp_sz + dly_sz
    names = bytearray()
    rva = {}
    for n in plain + delay:
        if n not in rva:
            rva[n] = SEC_RVA + names_at + len(names)
            names += n.encode('latin1') + b'\0'
    for n in plain:
        blob += struct.pack('<IIIII', 0, 0, 0, rva[n], 0)
    blob += b'\0' * 20
    for n in delay:
        # grAttrs=1 — поля описателя суть RVA. Ветка с абсолютными адресами старого формата
        # проверяется отдельной фикстурой (--delay-va).
        blob += struct.pack('<IIIIIIII', 1, rva[n], 0, 0, 0, 0, 0, 0)
    if delay:
        blob += b'\0' * 32
    blob += names

    noterm_rva = SEC_RVA
    if noterm:
        blob, noterm_rva = _noterm_blob(plain)

    magic, base_fld, ddoff = (0x10b, 28, 96) if pe32 else (0x20b, 24, 112)
    image_base = IMAGE_BASE32 if pe32 else IMAGE_BASE64
    opt = bytearray(ddoff + 16 * 8)
    struct.pack_into('<H', opt, 0, magic)
    if pe32:
        struct.pack_into('<I', opt, base_fld, image_base)
    else:
        struct.pack_into('<Q', opt, base_fld, image_base)
    struct.pack_into('<I', opt, 32, 0x1000)
    struct.pack_into('<I', opt, 36, 0x200)
    struct.pack_into('<I', opt, 56, SEC_RVA + 0x1000)
    struct.pack_into('<I', opt, 60, SEC_OFF)
    struct.pack_into('<I', opt, ddoff - 4, nrva)
    # Заниженный (но НЕнулевой) Size в каталоге данных — форма, которую пишут реальные линкеры и
    # которую загрузчик Windows игнорирует, идя до нулевого дескриптора. Читатель, принявший Size
    # за границу, вернёт лишь первое имя — и промолчит об этом.
    if plain:
        # RVA каталога, уведённый в НЕинициализированный хвост секции: файловых байтов там нет, и
        # читатель, вернувший оттуда смещение, читал бы чужие данные. Молчаливый пустой список в
        # этом случае неотличим от бинаря без импортов вовсе.
        head = SEC_RVA + (0x2000 if unmapped else 0)
        # Таблица БЕЗ нулевого дескриптора лежит своим блоком в конце секции — её адрес называет
        # _noterm_blob.
        if noterm:
            head = noterm_rva
        struct.pack_into('<II', opt, ddoff + 8, head, 20 if short_size else imp_sz)
    if delay:
        struct.pack_into('<II', opt, ddoff + 13 * 8, SEC_RVA + imp_sz, 32 if short_size else dly_sz)

    coff = struct.pack('<HHIIIHH', 0x8664 if not pe32 else 0x14c, 1, 0, 0, 0, len(opt), 0x2022)
    raw = (len(blob) + 0x1FF) & ~0x1FF
    sec = struct.pack('<8sIIII', b'.rdata', raw + 0x3000 if unmapped else len(blob), SEC_RVA,
                      raw, SEC_OFF) + struct.pack('<IIHHI', 0, 0, 0, 0, 0x40000040)

    out = bytearray(b'MZ' + b'\0' * 0x3e)
    struct.pack_into('<I', out, 0x3c, 0x40)
    out += b'PE\0\0' + coff + bytes(opt) + sec
    out += b'\0' * (SEC_OFF - len(out))
    out += blob
    out += b'\0' * ((-len(out)) % 0x200)
    return bytes(out)


def main(argv):
    out, plain, delay, pe32, va, short = None, [], [], False, False, False
    unmapped, noterm, nrva = False, False, 16
    i = 1
    while i < len(argv):
        a = argv[i]
        if a == '--import':
            i += 1
            plain.append(argv[i])
        elif a == '--delay':
            i += 1
            delay.append(argv[i])
        elif a == '--pe32':
            pe32 = True
        elif a == '--short-size':
            short = True
        elif a == '--unmapped-dir':
            unmapped = True
        elif a == '--no-terminator':
            noterm = True
        elif a == '--nrva':
            i += 1
            nrva = int(argv[i])
        elif a == '--delay-va':
            # Абсолютные адреса в описателе — формат 32-битных линкеров: в PE32+ ImageBase не
            # помещается в поле, поэтому форма без разрядности не существует и не подделывается.
            va, pe32 = True, True
        elif out is None:
            out = a
        else:
            sys.stderr.write('pe-fixture: лишний аргумент %s\n' % a)
            return 2
        i += 1
    if out is None:
        sys.stderr.write('usage: pe_fixture.py <out> [--import NAME] [--delay NAME] [--pe32]'
                         ' [--delay-va] [--short-size] [--unmapped-dir] [--no-terminator] [--nrva N]\n')
        return 2
    data = build(plain, delay, pe32, short, unmapped, noterm, nrva)
    if va:
        data = _to_va(data, plain, delay)
    with open(out, 'wb') as f:
        f.write(data)
    return 0


def _to_va(data, plain, delay):
    """Тот же образ, но описатели отложенной загрузки — со СБРОШЕННЫМ битом RVA и абсолютными
    адресами: читатель обязан узнать оба формата, иначе бинарь старого линкера прочитается как
    «отложенных импортов нет»."""
    d = bytearray(data)
    p = SEC_OFF + (len(plain) + 1) * 20
    for _ in delay:
        attrs, nm = struct.unpack_from('<II', d, p)
        struct.pack_into('<II', d, p, attrs & ~1, nm + IMAGE_BASE32)
        p += 32
    return bytes(d)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
