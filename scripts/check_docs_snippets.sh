#!/usr/bin/env bash
# Обёртка гейта врезок: список документов берётся у `docs_all_files` и уходит разбору на стдин.
#
# Своим файлом, а не строкой в трёх местах (checks.json, preflight, шаг CI): написанный врозь, этот
# пайп разъехался бы молча — ровно list-drift, только вместо списка файлов список команд. Читателю
# гейта при этом всё равно, чем он устроен внутри: он зовёт одно имя.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1
# shellcheck source=scripts/docs_content_lib.sh
. "$ROOT/scripts/docs_content_lib.sh"

docs_all_files . | python3 scripts/check_docs_snippets.py .
