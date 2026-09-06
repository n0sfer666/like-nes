#!/usr/bin/env bash
# Позитивный контроль инварианта 1 спеки #14 (`tree_invariants.sh deps`): утверждение, у которого
# нет фикстуры, где оно падает, неотличимо от отсутствующего. До аудита #21 у семейства инвариантов
# не было ни одной порчи вовсе — все четыре правила держали ВНУТРЕННИЕ vacuous-контроли и ничем не
# доказывали, что умеют отбить нарушение.
#
# Предмет здесь ОДИН — направление зависимостей, и границу набор называет вслух: соседние три
# правила (швы ОС, argv, окружение) своего позитивного контроля пока не имеют, это записано в
# вердикте аудита. Фикстуре под них пришлось бы нести полтора десятка `main` и живые швы platform,
# то есть половину дерева.
#
# Ключевая порча — ребро к слою из `engine/achievements`: до этого раунда список подсистем был
# написан руками и перечислял восемь каталогов из двенадцати, поэтому она проезжала ЗЕЛЁНОЙ. Что
# порча различает реализации, доказано подменой: копия с прежним рукописным списком обязана её
# пропустить.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
NAME=tree-invariants-deps-selftest
pass=0; mut=0; fail=0

# Фикстура — настоящее дерево, а не подсунутая переменная: гейт сам делает `cd` в корень над
# scripts/, и подменить ему корень можно только тем, где он лежит.
tree_for() {
  local d
  d=$(mktemp -d "${TMPDIR:-/tmp}/tree-inv.XXXXXX")
  mkdir -p "$d/scripts" "$d/engine/framework/core" "$d/engine/render" "$d/engine/achievements" \
           "$d/engine/light" "$d/engine/material"
  cp "$ROOT/scripts/tree_invariants.sh" "$d/scripts/tree_invariants.sh"
  printf 'add_library(framework_core STATIC schedule.cpp)\n' > "$d/engine/framework/core/CMakeLists.txt"
  printf 'add_library(render_core STATIC device.cpp)\ntarget_link_libraries(render_core PUBLIC platform_core)\n' \
      > "$d/engine/render/CMakeLists.txt"
  printf 'add_library(ach_core STATIC bake.cpp)\ntarget_link_libraries(ach_core PUBLIC platform_core)\n' \
      > "$d/engine/achievements/CMakeLists.txt"
  printf 'add_library(material_text STATIC text.cpp)\ntarget_link_libraries(material_text PUBLIC platform_core)\n' \
      > "$d/engine/material/CMakeLists.txt"
  printf 'add_library(light_bake STATIC bake.cpp)\ntarget_link_libraries(light_bake PUBLIC platform_core)\n' \
      > "$d/engine/light/CMakeLists.txt"
  printf '%s\n' "$d"
}

# Порча обязана РЕАЛЬНО появиться в дереве: подмена, ничего не подменившая, читается как
# «утверждение её отбило» — тот же класс, что чинили в refusal-наборе релиза.
edge_to_layer() {
  local d="$1" rel="$2"
  [ -e "$d/$rel" ] && {
    printf '%s: FAIL порча %s уже была в фикстуре\n' "$NAME" "$rel" >&2
    fail=$((fail + 1))
  }
  printf '#include "../framework/core/text_fields.hpp"\n' > "$d/$rel"
  [ -s "$d/$rel" ] || {
    printf '%s: FAIL порча не создала %s — проверяли бы собственный промах\n' "$NAME" "$rel" >&2
    fail=$((fail + 1))
  }
}

# Прежняя реализация: список подсистем, написанный руками. Она обязана ПРОПУСТИТЬ ребро из
# achievements — этим и доказывается, что порча различает реализации, а не падает всегда.
handwritten() {
  local f="$1/scripts/tree_invariants.sh" before after
  before=$(cksum < "$f")
  python3 - "$f" <<'PY'
import pathlib, re, sys
p = pathlib.Path(sys.argv[1])
s = p.read_text(encoding="utf-8")
old = s[s.index("    local layer=engine/framework"):s.index('    [ -n "$dirs" ] || fail')]
new = '    local dirs="engine/render engine/light engine/material"\n'
p.write_text(s.replace(old + '    [ -n "$dirs" ] || fail "no subsystem directories found — the gate is vacuous"\n', new), encoding="utf-8")
PY
  after=$(cksum < "$f")
  [ "$before" != "$after" ] || {
    printf '%s: FAIL подмена реализации ничего не подменила\n' "$NAME" >&2
    fail=$((fail + 1))
  }
}

