#!/usr/bin/env bash
# Позитивный контроль СЛОМАННЫМИ ФИКСТУРАМИ (спека #20, вертикаль 4, шаг B): AppDir с вырезанной или
# испорченной частью обязан валить стерегущее её утверждение. Утверждение, у которого нет фикстуры,
# где оно падает, неотличимо от отсутствующего.
#
# Подмены РЕАЛИЗАЦИЙ живут в …_impl_selftest.sh, вызов инструмента — в …_pack_selftest.sh, то, что
# уезжает в СУММУ образа, — в …_norm_selftest.sh, связь внутри ELF — в …_runpath_selftest.sh: граница
# по предмету, та же, что делит наборы шага A. Каждый следующий зовётся ВНЕШНЕЙ командой.
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
# shellcheck source=scripts/release_appimage_fixture_lib.sh
. "$ROOT/scripts/release_appimage_fixture_lib.sh"

# Набор гоняется и на Linux, и на macOS — раскладка AppDir не требует ни appimagetool, ни squashfs, и
# правила выгоднее проверять на обеих машинах, а не только на той, где собирается пакет. Windows
# пропускается ВСЛУХ: git-bash не моделирует бит исполнения, фикстуры прав доказывали бы там
# платформу, а не правило, — а молчаливый пропуск читался бы как пройденный набор.
case "$(uname -s)" in
  Linux|Darwin) : ;;
  *) echo "appimage-selftest: ПРОПУСК — git-bash не моделирует бит исполнения (здесь $(uname -s))"; exit 0 ;;
esac

VER=v0.0.0-check
SHA=abc1234
TRIPLE=linux-x86_64
LICS=usr/share/licenses/like-nes
STAMPF=usr/share/like-nes/version.txt
BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'appimage-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'appimage-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Каждая фикстура — свой каталог со своим стейджем: порча одного AppDir иначе доезжала бы до
# следующего утверждения, и «отбито» выходило бы из чужой поломки.
appdir() {
  appimage_fixture_appdir "$ROOT" "$FIX/$1" "$VER" "$SHA" "$TRIPLE" || return 1
  appimage_fixture_manifest "$FIX/$1"
  printf '%s\n' "$FIX/$1"
}

LIC1=$(basename "$(head -1 "$ROOT/cmake/licenses.manifest")")
RUNTIME=libwgpu_native.so
ICON="$ROOT/packaging/$APPIMAGE_ICON"
CACHE="$FIX/CMakeCache.txt"
printf 'WGPU_RUNTIME_LIB:FILEPATH=/nowhere/%s\n' "$RUNTIME" > "$CACHE"

# --- опора: нетронутый AppDir --------------------------------------------------------------------
G=$(appdir good)
if [ ! -f "$G/AppDir/AppRun" ]; then
  echo "appimage-selftest: БРАК фикстурный AppDir не собрался — порчи ниже отбились бы за отсутствием предмета" >&2
  exit 1
fi
expect pass "нетронутый AppDir · состав" assert_appimage_composition "$ROOT" "$G/AppDir"
expect pass "нетронутый AppDir · зеркало стейджа" assert_appimage_mirrors_stage "$G/AppDir" "$G/stage"
expect pass "нетронутый AppDir · AppRun" assert_appimage_apprun "$G/AppDir"
expect pass "нетронутый AppDir · .desktop" assert_appimage_desktop "$G/AppDir"
expect pass "нетронутый AppDir · значок" assert_appimage_icon "$G/AppDir" "$ICON"
expect pass "нетронутый AppDir · права" assert_appimage_modes "$G/AppDir"
expect pass "нетронутый AppDir · манифест" assert_dir_matches "$G/AppDir" "$G/AppDir.manifest"
expect pass "нетронутый AppDir · лицензии" assert_licenses "$ROOT" "$G/AppDir" "$LICS"
expect pass "нетронутый AppDir · рантайм" assert_runtime_named "$CACHE" "$G/AppDir" usr/bin
expect pass "нетронутый AppDir · штамп" assert_stamp "$G/AppDir" "$VER" "$TRIPLE" "$STAMPF"

# --- состав ----------------------------------------------------------------------------------------
V=$(appdir nolic); rm -f "$V/AppDir/$LICS/$LIC1"
expect fail "в AppDir нет лицензии · состав" assert_appimage_composition "$ROOT" "$V/AppDir"
expect fail "в AppDir нет лицензии · лицензии" assert_licenses "$ROOT" "$V/AppDir" "$LICS"

