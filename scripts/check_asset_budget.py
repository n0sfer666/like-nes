#!/usr/bin/env python3
"""Инвариант: чужие ассеты одной игры (`games/<игра>/assets/`) весят не больше 30 МиБ.

    python3 scripts/check_asset_budget.py             # гейт
    python3 scripts/check_asset_budget.py --selftest  # правило проверяется на фикстурах

Спека #24, В2: ассеты лежат в git, и каждый мегабайт навсегда остаётся в истории каждого клона —
удаление файла его не возвращает. Порог — на всю игру, а не на раунд; если паки пяти уровней не
влезут, это решение #28 (Git LFS или скачивание со сверкой sha256), а не тихое поднятие числа.
Считается логический размер всех файлов каталога, включая инвентарь и тексты лицензий.
"""
import os
import sys
import tempfile
from pathlib import Path

import py_utf8
from game_assets import asset_dirs, files_under

LIMIT = 30 * 1024 * 1024


def size_of(base):
    return sum(os.lstat(base / rel).st_size for rel in files_under(base))


def audit(base):
    size = size_of(base)
    if size <= LIMIT:
        return None
    return (f"{size / 1048576:.2f} MiB ({size} bytes) > {LIMIT // 1048576} MiB; "
            "the budget is per game, see spec #24 open question 1")


def gate(root):
    dirs = asset_dirs(root)
    if not dirs:
        print(f"asset-budget: FAIL — под {root}/games нет ни одного каталога assets/")
        return 1
    bad = [f"{d.relative_to(root).as_posix()}: {msg}" for d in dirs if (msg := audit(d))]
    for line in bad:
        print(line)
    print(f"asset-budget: {'FAIL' if bad else 'PASS'} — {len(dirs)} каталог(ов) assets/, "
          f"находок: {len(bad)}")
    return 1 if bad else 0


def _sized(base, sizes):
    for rel, size in sizes.items():
        (base / rel).parent.mkdir(parents=True, exist_ok=True)
        with open(base / rel, "wb") as f:
            f.truncate(size)


# (метка, {путь: размер}, отказ ли). Граница — на концах: ровно предел проходит, байт сверху — нет.
CASES = [
    ("пусто", {}, False),
    ("ровно 30 МиБ в двух каталогах", {"a/x.png": LIMIT - 10, "b/c/y.png": 10}, False),
    ("30 МиБ и один байт", {"a/x.png": LIMIT - 10, "b/c/y.png": 11}, True),
    ("каталог 31 МиБ", {"a/x.png": 31 * 1024 * 1024}, True),
]


def selftest(verbose=True):
    failures = 0
    for title, sizes, refused in CASES:
        with tempfile.TemporaryDirectory() as tmp:
            _sized(Path(tmp), sizes)
            ok = (audit(Path(tmp)) is not None) == refused
        failures += 0 if ok else 1
        if verbose or not ok:
            print(f"  {'ok  ' if ok else 'FAIL'}  {title}")
    with tempfile.TemporaryDirectory() as tmp:
        names = ["neon", "alpha", "kite", "zulu", "bravo", "mike", "echo"]
        _sized(Path(tmp), {**{f"games/{n}/assets/x.png": 1 for n in names}, "games/solo/z": 1})
        ok = [d.parent.name for d in asset_dirs(tmp)] == sorted(names)
    failures += 0 if ok else 1
    if verbose or not ok:
        print(f"  {'ok  ' if ok else 'FAIL'}  обход: games/*/assets по именам, игра без assets/")
    if verbose or failures:
        print(f"asset-budget selftest: {'FAIL' if failures else 'PASS'} — "
              f"{len(CASES) + 1} кейсов, провалов: {failures}")
    return 1 if failures else 0


def main(argv):
    py_utf8.enable()
    if argv not in ([], ["--selftest"]):
        print(f"asset-budget: FAIL — аргументы {argv}: ждали --selftest или ничего")
        return 2
    if "--selftest" in argv:
        return selftest()
    if selftest(verbose=False):
        print("asset-budget: FAIL — сломано само правило, находкам верить нельзя")
        return 1
    return gate(Path(__file__).resolve().parent.parent)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
