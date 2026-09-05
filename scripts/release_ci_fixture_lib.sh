# shellcheck shell=bash
# Фикстурные пакеты пути через CI (спека #20, вертикаль 3). Отдельный файл, потому что потребителей
# два — гейт правил (ему нужен честный пакет, чтобы утверждения о приехавшем вообще нашли предмет) и
# impl-набор (ему нужны сломанные). Копия фабрики у каждого значила бы, что «утверждение отбило
# подмену» проверяется на пакете, который сам собран иначе, чем тот, на котором утверждение проходит.
#
# Состав берётся из expected_files — ТОГО ЖЕ источника, что и сам гейт. Рукописный список файлов
# здесь был бы второй копией install_engine.cmake и разъехался бы с ним молча, оставив фикстуру
# «честной» по вчерашнему составу.
# Порча задаётся ОДНИМ аргументом в форме `drop:<путь>` или `zero:<путь>`: пропавший файл ловит
# утверждение о составе, а файл на нуль байт — только отдельное утверждение о лицензиях, ради
# которого оно от состава и отделено (регресс спеки #9). Одной формой, а не двумя флагами, потому
# что фикстура ломается по одному месту за раз: две порчи разом не дали бы сказать, которая отбита.
ci_make_pkg() {
  local root="$1" dir="$2" version="$3" commit="$4" triple="$5"
  local damage="${6:-}" stamp_triple="${7:-$5}"
  local os stage f pkg mode target imp
  os=$(ci_os_of_triple "$triple") || { printf 'фикстура: незнакомая тройка %s\n' "$triple" >&2; return 1; }
  mode=${damage%%:*}
  target=${damage#*:}
  [ -n "$damage" ] || { mode=""; target=""; }
  stage="$dir/stage"
  mkdir -p "$stage"
  # Цикл кормится подстановкой процесса, а не пайпом: у пайпа тело идёт в ПОДОБОЛОЧКЕ, её код
  # возврата отбрасывается, и фабрика, не создавшая ни одного валидного PE, отдавала бы «честный»
  # пакет — то есть позитивный контроль вертикали становился бы вакуумным.
  while read -r f; do
    [ "$mode" != drop ] || [ "$f" != "$target" ] || continue
    mkdir -p "$stage/$(dirname "$f")"
    if [ "$mode" = zero ] && [ "$f" = "$target" ]; then : > "$stage/$f"; continue; fi
    case "$f" in
      # `-` вместо коммита означает пакет БЕЗ штампа: version.txt не кладётся вовсе, и утверждение о
      # цепочке обязано отличать это от штампа с чужим коммитом.
      */version.txt)
        [ "$commit" != - ] || continue
        printf 'like-nes engine %s\ncommit %s\ntarget %s\n' "$version" "$commit" "$stamp_triple" > "$stage/$f" ;;
      # Фикстурный бинарь Windows — НАСТОЯЩИЙ PE с заданной таблицей импортов: утверждения о CRT
      # читают формат, и текстовая заглушка проверяла бы их отказ разобрать файл, а не пакет.
      # Импорты повторяют жизнь: наши exe статичны, чужой рантайм просит VCRUNTIME140.dll, и она
      # едет рядом.
      *.exe|*.dll)
        imp="--import KERNEL32.dll"
        case "$f" in
          */wgpu_native.dll) imp="$imp --import VCRUNTIME140.dll" ;;
        esac
        [ "$mode" != crt ] || [ "$f" != "$target" ] || imp="$imp --import msvcp140.dll"
        [ "$mode" != dyncrt ] || [ "$f" != "$target" ] || imp="$imp --import VCRUNTIME140.dll"
        [ "$mode" != delaycrt ] || [ "$f" != "$target" ] || imp="$imp --delay msvcp140.dll"
        # shellcheck disable=SC2086
        python3 "$root/scripts/pe_fixture.py" "$stage/$f" $imp || return 1 ;;
      *) printf 'fixture %s\n' "$f" > "$stage/$f" ;;
    esac
  done < <(expected_files "$root" "$os")
  pkg="$dir/like-nes-engine-$version-$triple.tar.gz"
  # Тем же упаковщиком, что настоящий релиз: фикстура, собранная своим tar, проверяла бы формат,
  # которого в природе нет.
  pack_dir "$stage" "$pkg" "$(release_stamp "$root")" || return 1
  ( cd "$dir" && printf '%s  %s\n' "$(sha256_of "$(basename "$pkg")")" "$(basename "$pkg")" > SHA256SUMS )
  printf '%s\n' "$pkg"
}
