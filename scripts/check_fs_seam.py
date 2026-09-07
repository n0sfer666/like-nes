#!/usr/bin/env python3
"""Инвариант: файловый ввод-вывод идёт через платформенный шов, а не через узкие CRT-функции.

    python3 scripts/check_fs_seam.py            # гейт
    python3 scripts/check_fs_seam.py --selftest # правила проверяются сломанными фикстурами

Находка 6 аудита #21. Конвенция репозитория называет требование прямо, и для двух соседних её
половин — argv и переменных окружения — греп-гейты с позитивным контролем заведены давно
(`tree_invariants.sh argv|env`). Для файловой не было ни одного, и обходили её девять раз:
`std::remove` в трёх тестах достижений и в игре-образце.

Своим скриптом, а не пятым инвариантом в `tree_invariants.sh`: те четыре — семейство грепов, а
здесь нужен РАЗБОР СКОБОК. `std::remove(first, last, value)` из `<algorithm>` — законная форма, она
живёт в дереве, и гейт на подстроке требовал бы отписки за корректный код. Что считать обходом —
`scripts/fs_seam_rules.py`; здесь обход дерева и охранники, отличающие «находок нет» от «искать
было негде».
"""

import subprocess
import sys
from pathlib import Path

import py_utf8
from fs_seam_rules import FIX, SEAM, file_hits, seam_forms

EXTS = {".c", ".cc", ".cxx", ".cpp", ".h", ".hpp", ".inl", ".m", ".mm"}
# ЧЕТВЁРТАЯ копия списка корней; внешнего эталона у него нет, поэтому копии сверяются между
# собой (`scripts/check_tree_roots.py`, форма `mirrors-group`).
ROOTS = ("engine", "tools", "example_ugly_game", "platform", "docs/examples")

# Позитивный контроль: обход обязан видеть файлы шва, дерево — быть не пустым, а правило — что-то
# находить в САМОМ шве. Первого условия мало: файлы на месте, а регулярка имени могла сломаться и
# молчать неотличимо от чистого дерева.
MIN_FILES = 60


def audit(files):
    """files: {путь: текст}. Находки словами, файлы шва пропущены."""
    bad = []
    for path in sorted(files):
        if path in SEAM:
            continue
        hits, ok = file_hits(files[path])
        for line, why in hits:
            if line in ok:
                continue
            bad.append(f"{path}:{line}: {why}. {FIX}")
    return bad


def guards(files):
    """Отказы охранников: обход, промахнувшийся мимо дерева, обязан отличаться от чистого прогона."""
    missing = [p for p in SEAM if p not in files]
    if missing:
        return [f"обход не нашёл файл(ы) шва {', '.join(missing)}: искать было негде, "
                f"и 'находок нет' здесь значит 'ничего не проверено'"]
    if len(files) < MIN_FILES:
        return [f"обход дал {len(files)} файл(ов) при пороге {MIN_FILES}: он описывает не то "
                f"дерево, по которому его запустили"]
    inside = sum(seam_forms(files[p])[0] for p in SEAM)
    if not inside:
        return ["в самих файлах шва не найдено ни одного узкого вызова: правило имён сломано, "
                "и молчит оно неотличимо от чистого дерева"]
    return []


def scan(root):
    """Наши исходники глазами git — та же идиома, что у line_budget.py и check_hash_seam.py.

    Ненаписанное в индекс берётся наравне с индексом: иначе свежесозданный обход шва проходил бы
    локальный прогон молча и падал бы только на коммит-гейте.
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
            print(f"fs-seam: FAIL — {line}")
        return 1
    bad = audit(files)
    for line in bad:
        print(line)
    print(f"fs-seam: {'FAIL' if bad else 'PASS'} — {len(files)} файл(ов), находок: {len(bad)}")
    return 1 if bad else 0


def main(argv):
    py_utf8.enable()
    from check_fs_seam_selftest import selftest
    if "--selftest" in argv:
        return selftest(audit, guards, scan)
    # Та же дисциплина, что у ci_lint.py: сломанное правило молчит ровно так же, как чистое дерево.
    if selftest(audit, guards, scan, verbose=False):
        print("fs-seam: FAIL — сломаны сами правила, находкам верить нельзя")
        return 1
    return gate(Path(__file__).resolve().parent.parent)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
