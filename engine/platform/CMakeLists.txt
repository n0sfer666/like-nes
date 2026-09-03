# Платформенный шов (спека #12, решение 2): «абстракции, а не #ifdef по коду». Реализацию
# выбирает ЭТОТ файл — потому условной компиляции по ОС нет ни в одном .cpp платформы, и
# инвариант «#ifdef не расползается» проверяется грепом по всему дереву, а не обещанием.
add_library(platform_core STATIC platform_fs.cpp platform_args.cpp platform_watch_poll.cpp)
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

# Шов запуска процесса с захватом вывода (спека #13): перезапускает сам себя со служебным
# режимом, потому отдельной цели-ребёнка не нужно. Тоже вне ветки WIN32 — гоняется на трёх ОС.
add_executable(platform_process_test platform_process_test.cpp)
target_link_libraries(platform_process_test PRIVATE platform_core)

# Шов наблюдения за каталогом (спека #13, решение 4): три РАЗНЫЕ по устройству реализации
# (опрос дескриптора / колбэк с потока run loop / асинхронное чтение) под одним опросным API —
# расхождение между ними иначе всплыло бы только в живом build-loop. Тест гоняет и нативный
# бэкенд, и принудительный поллинг, поэтому объявлен вне ветки WIN32.
add_executable(platform_watch_test platform_watch_test.cpp)
target_link_libraries(platform_watch_test PRIVATE platform_core Threads::Threads)

if(WIN32)
  target_sources(platform_core PRIVATE
    win32_utf.cpp win32_cmdline.cpp
    platform_io_win32.cpp platform_fs_win32.cpp platform_path_win32.cpp platform_env_win32.cpp
    platform_process_win32.cpp platform_module_win32.cpp platform_guard_win32.cpp
    platform_args_win32.cpp platform_shmem_win32.cpp win32_spawn.cpp platform_watch_win32.cpp)
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
  # Наблюдение — единственный шов, где POSIX не общий: inotify есть только на Linux, FSEvents
  # только на macOS, и «POSIX-реализация» тут была бы фикцией из двух #ifdef.
  if(APPLE)
    target_sources(platform_core PRIVATE platform_watch_macos.cpp)
    target_link_libraries(platform_core PUBLIC "-framework CoreServices")
  else()
    target_sources(platform_core PRIVATE platform_watch_linux.cpp)
  endif()
  # shm_open на Linux живёт в librt (на macOS — в libc). Раньше эту зависимость тянул ipc_core
  # из tools/ide; после переезда шва в платформу она принадлежит platform_core.
  if(NOT APPLE)
    target_link_libraries(platform_core PUBLIC rt)
  endif()
endif()

# Шов №8 (спека #22, решение 4): UDP-датаграммы. ОТДЕЛЬНАЯ цель, а не источники в platform_core, —
# требование спеки «сеть отключается целиком: одиночная игра не платит за сетевой слой ни
# аллокацией, ни тиком». Winsock и ws2_32 приезжают только тому, кто линкует platform_net.
add_library(platform_net STATIC platform_net.cpp)
target_include_directories(platform_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(platform_net PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(WIN32)
  target_sources(platform_net PRIVATE platform_net_win32.cpp)
  target_link_libraries(platform_net PUBLIC ws2_32)
else()
  target_sources(platform_net PRIVATE platform_net_posix.cpp)
endif()

# Прогон по петле 127.0.0.1 — вне ветки WIN32 по тому же основанию, что env/shmem/process:
# обе реализации отдают «работает» одинаково, и расхождение молчит до живого прогона.
# Threads нужен ожиданию датаграммы: петля быстра, но «быстра» — не утверждение.
add_executable(platform_net_address_test platform_net_address_test.cpp)
target_link_libraries(platform_net_address_test PRIVATE platform_net)

add_executable(platform_net_socket_test platform_net_socket_test.cpp)
target_link_libraries(platform_net_socket_test PRIVATE platform_net)

add_executable(platform_net_test platform_net_test.cpp)
target_link_libraries(platform_net_test PRIVATE platform_net Threads::Threads)
