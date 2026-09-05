#!/usr/bin/env python3
"""Импортируемые DLL исполняемого файла Windows — читателем формата, а не грепом по строкам.

Гейт вертикали 5 спеки #20 утверждает, что пакет не требует VC++ Redistributable. Судить об этом
`strings`-ом нельзя: имя `VCRUNTIME140.dll` лежит в бинаре и тогда, когда его туда положил
компилятор в диагностике, и не лежит там, где импорт пришёл отложенной загрузкой. Отвечает на
вопрос ровно таблица импортов, поэтому она и читается.

Читается ОБЕ таблицы — обычная (каталог 1) и отложенная (каталог 13): отложенно загруженный
рантайм нужен процессу ровно так же, просто позже, и пропуск такой записи выдал бы пакет с
недостающей зависимостью за самодостаточный.
"""
import struct
import sys


class NotPE(Exception):
    pass


class Truncated(Exception):
    """Читатель дошёл до места, дальше которого честного ответа нет.

    Отдельно от NotPE, потому что предмет иной: там файл вовсе не образ, здесь образ разобран, но
    ЧАСТЬ таблицы прочитать нечем. Молчаливое «имён больше нет» в этом случае неотличимо от
    статически слинкованного бинаря — ровно тот дефект, против которого написан весь читатель, и
    он приезжал бы в гейт нулевым кодом возврата (класс vacuous-gate из ci_lint.py).
    """


def _sections(data, opt, optsz, nsec):
    out = []
    p = opt + optsz
    for _ in range(nsec):
        vsz, va, rsz, ra = struct.unpack_from('<IIII', data, p + 8)
        # У секции без инициализированных данных `rsz` нулевой, а `vsz` — настоящий размер: брать
        # только одно из двух значит промахиваться мимо RVA то в одну, то в другую сторону.
        out.append((va, max(vsz, rsz), ra, rsz))
        p += 40
    return out


def _reader(data):
    if data[:2] != b'MZ':
        raise NotPE('нет сигнатуры MZ')
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise NotPE('нет сигнатуры PE')
    coff = pe + 4
    nsec, = struct.unpack_from('<H', data, coff + 2)
    optsz, = struct.unpack_from('<H', data, coff + 16)
    opt = coff + 20
    magic, = struct.unpack_from('<H', data, opt)
    if magic == 0x20b:
        base = struct.unpack_from('<Q', data, opt + 24)[0]
        dd = opt + 112
    elif magic == 0x10b:
        base = struct.unpack_from('<I', data, opt + 28)[0]
        dd = opt + 96
    else:
        raise NotPE('незнакомый формат опционального заголовка %#x' % magic)
    nrva, = struct.unpack_from('<I', data, dd - 4)
    # Число каталогов берётся из файла и потому не доверенное. Объявленное больше шестнадцати
    # клампится (столько их в формате), объявленное шире опционального заголовка — отказ: читать
    # каталог за пределами заголовка значит выдавать соседние байты за RVA.
    nrva = min(nrva, 16)
    if (dd - opt) + nrva * 8 > optsz:
        raise Truncated('таблица каталогов (%d записей) не помещается в опциональный заголовок (%d байт)'
                        % (nrva, optsz))
    secs = _sections(data, opt, optsz, nsec)

    def off(rva):
        for va, sz, ra, rsz in secs:
            if va <= rva < va + sz:
                d = rva - va
                # RVA внутри неинициализированного хвоста секции файлу не соответствует ничем:
                # вернуть туда смещение значило бы читать чужие байты и выдавать их за имя.
                return ra + d if d < rsz else None
        return None

    def extent(rva):
        """Конец файловых данных секции, в которую попал RVA. Это и есть единственная честная
        граница обхода дескрипторов: `Size` из каталога данных загрузчик Windows не читает вовсе,
        а линкеры пишут туда и заниженное значение — по нему список импортов обрывался бы ТИХО,
        нулевым кодом возврата, и бинарь с `msvcp140.dll` выдавался бы за статически слинкованный
        (тот же класс, что правило vacuous-gate в ci_lint.py)."""
        for va, sz, ra, rsz in secs:
            if va <= rva < va + sz:
                return min(ra + rsz, len(data))
        return len(data)

    def cstr(rva):
        # Имя внутри НАЙДЕННОГО дескриптора обязано читаться. Вернуть здесь None значило бы молча
        # выбросить одну строку из списка и продолжить — то есть отдать неполную таблицу нулевым
        # кодом возврата. Старый формат отложенных импортов вычитает ImageBase, и промах уводит rva
        # в минус: это тоже сюда.
        o = off(rva)
        if o is None or o >= len(data):
            raise Truncated('имя по RVA %#x не отображается в файл' % (rva & 0xffffffff))
        end = data.find(b'\0', o)
        if end == -1:
            raise Truncated('имя по RVA %#x не закрыто нулём' % (rva & 0xffffffff))
        return data[o:end].decode('latin1')

    def entry(i):
        # Каталог, объявленный вне таблицы, — отказ, а не «его нет»: отложенные импорты лежат
        # четырнадцатыми, и заниженное NumberOfRvaAndSizes иначе выдавало бы бинарь с отложенным
        # msvcp140.dll за чистый.
        if i >= nrva:
            raise Truncated('каталог %d объявлен вне таблицы (в ней %d записей)' % (i, nrva))
        return struct.unpack_from('<II', data, dd + i * 8)

    return base, off, extent, cstr, entry


