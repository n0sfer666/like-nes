# shellcheck shell=bash
# Утверждения о ПРОВОДКЕ статического CRT (спека #20, вертикаль 5) — отдельно от утверждений о
# готовом пакете (release_crt_lib.sh). Предмет здесь не таблица импортов, а само дерево: чем
# задаётся рантайм и не разъехались ли две копии закрытого списка redist-имён. Читаются оба без
# сборки и без Windows, поэтому и живут врозь: пакета у них нет вовсе.
#
# Печать берётся у соседа (crt_ok/crt_bad), а ожидаемый состав пакета — у release_check_lib.sh
# (expected_files); подключать обоих обязан вызывающий: свои копии разъехались бы и в форматах
# вывода, по которым владелец сверяет прогон построчно, и в самом списке ожидаемых файлов.

# Порядок в CMakeLists.txt — тот же класс, что у cmake/warnings.cmake: выбор рантайма есть свойство
# ЦЕЛИ, а не каталога, поэтому цель, объявленная раньше нашего include, возьмёт динамический CRT и
# приведёт в один образ вторую кучу. Утверждение читается без сборки и без Windows, а потеря самой
# строки иначе видна только на раннере — то есть через полчаса и в чужом логе.
assert_msvc_runtime_wired() {
  local root="$1" f="$1/CMakeLists.txt" inc dep
  [ -f "$f" ] || { crt_bad "нет $f — порядок включения проверять не в чем"; return 1; }
  inc=$(grep -n '^[^#]*include(.*cmake/msvc_runtime\.cmake' "$f" | head -1 | cut -d: -f1)
  [ -n "$inc" ] || {
    crt_bad "CMakeLists.txt не включает cmake/msvc_runtime.cmake — CRT остался бы динамическим"
    return 1
  }
  # Первая зависимость ищется отдельно и её отсутствие есть ОТКАЗ: файл без FetchContent_Declare
  # сравнивать не с чем, и «нарушений нет» на нём означало бы, что гейт разучился читать дерево.
  dep=$(grep -n '^[^#]*FetchContent_Declare' "$f" | head -1 | cut -d: -f1)
  [ -n "$dep" ] || {
    crt_bad "в CMakeLists.txt нет ни одного FetchContent_Declare — порядок сверять не с чем"
    return 1
  }
  if [ "$inc" -ge "$dep" ]; then
    crt_bad "msvc_runtime.cmake включён строкой $inc, ПОСЛЕ первой зависимости (строка $dep)"
    return 1
  fi
  crt_ok "статический CRT задаётся до зависимостей (строка $inc против $dep)"
}

# Закрытый список redist-имён живёт в ДВУХ копиях: glob-case здесь и regex в cmake/msvc_redist.cmake,
# который по нему решает, какие файлы класть в пакет. Механической связи между копиями нет, а
# расхождение молчит: cmake не положит файл, гейт не найдёт его — но увидит это только владелец на
# приехавшем из CI пакете. Поэтому копии сверяются МЕЖДУ СОБОЙ, ровно как форма mirrors-group в
# ci_lint.py, где внешнего эталона у списка тоже нет.
assert_redist_list_mirrored() {
  local root="$1" here there
  # Читается ровно тело crt_is_redist, а не файл целиком: имена-образцы встречаются и в комментариях,
  # и в самой этой функции, и разбор по всему файлу сверял бы список сам с собой.
  here=$(sed -n '/^crt_is_redist() {/,/^}/p' "$root/scripts/release_crt_lib.sh" \
    | grep -o '[a-z]*\[0-9\]\*\.dll' | sed 's/\[0-9\].*//' | LC_ALL=C sort -u)
  there=$(sed -n 's/.*MATCHES "^(\([a-z|]*\))\[0-9\].*/\1/p' "$root/cmake/msvc_redist.cmake" \
    | tr '|' '\n' | LC_ALL=C sort -u)
  # Пустое с обеих сторон равно само себе: разбор, промахнувшийся мимо обоих файлов, иначе
  # печатал бы «копии совпадают» (тот же класс, что vacuous-gate).
  if [ -z "$here" ] || [ -z "$there" ]; then
    crt_bad "список redist-имён не вычитан: shell=$(printf '%s' "$here" | wc -w | tr -d ' '), cmake=$(printf '%s' "$there" | wc -w | tr -d ' ')"
    return 1
  fi
  if [ "$here" != "$there" ]; then
    crt_bad "копии закрытого списка redist-имён разошлись"
    diff <(printf '%s\n' "$here") <(printf '%s\n' "$there") | sed 's/^/       /' >&2 || true
    return 1
  fi
  crt_ok "копии закрытого списка redist-имён совпадают ($(printf '%s\n' "$here" | wc -l | tr -d ' ') префиксов)"
}

