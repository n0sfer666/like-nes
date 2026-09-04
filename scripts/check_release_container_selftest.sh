#!/usr/bin/env bash
# Позитивный контроль гейта контейнерного пути (спека #20, вертикаль 2): каждое утверждение
# обязано УПАСТЬ на копии, сломанной ровно под него. Без этого набор неотличим от функций, всегда
# возвращающих ноль, — и узнаётся это в день, когда пакет Linux уедет собранным не той базой.
#
# Ломаются здесь ДВЕ разные вещи: файлы дерева (копия скрипта с вырезанной мерой) и реализации
# функций (подмена поверх загруженной библиотеки). Первое ловит правку кода, второе — правило,
# которое умеет проходить только на своей же реализации.
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

TMP=$(mktemp -d)
DOCKERFILE="$ROOT/scripts/release_linux.Dockerfile"
BAD=0
# Копии скриптов лежат В scripts/, а не в /tmp: и release.sh, и release_container.sh считают корень
# дерева от своего расположения и оттуда же берут библиотеки. Копия в /tmp падала бы на первом
# `source` — то есть «отбита» выходило бы из ненайденного файла, а не из вырезанной меры.
# Уборка идёт ПО ШАБЛОНУ, а не по списку, накопленному копированием: сами копии заводятся в
# главном шелле, но первая версия звала копирование из `$( … )`, то есть из сабшелла, и список
# оставался пуст — десять `.selftest_*.sh` пережили прогон и всплыли в `git status`. Шаблон не
# зависит от того, кто и откуда завёл файл, и переживает прерванный прогон.
trap 'rm -rf "$TMP"; rm -f "$ROOT"/scripts/.selftest_*' EXIT

# Путь копии считается отдельно от её создания, потому что создание обязано уметь ВАЛИТЬ набор.
# Через `X=$(copy_script …)` оно этого не могло: подстановка команд — сабшелл, и ни `exit`, ни
# присваивание флага оттуда до набора не доезжают, а неудачное копирование выглядело бы как
# «подмена отбита» — за отсутствием предмета.
copy_path() {
  printf '%s\n' "$ROOT/scripts/.selftest_$1"
}

