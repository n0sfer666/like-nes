# --- Аудио-вертикаль (спека #3): mixer + шины + 2D-пан + стрим/резидент + детерминизм ---
# audio_core: микшер (Float/Fix32 seam) + декод (stb_vorbis, present в fetched stb). Без
# устройства/ассетов → deviceless-гейты гоняются в CI на всех трёх ОС (спека #12, гейт 2).
add_library(audio_core STATIC
  mixer.cpp decoder.cpp)
# stb_vorbis.c — сторонний целиком, ни строки нашей: TU глушится по факту происхождения, а не
# списком диагностик. Список не работал в принципе — `-Wno-tautological-compare` закрывал ровно
# то, что успел показать clang, а MSVC пришёл со своим C4701 из той же чужой функции; следующий
# компилятор пришёл бы с третьей. Ветка `if(NOT MSVC)` тут была не про переносимость флага, а про
# то, что подход требует по флагу на компилятор.
like_nes_vendored_object(stb_vorbis_vendored stb_vorbis_impl.c)
target_include_directories(stb_vorbis_vendored SYSTEM PRIVATE ${stb_SOURCE_DIR})
target_link_libraries(audio_core PRIVATE stb_vorbis_vendored)
target_include_directories(audio_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/asset)
target_include_directories(audio_core SYSTEM PUBLIC ${stb_SOURCE_DIR})   # stb — сторонний
target_link_libraries(audio_core PUBLIC engine_core Threads::Threads)

# Гейт #1 (байт-golden хеш микса, Fix32 над детерм. PCM). CI на всех ОС.
add_executable(audio_golden audio_golden_main.cpp)
target_link_libraries(audio_golden PRIVATE audio_core)

# Гейт #2 (замедл./джиттер callback → тот же sim-hash). TSan-джоб. CI на всех ОС.
add_executable(audio_determinism_test audio_determinism_test.cpp)
target_link_libraries(audio_determinism_test PRIVATE audio_core)

# Гейт #3 (RT-safe callback: no-alloc/lock-free). ASan/UBSan-джоб. CI на всех ОС.
add_executable(audio_rt_test audio_rt_test.cpp)
target_link_libraries(audio_rt_test PRIVATE audio_core platform_core)   # шов: PLATFORM_NOINLINE

# Шов asset->audio: бейкнутый vorbis → decode → mixer → offline WAV (+ --play на устройстве).
add_executable(audio_seam audio_seam_main.cpp)
target_link_libraries(audio_seam PRIVATE audio_core asset_core)

if(AUDIO_MINIAUDIO)
  add_library(audio_device STATIC device.cpp)
  like_nes_vendored_object(miniaudio_vendored miniaudio_impl.c)   # TU целиком — реализация miniaudio
  target_include_directories(miniaudio_vendored SYSTEM PRIVATE ${miniaudio_SOURCE_DIR})
  target_link_libraries(audio_device PRIVATE miniaudio_vendored)
  target_include_directories(audio_device PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  # SYSTEM по той же причине, что и у imgui: device.cpp — наш код в одной цели с miniaudio.
  target_include_directories(audio_device SYSTEM PUBLIC ${miniaudio_SOURCE_DIR})
  target_link_libraries(audio_device PUBLIC engine_core)
  target_compile_definitions(audio_device PUBLIC AUDIO_HAVE_MINIAUDIO)
  if(APPLE)
    target_link_libraries(audio_device PUBLIC "-framework CoreAudio" "-framework AudioToolbox"
      "-framework CoreFoundation")
  elseif(WIN32)
    # Бэкенд по умолчанию — WASAPI. Сам miniaudio тянет ole32/avrt через LoadLibrary, но
    # PropVariantClear/CoTaskMemFree линкуются статически → ole32.
    target_link_libraries(audio_device PUBLIC ole32)
  else()
    target_link_libraries(audio_device PUBLIC ${CMAKE_DL_LIBS} Threads::Threads m)
  endif()
  target_link_libraries(audio_seam PRIVATE audio_device)
endif()
