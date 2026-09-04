# shellcheck shell=bash
# Как AppImage находит рантайм (спека #20, вертикаль 4, шаг B) — своим файлом, а не строкой в
# release_appimage_check_lib.sh, по той же границе, что у пары `release_dmg_rpath_lib.sh`: предмет
# здесь не содержимое AppDir, а СВЯЗЬ ВНУТРИ ELF, и читается она единственным в наборе `readelf`.
#
# Это линуксовая половина того же механизма: `reproducible_rpath` из cmake/determinism.cmake ставит
# `@executable_path` на macOS и `$ORIGIN` на Linux. Связка тоже двухступенчатая — исполняемый
# называет библиотеку по SONAME (DT_NEEDED) и сам же говорит, где её искать (DT_RUNPATH). Сломай
# любую половину: AppDir соберётся, состав, права и лицензии сойдутся, а образ не стартует нигде,
# кроме машины, где его собрали, — и увидит это пользователь, а не гейт.
#
# Абсолютный путь поиска отбивается наравне с отсутствующим: он ведёт в каталог сборки, то есть
# ровно туда, куда пакет уехать не может. Именно от этого `reproducible_rpath` и переопределяет
# rpath после копирующего помощника CMake.

# Уводит ли путь поиска за пределы AppDir. Глубина считается от каталога исполняемого (usr/bin, то
# есть два уровня от корня образа): пока баланс неотрицателен, путь остаётся внутри пакета, а первый
# же сегмент, ушедший ниже нуля, ведёт на машину, где образ запускают, — то есть никуда.
appimage_runpath_inside() {
  local rest="$1" seg depth=2 IFS=/
  for seg in $rest; do
    case "$seg" in
      ..) depth=$((depth - 1)); [ "$depth" -ge 0 ] || return 1 ;;
      .|'') : ;;
      *) depth=$((depth + 1)) ;;
    esac
  done
}

appimage_elf_dyn() {
  readelf -d "$1" 2>/dev/null | sed -n "s/.*($2)[^:]*: *\[\{0,1\}\([^]]*\)\]\{0,1\}.*/\1/p"
}

assert_appimage_runpath() {
  local dir="$1" runtime="$2" f base needed rp one seen=0 list="" glob_was_off=""
  [ -n "$runtime" ] || { bad "имя рантайма не названо — утверждать о runpath нечего"; return 1; }
  for f in "$dir/usr/bin"/*; do
    [ -f "$f" ] || continue
    base=$(basename "$f")
    [ "$base" != "$runtime" ] || continue
    # Не-ELF отбивается ВСЛУХ: `readelf` о текстовом файле отвечает «Not an ELF file» и КОДОМ
    # НОЛЬ на части сборок binutils, поэтому молчаливый пропуск сделал бы утверждение вакуумным —
    # ровно та же ловушка, что у `otool` на шаге A.
    if ! readelf -h "$f" 2>/dev/null | grep -q 'ELF Header'; then
      list="$list $base(не ELF)"; continue
    fi
    seen=$((seen + 1))
    needed=$(appimage_elf_dyn "$f" NEEDED | grep -Fx "$runtime" | head -1)
    [ -n "$needed" ] || list="$list $base(не линкует $runtime)"
    # DT_RUNPATH и DT_RPATH — одно и то же поле для нашей цели; современный ld пишет первое, старый
    # второй, и утверждать про одно значило бы падать на тулчейне, который делает то же самое.
    rp=$(appimage_elf_dyn "$f" RUNPATH; appimage_elf_dyn "$f" RPATH)
    [ -n "$rp" ] || list="$list $base(нет RUNPATH)"
    # Разбиение по двоеточию идёт с ВЫКЛЮЧЕННОЙ подстановкой имён: элемент со звёздочкой иначе
    # раскрылся бы по каталогу, и диагностика печатала бы не то, что записано в ELF.
    # Прежнее состояние подстановки ВОЗВРАЩАЕТСЯ, а не выставляется в `+f`: библиотеку сорсит и гейт,
    # и оба набора самопроверки, и `set +f` посреди чужого прогона включал бы им подстановку, которую
    # они выключили для себя.
    case $- in *f*) glob_was_off=1 ;; *) glob_was_off="" ;; esac
    set -f
    for one in $(printf '%s' "$rp" | tr ':' ' '); do
      # Хвост проверяется РАЗДЕЛИТЕЛЕМ, а не «начинается с $ORIGIN»: шаблон-префикс принимает
      # `$ORIGINFOO` — чужой каталог с похожим именем, к образу отношения не имеющий.
      #
      # `..` сам по себе нарушением НЕ является: исполняемые лежат в usr/bin, и `$ORIGIN/../lib`
      # указывает внутрь того же AppDir. Наружу выводит не наличие `..`, а их ПЕРЕВЕС над глубиной,
      # поэтому считается баланс, а не ищется подстрока.
      case "$one" in
        '$ORIGIN') : ;;
        '$ORIGIN/'*) appimage_runpath_inside "${one#'$ORIGIN/'}" || list="$list $base(runpath $one)" ;;
        *) list="$list $base(runpath $one)" ;;
      esac
    done
    [ -n "$glob_was_off" ] || set +f
  done
  if [ -n "$list" ]; then bad "рантайм из AppImage не находится:$list"; return 1; fi
  [ "$seen" != 0 ] || { bad "в usr/bin нет ни одного ELF — утверждение о runpath вакуумно"; return 1; }
  ok "runpath: $seen исполняемых линкуют $runtime и ищут его рядом с собой (\$ORIGIN)"
}
