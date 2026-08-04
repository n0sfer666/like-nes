#!/usr/bin/env bash
# Гейт 8 спеки #13 (сквозной цикл) одной командой: свежий клон → сборка → кадр ДО → правка видимой
# константы в коде игры → пересборка → кадр ПОСЛЕ → ЧИСЛЕННОЕ доказательство сдвига + целость
# sim-хеша. Работает на трёх ОС, ручной работы не оставляет.
#
# Почему скриптом, а не списком шагов в доке. «Изменение видно» глазами — единственная часть гейта,
# которую владелец мог подтвердить только собой; кадры игра и так пишет offscreen, значит сдвиг
# цвета можно ИЗМЕРИТЬ, и тогда доказательством становится число в отчёте, а не память о том, что
# фон покраснел. Заодно снимается вторая половина риска: правка делается ВНУТРИ клона, поэтому
# рабочее дерево не трогается вовсе и «забыл откатить пробу» перестаёт быть возможным.
#
# Запуск:
#   bash scripts/gate8_e2e.sh
# Windows — из x64 Native Tools Command Prompt for VS, шеллом git-bash:
#   "C:\Program Files\Git\bin\bash.exe" scripts/gate8_e2e.sh
#
# Ручки: GATE8_SOURCE (что клонировать, по умолчанию этот репозиторий), GATE8_FRAMES,
# GATE8_DR_MIN (порог сдвига красного), GATE8_KEEP=1 (не удалять клон после прогона).
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1

OS_TAG=$(uname -s | tr '[:upper:]' '[:lower:]' | tr -d ' ')
OUT="$ROOT/build/gate8"
REPORT="$OUT/gate8-report-$OS_TAG.txt"
WORK="$OUT/clone"
SOURCE=${GATE8_SOURCE:-$ROOT}
FRAMES=${GATE8_FRAMES:-60}
DR_MIN=${GATE8_DR_MIN:-4}
FAILS=0

rm -rf "$OUT"
mkdir -p "$OUT" || exit 1
: > "$REPORT"

say() { printf '%s\n' "$*" | tee -a "$REPORT"; }
head_() { printf '\n=== %s\n' "$*" | tee -a "$REPORT"; }
have() { command -v "$1" >/dev/null 2>&1; }
ok() { say "  ok   $1"; }
bad() { say "  FAIL $1"; FAILS=$((FAILS + 1)); }
check() { if [ "$1" = "0" ]; then ok "$2"; else bad "$2"; fi; }
# Имя без расширения на Windows не находится: там бинарь зовётся *.exe, и `[ -x ]` по короткому
# имени молча ложен — шаг выглядел бы пропущенным, а не сломанным.
bin() { if [ -x "$1" ]; then printf '%s' "$1"; elif [ -x "$1.exe" ]; then printf '%s' "$1.exe"; fi; }

PY=""
for cand in python3 python py; do
    if have "$cand" && "$cand" -c 'import sys' >/dev/null 2>&1; then PY=$cand; break; fi
done

head_ "Гейт 8 спеки #13 — сквозной цикл (клон → сборка → правка → видимое изменение)"
say "date        : $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
say "os          : $(uname -s -r -m)"
say "python      : ${PY:-НЕ НАЙДЕН}"
[ -n "$PY" ] || { bad "нужен python3 — им меряется цвет кадра"; say ""; say "гейт 8: FAIL"; exit 1; }

head_ "1. Свежий клон"
# Клонируется КОММИТ, а не рабочее дерево: несохранённая правка в гейт не попадает, и это ровно то
# состояние, которое получит чужая машина. Поэтому хеш клона печатается — отчёт без него не
# отвечает на вопрос «что именно проверено».
git clone --quiet "$SOURCE" "$WORK"
check $? "склонировано из $SOURCE"
[ -d "$WORK" ] || { say ""; say "гейт 8: FAIL"; exit 1; }
say "  commit    : $(git -C "$WORK" rev-parse HEAD) ($(git -C "$WORK" rev-parse --abbrev-ref HEAD))"

head_ "2. Сборка клона с нуля"
GEN=""
have ninja && GEN="-G Ninja"
cmake -S "$WORK" -B "$WORK/build" $GEN -DCMAKE_BUILD_TYPE=Release > "$OUT/configure.log" 2>&1
check $? "конфигурирование (лог: build/gate8/configure.log)"
cmake --build "$WORK/build" > "$OUT/build-before.log" 2>&1
check $? "сборка дерева (лог: build/gate8/build-before.log)"

GAME=$(bin "$WORK/build/game_sidescroller")
SIM=$(bin "$WORK/build/game_sim_test")
EDITOR=$(bin "$WORK/build/editor_shell")
if [ -n "$GAME" ]; then ok "собран game_sidescroller"; else bad "game_sidescroller не собрался"; fi
if [ -n "$SIM" ];  then ok "собран game_sim_test";      else bad "game_sim_test не собрался"; fi
# Редактор — часть того же гейта («открыть редактор из свежего клона»), но его окно проверяет
# гейт 6: здесь доказывается только то, что клон его ДАЁТ. В headless-конфигурации цели нет.
if [ -n "$EDITOR" ]; then
    say "  ok   собран editor_shell — окно и гизмо закрывает гейт 6: $EDITOR --gate6 gate6.png"
