#!/usr/bin/env bash
# Позитивный контроль утверждения о ТРЕТЬЕЙ паре копий закрытого списка (спека #20, вертикаль 5):
# app-local DLL, названные в expected_files руками, против таблицы импортов настоящего рантайма, по
# которой их кладёт в пакет cmake. Отделён от соседнего набора по ПРЕДМЕТУ: тот ломает ФАЙЛЫ дерева
# (CMakeLists.txt и две копии списка), а здесь ломать нечего — утверждение зовёт expected_files как
# ЗАГРУЖЕННУЮ реализацию, поэтому порча идёт подменой функции, а рантайм подставляется фикстурным
# деревом. Зовётся ВНЕШНЕЙ командой из check_release_crt_wiring_selftest.sh, чтобы в логе стояло имя
# упавшего набора.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_crt_lib.sh
. "$ROOT/scripts/release_crt_lib.sh"
# shellcheck source=scripts/release_crt_wiring_lib.sh
. "$ROOT/scripts/release_crt_wiring_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"
# shellcheck source=scripts/selftest_sub_lib.sh
. "$ROOT/scripts/selftest_sub_lib.sh"

BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

expect() {
  local want="$1" name="$2"; shift 2
  local rc=0
  "$@" >/dev/null 2>&1 || rc=$?
  if { [ "$want" = pass ] && [ "$rc" = 0 ]; } || { [ "$want" = fail ] && [ "$rc" != 0 ]; }; then
    printf 'crt-named-selftest: OK   %s (%s)\n' "$name" "$want"
  else
    printf 'crt-named-selftest: БРАК %s: ожидали %s, получили код %s\n' "$name" "$want" "$rc" >&2
    BAD=1
  fi
}

# Незагруженная функция даёт код 127, а он ненулевой — то есть каждый ожидающий fail кейс проезжал
# бы как «утверждение отбило подмену», не позвав утверждения ни разу.
if ! declare -F assert_redist_expected_named >/dev/null; then
  printf 'crt-named-selftest: БРАК утверждение assert_redist_expected_named не определено\n' >&2
  exit 1
fi

# Третья пара копий: app-local DLL, названные в expected_files руками, против таблицы импортов
# рантайма, по которой их кладёт в пакет cmake. Порча идёт ПОДМЕНОЙ ФУНКЦИИ, а не файла: утверждение
# зовёт expected_files как загруженную реализацию, и копия файла в фикстурном дереве никем бы не
# читалась.
ORIG_EXPECTED=$(declare -f expected_files)

# Дерево, у которого от настоящего только читатель PE и манифест лицензий, а рантайм — фикстура.
named_tree() {
  local tag="$1"
  local d="$FIX/named-$tag"
  local sub
  sub="$d/build-fix/$(crt_dist_subdir)"
  mkdir -p "$sub" "$d/scripts" "$d/cmake"
  cp "$ROOT/scripts/pe_imports.py" "$d/scripts/" || return 1
  cp "$ROOT/cmake/licenses.manifest" "$d/cmake/" || return 1
  printf '%s\n' "$d"
}

# Место рантайма в фикстурном дереве считается ТЕМ ЖЕ источником, каким его ищет утверждение:
# написанные врозь, они разъехались бы молча, и порча ложилась бы мимо файла, куда смотрит гейт.
named_fixture_dll() { printf '%s/build-fix/%s/wgpu_native.dll\n' "$1" "$(crt_dist_subdir)"; }

# Три кейса ниже зовут утверждение на НАСТОЯЩЕМ дереве, а без выкачанной дистрибуции оно возвращает
# ноль ПРОПУСКОМ ВСЛУХ — то есть опорный `pass` подтверждал бы не сверку списков, а пропуск, обе
# порчи получали бы тот же ноль, и набор падал бы по причине ОКРУЖЕНИЯ, неотличимо от честно
# сломанного утверждения. На свежем клоне (`build*` в .gitignore) это роняло бы весь этап preflight.
# Защита та же, что у якоря в соседнем наборе, и пропуск здесь тоже ВСЛУХ.
if ! crt_real_dll "$ROOT" >/dev/null; then
  echo "crt-named-selftest: ПРОПУСК — настоящего wgpu_native.dll нет в дереве, сверка на нём не проверена"
else
  expect pass "настоящее дерево · app-local против импортов" assert_redist_expected_named "$ROOT"

  # Строка `vcruntime140.dll` пропала из ожидаемого состава: cmake положит её в пакет по таблице
  # импортов, а утверждение о составе будет ждать пакет без неё — разойдутся они на машине владельца.
  case_named_missing() {
    (
      eval "expected_files_real() $(printf '%s' "$ORIG_EXPECTED" | tail -n +2)"
      expected_files() { expected_files_real "$@" | grep -v 'vcruntime140\.dll'; }
      subbed expected_files "$ORIG_EXPECTED" || exit 1
      assert_redist_expected_named "$ROOT"
    )
  }
  expect fail "app-local DLL пропала из ожидаемого состава" case_named_missing

  # Обратная порча: в составе названа лишняя redist-DLL, которой рантайм не просит, — пакет ждал бы
  # файла, которого cmake не кладёт.
  case_named_extra() {
    (
      eval "expected_files_real() $(printf '%s' "$ORIG_EXPECTED" | tail -n +2)"
      expected_files() { expected_files_real "$@"; printf 'like-nes/bin/msvcp140.dll\n'; }
      subbed expected_files "$ORIG_EXPECTED" || exit 1
      assert_redist_expected_named "$ROOT"
    )
  }
  expect fail "в составе названа лишняя app-local DLL" case_named_extra
fi

# Рантайм без единого redist-импорта: списки пусты с обеих сторон и равны сами себе — читатель,
# промахнувшийся мимо таблицы, выдал бы за согласие ровно ту тишину, ради которой заведена вертикаль.
case_named_norequest() {
  local d; d=$(named_tree norequest) || return 1
  python3 "$ROOT/scripts/pe_fixture.py" "$(named_fixture_dll "$d")" --import kernel32.dll || return 1
  assert_redist_expected_named "$d"
}
expect fail "рантайм не просит ни одной redist-DLL" case_named_norequest

# Файл на месте, но не PE: отказ читателя обязан быть отказом утверждения, а не пустым списком.
# Ветка эта была недостижима: код возврата пайпа есть код фильтра, а не читателя, — и фикстура
# падала по причине СОСЕДНЕГО утверждения («не просит ни одной redist-DLL»), ничего не доказывая.
case_named_unreadable() {
  local d; d=$(named_tree unreadable) || return 1
  printf 'not a PE\n' > "$(named_fixture_dll "$d")"
  assert_redist_expected_named "$d"
}
expect fail "таблица импортов рантайма не прочитана" case_named_unreadable

if [ "$BAD" != 0 ]; then echo "crt-named-selftest: FAIL" >&2; exit 1; fi
echo "crt-named-selftest: PASS"
