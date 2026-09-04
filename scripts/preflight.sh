#!/usr/bin/env bash
# Всё, что можно узнать про будущий прогон CI, не запуская CI. Гоняется перед push.
#
# Смысл: круг «push → 20 минут на трёх ОС → красный по опечатке» — самая дорогая ошибка в этом
# проекте. Поэтому этапы НЕ останавливают друг друга: один прогон обязан выдать все находки разом,
# иначе экономия превращается в ту же последовательность кругов, только локальную.
#
# Здесь живёт ПОРЯДОК и условия этапов, а не сами проверки: тела, доросшие до собственного имени,
# вынесены в свои скрипты (`check_dco.sh`, `check_goldens.sh`, `check_debug_golden.sh`,
# `check_library_bundle.sh`) и зовутся как
# внешние команды. Так
# каждую можно прогнать по одной, не выбирая между «весь preflight» и «руками из истории шелла».
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

# Первым и на полсекунды — обоснование в шапке самого скрипта: коммит без `-s` чинится только
# переписыванием SHA всей ветки, и узнавать про него в конце раунда дороже всего остального здесь.
stage "Подпись DCO на коммитах ветки" bash scripts/check_dco.sh

stage "Линтер workflow — самопроверка правил" python3 scripts/ci_lint.py --selftest
stage "Линтер workflow — .github/workflows" python3 scripts/ci_lint.py

# Те же скрипты, что и шаги CI. Гейты статические, знание о нарушении полностью доступно
# локально — узнавать про него из красного раннера значило платить двадцать минут за находку
# на полсекунды.
stage "Статические инварианты дерева (швы, зависимости, ASCII-вывод)" \
    bash scripts/tree_invariants.sh
stage "Бюджет длины файлов — самопроверка правил" \
    python3 scripts/line_budget.py --selftest
stage "Бюджет длины файлов — дерево" python3 scripts/line_budget.py
stage "Гейт релизного пакета — самопроверка правил" bash scripts/check_release_selftest.sh
stage "Гейт контейнерного релиза — самопроверка правил" \
    bash scripts/check_release_container_selftest.sh
# Правила контейнерного пути читают ФАЙЛЫ (пин базы, монтирование, коды отказа) и демона не требуют.
# Живая сборка осталась за `--live`: она поднимает docker и качает базу, а preflight — перед push.
stage "Правила контейнерного релиза (#20, вертикаль 2)" bash scripts/check_release_container.sh
# Обход perf_sweep.sh руками не гоняют, и его контроль шума живьём не воспроизвести — чужой процесс
# по заказу машину не займёт. Значит, единственное, что стоит между «правило работает» и
# «правило молчит всегда», это фикстуры, и место им там же, где остальным самопроверкам.
stage "Контроль шума в замере — самопроверка правил" bash scripts/perf_sweep_selftest.sh
# Наблюдатель CI по сети сюда не попадает — гейт без сети, — но его СУЖДЕНИЕ о прогонах чистое и
# проверяется фикстурами. Место фикстурам там же, где остальным самопроверкам.
stage "Вердикт о прогонах CI — самопроверка правил" bash scripts/ci_watch_selftest.sh

# Чем ставить пропущенный инструмент — вопрос к МАШИНЕ, а не к одной ОС: обе строки ниже до
# 2026-08-29 звали `brew`, и на Nobara совет был неисполним, отчего пропуск читался как «этого
# гейта здесь и не бывает», хотя shellcheck лежит в репозитории дистрибутива. Имя пакета врозь с
# именем команды: в Fedora пакет зовётся ShellCheck. Пусто вместо пакета значит «в этом менеджере
# его нет» — тогда называется источник, а не выдумывается формула.
install_hint() {  # $1 brew-формула, $2 пакет dnf, $3 пакет apt, $4 запасной источник
    if command -v brew >/dev/null; then echo "brew install $1"
    elif [ -n "$2" ] && command -v dnf >/dev/null; then echo "sudo dnf install $2"
    elif [ -n "$3" ] && command -v apt-get >/dev/null; then echo "sudo apt-get install $3"
    else echo "$4"; fi
}

