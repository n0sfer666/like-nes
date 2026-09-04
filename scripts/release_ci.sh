#!/usr/bin/env bash
# Пакет Windows забирается из CI (спека #20, вертикаль 3, решение 1). Нативный Windows-пакет на
# macOS честно не собрать — нужен MSVC, — поэтому оркестратор не «кросс-компилирует», а ждёт
# прогон .github/workflows/release_engine.yml, скачивает его артефакт и проверяет, что доехал ТОТ
# пакет: прогон на нашем коммите → артефакт этого прогона → тот же коммит в штампе внутри.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=scripts/release_lib.sh
. "$ROOT/scripts/release_lib.sh"
# shellcheck source=scripts/release_ci_lib.sh
. "$ROOT/scripts/release_ci_lib.sh"
# shellcheck source=scripts/release_check_lib.sh
. "$ROOT/scripts/release_check_lib.sh"
# shellcheck source=scripts/release_ci_check_lib.sh
. "$ROOT/scripts/release_ci_check_lib.sh"

WORKFLOW="$ROOT/.github/workflows/release_engine.yml"
VERSION=""
OUT="$ROOT/release"
DISPATCH=""
REF=""
DEADLINE_MIN=${LIKE_NES_CI_DEADLINE_MIN:-70}
GH=${LIKE_NES_GH:-gh}

usage() {
  cat <<'USAGE'
usage: scripts/release_ci.sh [--version vX.Y.Z] [--out DIR] [--dispatch] [--ref BRANCH]
  зовётся обычно не напрямую, а через scripts/release.sh --only windows
  --dispatch  запустить прогон самому (workflow_dispatch), а не ждать прогон по тегу
  --ref       ветка для --dispatch (умолчание — текущая)
USAGE
}

need_value() {
  [ "$1" -ge 2 ] || { echo "release: $2 требует значения" >&2; usage >&2; exit 2; }
}

while [ $# -gt 0 ]; do
  case "$1" in
    --version) need_value $# "$1"; VERSION="$2"; shift 2 ;;
    --out) need_value $# "$1"; OUT="$2"; shift 2 ;;
    --ref) need_value $# "$1"; REF="$2"; shift 2 ;;
    --dispatch) DISPATCH=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "release: непонятый аргумент: $1" >&2; usage >&2; exit 2 ;;
  esac
done

VERSION=$(resolve_version "$ROOT" "$VERSION") || exit 2

# Адресат у всех трёх запросов ОДИН. `workflow run` шёл с `-R origin`, а `run list`/`run download` —
# без него, то есть по каталогу ВЫЗЫВАЮЩЕГО: запуск `bash ~/_dev/like-nes/scripts/release.sh` из
# чужого каталога отправлял dispatch в наш репозиторий, а опрос — в чужой, и через час выходило
# «прогона не появилось». Тот же класс, что «прогон выбирается по коммиту, а не по тегу»: адресат
# обязан быть один и назван явно.
REPO=$(git -C "$ROOT" remote get-url origin)

# Код 4 — «машина не готова», отдельный и от 2 (опечатка), и от 3 («эта платформа собирается не
# здесь»): у него единственный смысл — доустанови и повтори ту же команду.
RC=0; ci_gh_ready || RC=$?
if [ "$RC" != 0 ]; then ci_gh_hint "$RC"; exit 4; fi

# Коммит определяется ЛОКАЛЬНО и до всякого запроса: он же будет сверяться со штампом внутри
# пакета. Тег на remote может указывать не туда, куда локальный, и узнать об этом по несошедшейся
# цепочке лучше, чем принять чужую сборку за свою.
if [ -n "$DISPATCH" ]; then
  [ -n "$REF" ] || REF=$(git -C "$ROOT" rev-parse --abbrev-ref HEAD)
  SHA=$(git -C "$ROOT" rev-parse HEAD)
  # Прогон собирает то, что лежит на РЕМОУТЕ, а цепочка сверяется с локальным HEAD. Незапушенный
  # коммит поэтому гарантировал час ожидания и вопрос «коммит запушен?» ПОСЛЕ него — при том, что
  # ответ известен одной командой до запуска. Висящий гейт хуже падающего.
  REMOTE_SHA=$(git -C "$ROOT" ls-remote "$REPO" "refs/heads/$REF" | awk '{print $1}')
  if [ -z "$REMOTE_SHA" ]; then
    echo "release: ветки $REF на remote нет — запушь её, иначе прогону нечего собирать" >&2
    exit 1
  fi
  if [ "$REMOTE_SHA" != "$SHA" ]; then
    echo "release: на remote у $REF коммит ${REMOTE_SHA:0:12}, а локально ${SHA:0:12} — прогон собрал бы не то дерево" >&2
    echo "        git push, затем повтори ту же команду" >&2
    exit 1
  fi
else
  SHA=$(git -C "$ROOT" rev-list -n1 "$VERSION" 2>/dev/null || true)
  if [ -z "$SHA" ]; then
    echo "release: тега $VERSION в этом репозитории нет — создай и запушь его, либо запусти прогон вручную: scripts/release_ci.sh --dispatch --version $VERSION" >&2
    exit 1
  fi
