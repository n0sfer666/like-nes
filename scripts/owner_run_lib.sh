# shellcheck shell=bash

OWNER_RUN_LINES='^neon-rumble: ((seat|hit|react) tick |brawl tick [1-9])'

owner_run_directive() {
    sed -n "s/^! $2 //p" "$1" | tr -d '\r'
}

owner_run_expect() {
    local keys=$1 out=$2 exp
    exp=$(owner_run_directive "$keys" expect)
    [ -n "$exp" ] || return 0
    printf '%s\n' "$exp" | awk '
        NR == FNR { want[++n] = $0; next }
        got < n && $0 ~ want[got + 1] { got++ }
        END {
            for (i = 1; i <= got; i++) print "ok    expect " want[i]
            if (got < n) { print "FAIL  expect " want[got + 1] " (and " n - got - 1 " after it)"; exit 1 }
        }
    ' - "$out"
}

owner_run_absent() {
    local keys=$1 out=$2 re rc=0
    while IFS= read -r re; do
        [ -n "$re" ] || continue
        if grep -Eq -- "$re" "$out"; then
            printf 'FAIL  absent %s: %s\n' "$re" "$(grep -E -m1 -- "$re" "$out")"
            rc=1
        else
            printf 'ok    absent %s\n' "$re"
        fi
    done <<EOF
$(owner_run_directive "$keys" absent)
EOF
    return $rc
}

owner_run_same() {
    local window=$1 headless=$2 d
    d=$(diff <(grep -E "$OWNER_RUN_LINES" "$headless") <(grep -E "$OWNER_RUN_LINES" "$window"))
    if [ -z "$d" ] && grep -Eq "$OWNER_RUN_LINES" "$headless"; then
        printf 'ok    same as headless: %s line(s)\n' "$(grep -Ec "$OWNER_RUN_LINES" "$headless")"
        return 0
    fi
    printf 'FAIL  differs from headless (< headless, > window):\n%s\n' "${d:-  no lines to compare}"
    return 1
}

owner_run_strip_cr() {
    local f
    for f in "$@"; do
        [ -f "$f" ] || continue
        tr -d '\r' < "$f" > "$f.lf" && mv "$f.lf" "$f" || return 1
    done
}

owner_run_shots() {
    local keys=$1 out=$2 want got missing
    want=$(tr -d '\r' < "$keys" | awk '$1 ~ /^[#!]/ { next } { i = $1 == "menu" ? 3 : 2 } $i == "shot" { print $(i + 1) }' | sort)
    got=$(sed -n 's/^shot \([^ ]*\) .*/\1/p' "$out" | sort)
    missing=$(comm -23 <(printf '%s\n' "$want") <(printf '%s\n' "$got") | grep -v '^$')
    if [ -n "$missing" ]; then
        printf 'FAIL  frames not written: %s\n' "$(printf '%s' "$missing" | tr '\n' ' ')"
        return 1
    fi
    [ -z "$want" ] || printf 'ok    frames: %s\n' "$(printf '%s\n' "$want" | grep -c .)"
}

owner_run_judge() {
    local keys=$1 out=$2 headless=$3 rc=0
    owner_run_shots "$keys" "$out" || rc=1
    owner_run_expect "$keys" "$out" || rc=1
    owner_run_absent "$keys" "$out" || rc=1
    if [ -n "$headless" ]; then owner_run_same "$out" "$headless" || rc=1; fi
    return $rc
}

owner_run_html_escape() {
    sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g'
}

owner_run_sheet() {
    local out=$1 run=$2 dir name keys shot frame tick look
    printf '<!doctype html>\n<meta charset="utf-8">\n<title>owner run %s</title>\n' "$run"
    printf '<style>body{background:#111;color:#ddd;font:14px monospace;margin:16px}img{max-width:100%%;display:block}'
    printf 'figure{margin:0 0 24px}figcaption{padding:4px 0;color:#fc6}pre{white-space:pre-wrap}</style>\n'
    printf '<h1>%s</h1>\n<pre>' "$run"
    owner_run_html_escape < "$out/summary.txt"
    printf '</pre>\n'
    for dir in "$out"/*/; do
        name=$(basename "$dir")
        keys=$dir/$name.keys
        [ -f "$keys" ] || continue
        printf '<h2 id="%s">%s</h2>\n<pre>' "$name" "$name"
        owner_run_html_escape < "$dir/verdict.txt"
        printf '</pre>\n'
        while read -r _ shot _ frame _ tick; do
            look=$(owner_run_directive "$keys" look | awk -v n="$shot" '$1 == n { sub(/^[^ ]+ +/, ""); print }')
            printf '<figure><img src="%s/%s.png" loading="lazy"><figcaption>%s - tick %s, frame %s: ' \
                "$name" "$shot" "$shot" "$tick" "$frame"
            printf '%s' "$look" | owner_run_html_escape
            printf '</figcaption></figure>\n'
        done < <(grep '^shot ' "$dir/window.out")
        printf '<details><summary>hit, react, seat</summary><pre>'
        grep -E "$OWNER_RUN_LINES" "$dir/window.out" | owner_run_html_escape
        printf '</pre></details>\n'
    done
}

OWNER_RUN_REF=refs/owner-runs/main

owner_run_publish() (
    out=${1%/} run=$2
    [ -n "$out" ] && [ -d "$out" ] || { echo "owner-run: no report folder '$out'" >&2; exit 1; }
    ! command -v cygpath >/dev/null 2>&1 || out=$(cygpath -m "$out")
    idx=$out.index list=$out.files fetched=refs/owner-runs/fetched
    trap 'rm -f "$idx" "$list"' EXIT
    (cd "$out" && find . -type f | sed 's|^\./||' | sort) > "$list"
    for try in 1 2 3; do
        parent=""
        git fetch -q origin "+$OWNER_RUN_REF:$fetched" 2>/dev/null && parent=$(git rev-parse "$fetched")
        rm -f "$idx"
        [ -z "$parent" ] || GIT_INDEX_FILE=$idx git read-tree "$parent" || exit 1
        sed "s|^|$out/|" "$list" | git hash-object -w --stdin-paths \
            | paste - "$list" | awk -F'\t' -v r="$run" '{ print "100644 " $1 "\t" r "/" $2 }' \
            | GIT_INDEX_FILE=$idx git update-index --add --index-info || exit 1
        commit=$(git commit-tree "$(GIT_INDEX_FILE=$idx git write-tree)" ${parent:+-p "$parent"} \
            -m "owner run $run at $(git rev-parse --short HEAD)" \
            -m "Signed-off-by: $(git config user.name) <$(git config user.email)>") || exit 1
        git push -q origin "$commit:$OWNER_RUN_REF" 2>/dev/null && { printf '%s\n' "$commit"; exit 0; }
        echo "owner-run: push attempt $try rejected, refetching" >&2
    done
    exit 1
)

owner_run_manual() {
    local doc=$1 covered
    shift
    covered=$(for k in "$@"; do owner_run_directive "$k" gate; done | tr ' ' '\n' | sort -u | tr '\n' ' ')
    owner_gates_open_lines "$doc" | awk -v c=" $covered" -v s="(§" '
        { n = substr($0, index($0, s) + length(s)); sub(/ .*$/, "", n) }
        index(c, " " n " ") { sheet = sheet (sheet == "" ? "" : ", ") "§" n }
        { print }
        END { if (sheet != "") print "  scenarios drive " sheet " - read every frame of the sheet, the rest of the gate is above" }
    '
}