else
    say "  --   editor_shell не собран (PLUGIN_UI выключен) — гейт 6 гоняется отдельной сборкой"
fi
[ -n "$GAME" ] && [ -n "$SIM" ] || { say ""; say "гейт 8: FAIL (нечего прогонять)"; exit 1; }

head_ "3. Кадр ДО правки"
# Каталоги создаются заранее: игра пишет кадр по готовому пути и на отсутствующий каталог не
# ругается — гейт получил бы «кадров нет» вместо «каталога нет».
mkdir -p "$OUT/before" "$OUT/after"
"$GAME" --demo "$OUT/before" --frames "$FRAMES" > "$OUT/demo-before.log" 2>&1
check $? "игра отрисовала $FRAMES кадров offscreen"
LAST=$(printf 'frame_%04d.png' $((FRAMES - 1)))
if [ -f "$OUT/before/$LAST" ]; then ok "есть кадр $LAST"; else bad "кадр $LAST не записан"; fi
HASH_BEFORE=$("$SIM" 2>/dev/null | sed -n 's/.*combat-golden-hash = //p')
say "  sim-hash  : ${HASH_BEFORE:-НЕ ПОЛУЧЕН}"

head_ "4. Правка кода игры"
SRC="$WORK/example_ugly_game/draw.cpp"
OLD='WGPUColor{0.02, 0.02, 0.07, 1.0}'
NEW='WGPUColor{0.25, 0.02, 0.05, 1.0}'
# sed -i писать нельзя: у GNU и BSD он требует разных аргументов, и портит файл ровно на той ОС,
# где владелец гейт и гоняет. Через временный файл — одинаково везде.
sed 's/0\.02, 0\.02, 0\.07/0.25, 0.02, 0.05/' "$SRC" > "$SRC.tmp" && mv "$SRC.tmp" "$SRC"
grep -q "0.25, 0.02, 0.05" "$SRC"
check $? "цвет очистки заменён на красный (правка ПРИМЕНИЛАСЬ, а не тихо промахнулась)"
say "  правка    : $OLD → $NEW"

head_ "5. Пересборка и кадр ПОСЛЕ"
cmake --build "$WORK/build" --target game_sidescroller > "$OUT/build-after.log" 2>&1
check $? "инкрементальная пересборка игры"
"$GAME" --demo "$OUT/after" --frames "$FRAMES" > "$OUT/demo-after.log" 2>&1
check $? "игра отрисовала $FRAMES кадров с правкой"

head_ "6. Доказательство"
B=$("$PY" "$ROOT/scripts/png_mean.py" "$OUT/before/$LAST" 2>&1)
A=$("$PY" "$ROOT/scripts/png_mean.py" "$OUT/after/$LAST" 2>&1)
say "  до        : R G B = $B"
say "  после     : R G B = $A"
# Красный обязан вырасти И обогнать оба других канала: «кадр изменился» доказывало бы и любое
# дрожание рендера, а гейт про то, что изменилась ИМЕННО правленая константа.
VERDICT=$(printf '%s %s' "$B" "$A" | awk -v m="$DR_MIN" '
    NF == 6 {
        dr = $4 - $1; dg = $5 - $2; db = $6 - $3;
        printf "dR=%+.3f dG=%+.3f dB=%+.3f ", dr, dg, db;
        print (dr >= m && dr > dg && dr > db) ? "PASS" : "FAIL";
    }
    NF != 6 { print "кадры не прочитались: FAIL" }')
say "  сдвиг     : $VERDICT (порог dR ≥ $DR_MIN)"
case "$VERDICT" in
    *PASS) ok "изменение ВИДНО: кадр сдвинулся в красное" ;;
    *)     bad "изменение не видно в кадре" ;;
esac

HASH_AFTER=$("$SIM" 2>/dev/null | sed -n 's/.*combat-golden-hash = //p')
say "  sim-hash  : ${HASH_AFTER:-НЕ ПОЛУЧЕН}"
# Смысл именно рендерной константы: правка обязана быть видимой и при этом НЕ трогать симуляцию.
# Совпадение хеша — вторая половина доказательства, без неё гейт прошёл бы и сломанный геймплей.
if [ -n "$HASH_BEFORE" ] && [ "$HASH_BEFORE" = "$HASH_AFTER" ]; then
    ok "sim-golden не сдвинулся (правка рендерная, симуляция цела)"
else
    bad "sim-golden разошёлся: было '$HASH_BEFORE', стало '$HASH_AFTER'"
fi

head_ "Итог"
say "артефакты   : $OUT (кадры before/after, логи сборки, этот отчёт)"
if [ "${GATE8_KEEP:-0}" = "1" ]; then
    say "клон        : $WORK (оставлен по GATE8_KEEP=1)"
else
    rm -rf "$WORK"
    say "клон        : удалён — правка жила только в нём, рабочее дерево не тронуто"
fi
say "гейт 8: $([ "$FAILS" -eq 0 ] && echo PASS || echo FAIL) (провалов: $FAILS)"
[ "$FAILS" -eq 0 ]
