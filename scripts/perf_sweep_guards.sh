#!/usr/bin/env bash
# Проверки, отвечающие на ОДИН вопрос: имеет ли смысл начинать замер вообще. Отделены и от обхода, и
# от perf_sweep_lib.sh, потому что отличаются от всего остального последствием: находка тут делает
# недействительной ВСЮ таблицу целиком, а не одну строку. Контроль шума метит строку и прогон не
# валит; эти двое валят до первой пересборки, потому что после неё уже потрачены минуты.
#
# Обе заведены по случившемуся, а не придуманы:
#   - 22 августа владелец прогнал матрицу на Windows-коробке, стоявшей на коммите десятидневной
#     давности. Скрипт отработал молча и выдал таблицу СТАРОГО формата — без повторов, без медианы,
#     без контроля column. Понять это можно было единственным способом: сличить заголовок таблицы с
#     тем, что печатает нынешний код. Прогон пропал целиком.
#   - Тип сборки не проверялся вовсе. Каталог, сконфигурированный в Debug, дал бы числа в разы
#     медленнее, и КАЖДЫЙ контроль остался бы зелёным: ячейки замедляются вместе, отношения
#     сохраняются, column совпадает, разброс мал. Таблица выглядела бы честной и была бы неверной.
#
# Файл подключается через `.`, самостоятельно не запускается. Функции печатают диагноз в stderr и
# возвращают 1; словами о прерывании говорит вызывающий — у него есть die().

sweep_warn() { printf 'perf-sweep: ВНИМАНИЕ — %s\n' "$*" >&2; }
sweep_bad() { printf 'perf-sweep: %s\n' "$*" >&2; }

# Строка происхождения для пасты. Таблица, отправленная в чат, обязана нести свою версию с собой:
# без неё «это старый скрипт» — вывод из сличения заголовков глазами, то есть находка задним числом.
sweep_provenance() {
    local sha date dirty=""
    sha=$(git rev-parse --short HEAD 2>/dev/null) || sha="?"
    date=$(git log -1 --format=%ad --date=short 2>/dev/null) || date="?"
    git diff --quiet HEAD 2>/dev/null || dirty=", дерево ГРЯЗНОЕ"
    printf 'инструмент %s от %s%s' "$sha" "$date" "$dirty"
}

# Отказ, если checkout отстал от своей же ветки. Сравнение идёт ПОСЛЕ fetch: на коробке, которая
# давно не тянула, локальный origin/dev устарел ровно так же, как рабочее дерево, и сравнение с ним
# уверенно сказало бы «всё свежее» — тот же вакуумный зелёный, что ловит vacuous-gate в ci_lint.py.
# Сети нет — это НЕ находка: замер идёт, но громким баннером сказано, что версия не проверена.
sweep_guard_version() {
    local up head remote behind
    up=$(git rev-parse --abbrev-ref --symbolic-full-name '@{u}' 2>/dev/null) || up=""
    if [ -z "$up" ]; then
        sweep_warn "у ветки нет upstream — версию инструмента проверить нечем"
        return 0
    fi
    if ! GIT_TERMINAL_PROMPT=0 git fetch --quiet "${up%%/*}" 2>/dev/null; then
        sweep_warn "git fetch не прошёл (нет сети?) — версия инструмента НЕ проверена"
        return 0
    fi
    head=$(git rev-parse HEAD 2>/dev/null) || return 0
    remote=$(git rev-parse "$up" 2>/dev/null) || return 0
    [ "$head" = "$remote" ] && return 0
    if git merge-base --is-ancestor "$head" "$remote" 2>/dev/null; then
        behind=$(git rev-list --count "HEAD..$up" 2>/dev/null || printf '?')
        sweep_bad "checkout отстал от $up на $behind коммит(ов) — замер пошёл бы СТАРЫМ скриптом."
        sweep_bad "Именно так пропал прогон 2026-08-22: таблица вышла в старом формате."
        sweep_bad "Лечится одной командой:  git pull --ff-only"
        return 1
    fi
    sweep_warn "HEAD впереди $up или разошёлся с ним — замер идёт ВАШЕЙ версией скрипта"
    return 0
}

# Отказ, если каталог сборки не Release. Пустой CMAKE_BUILD_TYPE у мультиконфиг-генератора (Visual
# Studio, Xcode, Ninja Multi-Config) — тоже отказ: обход зовёт `cmake --build` БЕЗ `--config`, а там
# это молча значит Debug. Неизвестное ведёт себя как плохое: тип сборки, который не удалось
# прочитать, обязан остановить замер, а не пройти как Release по умолчанию.
sweep_guard_build_type() {
    local dir=$1 cache type gen
    cache="$dir/CMakeCache.txt"
    if [ ! -f "$cache" ]; then
        sweep_bad "в '$dir' нет CMakeCache.txt — тип сборки прочитать нечем"
        return 1
    fi
    type=$(grep -m1 '^CMAKE_BUILD_TYPE:' "$cache" | cut -d= -f2-)
    gen=$(grep -m1 '^CMAKE_GENERATOR:' "$cache" | cut -d= -f2-)
    [ "$type" = "Release" ] && return 0
    sweep_bad "каталог '$dir' собран как «${type:-<пусто>}» (генератор ${gen:-?}), а не Release."
    sweep_bad "Числа вышли бы в разы медленнее, и ВСЕ контроли остались бы зелёными: ячейки"
    sweep_bad "замедляются вместе, отношения сохраняются, column совпадает, разброс мал."
    sweep_bad "Лечится так:  cmake -S . -B $dir -G Ninja -DCMAKE_BUILD_TYPE=Release"
    return 1
}
