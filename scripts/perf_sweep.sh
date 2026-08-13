#!/usr/bin/env bash
# Цена шага физики на РАЗНЫХ значениях двух констант, снятая на одной машине за один заход.
#
# Зачем отдельный скрипт. Прогон владельца 2026-08-12 показал, что `VELOCITY_ITERATIONS` поднимали с
# 8 до 16, зная цену только на Apple M3 Pro (2.25 -> 3.64 мс, 22% бюджета кадра). На Intel UHD 620
# та же сцена стоит 9.594 мс среднего и 13.475 худшего из 16.67 — то есть решение о числе итераций
# принималось по самой быстрой машине набора, а цена на самой медленной не измерена НИ РАЗУ.
# Померить её руками — это правка двух заголовков, пересборка и прогон, повторённые по числу ячеек
# матрицы; на четвёртой ячейке владелец сбивается, а sed, молча не совпавший с константой, выдаёт
# четыре одинаковые строки, неотличимые от честного результата.
#
# Запуск (Linux/macOS, и git-bash на Windows — как owner_check.sh):
#   bash scripts/perf_sweep.sh                 # матрица по умолчанию: 16/8 итераций x 500/300 тел
#   bash scripts/perf_sweep.sh 16:500 8:500    # свой список ячеек «итерации:тела»
#
# Скрипт ВРЕМЕННО правит два заголовка и возвращает их обратно, поэтому он отказывается работать,
# если в них есть незакоммиченные изменения: иначе восстановление затёрло бы чужую работу.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1

BUILD_DIR=${BUILD_DIR:-build}
SOLVER="engine/framework/physics/solver.hpp"
LOAD="engine/framework/physics/framework_physics_load.hpp"
TARGET="framework_physics_perf_test"
# Разбор чисел и контроль шума живут отдельно и проверяются фикстурами без единой сборки.
. "$ROOT/scripts/perf_sweep_lib.sh"
# Пауза между сборкой и прогоном. Замер идёт сразу после компиляции ТОЙ ЖЕ цели, то есть на CPU,
# который только что держал все ядра занятыми: буст израсходован, вентилятор догоняет. Это
# единственное различие условий, которое у нас на руках, а разошлись прошлые числа владельца на той
# же коробке на 31%. Ноль оставлен для отладки самой обёртки, где время не мерят.
SETTLE_S=${SETTLE_S:-10}

die() {
    printf 'perf-sweep: %s\n' "$*" >&2
    exit 1
}

# Подстановка с ПРОВЕРКОЙ, что она случилась. Без неё скрипт после переименования константы честно
# собирал бы одно и то же четыре раза и печатал ровный результат — ту самую вакуумную зелень,
# которую ci_lint.py ловит правилом `vacuous-gate` в чужих workflow.
set_const() {
    local file=$1 name=$2 value=$3 tmp
    tmp="$file.sweep.tmp"
    sed -e "s/\(constexpr uint32_t $name = \)[0-9]*;/\1$value;/" "$file" >"$tmp" || die "sed упал на $file"
    mv "$tmp" "$file" || die "не удалось записать $file"
    grep -q "constexpr uint32_t $name = $value;" "$file" ||
        die "константа $name не встала в значение $value — подстановка промахнулась мимо $file"
}

binary() {
    local b="$BUILD_DIR/$TARGET"
    [ -x "$b" ] && {
        printf '%s' "$b"
        return 0
    }
    [ -x "$b.exe" ] && {
        printf '%s' "$b.exe"
        return 0
    }
    return 1
}

