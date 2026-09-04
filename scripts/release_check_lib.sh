# shellcheck shell=bash
# Утверждения о релизном пакете (спека #20, гейты 1, 2, 9, 10) — отдельно от того, что их
# запускает. Каждое берёт КАТАЛОГ и возвращает код: так один и тот же код проверяет и настоящий
# пакет (check_release.sh), и сломанные фикстуры (check_release_selftest.sh). Утверждение,
# существующее только внутри своего прогона, невозможно уронить нарочно, а значит — неизвестно,
# умеет ли оно вообще падать.

ok() { printf 'release-check: OK   %s\n' "$1"; }
bad() { printf 'release-check: FAIL %s\n' "$1" >&2; }

# Ожидаемый состав считается из ТЕХ ЖЕ источников, что и сам пакет: список лицензий — из
# cmake/licenses.manifest (его читает cmake/licenses.cmake), имя рантайма — из ОС. Рукописная копия
# разъехалась бы с install_engine.cmake молча, и гейт продолжал бы утверждать вчерашний состав.
# Целевая ОС — АРГУМЕНТ, а не всегда `uname`: контейнерный пакет собирается под Linux с macOS, и
# состав, посчитанный по хосту, ждал бы там `libwgpu_native.dylib`. Умолчание оставлено хостом,
# потому что у хостового гейта другого источника платформы и нет.
expected_files() {
  local root="$1" os="${2:-$(uname -s)}" lic
  # Незнакомая ОС — отказ, как и в release.sh: ветка `*)` молча раздавала Linux-именование, и на
  # FreeBSD самопроверка проходила бы вакуумно, утверждая состав, которого там не бывает.
  case "$os" in
    Darwin) printf 'like-nes/bin/assetc\nlike-nes/bin/editor_shell\nlike-nes/bin/libwgpu_native.dylib\n' ;;
    MINGW*|MSYS*|CYGWIN*) printf 'like-nes/bin/assetc.exe\nlike-nes/bin/editor_shell.exe\nlike-nes/bin/wgpu_native.dll\n' ;;
    Linux) printf 'like-nes/bin/assetc\nlike-nes/bin/editor_shell\nlike-nes/bin/libwgpu_native.so\n' ;;
    *) bad "незнакомая ОС $os — ожидаемый состав пакета неизвестен"; return 1 ;;
  esac
  while read -r lic; do
    [ -n "$lic" ] && printf 'like-nes/licenses/%s\n' "$(basename "$lic")"
  done < "$root/cmake/licenses.manifest"
  printf 'like-nes/version.txt\n'
}

# Равенство МНОЖЕСТВ, а не «всё ожидаемое на месте»: файл, добавленный в install_engine.cmake и не
# названный здесь, обязан валить гейт. Проверка «ожидаемое ⊆ фактическому» пропускала бы и лишний
# файл, и целый чужой каталог, заехавший в пакет вместе с ним.
assert_composition() {
  local root="$1" pkg_root="$2" os="${3:-}" got exp
  got=$( cd "$pkg_root" && find . -type f | sed 's|^\./||' | LC_ALL=C sort )
  exp=$( expected_files "$root" ${os:+"$os"} | LC_ALL=C sort )
  if [ "$got" != "$exp" ]; then
    bad "состав пакета разошёлся с ожидаемым"
    diff <(printf '%s\n' "$exp") <(printf '%s\n' "$got") | sed 's/^/       /' >&2 || true
    return 1
  fi
  ok "состав пакета поимённо ($(printf '%s\n' "$exp" | wc -l | tr -d ' ') файлов)"
}

# Отдельно от состава, потому что имя файла и его содержимое — разные утверждения: лицензия,
# ужавшаяся до нуля байт, проходит проверку по имени и нарушает регресс спеки #9 ровно так же.
assert_licenses() {
  local root="$1" pkg_root="$2" n=0 lic
  while read -r lic; do
    [ -n "$lic" ] || continue
    local f
    f="$pkg_root/like-nes/licenses/$(basename "$lic")"
    if [ ! -s "$f" ]; then bad "лицензия пуста или отсутствует: $(basename "$lic")"; return 1; fi
    n=$((n + 1))
  done < "$root/cmake/licenses.manifest"
  if [ "$n" -lt 7 ]; then bad "лицензий в манифесте $n, а спека #9 требует семь"; return 1; fi
  ok "лицензии на месте и непусты ($n)"
}

