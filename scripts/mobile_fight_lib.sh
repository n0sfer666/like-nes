# shellcheck shell=bash
# Сверка драки мобильной игры с десктопной — общая у `ios_sim_rumble.sh` и `android_rumble.sh`.
# Отдельно от суда возврата (`mobile_resume_lib.sh`): тот судит, что мир пережил фон, эта — что мир
# тот же, что у десктопа.

# Телефон играет ту же драку, что десктоп: тот же демо-ввод и тот же порядок шага дают на тике detach
# тот же хеш у `--headless` десктопной игры той же сборки. Успех — FIGHT_SAID с уликой, отказ — код 1
# и причина в FIGHT_SAID.
desktop_same_fight() {
  local desk=$1 detached=$2 who=$3 tick world hash
  tick=${detached#tick=}
  tick=${tick%% *}
  world=${detached##*world=}
  [ "$tick" -gt 0 ] || { FIGHT_SAID="detached at tick 0: the fight never ran"; return 1; }
  hash=$("$desk/neon_rumble" --headless --frames "$tick" | sed -n "s/^neon-rumble: brawl tick $tick: hash \([0-9a-f]*\),.*/\1/p")
  [ -n "$hash" ] || { FIGHT_SAID="desktop headless printed no brawl hash at tick $tick"; return 1; }
  if [ "$hash" != "$world" ]; then
    FIGHT_SAID="$who world $world at tick $tick, desktop headless $hash: the fight differs"
    return 1
  fi
  FIGHT_SAID="desktop headless at tick $tick hashes $hash, as the $who"
}
