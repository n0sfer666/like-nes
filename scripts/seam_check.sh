#!/usr/bin/env bash
# Инвариант 2 спеки #12: условная компиляция по ОС живёт ТОЛЬКО в engine/platform/*.
# Проверяется грепом, а не обещанием, и до сборки — нарушение видно первым же шагом.
#
# Гейт жил телом шага в ci.yml, и цена этого измерена: нарушение приезжало в CI, круг
# «push → 20 минут → красный» тратился на однострочную находку, которую видно локально за
# полсекунды. Отдельным скриптом его гоняет и CI, и preflight — одним и тем же кодом, а не
# двумя копиями, разъезжающимися с первой правкой.
set -uo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1

# Совпадать обязан ЦЕЛЫЙ идентификатор, а не подстрока: `GLFW_PLATFORM_WIN32` — рантайм-константа
# GLFW, никакой условной компиляции в ней нет, а подстрочный `_WIN32` её ловил. Ограничители
# написаны классами символов, а не `\b`: `\b` — GNU-расширение, а гейт обязан вести себя
# одинаково у всех, кто его запускает (правило `portability` в ci_lint.py про то же).
PATTERN='(^|[^A-Za-z0-9_])(_WIN32|_MSC_VER|__APPLE__|__linux__|__unix__)([^A-Za-z0-9_]|$)'
# Расширения фильтрует сам grep: постфильтр по строке пропускал бы .c/.cc/.cxx/.m (а
# engine/audio/*_impl.c в дереве есть) и заодно ловил бы содержимое строк, где просто
# встречается ".cpp:" — фикстуры парсера диагностик.
EXT="--include=*.c --include=*.cc --include=*.cxx --include=*.cpp --include=*.h
     --include=*.hpp --include=*.mm --include=*.m --include=*.inl"

# Позитивный контроль: внутри шва условная компиляция обязана быть. Ноль совпадений здесь значит,
# что сломан сам поиск (промах регулярки, переименованный корень), — без этой проверки гейт после
# такой поломки зелёный навсегда. Не гипотеза: под zsh незакавыченный $EXT разъезжается глобом, и
# гейт печатает PASS, не прочитав ни файла.
# shellcheck disable=SC2086
SEAM=$(grep -rnE "$PATTERN" $EXT engine/platform | wc -l | tr -d '[:space:]')
if [ "$SEAM" -lt 3 ]; then
    echo "ifdef gate: search itself is broken (seam hits: $SEAM)"
    exit 1
fi

# shellcheck disable=SC2086
HITS=$(grep -rnE "$PATTERN" $EXT engine tools example_ugly_game | grep -v '^engine/platform/' || true)
if [ -n "$HITS" ]; then
    echo "OS #ifdef leaked outside the platform seam:"
    echo "$HITS"
    exit 1
fi

echo "platform seam: PASS (no OS conditionals outside engine/platform, seam hits: $SEAM)"
