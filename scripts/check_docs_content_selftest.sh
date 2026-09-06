#!/usr/bin/env bash
# Позитивный контроль утверждений о СОДЕРЖИМОМ документа (спека #19, вертикаль 1): внутренние
# ссылки открываются, лицензионная часть на месте в ОБОИХ README. Отделён от соседнего набора по
# предмету, а не по длине: тот ломает связь двух языков, здесь язык один и ломается сам документ.
# Зовётся ВНЕШНЕЙ командой из check_docs_selftest.sh, чтобы в логе стояло имя упавшего набора.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
export DOCS_FIXTURE_SELF="$ROOT"
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/docs_pairs_lib.sh
. "$ROOT/scripts/docs_pairs_lib.sh"
# shellcheck source=scripts/docs_content_lib.sh
. "$ROOT/scripts/docs_content_lib.sh"
# shellcheck source=scripts/docs_fixture_lib.sh
. "$ROOT/scripts/docs_fixture_lib.sh"

BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'docs-content-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'docs-content-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Незагруженная функция даёт код 127, а он ненулевой — все ожидающие fail кейсы проезжали бы как
# «утверждение отбило порчу», ни разу его не позвав.
# Помощники сюда входят наравне с утверждениями: набор зовёт их сам (docs_normalize в проверке
# нормализатора), а печать docs_ok/docs_bad живёт в СОСЕДНЕЙ библиотеке — забудь её подключить, и
# каждое утверждение падало бы кодом 127, то есть все ожидающие fail кейсы выглядели бы пройденными.
for fn in assert_docs_links_live assert_docs_anchors_live assert_docs_license \
          docs_normalize docs_ok docs_bad docs_slug; do
  declare -F "$fn" >/dev/null || {
    printf 'docs-content-selftest: БРАК утверждение %s не определено\n' "$fn" >&2
    exit 1
  }
done

tree_of() {
  local d="$FIX/$1"
  docs_fixture_tree "$d" >/dev/null || return 1
  printf '%s\n' "$d"
}

case_on() {
  local tag="$1" fn="$2" d
  shift 2
  d=$(tree_of "$tag") || return 1
  "$@" "$d" || return 1
  "$fn" "$d"
}

noop() { :; }

expect pass "исправная фикстура · ссылки живы" case_on ok-links assert_docs_links_live noop
expect pass "исправная фикстура · лицензионная часть" case_on ok-lic assert_docs_license noop

# Якорь на настоящем дереве: фикстура игрушечная, и утверждение, заточенное под неё, выглядело бы
# здоровым до первого прогона гейта.
anchor_real() {
  assert_docs_links_live "$ROOT" && assert_docs_anchors_live "$ROOT" && assert_docs_license "$ROOT"
}
expect pass "настоящее дерево репозитория · все три утверждения" anchor_real

# --- ссылки -------------------------------------------------------------------------------------
break_link() {
  sed -i.bak 's|getting-started/build\.md|getting-started/nope.md|' "$1/docs/en/index.md" \
    && rm -f "$1/docs/en/index.md.bak"
}
expect fail "относительная ссылка ведёт в никуда" case_on l-broken assert_docs_links_live break_link

# Тот же класс, что неверная глубина у переключателя: имя файла в ссылке правильное, сегментов на
# один больше, и открывается она в пустоту.
wrong_depth() {
  sed -i.bak 's|(\.\./index\.md)|(../../index.md)|' "$1/docs/en/getting-started/build.md" \
    && rm -f "$1/docs/en/getting-started/build.md.bak"
}
expect fail "ссылка неверной глубины" case_on l-depth assert_docs_links_live wrong_depth

# Пустое равно пустому: обход без документов не находит ни одной ссылки, нарушений тоже нет — и
# гейт печатал бы «битых нет» про дерево, которого он не читал.
no_docs() { rm -rf "${1:?}/docs"; rm -f "$1/README.md" "$1/README.ru.md"; }
expect fail "документов нет — ссылок не найдено" case_on l-nodocs assert_docs_links_live no_docs

