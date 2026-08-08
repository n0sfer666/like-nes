#!/usr/bin/env bash
# Сборочный гейт: ноль ошибок И ноль предупреждений. Аналог тайпчекера в strict-режиме —
# без зелёного прогона коммит запрещён (см. CLAUDE.md, «Сборочный гейт»).
#
# Почему не хватает одного `cmake --build`: -Werror ловит предупреждения КОМПИЛЯТОРА, но мимо
# него проходят драйвер (MSVC D9xxx: «/O2 переопределён /Od»), линкер и сам CMake. Поэтому
# гейт двойной — код возврата сборки И чистота её лога.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-build}
# Урезанный набор опций разрешён только тому, кто заявил его явно (шаг Configure в CI, headless-
# этап preflight). Умолчание — полный набор: иначе каталог, однажды сконфигурированный без
# imgui/miniaudio/wasm, навсегда делает гейт зелёным по целям, которых он не собирает.
SUBSET=${LIKE_NES_BUILD_SUBSET:-0}
FEATURES="AUDIO_MINIAUDIO PLUGIN_UI PLUGIN_WASM"
# Список целей — для шагов CI, поднимающих ОДНУ опцию в отдельном каталоге (miniaudio, imgui):
# там интересна ровно её ветка, а собирать ради неё дерево второй раз — двадцать минут на трёх ОС.
# Умолчание пустое: каталог коммит-гейта собирается целиком, иначе гейт зеленел бы по недостроенному.
TARGETS=${LIKE_NES_BUILD_TARGETS:-}
# Конфигурация сборки. Умолчание Release, потому что коммит-гейт про неё и договаривались; Debug
# заявляется явно — его гоняет шаг CI, сверяющий голден физики на -O0 (инвариант 1 спеки #15
# требует совпадения в Debug И Release, а расхождение между уровнями оптимизации в целочисленной
# арифметике означает UB, а не «погрешность»).
BUILD_TYPE=${LIKE_NES_BUILD_TYPE:-Release}
cd "$ROOT" || exit 1

CACHE="$BUILD_DIR/CMakeCache.txt"
if [ ! -f "$CACHE" ]; then
    echo "build-check: конфигурирую $BUILD_DIR ($BUILD_TYPE, первый прогон)"
    cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE" || exit 1
else
    RESTORE=()
    # Иначе гейт молча пройдёт по объектам, собранным без -Werror: ninja пересоберёт их
    # сам, увидев изменившуюся командную строку.
    grep -q '^LIKE_NES_WERROR:BOOL=OFF' "$CACHE" && RESTORE+=(-DLIKE_NES_WERROR=ON)
    if [ "$SUBSET" != "1" ]; then
        for opt in $FEATURES; do
            grep -q "^$opt:BOOL=OFF" "$CACHE" && RESTORE+=("-D$opt=ON")
        done
    fi
    if [ ${#RESTORE[@]} -gt 0 ]; then
        echo "build-check: восстанавливаю набор опций в $BUILD_DIR: ${RESTORE[*]}"
        cmake -S . -B "$BUILD_DIR" "${RESTORE[@]}" >/dev/null || exit 1
    fi
fi

# Позитивный контроль на сам гейт: записи в кеше нет вовсе, если warnings.cmake перестал
# подключаться (переезд include, ранний return). Греп на «=OFF» такую пропажу не видит, и гейт
# собирал бы на уровне предупреждений по умолчанию — зелёный навсегда.
if ! grep -q '^LIKE_NES_WERROR:BOOL=ON' "$CACHE"; then
    echo "build-check: FAIL — в кеше нет LIKE_NES_WERROR=ON: строгие флаги не подключены"
    exit 1
fi

# Каталог, однажды сконфигурированный не той конфигурацией, молча остаётся ею навсегда: ветка выше
# трогает кеш, только когда его нет. Гейт, который договаривались считать релизным, проверял бы -O0
# и не сказал бы об этом ни строкой — ровно тот класс молчания, из-за которого набор опций
# восстанавливается принудительно. Расхождение чинится на месте, а не сообщением «переконфигурируй».
if ! grep -q "^CMAKE_BUILD_TYPE:STRING=$BUILD_TYPE\$" "$CACHE"; then
    echo "build-check: $BUILD_DIR сконфигурирован не как $BUILD_TYPE — переконфигурирую"
    cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" >/dev/null || exit 1
fi

LOG=$(mktemp)
trap 'rm -f "$LOG"' EXIT
VERDICT="$BUILD_DIR/.build_check_verdict"

# Предупреждение драйвера или линкера не роняет цель — её объект остаётся свежим, и при
# следующем прогоне ninja эту цель не трогает: лог чист не потому, что чист код. Правки в
# СОСЕДНЕЙ цели этого не лечат, поэтому после провала пересобираем начисто.
if [ "$(cat "$VERDICT" 2>/dev/null || true)" = "fail" ]; then
    echo "build-check: прошлый прогон нашёл предупреждения — пересобираю начисто"
    cmake --build "$BUILD_DIR" --target clean >/dev/null 2>&1
fi

BUILD_ARGS=(--build "$BUILD_DIR" -j)
if [ -n "$TARGETS" ]; then
    read -ra TARGET_LIST <<< "$TARGETS"
    BUILD_ARGS+=(--target "${TARGET_LIST[@]}")
fi
cmake "${BUILD_ARGS[@]}" 2>&1 | tee "$LOG"
if [ "${PIPESTATUS[0]}" -ne 0 ]; then
    echo fail > "$VERDICT"
    echo "build-check: FAIL — сборка не прошла"
    exit 1
fi

# Шаблон привязан к формам диагностик, а не к слову: в дереве есть cmake/warnings.cmake, и
# грепом по «warning» гейт красил бы сам себя.
WARNINGS=$(grep -inE ': warning[ :]|warning [CD][0-9]{4}|cmake warning|предупреждение [CD][0-9]{4}' \
    "$LOG" || true)
if [ -n "$WARNINGS" ]; then
    echo "$WARNINGS"
    echo fail > "$VERDICT"
    echo "build-check: FAIL — в логе сборки есть предупреждения"
    exit 1
fi

# Вердикт — про каталог целиком, а таргетный прогон видел лишь часть его целей: записав `pass`,
# он стёр бы `fail` предыдущего полного прогона, и тот не сделал бы чистую пересборку. Запись
# `fail` остаётся безусловной: предупреждение в подмножестве — предупреждение и в каталоге.
if [ -z "$TARGETS" ]; then
    echo pass > "$VERDICT"
fi
echo "build-check: PASS — сборка без ошибок и без предупреждений"
