#!/usr/bin/env bash
# Прогон на машине разработчика: всё, что раннер GitHub проверить не может — реальный GPU-драйвер,
# реальная десктоп-сессия (X11/Wayland), реальный MSVC, реальное железо ввода.
#
# Скрипт закрывает АВТОМАТИЗИРУЕМУЮ половину: паспорт машины, сборочный гейт, прогон всех тестов
# дерева и замер цикла правка→сборка→hot-reload (гейт 8 спеки #13 требует записать его как факт
# для Linux и Windows). Ручная половина — скриншоты, гизмо, пад — в docs/owner-verification.md.
#
# Запуск:
#   bash scripts/owner_check.sh
# Windows — из x64 Native Tools Command Prompt for VS (там 64-битный cl.exe), шеллом git-bash:
#   "C:\Program Files\Git\bin\bash.exe" scripts/owner_check.sh
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1
# shellcheck source=scripts/owner_gates_lib.sh
. "$ROOT/scripts/owner_gates_lib.sh"
# shellcheck source=scripts/redact_home_lib.sh
. "$ROOT/scripts/redact_home_lib.sh"

BUILD_DIR=${BUILD_DIR:-build}
OS_TAG=$(uname -s | tr '[:upper:]' '[:lower:]' | tr -d ' ')
REPORT="$ROOT/$BUILD_DIR/owner-report-$OS_TAG.txt"
REPORT_SHOWN="$BUILD_DIR/owner-report-$OS_TAG.txt"
mkdir -p "$BUILD_DIR"
: > "$REPORT"
# Отчёт владелец присылает целиком — маскировка домашнего каталога в нём на выходе, при любом исходе.
trap 'redact_home_file "$REPORT"' EXIT

say() { printf '%s\n' "$*" | tee -a "$REPORT"; }
head_() { printf '\n=== %s\n' "$*" | tee -a "$REPORT"; }
have() { command -v "$1" >/dev/null 2>&1; }

# Интерпретатор ищется прогоном, а не наличием в PATH: на Windows `python3` — это чаще всего
# заглушка Microsoft Store, которая открывает магазин и выходит ненулём, а настоящий питон зовётся
# `python` или `py`. Проверка `-c` отличает одно от другого.
PY=""
for cand in python3 python py; do
    if have "$cand" && "$cand" -c 'import sys' >/dev/null 2>&1; then PY=$cand; break; fi
done