if command -v actionlint >/dev/null; then
    # severity=warning: SC2086 (несплитящиеся "$VAR") в этом файле осознан — им собирается
    # командная строка, — а вот warning и выше означает реальный дефект скрипта.
    #
    # На Windows интеграция с линтером шелла снимается, и это не косметика: actionlint кормит его
    # через пайп и на git-bash не дожидается ответа НИКОГДА — прогон 2026-09-01 простоял час,
    # накопив 0.9 с процессорного времени, то есть висел, а не считал. Разведено врозь: без него
    # в PATH тот же вызов отдаёт вердикт мгновенно, с ним — не отдаёт вовсе, а `-shellcheck=`
    # при том же PATH снова мгновенен. Сам линтер здесь ни при чём: из stdin он отвечает.
    # Скрипты ВНУТРИ workflow остаются за CI (там этап идёт на Linux), гейт-скрипты дерева
    # закрывает отдельный этап ниже, и он на Windows работает. Висящий гейт хуже падающего:
    # падение читается как находка, час тишины — как «ещё считает».
    al_title="actionlint + shellcheck (severity=warning)"
    al_cmd=(env SHELLCHECK_OPTS="--severity=warning" actionlint)
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            al_title="actionlint (без shellcheck: связка виснет на Windows)"
            al_cmd=(actionlint -shellcheck=) ;;
    esac
    stage "$al_title" "${al_cmd[@]}"
else
    skip "actionlint" "не установлен ($(install_hint actionlint '' '' 'релиз с github.com/rhysd/actionlint/releases либо go install github.com/rhysd/actionlint/cmd/actionlint@latest'))"
fi

if command -v shellcheck >/dev/null; then
    # Сами гейты — тоже скрипты, и ошибка в них тихо превращает проверку в декорацию. Скрипты
    # владельца здесь по той же причине и с добавкой: их гоняют на чужой машине, где сломанный
    # шаг выглядит сломанным ДЕРЕВОМ, а не сломанным скриптом.
    stage "shellcheck скриптов гейтов" \
        shellcheck --severity=warning scripts/build_check.sh scripts/preflight.sh \
                   scripts/check_dco.sh scripts/check_goldens.sh scripts/check_debug_golden.sh \
                   scripts/check_library_bundle.sh \
                   scripts/owner_check.sh scripts/gate8_e2e.sh \
                   scripts/tree_invariants.sh \
                   scripts/ci_watch.sh scripts/ci_watch_lib.sh scripts/ci_watch_selftest.sh \
                   scripts/ci_watch_wait_selftest.sh \
                   scripts/perf_sweep.sh scripts/perf_sweep_lib.sh \
                   scripts/perf_sweep_guards.sh scripts/perf_sweep_report.sh \
                   scripts/perf_sweep_selftest.sh \
                   scripts/release*.sh scripts/check_release*.sh
    # Семейство релиза берётся ШАБЛОНОМ, а не списком: за две вертикали спеки #20 оно выросло с
    # трёх файлов до четырнадцати, и каждый новый гейт приходилось дописывать сюда руками. Забытая
    # строка не падает — она молча выводит скрипт из-под проверки, ровно тот класс, ради которого
    # в `ci_lint.py` заведено правило `list-drift`. Шаблон покрывает и ненаписанный ещё файл.
else
    skip "shellcheck" "не установлен ($(install_hint shellcheck ShellCheck shellcheck 'пакет shellcheck вашего дистрибутива'))"
fi

# Каталог build-ci, а не build: `build` — умолчание коммит-гейта, и оставлять его в урезанном
# наборе опций значит выключить гейт для imgui, miniaudio и wasm до следующей ручной настройки.
stage "Сборка headless (набор целей CI)" build_config build-ci \
    -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF
