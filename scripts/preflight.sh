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

# Единственный job CI, который живёт не в ci.yml (.github/workflows/dco.yml). Он срабатывает
# только на pull_request, поэтому коммит без `-s` лежит в ветке незамеченным до самого раунда и
# всплывает поверх всей проделанной работы. Чинится он ровно одним способом — rebase + force-push,
# то есть переписыванием ВСЕХ SHA ветки, после чего протухают ссылки на коммиты в ADR, теле PR и
# dev-log. Полсекунды здесь против переписанной истории там.
dco_signoff() {
    local base_ref=main
    git rev-parse --verify -q origin/main >/dev/null && base_ref=origin/main
    local base
    base=$(git merge-base "$base_ref" HEAD 2>/dev/null) || {
        echo "база $base_ref недоступна — проверять подпись не от чего"
        return 1
    }
    local shas
    shas=$(git rev-list --no-merges "$base"..HEAD)
    if [ -z "$shas" ]; then
        echo "поверх $base_ref нет коммитов — подписывать нечего"
        return 0
    fi
    # Автор ИЛИ коммиттер — тот же критерий, что у job'а: локально строже значило бы отбивать то,
    # что CI пропускает, и расхождение пришлось бы разбирать на раннере.
    local fail=0 n=0 sha author committer signers
    for sha in $shas; do
        n=$((n + 1))
        author=$(git show -s --format='%an <%ae>' "$sha" | tr '[:upper:]' '[:lower:]')
        committer=$(git show -s --format='%cn <%ce>' "$sha" | tr '[:upper:]' '[:lower:]')
        signers=$(git show -s --format='%(trailers:key=Signed-off-by,valueonly,unfold)' "$sha" |
            tr -d '\r' | tr '[:upper:]' '[:lower:]')
        if [ -z "$signers" ]; then
            printf '  %s — нет Signed-off-by\n' "$(git show -s --format='%h %s' "$sha")"
            fail=1
        elif ! grep -qxF "$author" <<<"$signers" && ! grep -qxF "$committer" <<<"$signers"; then
            printf '  %s — подпись не совпадает ни с автором, ни с коммиттером\n' \
                "$(git show -s --format='%h %s' "$sha")"
            fail=1
        fi
    done
    if [ "$fail" -ne 0 ]; then
        printf 'Починка: git rebase --signoff %s && git push --force-with-lease\n' "$base"
        return 1
    fi
    printf 'подписаны все %d коммит(ов) поверх %s\n' "$n" "$base_ref"
}

stage "Подпись DCO на коммитах ветки" dco_signoff

stage "Линтер workflow — самопроверка правил" python3 scripts/ci_lint.py --selftest
stage "Линтер workflow — .github/workflows" python3 scripts/ci_lint.py

# Те же скрипты, что и шаги CI. Гейты статические, знание о нарушении полностью доступно
# локально — узнавать про него из красного раннера значило платить двадцать минут за находку
# на полсекунды.
stage "Статические инварианты дерева (швы, зависимости, ASCII-вывод)" \
    bash scripts/tree_invariants.sh

if command -v actionlint >/dev/null; then
    # severity=warning: SC2086 (несплитящиеся "$VAR") в этом файле осознан — им собирается
    # командная строка, — а вот warning и выше означает реальный дефект скрипта.
    stage "actionlint + shellcheck (severity=warning)" \
        env SHELLCHECK_OPTS="--severity=warning" actionlint
else
    skip "actionlint" "не установлен (brew install actionlint shellcheck)"
fi

if command -v shellcheck >/dev/null; then
    # Сами гейты — тоже скрипты, и ошибка в них тихо превращает проверку в декорацию. Скрипты
    # владельца здесь по той же причине и с добавкой: их гоняют на чужой машине, где сломанный
    # шаг выглядит сломанным ДЕРЕВОМ, а не сломанным скриптом.
    stage "shellcheck скриптов гейтов" \
        shellcheck --severity=warning scripts/build_check.sh scripts/preflight.sh \
                   scripts/owner_check.sh scripts/gate8_e2e.sh \
                   scripts/tree_invariants.sh
