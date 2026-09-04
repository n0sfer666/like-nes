#!/usr/bin/env bash
# Живой цикл образа (спека #20, вертикаль 4, шаг A): упаковать, смонтировать, снять. Единственное
# место во всём наборе, где запускается `hdiutil`, — потому и файл свой: у соседей предмет чисто
# файловый (содержимое тома и раскладка бандла), они идут за доли секунды и не оставляют за собой
# ничего, что нужно снимать с системы.
#
# Без этого файла утверждение о следах монтирования проверялось бы на каталоге, который никто не
# монтировал, то есть не проверялось бы вовсе: `hdiutil info` о таком каталоге молчит, и
# assert_no_mount_left проходил бы всегда.
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
# shellcheck source=scripts/release_dmg_fixture_lib.sh
. "$ROOT/scripts/release_dmg_fixture_lib.sh"

if [ "$(uname -s)" != Darwin ]; then
  echo "dmg-live-selftest: ПРОПУСК — образ .dmg собирается только на macOS (здесь $(uname -s))"
  exit 0
fi

BAD=0
FIX=$(mktemp -d)
MNT="$FIX/live.mnt"
# Уборка снимает том ПЕРВОЙ строкой, как и в гейте: `rm -rf` по каталогу с примонтированным образом
# на macOS не удаляет ничего и оставляет том в системе — след, которого `git status` не видит.
trap 'dmg_umount "$MNT"; rm -rf "$FIX"' EXIT

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'dmg-live-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'dmg-live-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

dmg_fixture_vol "$ROOT" "$FIX/live" v0.0.0-check abc1234 macos-arm64 \
  || { echo "dmg-live-selftest: БРАК том не собрался — проверять нечего" >&2; exit 1; }
dmg_fixture_manifest "$FIX/live"
if ! dmg_pack "$FIX/live/vol" "$FIX/live.dmg" "like-nes selftest" \
   || ! dmg_mount "$FIX/live.dmg" "$MNT"; then
  echo "dmg-live-selftest: БРАК образ не собрался или не смонтировался — проверять нечего" >&2
  exit 1
fi

# Опорное утверждение цикла: то, что уехало в образ, совпало с тем, что паковали. Оно же — предмет
# гейта, только там образ делает release.sh, а здесь фабрика фикстур.
expect pass "смонтированный образ совпал со своим манифестом" assert_dir_matches "$MNT" "$FIX/live/vol.manifest"
expect fail "смонтированный том · утверждение о следах" assert_no_mount_left "$MNT"
# Сосед по префиксу: «…/live» — начало «…/live.mnt», и утверждение, ищущее ВХОЖДЕНИЕ подстроки,
# обвинило бы прогон в следе, которого он не оставлял. Разница видна только на смонтированном томе.
expect pass "чужой путь с общим префиксом не читается как смонтированный том" \
  assert_no_mount_left "$FIX/live"

# Размонтирование-пустышка: гейт, который зовёт её вместо настоящей, оставляет том в системе и
# печатает PASS. Отбить это может только утверждение о следах, и вот доказательство, что оно умеет.
noop_umount() ( dmg_umount() { :; }; dmg_umount "$MNT"; assert_no_mount_left "$MNT" )
expect fail "размонтирование-пустышка не убрало том" noop_umount

dmg_umount "$MNT"
expect pass "после настоящего размонтирования следов нет" assert_no_mount_left "$MNT"

if [ "$BAD" != 0 ]; then echo "dmg-live-selftest: FAIL" >&2; exit 1; fi
echo "dmg-live-selftest: PASS"
