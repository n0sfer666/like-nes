# shellcheck shell=bash
# Фабрика фикстурных AppDir (спека #20, вертикаль 4, шаг B). Одна на оба файловых набора по той же
# причине, что и release_dmg_fixture_lib.sh: собери один набор честный каталог своей копией, а
# другой сломанный — своей, и «утверждение отбило подмену» проверялось бы на предмете иного
# устройства, чем тот, на котором то же утверждение проходит.
#
# Раскладывает НАСТОЯЩИЙ раскладчик (appimage_make_appdir + appimage_seal_appdir): фикстура,
# разложенная копией правил, проверяла бы копию. Самого appimagetool здесь нет и не нужно — предмет
# наборов есть содержимое AppDir, а не squashfs вокруг него.

# Стейдж повторяет то, что ставит install_engine.cmake, а список файлов берёт из expected_files —
# того же источника, что и утверждение о составе. Рукописная копия отставала бы от
# install_engine.cmake ровно с того дня, когда туда добавили файл.
appimage_fixture_stage() {
  local root="$1" stage="$2" version="$3" commit="$4" triple="$5" rel
  while read -r rel; do
    [ -n "$rel" ] || continue
    mkdir -p "$stage/$(dirname "$rel")"
    case "$rel" in
      like-nes/version.txt) : ;;
      like-nes/bin/*) printf 'fixture %s\n' "$(basename "$rel")" > "$stage/$rel"; chmod 755 "$stage/$rel" ;;
      *) printf 'fixture license %s\n' "$(basename "$rel")" > "$stage/$rel"; chmod 644 "$stage/$rel" ;;
    esac
  done < <(expected_files "$root" Linux)
  printf 'like-nes engine %s\ncommit %s\ntarget %s\n' "$version" "$commit" "$triple" \
    > "$stage/like-nes/version.txt"
  chmod 644 "$stage/like-nes/version.txt"
}

# AppDir целиком: стейдж → раскладка → выравнивание времени. Штамп взят постоянной строкой, а не
# временем коммита: у фикстуры нет своего дерева, а на содержимое манифеста mtime не влияет вовсе.
# Значок берётся из дерева — тот самый, который утверждение и сверяет с packaging/.
appimage_fixture_appdir() {
  local root="$1" base="$2" version="$3" commit="$4" triple="${5:-linux-x86_64}"
  rm -rf "$base"
  mkdir -p "$base/stage"
  appimage_fixture_stage "$root" "$base/stage" "$version" "$commit" "$triple" || return 1
  appimage_make_appdir "$base/stage" "$base/AppDir" "$root/packaging/$APPIMAGE_ICON" || return 1
  appimage_seal_appdir "$base/AppDir" 202601010000.00 || return 1
}

# Манифест считается ТОЙ ЖЕ функцией, что в release.sh, и пишется рядом: утверждение
# assert_dir_matches иначе нечем было бы ни провести, ни уронить.
appimage_fixture_manifest() {
  manifest_of "$1/AppDir" > "$1/AppDir.manifest"
}

# ELF для утверждения о runpath. Текстовые «бинари» остальных фикстур для него не годятся вовсе:
# `readelf` о них не скажет ни DT_NEEDED, ни DT_RUNPATH, и утверждение обязано отбить их СВОЕЙ
# причиной, а не молча. Параметризовано ровно тем, что утверждение и читает, — SONAME, под которым
# библиотека попадает в исполняемый, и путём поиска (пустая строка — без DT_RUNPATH).
#
# `--disable-new-dtags` не задаётся: какое из двух полей напишет линкер, решает тулчейн, и
# утверждение читает оба именно поэтому.
# Диагностика компилятора ПЕЧАТАЕТСЯ, а не глушится: без неё «фикстурный ELF не построился» —
# единственное, что видел бы владелец, а причина (нет заголовков, чужая архитектура, отказавший
# линкер) оставалась бы в выброшенном stderr. Исходник убирается на ВСЕХ путях: оставленный отказом
# `.fixture.c` попадает в usr/bin, то есть в следующий же осмотр каталога.
appimage_fixture_cc() {
  local log rc=0
  log=$(mktemp)
  cc "$@" > "$log" 2>&1 || rc=$?
  [ "$rc" = 0 ] || { printf 'appimage-fixture: cc отказал кодом %s:\n' "$rc" >&2; sed 's/^/       /' "$log" >&2; }
  rm -f "$log"
  return "$rc"
}

appimage_fixture_elf() {
  local dir="$1" runtime="$2" soname="$3" runpath="$4" src rc=0
  mkdir -p "$dir"
  src="$dir/.fixture.c"
  printf 'int like_nes_fixture(void) { return 0; }\n' > "$src"
  # Библиотека линкуется ПУТЁМ, а не через -l: DT_NEEDED тогда несёт заданный soname, а поля поиска
  # линкер не пишет вовсе — то есть «без DT_RUNPATH» получается само, без флагов, гасящих то или
  # другое поле. Какое из двух напишет `-rpath`, решает тулчейн, и утверждение читает оба именно
  # поэтому.
  if appimage_fixture_cc -shared -fPIC -Wl,-soname,"$soname" -o "$dir/$runtime" "$src"; then
    printf 'int like_nes_fixture(void);\nint main(void) { return like_nes_fixture(); }\n' > "$src"
    if [ -n "$runpath" ]; then
      appimage_fixture_cc -o "$dir/assetc" "$src" "$dir/$runtime" -Wl,-rpath,"$runpath" || rc=1
    else
      appimage_fixture_cc -o "$dir/assetc" "$src" "$dir/$runtime" || rc=1
    fi
  else
    rc=1
  fi
  rm -f "$src"
  return "$rc"
}
