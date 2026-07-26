# --- Аудио-вертикаль (спека #3): mixer + шины + 2D-пан + стрим/резидент + детерминизм ---
# audio_core: микшер (Float/Fix32 seam) + декод (stb_vorbis, present в fetched stb). Без
# устройства/ассетов → deviceless-гейты гоняются в CI на всех POSIX.
if(NOT WIN32)
  add_library(audio_core STATIC
    mixer.cpp decoder.cpp stb_vorbis_impl.c)
  # stb_vorbis.c — сторонний (tautological-compare); глушим точечно на этом TU.
  if(NOT MSVC)  # GCC/Clang-only флаг; MSVC → D8021
    set_source_files_properties(stb_vorbis_impl.c PROPERTIES COMPILE_OPTIONS "-Wno-tautological-compare")
  endif()
  target_include_directories(audio_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/asset ${stb_SOURCE_DIR})
  target_link_libraries(audio_core PUBLIC engine_core Threads::Threads)

  # Гейт #1 (байт-golden хеш микса, Fix32 над детерм. PCM). CI на всех POSIX.
  add_executable(audio_golden audio_golden_main.cpp)
  target_link_libraries(audio_golden PRIVATE audio_core)

  # Гейт #2 (замедл./джиттер callback → тот же sim-hash). TSan-джоб. CI на всех POSIX.
  add_executable(audio_determinism_test audio_determinism_test.cpp)
  target_link_libraries(audio_determinism_test PRIVATE audio_core)

  # Гейт #3 (RT-safe callback: no-alloc/lock-free). ASan/UBSan-джоб. CI на всех POSIX.
  add_executable(audio_rt_test audio_rt_test.cpp)
  target_link_libraries(audio_rt_test PRIVATE audio_core)

  # Шов asset->audio: бейкнутый vorbis → decode → mixer → offline WAV (+ --play на устройстве).
  add_executable(audio_seam audio_seam_main.cpp)
  target_link_libraries(audio_seam PRIVATE audio_core asset_core)

  if(AUDIO_MINIAUDIO)
    add_library(audio_device STATIC device.cpp miniaudio_impl.c)
    target_include_directories(audio_device PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR} ${miniaudio_SOURCE_DIR})
    target_link_libraries(audio_device PUBLIC engine_core)
    target_compile_definitions(audio_device PUBLIC AUDIO_HAVE_MINIAUDIO)
    if(APPLE)
      target_link_libraries(audio_device PUBLIC "-framework CoreAudio" "-framework AudioToolbox"
        "-framework CoreFoundation")
    else()
      target_link_libraries(audio_device PUBLIC ${CMAKE_DL_LIBS} Threads::Threads m)
    endif()
    target_link_libraries(audio_seam PRIVATE audio_device)
  endif()
endif()
