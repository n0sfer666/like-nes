if(NOT WIN32)
  add_library(asset_core STATIC
    bundle_writer.cpp codec.cpp
    platform_io.cpp bundle_view.cpp asset_manager.cpp)
  target_include_directories(asset_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR} ${stb_SOURCE_DIR} ${zstd_SOURCE_DIR}/lib)
  target_link_libraries(asset_core PUBLIC libzstd_static Threads::Threads)

  # Headless гейт #2 (zero-copy / no per-frame heap). ASan/UBSan-джоб — Phase 6.
  add_executable(asset_test asset_test.cpp)
  target_link_libraries(asset_test PRIVATE asset_core)

  # Headless гейт #3 (замедленный I/O → тот же sim-hash). Без GPU — CI на всех POSIX-ОС.
  add_executable(asset_determinism_test asset_determinism_test.cpp)
  target_link_libraries(asset_determinism_test PRIVATE asset_core)

  # Hot-reload roundtrip (свап бандла, стабильный guid). Self-contained — CI на всех POSIX.
  add_executable(asset_hot_reload_test asset_hot_reload_test.cpp)
  target_link_libraries(asset_hot_reload_test PRIVATE asset_core)
endif()

# --- Шов asset→render (спека #5 гейт): baked BC7 tex + baked Tint SPIR-V → GPU → offscreen ---
if(NOT WIN32)
  # basis транскодер — один файл; KTX2 без zstd-суперкомпрессии → BASISD_SUPPORT_KTX2_ZSTD=0
  # (не тянем вендорный zstd, нет конфликта символов с нашим libzstd).
  add_library(basis_transcoder STATIC ${basisu_SOURCE_DIR}/transcoder/basisu_transcoder.cpp)
  target_include_directories(basis_transcoder SYSTEM PUBLIC ${basisu_SOURCE_DIR}/transcoder)
  target_compile_definitions(basis_transcoder PUBLIC BASISD_SUPPORT_KTX2=1 BASISD_SUPPORT_KTX2_ZSTD=0)
  # basisu-транскодер — сторонний: memset/memcpy non-trivial + GCC-only -Wclass-memaccess pragma
  # (clang: unknown-warning-option). Глушим точечно на сторонних TU.
  target_compile_options(basis_transcoder PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-nontrivial-memcall -Wno-unknown-warning-option>)

  add_library(asset_gpu STATIC transcode.cpp)
  target_link_libraries(asset_gpu PUBLIC basis_transcoder)
  target_compile_options(asset_gpu PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-unknown-warning-option>)   # basisu.h GCC-pragma

  add_executable(asset_render asset_render_main.cpp)
  target_include_directories(asset_render PRIVATE ${CMAKE_SOURCE_DIR}/engine/render)
  target_link_libraries(asset_render PRIVATE asset_core asset_gpu render_core webgpu)
  target_copy_webgpu_binaries(asset_render)
  reproducible_rpath(asset_render)
endif()
