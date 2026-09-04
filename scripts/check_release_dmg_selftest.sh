#!/usr/bin/env bash
# Позитивный контроль СЛОМАННЫМИ ФИКСТУРАМИ (спека #20, вертикаль 4, шаг A): том с вырезанной или
# испорченной частью обязан валить то утверждение, которое эту часть стережёт. Утверждение, у
# которого нет фикстуры, где оно падает, неотличимо от отсутствующего.
#
# Подмены самих РЕАЛИЗАЦИЙ (раскладка, монтирование) живут в check_release_dmg_impl_selftest.sh —
# граница по предмету, та же, что делит пару наборов вертикали 3; он зовётся отсюда внешней
# командой, чтобы в логе стояло имя упавшего набора.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_dmg_lib.sh
. "$ROOT/scripts/release_dmg_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"
# shellcheck source=scripts/release_dmg_check_lib.sh
. "$ROOT/scripts/release_dmg_check_lib.sh"
# shellcheck source=scripts/release_dmg_rpath_lib.sh
. "$ROOT/scripts/release_dmg_rpath_lib.sh"
# shellcheck source=scripts/release_dmg_fixture_lib.sh
. "$ROOT/scripts/release_dmg_fixture_lib.sh"

# Пропуск ВСЛУХ: раскладка обходится без hdiutil, но `chmod`/`readlink`/PlistBuddy у утверждений
# осмысленны только на macOS, а молчаливо пропущенный набор выглядит как пройденный.
if [ "$(uname -s)" != Darwin ]; then
  echo "dmg-selftest: ПРОПУСК — предмет набора есть бандл macOS (здесь $(uname -s))"
  exit 0
fi

VER=v0.0.0-check
SHA=abc1234
TRIPLE=macos-arm64
LICS="$DMG_APP_NAME/Contents/Resources/licenses"
BINS="$DMG_APP_NAME/Contents/MacOS"
STAMPF="$DMG_APP_NAME/Contents/Resources/version.txt"
BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'dmg-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'dmg-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Каждая фикстура — свой каталог со своим стейджем: порча одного тома иначе доезжала бы до
# следующего утверждения, и «отбито» выходило бы из чужой поломки.
vol() {
  dmg_fixture_vol "$ROOT" "$FIX/$1" "$VER" "$SHA" "$TRIPLE" || return 1
  dmg_fixture_manifest "$FIX/$1"
  printf '%s\n' "$FIX/$1"
}

LIC1=$(basename "$(head -1 "$ROOT/cmake/licenses.manifest")")
RUNTIME=libwgpu_native.dylib
CACHE="$FIX/CMakeCache.txt"
printf 'WGPU_RUNTIME_LIB:FILEPATH=/nowhere/%s\n' "$RUNTIME" > "$CACHE"

# --- опора: нетронутый том ----------------------------------------------------------------------
G=$(vol good)
if [ ! -d "$G/vol/$DMG_APP_NAME" ]; then
  echo "dmg-selftest: БРАК фикстурный том не собрался — порчи ниже отбились бы за отсутствием предмета" >&2
  exit 1
fi
expect pass "нетронутый том · состав" assert_dmg_composition "$ROOT" "$G/vol"
expect pass "нетронутый том · зеркало стейджа" assert_dmg_mirrors_stage "$G/vol" "$G/stage"
expect pass "нетронутый том · симлинк" assert_dmg_applications_link "$G/vol"
expect pass "нетронутый том · права" assert_dmg_modes "$G/vol"
expect pass "нетронутый том · plist" assert_dmg_plist "$G/vol" "$VER" "$SHA"
expect pass "нетронутый том · манифест" assert_dmg_matches "$G/vol" "$G/vol.manifest"
expect pass "нетронутый том · лицензии" assert_licenses "$ROOT" "$G/vol" "$LICS"
expect pass "нетронутый том · рантайм" assert_runtime_named "$CACHE" "$G/vol" "$BINS"
expect pass "нетронутый том · штамп" assert_stamp "$G/vol" "$VER" "$TRIPLE" "$STAMPF"

# --- состав ---------------------------------------------------------------------------------------
V=$(vol nolic); rm -f "$V/vol/$LICS/$LIC1"
expect fail "в бандле нет лицензии · состав" assert_dmg_composition "$ROOT" "$V/vol"
expect fail "в бандле нет лицензии · лицензии" assert_licenses "$ROOT" "$V/vol" "$LICS"

