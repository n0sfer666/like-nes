# shellcheck shell=bash
# Общая оснастка позитивного контроля гейта 5 (спека #19): фикстурное дерево, порча, судья исхода.
# Одна на оба набора — документный (check_docs_start_selftest.sh) и прогонный
# (check_docs_start_run_selftest.sh): собери каждый свою копию правил порчи, и «утверждение отбило
# подмену» проверялось бы на деревьях разного устройства.
#
# Порчи требуют ПОДСТРОКИ в тексте отказа, а не только его факта: набор из двадцати кейсов, каждый
# из которых доволен любым ненулевым кодом, доказывает лишь то, что гейт умеет падать. Так уже было
# в вертикали 3 спеки #20, где фикстура полгейта проверяла собственный промах sed.

# Имя набора стоит в КАЖДОЙ строке, а не только в вердикте: наборов два, у них общая оснастка, и
# «упало что-то из сорока» не говорит, в каком файле искать порчу. Задаёт его сам набор.
: "${SELFTEST_NAME:?набор обязан назвать себя в SELFTEST_NAME}"

BAD=0
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT

gate() { bash "$1/scripts/check_docs_start.sh"; }

expect() {
  local want="$1" msg="$2" name="$3" dir="$4" out rc=0
  out=$(gate "$dir" 2>&1) || rc=$?
  if [ "$want" = pass ]; then
    if [ "$rc" = 0 ]; then printf '%s: OK   %s (pass)\n' "$SELFTEST_NAME" "$name"; return; fi
    printf '%s: БРАК %s: ждали pass, код %s\n%s\n' "$SELFTEST_NAME" "$name" "$rc" "$out" >&2
    BAD=1
    return
  fi
  if [ "$rc" = 0 ]; then
    printf '%s: БРАК %s: порча проехала гейт\n' "$SELFTEST_NAME" "$name" >&2
    BAD=1
    return
  fi
  case "$out" in
    *"$msg"*) printf '%s: OK   %s (fail: %s)\n' "$SELFTEST_NAME" "$name" "$msg" ;;
    *)
      printf '%s: БРАК %s: упал не по своей причине, ждали «%s»\n%s\n' "$SELFTEST_NAME" \
        "$name" "$msg" "$out" >&2
      BAD=1
      ;;
  esac
}

# Каталог берётся у mktemp, а не у счётчика: вызывается tree_for в подстановке команды, то есть в
# СУБШЕЛЛЕ, и счётчик не пережил бы ни одного кейса — все порчи копились бы в одном дереве.
tree_for() {
  local d
  d=$(mktemp -d "$FIX/tXXXXXX")
  start_fix_tree "$d" "$ROOT"
  printf '%s' "$d"
}

# Порча документа идёт в ОБА языка разом, если она не про их расхождение: иначе кейс падал бы на
# соседнем утверждении (команды en и ru разошлись) и о своём не сказал бы ничего.
doc_both() {
  local d="$1" f="$2" e="$3" l p
  for l in en ru; do
    p="$d/docs/$l/getting-started/$f"
    start_fix_mutate "$p" sed -i.bak -e "$e" "$p" || return 1
    rm -f "$p.bak"
  done
}
doc_one() {
  local p="$1/docs/$2/getting-started/$3"
  start_fix_mutate "$p" sed -i.bak -e "$4" "$p" || return 1
  rm -f "$p.bak"
}
file_of() { printf '%s/scripts/%s' "$1" "$2"; }
script_mut() {
  local p; p=$(file_of "$1" "$2")
  start_fix_mutate "$p" sed -i.bak -e "$3" "$p" || return 1
  rm -f "$p.bak"
}

# Вердикт набора печатает СВОЁ имя.
start_selftest_verdict() {
  [ "$BAD" = 0 ] || { printf '%s: FAIL\n' "$SELFTEST_NAME" >&2; return 1; }
  printf '%s: PASS\n' "$SELFTEST_NAME"
}
