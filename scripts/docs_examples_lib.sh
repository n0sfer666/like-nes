# shellcheck shell=bash
# Утверждения о ПРИМЕРАХ документации (гейт 2 спеки #19): каждый файл `docs/examples/*.cpp`
# собирается целью `doc_example_<имя>`, запускается и печатает ровно то, что лежит рядом в `.out`.
# Врезки в тексте берутся из этих же файлов (`check_docs_snippets.py`), поэтому «показанный код
# собирается и работает» здесь ДОКАЗАНО, а не заявлено: разойдись поведение с текстом — расходится
# голден, а не мнение читателя.
#
# Утверждения живут отдельно от гейта, потому что позитивный контроль строит им фикстурные деревья
# и подменяет реализации: набор, гоняющий гейт целиком, требовал бы настоящей сборки на каждую
# порчу.

ex_ok()  { printf 'docs-examples: OK   %s\n' "$1"; }
ex_bad() { printf 'docs-examples: FAIL %s\n' "$1" >&2; }

# Каталог примеров назван ОДНИМ местом: его читают гейт, обе половины утверждений и фабрика фикстур,
# а написанный врозь он разъехался бы молча — ровно list-drift.
docs_examples_dir() { printf 'docs/examples\n'; }

# Списки ВЫВОДЯТСЯ обходом, а не пишутся руками: пример, добавленный в каталог, обязан попасть под
# гейт сам. `basename … .cpp` вместо ветвления по расширению — имя цели строится из него же.
docs_examples_names() {
    local root="$1" f
    for f in "$root/$(docs_examples_dir)"/*.cpp; do
        [ -f "$f" ] || continue
        basename "$f" .cpp
    done | LC_ALL=C sort
}

docs_examples_goldens() {
    local root="$1" f
    for f in "$root/$(docs_examples_dir)"/*.out; do
        [ -f "$f" ] || continue
        basename "$f" .out
    done | LC_ALL=C sort
}

# Ноль примеров — ОТКАЗ, а не «нарушений нет»: гейт описывает каталог, который сам же и обходит, и
# промах пути читался бы как чистый прогон (тот же класс, что vacuous-gate в ci_lint.py).
assert_examples_present() {
    local root="$1" n
    n=$(docs_examples_names "$root" | grep -c . || true)
    if [ "$n" = 0 ]; then
        ex_bad "в $(docs_examples_dir)/ нет ни одного примера — гейту нечего собирать"
        return 1
    fi
    ex_ok "примеров в дереве: $n"
}

# Пара «пример ↔ ожидаемый вывод» проверяется РАВЕНСТВОМ МНОЖЕСТВ, а не «у каждого примера есть
# .out»: голден, переживший свой пример, никем больше не читается и молча описывает вчерашний код.
assert_examples_paired() {
    local root="$1" src out
    src=$(docs_examples_names "$root")
    out=$(docs_examples_goldens "$root")
    if [ "$src" != "$out" ]; then
        ex_bad "примеры и ожидаемые выводы разошлись"
        diff <(printf '%s\n' "$src") <(printf '%s\n' "$out") | sed 's/^/       /' >&2 || true
        return 1
    fi
    ex_ok "у каждого примера есть свой .out и наоборот"
}

# Бинарь ищется find'ом, а не собирается по пути: ninja кладёт исполняемый в корень каталога сборки,
# а генераторы Visual Studio — в подкаталог конфигурации, и собранный руками путь молчал бы на одной
# из трёх ОС. Тот же приём, что в check_goldens.sh.
docs_example_binary() {
    local build="$1" name="$2"
    find "$build" -maxdepth 3 -type f \
        \( -name "doc_example_$name" -o -name "doc_example_$name.exe" \) | head -1
}

# Возврат каретки снимается ЗДЕСЬ, а не в каждом сравнении: на Windows stdout идёт текстовым
# режимом, и `\n` уезжает в CRLF. Снимается он с ОБЕИХ сторон сравнения, а не только с вывода:
# windows-раннер держит `core.autocrlf=true`, то есть в checkout с CRLF выезжает и сам голден, и
# `diff` краснел бы на КАЖДОЙ строке ровно на той ОС, ради которой мера и заведена. Пин
# `*.out text eol=lf` в .gitattributes — второй замок, а не замена этому: он держит дерево, а
# утверждение обязано держаться само (найдено ревью раунда #19).
docs_example_strip_cr() { tr -d '\r'; }

# Отсутствие бинаря — ОТКАЗ, а не пропуск: каталог сборки гейт не создаёт (его делает вызывающий),
# поэтому промах каталога обязан быть слышен. То же решение, что у core-голденов.
assert_example_runs() {
    local root="$1" build="$2" name="$3" bin rc=0 tmp
    bin=$(docs_example_binary "$build" "$name")
    if [ -z "$bin" ]; then
        ex_bad "$name: цель doc_example_$name не найдена в $build — пример не собран"
        return 1
    fi
    tmp=$(mktemp) || return 1
    "$bin" >"$tmp.raw" 2>&1 || rc=$?
    docs_example_strip_cr <"$tmp.raw" >"$tmp"
    if [ "$rc" != 0 ]; then
        ex_bad "$name: пример упал кодом $rc"
        sed 's/^/       /' <"$tmp" >&2
        rm -f "$tmp" "$tmp.raw"
        return 1
    fi
    docs_example_strip_cr <"$root/$(docs_examples_dir)/$name.out" >"$tmp.gold"
    # Сравнение идёт ФАЙЛАМИ, а не подстановкой: $( … ) съедает хвостовые переводы строк с обеих
    # сторон, то есть пример, потерявший последнюю строку вывода, сходился бы с голденом.
    if ! diff -u "$tmp.gold" "$tmp" >"$tmp.diff" 2>&1; then
        ex_bad "$name: вывод разошёлся с $(docs_examples_dir)/$name.out"
        sed 's/^/       /' <"$tmp.diff" >&2
        rm -f "$tmp" "$tmp.raw" "$tmp.gold" "$tmp.diff"
        return 1
    fi
    rm -f "$tmp" "$tmp.raw" "$tmp.gold" "$tmp.diff"
    ex_ok "$name: собран, запущен, вывод совпал с голденом"
}

# Все примеры разом, и первый упавший не отменяет остальных: один прогон обязан выдать все находки.
assert_examples_run_all() {
    local root="$1" build="$2" rc=0 name
    while read -r name; do
        [ -n "$name" ] || continue
        assert_example_runs "$root" "$build" "$name" || rc=1
    done < <(docs_examples_names "$root")
    return $rc
}
