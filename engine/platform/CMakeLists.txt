# Платформенный шов (спека #12, решение 2): «абстракции, а не #ifdef по коду». Реализацию
# выбирает ЭТОТ файл — потому условной компиляции по ОС нет ни в одном .cpp платформы, и
# инвариант «#ifdef не расползается» проверяется грепом по всему дереву, а не обещанием.
add_library(platform_core STATIC platform_fs.cpp platform_args.cpp)
target_include_directories(platform_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
  target_sources(platform_core PRIVATE
    win32_utf.cpp win32_cmdline.cpp
    platform_io_win32.cpp platform_fs_win32.cpp platform_path_win32.cpp
    platform_process_win32.cpp platform_module_win32.cpp platform_guard_win32.cpp
    platform_args_win32.cpp)
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
    platform_io_posix.cpp platform_fs_posix.cpp platform_path_posix.cpp
    platform_process_posix.cpp platform_module_posix.cpp platform_guard_posix.cpp
    platform_args_posix.cpp)
  target_link_libraries(platform_core PRIVATE ${CMAKE_DL_LIBS})
endif()