V=$(appdir zerolic); : > "$V/AppDir/$LICS/$LIC1"
expect pass "лицензия в нуль байт · состав слеп к содержимому" assert_appimage_composition "$ROOT" "$V/AppDir"
expect fail "лицензия в нуль байт · лицензии" assert_licenses "$ROOT" "$V/AppDir" "$LICS"

V=$(appdir extra); printf 'x\n' > "$V/AppDir/usr/share/лишний.txt"
expect fail "в AppDir лишний файл" assert_appimage_composition "$ROOT" "$V/AppDir"

V=$(appdir noruntime); rm -f "$V/AppDir/usr/bin/$RUNTIME"
expect fail "в AppDir нет рантайма" assert_runtime_named "$CACHE" "$V/AppDir" usr/bin

# --- зеркало стейджа ---------------------------------------------------------------------------------
# Подменённый байт не меняет ни имён, ни числа файлов: состав его не видит по построению.
V=$(appdir tampered); printf 'подмена\n' > "$V/AppDir/usr/bin/assetc"
expect pass "подменённый байт · состав слеп" assert_appimage_composition "$ROOT" "$V/AppDir"
expect fail "подменённый байт · зеркало стейджа" assert_appimage_mirrors_stage "$V/AppDir" "$V/stage"
expect fail "подменённый байт · манифест" assert_dir_matches "$V/AppDir" "$V/AppDir.manifest"

V=$(appdir twoextra)
printf 'a\n' > "$V/AppDir/usr/share/a.txt"
expect fail "лишних файлов больше четырёх" assert_appimage_mirrors_stage "$V/AppDir" "$V/stage"

# --- AppRun ------------------------------------------------------------------------------------------
V=$(appdir noapprun); rm -f "$V/AppDir/AppRun"
expect fail "AppRun вырезан" assert_appimage_apprun "$V/AppDir"

V=$(appdir apprunnoexec); chmod 644 "$V/AppDir/AppRun"
expect fail "AppRun потерял бит +x" assert_appimage_apprun "$V/AppDir"

V=$(appdir apprunother)
printf '#!/bin/sh\nexec "$here/usr/bin/чужой" "$@"\n' > "$V/AppDir/AppRun"; chmod 755 "$V/AppDir/AppRun"
expect fail "AppRun зовёт несуществующий бинарь" assert_appimage_apprun "$V/AppDir"

V=$(appdir apprunpath)
printf '#!/bin/sh\nexec editor_shell "$@"\n' > "$V/AppDir/AppRun"; chmod 755 "$V/AppDir/AppRun"
expect fail "AppRun зовёт точку входа по PATH" assert_appimage_apprun "$V/AppDir"

# --- .desktop ------------------------------------------------------------------------------------------
V=$(appdir nodesktop); rm -f "$V/AppDir/$APPIMAGE_DESKTOP"
expect fail ".desktop вырезан" assert_appimage_desktop "$V/AppDir"

# Ключ в чужой секции — ровно то, чего не видит греп: файл выглядит наполненным и разбирается в
# пустоту.
V=$(appdir othersection)
printf '[Desktop Entry]\nType=Application\nIcon=like-nes\n[Other]\nExec=%s\n' "$APPIMAGE_EXE" \
  > "$V/AppDir/$APPIMAGE_DESKTOP"
expect fail ".desktop называет точку входа в чужой секции" assert_appimage_desktop "$V/AppDir"

V=$(appdir desktopexe)
sed 's|^Exec=.*|Exec=чужой|' "$V/AppDir/$APPIMAGE_DESKTOP" > "$V/d" && mv "$V/d" "$V/AppDir/$APPIMAGE_DESKTOP"
expect fail ".desktop называет чужую точку входа" assert_appimage_desktop "$V/AppDir"

V=$(appdir desktopicon)
sed 's|^Icon=.*|Icon=чужой|' "$V/AppDir/$APPIMAGE_DESKTOP" > "$V/d" && mv "$V/d" "$V/AppDir/$APPIMAGE_DESKTOP"
expect fail ".desktop называет значок, которого нет" assert_appimage_desktop "$V/AppDir"

V=$(appdir desktoptype)
sed 's|^Type=.*|Type=Directory|' "$V/AppDir/$APPIMAGE_DESKTOP" > "$V/d" && mv "$V/d" "$V/AppDir/$APPIMAGE_DESKTOP"
expect fail ".desktop не объявляет Type=Application" assert_appimage_desktop "$V/AppDir"

