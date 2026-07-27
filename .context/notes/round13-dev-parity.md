# Раунд #13 — паритет среды разработки (рабочий скретчпад)

Спека: [`../specs/2026-07-26-desktop-dev-parity.md`](../specs/2026-07-26-desktop-dev-parity.md).
Полигон гейтов 6/8 подтверждён владельцем: VM (Parallels/UTM) на macOS-железе.

## Что было на входе

Единственный оставшийся POSIX-блок дерева — `if(NOT WIN32)` в `tools/ide/CMakeLists.txt`:
`ipc_core`, `ide_ipc_selftest`, `game_child`, `play_spawn_test`, `ide_ipc_bench`, `build_core`,
`build_loop_test`, `editor_shell`. Прямые POSIX-вызовы (не `#ifdef`, потому греп-гейт #12 их
не видит, но на MSVC они не компилируются):

| файл | что |
|---|---|
| `tools/ide/ipc/shmem.cpp` | `shm_open`/`mmap`/`ftruncate`/`fcntl` |
| `tools/ide/play_spawn_test.cpp` | `fork`/`execl`/`kill`/`waitpid`/`WIFSIGNALED` |
| `tools/ide/compile/build_orchestrator.cpp` | `pipe`/`fork`/`dup2`/`execvp`/`waitpid` |

## Разбиение на коммиты

1. **`platform_shmem`** — шов в `engine/platform` (`shm_open+mmap` / `CreateFileMappingW+MapViewOfFile`).
   Имя сегмента портируемое (голый токен), декорирует реализация: `/name` против `Local\name`.
   `ide::ipc::shmem` становится тонкой обёрткой. Решение 1 спеки.
2. **`platform::Child` — код завершения.** `wait(ExitStatus&)` с `ExitKind{Exited,Crashed,Killed}`.
   «Убили мы» отличается от «крэшнул сам» флагом в объекте, а не угадыванием кода: на Windows
   `TerminateProcess` задаёт код возврата, и `exit(1)` от него неотличим.
3. **`platform::run_capture`** — запуск с захватом объединённого stdout+stderr (pipe/CreatePipe).
   `run_build` перестаёт быть POSIX.
4. **Диагностики MSVC** — `file(LINE,COL): error C2065: msg` вторым парсером за той же
   `parse_diagnostics`; click-to-open работает от структуры (решение 3).
5. **`platform_watch`** — `inotify` / `ReadDirectoryChangesW` / FSEvents, поллинг — фолбэк
   (решение 4). Три реализации, выбор — CMake.
6. **Снятие `if(NOT WIN32)`** + шаги CI на 3 ОС для гейтов 1–5, 7.
7. **Живые гейты 6 и 8** на VM (X11 + Wayland + Windows), доки системных зависимостей и
   первого запуска, ADR 0013, закрытие спеки.

## Прогресс

- [x] 1 shmem (`ipc_core` не обёрткой, а удалён: после переезда шва в нём остались одни заголовки)
- [x] 2 Child::wait (+ `process_id`; `play_spawn_test` без fork → уехал из POSIX-ветки)
- [x] 3 run_capture (наследование хендлов на Windows ограничено списком → `win32_spawn.cpp`;
      `build_core` уехал из POSIX-ветки, `build_loop_test` остался — там прямой `dlopen`)
- [x] 4 MSVC-диагностики (второй парсер за общей `parse_diagnostics`, `VSLANG=1033`)
- [x] 5 watcher (`platform_watch`: inotify / FSEvents на dispatch-очереди / ReadDirectoryChangesW,
      общий фолбэк-поллинг + `LIKE_NES_WATCH=poll`; попутно `platform::list_dir` в `platform_fs`)
- [x] 6 CMake + CI (`build_loop_test` на Watcher+Module, командная строка компилятора шаблоном
      из CMake; `file_changed` удалён; `editor_shell` вне ветки. Единственный `if(NOT WIN32)` в
      дереве — `ide_ipc_bench`, и это ПОСТОЯННАЯ граница, решение владельца: замер свою работу
      сделал в ADR 0007, порт на Winsock повторял бы принятое измерение)
- [ ] 7 живые гейты + ADR
