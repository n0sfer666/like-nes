#!/usr/bin/env bash
# Всё, что можно узнать про будущий прогон CI, не запуская CI. Гоняется перед push.
#
# Смысл: круг «push → 20 минут на трёх ОС → красный по опечатке» — самая дорогая ошибка в этом
# проекте. Поэтому этапы НЕ останавливают друг друга: один прогон обязан выдать все находки разом,
# иначе экономия превращается в ту же последовательность кругов, только локальную.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1

FAILED=()
stage() {
    local title=$1; shift
    printf '\n\033[1m=== %s\033[0m\n' "$title"
    if "$@"; then
        printf '\033[32m--- OK: %s\033[0m\n' "$title"
    else
        printf '\033[31m--- ПРОВАЛ: %s\033[0m\n' "$title"
        FAILED+=("$title")
    fi
}

skip() { printf '\n\033[1m=== %s\033[0m\n\033[33m--- пропущено: %s\033[0m\n' "$1" "$2"; }

# Конфигурация headless повторяет шаг Configure в CI дословно: набор опций меняет состав целей,
# и предупреждение, живущее в выключенной цели, локально бы не всплыло.
build_config() {
    local dir=$1; shift
    # Конфигурируется безусловно. Пропуск при готовом кеше выглядит экономией, но тогда опции из
    # аргументов не применяются вовсе: каталог, созданный build_check.sh с дефолтами, молча
    # прикидывается headless-конфигурацией, и два этапа схлопываются в один — зелёный и пустой.
    cmake -S . -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Release "$@" >/dev/null || return 1
    # SUBSET=1: набор опций здесь задаётся аргументами осознанно, а build_check.sh по умолчанию
    # возвращает полный набор — иначе первый же preflight делал бы урезанным каталог коммит-гейта.
    BUILD_DIR="$dir" LIKE_NES_BUILD_SUBSET=1 bash scripts/build_check.sh
}

stage "Линтер workflow — самопроверка правил" python3 scripts/ci_lint.py --selftest
stage "Линтер workflow — .github/workflows" python3 scripts/ci_lint.py

if command -v actionlint >/dev/null; then
    # severity=warning: SC2086 (несплитящиеся "$VAR") в этом файле осознан — им собирается
    # командная строка, — а вот warning и выше означает реальный дефект скрипта.
    stage "actionlint + shellcheck (severity=warning)" \
        env SHELLCHECK_OPTS="--severity=warning" actionlint
else
    skip "actionlint" "не установлен (brew install actionlint shellcheck)"
fi

if command -v shellcheck >/dev/null; then
    # Сами гейты — тоже скрипты, и ошибка в них тихо превращает проверку в декорацию.
    stage "shellcheck скриптов гейтов" \
        shellcheck --severity=warning scripts/build_check.sh scripts/preflight.sh
else
    skip "shellcheck" "не установлен (brew install shellcheck)"
fi

# Каталог build-ci, а не build: `build` — умолчание коммит-гейта, и оставлять его в урезанном
# наборе опций значит выключить гейт для imgui, miniaudio и wasm до следующей ручной настройки.
stage "Сборка headless (набор целей CI)" build_config build-ci \
    -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF
stage "Сборка полного набора опций (imgui, miniaudio, wasm, IDE)" build_config build-full

# Диагностики компиляторов не совпадают: -Wformat-truncation есть у gcc и нет у clang, и именно
# он поймал усечение snprintf, которое трижды прошло локальную сборку. Если gcc в системе есть —
# третья конфигурация закрывает этот класс до CI.
GXX=$(command -v g++-15 || command -v g++-14 || true)
gcc_build() {
    # Подмена только в basename: каталог с `g++` в имени иначе давал бы несуществующий CC и
    # невнятный отказ конфигурации вместо честного «gcc не найден».
    local base=${GXX##*/}
    CXX="$GXX" CC="${GXX%/*}/${base/g++/gcc}" build_config build-gcc \
        -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF
}
if [ -n "$GXX" ] && ! "$GXX" --version 2>/dev/null | grep -qi clang; then
    stage "Сборка gcc (диагностики, которых нет у clang)" gcc_build
else
    skip "Сборка gcc" "gcc не найден (brew install gcc) — класс -Wformat-truncation остаётся за CI"
fi

printf '\n'
if [ ${#FAILED[@]} -eq 0 ]; then
    echo "preflight: PASS — локально проверено всё, что не требует раннеров GitHub"
    echo "Вне досягаемости остаются: MSVC /W4, Windows-раннер, lavapipe/Vulkan на Linux."
    exit 0
fi
printf 'preflight: FAIL — %d этап(ов):\n' "${#FAILED[@]}"
printf '  - %s\n' "${FAILED[@]}"
exit 1