CELLS=("$@")
[ ${#CELLS[@]} -eq 0 ] && CELLS=(16:500 8:500 16:300 8:300)

command -v cmake >/dev/null 2>&1 || die "cmake не найден"
[ -f "$SOLVER" ] && [ -f "$LOAD" ] || die "заголовки не на месте: $SOLVER / $LOAD"
[ -d "$BUILD_DIR" ] || die "каталог сборки '$BUILD_DIR' не сконфигурирован (BUILD_DIR=... чтобы задать свой)"

# Windows: шелл, не видевший vcvars64.bat, компилятор не поднимет. Проверяется ОКРУЖЕНИЕ, а не PATH:
# путь к cl.exe cmake помнит абсолютным, поэтому падает не поиск бинаря, а сама компиляция — на
# отсутствии INCLUDE/LIB. Без этой проверки владелец получил бы двадцать строк лога ninja и чинил бы
# не то: ровно по этой причине owner_check.sh и gate8_e2e.sh на Windows зовутся действиями
# win-dev.bat, а не прямо. Проверка советующая: она называет процедуру, а не заменяет её.
case "$(uname -s 2>/dev/null)" in
    MINGW* | MSYS* | CYGWIN*)
        if grep -qi 'CMAKE_CXX_COMPILER:[^=]*=.*cl\.exe' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null &&
            [ -z "${INCLUDE:-}" ]; then
            printf 'perf-sweep: этот шелл не видел vcvars64.bat — cl.exe тут не соберёт ничего.\n' >&2
            printf 'Порядок на Windows (два шага, из корня дерева):\n' >&2
            printf '  scripts\\win-dev.bat shell\n' >&2
            printf '  "%%ProgramFiles%%\\Git\\bin\\bash.exe" scripts/perf_sweep.sh\n' >&2
            die "запуск прерван до первой правки заголовков"
        fi
        ;;
esac

# Отказ по грязным заголовкам. Восстановление в конце — это `git checkout --`, то есть откат к
# HEAD: на файле с чужой правкой он молча уничтожил бы её.
git diff --quiet HEAD -- "$SOLVER" "$LOAD" 2>/dev/null ||
    die "в $SOLVER или $LOAD есть незакоммиченные изменения — закоммить или отложи их, замер их затрёт"

restore() { git checkout -- "$SOLVER" "$LOAD" 2>/dev/null; }
trap restore EXIT INT TERM

ITERS=()
BODIES=()
WORST=()
MEAN=()
COL=()
VEL=()

for cell in "${CELLS[@]}"; do
    it=${cell%%:*}
    bo=${cell##*:}
    case "$it$bo" in
        *[!0-9]* | "") die "ячейка '$cell' не в форме «итерации:тела», например 16:500" ;;
    esac
    # Чётность обязательна: точки манифольда обходятся вперёд и назад по чётности итерации
    # (`solver.cpp`), и нечётное число тихо перекашивает решатель в одну сторону.
    [ $((it % 2)) -eq 0 ] || die "число итераций $it нечётное — решатель обходит точки манифольда по чётности"

    printf '\n=== итераций %s, тел %s\n' "$it" "$bo"
    set_const "$SOLVER" VELOCITY_ITERATIONS "$it"
    set_const "$LOAD" BODIES "$bo"

    if ! cmake --build "$BUILD_DIR" --target "$TARGET" >/dev/null 2>&1; then
        cmake --build "$BUILD_DIR" --target "$TARGET" 2>&1 | tail -20
        die "сборка цели $TARGET не прошла на ячейке $cell"
    fi
    bin=$(binary) || die "бинарь $TARGET не найден в $BUILD_DIR после сборки"
    [ "$SETTLE_S" -gt 0 ] && sleep "$SETTLE_S"

    out=$("$bin" 2>&1)
    rc=$?
    printf '%s\n' "$out"
    # 126/127 — «найден, но запуск запрещён» и «не найден»: на Windows так выглядит защитник на
    # свежесобранном неподписанном бинаре, а обход перепекает его на КАЖДОЙ ячейке. Без этой ветки
    # пустой вывод уехал бы в диагноз «цель изменила формат печати» — обвинение не того, ровно тот
    # дефект, который owner_check.sh уже разводил счётчиком BLOCKED.
    if [ $rc -eq 126 ] || [ $rc -eq 127 ]; then
        printf 'perf-sweep: запуск %s запрещён (код %d) — тест НЕ ВЫПОЛНЯЛСЯ.\n' "$bin" "$rc" >&2
        printf 'На Windows это защитник: PowerShell от администратора, ОДИН раз на машину —\n' >&2
        printf "  Add-MpPreference -ExclusionPath '%s'\n" "$ROOT" >&2
        die "разбор в docs/owner-verification.md, раздел про Defender"
    fi
    # Ненулевой возврат здесь ОЖИДАЕМ и не прерывает замер: утверждения гейта прибиты к 500 телам
    # (`heap.pairs > BODIES/2` и порог доли), и на урезанной сцене часть из них падает по построению.
    # Нас интересуют счётчики и время, а они печатаются до вердикта. Молчать об этом нельзя — строка
    # ниже отличает «сцена другая» от «замер сломан».
    [ $rc -ne 0 ] && printf '  (вердикт цели красный, rc=%d — на урезанной сцене это ожидаемо)\n' "$rc"

    line=$(printf '%s\n' "$out" | grep -m1 'heap: worst=')
    counters=$(printf '%s\n' "$out" | grep -m1 'heap: bodies=')
    col=$(printf '%s\n' "$out" | grep -m1 'column: worst=')
    [ -n "$line" ] && [ -n "$counters" ] && [ -n "$col" ] ||
        die "в выводе нет строк замера — цель изменила формат печати"

    ITERS+=("$it")
    BODIES+=("$bo")
    WORST+=("$(printf '%s' "$line" | sed -e 's/.*worst=\([0-9.]*\).*/\1/')")
    MEAN+=("$(printf '%s' "$line" | sed -e 's/.*mean=\([0-9.]*\).*/\1/')")
    COL+=("$(printf '%s' "$col" | sed -e 's/.*mean=\([0-9.]*\).*/\1/')")
    VEL+=("$(printf '%s' "$counters" | sed -e 's/.*vel=\([0-9]*\).*/\1/')")
done

restore
trap - EXIT INT TERM
cmake --build "$BUILD_DIR" --target "$TARGET" >/dev/null 2>&1

# Позитивный контроль всего замера: две ячейки с разным числом итераций обязаны дать РАЗНОЕ число
# проекций решателя. Совпадение означает, что пересборка не доехала и все строки таблицы сняты с
# одного бинаря, — а таблица при этом выглядит идеально ровной.
n=${#ITERS[@]}
same=0
for ((i = 0; i < n; i++)); do
    for ((j = i + 1; j < n; j++)); do
        [ "${BODIES[$i]}" = "${BODIES[$j]}" ] && [ "${ITERS[$i]}" != "${ITERS[$j]}" ] &&
            [ "${VEL[$i]}" = "${VEL[$j]}" ] && same=1
    done
done

pairs=()
for ((i = 0; i < n; i++)); do pairs+=("${BODIES[$i]}:${COL[$i]}"); done
NOISY=" $(column_noise "${pairs[@]}" | tr '\n' ' ')"

printf '\n=== Итог (бюджет кадра %d.%02d мс)\n' $((BUDGET_US / 1000)) $((BUDGET_US % 1000 / 10))
# Судейское число — mean, и оно стоит первым: на трёх повторах одной ячейки среднее сошлось до 0.8%,
# а худшее разошлось на 20% (macOS, 2026-08-13) — при том, что бинарь и сцена те же. Худшее не
# выброшено: оно наблюдаемый факт про машину, просто не критерий, по которому назначают число тел.
printf 'итераций  тел   mean, мс  %% бюдж   worst, мс  %% бюдж   vel       контроль\n'
noisy=0
for ((i = 0; i < n; i++)); do
    peers=0
    for ((j = 0; j < n; j++)); do [ "${BODIES[$j]}" = "${BODIES[$i]}" ] && peers=$((peers + 1)); done
    # «не проверено» и «чисто» обязаны различаться на взгляд: группа из одной ячейки сравнивать не с
    # чем, и молчаливый «ok» под ней читался бы как доказанная чистота.
    mark="ok"
    [ "$peers" -lt 2 ] && mark="?"
    case "$NOISY" in *" $i "*)
        mark="ШУМ"
        noisy=1
        ;;
    esac
    printf '%-9s %-5s %-9s %-8s %-10s %-8s %-9s %s\n' \
        "${ITERS[$i]}" "${BODIES[$i]}" "${MEAN[$i]}" "$(pct_of_budget "${MEAN[$i]}")" \
        "${WORST[$i]}" "$(pct_of_budget "${WORST[$i]}")" "${VEL[$i]}" "$mark"
done

if [ "$same" -eq 1 ]; then
    printf '\nperf-sweep: FAIL — две ячейки с разным числом итераций дали одинаковый vel:\n'
    printf 'пересборка не доехала, и таблица выше снята с одного бинаря. Числам верить нельзя.\n'
    exit 1
fi
if [ "$noisy" -eq 1 ]; then
    printf '\nperf-sweep: ШУМ — в ячейках выше сцена column при равной работе стоила разного.\n'
    printf 'Машину в этот момент грузило посторонним; помеченные строки перезапусти по одной.\n'
    exit 2
fi
printf '\nperf-sweep: PASS — заголовки восстановлены, цель пересобрана из исходников HEAD\n'
