#!/usr/bin/env bash
# Гейт образа Linux (спека #20, вертикаль 4, шаг B): упаковать ДВАЖДЫ, утверждать байт-равенство и
# осмотреть то, что уезжает к людям, — состав, содержимое, права, лицензии, штамп, AppRun, `.desktop`
# и то, чем образ находит рантайм. Правила живут в scripts/release_appimage_check_lib.sh, здесь
# только прогон; сломанные фикстуры гоняют check_release_appimage_selftest.sh и его impl-близнец.
#
# У `.dmg` шага A байт-равенство пришлось признать недостижимым (hdiutil пишет в UDIF время и UUID),
# и граница названа вслух. Здесь оно достижимо, но НЕ ДАРОМ: в squashfs едут содержимое, mtime,
# владелец и права — включая права КАТАЛОГОВ, — то есть без выравнивания сумма образа зависела бы от
# umask и от того, под кем шла сборка. Выравнивает их упаковщик (`-all-root`, явный chmod), и оба
# прогона гейта слепы к этому по построению: они идут в одном шелле от одного пользователя. Поэтому
# рядом с равенством сумм стоит `assert_appimage_normalized`, читающая владельца и права ИЗ ОБРАЗА, —
# ровно тот довод, ради которого для tar.gz заведён `assert_pack_normalized`.
#
# Содержимое осматривается СВЕРХ равенства сумм, а не вместо: равные суммы двух образов говорят, что
# упаковщик повторяем, и ничего не говорят о том, что внутри.
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
# shellcheck source=scripts/release_appimage_runpath_lib.sh
. "$ROOT/scripts/release_appimage_runpath_lib.sh"
# shellcheck source=scripts/release_appimage_squash_lib.sh
. "$ROOT/scripts/release_appimage_squash_lib.sh"
# shellcheck source=scripts/release_check_hygiene.sh
. "$ROOT/scripts/release_check_hygiene.sh"

# Пропуск ВСЛУХ и кодом 0: образ собирает appimagetool, которого нет ни на macOS-машине владельца,
# ни на windows-раннере, а молчаливый пропуск в общем прогоне preflight читался бы как пройденный
# гейт. На машине владельца этот путь закрыт контейнером — тем же, что собирает tar.gz.
if [ "$(uname -s)" != Linux ]; then
  echo "release-appimage-check: ПРОПУСК — .AppImage собирается только на Linux (здесь $(uname -s))"
  exit 0
fi
if ! command -v appimagetool >/dev/null 2>&1; then
  echo "release-appimage-check: ПРОПУСК — appimagetool не установлен (он живёт в образе scripts/release_linux.Dockerfile)"
  exit 0
fi

VER=${LIKE_NES_RELEASE_VERSION:-v0.0.0-check}
BUILD=${LIKE_NES_RELEASE_BUILD:-$ROOT/build-release}
TMP=$(mktemp -d)
OUT1="$ROOT/release"
OUT2="$TMP/second"
NAME="like-nes-engine-$VER"
FAIL=0
BEFORE=$(git -C "$ROOT" status --porcelain)
# Утверждение идёт ДО установки trap, и это несущий порядок: первый прогон пишет прямо в `release/`
# дерева, а убирает за собой `rm -rf`. Версия берётся из окружения, поэтому прогон с версией
# настоящего релиза затирал бы готовый пакет и удалял его вместе с SHA256SUMS — то есть проверка
# ломала бы предмет проверки. Находка ревью шага B вертикали 4.
assert_run_dir_absent "$OUT1/$VER" || exit 1
trap 'rm -rf "$TMP" "$OUT1/$VER"; rmdir "$OUT1" 2>/dev/null || true' EXIT

echo "=== прогон 1: $OUT1"
bash "$ROOT/scripts/release.sh" --version "$VER" --out "$OUT1" --build "$BUILD" >/dev/null || {
  bad "первый прогон release.sh упал"; exit 1; }
echo "=== прогон 2: $OUT2"
bash "$ROOT/scripts/release.sh" --version "$VER" --out "$OUT2" --build "$BUILD" >/dev/null || {
  bad "второй прогон release.sh упал"; exit 1; }

IMG1=$(ls "$OUT1/$VER/$NAME"-*.AppImage 2>/dev/null | head -1)
IMG2=$(ls "$OUT2/$VER/$NAME"-*.AppImage 2>/dev/null | head -1)
if [ -z "$IMG1" ] || [ -z "$IMG2" ]; then
  bad "образ не найден после прогона: '$IMG1' / '$IMG2'"; exit 1