fi

ARTIFACT=$(ci_artifact_name "$WORKFLOW") || {
  echo "release: в $WORKFLOW не найден шаг upload-artifact — имя артефакта читать неоткуда" >&2
  exit 1
}
WF=$(basename "$WORKFLOW")
echo "release: жду прогон $WF на коммите ${SHA:0:12} (артефакт $ARTIFACT)"

if [ -n "$DISPATCH" ]; then
  echo "release: запускаю прогон на ветке $REF"
  "$GH" -R "$REPO" workflow run "$WF" --ref "$REF" -f version="$VERSION"
fi

# Дедлайн мерится стенными часами, а узнаём мы что-либо только опросом — те же две величины, что
# разошлись у ci_watch.sh 24 августа. Поэтому опросы СЧИТАЮТСЯ, и их число печатается: «прогон не
# нашёлся» и «мы почти не смотрели» иначе выглядят одинаково.
DEADLINE=$(( $(date +%s) + DEADLINE_MIN * 60 ))
POLLS=0
RUN_ID=""
while :; do
  POLLS=$((POLLS + 1))
  LIST=$(mktemp)
  if "$GH" -R "$REPO" run list --workflow "$WF" --limit 50 \
      --json databaseId,headSha,status,conclusion,createdAt > "$LIST" 2>/dev/null; then
    PICKED=$(ci_pick_run "$LIST" "$SHA" || true)
  else
    PICKED=""
    echo "release: запрос к GitHub не удался (опрос $POLLS)" >&2
  fi
  rm -f "$LIST"
  if [ -n "$PICKED" ]; then
    # Разбор и вердикт — одной функцией из библиотеки: у идущего прогона conclusion пуст, и
    # `set -- $PICKED` с обращением к `"$3"` убивал цикл под `set -u` на первом же таком ответе.
    VERDICT=$(ci_picked_verdict "$PICKED")
    RUN_ID=${VERDICT%% *}
    case "${VERDICT##* }" in
      ok) echo "release: прогон $RUN_ID зелёный (опросов $POLLS)"; break ;;
      fail)
        echo "release: прогон $RUN_ID завершился неуспешно — пакета Windows не будет" >&2
        echo "        смотреть: gh run view $RUN_ID --log-failed" >&2
        exit 1 ;;
      *) : ;;
    esac
  fi
  if [ "$(date +%s)" -ge "$DEADLINE" ]; then
    if [ -z "$RUN_ID" ]; then
      echo "release: за $DEADLINE_MIN мин прогона $WF на коммите ${SHA:0:12} не появилось (опросов $POLLS)" >&2
      echo "        коммит запушен? тег на remote? иначе: scripts/release_ci.sh --dispatch --version $VERSION" >&2
    else
      echo "release: прогон $RUN_ID не досчитал за $DEADLINE_MIN мин (опросов $POLLS)" >&2
    fi
    exit 1
  fi
  sleep 30
done

# Артефакт скачивается в СТЕЙДЖ, а не прямо в каталог версии. Внутри него лежит `release/<версия>/*`
# прогона — пакет, манифест И `SHA256SUMS` с одной строкой (на раннере в каталоге только пакет
# Windows), поэтому скачивание в каталог версии затирало суммы пакетов macOS и Linux, положенных
# туда другими запусками. Ровно тот дефект, от которого release.sh защищается пересчётом по
# фактическому содержимому каталога, — он вернулся с другой стороны.
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
"$GH" -R "$REPO" run download "$RUN_ID" -n "$ARTIFACT" -D "$STAGE"

# Утверждение про ИСХОД, а не про код возврата gh: скачивание пустого артефакта успешно, и каталог
# без пакета при нулевом коде читается ровно как собранный релиз.
PKG=$(find "$STAGE" -maxdepth 1 -name "like-nes-engine-$VERSION-windows-*.tar.gz" | head -1)
if [ -z "$PKG" ]; then
  echo "release: артефакт $ARTIFACT скачан, но пакета Windows в нём нет" >&2
  exit 1
fi

assert_download_intact "$STAGE" "$PKG" || exit 1
assert_ci_chain "$PKG" "$VERSION" "$SHA" || exit 1
assert_ci_package "$ROOT" "$PKG" "$VERSION" || exit 1

DEST="$OUT/$VERSION"
mkdir -p "$DEST"
cp "$PKG" "$DEST/"
# Манифест кладётся рядом, если прогон его положил: он не обязателен для пакета, но им проверяется
# содержимое архива, и терять его при переносе значило бы отдавать Windows-треть без того, что есть
# у двух других.
MANIFEST="$STAGE/$(basename "$PKG" .tar.gz).manifest"
[ ! -f "$MANIFEST" ] || cp "$MANIFEST" "$DEST/"
# Суммы пересчитываются по каталогу — общей функцией с хостовым упаковщиком, а не копией правила и
# не файлом из артефакта: тот знает про одну треть релиза.
write_sums "$DEST"
printf 'release: пакет Windows приехал: %s\n' "$DEST/$(basename "$PKG")"
printf 'суммы:    %s\n' "$DEST/SHA256SUMS"
