#!/usr/bin/env bash
# Гейт пути через CI (спека #20, вертикаль 3). Два этапа, как у контейнерного гейта.
#
# Без флага — ПРАВИЛА: что собирает прогон, чем и что он не публикует, как оркестратор выбирает
# прогон и как отказывает неготовой машине. Сети не требует, поэтому живёт в preflight.sh.
# С --live — настоящий прогон: dispatch, ожидание, скачивание артефакта и сверка цепочки. Он стоит
# до часа раннерского времени и запускается руками.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"
# shellcheck source=scripts/release_ci_lib.sh
. "$ROOT/scripts/release_ci_lib.sh"
# shellcheck source=scripts/release_ci_check_lib.sh
. "$ROOT/scripts/release_ci_check_lib.sh"
# shellcheck source=scripts/release_ci_rules_lib.sh
. "$ROOT/scripts/release_ci_rules_lib.sh"

# shellcheck source=scripts/release_ci_fixture_lib.sh
. "$ROOT/scripts/release_ci_fixture_lib.sh"

WF="$ROOT/.github/workflows/release_engine.yml"
RELEASE="$ROOT/scripts/release.sh"
LIVE=""
# Незнакомый аргумент — ОШИБКА УПОТРЕБЛЕНИЯ, а не «значит, правила»: опечатка `--live` (`--Live`,
# `-live`) тихо давала прогон без сети, печатающий PASS, и читалось это как закрытый живой гейт.
case "${1:-}" in
  --live) LIVE=1 ;;
  "") ;;
  *) echo "употребление: check_release_ci.sh [--live]" >&2; exit 2 ;;
esac

BAD=0
assert_ci_no_second_packer "$WF" || BAD=1
assert_ci_artifact_named "$WF" || BAD=1
assert_ci_dispatchable "$WF" || BAD=1
assert_ci_publishes_nothing "$WF" || BAD=1
assert_windows_delegated "$RELEASE" || BAD=1
assert_gh_hint_split || BAD=1
assert_run_picked_by_commit || BAD=1
assert_verdict_split || BAD=1
assert_picked_parsed || BAD=1

# Утверждения о ПРИЕХАВШЕМ пакете проверяются здесь на фикстуре, а не на настоящем архиве: гейт
# правил не ходит в сеть, и настоящего Windows-пакета у него нет. Фикстура строится тем же
# упаковщиком и тем же штампом, что настоящий пакет, поэтому утверждение остаётся про формат, а не
# про свою же выдумку; на живом архиве те же две функции зовёт --live и сам оркестратор.
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT
SHORT=$(git -C "$ROOT" rev-parse --short HEAD)
# Фикстура строится ОБЩЕЙ фабрикой с самопроверкой (release_ci_fixture_lib.sh) и несёт ПОЛНЫЙ состав
# пакета, а не один version.txt: гейт с урезанной фикстурой не мог звать assert_ci_package вовсе,
# то есть утверждение о составе существовало бы только внутри набора сломанных фикстур.
PKG=$(ci_make_pkg "$ROOT" "$FIX" v0.0.0-check "$SHORT" windows-x86_64) || {
  echo "release-ci-check: фикстурный пакет не собрался" >&2; exit 1
}

assert_download_intact "$FIX" "$PKG" || BAD=1
assert_ci_chain "$PKG" v0.0.0-check "$(git -C "$ROOT" rev-parse HEAD)" || BAD=1
assert_ci_package "$ROOT" "$PKG" v0.0.0-check || BAD=1

if [ -n "$LIVE" ]; then
  echo "--- живой прогон: dispatch, ожидание, скачивание"
  LIVE_OUT="$FIX/out"
  # Оркестратор зовётся ЦЕЛИКОМ и своими же утверждениями проверяет то, что скачал: гейт, который
  # повторил бы проверку сам, проверял бы свою копию, а не то, что делает релиз.
  bash "$ROOT/scripts/release_ci.sh" --dispatch --version v0.0.0-check --out "$LIVE_OUT" || BAD=1
fi

if [ "$BAD" != 0 ]; then echo "release-ci-check: FAIL" >&2; exit 1; fi
if [ -n "$LIVE" ]; then
  echo "release-ci-check: PASS (правила и живой прогон)"
else
  echo "release-ci-check: PASS (правила; живой прогон — bash scripts/check_release_ci.sh --live)"
fi
