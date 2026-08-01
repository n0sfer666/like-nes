#!/usr/bin/env python3
"""Прогоняет каждый TU из compile_commands.json и печатает только диагностики.

Зачем отдельно от build_check.sh. Сборка останавливается на первой упавшей цели, поэтому
компилятор, который не умеет собрать дерево ЦЕЛИКОМ, не даёт про него сказать ничего. А
несобираемость обычно локальна: на macOS gcc умирает на `.mm` (нет ObjC ARC) и на заголовках
Apple SDK с блоками — то есть ровно на трёх файлах из ста семидесяти. Обход по базе компиляции
компилирует TU по одному и независимо, так что падение одного не скрывает диагностики остальных.
Так был найден `-Wstringop-overflow` в tools/ide/game_child.cpp (разыменование nullptr), которого
не видел ни один раннер.

Каталог сборки обязан быть сконфигурирован ТЕМ ЖЕ компилятором, которым идёт обход: флаги в базе
проверяются на валидность, и clang-овские ключи g++ просто отвергнет.

    cmake -S . -B build-sweep -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER=g++-16 -DCMAKE_C_COMPILER=gcc-16 \
        -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF
    python3 scripts/tu_sweep.py build-sweep

Выход 0 — ни одной диагностики. Выход 1 — есть предупреждения. Выход 2 — обход не состоялся
(нет базы, нечего обходить). TU, который не собрался, печатается отдельно и НЕ роняет прогон:
несобираемость на чужом компиляторе — известное условие, ради которого скрипт и написан.
`-Werror` из команд снимается — почему, см. комментарий у WERROR ниже.
"""

import argparse
import json
import os
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Пропуск по умолчанию — не «шумные» файлы, а те, где чужой компилятор падает на языке, а не на
# нашем коде: ObjC++ требует ARC, которого нет у gcc. Список печатается поимённо: «пропущено
# молча» и «находок нет» с одного взгляда неразличимы, а это ровно та ошибка, ради которой в
# ci_lint.py заведено правило vacuous-gate.
SKIP_SUFFIX = (".mm", ".m")


def load_db(build_dir):
    path = os.path.join(build_dir, "compile_commands.json")
    if not os.path.exists(path):
        sys.stderr.write(
            "tu-sweep: нет {}\n"
            "  каталог не сконфигурирован либо собран без -DCMAKE_EXPORT_COMPILE_COMMANDS=ON\n"
            "  (генератор Ninja включает её сам)\n".format(path))
        sys.exit(2)
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def argv_of(entry):
    if "arguments" in entry:
        return list(entry["arguments"])
    return shlex.split(entry["command"])


def select(db, skip_patterns):
    jobs, skipped = [], []
    for entry in db:
        rel = os.path.relpath(entry["file"], ROOT)
        if "_deps" in rel.split(os.sep):
            continue  # вендоренное: пин на коммит — условие байт-детерминизма бейка, не наш код
        if rel.endswith(SKIP_SUFFIX):
            skipped.append((rel, "ObjC++ — чужому компилятору нечем (-fobjc-arc)"))
            continue
        hit = next((p for p in skip_patterns if p in rel), None)
        if hit:
            skipped.append((rel, "--skip {}".format(hit)))
            continue
        jobs.append((rel, entry))
    return jobs, skipped


# -Werror снимается намеренно. Обход — аудит, а не гейт: под -Werror первая же диагностика
# становится ошибкой, компиляция TU обрывается на ней, и остальные его находки не печатаются
# вовсе — то самое «увидеть ВСЕ предупреждения разом», ради которого в мануале owner-проверки
# описан LIKE_NES_WERROR=OFF в отдельном каталоге. Хуже того, промотированное предупреждение
# попадало бы в корзину «не собрался», где живёт неумение чужого компилятора разобрать файл, —
# два разных факта под одной строкой. Гейт остаётся у build_check.sh, здесь его роли нет.
WERROR = ("-Werror", "/WX", "/WX-")


def compile_one(job, extra):
    rel, entry = job
    argv = [a for a in argv_of(entry) if a not in WERROR]
    try:
        argv[argv.index("-o") + 1] = os.devnull
    except (ValueError, IndexError):
        return rel, None, "tu-sweep: в команде нет пары -o <файл>, TU пропущен\n"
    argv += extra
    proc = subprocess.run(argv, cwd=entry["directory"], capture_output=True, text=True)
    return rel, proc.returncode, proc.stderr


def main():
    ap = argparse.ArgumentParser(description="обход compile_commands.json по одному TU")
    ap.add_argument("build_dir", help="каталог сборки, сконфигурированный целевым компилятором")
    ap.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--skip", action="append", default=[], metavar="ПОДСТРОКА",
                    help="не компилировать TU, чей путь содержит подстроку (можно повторять)")
    ap.add_argument("--extra", action="append", default=["-Wall", "-Wextra"], metavar="ФЛАГ",
                    help="дописать флаг к каждой команде (по умолчанию -Wall -Wextra)")
    args = ap.parse_args()

    jobs, skipped = select(load_db(args.build_dir), args.skip)
    if not jobs:
        sys.stderr.write("tu-sweep: обходить нечего — в базе нет наших TU\n")
        sys.exit(2)

    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(lambda j: compile_one(j, args.extra), jobs))

    warned = [(rel, err) for rel, _, err in results if "warning:" in err]
    failed = [(rel, err) for rel, rc, err in results if rc != 0]

    for rel, err in warned:
        print("\n===== {}".format(rel))
        print(err.rstrip())
    for rel, err in failed:
        print("\n----- не собрался: {}".format(rel))
        print(err.rstrip()[:2000])
    for rel, why in skipped:
        print("      пропущен: {} — {}".format(rel, why))

    print("\ntu-sweep: TU {}, с предупреждениями {}, не собрались {}, пропущено {}".format(
        len(jobs), len(warned), len(failed), len(skipped)))
    sys.exit(1 if warned else 0)


if __name__ == "__main__":
    main()
