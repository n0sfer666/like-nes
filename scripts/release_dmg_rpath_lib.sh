# shellcheck shell=bash
# Как бандл находит рантайм (спека #20, вертикаль 4, шаг A) — своим файлом, а не строкой в
# release_dmg_check_lib.sh: предмет здесь не содержимое тома, а СВЯЗЬ ВНУТРИ Mach-O, и читается он
# единственным инструментом, которого нет больше нигде в наборе (`otool`).

# Механизм, которым бандл находит рантайм, ДВУХСТУПЕНЧАТЫЙ, и утверждение обязано проверять обе
# половины: исполняемый записывает библиотеку как `@rpath/libwgpu_native.dylib` (LC_LOAD_DYLIB) и
# сам же говорит, чем разворачивать `@rpath` (LC_RPATH = `@executable_path`, его ставит
# `reproducible_rpath` из cmake/determinism.cmake). Сломай любую — бандл соберётся, состав, права и
# лицензии сойдутся, а `Library not loaded` увидит владелец на живой машине, а не гейт.
#
# Абсолютный rpath отбивается наравне с отсутствующим: он указывает в каталог сборки, то есть пакет
# работает ровно на той машине, где его собрали, — а это ровно то, ради чего `reproducible_rpath`
# и переопределяет rpath после копирующего помощника.
#
# Про tar.gz здесь не утверждается ничего: связка у него та же, но своя фабрика фикстур, где
# «бинари» — текст, и утверждение без сломанной фикстуры неотличимо от отсутствующего.
assert_dmg_rpath() {
  local mnt="$1" bins="$2" runtime="$3" f base load rp one seen=0 bad_list=""
  [ -n "$runtime" ] || { bad "имя рантайма не названо — утверждать о rpath нечего"; return 1; }
  for f in "$mnt/$bins"/*; do
    [ -f "$f" ] || continue
    base=$(basename "$f")
    [ "$base" != "$runtime" ] || continue
    # Не-Mach-O отбивается ВСЛУХ: `otool` о текстовом файле отвечает «is not an object file» и
    # кодом НОЛЬ, поэтому молчаливый пропуск такого файла сделал бы утверждение вакуумным.
    if ! otool -h "$f" 2>/dev/null | grep -q magic; then
      bad "в $bins лежит не Mach-O: $base — утверждать о rpath нечего"; return 1
    fi
    seen=$((seen + 1))
    load=$(otool -L "$f" | tail -n +2 \
      | awk -v n="$runtime" '{ p = $1; k = p; sub(/.*\//, "", k); if (k == n) print p }' | head -1)
    case "$load" in
      "@rpath/$runtime") : ;;
      "") bad_list="$bad_list $base(не линкует $runtime)" ;;
      *) bad_list="$bad_list $base(линкует $load)" ;;
    esac
    rp=$(otool -l "$f" | awk '/LC_RPATH/ { r = 1 } r && /^ *path / { print $2; r = 0 }')
    [ -n "$rp" ] || bad_list="$bad_list $base(нет LC_RPATH)"
    for one in $rp; do
      case "$one" in
        @executable_path*|@loader_path*) : ;;
        *) bad_list="$bad_list $base(rpath $one)" ;;
      esac
    done
  done
  if [ -n "$bad_list" ]; then bad "рантайм из бандла не находится:$bad_list"; return 1; fi
  [ "$seen" != 0 ] || { bad "в $bins нет ни одного исполняемого — утверждение о rpath вакуумно"; return 1; }
  ok "rpath: $seen исполняемых зовут @rpath/$runtime и разворачивают его рядом с собой"
}