V=$(vol zerolic); : > "$V/vol/$LICS/$LIC1"
expect pass "лицензия в нуль байт · состав слеп к содержимому" assert_dmg_composition "$ROOT" "$V/vol"
expect fail "лицензия в нуль байт · лицензии" assert_licenses "$ROOT" "$V/vol" "$LICS"

V=$(vol extra); printf 'x\n' > "$V/vol/$DMG_APP_NAME/Contents/Resources/лишний.txt"
expect fail "в бандле лишний файл" assert_dmg_composition "$ROOT" "$V/vol"

V=$(vol noruntime); rm -f "$V/vol/$BINS/$RUNTIME"
expect fail "в бандле нет рантайма" assert_runtime_named "$CACHE" "$V/vol" "$BINS"

# --- зеркало стейджа --------------------------------------------------------------------------------
# Подменённый байт не меняет ни имён, ни числа файлов: состав его не видит по построению.
V=$(vol tampered); printf 'подмена\n' > "$V/vol/$BINS/assetc"
expect pass "подменённый байт · состав слеп" assert_dmg_composition "$ROOT" "$V/vol"
expect fail "подменённый байт · зеркало стейджа" assert_dmg_mirrors_stage "$V/vol" "$V/stage"
expect fail "подменённый байт · манифест" assert_dmg_matches "$V/vol" "$V/vol.manifest"

V=$(vol twoextra)
printf 'a\n' > "$V/vol/$DMG_APP_NAME/Contents/Resources/a.txt"
printf 'b\n' > "$V/vol/$DMG_APP_NAME/Contents/Resources/b.txt"
expect fail "лишних файлов больше одного" assert_dmg_mirrors_stage "$V/vol" "$V/stage"

# --- симлинк ----------------------------------------------------------------------------------------
V=$(vol nolink); rm -f "$V/vol/Applications"
expect fail "симлинка Applications нет" assert_dmg_applications_link "$V/vol"

V=$(vol badlink); rm -f "$V/vol/Applications"; ln -s "$DMG_APP_NAME" "$V/vol/Applications"
expect fail "симлинк ведёт в сам том" assert_dmg_applications_link "$V/vol"

# --- права -------------------------------------------------------------------------------------------
V=$(vol noexec); chmod 644 "$V/vol/$BINS/$DMG_APP_EXE"
expect fail "точка входа потеряла бит +x · права" assert_dmg_modes "$V/vol"
expect fail "точка входа потеряла бит +x · plist" assert_dmg_plist "$V/vol" "$VER" "$SHA"

V=$(vol execres); chmod 755 "$V/vol/$STAMPF"
expect fail "ресурс получил бит +x" assert_dmg_modes "$V/vol"

# Пропавший каталог — не «нарушений нет»: `find` по нему молчит, цикл не выполняется ни разу, и
# утверждение без счётчика печатало бы `ok` про бандл вовсе без точки входа.
V=$(vol nobins); rm -rf "${V:?}/vol/$BINS"
expect fail "в бандле нет каталога MacOS · права" assert_dmg_modes "$V/vol"

