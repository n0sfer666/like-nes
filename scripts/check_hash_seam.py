#!/usr/bin/env python3
"""Инвариант: константы FNV-1a живут в примитивах, а не рукописными копиями по дереву.

    python3 scripts/check_hash_seam.py            # гейт
    python3 scripts/check_hash_seam.py --selftest # правила проверяются сломанными фикстурами

Находка 5 аудита #21: тот же цикл FNV-1a был переписан по дереву около четырнадцати раз, ДВУМЯ
семьями констант — канонической (`0xcbf29ce484222325`) и нашей (`1469598103934665603`), — и
расхождение ничем себя не проявляло: оба хеша просто считались, оба перепиниваются голденами, а
несравнимость всплывает через полгода, когда кто-нибудь сверит один голден с другим. Правило дерево
записало само в шапке `hash_mix.hpp`: примитив выносится, как только потребителей стало двое.

Копии сведены, но ничто не мешает написать пятнадцатую: `h ^= x; h *= 0x100000001b3ull;` — три
знакомых строки, которые пишутся по памяти за десять секунд и проходят все прочие гейты дерева.
Что именно считается копией и чем это подавляется — `scripts/hash_seam_rules.py`; здесь обход
дерева и охранники, отличающие «находок нет» от «искать было негде».
"""

import subprocess
import sys
from pathlib import Path

import py_utf8
from hash_seam_rules import PRIMITIVES, file_hits

EXTS = {".c", ".cc", ".cxx", ".cpp", ".h", ".hpp", ".inl", ".m", ".mm"}
# ТРЕТЬЯ копия списка корней; внешнего эталона у него нет, поэтому копии сверяются между
# собой (`scripts/check_tree_roots.py`, форма `mirrors-group`). Корень, дописанный в две
# копии из трёх, проходит инварианты швов и ASCII-проверку и молча остаётся вне этого гейта.
ROOTS = ("engine", "tools", "example_ugly_game", "platform", "docs/examples")

# Позитивный контроль: обход обязан видеть оба примитива, дерево — быть не пустым, а КАЖДОЕ из двух
# правил обязано что-то находить в самих примитивах. Первого условия мало — файл на месте, а разбор
# чисел (или регулярка формы) мог сломаться и молчать.
MIN_FILES = 60


def audit(files):
    """files: {путь: текст}. Находки словами, примитивы пропущены."""
    bad = []
    for path in sorted(files):
        if path in PRIMITIVES:
            continue
        hits, ok = file_hits(files[path])
        for line, why in hits:
            if line in ok:
                continue
            bad.append(f"{path}:{line}: {why} — рукописная копия FNV. Возьми примитив "
                       f"({' или '.join(PRIMITIVES)}) или объясни маркером "
                       f"`// hash-seam: allow <причина в три слова>`.")
    return bad


def guards(files):
    """Отказы охранников: обход, промахнувшийся мимо дерева, обязан отличаться от чистого прогона."""
    missing = [p for p in PRIMITIVES if p not in files]
    if missing:
        return [f"обход не нашёл примитив(ы) {', '.join(missing)}: искать было негде, "
                f"и 'находок нет' здесь значит 'ничего не проверено'"]
    if len(files) < MIN_FILES:
        return [f"обход дал {len(files)} файл(ов) при пороге {MIN_FILES}: он описывает не то "
                f"дерево, по которому его запустили"]
    nums = forms = 0
    for path in PRIMITIVES:
        hits, _ = file_hits(files[path])
        nums += sum(1 for _, why in hits if "литерал" in why)
        forms += len(hits) - sum(1 for _, why in hits if "литерал" in why)
    bad = []
    if not nums:
        bad.append("в самих примитивах не найдено ни одной константы FNV: разбор чисел сломан, "
                   "и пустой список находок ничего не значит")
    if not forms:
        bad.append("в самих примитивах не найдено ни одного смешивания именованной константой: "
                   "правило формы сломано, и молчит оно неотличимо от чистого дерева")
    return bad


def scan(root):
    """Наши исходники глазами git — та же идиома, что у line_budget.py и ascii_output_check.py.

    Ненаписанное в индекс берётся наравне с индексом: иначе свежесозданная копия проходила бы
    локальный прогон молча и падала бы только на коммит-гейте.
    """
    out = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        capture_output=True, text=True, encoding="utf-8", check=True).stdout
    prefixes = tuple(r + "/" for r in ROOTS)
    files = {}
    for name in (n for n in out.split("\0") if n):
        path = Path(name)
        if path.suffix not in EXTS or not name.startswith(prefixes):
            continue
        blob = root / path
        if blob.is_file():
            files[name] = blob.read_text(encoding="utf-8", errors="replace")
    return files


def gate(root):
    files = scan(root)
    stop = guards(files)
    if stop:
        for line in stop:
            print(f"hash-seam: FAIL — {line}")
        return 1
    bad = audit(files)
    for line in bad:
        print(line)
    print(f"hash-seam: {'FAIL' if bad else 'PASS'} — {len(files)} файл(ов), находок: {len(bad)}")
    return 1 if bad else 0


def main(argv):
    py_utf8.enable()
    from check_hash_seam_selftest import selftest
    if "--selftest" in argv:
        return selftest(audit, guards, scan)
    # Та же дисциплина, что у ci_lint.py: сломанное правило молчит ровно так же, как чистое дерево.
    if selftest(audit, guards, scan, verbose=False):
        print("hash-seam: FAIL — сломаны сами правила, находкам верить нельзя")
        return 1
    return gate(Path(__file__).resolve().parent.parent)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
