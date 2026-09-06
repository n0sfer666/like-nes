#!/usr/bin/env python3
"""Гейт: скрипт, печатающий не-ASCII, обязан переключить вывод на UTF-8 (`py_utf8.enable()`).

    python3 scripts/check_py_utf8.py             # гейт
    python3 scripts/check_py_utf8.py --selftest  # правила проверяются сломанными фикстурами

Python берёт кодировку stdout у локали, и на windows-раннере GitHub это cp1252. Русская строка там
не «портится» — `print` падает `UnicodeEncodeError`, скрипт возвращает НЕНУЛЕВОЙ код на исправном
дереве, и «гейт нашёл нарушение» становится неотличимо от «гейт не смог напечатать вердикт». Ровно
это и случилось 2026-09-06: четыре опорных `pass` самопроверки врезок отдали код 1 при исправных
фикстурах, а сам набор умер трейсбеком в собственном `print`.

Мера при этом уже существовала — ТРЕМЯ копиями внутри скриптов, — и четыре новых файла её просто не
взяли. Список без эталона молчит: то же основание, по которому заведены `list-drift` в `ci_lint.py`
и сверка копий списка корней. Теперь мера одна (`py_utf8.py`), а этот гейт требует её у каждого.

Кандидат — файл с ШЕБАНГОМ (то есть точка входа: импортируемому модулю поток настраивает тот, кто
его запустил) и не-ASCII в СТРОКОВЫХ ЛИТЕРАЛАХ. Литералах, а не в файле: комментарии по-русски —
стиль репозитория, и до вывода они не доезжают. Отсюда разбор `ast`, а не греп: `py_utf8.enable()`,
написанное в комментарии, гейт обязан отбить наравне с отсутствующим.

Правил ДВА, потому что у шва процесса две стороны, и первое ничего не говорит о второй. Наш вывод
переключает `py_utf8.enable()`; вывод ДОЧЕРНЕГО процесса родитель декодирует той же локалью, и
`subprocess.run(..., text=True)` без `encoding` умирает `UnicodeDecodeError` в потоке-читателе —
ровно это и случилось на прогоне 3381a95, уже ПОСЛЕ того, как первое правило закрыло запись.
Область второго правила — ВСЕ файлы .py, а не кандидаты: шебанг отделяет точку входа только у
своего вывода, а чужой декодирует тот, кто его читает, будь то точка входа или модуль.
"""
import ast
import os
import subprocess
import sys
import tempfile

import py_utf8

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SELF = "scripts/check_py_utf8.py"
MODULE = "py_utf8"
PROC_CALLS = ("run", "Popen", "check_output", "check_call", "call")
TEXT_KW = ("text", "universal_newlines")


def tracked(root):
    """Файлы .py у git ВМЕСТЕ с ненаписанными в индекс: свежесозданный скрипт иначе проходил бы
    локальный прогон молча и падал бы только на коммит-гейте, когда под него уже написан код."""
    out = subprocess.run(["git", "ls-files", "--cached", "--others", "--exclude-standard", "*.py"],
                         cwd=root, capture_output=True, text=True, encoding="utf-8")
    return sorted(p for p in out.stdout.splitlines() if p.strip())


def needs_utf8(text):
    """Кандидат ли: шебанг плюс не-ASCII в строковом литерале."""
    if not text.startswith("#!"):
        return False
    try:
        tree = ast.parse(text)
    except SyntaxError:
        return False
    return any(isinstance(n, ast.Constant) and isinstance(n.value, str) and not n.value.isascii()
               for n in ast.walk(tree))


def calls_enable(text):
    """Есть ли настоящий вызов `py_utf8.enable()` — вызовом, а не вхождением подстроки."""
    try:
        tree = ast.parse(text)
    except SyntaxError:
        return False
    imported = any(
        (isinstance(n, ast.Import) and any(a.name == MODULE for a in n.names))
        or (isinstance(n, ast.ImportFrom) and n.module == MODULE)
        for n in ast.walk(tree))
    called = any(
        isinstance(n, ast.Call) and isinstance(n.func, ast.Attribute) and n.func.attr == "enable"
        and isinstance(n.func.value, ast.Name) and n.func.value.id == MODULE
        for n in ast.walk(tree))
    return imported and called


