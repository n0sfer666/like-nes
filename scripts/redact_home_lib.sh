# shellcheck shell=bash
# Домашний каталог в файле, который уходит из машины (отчёт owner_check.sh), заменяется на «~»:
# пути в нём абсолютные, с именем учётной записи ОС (аудит #21 A·3·12). Маскируется файл целиком
# после записи, а не поток каждого этапа: этапы пишут в отчёт десятком способов, и фильтр на
# каждом пропустил бы первый же новый.
#
# Вхождение дома маскируется, только когда оно — целый компонент пути. Справа — разделитель, конец
# строки (`\r` вывода MSVC не в счёт) или знак, которым путь кончается в команде и выводе: пробел,
# кавычка, `)]>,;:`. Слева — от ближайшей такой же границы (двоеточие буквы диска ею не считается)
# до дома допустим только префикс флага или ответного файла (`-I`, `@`) либо `//` URL `file:///`:
# `-I/Users/al/x` маскируется, а `/mnt/backup/Users/al` и `C:/Users/al` при доме `/Users/al` — нет.
# Замена литеральная: `*`, `[` и пробел в доме шаблоном не читаются. Под git-bash дом пишется в
# отчёт тремя формами — `/c/Users/al` от bash, `C:/Users/al` и `C:\Users\al` от cmake, cl и ninja, — и
# все три сравниваются без учёта регистра, как их и читает Windows. Пустой дом и `/` не маскируют
# ничего: иначе `~` встал бы перед каждым путём. Сбой маскировки говорит об этом вслух: молча
# оставленный отчёт с домом уехал бы из машины как замаскированный.
redact_home_forms() {
    local h=${HOME:-}
    while [ "${h%/}" != "$h" ]; do h=${h%/}; done
    [ -n "$h" ] || return 0
    printf '%s\n' "$h"
    command -v cygpath >/dev/null 2>&1 || return 0
    cygpath -m "$h"
    cygpath -w "$h"
}

redact_home_file() {
    local file=$1 forms ci=0
    [ -f "$file" ] || return 0
    forms=$(redact_home_forms)
    [ -n "$forms" ] || return 0
    command -v cygpath >/dev/null 2>&1 && ci=1
    RH_FORMS=$forms RH_CI=$ci awk '
        function bound(c) { return index(" \t=\047\"(<[,;`", c) > 0 }
        function stop(c) { return c == "" || sep(c) || index(" \t\047\")]>,;:`", c) > 0 }
        function sep(c) { return c == "/" || (ci && c == "\\") }
        function drive_colon(s, q) {
            return substr(s, q - 1, 1) ~ /^[A-Za-z]$/ && (q == 2 || bound(substr(s, q - 2, 1)))
        }
        function left_ok(s, p,   q, c) {
            for (q = p - 1; q >= 1; q--) {
                c = substr(s, q, 1)
                if (bound(c) || (c == ":" && !drive_colon(s, q))) break
            }
            c = substr(s, q + 1, p - q - 1)
            return c ~ /^[-@A-Za-z0-9_+]*$/ || (c == "//" && q >= 1 && substr(s, q, 1) == ":")
        }
        function mask(s, h,   L, needle, hay, out, start, i, p, nc) {
            L = length(h); needle = ci ? tolower(h) : h; hay = ci ? tolower(s) : s
            out = ""; start = 1
            while ((i = index(substr(hay, start), needle)) > 0) {
                p = start + i - 1
                nc = substr(s, p + L, 1)
                if (left_ok(s, p) && stop(nc)) {
                    out = out substr(s, start, p - start) "~"; start = p + L
                } else {
                    out = out substr(s, start, p - start + 1); start = p + 1
                }
            }
            return out substr(s, start)
        }
        BEGIN { ci = ENVIRON["RH_CI"] == 1; n = split(ENVIRON["RH_FORMS"], form, "\n") }
        {
            cr = sub(/\r$/, "")
            line = $0
            for (f = 1; f <= n; f++) if (form[f] != "") line = mask(line, form[f])
            printf "%s%s\n", line, (cr ? "\r" : "")
        }
    ' "$file" > "$file.redact" && mv "$file.redact" "$file" && return 0
    rm -f "$file.redact"
    echo "redact-home: отчёт НЕ замаскирован, в $file остался домашний каталог" >&2
    return 1
}