else
    skip "shellcheck" "не установлен (brew install shellcheck)"
fi

# Каталог build-ci, а не build: `build` — умолчание коммит-гейта, и оставлять его в урезанном
# наборе опций значит выключить гейт для imgui, miniaudio и wasm до следующей ручной настройки.
stage "Сборка headless (набор целей CI)" build_config build-ci \
    -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF
stage "Сборка полного набора опций (imgui, miniaudio, wasm, IDE)" build_config build-full

# Инвариант 1 спеки #15: голден обязан совпасть в Debug и Release. Расхождение между уровнями
# оптимизации в целочисленной арифметике — это UB, а не «погрешность», и локально оно проверяемо
# целиком: ни раннера, ни железа тут не нужно. Список — только цели, лежащие на пути СОСТОЯНИЯ:
# дерево Debug'ом собирать незачем. Таблица синуса в нём потому, что вращение считается ею, и её
# собственное расхождение между уровнями оптимизации обязано быть названо ею, а не голденом сцены.
STATE_TARGETS="framework_physics_test framework_physics_point_test framework_physics_sat_test framework_physics_gap_test framework_physics_order_test framework_physics_range_test framework_physics_clamp_test framework_physics_terms_test framework_physics_rt_test framework_physics_query_test framework_physics_overlap_test framework_physics_filter_test framework_physics_event_test framework_physics_stack_test framework_physics_sleep_test framework_physics_band_test framework_physics_wake_test framework_trig_test"
physics_debug_golden() {
    cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
        -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF >/dev/null || return 1
    BUILD_DIR=build-debug LIKE_NES_BUILD_SUBSET=1 LIKE_NES_BUILD_TYPE=Debug \
        LIKE_NES_BUILD_TARGETS="$STATE_TARGETS" bash scripts/build_check.sh || return 1
    local t bin out rc=0 golden=""
    for t in $STATE_TARGETS; do
        bin=$(find build-debug -maxdepth 2 -type f -name "$t" | head -1)
        if [ -z "$bin" ]; then echo "$t не собран"; rc=1; continue; fi
        out=$("$bin") || rc=1
        printf '%s\n' "$out" | grep -q ": PASS" || { printf '%s\n' "$out"; rc=1; }
        if [ "$t" = framework_physics_test ]; then
            golden=$(printf '%s\n' "$out" | grep -o 'physics-state-hash = 0x[0-9a-f]*')
        fi
    done
    # Тот же литерал, что у Release-шага в CI. Сверять Debug сам с собой значило бы проверять, что
    # -O0 воспроизводим, а не что он согласен с -O3, — то есть не проверять ничего.
    if [ "$golden" != "physics-state-hash = 0xbeb8e8bee1bba145" ]; then
        echo "Debug разошёлся с Release по голдену физики: '$golden'"
        rc=1
    fi
    return $rc
}
stage "Голден физики совпадает в Debug (инвариант 1 спеки #15)" physics_debug_golden

