#!/usr/bin/env bash
# Позитивный контроль того, ЧТО УЕЗЖАЕТ В СУММУ ОБРАЗА (спека #20, вертикаль 4, шаг B): время, права
# каталогов и владелец. Предмет свой, а не хвост соседей: check_release_appimage_selftest.sh ломает
# содержимое готового AppDir, …_impl_selftest.sh — раскладку, …_pack_selftest.sh — разбор исхода
# appimagetool, и ни один из них не отвечает за то, почему две сборки одного дерева дают равные
# байты. Байт-равенство гейт утверждает ПЕРВЫМ, а два его прогона слепы к трём величинам здесь по
# построению: они идут в одном шелле, от одного пользователя, с одной umask.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_appimage_lib.sh
. "$ROOT/scripts/release_appimage_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"
# shellcheck source=scripts/release_appimage_check_lib.sh
. "$ROOT/scripts/release_appimage_check_lib.sh"
# shellcheck source=scripts/release_appimage_squash_lib.sh
. "$ROOT/scripts/release_appimage_squash_lib.sh"
# shellcheck source=scripts/release_appimage_fixture_lib.sh
. "$ROOT/scripts/release_appimage_fixture_lib.sh"
# shellcheck source=scripts/selftest_sub_lib.sh
. "$ROOT/scripts/selftest_sub_lib.sh"

case "$(uname -s)" in
  Linux|Darwin) : ;;
  *) echo "appimage-norm-selftest: ПРОПУСК — git-bash не моделирует бит исполнения (здесь $(uname -s))"; exit 0 ;;
esac

VER=v0.0.0-check
SHA=abc1234
TRIPLE=linux-x86_64
BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

ORIG_SEAL=$(declare -f appimage_seal_appdir)
ORIG_MAKE=$(declare -f appimage_make_appdir)
# Копия настоящего раскладчика под своим именем: подмена ниже обязана позвать НАСТОЯЩУЮ раскладку и
# лишь потом испортить один каталог — фикстура, разложенная своей копией правил, проверяла бы
# утверждение на томе иного устройства, чем тот, на котором оно проходит.
eval "appimage_make_appdir_real() $(declare -f appimage_make_appdir | tail -n +2)"

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'appimage-norm-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'appimage-norm-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Выравнивание времени — несущая мера, а не гигиена: сумма squashfs зависит от mtime. Про её ИСХОД
# утверждает гейт (два прогона), а здесь доказывается, что мера работает вообще.
case_seal() {
  local arm="$1"
  local base="$FIX/seal"
  local n
  (
    if [ "$arm" = broken ]; then
      appimage_seal_appdir() { :; }
      subbed appimage_seal_appdir "$ORIG_SEAL" || exit 1
    fi
    rm -rf "$base"
    appimage_fixture_appdir "$ROOT" "$base" "$VER" "$SHA" "$TRIPLE" || exit 1
    n=$(TZ=UTC0 find "$base/AppDir" -type f -newermt '2026-01-01 00:01' | wc -l | tr -d ' ')
    [ "$n" = 0 ]
  )
}
expect pass "время выравнено штампом" case_seal ok
expect fail "запечатывание без выравнивания времени" case_seal broken

# Права КАТАЛОГОВ едут в squashfs наравне с файлами, то есть в СУММУ образа: `mkdir -p` берёт их у
# umask машины, и образ, собранный при umask 077, отличался бы байтами от собранного при 022 при том
# же содержимом. Два прогона гейта этого не видят по построению — они идут в одном шелле с одной
# umask, — поэтому выравнивание доказывается здесь, подменой, снимающей его с одного каталога.
case_dirmode() {
  local base="$FIX/dirmode" what="$1"
  (
    appimage_make_appdir() {
      appimage_make_appdir_real "$@" || return 1
      chmod 0700 "$2/usr/bin"
    }
    subbed appimage_make_appdir "$ORIG_MAKE" || exit 1
    [ -d "$base/AppDir" ] || appimage_fixture_appdir "$ROOT" "$base" "$VER" "$SHA" "$TRIPLE" || exit 1
    case "$what" in
      composition) assert_appimage_composition "$ROOT" "$base/AppDir" ;;
      modes) assert_appimage_modes "$base/AppDir" ;;
    esac
  )
}
expect pass "каталог с чужими правами · состав слеп" case_dirmode composition
expect fail "каталог с чужими правами · права" case_dirmode modes

# Нормализация образа: владелец и права каталогов уезжают в squashfs, прочитать их можно только из
# самого образа, а живой образ есть только там, где есть appimagetool. Суждение о листинге вынесено
# ЧИСТОЙ функцией (тот же приём, что ci_picked_verdict вертикали 3) — иначе ветка «владелец не root»
# не проверялась бы ничем, кроме контейнерного прогона.
CLEAN='drwxr-xr-x root/root                87 2026-09-04 00:00 squashfs-root
-rwxr-xr-x root/root              1234 2026-09-04 00:00 squashfs-root/AppRun
-rw-r--r-- root/root               512 2026-09-04 00:00 squashfs-root/like-nes.desktop'
listing_says() {
  local want="$1" listing="$2" got
  got=$(printf '%s\n' "$listing" | appimage_listing_violations)
  if [ "$want" = clean ]; then [ -z "$got" ]; else printf '%s\n' "$got" | grep -q "$want"; fi
}
expect pass "нормализованный листинг · находок нет" listing_says clean "$CLEAN"
# Числовой владелец — тот же ноль: unsquashfs печатает `0/0`, когда в образе нет имён, и отбивать
# его значило бы валить гейт на честном образе.
expect pass "числовой владелец принимается" listing_says clean "$(printf '%s\n' "$CLEAN" | sed 's|root/root|0/0|')"
expect pass "чужой владелец назван" listing_says "владелец builder/builder" \
  "$(printf '%s\n' "$CLEAN" | sed 's|root/root|builder/builder|')"
expect pass "права каталога названы" listing_says "каталог drwx------" "${CLEAN/drwxr-xr-x/drwx------}"
# Пустой листинг — находка, а не «нарушений нет»: unsquashfs, промахнувшийся мимо образа, печатает
# ноль строк, и правило без этой ветки объявляло бы такой прогон нормализованным.
expect pass "пустой листинг — находка" listing_says "нет ни одной записи" ""

# Сдвиг squashfs внутри образа — находка ЖИВОГО прогона в контейнере, а не гипотеза: `unsquashfs`
# без `-o` читает начало файла, видит там ELF обёртки и отказывается, то есть утверждение о
# нормализации валило исправный образ. Разбор ответа `--appimage-offset` проверяется здесь, потому
# что живой образ есть только на Linux с appimagetool, а ошибиться в нём можно на любой машине.
expect pass "сдвиг числом принимается" appimage_offset_valid 936456
expect fail "пустой ответ — не сдвиг" appimage_offset_valid ""
expect fail "мусор вместо сдвига" appimage_offset_valid "offset: 936456"
# Ноль отбивается наравне с мусором: с нулевого сдвига `unsquashfs` читает ту же ELF-обёртку, то
# есть «сдвиг 0» неотличим от «сдвига не назвали».
expect fail "нулевой сдвиг" appimage_offset_valid 0

if [ "$BAD" != 0 ]; then echo "appimage-norm-selftest: FAIL" >&2; exit 1; fi
echo "appimage-norm-selftest: PASS"
