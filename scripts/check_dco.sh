#!/usr/bin/env bash
# Подпись DCO на коммитах ветки — то же утверждение, что у job'а `.github/workflows/dco.yml`.
#
# Стоит отдельным скриптом и первым этапом preflight по одной причине: это единственный job CI,
# который живёт не в `ci.yml` и срабатывает ТОЛЬКО на pull_request. Коммит без `-s` поэтому лежит
# в ветке незамеченным до самого раунда и всплывает поверх всей проделанной работы, а чинится ровно
# одним способом — rebase + force-push, то есть переписыванием ВСЕХ SHA ветки, после чего протухают
# ссылки на коммиты в ADR, теле PR и dev-log. Полсекунды здесь против переписанной истории там.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1

base_ref=main
git rev-parse --verify -q origin/main >/dev/null && base_ref=origin/main
if ! base=$(git merge-base "$base_ref" HEAD 2>/dev/null); then
    echo "база $base_ref недоступна — проверять подпись не от чего"
    exit 1
fi

shas=$(git rev-list --no-merges "$base"..HEAD)
if [ -z "$shas" ]; then
    echo "поверх $base_ref нет коммитов — подписывать нечего"
    exit 0
fi

# Автор ИЛИ коммиттер — тот же критерий, что у job'а: локально строже значило бы отбивать то,
# что CI пропускает, и расхождение пришлось бы разбирать на раннере.
fail=0
n=0
for sha in $shas; do
    n=$((n + 1))
    author=$(git show -s --format='%an <%ae>' "$sha" | tr '[:upper:]' '[:lower:]')
    committer=$(git show -s --format='%cn <%ce>' "$sha" | tr '[:upper:]' '[:lower:]')
    signers=$(git show -s --format='%(trailers:key=Signed-off-by,valueonly,unfold)' "$sha" |
        tr -d '\r' | tr '[:upper:]' '[:lower:]')
    if [ -z "$signers" ]; then
        printf '  %s — нет Signed-off-by\n' "$(git show -s --format='%h %s' "$sha")"
        fail=1
    elif ! grep -qxF "$author" <<<"$signers" && ! grep -qxF "$committer" <<<"$signers"; then
        printf '  %s — подпись не совпадает ни с автором, ни с коммиттером\n' \
            "$(git show -s --format='%h %s' "$sha")"
        fail=1
    fi
done

if [ "$fail" -ne 0 ]; then
    printf 'Починка: git rebase --signoff %s && git push --force-with-lease\n' "$base"
    exit 1
fi
printf 'подписаны все %d коммит(ов) поверх %s\n' "$n" "$base_ref"