def reads_locale(text):
    """Текстовые вызовы subprocess: сколько их всего и какие берут кодировку у локали.

    Имя модуля берётся из ИМПОРТА, а не прибито строкой: `import subprocess as sp` иначе делал бы
    правило обходимым переименованием.
    """
    try:
        tree = ast.parse(text)
    except SyntaxError:
        return 0, []
    mods = {a.asname or a.name
            for n in ast.walk(tree) if isinstance(n, ast.Import)
            for a in n.names if a.name == "subprocess"}
    seen, bad = 0, []
    for n in ast.walk(tree):
        if not (isinstance(n, ast.Call) and isinstance(n.func, ast.Attribute)
                and n.func.attr in PROC_CALLS and isinstance(n.func.value, ast.Name)
                and n.func.value.id in mods):
            continue
        kw = {k.arg: k.value for k in n.keywords if k.arg}
        if not any(isinstance(kw.get(a), ast.Constant) and kw[a].value is True for a in TEXT_KW):
            continue
        seen += 1
        if "encoding" not in kw:
            bad.append(n.lineno)
    return seen, bad


def gate(root, quiet=False):
    bad, seen, reads, locale_bad = [], 0, 0, []
    files = tracked(root)
    for rel in files:
        path = os.path.join(root, rel)
        if not os.path.isfile(path):
            continue
        text = open(path, encoding="utf-8").read()
        found, lines = reads_locale(text)
        reads += found
        locale_bad += ["%s:%d" % (rel, line) for line in lines]
        if not needs_utf8(text):
            continue
        seen += 1
        if not calls_enable(text):
            bad.append(rel)
    # Пустое равно пустому: обход, промахнувшийся мимо дерева, обязан отличаться от чистого
    # прогона — тот же класс, что правило vacuous-gate в ci_lint.py.
    if not seen or not reads:
        sys.stderr.write("py-utf8: FAIL — обход ничего не нашёл (скриптов с не-ASCII выводом: %d, "
                         "текстовых вызовов subprocess: %d), проверять нечего\n" % (seen, reads))
        return 1
    # Гейт, не нашедший собственного исходника, описывает не то дерево, по которому его запустили.
    if os.path.isfile(os.path.join(root, SELF)) and SELF not in files:
        sys.stderr.write("py-utf8: FAIL — обход не видит собственный исходник %s\n" % SELF)
        return 1
    for rel in bad:
        sys.stderr.write("%s: печатает не-ASCII и не зовёт py_utf8.enable() — на windows-раннере "
                         "вывод уйдёт в cp1252 и скрипт умрёт трейсбеком в print\n" % rel)
    for place in locale_bad:
        sys.stderr.write("%s: subprocess в текстовом режиме без encoding — вывод дочернего процесса "
                         "будет разобран локалью и на windows-раннере убьёт поток-читатель "
                         "UnicodeDecodeError\n" % place)
    if bad or locale_bad:
        sys.stderr.write("py-utf8: FAIL — скриптов осмотрено: %d, вызовов: %d, находок: %d\n"
                         % (seen, reads, len(bad) + len(locale_bad)))
        return 1
    if not quiet:
        print("py-utf8: ok (%d скрипт(ов) с не-ASCII выводом переключают поток, "
              "%d текстовых вызов(ов) subprocess задают кодировку)" % (seen, reads))
    return 0


def selftest():
    from check_py_utf8_selftest import selftest as run
    return run(gate, tempfile.mkdtemp)


if __name__ == "__main__":
    py_utf8.enable()
    if len(sys.argv) > 1 and sys.argv[1] == "--selftest":
        sys.exit(selftest())
    sys.exit(gate(ROOT))