head_ "Паспорт машины"
say "date        : $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
say "os          : $(uname -s -r -m)"
say "commit      : $(git rev-parse HEAD 2>/dev/null || echo '?') ($(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo '?'))"
say "cmake       : $(cmake --version 2>/dev/null | head -1)"
say "python      : ${PY:-НЕ НАЙДЕН}$([ -n "$PY" ] && printf ' (%s)' "$("$PY" --version 2>&1)")"
say "ninja       : $(ninja --version 2>/dev/null || echo 'нет')"
# Компилятор спрашивается у CMake, а не угадывается: на Windows его выбирает vcvars, и версия
# из PATH сказала бы про git-bash, а не про тот cl.exe, которым собрано дерево.
say "compiler    : $(grep -m1 'CMAKE_CXX_COMPILER:' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2- || echo 'каталог сборки ещё не сконфигурирован')"
# Разрядность — по той же причине, но из другого файла: CMAKE_SIZEOF_VOID_P не кешируется, CMake
# выводит его при каждом конфигурировании из размера указателя компилятора. 32-битный тулчейн на
# Windows выбирается сам собой (обычный Developer Command Prompt), а проявляется как C4244 в нашем
# render/capture.cpp — то есть виноватым выглядит движок.
PTR=$(grep -h 'CMAKE_CXX_SIZEOF_DATA_PTR "' "$BUILD_DIR"/CMakeFiles/*/CMakeCXXCompiler.cmake 2>/dev/null | head -1 | tr -dc '0-9')
say "pointer     : ${PTR:-?} байт$([ "$PTR" = "4" ] && printf ' — 32-битный тулчейн, нужен x64 Native Tools Command Prompt for VS')"
# Сокеты дисплея — только «задан/не задан»: отчёт уезжает в публичный PR (аудит #21 A·3·12).
set_or_unset() { if [ -n "$1" ]; then printf set; else printf unset; fi; }
say "session     : XDG_SESSION_TYPE='${XDG_SESSION_TYPE:-}' WAYLAND_DISPLAY=$(set_or_unset "${WAYLAND_DISPLAY:-}") DISPLAY=$(set_or_unset "${DISPLAY:-}")"
# Дистрибутив — не украшение отчёта: имена пакетов и умолчания сессии у Fedora/Nobara, Arch и
# Debian разные, и «гейт 6 не воспроизвёлся» читается только вместе с тем, где он гонялся.
if [ "$OS_TAG" = "linux" ] && [ -r /etc/os-release ]; then
    say "distro      : $(. /etc/os-release; printf '%s' "${PRETTY_NAME:-${NAME:-?}}")"
fi
# Подсказку по установке даём той командой, которая на этой машине есть: совет про apt-get на
# Fedora выглядит как «мануал писали не для меня» и тратит время владельца на перевод.
install_cmd() {
    if have dnf; then printf 'sudo dnf install'
    elif have pacman; then printf 'sudo pacman -S'
    elif have apt-get; then printf 'sudo apt-get install'
    else printf 'поставить пакет'; fi
}
if have vulkaninfo; then
    # Не `… | grep -m1 … || echo 'нет устройств'`: grep уходит по первому совпадению, vulkaninfo
    # получает SIGPIPE, и под `pipefail` успешный конвейер отдаёт ненулевой код — ветка `||`
    # срабатывала ВМЕСТЕ с найденным именем, дописывая «нет устройств» к живому устройству.
    # Отдельная переменная снимает вопрос: пусто и есть «не нашли».
    VK=$(vulkaninfo --summary 2>/dev/null | grep -m1 -i 'deviceName' | sed 's/^[[:space:]]*//')
    say "vulkan      : ${VK:-нет устройств}"
elif [ "$OS_TAG" = "linux" ]; then
    say "vulkan      : vulkaninfo не установлен ($(install_cmd) vulkan-tools) — гейт 6 без него слеп"
fi
if [ "$OS_TAG" = "linux" ]; then
    PADS=$(ls /dev/input/js* /dev/input/event* 2>/dev/null | tr '\n' ' ')
    say "input dev   : ${PADS:-нет узлов /dev/input (пад не подключён или нет прав)}"
fi
case "$OS_TAG" in
    mingw*|msys*|cygwin*)
        # rc.exe и mt.exe приносит Windows SDK — отдельный компонент, а в PATH их кладёт vcvars.
        # Пропажа любого из двух выглядит как «cl.exe не может собрать простую программу»: CMake
        # видит только неудачную линковку пробника и про SDK в этом сообщении не говорит ничего.
        say "windows sdk : rc=$(command -v rc || echo 'НЕ НАЙДЕН') mt=$(command -v mt || echo 'НЕ НАЙДЕН')"
        if ! have rc || ! have mt; then
            say "              ^ либо шелл запущен не из developer-консоли VS, либо в VS"
            say "                Installer не отмечен компонент Windows 11 SDK"
        fi
        ;;
esac

STAGES_FAILED=()
stage() {
    local title=$1; shift
    head_ "$title"
    if "$@" >>"$REPORT" 2>&1; then
        say "--- OK: $title"
    else
        say "--- ПРОВАЛ: $title (подробности выше в $REPORT_SHOWN)"
        STAGES_FAILED+=("$title")
    fi
}

stage "Сборочный гейт (ноль ошибок, ноль предупреждений)" bash scripts/build_check.sh
if [ -n "$PY" ]; then
    stage "Линтер workflow — самопроверка правил" "$PY" scripts/ci_lint.py --selftest
    stage "Линтер workflow" "$PY" scripts/ci_lint.py
else
    head_ "Линтер workflow"
    say "--- ПРОВАЛ: питона нет (Windows: установить python.org и перезапустить шелл) — гейт не прогнан"
    STAGES_FAILED+=("Линтер workflow")
fi

# Обход тестов дерева — отдельным файлом: длина этого скрипта упёрлась в жёсткий лимит, а обход
# самодостаточен и имеет своё имя. Подключается, а не запускается: вердикт внизу считает по его
# массивам, а у дочернего процесса их не занять.
# shellcheck source=scripts/owner_check_tests.sh
. "$ROOT/scripts/owner_check_tests.sh"

