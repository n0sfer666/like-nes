#!/usr/bin/env bash
# Статические инварианты дерева: то, что доказывается грепом, без сборки и без раннера.
#
#   bash scripts/tree_invariants.sh          # все
#   bash scripts/tree_invariants.sh seam     # по одному (так их зовёт CI — чтобы в логе было
#                                            # видно, какой именно инвариант сломан)
#
# Жили телом шагов ci.yml, и цена измерена дважды за один вечер: нарушение приезжало в CI, круг
# «push → 20 минут на трёх ОС → красный» тратился на однострочную находку, видимую локально за
# полсекунды. Отсюда один файл на всё семейство, а не четыре: у них общий приём — ПОЗИТИВНЫЙ
# КОНТРОЛЬ. Греп-гейт без доказательства, что поиск вообще что-то находит, зелен вакуумно:
# промахнулась регулярка или переехал корень — и гейт молчит навсегда. Каждая проверка ниже
# сначала утверждает, что видит то, что обязано быть, и только потом ищет нарушение.
set -uo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1

EXT="--include=*.c --include=*.cc --include=*.cxx --include=*.cpp --include=*.h
     --include=*.hpp --include=*.mm --include=*.m --include=*.inl"
ROOTS="engine tools example_ugly_game platform"

fail() { echo "$@"; exit 1; }

# --- Инвариант 2 спеки #12: условная компиляция по ОС — только в engine/platform/* -------------
inv_seam() {
    # Совпадать обязан ЦЕЛЫЙ идентификатор, а не подстрока: `GLFW_PLATFORM_WIN32` — рантайм-
    # константа GLFW, никакой условной компиляции в ней нет, а подстрочный `_WIN32` её ловил.
    # Ограничители написаны классами символов, а не `\b`: `\b` — GNU-расширение, а гейт обязан
    # вести себя одинаково у всех, кто его запускает (правило `portability` в ci_lint.py про то же).
    local pat='(^|[^A-Za-z0-9_])(_WIN32|_MSC_VER|__APPLE__|__linux__|__unix__)([^A-Za-z0-9_]|$)'
    # Расширения фильтрует сам grep: постфильтр по строке пропускал бы .c/.cc/.cxx/.m (а
    # engine/audio/*_impl.c в дереве есть) и заодно ловил бы содержимое строк, где просто
    # встречается ".cpp:" — фикстуры парсера диагностик.
    # shellcheck disable=SC2086
    local seam; seam=$(grep -rnE "$pat" $EXT engine/platform | wc -l | tr -d '[:space:]')
    # Не гипотеза: под zsh незакавыченный $EXT разъезжается глобом, и гейт печатает PASS,
    # не прочитав ни файла.
    [ "$seam" -ge 3 ] || fail "ifdef gate: search itself is broken (seam hits: $seam)"
    # shellcheck disable=SC2086
    local hits; hits=$(grep -rnE "$pat" $EXT engine tools example_ugly_game \
                       | grep -v '^engine/platform/' || true)
    [ -z "$hits" ] || fail "OS #ifdef leaked outside the platform seam:
$hits"
    echo "platform seam: PASS (no OS conditionals outside engine/platform, seam hits: $seam)"
}

# --- Он же, вторая половина: argv только через platform::Args ----------------------------------
inv_argv() {
    # Исключён только iOS-хост: свой CRT, MSVC там нет, и ANSI-перекодировка argv до него
    # не доходит.
    local exclude='^(platform/ios/app\.mm)$'
    # `int main (int` / `int main( int` / `auto main(int` / `wmain` — форма объявления дрейфует,
    # а гейт на промахе регулярки молча начал бы пропускать.
    local files; files=$(grep -rlE '^[[:space:]]*(int|auto)[[:space:]]+w?main[[:space:]]*\([[:space:]]*int' \
        --include='*.c' --include='*.cc' --include='*.cxx' --include='*.cpp' \
        --include='*.mm' --include='*.m' \
        engine tools example_ugly_game platform | grep -vE "$exclude" || true)
    local count; count=$(printf '%s\n' "$files" | grep -c . || true)
    [ "$count" -ge 15 ] || fail "argv gate matched only $count main() files — the search itself is broken"
    local bad=""
    # read -r, а не `for f in $(...)`: в дереве уже есть каталог с пробелом в имени.
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        grep -q 'platform::Args' "$f" || bad="$bad $f"
    done <<< "$files"
    [ -z "$bad" ] || fail "main() without the UTF-8 argv seam (platform::Args):$bad"
    echo "argv seam: PASS ($count mains, all through platform::Args)"
}

