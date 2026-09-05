#!/usr/bin/env bash
# Позитивный контроль СЛОМАННЫМИ ФАЙЛАМИ (спека #20, вертикаль 3): копия workflow или release.sh с
# вырезанной мерой обязана валить то утверждение, которое эту меру стережёт. Утверждение, у
# которого нет фикстуры, где оно падает, неотличимо от отсутствующего.
#
# Подмены самих РЕАЛИЗАЦИЙ живут в check_release_ci_impl_selftest.sh — граница по предмету, та же,
# что делит контейнерную пару наборов; он зовётся отсюда внешней командой, чтобы в логе стояло имя
# упавшего набора.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"
# shellcheck source=scripts/release_ci_lib.sh
. "$ROOT/scripts/release_ci_lib.sh"
# shellcheck source=scripts/release_ci_rules_lib.sh
. "$ROOT/scripts/release_ci_rules_lib.sh"
# shellcheck source=scripts/release_msi_ci_lib.sh
. "$ROOT/scripts/release_msi_ci_lib.sh"

WF="$ROOT/.github/workflows/release_engine.yml"
BAD=0
# Уборка идёт ШАБЛОНОМ, а не списком: список, накопленный внутри `$( … )`, живёт в сабшелле и до
# trap не доезжает — так десять копий пережили прогон контейнерного набора и всплыли в git status.
trap 'rm -f "$ROOT"/scripts/.selftest_ci_* "$ROOT"/.github/workflows/.selftest_ci_*' EXIT

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'ci-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'ci-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Раздельные `local`: на bash 3.2 (штатном для macOS) все имена одной строки объявляются разом и
# лишь потом присваиваются, поэтому ссылка на соседнее имя под `set -u` умирает как unbound. Так
# контейнерный набор однажды напечатал PASS, не создав ни одной копии.
copy_wf() {
  local src="$1"
  local name="$2"
  local dst="$ROOT/.github/workflows/.selftest_ci_$name"
  cp "$src" "$dst"
  printf '%s\n' "$dst"
}
copy_release() {
  local name="$1"
  local dst="$ROOT/scripts/.selftest_ci_$name"
  cp "$ROOT/scripts/release.sh" "$dst"
  printf '%s\n' "$dst"
}

# Подмена, которая ничего не подменила, читается ровно как «утверждение её отбило»: копия остаётся
# исправной и честно проходит. Так уже вышло в соседнем refusal-наборе — sed промахнулся мимо
# строки, уехавшей в этой же вертикали, и фикстура полгейта проверяла собственный промах. Сравнение
# с оригиналом отбирает у промаха право быть вердиктом.
mutated() {
  local dst="$1"
  local src="$2"
  if cmp -s "$src" "$dst"; then
    printf 'ci-selftest: БРАК подмена %s ничего не изменила\n' "$(basename "$dst")" >&2
    BAD=1
  fi
}

# Опорный прогон: на НЕТРОНУТЫХ копиях всё обязано проходить. Без него «фикстура отбита» выходило
# бы из утверждения, которое падает на чём угодно, включая исправный файл.
INTACT_WF=$(copy_wf "$WF" intact.yml)
expect pass "нетронутая копия · упаковщик" assert_ci_no_second_packer "$INTACT_WF"
expect pass "нетронутая копия · имя артефакта" assert_ci_artifact_named "$INTACT_WF"
expect pass "нетронутая копия · запуск руками" assert_ci_dispatchable "$INTACT_WF"
expect pass "нетронутая копия · ничего не публикует" assert_ci_publishes_nothing "$INTACT_WF"
expect pass "нетронутая копия · WiX пиннут" assert_ci_wix_pinned "$INTACT_WF"
INTACT_R=$(copy_release intact.sh)
expect pass "нетронутая копия · windows делегируется" assert_windows_delegated "$INTACT_R"

