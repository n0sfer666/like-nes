# shellcheck shell=bash
# Фабрика фикстурных томов (спека #20, вертикаль 4, шаг A). Одна на оба набора самопроверки по той
# же причине, что и release_ci_fixture_lib.sh: собери один набор честный том своей копией, а другой
# сломанный — своей, и «утверждение отбило подмену» проверялось бы на томе иного устройства, чем
# тот, на котором то же утверждение проходит.
#
# Стейдж строится в форме компонента `engine` и раскладывается НАСТОЯЩИМ раскладчиком
# (dmg_make_app + dmg_seal_volume): фикстура, разложенная своей копией правил, проверяла бы копию.

# Стейдж повторяет то, что ставит install_engine.cmake, а список файлов берёт из expected_files —
# того же источника, что и утверждение о составе. Рукописная копия здесь означала бы, что фикстура
# отстаёт от install_engine.cmake ровно с того дня, когда туда добавили файл.
dmg_fixture_stage() {
  local root="$1" stage="$2" version="$3" commit="$4" triple="$5" rel
  while read -r rel; do
    [ -n "$rel" ] || continue
    mkdir -p "$stage/$(dirname "$rel")"
    case "$rel" in
      like-nes/version.txt) : ;;
      like-nes/bin/*) printf 'fixture %s\n' "$(basename "$rel")" > "$stage/$rel"; chmod 755 "$stage/$rel" ;;
      *) printf 'fixture license %s\n' "$(basename "$rel")" > "$stage/$rel"; chmod 644 "$stage/$rel" ;;
    esac
  done < <(expected_files "$root" Darwin)
  printf 'like-nes engine %s\ncommit %s\ntarget %s\n' "$version" "$commit" "$triple" \
    > "$stage/like-nes/version.txt"
  chmod 644 "$stage/like-nes/version.txt"
}

# Том целиком: стейдж → бандл → симлинк. Штамп времени взят постоянной строкой, а не временем
# коммита: у фикстуры нет своего дерева, а на содержимое манифеста mtime не влияет вовсе.
dmg_fixture_vol() {
  local root="$1" base="$2" version="$3" commit="$4" triple="${5:-macos-arm64}"
  rm -rf "$base"
  mkdir -p "$base/stage" "$base/vol"
  dmg_fixture_stage "$root" "$base/stage" "$version" "$commit" "$triple" || return 1
  dmg_make_app "$base/stage" "$base/vol/$DMG_APP_NAME" "$version" || return 1
  dmg_seal_volume "$base/vol" 202601010000.00 || return 1
}

# Манифест тома считается ТОЙ ЖЕ функцией, что в release.sh, и пишется рядом: утверждение
# assert_dmg_matches иначе нечем было бы ни провести, ни уронить.
dmg_fixture_manifest() {
  manifest_of "$1/vol" > "$1/vol.manifest"
}

# Mach-O для утверждения о rpath. Текстовые «бинари» остальных фикстур для него не годятся вовсе:
# `otool` о них не скажет ничего, и утверждение обязано отбить их СВОЕЙ причиной, а не молча.
# Параметризовано ровно тем, что утверждение и читает, — именем, под которым библиотека попадает в
# исполняемый (install_name), и тем, чем тот разворачивает `@rpath` (пустая строка — без LC_RPATH).
#
# Собирается тем же `cc`, которым собирается дерево: набор идёт ДО сборок в preflight, поэтому
# опереться на настоящий продукт фикстура не может, а компилятор на машине есть по построению.
dmg_fixture_macho() {
  local dir="$1" runtime="$2" instname="$3" rpath="$4" src
  mkdir -p "$dir"
  src="$dir/.fixture.c"
  printf 'int like_nes_fixture(void) { return 0; }\n' > "$src"
  cc -dynamiclib -install_name "$instname" -o "$dir/$runtime" "$src" 2>/dev/null || return 1
  printf 'int like_nes_fixture(void);\nint main(void) { return like_nes_fixture(); }\n' > "$src"
  if [ -n "$rpath" ]; then
    cc -o "$dir/assetc" "$src" "$dir/$runtime" -Wl,-rpath,"$rpath" 2>/dev/null || return 1
  else
    cc -o "$dir/assetc" "$src" "$dir/$runtime" 2>/dev/null || return 1
  fi
  rm -f "$src"
}
