#!/usr/bin/env bash
# Гейт 2 спеки #19: каждый файл `docs/examples/*.cpp` собран целью `doc_example_<имя>`, запускается
# с нулевым кодом и печатает ровно то, что лежит рядом в `.out`.
#
# Врезки в руководствах берутся из этих же файлов (`check_docs_snippets.py`), и связка двух гейтов —
# весь смысл решения 5 спеки: «примерного кода» в документации нет. Показанный фрагмент собирается
# и работает, потому что его источник собирается и работает здесь.
#
# Каталог сборки гейт НЕ создаёт: его делает вызывающий (этап preflight или шаг CI), и промах
# каталога обязан быть слышен отказом, а не пропуском — то же решение, что у `check_goldens.sh`.
#
#   bash scripts/check_docs_examples.sh [каталог-сборки]     # по умолчанию build-full
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1
# shellcheck source=scripts/docs_examples_lib.sh
. "$ROOT/scripts/docs_examples_lib.sh"

# Относительный каталог резолвится от КОРНЯ, а не от cwd вызывающего (как и в check_docs_snippets.sh):
# CI передаёт `build`, preflight — `build-full`, и запуск гейта из другого каталога отдавал бы
# «каталога сборки нет», то есть ошибка употребления выглядела бы как незапущенная сборка.
BUILD="${1:-build-full}"
case "$BUILD" in /*) ;; *) BUILD="$ROOT/$BUILD" ;; esac
[ -d "$BUILD" ] || { ex_bad "каталога сборки $BUILD нет — примеры проверять не на чем"; exit 1; }

RC=0
assert_examples_present "$ROOT" || RC=1
assert_examples_paired "$ROOT" || RC=1
# Прогон примеров идёт даже после находки выше: непарный голден и упавший пример — разные новости,
# и одна не должна прятать другую.
assert_examples_run_all "$ROOT" "$BUILD" || RC=1

if [ "$RC" != 0 ]; then echo "docs-examples: FAIL" >&2; exit 1; fi
echo "docs-examples: PASS"
