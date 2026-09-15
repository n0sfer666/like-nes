#!/usr/bin/env python3
"""Признак обхода дерева живёт в ЧЕТЫРЁХ копиях, и сверять их между собой некому.

`scripts/tree_invariants.sh` перечисляет корни в `ROOTS_CODE`/`ROOTS`, а `ascii_output_check.py`,
`check_hash_seam.py` и `check_fs_seam.py` — в своих `ROOTS`. Внешнего эталона у списка нет: «все
каталоги дерева» тут неверно (`deps`, `packaging`, `docs/ru` кода не несут), поэтому равенство
проверяется МЕЖДУ КОПИЯМИ — ровно форма
`mirrors-group` из `ci_lint.py`, та же, что у пары списков redist-имён в гейте CRT.

Половин у признака ДВЕ, и вторая до находки ревью не сверялась ничем: КУДА идти (`ROOTS`) и ЧТО
там брать (`EXTS` в питоне, `EXT` из `--include=` в шелле). Расширение, дописанное в три копии из
четырёх, ведёт себя ровно как недостающий корень — файл честно проходит три гейта и остаётся вне
четвёртого. `line_budget.py` со своим `EXTS` сюда НЕ входит: он судит авторский код целиком и
перечисляет сверх этого `.bat`, `.py` и `.sh`, то есть не копия, а другой список — тот же случай,
что соседний `CHAR=` у правила `list-drift`.

Расхождение молчит: корень, добавленный в одну копию, проходит инварианты швов и остаётся вне
ASCII-проверки (или наоборот), и узнать об этом можно только по коду, который туда положат потом.
Именно так `docs/examples` и попал в дерево — двумя правками, между которыми связи нет.

    python3 scripts/check_tree_roots.py             # гейт
    python3 scripts/check_tree_roots.py --selftest  # правила на сломанных копиях
"""
import os
import re
import sys

import py_utf8

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SH = "scripts/tree_invariants.sh"
PY = "scripts/ascii_output_check.py"
SEAM = "scripts/check_hash_seam.py"
FS = "scripts/check_fs_seam.py"

SH_ASSIGN = re.compile(r'^\s*(ROOTS|ROOTS_CODE)="([^"]*)"\s*$', re.M)
PY_ASSIGN = re.compile(r'^ROOTS\s*=\s*\(([^)]*)\)', re.M)
PY_ITEM = re.compile(r'"([^"]+)"')
SH_EXT = re.compile(r'^EXT="([^"]*)"', re.M | re.S)
PY_EXT = re.compile(r'^EXTS\s*=\s*\{([^}]*)\}', re.M)
EXT_ITEM = re.compile(r'--include=\*(\.\w+)')


def shell_roots(text):
    """Значение `ROOTS` из шелла с раскрытой подстановкой других переменных этого же файла."""
    seen = {}
    for name, value in SH_ASSIGN.findall(text):
        for var, val in seen.items():
            value = value.replace("${%s}" % var, val).replace("$" + var, val)
        seen[name] = value
    return set(seen.get("ROOTS", "").split())


def python_roots(text):
    m = PY_ASSIGN.search(text)
    return set(PY_ITEM.findall(m.group(1))) if m else set()


def shell_exts(text):
    """Расширения из `EXT` шелла: список записан флагами grep, а не словами."""
    m = SH_EXT.search(text)
    return set(EXT_ITEM.findall(m.group(1))) if m else set()


def python_exts(text):
    m = PY_EXT.search(text)
    return set(PY_ITEM.findall(m.group(1))) if m else set()


# Копий столько, сколько мест обходит дерево по этому списку. Третья приехала со швом хешей, а
# четвёртая — со швом файлового ввода-вывода (находки 5 и 6 аудита #21): корень, добавленный в три
# копии из четырёх, проходит инварианты швов и ASCII-проверку и молча остаётся вне гейта констант.
COPIES = ((SH, "shell"), (PY, "python"), (SEAM, "python"), (FS, "python"))
# Обе половины признака обхода. Имя списка идёт в текст находки: «расширение .inl есть в трёх
# копиях» и «корень tools есть в трёх копиях» — разные поломки, и различить их обязан лог.
GROUPS = (("корень", "ROOTS", {"shell": shell_roots, "python": python_roots}),
          ("расширение", "EXTS", {"shell": shell_exts, "python": python_exts}))


def copy_sets(root, parsers):
    """{путь копии: множество имён}. OSError наружу — читает вызывающий."""
    out = {}
    for rel, kind in COPIES:
        text = open(os.path.join(root, rel), encoding="utf-8").read()
        out[rel] = parsers[kind](text)
    return out


def check(root):
    """Находки словами. Пустая копия — ОТКАЗ: пустое равно пустому, и разбор, промахнувшийся мимо
    файла, иначе печатал бы «копии совпадают» (тот же класс, что vacuous-gate в ci_lint.py)."""
    bad = []
    for label, listname, parsers in GROUPS:
        try:
            sets = copy_sets(root, parsers)
        except OSError as e:
            return ["копию списка не прочитать: %s" % e]
        empty = ["%s: %s не разобран — сверять не с чем" % (rel, listname)
                 for rel, names in sorted(sets.items()) if not names]
        if empty:
            bad += empty
            continue
        for name in sorted(set().union(*sets.values())):
            lack = [rel for rel, names in sets.items() if name not in names]
            if lack:
                have = [rel for rel, names in sets.items() if name in names]
                bad.append("%s %s есть в %s и отсутствует в %s"
                           % (label, name, ", ".join(have), ", ".join(lack)))
    return bad


def gate(root, quiet=False):
    bad = check(root)
    for m in bad:
        sys.stderr.write("tree-roots: %s\n" % m)
    if bad:
        if not quiet:
            sys.stderr.write("tree-roots: FAIL (%d находок)\n" % len(bad))
        return 1
    if not quiet:
        roots, exts = (copy_sets(root, g[2]) for g in GROUPS)
        print("tree-roots: ok (%d корней и %d расширений в %d копиях)"
              % (len(roots[SH]), len(exts[SH]), len(roots)))
    return 0


if __name__ == "__main__":
    py_utf8.enable()
    if len(sys.argv) > 1 and sys.argv[1] == "--selftest":
        from check_tree_roots_selftest import selftest
        sys.exit(selftest())
    sys.exit(gate(ROOT))
