#!/usr/bin/env bash
# Самопроверка redact_home_lib.sh (аудит #21 A·3·12): отчёт owner_check.sh уходит из машины целиком,
# и домашний каталог в нём обязан стать «~». Кейсы — HOME с символами шаблона и пробелом, граница
# компонента с обеих сторон (префикс флага, `file:///`, кавычки, скобки, запятая), голый HOME в
# конце строки (и перед `\r`), корень и пустой HOME, три формы дома под git-bash, громкий сбой. Формы Windows на других ОС даёт подставной `cygpath`; на Windows-раннере
# сверх того проверяется настоящий.
set -uo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/redact_home_lib.sh
. "$ROOT/scripts/redact_home_lib.sh"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fail=0

expect() {
    local label=$1 home=$2 input=$3 want=$4 got
    printf '%s\n' "$input" > "$tmp/r.txt"
    printf '%s\n' "$want" > "$tmp/want.txt"
    HOME=$home redact_home_file "$tmp/r.txt"
    got=$(od -c "$tmp/r.txt" | sed -n '1,4p')
    if cmp -s "$tmp/r.txt" "$tmp/want.txt"; then
        echo "ok   $label"
    else
        echo "FAIL ${label}: байты разошлись, ждали «${want}», получено:"; echo "$got"; fail=1
    fi
}

expect_win() {
    local m=$1 w=$2
    shift 2
    eval "cygpath() { case \$1 in -m) printf '%s\\n' '$m';; -w) printf '%s\\n' '$w';; esac; }"
    expect "$@"
    unset -f cygpath
}

REAL_CYGPATH=$(command -v cygpath || true)
expect "путь под домом" /Users/al "cwd /Users/al/_dev/x" "cwd ~/_dev/x"
expect "голый дом в конце строки" /Users/al "HOME=/Users/al" "HOME=~"
expect "граница компонента" /Users/al "/Users/alice/x /Users/al" "/Users/alice/x ~"
expect "несколько вхождений" /Users/al "/Users/al/a:/Users/al/b" "~/a:~/b"
expect "символы шаблона и пробел в HOME" "/tmp/we ird[1]*" "at /tmp/we ird[1]*/z & \\ q" "at ~/z & \\ q"
expect "строки без дома не тронуты" /Users/al $'a\n\nb' $'a\n\nb'
expect "корень не маскирует" / "/usr/bin" "/usr/bin"
expect "пустой HOME не маскирует" "" "/Users/al/x" "/Users/al/x"
expect "хвостовой слэш у HOME" /Users/al/ "at /Users/al/x" "at ~/x"
expect "путевой символ слева" /Users/al "/mnt/backup/Users/al/x" "/mnt/backup/Users/al/x"
expect "буква диска слева" /Users/al "C:/Users/al/x" "C:/Users/al/x"
expect "голый дом перед CR" /Users/al $'HOME=/Users/al\r\nnext' $'HOME=~\r\nnext'
expect_win 'C:/Users/al' 'C:\Users\al' "git-bash: три формы, регистр" /c/Users/al \
    'a /c/Users/al/x b=c:/users/AL/y "C:\Users\al\z"' 'a ~/x b=~/y "~\z"'
expect_win 'C:/Users/al' 'C:\Users\al' "git-bash: граница с обратным слэшем" /c/Users/al \
    'C:\Users\alice\q C:\Users\al' 'C:\Users\alice\q ~'
expect "префикс флага -I" /Users/al "cc -I/Users/al/x a.c" "cc -I~/x a.c"
expect_win 'C:/Users/al' 'C:\Users\al' "префикс флага перед буквой диска" /c/Users/al \
    'cl -IC:\Users\al\x a.c' 'cl -I~\x a.c'
expect "ответный файл @" /Users/al "ld @/Users/al/rsp" "ld @~/rsp"
expect "URL file:///" /Users/al "open file:///Users/al/x" "open file://~/x"
expect "дом в одинарных кавычках" /Users/al "HOME='/Users/al'" "HOME='~'"
expect "дом перед пробелом" /Users/al "cd /Users/al && make" "cd ~ && make"
expect "дом в скобках" /Users/al "see (/Users/al)" "see (~)"
expect "дом перед запятой" /Users/al "key: /Users/al, next" "key: ~, next"
expect "каталог-сосед с префиксом" /Users/al "/Users/al.bak/x /Users/al-2" "/Users/al.bak/x /Users/al-2"

printf 'at /Users/al/x\n' > "$tmp/f.txt"
awk() { return 1; }
err=$(HOME=/Users/al redact_home_file "$tmp/f.txt" 2>&1); rc=$?
unset -f awk
if [ "$rc" -ne 0 ] && [[ $err == *"НЕ замаскирован"* ]] && [ ! -e "$tmp/f.txt.redact" ]; then
    echo "ok   сбой маскировки — вслух, без временного файла"
else
    echo "FAIL сбой маскировки: rc=${rc}, «${err}»"; fail=1
fi

if [ -n "$REAL_CYGPATH" ]; then
    home_w=$(cygpath -w "$HOME")
    expect "настоящий cygpath: форма Windows" "$HOME" "cl ${home_w}\\a.cpp" "cl ~\\a.cpp"
fi

[ "$fail" -eq 0 ] && echo "redact-home selftest: PASS" || echo "redact-home selftest: FAIL"
exit "$fail"
