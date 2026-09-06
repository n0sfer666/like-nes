#!/usr/bin/env python3
"""Переписать тела врезок из их источников (гейт 3 спеки #19).

    docs_all_files <корень> | python3 scripts/docs_snippet_sync.py <корень>

Список документов приходит на СТДИН от той же `docs_all_files`, что кормит гейт: обойди
синхронизатор дерево сам — и он правил бы не то множество, которое потом проверяют.

Массовость здесь безвредна, в отличие от `docs_stamp.sh`, который штампует ОДИН названный файл.
Разница не во вкусе: штамп перевода — акт подтверждения человеком, механически отличить свежий
перевод от протухшего нечем, поэтому массовая штамповка была бы кнопкой «сделать зелёным». Тело
врезки, напротив, выводится из источника механически и целиком — подтверждать в нём нечего.

Правится только то, что разошлось; форма врезки и существование источника — дело гейта, здесь
непонятое молча пропускается, иначе синхронизатор чинил бы находку, вместо того чтобы дать её
увидеть.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import docs_snippets_lib as lib  # noqa: E402
import py_utf8  # noqa: E402


def body_for(root, src, name):
    path = os.path.join(root, src)
    if not os.path.isfile(path):
        return None
    text = open(path, encoding="utf-8").read()
    if name is None:
        return text.splitlines()
    bodies, bad = lib.markers(text)
    if bad or name not in bodies:
        return None
    return bodies[name]


def sync(root, doc):
    path = doc if os.path.isabs(doc) else os.path.join(root, doc)
    if not os.path.isfile(path):
        return 0
    lines = open(path, encoding="utf-8").read().splitlines()
    found, _ = lib.snippets("\n".join(lines))
    changed = 0
    # Врезки правятся С КОНЦА: правка сдвигает номера строк ниже себя, и проход сверху вниз испортил
    # бы все следующие врезки того же документа.
    for line, src, name, _lang, body in reversed(found):
        want = body_for(root, src, name)
        if want is None or want == body:
            continue
        lines[line + 1:line + 1 + len(body)] = want
        changed += 1
    if changed:
        # Перевод строки задаётся ЯВНО: на Windows python в текстовом режиме пишет CRLF, и
        # синхронизатор, запущенный там, менял бы концы строк во всём документе — гейт врезок
        # покраснел бы на файле, который он же и починил. Пин `*.md text eol=lf` в `.gitattributes`
        # — второй замок, а не замена этому: он чинит checkout, а не запись.
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(lines) + "\n")
    return changed


def main():
    py_utf8.enable()
    if len(sys.argv) != 2:
        sys.stderr.write("usage: docs_all_files <root> | %s <root>\n" % sys.argv[0])
        return 2
    root = sys.argv[1]
    docs = [d.strip() for d in sys.stdin.read().splitlines() if d.strip()]
    if not docs:
        sys.stderr.write("snippet-sync: на входе ни одного документа\n")
        return 1
    total = 0
    for doc in docs:
        n = sync(root, doc)
        if n:
            print("snippet-sync: %s — обновлено врезок: %d" % (doc, n))
            total += n
    print("snippet-sync: готово, обновлено врезок: %d" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