V=$(vol emptyres); rm -f "$V/vol/$STAMPF" "$V/vol/$LICS"/*
expect fail "в Resources не осталось файлов · права" assert_dmg_modes "$V/vol"

# --- как бандл находит рантайм ------------------------------------------------------------------
# Фикстуры здесь настоящие Mach-O: утверждение читает LC_LOAD_DYLIB и LC_RPATH, а текстовому файлу
# `otool` не скажет ни того, ни другого. Сборка фикстуры, которая не собралась, — БРАК набора, а не
# пропуск: молча пропущенный сценарий выглядит ровно как пройденный.
RP="$FIX/rpath"
if dmg_fixture_macho "$RP/ok/$BINS" "$RUNTIME" "@rpath/$RUNTIME" "@executable_path" \
   && dmg_fixture_macho "$RP/abs/$BINS" "$RUNTIME" "/opt/like-nes/$RUNTIME" "@executable_path" \
   && dmg_fixture_macho "$RP/norp/$BINS" "$RUNTIME" "@rpath/$RUNTIME" "" \
   && dmg_fixture_macho "$RP/absrp/$BINS" "$RUNTIME" "@rpath/$RUNTIME" "/opt/like-nes/lib"; then
  expect pass "исполняемый зовёт @rpath и разворачивает его рядом с собой" \
    assert_dmg_rpath "$RP/ok" "$BINS" "$RUNTIME"
  expect fail "исполняемый несёт абсолютный путь к рантайму" assert_dmg_rpath "$RP/abs" "$BINS" "$RUNTIME"
  expect fail "у исполняемого нет LC_RPATH — разворачивать @rpath нечем" \
    assert_dmg_rpath "$RP/norp" "$BINS" "$RUNTIME"
  expect fail "rpath ведёт в каталог машины сборщика" assert_dmg_rpath "$RP/absrp" "$BINS" "$RUNTIME"
else
  echo "dmg-selftest: БРАК фикстурный Mach-O не собрался — утверждение о rpath проверять нечем" >&2
  BAD=1
fi
# Текстовые «бинари» остальных фикстур обязаны быть отбиты ВСЛУХ: `otool` отвечает на них «is not an
# object file» кодом НОЛЬ, и молчаливый пропуск сделал бы утверждение вечно зелёным.
expect fail "в MacOS не Mach-O" assert_dmg_rpath "$G/vol" "$BINS" "$RUNTIME"
expect fail "имя рантайма не названо" assert_dmg_rpath "$RP/ok" "$BINS" ""

# --- Info.plist ---------------------------------------------------------------------------------------
V=$(vol notplist); printf 'это не plist\n' > "$V/vol/$DMG_APP_NAME/Contents/Info.plist"
expect fail "Info.plist не разбирается" assert_dmg_plist "$V/vol" "$VER" "$SHA"

V=$(vol otherexe)
dmg_plist чужой "$SHA" > "$V/vol/$DMG_APP_NAME/Contents/Info.plist"
expect fail "plist называет чужую точку входа" assert_dmg_plist "$V/vol" "$VER" "$SHA"

V=$(vol otherver)
sed -i.bak "s|<string>$VER</string>|<string>v9.9.9</string>|" "$V/vol/$DMG_APP_NAME/Contents/Info.plist"
rm -f "$V/vol/$DMG_APP_NAME/Contents/Info.plist.bak"
expect fail "plist называет чужую версию" assert_dmg_plist "$V/vol" "$VER" "$SHA"

V=$(vol othercommit)
sed -i.bak "s|<string>$SHA</string>|<string>deadbee</string>|" "$V/vol/$DMG_APP_NAME/Contents/Info.plist"
rm -f "$V/vol/$DMG_APP_NAME/Contents/Info.plist.bak"
expect fail "plist называет чужой коммит" assert_dmg_plist "$V/vol" "$VER" "$SHA"

# --- штамп -------------------------------------------------------------------------------------------
V=$(vol othertriple)
printf 'like-nes engine %s\ncommit %s\ntarget linux-x86_64\n' "$VER" "$SHA" > "$V/vol/$STAMPF"
expect fail "штамп называет чужую тройку" assert_stamp "$V/vol" "$VER" "$TRIPLE" "$STAMPF"

V=$(vol nostamp); rm -f "$V/vol/$STAMPF"
expect fail "в бандле нет штампа" assert_stamp "$V/vol" "$VER" "$TRIPLE" "$STAMPF"

# --- манифест двух прогонов ----------------------------------------------------------------------------
# Расходятся тома, собранные из РАЗНЫХ деревьев (чужой коммит в штампе), а не испорченные после
# снятия манифеста: у тех манифест снят до порчи и совпадает по построению — это утверждение тогда
# проверяло бы порядок строк набора, а не сравнение.
dmg_fixture_vol "$ROOT" "$FIX/other" "$VER" deadbee "$TRIPLE" && dmg_fixture_manifest "$FIX/other"
V=$(vol second)
expect fail "манифесты томов из разных деревьев" assert_same_manifest "$G/vol.manifest" "$FIX/other/vol.manifest"
expect pass "манифесты одинаковых томов совпали" assert_same_manifest "$G/vol.manifest" "$V/vol.manifest"

bash "$ROOT/scripts/check_release_dmg_impl_selftest.sh" || BAD=1

if [ "$BAD" != 0 ]; then echo "dmg-selftest: FAIL" >&2; exit 1; fi
echo "dmg-selftest: PASS"