# Фильтр «оставить из потока имён только redist-DLL, приведя к нижнему регистру». Читателей у него
# двое — список, названный в expected_files руками, и таблица импортов настоящего рантайма, — и
# сравнивать их можно только после одинакового приведения: на диске лежит `vcruntime140.dll`, в
# таблице импортов стоит `VCRUNTIME140.dll`, и на macOS такая пара совпала бы сама собой, а на
# Linux нет.
crt_redist_lower() {
  local n
  while read -r n; do
    [ -n "$n" ] || continue
    n=$(basename "$n")
    crt_is_redist "$n" || continue
    printf '%s\n' "$(printf '%s' "$n" | tr 'A-Z' 'a-z')"
  done | LC_ALL=C sort -u
}

# Вторая пара копий того же закрытого списка, и связи между ними нет тем более: app-local файлы в
# пакет кладёт cmake ПО ТАБЛИЦЕ ИМПОРТОВ рантайма, а ожидаемый состав пакета называет их в
# expected_files РУКАМИ. Комментарий над той строкой обещает, что расхождение станет находкой, —
# и до этого утверждения обещание не проверял никто: пересборка дистрибуции чужим тулчейном
# сменила бы VCRUNTIME140 на следующую, cmake положил бы в пакет её, а состав ждал бы прежнюю,
# и разошлись бы они на машине владельца, а не здесь. Форма та же mirrors-group из ci_lint.py:
# внешнего эталона у списка нет, копии сверяются между собой.
assert_redist_expected_named() {
  local root="$1" dll named imported
  if ! dll=$(crt_real_dll "$root"); then
    crt_ok "ПРОПУСК: настоящего wgpu_native.dll нет в дереве — app-local состав сверять не с чем"
    return 0
  fi
  # Импорты забираются в переменную ДО фильтра: код возврата пайпа есть код ПОСЛЕДНЕЙ команды, то
  # есть отказ читателя внутри `python3 … | crt_redist_lower` не виден вовсе, и нечитаемый рантайм
  # проезжал бы дальше как рантайм без единого redist-импорта. Тот же класс уже чинили в
  # assert_crt_self_contained.
  local raw
  if ! raw=$(python3 "$root/scripts/pe_imports.py" "$dll"); then
    crt_bad "таблица импортов настоящего wgpu_native.dll не прочитана — состав app-local сверять нечем"
    return 1
  fi
  imported=$(printf '%s\n' "$raw" | crt_redist_lower)
  # Ноль redist-импортов у настоящего рантайма — отказ, а не «списки совпали»: пустое равно пустому,
  # и читатель, промахнувшийся мимо таблицы, выдал бы за согласие ровно ту тишину, ради которой
  # вертикаль и заведена (тот же класс, что vacuous-gate).
  if [ -z "$imported" ]; then
    crt_bad "настоящий wgpu_native.dll не просит ни одной redist-DLL — предмет app-local меры исчез"
    return 1
  fi
  named=$(expected_files "$root" MINGW | crt_redist_lower)
  if [ "$named" != "$imported" ]; then
    crt_bad "состав пакета называет не те app-local DLL, что просит рантайм"
    diff <(printf '%s\n' "$named") <(printf '%s\n' "$imported") | sed 's/^/       /' >&2 || true
    return 1
  fi
  crt_ok "app-local DLL в ожидаемом составе совпадают с импортами рантайма ($(printf '%s\n' "$imported" | wc -l | tr -d ' '))"
}