# Замер цикла правка→сборка→hot-reload. Гейт 8 спеки #13 требует записать его фактом для Linux и
# Windows: в CI число измеряет буферизацию логов раннера, а не сборку.
head_ "Цикл правка→сборка→hot-reload (build_loop_test, 3 прогона)"
LOOP_BIN="$BUILD_DIR/build_loop_test"
[ -x "$LOOP_BIN" ] || LOOP_BIN="$BUILD_DIR/build_loop_test.exe"
if [ -x "$LOOP_BIN" ] && [ -n "$PY" ]; then
    "$PY" - "$LOOP_BIN" <<'PY' | tee -a "$REPORT"
import os, subprocess, sys, time
binary = os.path.abspath(sys.argv[1])
times, bad = [], 0
for i in range(3):
    t0 = time.perf_counter()
    r = subprocess.run([binary], capture_output=True, text=True)
    dt = time.perf_counter() - t0
    times.append(dt)
    print("  run %d: %.2f s (%s)" % (i + 1, dt, "PASS" if r.returncode == 0 else "FAIL"))
    if r.returncode != 0:
        bad += 1
        print(r.stdout[-800:])
print("  best: %.2f s, median: %.2f s" % (min(times), sorted(times)[1]))
sys.exit(1 if bad else 0)
PY
    [ "${PIPESTATUS[0]}" -eq 0 ] || STAGES_FAILED+=("Цикл правка→сборка→hot-reload")
elif [ -z "$PY" ]; then
    say "  замер пропущен: питона нет — число для гейта 8 спеки #13 не снято"
else
    say "  build_loop_test не собран — цель живёт под IDE_POC=ON"
fi

# Цена кадра — гейт 8 спеки #15 и гейт 7 спеки #16. Обе цели прогоняются и общим циклом выше, но
# там от них остаётся одно слово PASS: оно говорит, что счётчики совпали с эталоном, и молчит про
# время, а судить время может только эта машина. Вынесено внешней командой по тому же основанию,
# что этапы `preflight.sh`: числа владельцу нужны и сами по себе, без всего прогона.
head_ "Цена кадра (owner_perf.sh: шаг физики и тик персонажа)"
# Вердикт берётся из PIPESTATUS, а не из статуса конвейера: `tee` возвращает свой код, и без этого
# этап, напечатавший «FAIL: …», уходил бы в отчёт зелёным — гейт был бы декорацией.
BUILD_DIR="$BUILD_DIR" bash scripts/owner_perf.sh 2>&1 | tee -a "$REPORT"
[ "${PIPESTATUS[0]}" -eq 0 ] || STAGES_FAILED+=("Цена кадра (owner_perf.sh)")

head_ "Цена кадра сети (owner_net_budget.sh: пара пиров по петле, гейт 8 спеки #22)"
BUILD_DIR="$BUILD_DIR" bash scripts/owner_net_budget.sh 2>&1 | tee -a "$REPORT"
[ "${PIPESTATUS[0]}" -eq 0 ] || STAGES_FAILED+=("Цена кадра сети (owner_net_budget.sh)")

# Список выводится ИЗ документа (сверяет check_owner_gates.sh), а не пишется здесь: рукописная копия
# уже разъехалась — звала гейт 9 спеки #17 открытым четыре дня после закрытия и молчала о шести.
head_ "Ручная половина"
say "Осталось глазами и руками — $OWNER_DOC:"
owner_gates_open_lines "$ROOT/$OWNER_DOC" | tee -a "$REPORT"
owner_gates_closed_lines "$ROOT/$OWNER_DOC" | tee -a "$REPORT"

printf '\n' | tee -a "$REPORT"
# Незапущенное считается наравне с провалившимся, и в вердикте названо СВОИМ числом. Зелёный при
# непустом BLOCKED означал бы «половина целей не проверена, зато красиво»; одно общее число вернуло
# бы ту же неразличимость, ради устранения которой этот счётчик и заведён.
if [ ${#STAGES_FAILED[@]} -eq 0 ] && [ ${#FAILED_TESTS[@]} -eq 0 ] && [ ${#BLOCKED_TESTS[@]} -eq 0 ] &&
   [ ${#STALE_TESTS[@]} -eq 0 ]
then
    say "owner-check: PASS — автоматизируемая половина зелёная на этой машине"
    say "отчёт: $REPORT_SHOWN"
    exit 0
fi
say "owner-check: FAIL — этапов ${#STAGES_FAILED[@]}, тестов ${#FAILED_TESTS[@]}, не запущено $((${#BLOCKED_TESTS[@]} + ${#STALE_TESTS[@]}))"
say "отчёт: $REPORT_SHOWN"
exit 1
