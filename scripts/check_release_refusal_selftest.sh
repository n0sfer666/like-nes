#!/usr/bin/env bash
# Позитивный контроль утверждений о КОДАХ ОТКАЗА release.sh (спека #20, вертикаль 2): windows —
# код 3, linux — делегирование в контейнер, --platform на хостовой сборке — код 2. Предмет здесь
# один: что скрипт делает на ДИСПАТЧЕ, и узнаётся это только прогоном.
#
# Свой файл и внешняя команда — по той же границе, что у пары `check_release_selftest.sh` /
# `check_release_pack_selftest.sh`: соседний набор ломает файлы контейнерного пути (пин базы,
# монтирование, второй упаковщик), а этот — ветвление release.sh. В логе стоит имя упавшего.
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
# shellcheck source=scripts/release_refusal_check_lib.sh
. "$ROOT/scripts/release_refusal_check_lib.sh"

BAD=0
# Копии лежат В scripts/: release.sh считает корень дерева от своего расположения и оттуда же
# берёт библиотеки — копия в /tmp падала бы на первом `source`, то есть «отбита» выходило бы из
# ненайденного файла, а не из вырезанной меры. Префикс свой, уборка по шаблону: список, накопленный
# в сабшелле, до trap не доезжает — это уже стоило соседнему набору десяти переживших прогон копий.
trap 'rm -f "$ROOT"/scripts/.selftest_ref_*' EXIT

copy_path() {
  printf '%s\n' "$ROOT/scripts/.selftest_ref_$1"
}

# Раздельные `local`: bash 3.2 со штатного macOS объявляет все имена строки разом и лишь потом
# присваивает, поэтому `local a="$1" b="$a"` под `set -u` умирает как unbound — и набор печатал бы
# PASS, не создав ни одной копии.
copy_script() {
  local src="$1"
  local name="$2"
  local dst
  dst=$(copy_path "$name")
  cp "$src" "$dst" || { printf 'refusal-selftest: БРАК копия %s не создана\n' "$name" >&2; exit 1; }
  [ -s "$dst" ] || { printf 'refusal-selftest: БРАК копия %s пуста\n' "$name" >&2; exit 1; }
}

# Подмена, которая ничего не подменила, читается ровно как «утверждение её отбило»: копия остаётся
# нетронутой, а утверждение честно проходит. Так и вышло — проверка `--platform` переехала выше
# диспатча и обросла вторым условием, sed по старой строке промахнулся, и фикстура «съеден молча»
# полгейта проверяла собственный промах. Сравнение с оригиналом отбирает у промаха право быть
# вердиктом: это тот же класс, что правило vacuous-gate в ci_lint.py.
# Промах ОСТАНАВЛИВАЕТ сценарий, а не только помечает прогон: непорченая копия проходит утверждение,
# то есть следующий `expect fail` печатает вторую строку БРАК про совсем другое — про утверждение,
# которое на самом деле в порядке. Отсюда форма `mutated X && expect …`: не подменилось — проверять
# нечего.
mutated() {
  local dst
  dst=$(copy_path "$1")
  if cmp -s "$ROOT/scripts/release.sh" "$dst"; then
    printf 'refusal-selftest: БРАК подмена %s ничего не изменила\n' "$1" >&2
    BAD=1
    return 1
  fi
}

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'refusal-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'refusal-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Опорный прогон: нетронутая копия обязана ПРОХОДИТЬ каждое утверждение, которое ниже валит её
# сломанный близнец. Без этой пары «подмена отбита» неотличимо от «файла нет».
copy_script "$ROOT/scripts/release.sh" intact.sh
INTACT=$(copy_path intact.sh)
expect pass "нетронутая копия · чужая платформа код 3" assert_foreign_platform_refused "$INTACT"
expect pass "нетронутая копия · --platform отвергнут" assert_platform_rejected_on_host "$INTACT"
case "$(uname -s)" in
  Linux) echo "refusal-selftest: ПРОПУЩЕНО нетронутая копия · делегирование — на линукс-хосте это своя сборка" ;;
  *) expect pass "нетронутая копия · делегирование" assert_linux_delegated "$INTACT" ;;
esac

# Фикстура воспроизводит НАХОДКУ ревью, а не гипотезу: резолюция версии, поднятая обратно над
# диспатчем, отбирает у чужой платформы её код 3 на HEAD без тега — отказ выходит кодом 2
# («укажи --version»), и оркестратор читает незакрытую вертикаль как свою же опечатку. Пока
# утверждение звало только форму С `--version`, эта правка проходила молча.
copy_script "$ROOT/scripts/release.sh" hoisted.sh
HOISTED=$(copy_path hoisted.sh)
python3 - "$HOISTED" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
m = 'if [ "$ONLY" != "$HOST" ]; then\n'
open(p, 'w').write(s.replace(m, 'VERSION=$(resolve_version "$ROOT" "$VERSION") || exit 2\n' + m, 1))
PY
mutated hoisted.sh &&
  expect fail "версия резолвится над диспатчем" assert_foreign_platform_refused "$HOISTED"