fi
MAN1="${IMG1%.AppImage}.AppImage.manifest"
MAN2="${IMG2%.AppImage}.AppImage.manifest"
# Тройка берётся ИЗ ИМЕНИ образа и уходит в утверждение о штампе — тот же приём, что у tar.gz и у
# `.dmg`: имя пакета и штамп внутри него обязаны называть одну платформу.
TRIPLE=$(basename "$IMG1" .AppImage); TRIPLE=${TRIPLE#"like-nes-engine-$VER-"}
STAGE="$BUILD/stage/like-nes-engine-$VER-$TRIPLE"
# Стейдж проверяется на существование ВСЛУХ: промахнись вырезание тройки на строку выше, и
# утверждение о зеркале стейджа сравнивало бы образ с пустотой — «содержимое доехало не целиком»
# вместо «гейт смотрит не туда».
[ -d "$STAGE" ] || { bad "стейдж не найден: $STAGE"; exit 1; }
RUNTIME=$(runtime_lib_name "$BUILD/CMakeCache.txt") || exit 1

assert_appimage_reproducible "$IMG1" "$IMG2" || FAIL=1
assert_same_manifest "$MAN1" "$MAN2" || FAIL=1
# Нормализация читается ИЗ ОБРАЗА, а не из реализации упаковщика: `--appimage-extract` накладывает
# на распакованное umask распаковщика и теряет владельца, поэтому осмотр каталога ниже об этом не
# скажет ничего — нужен unsquashfs.
assert_appimage_normalized "$IMG1" || FAIL=1

# Осматривается РАСПАКОВАННЫЙ образ, а не каталог, из которого паковали: подмена между раскладкой
# AppDir и appimagetool иначе прошла бы мимо всех утверждений разом. Распаковка идёт в свой каталог,
# потому что `--appimage-extract` кладёт squashfs-root в ТЕКУЩИЙ.
EXT="$TMP/ext"
mkdir -p "$EXT"
# Код возврата распаковки слушается, а её вывод печатается при отказе: без этого «образ не
# распаковался» было единственной строкой, которую видел бы владелец, а причина (нет прав, битый
# squashfs, чужая архитектура рантайма) оставалась бы в выброшенном stderr.
EXTLOG="$TMP/extract.log"
if ! ( cd "$EXT" && APPIMAGE_EXTRACT_AND_RUN=1 "$IMG1" --appimage-extract > "$EXTLOG" 2>&1 ); then
  bad "распаковка образа отказала:"
  sed 's/^/       /' "$EXTLOG" >&2
  exit 1
fi
DIR="$EXT/squashfs-root"
[ -d "$DIR" ] || { bad "образ не распаковался — осматривать нечего"; exit 1; }

assert_appimage_composition "$ROOT" "$DIR" || FAIL=1
assert_appimage_mirrors_stage "$DIR" "$STAGE" || FAIL=1
assert_appimage_apprun "$DIR" || FAIL=1
assert_appimage_desktop "$DIR" || FAIL=1
assert_appimage_icon "$DIR" "$ROOT/packaging/$APPIMAGE_ICON" || FAIL=1
assert_appimage_modes "$DIR" || FAIL=1
assert_licenses "$ROOT" "$DIR" "usr/share/licenses/like-nes" || FAIL=1
assert_runtime_named "$BUILD/CMakeCache.txt" "$DIR" "usr/bin" || FAIL=1
assert_stamp "$DIR" "$VER" "$TRIPLE" "usr/share/like-nes/version.txt" || FAIL=1
assert_appimage_runpath "$DIR" "$RUNTIME" || FAIL=1
# Манифест, лежащий рядом с пакетом, обязан описывать ПАКЕТ, а не стейдж: по нему владелец опознаёт
# содержимое, не распаковывая, и разъехаться с образом ему нечем, кроме нашей ошибки.
assert_dir_matches "$DIR" "$MAN1" || FAIL=1

assert_sums_listed "$OUT1/$VER/SHA256SUMS" "$IMG1" "$(sha256_of "$IMG1")" || FAIL=1
assert_ignored "$ROOT" release || FAIL=1

rm -rf "${OUT1:?}/$VER"
if [ -e "$OUT1/$VER" ]; then
  bad "каталог прогона $OUT1/$VER остался в дереве"; FAIL=1
else
  ok "каталог прогона убран за собой"
fi
assert_tree_unchanged "$BEFORE" "$(git -C "$ROOT" status --porcelain)" || FAIL=1

if [ "$FAIL" = 0 ]; then
  echo "release-appimage-check: PASS"
else
  echo "release-appimage-check: FAIL" >&2
fi
exit "$FAIL"
