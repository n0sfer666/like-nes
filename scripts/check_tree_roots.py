#!/usr/bin/env python3
"""Список корней дерева живёт в ТРЁХ копиях, и сверять их между собой некому.

`scripts/tree_invariants.sh` перечисляет корни в `ROOTS_CODE`/`ROOTS`, `scripts/ascii_output_check.py`
и `scripts/check_hash_seam.py` — в своих `ROOTS`. Внешнего эталона у списка нет: «все каталоги дерева» тут неверно (`deps`,
`packaging`, `docs/ru` кода не несут), поэтому равенство проверяется МЕЖДУ КОПИЯМИ — ровно форма
`mirrors-group` из `ci_lint.py`, та же, что у пары списков redist-имён в гейте CRT.

Расхождение молчит: корень, добавленный в одну копию, проходит инварианты швов и остаётся вне
ASCII-проверки (или наоборот), и узнать об этом можно только по коду, который туда положат потом.
Именно так `docs/examples` и попал в дерево — двумя правками, между которыми связи нет.

    python3 scripts/check_tree_roots.py             # гейт
    python3 scripts/check_tree_roots.py --selftest  # правила на сломанных копиях
"""
import os
import re
import shutil
import sys
import tempfile

import py_utf8

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SH = "scripts/tree_invariants.sh"
PY = "scripts/ascii_output_check.py"
SEAM = "scripts/check_hash_seam.py"

SH_ASSIGN = re.compile(r'^\s*(ROOTS|ROOTS_CODE)="([^"]*)"\s*$', re.M)
PY_ASSIGN = re.compile(r'^ROOTS\s*=\s*\(([^)]*)\)', re.M)
PY_ITEM = re.compile(r'"([^"]+)"')


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


# Копий столько, сколько мест обходит дерево по этому списку. Третья приехала со швом хешей
# (находка 5 аудита #21): корень, добавленный в две копии из трёх, проходит инварианты швов и
# ASCII-проверку и молча остаётся вне гейта констант FNV.
COPIES = ((SH, "shell"), (PY, "python"), (SEAM, "python"))


def copy_roots(root):
    """{путь копии: множество корней}. OSError наружу — читает вызывающий."""
    out = {}
    for rel, kind in COPIES:
        text = open(os.path.join(root, rel), encoding="utf-8").read()
        out[rel] = shell_roots(text) if kind == "shell" else python_roots(text)
    return out


def check(root):
    """Находки словами. Пустая копия — ОТКАЗ: пустое равно пустому, и разбор, промахнувшийся мимо
    файла, иначе печатал бы «копии совпадают» (тот же класс, что vacuous-gate в ci_lint.py)."""
    bad = []
    try:
        roots = copy_roots(root)
    except OSError as e:
        return ["копию списка не прочитать: %s" % e]
    for rel, names in roots.items():
        if not names:
            bad.append("%s: ROOTS не разобран — сверять не с чем" % rel)
    if bad:
        return bad
    for name in sorted(set().union(*roots.values())):
        lack = [rel for rel, names in roots.items() if name not in names]
        if lack:
            have = [rel for rel, names in roots.items() if name in names]
            bad.append("корень %s есть в %s и отсутствует в %s"
                       % (name, ", ".join(have), ", ".join(lack)))
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
        roots = copy_roots(root)
        print("tree-roots: ok (%d корней в %d копиях)" % (len(roots[SH]), len(roots)))
    return 0


def selftest():
    bad = 0

    def run(want, name, mutate):
        nonlocal bad
        d = tempfile.mkdtemp()
        os.makedirs(os.path.join(d, "scripts"))
        changed = False
        for rel, _ in COPIES:
            text = open(os.path.join(ROOT, rel), encoding="utf-8").read()
            new = mutate(rel, text)
            changed = changed or new != text
            with open(os.path.join(d, rel), "w", encoding="utf-8", newline="\n") as fh:
                fh.write(new)
        if want == "fail" and not changed:
            sys.stderr.write("tree-roots-selftest: БРАК %s: порча ничего не изменила\n" % name)
            bad = 1
            shutil.rmtree(d, True)
            return
        rc = gate(d, quiet=True)
        shutil.rmtree(d, True)
        ok = (want == "pass" and rc == 0) or (want == "fail" and rc != 0)
        if ok:
            print("tree-roots-selftest: OK   %s (%s)" % (name, want))
        else:
            sys.stderr.write("tree-roots-selftest: БРАК %s: ожидали %s, код %d\n" % (name, want, rc))
            bad = 1

    run("pass", "нетронутые копии совпадают", lambda rel, t: t)
    run("fail", "корень пропал из копии в шелле",
        lambda rel, t: t.replace(' docs/examples"', '"') if rel == SH else t)
    run("fail", "корень пропал из копии в python",
        lambda rel, t: t.replace(', "docs/examples"', "") if rel == PY else t)
    run("fail", "лишний корень в копии python",
        lambda rel, t: t.replace('"platform",', '"platform", "deps",') if rel == PY else t)
    run("fail", "разбор не нашёл копию в шелле",
        lambda rel, t: t.replace("ROOTS_CODE=", "ROOTS_SRC=").replace("ROOTS=", "ROOTS_ALL=")
        if rel == SH else t)
    run("fail", "разбор не нашёл копию в python",
        lambda rel, t: t.replace("ROOTS = (", "ROOTS_ALL = (") if rel == PY else t)
    run("fail", "корень пропал из копии шва хешей",
        lambda rel, t: t.replace(', "docs/examples"', "") if rel == SEAM else t)
    run("fail", "лишний корень в копии шва хешей",
        lambda rel, t: t.replace('"platform",', '"platform", "deps",') if rel == SEAM else t)
    run("fail", "разбор не нашёл копию шва хешей",
        lambda rel, t: t.replace("ROOTS = (", "ROOTS_ALL = (") if rel == SEAM else t)

    if bad:
        sys.stderr.write("tree-roots-selftest: FAIL\n")
        return 1
    print("tree-roots-selftest: PASS")
    return 0


if __name__ == "__main__":
    py_utf8.enable()
    if len(sys.argv) > 1 and sys.argv[1] == "--selftest":
        sys.exit(selftest())
    sys.exit(gate(ROOT))
