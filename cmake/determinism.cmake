# P0-P2 воспроизводимый билд (per-triple same-host байт-идентичность). ADR 0008, развилка B.
# Путь-канонизация: абс. пути source/binary → стабильный префикс (путь build-дира не течёт в
# __FILE__/debug-info); SOURCE_DATE_EPOCH (env сборки) нейтрализует __DATE__/__TIME__; mtime архивов
# зануляется (ZERO_AR_DATE env на macOS / `D` у GNU ar); UUID/build-id пиннится; rpath — относительный
# (@executable_path / $ORIGIN), иначе CMake вшивает абсолютный build-rpath в _deps → путь течёт в exe.

if(MSVC)
  add_compile_options(/Brepro)
  add_link_options(/Brepro /INCREMENTAL:NO)
else()
  add_compile_options(
    "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=."
    "-ffile-prefix-map=${CMAKE_BINARY_DIR}=."
    -fno-ident)
  if(NOT APPLE)
    add_link_options(-Wl,--build-id=none)
    foreach(lang C CXX)
      set(CMAKE_${lang}_ARCHIVE_CREATE "<CMAKE_AR> Dqc <TARGET> <LINK_FLAGS> <OBJECTS>")
      set(CMAKE_${lang}_ARCHIVE_APPEND "<CMAKE_AR> Dq <TARGET> <LINK_FLAGS> <OBJECTS>")
      set(CMAKE_${lang}_ARCHIVE_FINISH "<CMAKE_RANLIB> -D <TARGET>")
    endforeach()
  endif()
endif()

# GPU-таргеты линкуют внешний wgpu-dylib → CMake иначе вшивает АБСОЛЮТНЫЙ build-rpath в _deps
# (путь build-дира течёт в exe → ломает байт-идентичность). Помощник target_copy_webgpu_binaries
# копирует dylib РЯДОМ с exe и хардкодит INSTALL_RPATH "./" (cwd-относительный). Переопределяем на
# @executable_path/$ORIGIN: детерминированно И находит dylib независимо от cwd. Звать ПОСЛЕ копирующего.
function(reproducible_rpath tgt)
  if(MSVC)
    return()
  endif()
  if(APPLE)
    set(rp "@executable_path")
  else()
    set(rp "$ORIGIN")
  endif()
  set_target_properties(${tgt} PROPERTIES BUILD_WITH_INSTALL_RPATH ON INSTALL_RPATH "${rp}")
endfunction()
