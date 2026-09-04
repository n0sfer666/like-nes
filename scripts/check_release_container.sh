#!/usr/bin/env bash
# Гейт контейнерного пути релиза (спека #20, вертикаль 2). Два этапа, и разведены они по цене, а
# не по важности: ПРАВИЛА проверяются без демона за секунду и идут в preflight, ЖИВАЯ сборка
# требует движка и десятков минут, поэтому просится флагом `--live`.
#
# Без правил живой этап был бы единственным, то есть на машине без демона гейт молчал бы целиком —
# а именно там и правится код, который эти правила держат.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"
# shellcheck source=scripts/release_check_hygiene.sh
. "$ROOT/scripts/release_check_hygiene.sh"
# shellcheck source=scripts/release_container_lib.sh
. "$ROOT/scripts/release_container_lib.sh"
# shellcheck source=scripts/release_container_check_lib.sh
. "$ROOT/scripts/release_container_check_lib.sh"

LIVE=""
case "${1:-}" in
  --live) LIVE=1 ;;
  "") ;;
  *) echo "usage: check_release_container.sh [--live]" >&2; exit 2 ;;
esac

DOCKERFILE="$ROOT/scripts/release_linux.Dockerfile"
FAIL=0

echo "=== правила контейнерного пути (демон не нужен)"
assert_base_pinned "$DOCKERFILE" || FAIL=1
assert_no_second_packer "$ROOT/scripts/release_container.sh" || FAIL=1
assert_ro_mount "$ROOT/scripts/release_container.sh" || FAIL=1
assert_tag_per_arch "$DOCKERFILE" || FAIL=1
assert_engine_liveness || FAIL=1
assert_broken_cache_detected || FAIL=1
assert_cache_outside_tree "$ROOT" || FAIL=1
assert_refusal_without_engine "$ROOT/scripts/release_container.sh" || FAIL=1
assert_platform_rejected_on_host "$ROOT/scripts/release.sh" || FAIL=1

# Две формы отказа проверяются только там, где чужая платформа действительно чужая: на линукс-хосте
# `--only linux` — своя сборка, на Windows своя же `--only windows`, и утверждение вместо отказа
# запустило бы полную сборку. Пропуск ГРОМКИЙ: «пропущено молча» и «находок нет» с одного взгляда
# неразличимы (то же основание, что у правила vacuous-gate в ci_lint.py).
case "$(uname -s)" in
  Linux) echo "release-check: ПРОПУЩЕНО делегирование linux — на линукс-хосте это своя сборка" ;;
  *) assert_linux_delegated "$ROOT/scripts/release.sh" || FAIL=1 ;;
esac
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) echo "release-check: ПРОПУЩЕНО отказ по windows — на windows-хосте это своя сборка" ;;
  *) assert_windows_refused "$ROOT/scripts/release.sh" || FAIL=1 ;;
esac

if [ -z "$LIVE" ]; then
  if [ "$FAIL" = 0 ]; then echo "release-container-check: PASS (правила; живая сборка — с --live)"; fi
  [ "$FAIL" = 0 ] || echo "release-container-check: FAIL" >&2
  exit "$FAIL"
fi

# --- живой этап -------------------------------------------------------------------------------
# Движок здесь обязателен и его отсутствие — ОТКАЗ, а не пропуск: `--live` просят ровно тогда,
# когда хотят пакет, и тихий возврат нуля выглядел бы как собранный Linux-пакет.
ENGINE=$(container_engine) || { container_engine_hint; exit 4; }
VER=${LIKE_NES_RELEASE_VERSION:-v0.0.0-check}
TMP=$(mktemp -d)
OUT1="$ROOT/release"
OUT2="$TMP/second"
trap 'rm -rf "$TMP" "$OUT1/$VER"; rmdir "$OUT1" 2>/dev/null || true' EXIT
BEFORE=$(git -C "$ROOT" status --porcelain)

echo "=== живая сборка в контейнере ($ENGINE), прогон 1: $OUT1"
bash "$ROOT/scripts/release_container.sh" --version "$VER" --out "$OUT1" >/dev/null || {
  bad "первый контейнерный прогон упал"; exit 1; }
echo "=== прогон 2: $OUT2"
bash "$ROOT/scripts/release_container.sh" --version "$VER" --out "$OUT2" >/dev/null || {
  bad "второй контейнерный прогон упал"; exit 1; }

PKG1=$(ls "$OUT1/$VER/like-nes-engine-$VER-linux-"*.tar.gz 2>/dev/null | head -1)
PKG2=$(ls "$OUT2/$VER/like-nes-engine-$VER-linux-"*.tar.gz 2>/dev/null | head -1)
if [ -z "$PKG1" ] || [ -z "$PKG2" ]; then
  bad "linux-пакет не найден после прогона: '$PKG1' / '$PKG2'"; exit 1
fi
TRIPLE=$(basename "$PKG1" .tar.gz); TRIPLE=${TRIPLE#"like-nes-engine-$VER-"}
CACHE=$(container_build_dir "${TRIPLE#linux-}")

UNPACKED="$TMP/unpacked"
mkdir -p "$UNPACKED"
tar -xzf "$PKG1" -C "$UNPACKED" || { bad "архив не распаковывается"; exit 1; }

# Целевая ОС названа Linux ЯВНО: пакет собран под неё, а гейт идёт на macOS, и состав, посчитанный
# по хосту, ждал бы в нём libwgpu_native.dylib.
assert_composition "$ROOT" "$UNPACKED" Linux || FAIL=1
assert_licenses "$ROOT" "$UNPACKED" || FAIL=1
assert_runtime_named "$CACHE/CMakeCache.txt" "$UNPACKED" || FAIL=1
assert_stamp "$UNPACKED" "$VER" "$TRIPLE" || FAIL=1
assert_pkg_matches "$PKG1" "${PKG1%.tar.gz}.manifest" || FAIL=1
assert_pack_normalized "$PKG1" || FAIL=1
assert_same_manifest "${PKG1%.tar.gz}.manifest" "${PKG2%.tar.gz}.manifest" || FAIL=1
SUM1=$(sha256_of "$PKG1")
assert_same_sum "$SUM1" "$(sha256_of "$PKG2")" || FAIL=1
assert_sums_listed "$(dirname "$PKG1")/SHA256SUMS" "$PKG1" "$SUM1" || FAIL=1

rm -rf "${OUT1:?}/$VER"
assert_tree_unchanged "$BEFORE" "$(git -C "$ROOT" status --porcelain)" || FAIL=1

if [ "$FAIL" = 0 ]; then
  echo "release-container-check: PASS (правила и живая сборка)"
else
  echo "release-container-check: FAIL" >&2
fi
exit "$FAIL"
