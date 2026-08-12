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

BUILD_DIR=${BUILD_DIR:-build}
OS_TAG=$(uname -s | tr '[:upper:]' '[:lower:]' | tr -d ' ')
REPORT="$ROOT/$BUILD_DIR/owner-report-$OS_TAG.txt"
mkdir -p "$BUILD_DIR"
: > "$REPORT"

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
say "session     : XDG_SESSION_TYPE='${XDG_SESSION_TYPE:-}' WAYLAND_DISPLAY='${WAYLAND_DISPLAY:-}' DISPLAY='${DISPLAY:-}'"
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
        say "--- ПРОВАЛ: $title (подробности выше в $REPORT)"
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

# Тесты берутся ИЗ КАТАЛОГА СБОРКИ, а не списком в скрипте: список разошёлся бы с деревом молча,
# и пропавшая цель читалась бы как «всё зелёное».
head_ "Тесты дерева"
PASSED=0; FAILED_TESTS=(); SKIPPED=()
for t in "$BUILD_DIR"/*_test "$BUILD_DIR"/*_test.exe; do
    [ -x "$t" ] || continue
    name=$(basename "$t")
    # Сопоставление идёт по имени БЕЗ `.exe`: точное имя в списке ниже на Windows не совпадало,
    # шесть тестов запускались без обязательных аргументов, печатали `usage:` и делали вердикт
    # ложно красным. Совпадали только шаблоны с `*` — то есть список молча работал наполовину.
    key=${name%.exe}
    # Пропуск НАЗЫВАЕТСЯ вслух — молчаливый читался бы как «всё прогнано». Две причины, и обе
    # про вход, а не про результат: интерактивные цели и тесты, которым нужны пути к плагинам,
    # бандлам и дочерним процессам. Последние CI зовёт с аргументами на всех трёх ОС — повторять
    # их вызовы здесь значило бы завести второй список путей, расходящийся с рабочим молча.
    case "$key" in
        *_probe*|*_bench*|input_demo*)
            SKIPPED+=("$name — интерактивный / замер"); continue;;
        ach_plugin_test|ach_sim_test|ach_steam_test|asset_test|asset_determinism_test|play_spawn_test|plugin_*)
            SKIPPED+=("$name — нужны пути к плагинам/бандлам, вызов живёт в ci.yml"); continue;;
    esac
    if out=$("$t" 2>&1); then
        PASSED=$((PASSED + 1))
        printf '  PASS %s\n' "$name" | tee -a "$REPORT"
    else
        FAILED_TESTS+=("$name")
        printf '  FAIL %s\n' "$name" | tee -a "$REPORT"
        printf '%s\n' "$out" | tail -20 >>"$REPORT"
    fi
done
say "тестов пройдено: $PASSED, провалов: ${#FAILED_TESTS[@]}"
for s in "${SKIPPED[@]:-}"; do [ -n "$s" ] && say "  SKIP $s"; done
for f in "${FAILED_TESTS[@]:-}"; do [ -n "$f" ] && say "  FAIL $f"; done

# Замер цикла правка→сборка→hot-reload. Гейт 8 спеки #13 требует записать его фактом для Linux и
# Windows: в CI число измеряет буферизацию логов раннера, а не сборку.
head_ "Цикл правка→сборка→hot-reload (build_loop_test, 3 прогона)"
LOOP_BIN="$BUILD_DIR/build_loop_test"
[ -x "$LOOP_BIN" ] || LOOP_BIN="$BUILD_DIR/build_loop_test.exe"
if [ -x "$LOOP_BIN" ] && [ -n "$PY" ]; then
    "$PY" - "$LOOP_BIN" <<'PY' | tee -a "$REPORT"
import subprocess, sys, time
binary = sys.argv[1]
times = []
for i in range(3):
    t0 = time.perf_counter()
    r = subprocess.run([binary], capture_output=True, text=True)
    dt = time.perf_counter() - t0
    times.append(dt)
    print("  run %d: %.2f s (%s)" % (i + 1, dt, "PASS" if r.returncode == 0 else "FAIL"))
    if r.returncode != 0:
        print(r.stdout[-800:])
print("  best: %.2f s, median: %.2f s" % (min(times), sorted(times)[1]))
PY
elif [ -z "$PY" ]; then
    say "  замер пропущен: питона нет — число для гейта 8 спеки #13 не снято"
else
    say "  build_loop_test не собран — цель живёт под IDE_POC=ON"
fi

# Цена шага физики — гейт 8 спеки #15. Цель прогоняется и общим циклом выше, но там от неё остаётся
# одно слово PASS: оно говорит, что счётчики совпали с эталоном, и молчит про время. Время и есть
# то, ради чего этап заведён отдельно, — судить его может только эта машина, а не раннер, и в отчёт
# оно обязано попасть числом. Заодно в отчёте оказываются сами счётчики: расхождение между ОС валит
# гейт само, но в отчёте обязано быть ВИДНО, каким числом именно.
head_ "Цена шага физики (framework_physics_perf_test, гейт 8 спеки #15)"
PERF_BIN="$BUILD_DIR/framework_physics_perf_test"
[ -x "$PERF_BIN" ] || PERF_BIN="$BUILD_DIR/framework_physics_perf_test.exe"
if [ -x "$PERF_BIN" ]; then
    # Вердикт берётся из PIPESTATUS, а не из статуса конвейера: `tee` возвращает свой код, и без
    # этого этап, напечатавший «FAIL: …», уходил бы в отчёт зелёным — гейт был бы декорацией.
    "$PERF_BIN" | tee -a "$REPORT"
    [ "${PIPESTATUS[0]}" -eq 0 ] || STAGES_FAILED+=("Цена шага физики (framework_physics_perf_test)")
    say "  бюджет кадра 16.67 мс; на Apple M3 Pro куча — 3.64 мс, 22% (было 2.25 мс до 16 итераций)"
    say "  счётчики закреплены в бинаре: расхождение печатает FAIL само, вправе гулять worst/mean"
else
    say "  framework_physics_perf_test не собран — цена шага на этой машине не снята"
fi

head_ "Ручная половина"
say "Осталось глазами и руками — docs/owner-verification.md:"
say "  #13 гейт 6 — редактор под X11 И под Wayland (-DLINUX_WAYLAND=ON), скриншот каждой сессии"
say "  #13 гейт 8 — сквозной сценарий: правка кода игры → hot-reload → изменение видно"
say "  #14 гейт 8 — framework_input_probe: паспорт пада, профиль, перебинд, отключение на ходу"
say "  #15 гейт 8 — судить время из этапа выше против бюджета кадра: раннер его печатает, но не судит"

printf '\n' | tee -a "$REPORT"
if [ ${#STAGES_FAILED[@]} -eq 0 ] && [ ${#FAILED_TESTS[@]} -eq 0 ]; then
    say "owner-check: PASS — автоматизируемая половина зелёная на этой машине"
    say "отчёт: $REPORT"
    exit 0
fi
say "owner-check: FAIL — этапов ${#STAGES_FAILED[@]}, тестов ${#FAILED_TESTS[@]}"
say "отчёт: $REPORT"
exit 1
