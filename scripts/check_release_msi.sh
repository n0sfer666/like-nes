#!/usr/bin/env bash
# Гейт установщика Windows (спека #20, вертикаль 4, шаг C): собрать пакет ДВАЖДЫ и осмотреть то, что
# уезжает к людям, — состав, лицензии, штамп, пользовательскую установку, полноту деинсталляции,
# линию обновления и ярлык. Правила живут в scripts/release_msi_check_lib.sh, здесь только прогон.
#
# Граница воспроизводимости у MSI та же, что у `.dmg`, и противоположна `.AppImage`: байт-равенства
# формат не даёт — в сводный поток пакета едут случайный package code и время сборки. ВЫЯСНЕНО
# прогоном, а не постулировано: два пакета из одного исходника совпали размером и разошлись ровно
# строкой 9 сводки, все прочие таблицы совпали. Поэтому сверяется СОДЕРЖИМОЕ — все таблицы плюс
# развёрнутое дерево.
#
# Стейдж гейт делает себе САМ, а не берёт у release.sh: `--only windows` с машины владельца уходит
# в CI, то есть настоящего стейджа Windows здесь взяться неоткуда. Фабрика — ci_make_pkg, та же,
# которой вертикаль 3 делает фикстурный пакет Windows: своя копия правил раскладки означала бы, что
# гейт осматривает пакет иного устройства, чем тот, что едет к людям.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_dmg_lib.sh
. "$ROOT/scripts/release_dmg_lib.sh"
# shellcheck source=scripts/release_appimage_lib.sh
. "$ROOT/scripts/release_appimage_lib.sh"
# shellcheck source=scripts/release_msi_lib.sh
. "$ROOT/scripts/release_msi_lib.sh"
# shellcheck source=scripts/release_msi_pack_lib.sh
. "$ROOT/scripts/release_msi_pack_lib.sh"
# shellcheck source=scripts/release_extra_lib.sh
. "$ROOT/scripts/release_extra_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"
# shellcheck source=scripts/release_ci_check_lib.sh
. "$ROOT/scripts/release_ci_check_lib.sh"
# shellcheck source=scripts/release_ci_fixture_lib.sh
. "$ROOT/scripts/release_ci_fixture_lib.sh"
# shellcheck source=scripts/release_msi_check_lib.sh
. "$ROOT/scripts/release_msi_check_lib.sh"
# shellcheck source=scripts/release_msi_behavior_lib.sh
. "$ROOT/scripts/release_msi_behavior_lib.sh"
# shellcheck source=scripts/release_check_hygiene.sh
. "$ROOT/scripts/release_check_hygiene.sh"

# Пропуски ВСЛУХ и кодом 0, и их два, потому что инструментов два и живут они на РАЗНЫХ машинах:
# компилятор MSI (WiX на раннере, wixl из msitools у владельца) и осмотр (msitools, которых на
# Windows нет вовсе). Молчаливый пропуск в общем прогоне preflight читался бы как пройденный гейт.
if ! msi_tool_name >/dev/null 2>&1; then
  echo "release-msi-check: ПРОПУСК — компилятора MSI нет (нужен wixl из msitools либо candle/light из WiX)"
  exit 0
fi
if ! command -v msiinfo >/dev/null 2>&1 || ! command -v msiextract >/dev/null 2>&1; then
  echo "release-msi-check: ПРОПУСК — msitools не установлены, осматривать пакет нечем (brew install msitools)"
  exit 0
fi

VER=${LIKE_NES_RELEASE_VERSION:-v0.0.0-check}
TRIPLE=windows-x86_64
NAME="like-nes-engine-$VER-$TRIPLE"
TMP=$(mktemp -d)
FAIL=0
BEFORE=$(git -C "$ROOT" status --porcelain)
trap 'rm -rf "$TMP"' EXIT

COMMIT=$(git -C "$ROOT" rev-parse HEAD)
ci_make_pkg "$ROOT" "$TMP/fx" "$VER" "$COMMIT" "$TRIPLE" >/dev/null || {
  bad "фикстурный стейдж Windows не собрался"; exit 1; }
STAGE="$TMP/fx/stage"

pack_once() {
  local n="$1"
  mkdir -p "$TMP/b$n/stage" "$TMP/d$n"
  extra_build windows "$STAGE" "$TMP/d$n" "$NAME" "$VER" 202601010000.00 "$TMP/b$n" "$ROOT/packaging"
}
pack_once 1 || { bad "первый прогон упаковки отказал"; exit 1; }
pack_once 2 || { bad "второй прогон упаковки отказал"; exit 1; }
MSI1="$TMP/d1/$NAME.msi"
MSI2="$TMP/d2/$NAME.msi"
[ -s "$MSI1" ] && [ -s "$MSI2" ] || { bad "пакета нет после прогона упаковки"; exit 1; }

assert_msi_runs_distinct "$MSI1" "$MSI2" || FAIL=1
assert_msi_content_reproducible "$MSI1" "$MSI2" || FAIL=1

EX="$TMP/ex"
msi_extract_to "$MSI1" "$EX" || { bad "пакет не разворачивается — осматривать нечего"; exit 1; }
assert_msi_mirrors_stage "$EX" "$STAGE" || FAIL=1
assert_composition "$ROOT" "$EX" MINGW || FAIL=1
assert_licenses "$ROOT" "$EX" || FAIL=1
assert_stamp "$EX" "$VER" "$TRIPLE" || FAIL=1
assert_msi_per_user "$MSI1" || FAIL=1
assert_msi_uninstall "$MSI1" || FAIL=1
assert_msi_upgrade "$MSI1" "$VER" || FAIL=1
assert_msi_version "$MSI1" "$VER" || FAIL=1
assert_msi_shortcut "$MSI1" || FAIL=1
# Манифест, лежащий рядом с пакетом, обязан описывать ПАКЕТ: по нему владелец опознаёт содержимое,
# не разворачивая, и разъехаться с ним нечем, кроме нашей ошибки.
assert_dir_matches "$EX" "$TMP/d1/$NAME.msi.manifest" || FAIL=1
assert_tree_unchanged "$BEFORE" "$(git -C "$ROOT" status --porcelain)" || FAIL=1

if [ "$FAIL" = 0 ]; then
  echo "release-msi-check: PASS"
else
  echo "release-msi-check: FAIL" >&2
fi
exit "$FAIL"