# --- значок --------------------------------------------------------------------------------------------
V=$(appdir icondrift); printf 'не png\n' > "$V/AppDir/$APPIMAGE_ICON"
expect fail "значок разошёлся с деревом" assert_appimage_icon "$V/AppDir" "$ICON"

V=$(appdir diricondrift); printf 'не png\n' > "$V/AppDir/.DirIcon"
expect fail ".DirIcon разошёлся со значком" assert_appimage_icon "$V/AppDir" "$ICON"

expect fail "значка нет в дереве" assert_appimage_icon "$G/AppDir" "$FIX/нет.png"

# --- права ---------------------------------------------------------------------------------------------
V=$(appdir noexec); chmod 644 "$V/AppDir/usr/bin/$APPIMAGE_EXE"
expect fail "точка входа потеряла бит +x · права" assert_appimage_modes "$V/AppDir"
expect fail "точка входа потеряла бит +x · AppRun" assert_appimage_apprun "$V/AppDir"
expect fail "точка входа потеряла бит +x · .desktop" assert_appimage_desktop "$V/AppDir"

V=$(appdir execlic); chmod 755 "$V/AppDir/$LICS/$LIC1"
expect fail "лицензия получила бит +x" assert_appimage_modes "$V/AppDir"

# Пропавший каталог — не «нарушений нет»: `find` молчит, цикл не идёт, и утверждение без счётчика печатало бы `ok` про AppDir без точки входа.
V=$(appdir nobins); rm -rf "${V:?}/AppDir/usr/bin"
expect fail "в AppDir нет каталога usr/bin · права" assert_appimage_modes "$V/AppDir"

V=$(appdir nolics); rm -rf "${V:?}/AppDir/$LICS"
expect fail "в AppDir не осталось лицензий · права" assert_appimage_modes "$V/AppDir"

# --- штамп -----------------------------------------------------------------------------------------------
V=$(appdir othertriple)
printf 'like-nes engine %s\ncommit %s\ntarget macos-arm64\n' "$VER" "$SHA" > "$V/AppDir/$STAMPF"
expect fail "штамп называет чужую тройку" assert_stamp "$V/AppDir" "$VER" "$TRIPLE" "$STAMPF"

V=$(appdir nostamp); rm -f "$V/AppDir/$STAMPF"
expect fail "в AppDir нет штампа" assert_stamp "$V/AppDir" "$VER" "$TRIPLE" "$STAMPF"

# --- два прогона -------------------------------------------------------------------------------------------
# Расходятся AppDir из РАЗНЫХ деревьев (чужой коммит в штампе): у испорченного после снятия манифеста тот снят до порчи и совпал бы по построению.
appimage_fixture_appdir "$ROOT" "$FIX/other" "$VER" deadbee "$TRIPLE" && appimage_fixture_manifest "$FIX/other"
V=$(appdir second)
expect fail "манифесты AppDir из разных деревьев" assert_same_manifest "$G/AppDir.manifest" "$FIX/other/AppDir.manifest"
expect pass "манифесты одинаковых AppDir совпали" assert_same_manifest "$G/AppDir.manifest" "$V/AppDir.manifest"

# Байт-равенство образов: предмет утверждения — файлы, а не AppDir, поэтому фикстуры у него свои.
printf 'один\n' > "$FIX/a.img"; cp "$FIX/a.img" "$FIX/b.img"; printf 'другой\n' > "$FIX/c.img"
expect pass "два одинаковых образа" assert_appimage_reproducible "$FIX/a.img" "$FIX/b.img"
expect fail "образы разошлись байтами" assert_appimage_reproducible "$FIX/a.img" "$FIX/c.img"
# Пропавшие образы — не «совпало»: sha256_of отдаёт пустую строку, и равенство пустых было бы «ок».
expect fail "образов нет вовсе" assert_appimage_reproducible "$FIX/missing1" "$FIX/missing2"

bash "$ROOT/scripts/check_release_appimage_impl_selftest.sh" || BAD=1
bash "$ROOT/scripts/check_release_appimage_pack_selftest.sh" || BAD=1
bash "$ROOT/scripts/check_release_appimage_runpath_selftest.sh" || BAD=1

if [ "$BAD" != 0 ]; then echo "appimage-selftest: FAIL" >&2; exit 1; fi
echo "appimage-selftest: PASS"
