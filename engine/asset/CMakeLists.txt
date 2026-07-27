# Гейт 1 спеки #12: модуль ассетов собирается на всех трёх ОС. POSIX-only он был ровно из-за
# двух вещей — fork/execvp у пекаря и mmap у загрузчика; обе ушли за platform_core.
add_library(asset_core STATIC
  bundle_writer.cpp codec.cpp
  bundle_view.cpp asset_manager.cpp)
target_include_directories(asset_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# stb и zstd — сторонние: SYSTEM, иначе строгий уровень варнингов вменяет нам их заголовки
# (реализация stb_image тянет три десятка неиспользуемых функций в codec.cpp).
target_include_directories(asset_core SYSTEM PUBLIC ${stb_SOURCE_DIR} ${zstd_SOURCE_DIR}/lib)
# platform_core — PUBLIC: MappedFile стоит полем AssetManager, заголовок нужен потребителям.
target_link_libraries(asset_core PUBLIC platform_core libzstd_static Threads::Threads)

# Headless гейт #2 (zero-copy / no per-frame heap). ASan/UBSan-джоб — Phase 6.
add_executable(asset_test asset_test.cpp)
target_link_libraries(asset_test PRIVATE asset_core)

# Headless гейт #3 (замедленный I/O → тот же sim-hash). Без GPU — CI на всех ОС.
add_executable(asset_determinism_test asset_determinism_test.cpp)
target_link_libraries(asset_determinism_test PRIVATE asset_core)

# Hot-reload roundtrip (свап бандла, стабильный guid). Self-contained — CI на всех ОС.
add_executable(asset_hot_reload_test asset_hot_reload_test.cpp)
target_link_libraries(asset_hot_reload_test PRIVATE asset_core)

# --- Шов asset→render (спека #5 гейт): baked BC7 tex + baked Tint SPIR-V → GPU → offscreen ---
# basis транскодер — один файл; KTX2 без zstd-суперкомпрессии → BASISD_SUPPORT_KTX2_ZSTD=0
# (не тянем вендорный zstd, нет конфликта символов с нашим libzstd).
add_library(basis_transcoder STATIC ${basisu_SOURCE_DIR}/transcoder/basisu_transcoder.cpp)
target_include_directories(basis_transcoder SYSTEM PUBLIC ${basisu_SOURCE_DIR}/transcoder)
target_compile_definitions(basis_transcoder PUBLIC BASISD_SUPPORT_KTX2=1 BASISD_SUPPORT_KTX2_ZSTD=0)
like_nes_vendored_target(basis_transcoder)   # цель целиком — один сторонний TU

add_library(asset_gpu STATIC transcode.cpp)
target_link_libraries(asset_gpu PUBLIC basis_transcoder)
target_compile_options(asset_gpu PRIVATE
  $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-unknown-warning-option>)   # basisu.h GCC-pragma

add_executable(asset_render asset_render_main.cpp)
target_include_directories(asset_render PRIVATE ${CMAKE_SOURCE_DIR}/engine/render)
target_link_libraries(asset_render PRIVATE asset_core asset_gpu render_core webgpu)
target_copy_webgpu_binaries(asset_render)
reproducible_rpath(asset_render)
