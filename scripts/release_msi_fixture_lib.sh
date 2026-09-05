# shellcheck shell=bash
# Фабрика фикстурных пакетов Windows (спека #20, вертикаль 4, шаг C). Одна на оба файловых набора по
# той же причине, что release_dmg_fixture_lib.sh и release_appimage_fixture_lib.sh: собери один
# набор честный пакет своей копией правил, а другой сломанный — своей, и «утверждение отбило
# подмену» проверялось бы на предмете иного устройства, чем тот, на котором то же утверждение
# проходит.
#
# Стейдж делает ci_make_pkg — та же фабрика, которой вертикаль 3 строит фикстурный пакет Windows и
# которой пользуется сам гейт: состав там выведен из expected_files, то есть из install_engine.cmake,
# а не из рукописного списка. Пакет собирает НАСТОЯЩИЙ упаковщик (extra_build → msi_make_source →
# msi_pack): фикстура, собранная копией правил, проверяла бы копию.
#
# Шага два, а не один, нарочно: между стейджем и упаковкой набор про содержимое вставляет свою
# порчу (лишний файл, файл по незнакомому пути), и слить их значило бы завести фабрике второй язык
# описания поломок вместо одной строки на месте вызова.

MSI_FIXTURE_TRIPLE=windows-x86_64

msi_fixture_stage() {
  local root="$1" base="$2" version="$3" commit="$4" damage="${5:-}" stamp_triple="${6:-$MSI_FIXTURE_TRIPLE}"
  rm -rf "$base"
  mkdir -p "$base/build/stage" "$base/dest"
  ci_make_pkg "$root" "$base" "$version" "$commit" "$MSI_FIXTURE_TRIPLE" "$damage" "$stamp_triple" >/dev/null
}

# Имя пакета постоянное: у наборов оно ни во что не входит (тройку и версию утверждения читают из
# штампа и из таблиц), а тасовать его между сценариями значило бы плодить пути на пустом месте.
msi_fixture_pack() {
  local root="$1" base="$2" version="$3"
  extra_build windows "$base/stage" "$base/dest" pkg "$version" 202601010000.00 "$base/build" \
    "$root/packaging" || return 1
  [ -s "$base/dest/pkg.msi" ] || return 1
}

msi_fixture_extract() {
  local base="$1"
  rm -rf "$base/ex"
  msi_extract_to "$base/dest/pkg.msi" "$base/ex"
}

msi_fixture_pkg() {
  local root="$1" base="$2" version="$3" commit="$4" damage="${5:-}" stamp_triple="${6:-$MSI_FIXTURE_TRIPLE}"
  msi_fixture_stage "$root" "$base" "$version" "$commit" "$damage" "$stamp_triple" || return 1
  msi_fixture_pack "$root" "$base" "$version" || return 1
  msi_fixture_extract "$base"
}
