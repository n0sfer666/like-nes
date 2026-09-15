#!/usr/bin/env bash
# Гейт 5 спеки #19: чистая машина проходит getting-started ДОСЛОВНО и получает то, что документ
# обещает. Расхождение текста с реальностью — находка, а не повод рассуждать.
#
#   bash scripts/check_docs_start.sh          # правила: ни демона, ни сети, ни четверти часа
#   bash scripts/check_docs_start.sh --live   # прогон в контейнере голой базы
#
# Механическая половина гейта — здесь; субъективная («редактор открыт, игра играется») и обе прочие
# ОС остаются владельцу: правило 18 глобального CLAUDE.md, docs/owner-verification.md.
#
# Свой Dockerfile здесь НЕ заводится: база берётся из релизного (release_linux.Dockerfile) той же
# container_base_pin, что и в вертикали 2 спеки #20. Вторая копия пина разъехалась бы с первой
# молча — ровно mirrors-group из ci_lint.py. И берётся именно ГОЛАЯ база, а не релизный образ: тот
# ставит пакеты сам, и прогон на нём был бы зелёным при документе, забывшем половину списка.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
. scripts/docs_start_lib.sh
# shellcheck source=scripts/docs_start_markup_rules_lib.sh
. scripts/docs_start_markup_rules_lib.sh
. scripts/docs_start_stub_lib.sh
. scripts/release_container_lib.sh
. scripts/docs_start_doc_rules_lib.sh
. scripts/docs_start_rules_lib.sh
# shellcheck source=scripts/docs_start_awk_lib.sh
. scripts/docs_start_awk_lib.sh

LIVE=0
case "${1-}" in
  --live) LIVE=1 ;;
  "") ;;
  *) echo "usage: $0 [--live]" >&2; exit 2 ;;
esac

rc=0
assert_start_marked "$ROOT" || rc=1
assert_start_langs_agree "$ROOT" || rc=1
assert_start_expect_in_text "$ROOT" || rc=1
assert_start_clone_url "$ROOT" || rc=1
assert_start_binaries_named "$ROOT" || rc=1
assert_start_check_expected "$ROOT" || rc=1
assert_start_bare_base "$ROOT" || rc=1
assert_start_reads_this_tree "$ROOT" || rc=1
assert_start_awk_portable "$ROOT" || rc=1
assert_start_stub_runs || rc=1

if [ "$LIVE" = 0 ]; then
  [ "$rc" = 0 ] || { echo "check-docs-start: FAIL правила разметки" >&2; exit 1; }
  echo "check-docs-start: PASS правила (прогон на чистой машине — --live)"
  exit 0
fi
[ "$rc" = 0 ] || { echo "check-docs-start: FAIL правила разметки, прогон не запускался" >&2; exit 1; }

# Движок выбирается ОТВЕТОМ на `info`, а не наличием в PATH: установленный Docker Desktop с
# погашенным демоном иначе выглядел бы годным и падал бы посреди прогона.
ENGINE=$(container_engine) || {
  container_engine_hint >&2
  exit 4
}
BASE=$(container_base_pin "$ROOT/scripts/release_linux.Dockerfile")
echo "check-docs-start: голая база $BASE, движок $ENGINE"

"$ENGINE" run --rm \
  -v "$ROOT:/src:ro" \
  -w /work \
  "$BASE" \
  bash -c 'mkdir -p /work && bash /src/scripts/docs_start_run.sh /src /work'
echo "check-docs-start: PASS чистая машина прошла getting-started"
