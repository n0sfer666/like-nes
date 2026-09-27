# Окно и WebGPU десктопа: GLFW, предсобранный wgpu-native и glue поверхности. Своим файлом, а не
# блоком корня: мобильная сборка (iOS/Android) идёт тем же корнем и берёт вместо него
# platform/mobile/wgpu_native.cmake — окна GLFW там нет, а wgpu-native собирается из Rust.

# --- GLFW (windowing) ---
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# По умолчанию X11-only: wayland-scanner и libwayland-dev на раннере не стоят, а рендер в CI —
# xvfb/X11. Wayland включается ЯВНО (`-DLINUX_WAYLAND=ON`), и без этого гейт 6 спеки #13 проверить
# нечем: под Wayland-сессией собранный без него GLFW живёт клиентом XWayland, то есть отвечает на
# вопрос про X11 второй раз. Системные зависимости — docs/en/getting-started/build.md.
set(GLFW_BUILD_WAYLAND ${LINUX_WAYLAND} CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw
  GIT_TAG a74efa0d5628b74adc0426af4c5710e287fa7c2c  # 3.4
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

# --- WebGPU backend ---
# Движок линкует wgpu-native (prebuilt) — Dawn-из-исходников не собирается на arm64-macOS
# (баг Abseil ×clang21: -msse4.1). Тот же стандартный webgpu.h, что и Dawn (продакшн-цель
# в ADR): render-код переносится без изменений, свап на Dawn — позже.
set(WEBGPU_BACKEND "WGPU" CACHE STRING "" FORCE)
set(WEBGPU_BUILD_FROM_SOURCE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(webgpu
  GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
  GIT_TAG f2b81861a0c1889ef51b79fc777da8e11c37bdb3  # main-v0.2.0
)
FetchContent_MakeAvailable(webgpu)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/wgpu_native_check.cmake)

# --- glfw3webgpu (surface creation glue) ---
FetchContent_Declare(glfw3webgpu
  GIT_REPOSITORY https://github.com/eliemichel/glfw3webgpu
  GIT_TAG 39a80205998c6cbf803e18e750d964c23a47ef39  # v1.2.0
)
FetchContent_MakeAvailable(glfw3webgpu)
# Бэкенд ОКНА этот glue выбирает на этапе компиляции, а `_GLFW_X11`/`_GLFW_WAYLAND` у самой glfw
# объявлены PRIVATE — до потребителя они не доходят, и на Linux ветка всегда бралась X11-ная
# (`#else` в его цепочке). GLFW при этом выбирает платформу В РАНТАЙМЕ, поэтому под Wayland-сессией
# бинарь спрашивал X11-дисплей у Wayland-платформы, получал NULL и умирал паникой wgpu
# «Display pointer is not set» — уже внутри создания поверхности, без единого слова про причину.
# Пробрасываем свой же выбор: каталог, собранный с LINUX_WAYLAND=ON, и поверхность создаёт
# Wayland-ную. Выбор остаётся ОДИН на каталог сборки — ровно поэтому гейт 6 требует двух деревьев.
# Под `Linux`, а не голым `if(LINUX_WAYLAND)`: у самой glfw опция `GLFW_BUILD_WAYLAND` вне Linux
# игнорируется ею же, а у glue такой защиты нет — `-DLINUX_WAYLAND=ON` на macOS увёл бы его в
# Wayland-ветку создания поверхности поверх Cocoa.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND LINUX_WAYLAND)
  target_compile_definitions(glfw3webgpu PRIVATE _GLFW_WAYLAND)
endif()
# Публичные макросы — НАШИ и объявляются ЗДЕСЬ, потому что выбор бэкенда окна это выбор ОС, а он по
# инварианту 1 спеки #12 живёт в сборке, не в исходнике: `#if defined(__linux__)` в коде редактора
# гейт `tree_invariants.sh seam` отбивает — и правильно. Потребителю нужно знать, под какую сессию
# собран каталог, чтобы назвать расхождение словами. Чужое `_GLFW_WAYLAND` для этого не годится:
# оно приватное намеренно (внутренний макрос glfw), публиковать его значило бы включить внутренние
# ветки glfw в нашем коде.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  if(LINUX_WAYLAND)
    target_compile_definitions(glfw3webgpu PUBLIC LIKE_NES_GLFW_WAYLAND)
  else()
    target_compile_definitions(glfw3webgpu PUBLIC LIKE_NES_GLFW_X11)
  endif()
endif()