def _plain(data, off, extent, cstr, rva):
    """Каталог 1: массив IMAGE_IMPORT_DESCRIPTOR, конец — запись из одних нулей."""
    names = []
    p = off(rva)
    if p is None:
        raise Truncated('каталог импортов (RVA %#x) не отображается ни в одну секцию' % rva)
    end = extent(rva)
    while p + 20 <= end:
        ilt, ts, fc, nm, iat = struct.unpack_from('<IIIII', data, p)
        if (ilt, ts, fc, nm, iat) == (0, 0, 0, 0, 0):
            return names
        names.append(cstr(nm))
        p += 20
    # Обход обязан кончиться нулевым дескриптором. Упереться в конец секции значит прочитать
    # неизвестно какую часть таблицы — и промолчать об этом.
    raise Truncated('дескрипторы импорта кончились концом секции, нулевого среди них нет')


def _delay(data, base, off, extent, cstr, rva):
    """Каталог 13: ImgDelayDescr. Бит 1 в `grAttrs` говорит, что поля — RVA; без него это
    абсолютные адреса старого формата, и вычесть ImageBase обязан читатель, а не автор бинаря."""
    names = []
    p = off(rva)
    if p is None:
        raise Truncated('каталог отложенных импортов (RVA %#x) не отображается ни в одну секцию' % rva)
    end = extent(rva)
    while p + 32 <= end:
        attrs, nm = struct.unpack_from('<II', data, p)
        if attrs == 0 and nm == 0:
            return names
        names.append(cstr(nm if attrs & 1 else nm - base))
        p += 32
    raise Truncated('дескрипторы отложенной загрузки кончились концом секции, нулевого среди них нет')


def imports(path):
    with open(path, 'rb') as f:
        data = f.read()
    base, off, extent, cstr, entry = _reader(data)
    # `Size` каталога сознательно НЕ используется границей: обход идёт до нулевого дескриптора,
    # упираясь в конец секции. Ненулевое заниженное значение иначе обрезает список молча.
    rva = entry(1)[0]
    names = _plain(data, off, extent, cstr, rva) if rva else []
    rva = entry(13)[0]
    names += _delay(data, base, off, extent, cstr, rva) if rva else []
    return names


def main(argv):
    if len(argv) != 2:
        sys.stderr.write('usage: pe_imports.py <file.exe|file.dll>\n')
        return 2
    try:
        names = imports(argv[1])
    except Truncated as e:
        sys.stderr.write('pe-imports: %s — таблица прочитана НЕ ЦЕЛИКОМ: %s\n' % (argv[1], e))
        return 2
    except NotPE as e:
        sys.stderr.write('pe-imports: %s — не PE: %s\n' % (argv[1], e))
        return 2
    except OSError as e:
        sys.stderr.write('pe-imports: %s\n' % e)
        return 2
    except (struct.error, IndexError) as e:
        sys.stderr.write('pe-imports: %s — заголовки не разобраны: %s\n' % (argv[1], e))
        return 2
    # Ноль импортов у нашего бинаря невозможен: даже статически слинкованный exe зовёт kernel32.
    # Молчаливый пустой ответ сделал бы любое утверждение о зависимостях вакуумно зелёным, поэтому
    # он ОТКАЗ, а не «находок нет» (тот же класс, что правило vacuous-gate в ci_lint.py).
    if not names:
        sys.stderr.write('pe-imports: %s — таблица импортов пуста\n' % argv[1])
        return 3
    seen = set()
    for n in names:
        k = n.lower()
        if k not in seen:
            seen.add(k)
            print(n)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
