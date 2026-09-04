#!/usr/bin/env bash
# Гейт образа macOS (спека #20, вертикаль 4, шаг A): упаковать ДВАЖДЫ и утверждать про то, что
# уезжает к людям, — состав, содержимое, права, лицензии, штамп, Info.plist и то, чем бандл
# находит рантайм. Правила живут в
# scripts/release_dmg_check_lib.sh, здесь только прогон; сломанные фикстуры гоняет
# scripts/check_release_dmg_selftest.sh.
#
# Два прогона сверяются СОДЕРЖИМЫМ, а не суммой образа: `hdiutil` пишет в UDIF своё время и UUID,
# два create из одного каталога дают разные байты, и «воспроизводимость .dmg» была бы обещанием,
# которого формат не даёт. Граница названа вслух — здесь, в ADR и в docs/owner-setup.txt.
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
# shellcheck source=scripts/release_check_hygiene.sh
. "$ROOT/scripts/release_check_hygiene.sh"

# Пропуск ВСЛУХ и кодом 0: образ собирает `hdiutil`, которого нет нигде, кроме macOS, и молчаливый
# пропуск в общем прогоне preflight читался бы как пройденный гейт.
if [ "$(uname -s)" != Darwin ]; then
  echo "release-dmg-check: ПРОПУСК — образ .dmg собирается только на macOS (здесь $(uname -s))"
  exit 0
fi

VER=${LIKE_NES_RELEASE_VERSION:-v0.0.0-check}
BUILD=${LIKE_NES_RELEASE_BUILD:-$ROOT/build-release}
TMP=$(mktemp -d)
OUT1="$ROOT/release"
OUT2="$TMP/second"
MNT1="$TMP/mnt1"
MNT2="$TMP/mnt2"
NAME="like-nes-engine-$VER"
FAIL=0
# Уборка снимает тома ПЕРВОЙ строкой: `rm -rf` по каталогу, на котором висит смонтированный образ,
# на macOS не удаляет ничего и оставляет том в системе — след, которого `git status` не видит.
trap 'dmg_umount "$MNT1"; dmg_umount "$MNT2"; rm -rf "$TMP" "$OUT1/$VER"; rmdir "$OUT1" 2>/dev/null || true' EXIT
BEFORE=$(git -C "$ROOT" status --porcelain)

echo "=== прогон 1: $OUT1"
bash "$ROOT/scripts/release.sh" --version "$VER" --out "$OUT1" --build "$BUILD" >/dev/null || {
  bad "первый прогон release.sh упал"; exit 1; }
echo "=== прогон 2: $OUT2"
bash "$ROOT/scripts/release.sh" --version "$VER" --out "$OUT2" --build "$BUILD" >/dev/null || {
  bad "второй прогон release.sh упал"; exit 1; }

DMG1=$(ls "$OUT1/$VER/$NAME"-*.dmg 2>/dev/null | head -1)
DMG2=$(ls "$OUT2/$VER/$NAME"-*.dmg 2>/dev/null | head -1)
if [ -z "$DMG1" ] || [ -z "$DMG2" ]; then
  bad "образ не найден после прогона: '$DMG1' / '$DMG2'"; exit 1
fi
MAN1="${DMG1%.dmg}.dmg.manifest"
MAN2="${DMG2%.dmg}.dmg.manifest"
# Тройка берётся ИЗ ИМЕНИ образа и уходит в утверждение о штампе — тот же приём, что у tar.gz:
# имя и штамп обязаны называть одну платформу.
TRIPLE=$(basename "$DMG1" .dmg); TRIPLE=${TRIPLE#"like-nes-engine-$VER-"}
STAGE="$BUILD/stage/like-nes-engine-$VER-$TRIPLE"
# Стейдж проверяется на существование ВСЛУХ: промахнись вырезание тройки на строку выше, и
# утверждение о зеркале стейджа сравнивало бы образ с пустотой — «содержимое доехало не целиком»
# вместо «гейт смотрит не туда».
[ -d "$STAGE" ] || { bad "стейдж не найден: $STAGE"; exit 1; }
RUNTIME=$(runtime_lib_name "$BUILD/CMakeCache.txt") || exit 1

dmg_mount "$DMG1" "$MNT1" || { bad "образ первого прогона не монтируется"; exit 1; }
COMMIT=$(sed -n 2p "$MNT1/$DMG_APP_NAME/Contents/Resources/version.txt" 2>/dev/null)
COMMIT=${COMMIT#commit }

assert_dmg_composition "$ROOT" "$MNT1" || FAIL=1
assert_dmg_mirrors_stage "$MNT1" "$STAGE" || FAIL=1
assert_dmg_applications_link "$MNT1" || FAIL=1
assert_dmg_modes "$MNT1" || FAIL=1
assert_dmg_plist "$MNT1" "$VER" "$COMMIT" || FAIL=1
assert_dmg_matches "$MNT1" "$MAN1" || FAIL=1
assert_licenses "$ROOT" "$MNT1" "$DMG_APP_NAME/Contents/Resources/licenses" || FAIL=1
assert_runtime_named "$BUILD/CMakeCache.txt" "$MNT1" "$DMG_APP_NAME/Contents/MacOS" || FAIL=1
assert_stamp "$MNT1" "$VER" "$TRIPLE" "$DMG_APP_NAME/Contents/Resources/version.txt" || FAIL=1
assert_dmg_rpath "$MNT1" "$DMG_APP_NAME/Contents/MacOS" "$RUNTIME" || FAIL=1
dmg_umount "$MNT1"

# Второй образ монтируется и сверяется со СВОИМ манифестом, а не только манифесты между собой:
# равенство манифестов — утверждение о двух каталогах стейджа, и образ второго прогона без этого
# шага не осматривает вообще никто.
assert_same_manifest "$MAN1" "$MAN2" || FAIL=1
dmg_mount "$DMG2" "$MNT2" || { bad "образ второго прогона не монтируется"; FAIL=1; }
assert_dmg_matches "$MNT2" "$MAN2" || FAIL=1
dmg_umount "$MNT2"

assert_sums_listed "$OUT1/$VER/SHA256SUMS" "$DMG1" "$(sha256_of "$DMG1")" || FAIL=1
assert_ignored "$ROOT" release || FAIL=1

rm -rf "${OUT1:?}/$VER"
if [ -e "$OUT1/$VER" ]; then
  bad "каталог прогона $OUT1/$VER остался в дереве"; FAIL=1
else
  ok "каталог прогона убран за собой"
fi
assert_no_mount_left "$MNT1" || FAIL=1
assert_no_mount_left "$MNT2" || FAIL=1
assert_tree_unchanged "$BEFORE" "$(git -C "$ROOT" status --porcelain)" || FAIL=1

if [ "$FAIL" = 0 ]; then
  echo "release-dmg-check: PASS"
else
  echo "release-dmg-check: FAIL" >&2
fi
exit "$FAIL"
