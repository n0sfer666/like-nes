# shellcheck shell=bash
# Читатель PE в фикстурном дереве (спека #20, вертикаль 5). Отдельный файл, потому что потребителей
# два — набор про сам разбор (check_release_crt_reader_selftest.sh) и набор про пару копий
# (check_release_crt_named_selftest.sh), — и копия у каждого разъехалась бы молча.
#
# Читатель НЕ ОДИН ФАЙЛ: решение 23 спеки #19 дало ему соседний модуль переключения вывода, а
# фикстуры копировали только его самого — и запуск в дереве умирал ImportError'ом. У набора про
# разбор это уронило якорь на настоящем рантайме, то есть было названо; у соседнего оба кейса ждут
# ОТКАЗА утверждения, и чужой отказ читался бы там как «утверждение отбило порчу».
#
# Поэтому список выводится ИЗ САМОГО читателя, а не пишется руками: следующая его зависимость
# приедет в фикстуру сама. Рукописный здесь есть ровно list-drift.
crt_copy_reader() {
  local root="$1" dest="$2" mod
  mkdir -p "$dest/scripts" || return 1
  cp "$root/scripts/pe_imports.py" "$dest/scripts/" || return 1
  for mod in $(sed -n 's/^import \([a-z_][a-z0-9_]*\)$/\1/p' "$root/scripts/pe_imports.py"); do
    [ -f "$root/scripts/$mod.py" ] || continue
    cp "$root/scripts/$mod.py" "$dest/scripts/" || return 1
  done
}
