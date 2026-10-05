"""Тела python, встроенные в швы процесса: heredoc под `py_run` или запуском python и строка `-c`.

Гейт кодировки разбирал только файлы `.py`, а дефект, ради которого он заведён, живёт и во
встроенном python: прогон на русской Windows 2026-10-03 нашёл пять `open()` без `encoding` в `.sh`
и `.yml` — руками, потому что ни один гейт их не видел. Отсюда тела достаются в той форме, в какой их
получит интерпретатор, и дальше судятся теми же правилами, что `.py`.

Обход идёт от МЕСТА ЗАПУСКА, а не от узнаваемой формы тела: запуск, чьё тело извлечь не удалось,
возвращается отказом. Иначе каждая форма, которую извлечение не знает (`"$PY" - "$a" <<'PY'`,
`python3 -X utf8 -c`, `<<\\PY`), была бы молчаливым пропуском — ревью 2026-10-04 нашло так живое
нарушение в `owner_check.sh`, на котором гейт печатал ok.

Имя вызова, область обхода и определение комментарной строки живут здесь, а `check_py_seam.py`
берёт их отсюда: оба гейта судят один и тот же шов, и две копии регекспа имени разъехались бы молча.
"""
import ast
import re
import textwrap
from collections import namedtuple

import py_heredoc

GLOBS = ("*.sh", "*.yml", "*.yaml")
# Хвост пути тоже вызов: `/usr/bin/python3 -` ломается точно так же, поэтому `/` в запрет перед
# именем не входит. Входят `\w`, `.` и `-`: ими начинаются `mypython3`, `x.python3` и `--python3`.
NAME = r"(?<![\w.\-])(?:python|python3)(?:\.exe)?"
COMMENT = re.compile(r"^\s*#")
# Словарь запуска шире NAME: интерпретатор, выбранный скриптом в переменную (`"$PY"`), лончер `py`
# и наш `py_run`. NAME остаётся узким — его делит `check_py_seam.py`, и расширение здесь меняло бы
# вердикты чужого гейта.
LAUNCH = re.compile(r"(?:" + NAME + r"|\"?\$\{?PY(?:THON)?\}?\"?|(?<![\w.\-$])py(?:\.exe)?"
                    r"|(?<![\w.\-])py_run)(?=[ \t]|\\\n)")
GAP = re.compile(r"(?:[ \t]|\\\n)+")
TOKEN = re.compile(r"(?:[^ \t\n\\]|\\[^\n])+")
SHELL_PYTHON = re.compile(r"^[ \t]*(?:-[ \t]+)?shell:[ \t]*['\"]?python", re.M)
# Внутри двойных кавычек шелл снимает обратную косую только перед этими знаками; перед прочими она
# доезжает до python как есть — `b'\r\n'` в строке -c обязан остаться байтовым литералом.
DQ_ESCAPED = "$`\"\\\n"

# `joins` — строки тела, на которых шелл склеил `\`+перенос: номер находки после склейки уезжал бы
# на строку вверх.
Body = namedtuple("Body", "line text joins")


def origin(body, row):
    """Номер строки файла для строки `row` тела (с единицы)."""
    return body.line + row - 1 + sum(1 for join in body.joins if join < row)


def parses(text):
    if text is None:
        return None
    try:
        ast.parse(text)
    except (SyntaxError, ValueError):
        return None
    return text


def _dequote(text, start, quote):
    out, joins, row, pos = [], [], 1, start
    while pos < len(text):
        char = text[pos]
        if char == quote:
            return "".join(out), tuple(joins)
        if quote == '"' and char == "\\" and pos + 1 < len(text) and text[pos + 1] in DQ_ESCAPED:
            if text[pos + 1] == "\n":
                joins.append(row)
            else:
                out.append(text[pos + 1])
            pos += 2
            continue
        row += char == "\n"
        out.append(char)
        pos += 1
    return None


