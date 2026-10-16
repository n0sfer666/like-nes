# shellcheck shell=bash
# Суд возврата из фона — общий у iOS-симулятора (`ios_sim_lib.sh`) и телефона Android
# (`android_rumble.sh`): обе оболочки печатают строки игры одного вида, и улики одни.
# Строки: `[game] surface attached #N tick=T world=H` и `[game] surface detached tick=T world=H`.
# Счётчик attach живёт в процессе, поэтому `#2` — улика того же процесса. Мир при `#2` обязан совпасть
# с миром при снятии поверхности: мир, пересозданный в attach, сохранил бы тик, но не хеш.

# Номер последнего `surface attached #N` в логе; 0 — строк attach ещё нет.
last_attach() {
  local n
  n=$(sed -n 's/.*surface attached #\([0-9]*\) .*/\1/p' "$1" | tail -1)
  printf '%s' "${n:-0}"
}

# `tick=T world=H` у attach #k; пусто — такой строки нет.
attach_at() {
  sed -n "s/.*surface attached #$2 \\(tick=[0-9]* world=[0-9a-f]*\\).*/\\1/p" "$1" | head -1
}

# `tick=T world=H` у первой detach после attach #n и до следующего attach; пусто — её нет. Пару судят
# по номеру, а не по первой строке лога: поворот или гашение экрана до Home дали бы свои пары, и суд
# первой пары принял бы их за возврат из фона.
detach_after() {
  awk -v n="$2" '
    on && index($0, "surface attached #") { exit }
    index($0, "surface attached #" n " ") { on = 1; next }
    on && match($0, /surface detached tick=[0-9]+ world=[0-9a-f]+/) { print substr($0, RSTART + 17, RLENGTH - 17); exit }
  ' "$1"
}

# Судит возврат после attach #n: attach #n+1 обязан нести мир detach, снятой после #n, и тик позже,
# чем у #n. Успех — RESUME_SAID с уликой и RESUME_DETACHED=`tick=T world=H`, отказ — код 1 и причина
# в RESUME_SAID.
resume_judged() {
  local log=$1 n=$2 t1 a2 t2
  t1=$(attach_at "$log" "$n")
  t1=${t1#tick=}
  t1=${t1%% *}
  a2=$(attach_at "$log" $((n + 1)))
  RESUME_DETACHED=$(detach_after "$log" "$n")
  [ -n "$t1" ] || { RESUME_SAID="no surface attach #$n before going to background — $log"; return 1; }
  if [ -z "$a2" ]; then
    RESUME_SAID="no surface attach #$((n + 1)) 30 s after returning to foreground — $log"
    return 1
  fi
  [ -n "$RESUME_DETACHED" ] || { RESUME_SAID="no surface detach between attach #$n and #$((n + 1)) — $log"; return 1; }
  if [ "$a2" != "$RESUME_DETACHED" ]; then
    RESUME_SAID="attach #$((n + 1)) at $a2, detach at $RESUME_DETACHED: the world restarted instead of surviving"
    return 1
  fi
  t2=${a2#tick=}
  t2=${t2%% *}
  if [ "$t2" -le "$t1" ]; then
    RESUME_SAID="attach #$((n + 1)) at tick $t2, #$n at $t1: no frames ran before going to background"
    return 1
  fi
  RESUME_SAID="foreground re-attached the surface at $a2, as detached (attach #$n at tick $t1)"
}