stage "Сборка полного набора опций (imgui, miniaudio, wasm, IDE)" build_config build-full

# Три проверки продуктов сборки — по одной, чтобы имя упавшей стояло в логе (обоснование и сами
# литералы — в `check_goldens.sh`). Порядок: Debug требует своей сборки и стоит дороже всех,
# бандл сверяется продуктом build-ci и потому идёт после сборок, а не до них.
stage "Голден физики совпадает в Debug (инвариант 1 спеки #15)" \
    bash scripts/check_goldens.sh debug
stage "8 голденов ядра целы (гейт 2 спеки #11, все восемь локально)" \
    bash scripts/check_goldens.sh core
stage "Бандл игры сверен с исходниками (tool-free секции)" \
    bash scripts/check_goldens.sh bundle
# У релиза СВОЙ каталог сборки (build-release), и одолжить ему build-full нельзя, хотя состав целей
# тот же: конфигурация релиза несёт LIKE_NES_RELEASE=ON и версию пакета, а кеш CMake её переживает —
# следующий прогон этапа «Сборка полного набора опций» тех же флагов уже не передаёт и молча
# собирает не то, что заявляет, штампуя игру-образец версией гейта. Минута сборки этого не стоит.
stage "Релизный пакет: состав, штамп, воспроизводимость (#20)" bash scripts/check_release.sh

# Диагностики компиляторов не совпадают: -Wformat-truncation есть у gcc и нет у clang, и именно
# он поймал усечение snprintf, которое трижды прошло локальную сборку. Если gcc в системе есть —
# третья конфигурация закрывает этот класс до CI.
# Перебор от старших версий: новый gcc приносит НОВЫЕ диагностики, а именно они и валят сборку
# на роллинг-дистрибутиве владельца, пока раннеры ubuntu-latest молчат. Список без верхней
# границы смысла не имеет — дописывать по мере выхода.
GXX=""
for v in 18 17 16 15 14; do
    GXX=$(command -v "g++-$v") && break
    GXX=""
done
gcc_build() {
    # Подменяется ТОЛЬКО CXX: наш код весь C++, а вендоренный C и так собирается с погашенными
    # предупреждениями — брать под gcc ещё и его значит чинить чужие сборки без выигрыша.
    local base=${GXX##*/}
    CXX="$GXX" CC="${GXX%/*}/${base/g++/gcc}" build_config build-gcc \
        -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF
}
if [ "$(uname -s)" = "Darwin" ]; then
    # Homebrew-gcc на macOS это дерево не собирает В ПРИНЦИПЕ, и дело не в наших предупреждениях:
    # у него нет ObjC ARC (`-fobjc-arc` для source_gamepad_macos.mm — unrecognized option), а
    # заголовки Apple SDK написаны на блоках (`^`), которых gcc не знает — platform_watch_macos.cpp
    # умирает внутри CoreServices. Гнать этап значило бы красить preflight в красный чужой
    # несовместимостью. Несобираемость при этом локальна — три файла из ста семидесяти, — поэтому
    # большую часть класса всё же можно закрыть тут: scripts/tu_sweep.py компилирует TU по одному
    # и падение одного не скрывает диагностики остальных. Linux-only TU (evdev, X11) остаются за CI.
    skip "Сборка gcc" "на macOS gcc не собирает .mm и Apple SDK — переносимые TU закрывает scripts/tu_sweep.py, Linux-only остаются за CI"
elif [ -n "$GXX" ] && ! "$GXX" --version 2>/dev/null | grep -qi clang; then
    stage "Сборка gcc (диагностики, которых нет у clang)" gcc_build
else
    skip "Сборка gcc" "gcc не найден ($(command -v dnf >/dev/null && echo 'sudo dnf install gcc-c++' || echo 'apt-get install g++')) — класс -Wformat-truncation остаётся за CI"
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