# Промах копирования обязан валить набор: `sed`, отработавший по пустому пути, оставляет фикстуру
# пустой, и все подмены «отбиваются» из-за ненайденного файла, а не из-за дефекта.
if [ ! -s "$INTACT_WF" ] || [ ! -s "$INTACT_R" ]; then
  echo "ci-selftest: БРАК копии не создались — все подмены ниже отбились бы за отсутствием предмета" >&2
  exit 1
fi

# --- сломанные файлы --------------------------------------------------------------------------
NOCALL=$(copy_wf "$WF" nocall.yml)
sed -i.bak 's|bash scripts/release\.sh|bash scripts/nothing.sh|' "$NOCALL" && rm -f "$NOCALL.bak"
mutated "$NOCALL" "$WF"
expect fail "workflow не зовёт release.sh" assert_ci_no_second_packer "$NOCALL"

OWNPACK=$(copy_wf "$WF" ownpack.yml)
printf '      - name: own tar\n        run: tar -czf pkg.tar.gz stage\n' >> "$OWNPACK"
mutated "$OWNPACK" "$WF"
expect fail "workflow пакует сам" assert_ci_no_second_packer "$OWNPACK"

NOART=$(copy_wf "$WF" noart.yml)
sed -i.bak 's|uses: actions/upload-artifact.*|uses: actions/nothing@v0|' "$NOART" && rm -f "$NOART.bak"
mutated "$NOART" "$WF"
expect fail "нет шага upload-artifact" assert_ci_artifact_named "$NOART"

NODISP=$(copy_wf "$WF" nodisp.yml)
sed -i.bak 's|^  workflow_dispatch:|  # workflow_dispatch:|' "$NODISP" && rm -f "$NODISP.bak"
mutated "$NODISP" "$WF"
expect fail "прогон не запускается руками" assert_ci_dispatchable "$NODISP"

NOVER=$(copy_wf "$WF" nover.yml)
sed -i.bak 's|^      version:|      other:|' "$NOVER" && rm -f "$NOVER.bak"
mutated "$NOVER" "$WF"
expect fail "у ручного запуска нет входа version" assert_ci_dispatchable "$NOVER"

WRITES=$(copy_wf "$WF" writes.yml)
sed -i.bak 's|^  contents: read|  contents: write|' "$WRITES" && rm -f "$WRITES.bak"
mutated "$WRITES" "$WF"
expect fail "прогон получает права на запись" assert_ci_publishes_nothing "$WRITES"

PUBLISH=$(copy_wf "$WF" publish.yml)
printf '      - name: publish\n        run: gh release create v0.0.0 pkg\n' >> "$PUBLISH"
mutated "$PUBLISH" "$WF"
expect fail "прогон публикует релиз" assert_ci_publishes_nothing "$PUBLISH"

NOPERM=$(copy_wf "$WF" noperm.yml)
sed -i.bak 's|^permissions:|# permissions:|' "$NOPERM" && rm -f "$NOPERM.bak"
mutated "$NOPERM" "$WF"
expect fail "прогон не объявляет permissions" assert_ci_publishes_nothing "$NOPERM"

# `contents: read` наверху и `contents: write` в job'е: обе строки на месте, и утверждение,
# смотревшее на присутствие ограничения, было зелено — а permissions job'а верхние ПЕРЕКРЫВАЮТ.
JOBWRITE=$(copy_wf "$WF" jobwrite.yml)
printf '    permissions:\n      contents: write\n' >> "$JOBWRITE"
mutated "$JOBWRITE" "$WF"
expect fail "job просит права на запись поверх верхних" assert_ci_publishes_nothing "$JOBWRITE"

# Ровно та фикстура, что воспроизводит закрытую вертикаль: windows снова отказывает кодом 3.
REFUSED=$(copy_release refused.sh)
python3 - "$REFUSED" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
head = '    windows)\n'
i = s.index(head)
j = s.index('      ;;\n', i) + len('      ;;\n')
open(p, 'w').write(s[:i] + '    windows) echo "release: пакет Windows собирается задачей CI" >&2 ;;\n' + s[j:])
PY
mutated "$REFUSED" "$ROOT/scripts/release.sh"
expect fail "windows снова отказывает кодом 3" assert_windows_delegated "$REFUSED"

