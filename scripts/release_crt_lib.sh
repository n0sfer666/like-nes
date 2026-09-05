# shellcheck shell=bash
# Утверждения о зависимости пакета Windows от VC++ Redistributable (спека #20, вертикаль 5) —
# отдельно от того, что их запускает: их зовёт гейт (check_release_crt.sh), осмотр приехавшего из
# CI пакета (release_ci_check_lib.sh) и самопроверка на сломанных фикстурах. Утверждение,
# существующее только внутри своего прогона, невозможно уронить нарочно, а значит — неизвестно,
# умеет ли оно вообще падать.
#
# Предмет здесь один: ЧЕГО не хватит на чистой Windows. Наши бинари собираются со статическим CRT
# (cmake/msvc_runtime.cmake) и не просят ничего; предсобранный wgpu_native.dll собран чужим
# тулчейном с динамическим, импортирует VCRUNTIME140.dll — и она едет В ПАКЕТЕ рядом с ним.
# UCRT (`api-ms-win-crt-*`, `ucrtbase.dll`) сюда не входит: с Windows 10 это компонент системы.

crt_ok() { printf 'crt-check: OK   %s\n' "$1"; }
crt_bad() { printf 'crt-check: FAIL %s\n' "$1" >&2; }

# Закрытый список имён из VC++ Redistributable. Список ЗАКРЫТ и потому проверяем: обратная форма —
# «всё, чего нет в системе» — потребовала бы перечислить DLL самой Windows, а тот список стареет с
# каждой её версией и молча превращал бы гейт в утверждение о вчерашней ОС.
# Цифра после имени обязательна: `msvcrt.dll` живёт в System32 с девяностых и redist'ом не
# является, а `msvcr120.dll` — является.
crt_is_redist() {
  case "$(printf '%s' "$1" | tr 'A-Z' 'a-z')" in
    vcruntime[0-9]*.dll|msvcp[0-9]*.dll|msvcr[0-9]*.dll|concrt[0-9]*.dll) return 0 ;;
    vccorlib[0-9]*.dll|mfc[0-9]*.dll|vcamp[0-9]*.dll|vcomp[0-9]*.dll) return 0 ;;
  esac
  return 1
}

# Чужие бинари пакета — те, что мы не собираем и пересобрать нечем. Список ПОИМЁННЫЙ, и это
# несущее свойство: наша будущая разделяемая библиотека попадает под утверждение о статичности CRT
# по умолчанию, а новый чужой бинарь требует осознанной записи здесь. Опознавать «наше» по
# расширению `.exe` было той же ошибкой, что рукописный список исходников: первая же наша .dll
# уехала бы на динамическом CRT незамеченной.
crt_is_foreign() {
  local name
  name=$(basename "$1")
  # Файлы САМОГО redist, приехавшие app-local, чужие по определению: их кладёт cmake из установки
  # MSVC, и требовать от vcruntime140.dll статического CRT бессмысленно. Ветка идёт через тот же
  # закрытый список, а не вторым перечислением имён.
  crt_is_redist "$name" && return 0
  case "$(printf '%s' "$name" | tr 'A-Z' 'a-z')" in
    wgpu_native.dll) return 0 ;;
  esac
  return 1
}

# Подкаталог дистрибуции внутри каталога сборки. Отдельным именем, потому что путь этот нужен ОБЕИМ
# сторонам: искать в нём настоящий рантайм и СТРОИТЬ по нему фикстурное дерево, куда смотрит то же
# утверждение. Написанный руками дважды, он есть ровно list-drift: переезд FetchContent или переход
# на windows-aarch64 погасил бы одну копию и оставил другую, а наборы начали бы молча пропускать
# якорь либо строить фикстуру мимо места, куда утверждение смотрит.
crt_dist_subdir() { printf '_deps/webgpu-backend-wgpu-src/bin/windows-x86_64\n'; }

# Настоящий wgpu_native.dll из выкачанной дистрибуции. Поиск ОДИН на всех, кто его спрашивает:
# якорь читателя и сверка app-local состава разъехались бы в глобе молча, и одна из них тихо
# начала бы пропускать себя за отсутствием файла.
crt_real_dll() {
  local dll sub
  sub=$(crt_dist_subdir)
  for dll in "$1"/build*/"$sub"/wgpu_native.dll; do
    [ -f "$dll" ] && { printf '%s\n' "$dll"; return 0; }
  done
  return 1
}

crt_pe_files() {
  find "$1" -type f \( -name '*.exe' -o -name '*.dll' \) | LC_ALL=C sort
}