# Штамп сверяется ЦЕЛИКОМ и по порядку, а не грепами по строкам: греп равнодушен к лишним строкам
# (version.txt из пятидесяти строк мусора с тремя правильными среди них проходил), и он же трактует
# точки версии как «любой символ» — `v1.2.3` совпадал с `v1X2Y3`. Сравнение строк не делает ни того
# ни другого и заодно отбивает CRLF-штамп: `\r` — лишний байт последней строки, а не невидимка.
#
# Тройка приходит ИЗ ИМЕНИ ПАКЕТА. До этого её считали двое (CMake и release.sh) и по-разному, так
# что архив с именем `…-macos-arm64` мог нести внутри `target Darwin-arm64`, а на Linux — вообще
# чужую платформу, и ни одно утверждение этого не видело: старое принимало ЛЮБУЮ строку.
assert_stamp() {
  local pkg_root="$1" want="$2" triple="$3"
  local f n l1 l2 l3
  f="$pkg_root/like-nes/version.txt"
  [ -f "$f" ] || { bad "нет version.txt"; return 1; }
  n=$(wc -l < "$f" | tr -d ' ')
  if [ "$n" != 3 ]; then bad "в штампе $n строк, а их ровно три"; return 1; fi
  { IFS= read -r l1; IFS= read -r l2; IFS= read -r l3; } < "$f"
  if [ "$l1" != "like-nes engine $want" ]; then
    bad "штамп не называет версию $want: '$l1'"; return 1
  fi
  if ! printf '%s' "$l2" | grep -Eq '^commit [0-9a-f]{7,40}$'; then
    bad "штамп не называет коммит: '$l2'"; return 1
  fi
  if [ "$l3" != "target $triple" ]; then
    bad "тройка штампа разошлась с именем пакета ($triple): '$l3'"; return 1
  fi
  ok "штамп называет версию, коммит и тройку ($triple)"
}

# Имя рантайма в пакете сверяется с тем, что назвал САМ CMake (WGPU_RUNTIME_LIB в кеше), а не с
# нашим списком: expected_files ветвится по ОС руками, и самопроверка строит фикстуру из него же —
# ошибись имя `wgpu_native.dll`, и обе половины ошиблись бы одинаково, оставшись зелёными. Источник
# здесь независимый, поэтому утверждение работает на той ОС, где гейт гоняется по-настоящему.
assert_runtime_named() {
  local cache="$1" pkg_root="$2" lib base
  [ -f "$cache" ] || { bad "нет $cache — имя рантайма сверить не с чем"; return 1; }
  lib=$(grep -m1 '^WGPU_RUNTIME_LIB:' "$cache" | sed 's/^[^=]*=//')
  [ -n "$lib" ] || { bad "в кеше CMake нет WGPU_RUNTIME_LIB"; return 1; }
  base=$(basename "$lib")
  if [ ! -f "$pkg_root/like-nes/bin/$base" ]; then
    bad "рантайм CMake зовётся $base, а в пакете его нет"
    return 1
  fi
  ok "имя рантайма совпало с тем, что назвал CMake ($base)"
}

assert_same_manifest() {
  local a="$1" b="$2"
  if ! diff -q "$a" "$b" >/dev/null; then
    bad "два прогона дали разное содержимое"
    diff "$a" "$b" | sed 's/^/       /' >&2 || true
    return 1
  fi
  ok "два прогона дали то же содержимое"
}

assert_same_sum() {
  local a="$1" b="$2"
  if [ "$a" != "$b" ]; then bad "сумма архива не воспроизвелась: $a против $b"; return 1; fi
  ok "сумма архива воспроизвелась ($a)"
}

# Архив сверяется с манифестом СВОИХ файлов: без этого гейт утверждал бы про стейдж, а уезжает к
# людям архив, и подмена между этими двумя шагами прошла бы незамеченной.
assert_pkg_matches() {
  local pkg="$1" manifest="$2" tmp got
  [ -s "$pkg" ] || { bad "пакет пуст или отсутствует: $pkg"; return 1; }
  tmp=$(mktemp -d)
  tar -xzf "$pkg" -C "$tmp" || { bad "архив не распаковывается"; rm -rf "$tmp"; return 1; }
  got=$(manifest_of "$tmp")
  if [ "$got" != "$(cat "$manifest")" ]; then
    bad "содержимое архива разошлось с манифестом"
    diff <(cat "$manifest") <(printf '%s\n' "$got") | sed 's/^/       /' >&2 || true
    rm -rf "$tmp"; return 1
  fi
  rm -rf "$tmp"
  ok "архив совпал со своим манифестом"
}

# Файл сумм — отдельное утверждение, потому что он единственный продукт релиза, который читает
# ЧЕЛОВЕК, скачавший пакет: сумма, посчитанная правильно и не попавшая в SHA256SUMS, проверяема
# ровно никак, а гейт, сверяющий два прогона между собой, этого не замечает.
assert_sums_listed() {
  local sums="$1" pkg="$2" want="$3"
  [ -s "$sums" ] || { bad "SHA256SUMS пуст или отсутствует"; return 1; }
  # grep -F: имя пакета несёт точки версии и расширения, и в шаблоне они совпадали с любым
  # символом — строка `…-v1X2X3-macos-arm64Ytar.gz` проходила как своя.
  if ! grep -Fqx -- "$want  $(basename "$pkg")" "$sums"; then
    bad "SHA256SUMS не называет $(basename "$pkg") с суммой $want"
    sed 's/^/       /' "$sums" >&2
    return 1
  fi
  ok "SHA256SUMS называет пакет и его сумму"
}
