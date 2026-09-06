#!/usr/bin/env bash
# Гейт публичной документации (спека #19, вертикаль 1): en — источник, ru — перевод, и расхождение
# между ними есть находка. Утверждения живут в docs_pairs_lib.sh (пара языков) и docs_content_lib.sh
# (содержимое документа); здесь только порядок и вердикт — тот же разрез, что у релизных гейтов.
#
# Гейт НЕ требует сборки и сети, поэтому стоит в checks.json рядом с линтером workflow и бюджетом
# длины: документация ломается правкой текста, а не сборкой, и узнавать об этом на раннере поздно.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/docs_pairs_lib.sh
. "$ROOT/scripts/docs_pairs_lib.sh"
# shellcheck source=scripts/docs_content_lib.sh
. "$ROOT/scripts/docs_content_lib.sh"

BAD=0
# Утверждения зовутся ВСЕ, а не до первой находки: один прогон обязан выдать полный список — то же
# правило, по которому этапы preflight.sh не останавливают друг друга.
assert_docs_mirrored "$ROOT" || BAD=1
assert_docs_translations_fresh "$ROOT" || BAD=1
assert_docs_language_switch "$ROOT" || BAD=1
assert_docs_links_live "$ROOT" || BAD=1
assert_docs_anchors_live "$ROOT" || BAD=1
assert_docs_license "$ROOT" || BAD=1

if [ "$BAD" != 0 ]; then echo "docs-check: FAIL" >&2; exit 1; fi
echo "docs-check: PASS"
