#!/usr/bin/env bash
# Позитивный контроль ПОДМЕНАМИ РЕАЛИЗАЦИЙ (спека #20, вертикаль 2). Отдельный файл, а не хвост
# `check_release_container_selftest.sh`: граница здесь по предмету, ровно как у пары
# `check_release_selftest.sh` / `check_release_pack_selftest.sh`. Тот набор ломает ФАЙЛЫ дерева —
# копию скрипта с вырезанной мерой, и это про правку кода. Этот ломает саму механику библиотеки,
# и это про правило, которое умеет проходить только на своей же реализации. В логе стоит имя
# упавшего набора, а не «что-то из двадцати восьми».
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"
# shellcheck source=scripts/release_container_lib.sh
. "$ROOT/scripts/release_container_lib.sh"
# shellcheck source=scripts/release_container_check_lib.sh
. "$ROOT/scripts/release_container_check_lib.sh"

DOCKERFILE="$ROOT/scripts/release_linux.Dockerfile"
BAD=0

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'container-impl-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'container-impl-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Опорный прогон: на НАСТОЯЩИХ реализациях всё обязано проходить. Иначе «подмена отбита» выходило
# бы из утверждения, которое падает всегда.
expect pass "живые реализации · тег по архитектуре" assert_tag_per_arch "$DOCKERFILE"
expect pass "живые реализации · живость движка" assert_engine_liveness
expect pass "живые реализации · сломанный кеш" assert_broken_cache_detected
expect pass "живые реализации · кеш вне дерева" assert_cache_outside_tree "$ROOT"

# --- подменные реализации ---------------------------------------------------------------------
# Правило, проходящее только на своей реализации, — не правило: подмена здесь отличается от
# настоящей РОВНО снятой мерой, и утверждение обязано это увидеть.
container_image_tag() { printf 'like-nes-release:one\n'; }
expect fail "тег один на две архитектуры" assert_tag_per_arch "$DOCKERFILE"
. "$ROOT/scripts/release_container_lib.sh"

container_engine() { command -v "${LIKE_NES_CONTAINER_ENGINE:-docker}" >/dev/null 2>&1 && printf 'docker\n'; }
expect fail "движок выбран по наличию в PATH" assert_engine_liveness
. "$ROOT/scripts/release_container_lib.sh"

container_cache_broken() { return 0; }
expect fail "сломанным считается любой каталог" assert_broken_cache_detected
. "$ROOT/scripts/release_container_lib.sh"

container_cache_broken() { return 1; }
expect fail "сломанного каталога не бывает" assert_broken_cache_detected
. "$ROOT/scripts/release_container_lib.sh"

container_build_dir() { printf '%s/build-container/%s\n' "$ROOT" "$1"; }
expect fail "кеш внутри дерева" assert_cache_outside_tree "$ROOT"
. "$ROOT/scripts/release_container_lib.sh"

if [ "$BAD" = 0 ]; then
  echo "container-impl-selftest: PASS"
else
  echo "container-impl-selftest: FAIL" >&2
fi
exit "$BAD"
