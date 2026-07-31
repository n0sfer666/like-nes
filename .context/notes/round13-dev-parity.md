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
- [x] 7 ADR 0013 (Proposed), `docs/first-run.md` (системные зависимости + клон→сборка→редактор),
      опция `LINUX_WAYLAND` (без неё гейт 6 проверяет X11 второй раз через XWayland), замер цикла
      на macOS. **Гейты 6 и 8 — ожидание проверки от разработчика** на его Windows- и
      Linux-машинах: им нужна живая сессия, а не раннер; после
      прогона: скриншоты, замер цикла на Linux/Windows, ADR → Accepted, спека → Validated.

## 2026-08-01: приоритет — живая машина владельца, CI отложен

Решение владельца: **CI на паузе, чиним разворачивание разработки**. На сегодня дерево
разворачивается только на macOS — на Windows сборка встаёт, Arch не проверялся вовсе.

Незакрытый долг (вернуться, когда Windows-машина соберёт дерево):
- `dev` красный по windows-легу; ubuntu и macOS зелёные с `f41359a`.
- Последняя находка CI — `clang-cl` не реализует `/external:templates-` и под `-Werror` валит
  сборку ASan-гейта на `argument unused during compilation`; фикс `8f26778` запушен, прогон
  `30668190136` не досмотрен.
- PR `dev` → `main` за раунд #14 не открыт: правило репозитория требует зелёный CI на 3 ОС.

Почему это вылезло только сейчас: локальный `build_check.sh` на macOS не компилирует ни
Windows-only TU, ни MSVC-диагностики (C4996/C4127/C2589/D9xxx у clang нет вовсе), а ветка
`if(MSVC)` в `cmake/warnings.cmake` здесь не исполняется. См. память
`branch-never-ci-validated`.

**Инструмент против кругов «собрал → упало → жди фикс»:** `scripts\win-dev.bat warn` —
сборка с `LIKE_NES_WERROR=OFF`, весь список предупреждений разом в `build\warnings.txt`,
строгость возвращается тем же прогоном.

**Развилка версий MSVC.** Прогон `8f26778` зелёный на всех трёх ОС — и ровно на нём машина
владельца падает на flecs C4127. Причина: у раннера `windows-latest` cl 14.51 (VS 18), у владельца
14.44 (VS 2022 Build Tools 17.14), и `/external:templates-` работает только на старшем. Следствие
общее: **зелёный CI не доказывает, что дерево собирается на машине разработчика**, и обратно.
