#!/usr/bin/env python3
"""Инвариант: каждый чужой ассет игры записан в инвентарь лицензий, и титры сгенерированы из него.

    python3 scripts/check_asset_licenses.py             # гейт
    python3 scripts/check_asset_licenses.py --write     # перегенерировать NOTICES.txt и credits.txt
    python3 scripts/check_asset_licenses.py --selftest  # правила проверяются сломанными фикстурами

Спека #24, В2 — до первого ассета в дереве: файл без записи о происхождении — это файл, который
нельзя распространять, и узнают об этом не в коммите, а в чужом письме. Правила — в
`asset_licenses.py`, устройство и находки — `.context/gates/asset-licenses.md`.

Python ≥ 3.11 ради `tomllib`. Младший отказывает ДО импорта с названной версией: трейсбек
`ModuleNotFoundError` читался бы как сломанный гейт, а не как старый интерпретатор.
"""
import sys
from pathlib import Path

import py_utf8

MIN_PYTHON = (3, 11)


def version_refusal(version):
    if tuple(version[:2]) >= MIN_PYTHON:
        return None
    return (f"asset-licenses: FAIL — нужен Python >= {MIN_PYTHON[0]}.{MIN_PYTHON[1]} (tomllib), "
            f"запущен {version[0]}.{version[1]}")


def gate(root, write):
    from asset_licenses import audit
    from game_assets import asset_dirs
    dirs = asset_dirs(root)
    # Обход, промахнувшийся мимо дерева, обязан отличаться от чистого прогона.
    if not dirs:
        print(f"asset-licenses: FAIL — под {root}/games нет ни одного каталога assets/")
        return 1
    bad = []
    for base in dirs:
        bad += [f"{base.relative_to(root).as_posix()}: {msg}" for _, msg in audit(base, write)]
    for line in bad:
        print(line)
    print(f"asset-licenses: {'FAIL' if bad else 'PASS'} — {len(dirs)} каталог(ов) assets/, "
          f"находок: {len(bad)}")
    return 1 if bad else 0


def main(argv):
    py_utf8.enable()
    unknown = [a for a in argv if a not in ("--selftest", "--write")]
    if unknown or len(argv) > 1:
        print(f"asset-licenses: FAIL — аргументы {argv}: ждали --selftest, --write или ничего")
        return 2
    refusal = version_refusal(sys.version_info)
    if refusal:
        print(refusal)
        return 2
    from check_asset_licenses_selftest import selftest
    if "--selftest" in argv:
        return selftest(version_refusal)
    if selftest(version_refusal, verbose=False):
        print("asset-licenses: FAIL — сломаны сами правила, находкам верить нельзя")
        return 1
    return gate(Path(__file__).resolve().parent.parent, "--write" in argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