expect() {
  local want="$1" needle="$2" what="$3" d="$4" out rc=0
  out=$(bash "$d/scripts/tree_invariants.sh" deps 2>&1) || rc=$?
  rm -rf "$d"
  if [ "$want" = pass ] && [ "$rc" != 0 ]; then
    printf '%s: FAIL %s — ждали PASS, код %s\n%s\n' "$NAME" "$what" "$rc" "$out" >&2
    fail=$((fail + 1)); return
  fi
  if [ "$want" = fail ]; then
    if [ "$rc" = 0 ]; then
      printf '%s: FAIL %s — порча проехала зелёной\n' "$NAME" "$what" >&2
      fail=$((fail + 1)); return
    fi
    if ! grep -qF -- "$needle" <<<"$out"; then
      printf '%s: FAIL %s — отказ не назвал «%s»:\n%s\n' "$NAME" "$what" "$needle" "$out" >&2
      fail=$((fail + 1)); return
    fi
  fi
  printf '%s: OK   %s\n' "$NAME" "$what"
  if [ "$want" = pass ]; then pass=$((pass + 1)); else mut=$((mut + 1)); fi
}

d=$(tree_for); expect pass '' 'исправное дерево' "$d"

# Опорный pass, фиксирующий ГРАНИЦУ инварианта: light линкует material_text и включает его
# заголовок — ребро подсистема→подсистема, инвариантом 1 НЕ запрещённое. Отчёт B прохода аудита
# читал это ребро как опровержение обоснования копий в material/text.hpp; запрещено другое —
# ребро подсистемы к СЛОЮ.
d=$(tree_for)
printf 'target_link_libraries(light_bake PUBLIC material_text)\n' >> "$d/engine/light/CMakeLists.txt"
printf '#include "../material/text.hpp"\n' > "$d/engine/light/bake.cpp"
expect pass '' 'ребро подсистема→подсистема не запрещено' "$d"

d=$(tree_for); edge_to_layer "$d" engine/achievements/probe.hpp
expect fail 'direction broken' 'ребро к слою в каталоге, которого рукописный список не знал' "$d"

d=$(tree_for); edge_to_layer "$d" engine/render/probe.hpp
expect fail 'direction broken' 'ребро к слою в каталоге из прежнего списка' "$d"

# Та же порча на ПРЕЖНЕЙ реализации: она обязана проехать зелёной, иначе кейс выше ничего не
# говорит о выросшем покрытии — он падал бы на любой реализации.
d=$(tree_for); handwritten "$d"; edge_to_layer "$d" engine/achievements/probe.hpp
expect pass '' 'прежняя реализация ту же порчу ПРОПУСКАЕТ' "$d"

d=$(tree_for)
printf 'target_link_libraries(x PUBLIC framework_core)\n' > "$d/engine/net_probe.cmake"
mkdir -p "$d/engine/net"; mv "$d/engine/net_probe.cmake" "$d/engine/net/CMakeLists.txt"
expect fail 'direction broken' 'ребро к слою именем цели, а не включением' "$d"

d=$(tree_for); rm -rf "$d/engine/framework"
expect fail 'the framework layer is not where the gate looks' 'каталога слоя нет вовсе' "$d"

d=$(tree_for); rm -rf "$d/engine/framework/core"
expect fail 'framework layer has no modules' 'слой без модулей — гейт вакуумен' "$d"

d=$(tree_for)
for f in "$d"/engine/*/CMakeLists.txt; do : > "$f"; done
expect fail 'search found no link edges' 'ни одного ребра линковки — гейт вакуумен' "$d"

printf '%s: %s — опорных pass %d, отбито порч %d\n' "$NAME" \
  "$( [ "$fail" = 0 ] && echo PASS || echo FAIL )" "$pass" "$mut"
[ "$fail" = 0 ]
