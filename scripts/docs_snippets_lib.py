"""Разбор врезок документации и маркеров в исходниках (гейт 3 спеки #19).

Разбор живёт в python, а не в шелле рядом с остальным гейтом документации, по той же причине, по
которой там живёт `ascii_output_check.py`: предмет здесь МНОГОСТРОЧНЫЙ — тело фрагмента сверяется с
телом врезки целиком, а построчная регулярка в шелле такое сравнение не выражает.

Формат врезки в документе:

    <!-- snippet: docs/examples/<файл>#<имя> -->
    ```<язык>
    <тело>
    ```
    <!-- /snippet -->

Без `#<имя>` берётся файл ЦЕЛИКОМ — так в текст попадает ожидаемый вывод примера (`<имя>.out`),
то есть документация не может показать вывод, которого программа не печатает.
"""
import re

BEGIN = re.compile(r"docs:begin\(([A-Za-z0-9_.-]+)\)")
END = re.compile(r"docs:end\(([A-Za-z0-9_.-]+)\)")
OPEN = re.compile(r"^<!--\s*snippet:\s*(\S+?)(?:#([A-Za-z0-9_.-]+))?\s*-->$")
CLOSE = re.compile(r"^<!--\s*/snippet\s*-->$")
FENCE = re.compile(r"^```")


def dedent(lines):
    """Снять общий отступ. Пустые строки в расчёт не идут: отступ по ним равен нулю всегда, и одна
    пустая строка внутри фрагмента обнуляла бы сдвиг для всего тела."""
    widths = [len(s) - len(s.lstrip()) for s in lines if s.strip()]
    cut = min(widths) if widths else 0
    return [s[cut:] if s.strip() else "" for s in lines]


def markers(text):
    """Тела маркеров источника: имя -> список строк. Возвращает (тела, находки).

    Вложенность запрещена, и это не упрощение реализации: строки внутреннего маркера пришлось бы
    либо оставить во внешнем теле (в документе появился бы `// docs:begin(...)`, которого читатель
    не просил), либо выбросить (внешний фрагмент показывал бы код с ДЫРОЙ). Оба варианта хуже
    отказа."""
    bodies, bad = {}, []
    open_name, open_line, body = None, 0, []
    for n, line in enumerate(text.splitlines(), 1):
        b, e = BEGIN.search(line), END.search(line)
        if b:
            if open_name is not None:
                bad.append("строка %d: docs:begin(%s) внутри незакрытого docs:begin(%s)"
                           % (n, b.group(1), open_name))
                continue
            if b.group(1) in bodies:
                bad.append("строка %d: маркер %s объявлен повторно" % (n, b.group(1)))
                continue
            open_name, open_line, body = b.group(1), n, []
            continue
        if e:
            if open_name is None:
                bad.append("строка %d: docs:end(%s) без docs:begin" % (n, e.group(1)))
            elif e.group(1) != open_name:
                bad.append("строка %d: docs:end(%s) закрывает docs:begin(%s)"
                           % (n, e.group(1), open_name))
                open_name = None
            else:
                bodies[open_name] = dedent(body)
                open_name = None
            continue
        if open_name is not None:
            body.append(line)
    if open_name is not None:
        bad.append("строка %d: docs:begin(%s) не закрыт" % (open_line, open_name))
    return bodies, bad


def snippets(text):
    """Врезки документа: список (строка, источник, имя-или-None, язык, тело). И находки формы.

    Форма строгая — фенс сразу за открывающим комментарием, закрывающий комментарий сразу за
    фенсом: свободная форма означала бы, что синхронизатор и гейт по-разному решают, где кончается
    тело, а расходились бы они молча."""
    out, bad = [], []
    lines = text.splitlines()
    i = 0
    fenced = False
    while i < len(lines):
        # Внутри ```-блока разметка врезки — ПОКАЗАННЫЙ текст, а не врезка: страница, объясняющая
        # сам формат (contributing — вероятная следующая), иначе разбиралась бы как несущая
        # настоящую врезку, и синхронизатор переписал бы ей тело. Тот же приём, что у
        # `docs_heading_slugs` в docs_content_lib.sh, где ради этого фенсы считает и она.
        if fenced:
            if FENCE.match(lines[i]):
                fenced = False
            i += 1
            continue
        m = OPEN.match(lines[i].strip())
        if not m:
            if FENCE.match(lines[i]):
                fenced = True
            i += 1
            continue
        start = i + 1
        if i + 1 >= len(lines) or not FENCE.match(lines[i + 1]):
            bad.append("строка %d: за врезкой %s нет открывающего ```" % (start, m.group(1)))
            i += 1
            continue
        lang = lines[i + 1].strip()[3:]
        j = i + 2
        while j < len(lines) and not FENCE.match(lines[j]):
            j += 1
        if j >= len(lines):
            bad.append("строка %d: блок врезки %s не закрыт ```" % (start, m.group(1)))
            i += 1
            continue
        if j + 1 >= len(lines) or not CLOSE.match(lines[j + 1].strip()):
            bad.append("строка %d: врезка %s не закрыта <!-- /snippet -->" % (start, m.group(1)))
            i += 1
            continue
        out.append((start, m.group(1), m.group(2), lang, lines[i + 2:j]))
        i = j + 2
    return out, bad


def bare_code_fences(text, code_langs):
    """Фенсы языка КОДА, лежащие вне врезок: список (строка, язык).

    Решение 5 спеки #19 — «примерного кода в документации нет» — до этого утверждения держалось
    дисциплиной: гейт сверял каждую ВРЕЗКУ с источником и молчал про блок, написанный руками рядом.
    Список языков закрытый и приходит от вызывающего: код здесь — то, что собирает и запускает
    гейт 2, а `sh`, `text`, `powershell` и `bat` показывают команды и вывод, и требовать для них
    компилируемого источника значило бы запретить документации показывать команду.
    """
    lines = text.splitlines()
    found, _ = snippets(text)
    inside = set()
    for start, _src, _name, _lang, body in found:
        # start — номер строки открывающего фенса (1-based), то есть 0-based индекс OPEN = start-1,
        # а закрывающий фенс стоит через тело от него. Диапазон считается ОТ РАЗБОРА, а не вторым
        # проходом по тексту: написанные врозь, они разъехались бы молча.
        i = start - 1
        inside.update(range(i, i + 3 + len(body) + 1))
    out = []
    fenced = False
    for n, line in enumerate(lines):
        if not FENCE.match(line):
            continue
        if fenced:
            fenced = False
            continue
        fenced = True
        if n in inside:
            continue
        lang = line.strip()[3:].strip()
        if lang in code_langs:
            out.append((n + 1, lang))
    return out