# Отказ чужой платформе обязан остаться кодом 3 и тогда, когда его подменили кодом 2: без этой
# фикстуры «linux больше не 3» прошло бы на скрипте, который перестал различать коды вовсе.
copy_script "$ROOT/scripts/release.sh" code2.sh
CODE2=$(copy_path code2.sh)
sed 's|^  exit 3$|  exit 2|' "$CODE2" > "$CODE2.tmp" && mv "$CODE2.tmp" "$CODE2"
mutated code2.sh &&
  expect fail "чужой платформе чужой код" assert_foreign_platform_refused "$CODE2"

copy_script "$ROOT/scripts/release.sh" refuse.sh
REFUSE=$(copy_path refuse.sh)
python3 - "$REFUSE" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
i = s.index('    linux)\n')
j = s.index('      ;;\n', i) + len('      ;;\n')
s = s[:i] + '    linux) echo "release: linux не здесь" >&2 ;;\n' + s[j:]
open(p, 'w').write(s)
PY
mutated refuse.sh || REFUSE=""
case "$(uname -s)" in
  Linux) echo "refusal-selftest: ПРОПУЩЕНО linux снова отказ кодом 3 — на линукс-хосте это своя сборка" ;;
  *) [ -z "$REFUSE" ] || expect fail "linux снова отказ кодом 3" assert_linux_delegated "$REFUSE" ;;
esac

# Строка проверки правится ОДНИМ местом на две фикстуры и подставляется КАК СТРОКА, а не шаблоном:
# она уже переезжала, и sed по старому тексту промахивался молча — фикстура тогда проверяла
# собственный промах. Про сам промах отвечает `mutated` ниже.
COND='if [ -n "$PLATFORM" ] && { [ "$ONLY" != linux ] || [ "$HOST" = linux ]; }; then'
swap_cond() {
  local dst
  dst=$(copy_path "$1")
  python3 -c 'import sys;p,a,b=sys.argv[1:4];s=open(p).read();open(p,"w").write(s.replace(a,b,1))' \
    "$dst" "$COND" "$2"
}

copy_script "$ROOT/scripts/release.sh" eaten.sh
swap_cond eaten.sh "if false; then"
mutated eaten.sh &&
  expect fail "--platform съеден молча" assert_platform_rejected_on_host "$(copy_path eaten.sh)"

# Фикстура воспроизводит НАХОДКУ первого прогона на настоящем Linux, а не гипотезу: условие
# `$ONLY != linux` сравнивает УЖЕ развёрнутое значение, поэтому на линукс-машине `--only host`
# становится `linux` и флаг чужого этажа съедается молча — ровно на той ОС, где гейт не гонялся.
# Утверждение обязано валить её на всех трёх: линукс-хост оно делает себе заглушкой само.
copy_script "$ROOT/scripts/release.sh" oldcond.sh
swap_cond oldcond.sh 'if [ -n "$PLATFORM" ] && [ "$ONLY" != linux ]; then'
mutated oldcond.sh &&
  expect fail "--platform съеден на линукс-хосте" assert_platform_rejected_on_host "$(copy_path oldcond.sh)"

# Фикстура доказывает, что утверждение читает ОТКАЗ, а не общий код: у release.sh код 2 отдают пять
# разных веток, и копия без разбора `--platform` вовсе отвечает «непонятый аргумент» тем же кодом 2.
# Пока утверждение сравнивало только код, оно принимало такую копию за исправную — то есть говорило
# «флаг отвергается» о скрипте, который про флаг не знает.
copy_script "$ROOT/scripts/release.sh" unknownflag.sh
python3 - "$(copy_path unknownflag.sh)" <<'PY2'
import sys
p = sys.argv[1]
s = open(p).read()
m = '    --platform) need_value $# "$1"; PLATFORM="$2"; shift 2 ;;\n'
assert m in s
open(p, 'w').write(s.replace(m, '', 1))
PY2
mutated unknownflag.sh &&
  expect fail "--platform не разбирается вовсе" assert_platform_rejected_on_host "$(copy_path unknownflag.sh)"

if [ "$BAD" = 0 ]; then
  echo "refusal-selftest: PASS"
else
  echo "refusal-selftest: FAIL" >&2
fi
exit "$BAD"