# --- Он же, третья половина: окружение только через platform_env -------------------------------
inv_env() {
    # Переименованный корень греп бы проглотил (нашлось в остальных — код 0), и гейт стал бы
    # зелёным для целого поддерева. Существование корней утверждается отдельно.
    for r in $ROOTS; do
        [ -d "$r" ] || fail "env gate: root '$r' does not exist"
    done
    # Запись ищется вместе с чтением: setenv/putenv мимо шва — тот же ANSI-барьер, только
    # значение портится на входе, а не на выходе. Annex-K-имена в альтернации не для красоты:
    # под /WX обойти шов через std::getenv на Windows нельзя вовсе (C4996 — ошибка), и
    # единственный работающий обход — как раз _dupenv_s/getenv_s/_putenv_s, куда MSVC и предлагает
    # уйти в тексте самой диагностики. Префиксы (_w, secure_) ловятся тем, что шаблон не якорится
    # по началу слова.
    # shellcheck disable=SC2086
    local all; all=$(grep -rnE '(get|set|unset|put|dup)env(_s)?[[:space:]]*\(' $EXT $ROOTS || true)
    # Позитивный контроль: ТОТ ЖЕ шаблон по ТЕМ ЖЕ корням обязан видеть getenv внутри шва — там он
    # и живёт. Исключение — весь шов, включая заголовок: в нём эти имена стоят в описании
    # контракта, и упоминание вида `_dupenv_s(...)` в комментарии роняло бы гейт на ровном месте.
    local seam; seam=$(printf '%s\n' "$all" | grep -c '^engine/platform/platform_env' || true)
    [ "$seam" -ge 1 ] || fail "env gate: search itself is broken (seam hits: $seam)"
    local hits; hits=$(printf '%s\n' "$all" | grep -v '^engine/platform/platform_env' | grep . || true)
    [ -z "$hits" ] || fail "env access outside the seam (platform::env_var/env_put):
$hits"
    echo "env seam: PASS (getenv/setenv live only in engine/platform/platform_env*)"
}

# --- Инвариант 1 спеки #14: подсистемы не зависят от слоя framework ----------------------------
inv_deps() {
    local dirs="engine/render engine/audio engine/input engine/asset engine/plugin engine/material engine/light"
    # Доказательство, что поиск работает: цели линкуются в каждом из этих каталогов.
    # shellcheck disable=SC2086
    grep -rn "target_link_libraries" $dirs >/dev/null \
        || fail "search found no link edges at all — the gate is vacuous"
    # Имена целей слоя берутся ИЗ ДЕРЕВА, а не из литерала: список, вписанный руками, отстаёт от
    # первого же нового модуля, и гейт молча пропускает ребро к нему — так и случилось, когда
    # появился framework_physics. Каталог = модуль = имя цели, это правило слоя (#14).
    local mods=""
    for d in engine/framework/*/; do
        [ -d "$d" ] || continue
        mods="$mods|$(basename "$d")"
    done
    mods="${mods#|}"
    [ -n "$mods" ] && echo "$mods" | grep -q "core" \
        || fail "framework layer has no modules — the gate is vacuous"
    # Ловим ребро к слою: имя цели (framework_core / framework_input / ...) и включение его
    # заголовков. Слово "framework" целиком не годится — им зовутся системные фреймворки macOS
    # ("-framework CoreAudio") и GameController.framework в комментариях.
    # shellcheck disable=SC2086
    if grep -rnE "framework_($mods)|include .*framework/" $dirs; then
        fail "a subsystem depends on the framework layer — direction broken"
    fi
    echo "framework dependency direction: PASS"
}

# --- Рантайм-вывод — чистый ASCII (консоль Windows не UTF-8) -----------------------------------
inv_ascii() {
    # Единственный инвариант семейства, который грепом не доказывается: отличить литерал от
    # комментария построчной регуляркой нельзя, а комментарии в дереве русские по стилю. Разбор
    # живёт в отдельном скрипте, и позитивный контроль там свой — число прочитанных литералов.
    python3 scripts/ascii_output_check.py --selftest >/dev/null \
        || fail "ascii-check: самопроверка правил не прошла (запусти её отдельно)"
    python3 scripts/ascii_output_check.py || exit 1
}

case "${1:-all}" in
    seam) inv_seam ;;
    argv) inv_argv ;;
    env)  inv_env ;;
    deps) inv_deps ;;
    ascii) inv_ascii ;;
    all)
        # Ни одна проверка не обрывает остальные: прогон обязан выдать все находки разом — то же
        # основание, что у этапов preflight.sh.
        rc=0
        for i in seam argv env deps ascii; do ( "inv_$i" ) || rc=1; done
        exit $rc
        ;;
    *) fail "usage: tree_invariants.sh [seam|argv|env|deps|ascii|all]" ;;
esac
