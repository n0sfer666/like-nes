#!/usr/bin/env bash
# Позитивный контроль ПОДМЕНЁННЫМИ РЕАЛИЗАЦИЯМИ (спека #20, вертикаль 3). Сломанные ФАЙЛЫ пути
# ломает соседний check_release_ci_selftest.sh; здесь ломается механика библиотеки — выбор прогона,
# вердикт, подсказки и разбор приехавшего пакета. Граница по предмету, та же, что делит
# check_release_container_selftest.sh и check_release_container_impl_selftest.sh.
#
# Набор, который сломанную реализацию ПРОПУСКАЕТ, выглядит ровно как честный: он печатает те же
# строки OK. Поэтому проверяется не «утверждение прошло», а «утверждение умеет падать».
set -uo pipefail

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
# shellcheck source=scripts/release_msi_ci_lib.sh
. "$ROOT/scripts/release_msi_ci_lib.sh"
# shellcheck source=scripts/release_ci_fixture_lib.sh
. "$ROOT/scripts/release_ci_fixture_lib.sh"

BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'ci-impl-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'ci-impl-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Подмена живёт в САБШЕЛЛЕ: наружу от неё нужен только код возврата, а библиотека обязана остаться
# нетронутой для следующего утверждения. Обратная сборка «подменил — восстановил» тем и плоха, что
# забытое восстановление делает все последующие проверки проверками подмены.
sub() {
  local want="$1" name="$2" body="$3" call="$4"
  local rc=0
  ( eval "$body"; eval "$call" ) >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'ci-impl-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'ci-impl-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# --- опора: нетронутые реализации ---------------------------------------------------------------
expect pass "нетронутая механика · выбор прогона" assert_run_picked_by_commit
expect pass "нетронутая механика · вердикт" assert_verdict_split
expect pass "нетронутая механика · подсказки" assert_gh_hint_split
expect pass "нетронутая механика · разбор выбора" assert_picked_parsed

# --- подмены механики ---------------------------------------------------------------------------
sub fail "выбор берёт первый попавшийся, а не свежайший" \
  'ci_pick_run() { python3 -c "
import json,sys
runs=json.load(open(sys.argv[1]))
mine=[r for r in runs if r.get(\"headSha\")==sys.argv[2]]
if not mine: sys.exit(1)
r=mine[0]
print(r[\"databaseId\"], r[\"status\"], r.get(\"conclusion\",\"\"))
" "$1" "$2"; }' \
  assert_run_picked_by_commit

sub fail "выбор игнорирует коммит" \
  'ci_pick_run() { python3 -c "
import json,sys
runs=json.load(open(sys.argv[1]))
r=sorted(runs,key=lambda r:r.get(\"createdAt\",\"\"))[-1]
print(r[\"databaseId\"], r[\"status\"], r.get(\"conclusion\",\"\"))
" "$1" "$2"; }' \
  assert_run_picked_by_commit

sub fail "вердикт всегда зелёный" \
  'ci_run_verdict() { printf "ok\n"; }' assert_verdict_split

sub fail "красный слипся с незавершённым" \
  'ci_run_verdict() { if [ "$1" = completed ] && [ "$2" = success ]; then printf "ok\n"; else printf "pending\n"; fi; }' \
  assert_verdict_split

sub fail "одна подсказка на обе причины" \
  'ci_gh_hint() { printf "release: gh на машине нет или не авторизован\n" >&2; }' \
  assert_gh_hint_split

# Подмена воспроизводит НАСТОЯЩИЙ дефект главного пути, а не гипотезу: разбор позиционными
# параметрами. У идущего прогона `conclusion` пуст, параметров выходит два, и `"$3"` под `set -u`
# убивает оркестратор — ветка «прогон ещё идёт» не достигалась никогда, а увидеть это мог только
# `--live`.
sub fail "разбор выбора позиционными параметрами" \
  'ci_picked_verdict() { set -- $1; printf "%s %s\n" "$1" "$(ci_run_verdict "$2" "$3")"; }' \
  assert_picked_parsed

# Пустая строка выбора обязана быть отказом: у `gh run list`, вернувшего ничего, разбор давал бы
# пустой id, а `gh run download ""` — отказ, неотличимый от красной сборки.
sub fail "пустая строка выбора сходит за прогон" \
  'ci_picked_verdict() { printf "%s pending\n" "${1%% *}"; }' \
  assert_picked_parsed

# Утверждение про имя артефакта проверяется подменой ЧТЕНИЯ, а не файлом: файл ломает соседний
# набор, а здесь ломается ровно тот молчаливый провал, из-за которого `gh run download -n ''`
# скачивает все артефакты прогона и отказ выглядит как успех.
sub fail "чтение имени артефакта молча даёт пустое" \
  'ci_artifact_name() { printf "\n"; }' \
  'assert_ci_artifact_named "$ROOT/.github/workflows/release_engine.yml"'

# --- фикстуры приехавшего пакета ----------------------------------------------------------------
# Фабрика — общая с гейтом правил (release_ci_fixture_lib.sh), и это не экономия строк: собери гейт
# честный пакет своей копией, а набор — сломанный своей, и «утверждение отбило подмену» проверялось
# бы на пакете иного устройства, чем тот, на котором утверждение проходит. Здесь только тонкая
# обёртка, дающая фикстурам этого набора общую тройку.
make_pkg() {
  ci_make_pkg "$ROOT" "$FIX/$1" "$2" "$3" windows-x86_64 "${4:-}" "${5:-windows-x86_64}"
}

SHA=$(git -C "$ROOT" rev-parse HEAD)
SHORT=$(git -C "$ROOT" rev-parse --short HEAD)

GOOD=$(make_pkg good v0.0.0-check "$SHORT")
expect pass "честный пакет · доставка" assert_download_intact "$FIX/good" "$GOOD"
expect pass "честный пакет · цепочка" assert_ci_chain "$GOOD" v0.0.0-check "$SHA"

# Промах фикстуры обязан валить набор: `make_pkg`, отработавший впустую, оставил бы все подмены
# ниже «отбитыми» за отсутствием предмета — ровно тот случай, когда контейнерный набор напечатал
# PASS, не создав ни одной копии.
if [ ! -s "$GOOD" ]; then
  echo "ci-impl-selftest: БРАК фикстурный пакет не собрался — подмены ниже отбились бы впустую" >&2
  exit 1
fi

WRONGSUM=$(make_pkg wrongsum v0.0.0-check "$SHORT")
printf '%s  %s\n' "0000000000000000000000000000000000000000000000000000000000000000" \
  "$(basename "$WRONGSUM")" > "$FIX/wrongsum/SHA256SUMS"
expect fail "сумма в SHA256SUMS чужая" assert_download_intact "$FIX/wrongsum" "$WRONGSUM"

NOSUMS=$(make_pkg nosums v0.0.0-check "$SHORT")
rm -f "$FIX/nosums/SHA256SUMS"
expect fail "в артефакте нет SHA256SUMS" assert_download_intact "$FIX/nosums" "$NOSUMS"

expect fail "пакета нет вовсе" assert_download_intact "$FIX/good" "$FIX/good/нет-такого.tar.gz"

OTHERSHA=$(make_pkg othersha v0.0.0-check deadbee)
expect fail "штамп называет чужой коммит" assert_ci_chain "$OTHERSHA" v0.0.0-check "$SHA"

OTHERVER=$(make_pkg otherver v9.9.9 "$SHORT")
expect fail "штамп называет чужую версию" assert_ci_chain "$OTHERVER" v0.0.0-check "$SHA"

NOSTAMP=$(make_pkg nostamp v0.0.0-check -)
expect fail "в пакете нет version.txt" assert_ci_chain "$NOSTAMP" v0.0.0-check "$SHA"

# Сравнение коммита идёт ПРЕФИКСОМ (штамп несёт короткий хеш), и это ровно то место, где легко
# получить утверждение, принимающее любую строку: пустой короткий хеш — префикс чего угодно.
EMPTYSHA=$(make_pkg emptysha v0.0.0-check "")
expect fail "штамп не называет коммит" assert_ci_chain "$EMPTYSHA" v0.0.0-check "$SHA"

# --- утверждения о СОСТАВЕ приехавшего пакета ----------------------------------------------------
# Цепочка (версия + коммит) о составе не говорит ничего: пакет без editor_shell.exe, без рантайма
# или с лицензией в нуль байт сходился по обеим строкам штампа и доезжал как успех. Прогон CI тоже
# ничего не утверждает — там только release.sh и upload.
LIC=$(head -1 "$ROOT/cmake/licenses.manifest")
LICF="like-nes/licenses/$(basename "$LIC")"
expect pass "честный пакет · состав, лицензии, штамп" assert_ci_package "$ROOT" "$GOOD" v0.0.0-check

NOLIC=$(make_pkg nolic v0.0.0-check "$SHORT" "drop:$LICF")
expect fail "в пакете не хватает лицензии" assert_ci_package "$ROOT" "$NOLIC" v0.0.0-check

ZEROLIC=$(make_pkg zerolic v0.0.0-check "$SHORT" "zero:$LICF")
expect fail "лицензия на нуль байт" assert_ci_package "$ROOT" "$ZEROLIC" v0.0.0-check

NORUNTIME=$(make_pkg noruntime v0.0.0-check "$SHORT" "drop:like-nes/bin/wgpu_native.dll")
expect fail "в пакете нет рантайма wgpu" assert_ci_package "$ROOT" "$NORUNTIME" v0.0.0-check

# Тройка в ШТАМПЕ против тройки в ИМЕНИ: пакет, собранный не на Windows, сходится по версии и
# коммиту — их считает не платформа. Расходится он ровно здесь.
OTHERTRIPLE=$(make_pkg othertriple v0.0.0-check "$SHORT" "" linux-x86_64)
expect fail "штамп называет чужую тройку" assert_ci_package "$ROOT" "$OTHERTRIPLE" v0.0.0-check

# Зависимость от VC++ Redistributable (вертикаль 5) — та же цепочка её не видит: пакет, который на
# чистой Windows не стартует, сходится по версии, коммиту, составу и лицензиям. Ветка внутри
# assert_ci_package без этих трёх фикстур зелена и с вырезанными строками — то есть неотличима от
# отсутствующей.
CRTMISS=$(make_pkg crtmiss v0.0.0-check "$SHORT" "crt:like-nes/bin/wgpu_native.dll")
expect fail "рантайм просит DLL, которой в пакете нет" assert_ci_package "$ROOT" "$CRTMISS" v0.0.0-check

# Пропажа самой vcruntime140.dll сюда НЕ дописана: её ловит assert_composition (файл назван в
# expected_files), то есть фикстура падала бы и с вырезанной CRT-веткой — «утверждение отбило
# подмену» вышло бы из соседнего утверждения. Обе оставшиеся падают ровно на этой ветке: проверено
# прогоном с вырезанными строками.

CRTDYN=$(make_pkg crtdyn v0.0.0-check "$SHORT" "dyncrt:like-nes/bin/editor_shell.exe")
expect fail "наш exe приехал с динамическим CRT" assert_ci_package "$ROOT" "$CRTDYN" v0.0.0-check

# Обратная половина той же ветки: на пакете НЕ для Windows утверждения о CRT неприменимы (PE-файлов
# нет вовсе, счётчик осмотренных нулевой), и пропуск обязан быть ВСЛУХ — молчаливый ноль здесь
# неотличим от пройденных утверждений.
case_ci_pkg_linux() {
  local pkg out
  pkg=$(ci_make_pkg "$ROOT" "$FIX/lin" v0.0.0-check "$SHORT" linux-x86_64 "" linux-x86_64) || return 1
  out=$(assert_ci_package "$ROOT" "$pkg" v0.0.0-check) || return 1
  printf '%s' "$out" | grep -q 'ПРОПУСК' || return 1
}
expect pass "пакет не для Windows · о CRT сказано ВСЛУХ" case_ci_pkg_linux

# Про сам гейт: имя assert_*, которое он зовёт, обязано быть определено. В контейнерном наборе это
# утверждение поймало настоящий дефект (`assert_pack_normalized: command not found` в ветке --live)
# до всякой сборки — ветка правил его не касалась вовсе.
for fn in $(grep -oE '\bassert_[a-z_]+' "$ROOT/scripts/check_release_ci.sh" | sort -u); do
  if ! declare -F "$fn" >/dev/null; then
    printf 'ci-impl-selftest: БРАК гейт зовёт %s, а такой функции нет\n' "$fn" >&2
    BAD=1
  fi
done

if [ "$BAD" != 0 ]; then echo "ci-impl-selftest: FAIL" >&2; exit 1; fi
echo "ci-impl-selftest: PASS"