def _refused(text, pos):
    return Body(text.count("\n", 0, pos) + 1, None, ())


def _dash_c(text, pos, yaml):
    quote = text[pos:pos + 1]
    # `$'…'`, подстановка или голое слово: что получит python, без исполнения шелла не узнать.
    found = _dequote(text, pos + 1, quote) if quote in ("'", '"') else None
    if found is None:
        return _refused(text, pos)
    body, joins = found
    return Body(text.count("\n", 0, pos) + 1, parses(textwrap.dedent(body) if yaml else body), joins)


def _heredoc(text, lines, pos, yaml):
    start, end = py_heredoc.logical(text, pos)
    match = py_heredoc.HEREDOC.match(text, pos)
    # Два heredoc в одной команде: какое из тел достанется python, решает порядок дескрипторов.
    if not match or len(list(py_heredoc.ANY.finditer(text, start, end))) != 1:
        return _refused(text, pos)
    first = text.count("\n", 0, end) + 1
    close = py_heredoc.span(lines, first, py_heredoc.word(match), match.group(1) == "-", yaml)
    if close is None:
        return _refused(text, pos)
    rows = lines[first:close]
    if match.group(1) == "-":
        rows = [raw.lstrip("\t") for raw in rows]
    body = "\n".join(rows)
    return Body(first + 1, parses(textwrap.dedent(body) if yaml else body), ())


def _site(text, lines, launch, yaml):
    """Тело у места запуска, отказ (Body с text=None) или None, если python получает скрипт."""
    run = launch.group(0) == "py_run"
    stdin, pos = run, launch.end()
    while True:
        gap = GAP.match(text, pos)
        pos = gap.end() if gap else pos
        token = TOKEN.match(text, pos)
        if not token:
            return _refused(text, launch.start()) if stdin else None
        word = token.group(0)
        if word.startswith("<<<"):
            return _refused(text, pos)
        if word.startswith("<<"):
            return _heredoc(text, lines, pos, yaml)
        if not run and word.startswith("-c"):
            if len(word) > 2:
                return _dash_c(text, pos + 2, yaml)
            gap = GAP.match(text, token.end())
            return _dash_c(text, gap.end() if gap else token.end(), yaml)
        pos = token.end()
        # После `-` и у `py_run` всё до heredoc — аргументы тела, а не флаги интерпретатора.
        if run or stdin:
            continue
        if word in ("-X", "-W"):
            gap = GAP.match(text, pos)
            nxt = TOKEN.match(text, gap.end() if gap else pos)
            pos = nxt.end() if nxt else pos
        elif word == "-":
            stdin = True
        elif not word.startswith("-"):
            return None


def _commented(text, pos):
    """Стоит ли `pos` после `#`, начинающего комментарий: в начале слова и вне кавычек."""
    start, quote = text.rfind("\n", 0, pos) + 1, None
    for index in range(start, pos):
        char = text[index]
        if quote:
            quote = None if char == quote else quote
        elif char in "'\"":
            quote = char
        elif char == "#" and (index == start or text[index - 1] in " \t"):
            return True
    return False


def bodies(text, yaml=False):
    """[Body] — тела в том виде, в каком их выполнит python. Тело, которое гейт не видит целиком
    (форма вне словаря, обрыв, синтаксис), приходит с text=None: решение за вызывающим."""
    lines, found = text.split("\n"), []
    skip = py_heredoc.hidden(lines, yaml)
    for launch in LAUNCH.finditer(text):
        if text.count("\n", 0, launch.start()) in skip or _commented(text, launch.start()):
            continue
        body = _site(text, lines, launch, yaml)
        if body is not None:
            found.append(body)
    # Шаг с `shell: python` — тело, которое гейт не судит вовсе: разбор блока YAML здесь не заведён.
    if yaml:
        found += [_refused(text, match.start()) for match in SHELL_PYTHON.finditer(text)]
    return sorted(found, key=lambda body: body.line)
