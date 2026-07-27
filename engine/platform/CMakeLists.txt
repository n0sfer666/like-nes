# Платформенный шов (спека #12, решение 2): «абстракции, а не #ifdef по коду». Реализацию
# выбирает ЭТОТ файл — потому условной компиляции по ОС нет ни в одном .cpp платформы, и
# инвариант «#ifdef не расползается» проверяется грепом по всему дереву, а не обещанием.
add_library(platform_core STATIC platform_fs.cpp platform_args.cpp)
target_include_directories(platform_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Статическую библиотеку линкуют и MODULE-цели (плагины читают окружение через шов), а объект без
# -fPIC в разделяемый объект не линкуется вовсе. На Ubuntu-раннере это до поры не всплывает —
# gcc там default-PIE, — но на любой сборке без PIE линкер валится на релокации.
set_target_properties(platform_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Единственный шов, где обе реализации возвращают строку и расхождение молчит: тест гоняется на
# трёх ОС, потому и объявлен вне ветки WIN32.
add_executable(platform_env_test platform_env_test.cpp)
target_link_libraries(platform_env_test PRIVATE platform_core)

# Шов разделяемой памяти (спека #13) — та же ловушка, что у окружения: обе реализации отдают
# «получилось», и расхождение молчит до живого прогона. Потому тоже вне ветки WIN32.
add_executable(platform_shmem_test platform_shmem_test.cpp)
target_link_libraries(platform_shmem_test PRIVATE platform_core)

if(WIN32)
  target_sources(platform_core PRIVATE
    win32_utf.cpp win32_cmdline.cpp
    platform_io_win32.cpp platform_fs_win32.cpp platform_path_win32.cpp platform_env_win32.cpp
    platform_process_win32.cpp platform_module_win32.cpp platform_guard_win32.cpp
    platform_args_win32.cpp platform_shmem_win32.cpp)
  # CommandLineToArgvW живёт в shell32: его зовёт platform_args_win32.cpp (разбор своей строки
  # запуска) и тест цитирования ниже. PRIVATE у статической библиотеки всё равно доезжает до
  # потребителей линковкой — потому тесту хватает одного platform_core.
  target_link_libraries(platform_core PRIVATE shell32)
  # Цитирование argv (win32_cmdline.cpp собирает строку для ребёнка) сверяется с настоящим
  # CommandLineToArgvW → тест только под Windows.
  add_executable(win32_cmdline_test win32_cmdline_test.cpp)
  target_link_libraries(win32_cmdline_test PRIVATE platform_core)
else()
  target_sources(platform_core PRIVATE
    platform_io_posix.cpp platform_fs_posix.cpp platform_path_posix.cpp platform_env_posix.cpp
    platform_process_posix.cpp platform_module_posix.cpp platform_guard_posix.cpp
    platform_args_posix.cpp platform_shmem_posix.cpp)
  target_link_libraries(platform_core PRIVATE ${CMAKE_DL_LIBS})
  # shm_open на Linux живёт в librt (на macOS — в libc). Раньше эту зависимость тянул ipc_core
  # из tools/ide; после переезда шва в платформу она принадлежит platform_core.
  if(NOT APPLE)
    target_link_libraries(platform_core PUBLIC rt)
  endif()
endif()
