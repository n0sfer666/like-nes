# Hot-reload PoC (dlopen/dlsym + sigsetjmp крэш-изоляция) — POSIX-only (dlfcn/sigaction),
# в Windows-джоб не входит. game_crash на -O0, чтобы null-deref не соптимизировался.
if(NOT WIN32)
    add_library(game_a MODULE game_a.cpp)
    add_library(game_b MODULE game_b.cpp)
    add_library(game_crash MODULE game_crash.cpp)
    target_compile_options(game_crash PRIVATE -O0)
    set_target_properties(game_a game_b game_crash PROPERTIES PREFIX "")
    add_executable(hotreload_host host.cpp)
endif()