# Регистр имени в таблице импортов пишет линкер, а на диске файл лежит так, как назвал его
# установщик: `VCRUNTIME140.dll` против `vcruntime140.dll`. На файловой системе macOS такая пара
# совпадает сама собой, на Linux — нет, и утверждение проверяло бы платформу вместо пакета.
crt_file_here() {
  local dir="$1" want
  want=$(printf '%s' "$2" | tr 'A-Z' 'a-z')
  local f
  for f in "$dir"/*; do
    [ -f "$f" ] || continue
    [ "$(basename "$f" | tr 'A-Z' 'a-z')" = "$want" ] && return 0
  done
  return 1
}

# Главное утверждение вертикали: КАЖДАЯ redist-DLL, которую просит хоть один бинарь пакета, лежит
# в том же каталоге. Это ровно смысл «пакет не требует VC++ Redistributable» — и ровно то, чего не
# видит ни состав (файл на месте, а просит его кто-то другой), ни штамп.
assert_crt_self_contained() {
  local root="$1" pkg_root="$2" n=0 rc=0 f name imp exe dir nexe=0
  local want=""
  while read -r f; do
    [ -n "$f" ] || continue
    n=$((n + 1))
    # Импорты забираются в переменную, а не читаются из подстановки процесса: отказ читателя
    # внутри `done < <(…)` не виден коду возврата цикла вовсе, и бинарь, который не разобрался,
    # проезжал бы как бинарь без единого redist-импорта.
    if ! imp=$(python3 "$root/scripts/pe_imports.py" "$f"); then
      crt_bad "таблица импортов не прочитана: $(basename "$f")"
      rc=1
      continue
    fi
    while read -r name; do
      [ -n "$name" ] || continue
      crt_is_redist "$name" || continue
      want="$want$name
"
    done <<< "$imp"
  done < <(crt_pe_files "$pkg_root")
  # Ноль осмотренных бинарей — находка, а не «нарушений нет»: `find` по несуществующему каталогу
  # молчит, цикл не выполняется ни разу, список нарушений пуст (тот же класс, что правило
  # vacuous-gate в ci_lint.py).
  if [ "$n" -eq 0 ]; then
    crt_bad "в пакете не найдено ни одного .exe/.dll — осматривать нечего"
    return 1
  fi
  # Ищется DLL в каталоге ИСПОЛНЯЕМОГО, а не рядом с тем, кто её просит: app-local поиск Windows
  # идёт по каталогу процесса. Сегодня и то и другое — один `bin/`, но после первого же переезда
  # файла проверка «рядом с импортёром» дала бы зелёный пакет, который не стартует.
  want=$(printf '%s' "$want" | LC_ALL=C sort -u)
  while read -r exe; do
    [ -n "$exe" ] || continue
    nexe=$((nexe + 1))
    dir=$(dirname "$exe")
    while read -r name; do
      [ -n "$name" ] || continue
      if ! crt_file_here "$dir" "$name"; then
        crt_bad "$(basename "$exe") не найдёт $name в своём каталоге — её нет в пакете"
        rc=1
      fi
    done <<< "$want"
  done < <(find "$pkg_root" -type f -name '*.exe' | LC_ALL=C sort)
  if [ "$nexe" -eq 0 ]; then
    crt_bad "в пакете не найдено ни одного .exe — искать app-local DLL не для кого"
    return 1
  fi
  [ "$rc" -eq 0 ] || return 1
  crt_ok "ни одной недостающей DLL из VC++ Redistributable ($n бинарей осмотрено, $nexe исполняемых)"
}

# Отдельное утверждение о НАШЕЙ половине, потому что предыдущее её не видит: пока vcruntime140.dll
# лежит в пакете, ссылаться на неё может кто угодно. Статический CRT снимает зависимость с наших
# целей, и потеря `CMAKE_MSVC_RUNTIME_LIBRARY` обязана быть находкой, а не тихим возвратом к тому,
# что в пакете и так есть.
assert_static_crt() {
  local root="$1" pkg_root="$2" n=0 rc=0 f name imp
  while read -r f; do
    [ -n "$f" ] || continue
    crt_is_foreign "$f" && continue
    n=$((n + 1))
    if ! imp=$(python3 "$root/scripts/pe_imports.py" "$f"); then
      crt_bad "таблица импортов не прочитана: $(basename "$f")"
      rc=1
      continue
    fi
    while read -r name; do
      [ -n "$name" ] || continue
      if crt_is_redist "$name"; then
        crt_bad "$(basename "$f") собран с ДИНАМИЧЕСКИМ CRT: импортирует $name"
        rc=1
      fi
    done <<< "$imp"
  done < <(crt_pe_files "$pkg_root")
  if [ "$n" -eq 0 ]; then
    crt_bad "в пакете не найдено ни одного НАШЕГО бинаря — статичность CRT проверять не на чем"
    return 1
  fi
  [ "$rc" -eq 0 ] || return 1
  crt_ok "наши бинари не просят VC++ Redistributable ($n осмотрено, чужие исключены поимённо)"
}

# Якорь читателя. Синтетические фикстуры доказывают, что утверждения умеют падать, но собственный
# генератор не доказывает, что читатель понимает формат Microsoft, — он доказывает, что читатель
# понимает ЭТОТ генератор. Настоящий wgpu_native.dll из дистрибуции закрывает вторую половину:
# он собран чужим тулчейном и его ожидаемые импорты известны.
assert_pe_reader_anchor() {
  local root="$1" dll got
  if ! dll=$(crt_real_dll "$root"); then
    crt_ok "ПРОПУСК: настоящего wgpu_native.dll нет в дереве (дистрибуция не выкачана) — якорь читателя не проверен"
    return 0
  fi
  got=$(python3 "$root/scripts/pe_imports.py" "$dll") || {
    crt_bad "читатель не разобрал настоящий wgpu_native.dll"; return 1
  }
  # Сверка РЕГИСТРОНЕЗАВИСИМА по той же причине, по какой её ведёт crt_file_here: регистр имени в
  # таблице импортов пишет линкер, и пересборка дистрибуции чужим тулчейном сменила бы `kernel32.dll`
  # на `KERNEL32.dll` — якорь отказал бы на исправном читателе. Фактический список печатается в
  # отказе: «читатель смотрит не туда» без того, что он ВЕРНУЛ, не отличить от смены имени в чужой
  # сборке.
  local want
  for want in kernel32.dll VCRUNTIME140.dll api-ms-win-crt-runtime-l1-1-0.dll; do
    printf '%s\n' "$got" | grep -Fqix -- "$want" || {
      crt_bad "в импортах настоящего wgpu_native.dll нет $want — читатель смотрит не туда; прочитано: $(printf '%s' "$got" | tr '\n' ' ')"
      return 1
    }
  done
  crt_ok "якорь читателя: настоящий wgpu_native.dll разобран ($(printf '%s\n' "$got" | wc -l | tr -d ' ') импортов)"
}