# Компилятор установщика — второй упаковщик ровно в том же смысле, что собственный tar: свой
# список файлов, разъезжающийся с install_engine.cmake молча.
OWNMSI=$(copy_wf "$WF" ownmsi.yml)
printf '      - name: own msi\n        run: wixl -o pkg.msi src.wxs\n' >> "$OWNMSI"
mutated "$OWNMSI" "$WF"
expect fail "workflow собирает MSI сам" assert_ci_no_second_packer "$OWNMSI"

# Пин WiX: без установки утверждать нечего, «latest» отдаёт другой инструмент через месяц, а без
# суммы ассет по той же ссылке переписывается молча.
NOWIX=$(copy_wf "$WF" nowix.yml)
sed -i.bak 's|https://github.com/wixtoolset/wix3/releases/download|https://example.invalid/wix|' "$NOWIX" && rm -f "$NOWIX.bak"
mutated "$NOWIX" "$WF"
expect fail "WiX не ставится вовсе" assert_ci_wix_pinned "$NOWIX"

LATEST=$(copy_wf "$WF" latest.yml)
sed -i.bak 's|releases/download/wix3141rtm|releases/latest/download|' "$LATEST" && rm -f "$LATEST.bak"
mutated "$LATEST" "$WF"
expect fail "WiX качается по latest" assert_ci_wix_pinned "$LATEST"

NOSUM=$(copy_wf "$WF" nosum.yml)
sed -i.bak "s|'6ac824e1642d6f7277d0ed7ea09411a508f6116ba6fae0aa5f2c7daa2ff43d31'|''|" "$NOSUM" && rm -f "$NOSUM.bak"
mutated "$NOSUM" "$WF"
expect fail "скачанный WiX не сверяется суммой" assert_ci_wix_pinned "$NOSUM"

# Сумма ищется в теле ТОГО ЖЕ шага: греп по всему файлу принимал за пин WiX любой чужой 64-hex —
# пин образа, ключ кеша, — и шаг без сверки проезжал зелёным. Находка ревью.
ALIENSUM=$(copy_wf "$WF" aliensum.yml)
sed -i.bak "s|'6ac824e1642d6f7277d0ed7ea09411a508f6116ba6fae0aa5f2c7daa2ff43d31'|''|" "$ALIENSUM" && rm -f "$ALIENSUM.bak"
printf '      - name: cache key\n        run: echo 1111111111111111111111111111111111111111111111111111111111111111\n' >> "$ALIENSUM"
mutated "$ALIENSUM" "$WF"
expect fail "сумма стёрта, рядом чужой 64-hex" assert_ci_wix_pinned "$ALIENSUM"

# И обязана участвовать в СРАВНЕНИИ: строка, которая просто лежит в шаге, ничего не проверяет.
NOCMP=$(copy_wf "$WF" nocmp.yml)
sed -i.bak 's|.*if (\$got -ne \$want).*|          $null = $got|' "$NOCMP" && rm -f "$NOCMP.bak"
mutated "$NOCMP" "$WF"
expect fail "сумма WiX ни с чем не сравнивается" assert_ci_wix_pinned "$NOCMP"

bash "$ROOT/scripts/check_release_ci_impl_selftest.sh" || BAD=1
# Утверждение о ПРИЕХАВШЕМ установщике проверяется своим набором: предмет там пакет, а не файлы
# конфигурации, и строить его нужно настоящим компилятором.
bash "$ROOT/scripts/check_release_msi_ci_selftest.sh" || BAD=1

if [ "$BAD" != 0 ]; then echo "ci-selftest: FAIL" >&2; exit 1; fi
echo "ci-selftest: PASS"
