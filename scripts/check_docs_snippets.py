#!/usr/bin/env python3
"""Гейт 3 спеки #19: фрагменты в тексте взяты из собираемых примеров, а не написаны рядом с ними.

    docs_all_files <корень> | python3 scripts/check_docs_snippets.py <корень>

Список документов приходит на СТДИН от `docs_all_files` — той же функции, которой пользуются
остальные утверждения гейта документации. Свой обход был бы вторым списком того же множества:
раздел, добавленный в `docs/`, молча остался бы без сверки врезок, ровно как в правиле `list-drift`
из `ci_lint.py`.

Источник фрагмента обязан лежать в `docs/examples/`. Тогда «показанный код собирается и
запускается» ДОКАЗАНО гейтом 2 для каждого источника, а не заявлено: путь в произвольный файл
дерева выглядел бы свободнее, но проверять его было бы нечем.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import docs_snippets_lib as lib  # noqa: E402
import py_utf8  # noqa: E402

EXAMPLES = "docs/examples/"
# Языки КОДА — закрытым списком: код здесь тот, что собирает и запускает гейт 2, а `sh`, `text`,
# `powershell` и `bat` показывают команды и вывод, для них компилируемого источника не бывает.
# Новый язык (rust, wgsl) требует осознанной записи — по тому же основанию, что `crt_is_foreign`
# опознаёт чужие бинари поимённо.
CODE_LANGS = {"cpp", "c", "cc", "h", "hpp"}
# Язык фенса обязан соответствовать источнику: `.cpp` под ```text и `.out` под ```cpp рендерятся
# неверно, а расширение механизму и так известно — утверждение бесплатное.
EXT_LANG = {".cpp": "cpp", ".cc": "cpp", ".h": "cpp", ".hpp": "cpp", ".out": "text"}


def resolve(root, doc, line, src, name, lang, body, bad):
    """Одна врезка против своего источника. Все находки называются, а не первая."""
    where = "%s:%d" % (doc, line)
    # Принадлежность каталогу проверяется ПОСЛЕ нормализации: `docs/examples/../../engine/core/x.hpp`
    # начинается с нужного префикса и открывается — тот же класс, который гейт документации уже
    # закрыл у ссылок, уходящих выше корня дерева.
    if os.path.normpath(src).replace(os.sep, "/") != src.rstrip("/") \
            or not src.startswith(EXAMPLES):
        bad.append("%s: источник %s вне %s — про такой файл никто не утверждает, что он собирается"
                   % (where, src, EXAMPLES))
        return
    want_lang = EXT_LANG.get(os.path.splitext(src)[1])
    if want_lang is not None and lang.strip() != want_lang:
        bad.append("%s: врезка из %s помечена ```%s, а не ```%s"
                   % (where, src, lang.strip(), want_lang))
    path = os.path.join(root, src)
    if not os.path.isfile(path):
        bad.append("%s: источника %s нет в дереве" % (where, src))
        return
    text = open(path, encoding="utf-8").read()
    # Маркеры источника разбираются ВСЕГДА, а не только для именованной врезки: незакрытый
    # `docs:begin` в файле, показанном целиком, иначе находкой не станет — а тело он забирает
    # до конца файла (находка ревью раунда #19).
    bodies, marker_bad = lib.markers(text)
    for m in marker_bad:
        bad.append("%s: %s: %s" % (where, src, m))
    if name is None:
        want = text.splitlines()
    else:
        if name not in bodies:
            bad.append("%s: в %s нет маркера docs:begin(%s)" % (where, src, name))
            return
        want = bodies[name]
    if body != want:
        bad.append("%s: тело врезки разошлось с %s%s"
                   % (where, src, "" if name is None else "#" + name))


def unused_markers(root, used, whole, bad):
    """Маркер источника, на который не ссылается ни одна врезка, — находка.

    Направление у гейта было одно: врезка без маркера — ошибка, маркер без врезки — тишина. После
    переименования врезки `docs:begin(x)` остаётся в примере мёртвым и молча описывает вчерашний
    текст — ровно тот класс, который у соседа стережёт `assert_examples_paired` («голден, переживший
    свой пример»). Файл, показанный ЦЕЛИКОМ, освобождает все свои маркеры: его строки уехали в
    документ и без имени.
    """
    d = os.path.join(root, EXAMPLES)
    if not os.path.isdir(d):
        return
    for fn in sorted(os.listdir(d)):
        rel = EXAMPLES + fn
        path = os.path.join(d, fn)
        if not os.path.isfile(path) or rel in whole:
            continue
        bodies, _ = lib.markers(open(path, encoding="utf-8").read())
        for name in sorted(bodies):
            if (rel, name) not in used:
                bad.append("%s: маркер docs:begin(%s) не показан ни одной врезкой" % (rel, name))


def main():
    py_utf8.enable()
    if len(sys.argv) != 2:
        sys.stderr.write("usage: docs_all_files <root> | %s <root>\n" % sys.argv[0])
        return 2
    root = sys.argv[1]
    docs = [d.strip() for d in sys.stdin.read().splitlines() if d.strip()]
    # Пустой вход — отказ, а не «нарушений нет»: обход, промахнувшийся мимо дерева, иначе печатал бы
    # тот же вердикт, что чистый прогон (тот же класс, что правило vacuous-gate в ci_lint.py).
    if not docs:
        sys.stderr.write("snippets: НА ВХОДЕ НИ ОДНОГО ДОКУМЕНТА — сверять нечего\n")
        return 1
    bad, total = [], 0
    used, whole = set(), set()
    for doc in docs:
        path = doc if os.path.isabs(doc) else os.path.join(root, doc)
        rel = os.path.relpath(path, root)
        if not os.path.isfile(path):
            bad.append("%s: документа нет в дереве" % rel)
            continue
        doc_text = open(path, encoding="utf-8").read()
        found, form_bad = lib.snippets(doc_text)
        for m in form_bad:
            bad.append("%s: %s" % (rel, m))
        for line, lang in lib.bare_code_fences(doc_text, CODE_LANGS):
            # Решение 5 спеки: «примерного кода» в документации нет. Врезки сверяются с
            # источниками, а блок кода, написанный руками рядом с ними, до этого утверждения
            # проходил молча — то есть инвариант держался дисциплиной, а не механикой.
            bad.append("%s:%d: блок ```%s вне врезки — показанный код обязан приходить из %s"
                       % (rel, line, lang, EXAMPLES))
        for line, src, name, lang, body in found:
            total += 1
            if name is None:
                whole.add(src)
            else:
                used.add((src, name))
            resolve(root, rel, line, src, name, lang, body, bad)
    unused_markers(root, used, whole, bad)
    # Ноль врезок во всём наборе — тоже отказ: механизм, который никто не употребляет, неотличим от
    # сломанного разбора, а гейт при этом зелен.
    if total == 0:
        sys.stderr.write("snippets: НИ ОДНОЙ ВРЕЗКИ в %d документах — разбор или дерево сломаны\n"
                         % len(docs))
        return 1
    for m in bad:
        sys.stderr.write("snippets: %s\n" % m)
    if bad:
        sys.stderr.write("snippets: FAIL (%d находок)\n" % len(bad))
        return 1
    print("snippets: ok (%d врезок в %d документах, все совпали с источниками)" % (total, len(docs)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