# Опорный: внешний адрес, почтовая схема и внутренний якорь к файловой системе отношения не имеют,
# и фильтр, который их не отбросил, сделал бы гейт вечно красным на первой же ссылке наружу.
external_ok() {
  cat >> "$1/docs/en/index.md" <<'MD'

See [the site](https://example.org/docs), write to [us](mailto:a@example.org),
jump to [a section](#docs) or to [the same page](index.md#docs).
MD
  docs_fixture_stamp "$1" docs/ru/index.md
}
expect pass "внешние адреса и якоря битыми не считаются" case_on l-ext assert_docs_links_live external_ok

# Ссылка, ушедшая выше корня дерева, нормализуется в ПУСТУЮ строку, а `[ -e "$root/" ]` истинно
# всегда: без явного отказа она читалась бы как живая.
above_root() {
  sed -i.bak 's|(getting-started/build\.md)|(../../../..)|' "$1/docs/en/index.md" \
    && rm -f "$1/docs/en/index.md.bak"
  docs_fixture_stamp "$1" docs/ru/index.md
}
expect fail "ссылка уходит выше корня дерева" case_on l-above assert_docs_links_live above_root

# --- якоря --------------------------------------------------------------------------------------
expect pass "исправная фикстура · якоря ведут в заголовки" case_on ok-anchor assert_docs_anchors_live noop

# Заголовок переименован, ссылка осталась прежней — ровно тот случай, ради которого утверждение
# заведено: ссылка открывается, а читатель попадает не туда.
rename_heading() {
  sed -i.bak '1,$s|^# docs$|# documentation|' "$1/docs/en/index.md" && rm -f "$1/docs/en/index.md.bak"
  docs_fixture_stamp "$1" docs/ru/index.md
}
expect fail "заголовок переименован, якорь остался" case_on a-rename assert_docs_anchors_live rename_heading

# Кириллический якорь — своя половина того же утверждения: хостинг не транслитерирует заголовок, и
# slug, написанный под латиницу, оставил бы ru-ветку без проверки вовсе.
rename_heading_ru() {
  sed -i.bak '1,$s|^# документация$|# документы|' "$1/docs/ru/index.md" \
    && rm -f "$1/docs/ru/index.md.bak"
}
expect fail "переименован кириллический заголовок" case_on a-rename-ru assert_docs_anchors_live rename_heading_ru

# `#` в начале строки внутри блока кода — комментарий шелла, а не заголовок. Приняв его за
# заголовок, утверждение объявляло бы живым якорь, которого у документа нет.
fenced_heading() {
  cat >> "$1/docs/en/index.md" <<'MD'

```sh
# fenced
```
MD
  sed -i.bak 's|(getting-started/build\.md)|(getting-started/build.md), [fenced](index.md#fenced)|' \
    "$1/docs/en/index.md" && rm -f "$1/docs/en/index.md.bak"
  docs_fixture_stamp "$1" docs/ru/index.md
}
expect fail "якорь ведёт в строку внутри блока кода" case_on a-fenced assert_docs_anchors_live fenced_heading

# Пустое равно пустому: дерево без единой ссылки с якорем обязано быть отказом, а не «битых нет».
drop_anchors() {
  sed -i.bak 's| and to its \[first section\](\.\./index\.md#docs)||' \
    "$1/docs/en/getting-started/build.md" && rm -f "$1/docs/en/getting-started/build.md.bak"
  sed -i.bak 's| и к его \[первому разделу\](\.\./index\.md#документация)||' \
    "$1/docs/ru/getting-started/build.md" && rm -f "$1/docs/ru/getting-started/build.md.bak"
  docs_fixture_stamp "$1" docs/ru/getting-started/build.md
  ! grep -rq '](\([^)]*#[^)]*\))' "$1/docs" || return 1
}
expect fail "ссылок с якорем в дереве нет" case_on a-none assert_docs_anchors_live drop_anchors

# Разбор slug'а проверяется НАПРЯМУЮ, а не только через дерево: заголовок из одних латинских букв
# ошибку класса пробельных символов не показывает вовсе, и `Prerequisites` → `prerequisi-es`
# оставался бы согласованно зелёным на любой ссылке, повторяющей ту же порчу.
slug_shape() {
  [ "$(docs_slug 'Windows: which shell')" = windows-which-shell ] || return 1
  [ "$(docs_slug 'Prerequisites')" = prerequisites ] || return 1
  [ "$(docs_slug 'Next steps')" = next-steps ] || return 1
  [ "$(docs_slug 'Windows: из какой оболочки')" = windows-из-какой-оболочки ] || return 1
}
expect pass "slug заголовка считается по правилу хостинга" slug_shape

# --- лицензионная часть ---------------------------------------------------------------------------
drop_spdx_en() { sed -i.bak 's/MIT OR Apache-2.0/proprietary/' "$1/README.md" && rm -f "$1/README.md.bak"; }
expect fail "README не называет двойную лицензию" case_on c-spdx assert_docs_license drop_spdx_en

# Перевод, потерявший лицензионную часть, врёт ровно так же, как оригинал: кейс доказывает, что
# проверяются ОБА README, а не только источник.
drop_spdx_ru() { sed -i.bak 's/MIT OR Apache-2.0/проприетарно/' "$1/README.ru.md" && rm -f "$1/README.ru.md.bak"; }
expect fail "перевод README потерял лицензионную часть" case_on c-spdx-ru assert_docs_license drop_spdx_ru

drop_ref() { sed -i.bak 's|(LICENSE-MIT)|LICENSE-MIT|' "$1/README.md" && rm -f "$1/README.md.bak"; }
expect fail "README не ссылается на текст лицензии" case_on c-ref assert_docs_license drop_ref

# Ссылка на месте, файл на нуль байт: читатель, которому назвали лицензию и открыли пустоту,
# получил ровно то же, что и без ссылки.
empty_license() { : > "$1/LICENSE-APACHE"; }
expect fail "текст лицензии пуст" case_on c-empty assert_docs_license empty_license

missing_notice() { rm -f "$1/THIRD-PARTY.md"; }
expect fail "уведомления третьих сторон отсутствуют" case_on c-notice assert_docs_license missing_notice

no_ru_readme() { rm -f "$1/README.ru.md"; }
expect fail "перевода README нет — проверено не в обоих" case_on c-noru assert_docs_license no_ru_readme

if [ "$BAD" != 0 ]; then echo "docs-content-selftest: FAIL" >&2; exit 1; fi
echo "docs-content-selftest: PASS"
