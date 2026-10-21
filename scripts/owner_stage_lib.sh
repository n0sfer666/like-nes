# shellcheck shell=bash

OWNER_STAGE_SKIP=3

owner_stage_skip() {
    printf 'skip  %s\n' "$2" >> "$1/verdict.txt"
    return "$OWNER_STAGE_SKIP"
}

owner_stage_end() {
    local dir=$1 name=$2 gates=$3 rc=$4
    case $rc in
        0) printf '%-5s %s%s\n' PASS "$name" "${gates:+ (gates $gates)}" ;;
        "$OWNER_STAGE_SKIP") printf '%-5s %s - %s\n' SKIP "$name" "$(sed -n 's/^skip  //p' "$dir/verdict.txt" | head -1)" ;;
        *) printf '%-5s %s%s\n' FAIL "$name" "${gates:+ (gates $gates)}" ;;
    esac > "$dir/summary"
    [ -z "$gates" ] || [ "$rc" = "$OWNER_STAGE_SKIP" ] || printf '%s\n' "$gates" > "$dir/gates"
    [ "$rc" = 0 ] || [ "$rc" = "$OWNER_STAGE_SKIP" ]
}

owner_stage_look() {
    printf '%s\t%s\n' "$2" "$3" >> "$1/looks"
}

owner_stage_sheet() {
    local dir=${1%/} name file look
    name=$(basename "$dir")
    printf '<h2 id="%s">%s</h2>\n<pre>' "$name" "$name"
    owner_run_html_escape < "$dir/verdict.txt"
    printf '</pre>\n'
    [ -f "$dir/looks" ] || return 0
    while IFS="$(printf '\t')" read -r file look; do
        [ -f "$dir/$file" ] || continue
        printf '<figure><img src="%s/%s" loading="lazy"><figcaption>%s: ' "$name" "$file" "${file%.png}"
        printf '%s' "$look" | owner_run_html_escape
        printf '</figcaption></figure>\n'
    done < "$dir/looks"
}