# Восемь голденов гейта 2 спеки #11 — ЦЕЛИКОМ, а не только физика. Этап заведён по конкретному
# промаху: правка округления в `fix32::operator*` (ядро, `engine/core/fixed.hpp`) сдвинула голден
# ядра И голден ввода, а локально не сработало ничего — физический голден-то сошёлся. Красное на
# трёх ОС за правку в общей арифметике — самый дорогой круг из тех, что этот файл существует, чтобы
# закрывать, и удивительным он был ровно потому, что арифметику правят не в той подсистеме, где
# краснеет.
#
# Хеш каждого проверяемого сверяется ЛИТЕРАЛОМ, тем же, что стоит в шаге CI. Отсутствие бинаря —
# ошибка, а не пропуск: «не собрано» и «сошлось» обязаны отличаться в выводе, иначе первая же
# опечатка в имени цели делает этап вечно зелёным.
core_goldens() {
    local rc=0 tmp
    check_hash() { # имя-цели ожидаемый-хеш [аргументы]
        local bin name="$1" want="$2" out rc
        shift 2
        bin=$(find build-full -maxdepth 2 -type f -name "$name" | head -1)
        if [ -z "$bin" ]; then echo "$name не собран"; return 1; fi
        out=$("$bin" "$@" 2>&1)
        rc=$?
        # Статус спрашивается НАРАВНЕ с хешем, и вторым его не назовёшь. Голден печатается раньше
        # остальных утверждений бинаря, поэтому прогон, упавший на любом из них, всё равно оставляет
        # в выводе нужную строку: грепом одним этап читал бы «хеш сошёлся» как «тест прошёл», а это
        # разные утверждения. Прогон при этом ОДИН — второй ради диагностики мог бы напечатать не то,
        # что было отбито.
        if [ $rc -eq 0 ] && printf '%s\n' "$out" | grep -q "$want"; then
            echo "  $name = $want"
            return 0
        fi
        echo "  $name НЕ ЗАКРЫТ (код возврата $rc, ожидался $want):"
        printf '%s\n' "$out" | grep -iE 'hash|golden|fail' | sed 's/^/    /'
        return 1
    }
    check_hash determinism_test 0x6c4b121dbb47d13b || rc=1
    check_hash input_determinism_test 0xcc26a1897a326f6f || rc=1
    check_hash audio_golden 0x2cf5b5597afa3241 || rc=1
    check_hash scene_roundtrip_test 0x2de54a36e54e0684 || rc=1
    check_hash achievements_test 0xe728fef199e87fc9 || rc=1
    check_hash game_sim_test 0x32a094e89eacf2f2 || rc=1
    # Пекарь пишет бандл в текущий каталог, поэтому зовётся из временного: рабочее дерево обязано
    # остаться чистым — гейт 4 спеки #11 требует, чтобы локальный прогон не порождал `git status`.
    tmp=$(mktemp -d) || return 1
    ( cd "$tmp" && "$PWD_ROOT/build-full/assetc" --synthetic syn.bundle 2>&1 ) |
        grep -q 0xf2255dc74fbdb6bc && echo "  assetc --synthetic = 0xf2255dc74fbdb6bc" || {
        echo "  assetc --synthetic РАЗОШЁЛСЯ с 0xf2255dc74fbdb6bc"
        rc=1
    }
    rm -rf "$tmp"
    # Восьмой — плагинный (`0x7ad0493f0f2ddf47`) — остаётся за CI: `plugin_wasm_test` принимает
    # аргументами `.wat` и `.so` и требует wasmtime C-API из `deps/`, которого на этой машине нет.
    # Названо вслух намеренно: «семь из восьми» и «восемь из восьми» обязаны различаться в логе.
    echo "  plugin (0x7ad0493f0f2ddf47) — только в CI: нужен wasmtime C-API из deps/"
    return $rc
}
PWD_ROOT=$PWD
stage "8 голденов ядра целы (гейт 2 спеки #11, семь из восьми локально)" core_goldens

# После сборки, потому что сверяет её продуктом. `game.bundle` лежит в git готовым, и перепечь его
# ЦЕЛИКОМ негде, кроме машины владельца: текстурная секция тянет tint и basisu. Секции, что пекутся
# чистыми парсерами, сверяются везде — без этого правка input.txt или achievements.txt без
# перепекания доезжает молча, а на бандле стоят и игра-образец, и sim-golden.
# Код возврата не различает «всё сошлось» и «сверять было нечего»: выпади секция из списка SECTIONS,
# бинарь честно напечатает меньшее число и вернёт ноль. Ассертится строка вместе с числом — тот же
# критерий, что в шаге CI, чтобы локально и на раннере отбивалось одно и то же.
verify_game_bundle() {
    local out
    out=$(build-ci/assetc --verify-game example_ugly_game/assets example_ugly_game/assets/game.bundle) || {
        printf '%s\n' "$out"
        return 1
    }
    printf '%s\n' "$out"
    case $out in
        *"verify: PASS - 2 tool-free section(s)"*) return 0 ;;
        *) echo "сверено не то число секций — список SECTIONS разошёлся с гейтом"; return 1 ;;
    esac
}
stage "Бандл игры сверен с исходниками (tool-free секции)" verify_game_bundle

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