# Раздельные `local`, а не одна строка с `local src="$1" dst="…$2"`: bash 3.2, который стоит на
# macOS штатно, объявляет ВСЕ имена строки разом и лишь потом присваивает, поэтому ссылка на
# соседнее имя под `set -u` умирает как unbound. Первая версия этого файла именно так и не
# создавала ни одной копии — а набор при этом печатал PASS, отбивая подмены за отсутствием
# предмета. На bash 5 (линукс-раннер) та же строка работала бы и находку спрятала.
copy_script() {
  local src="$1"
  local name="$2"
  local dst
  dst=$(copy_path "$name")
  cp "$src" "$dst" || { printf 'container-selftest: БРАК копия %s не создана\n' "$name" >&2; exit 1; }
  [ -s "$dst" ] || { printf 'container-selftest: БРАК копия %s пуста\n' "$name" >&2; exit 1; }
}

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'container-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'container-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Само копирование — тоже мера, и она уже ломалась дважды за один раунд (`local` на bash 3.2,
# затем `exit` из сабшелла). Поэтому набор утверждает, что промах копирования ВАЛИТ его: иначе
# «подмена отбита» снова начнёт выходить из ненайденного файла.
if ( copy_script "$ROOT/scripts/нет-такого-файла.sh" probe.sh ) >/dev/null 2>&1; then
  printf 'container-selftest: БРАК промах копирования не свалил набор\n' >&2
  BAD=1
else
  printf 'container-selftest: OK   промах копирования валит набор (fail)\n'
fi

# Каждое имя, которое гейт зовёт, обязано быть ОПРЕДЕЛЕНО. Ветка `--live` поднимает docker и в
# preflight не идёт, поэтому опечатка или незагруженная библиотека в ней доживают до дня релиза:
# именно так `assert_pack_normalized` и `assert_tree_unchanged` (они в release_check_hygiene.sh,
# который гейт не подключал) свалились как `command not found` на первом живом прогоне, а этап
# правил их не касался вовсе.
MISSING=0
for fn in $(grep -oE '\bassert_[a-z_]+' "$ROOT/scripts/check_release_container.sh" | sort -u); do
  if ! type -t "$fn" >/dev/null 2>&1; then
    printf 'container-selftest: БРАК гейт зовёт %s, а такой функции нет\n' "$fn" >&2
    MISSING=1
  fi
done
if [ "$MISSING" = 0 ]; then
  printf 'container-selftest: OK   все утверждения гейта определены (pass)\n'
else
  BAD=1
fi

# Опорный прогон: живое дерево обязано пройти ВСЁ. Иначе «падает всегда» читалось бы как «ловит
# всё», и ни одна находка ниже ничего не значила бы.
expect pass "живое дерево · пин базы" assert_base_pinned "$DOCKERFILE"
expect pass "живое дерево · нет второго упаковщика" assert_no_second_packer "$ROOT/scripts/release_container.sh"
expect pass "живое дерево · монтирование ro" assert_ro_mount "$ROOT/scripts/release_container.sh"
expect pass "живое дерево · тег по архитектуре" assert_tag_per_arch "$DOCKERFILE"
expect pass "живое дерево · живость движка" assert_engine_liveness
expect pass "живое дерево · сломанный кеш" assert_broken_cache_detected
expect pass "живое дерево · кеш вне дерева" assert_cache_outside_tree "$ROOT"

# --- сломанные файлы --------------------------------------------------------------------------
# Нетронутая копия обязана ПРОХОДИТЬ каждое утверждение, которое ниже валит её сломанный близнец.
# Без этой пары «подмена отбита» неотличимо от «файла нет»: ровно так первая версия набора и
# печатала PASS, не создав ни одной копии.
copy_script "$ROOT/scripts/release_container.sh" intact_container.sh
INTACT_C=$(copy_path intact_container.sh)
expect pass "нетронутая копия · нет второго упаковщика" assert_no_second_packer "$INTACT_C"
expect pass "нетронутая копия · монтирование ro" assert_ro_mount "$INTACT_C"
expect pass "нетронутая копия · отказ без движка" assert_refusal_without_engine "$INTACT_C"
printf 'FROM ubuntu:24.04\n' > "$TMP/tagged.Dockerfile"
expect fail "база по тегу" assert_base_pinned "$TMP/tagged.Dockerfile"
printf 'FROM ubuntu@sha256:33ceb719\n' > "$TMP/short.Dockerfile"
expect fail "дайджест обрезан" assert_base_pinned "$TMP/short.Dockerfile"
: > "$TMP/empty.Dockerfile"
expect fail "нет строки FROM" assert_base_pinned "$TMP/empty.Dockerfile"

copy_script "$ROOT/scripts/release_container.sh" packer.sh
PACKER=$(copy_path packer.sh)
printf 'tar -czf /out/pkg.tar.gz .\n' >> "$PACKER"
expect fail "контейнер пакует сам" assert_no_second_packer "$PACKER"

copy_script "$ROOT/scripts/release_container.sh" nodeleg.sh
NODELEG=$(copy_path nodeleg.sh)
sed 's|release\.sh|nothing|g' "$NODELEG" > "$NODELEG.tmp" && mv "$NODELEG.tmp" "$NODELEG"
expect fail "делегирования не видно" assert_no_second_packer "$NODELEG"

copy_script "$ROOT/scripts/release_container.sh" rw.sh
RW=$(copy_path rw.sh)
sed 's|-v "\$ROOT:/src:ro"|-v "$ROOT:/src"|' "$RW" > "$RW.tmp" && mv "$RW.tmp" "$RW"
expect fail "дерево смонтировано на запись" assert_ro_mount "$RW"

copy_script "$ROOT/scripts/release_container.sh" nomount.sh
NOMOUNT=$(copy_path nomount.sh)
sed 's|-v "\$ROOT:/src:ro"||' "$NOMOUNT" > "$NOMOUNT.tmp" && mv "$NOMOUNT.tmp" "$NOMOUNT"
expect fail "дерево не смонтировано вовсе" assert_ro_mount "$NOMOUNT"

# Отказ без движка: код 4 и инструкция — два РАЗНЫХ утверждения, поэтому и ломаются порознь.
copy_script "$ROOT/scripts/release_container.sh" code1.sh
CODE1=$(copy_path code1.sh)
sed 's|exit 4|exit 1|' "$CODE1" > "$CODE1.tmp" && mv "$CODE1.tmp" "$CODE1"
expect fail "отказ без движка чужим кодом" assert_refusal_without_engine "$CODE1"

copy_script "$ROOT/scripts/release_container.sh" mute.sh
MUTE=$(copy_path mute.sh)
sed 's|container_engine_hint; exit 4|exit 4|' "$MUTE" > "$MUTE.tmp" && mv "$MUTE.tmp" "$MUTE"
expect fail "отказ без движка молча" assert_refusal_without_engine "$MUTE"

# Соседние наборы — ВНЕШНИЕ команды, и граница между тремя проведена по предмету, как у пары
# `check_release_selftest.sh` / `check_release_pack_selftest.sh`. Здесь ломаются файлы
# контейнерного пути, в `impl` — механика библиотеки, в `refusal` — ветвление release.sh на
# диспатче. В логе стоит имя упавшего набора, а не «что-то из трёх разошлось».
bash "$ROOT/scripts/check_release_container_impl_selftest.sh" || BAD=1
bash "$ROOT/scripts/check_release_refusal_selftest.sh" || BAD=1

if [ "$BAD" = 0 ]; then
  echo "container-selftest: PASS"
else
  echo "container-selftest: FAIL" >&2
fi
exit "$BAD"
