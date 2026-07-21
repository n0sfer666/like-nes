# Скретчпад: IDE — UI/UX и архитектура редактора PoC (спека #7)

Дата старта: 2026-07-21. Tier: **T4**. Раунд: spec+ADR(Proposed)+PoC (owner-driven, интервью пройдено).
Ветка `poc/plugin-system` (продолжаем). Коммит на каждый шаг-риск (как render/asset/audio/input/plugin PoC).

## Развилки (owner, AskUserQuestion 2026-07-21 — PoC-исполнение)
- **Первый срез** — «Фундамент: flecs + scene-doc + round-trip + undo (гейты 1,2)». Детерм. backbone,
  CI-тестируемо на 3 OS, всё остальное строится поверх. Живой UI/процессы — следующие срезы.
- **Горизонт сессии** — «Один срез до зелёного + коммит-per-шаг». Довести гейты 1+2 зелёными в CI,
  чекпоинт, решение о продолжении.

Дизайн-развилки (UI-тулкит/рефлексия/IPC/композиция/…) — уже закрыты в spec #7 + ADR 0007.

## Инвариант (ядро дизайна — зеркало спеки #7)
Одна рефлексия (flecs meta) = property-grid + read-only IPC-зеркало + детерм. save/load. Отсюда:
схема состояния editor↔game **общая по построению** (нет дрейфа). Save/load round-trip **байт-детерм.**
(save→reload→save идентичны); bake сцены — тот же golden-hash gate, что текстуры/аудио #5. Стабильные
entity-GUID отделены от нестабильных flecs-entity-id (GUID — источник истины на диске). Иерархия =
opt-in `Parent`-компонент (не навязана). Все мутации сцены — через command-bus (единый линейный undo-стек).

## Гейты этого среза (красный=провал)
1. **Scene round-trip:** текст-source (JSON) + стабильные GUID → save→reload→save **байт-идентичны**;
   bake сцены → **детерм. golden-hash** (FNV, run-to-run + cross-machine, целочисленный). Сериализация
   идёт через **flecs meta** (доказывает архитектурную ставку «одна рефлексия»), НЕ per-component hand-code.
   fix32 = opaque meta-тип (raw int). Порядок детерминирован: сущности по GUID, компоненты по имени.
2. **Undo/redo:** command do/undo/redo корректность + группировка (drag = 1 команда). Единый линейный
   стек; new-команда после undo обрубает redo-хвост. Мутации через bus, не напрямую в мир.

Остальные гейты (3 Play-spawn+IPC, 4 крэш-изоляция, 5 build-loop, 6 live UI, 7 перф, 8 IPC-замер) —
следующие срезы (спека #7 тест-матрица).

## Переиспользование
- `poc/src/fixed.hpp` fix32 — детерм. значения компонентов / opaque meta-тип.
- `poc/asset/hash.hpp` FNV-1a — golden bake-hash (тот же паттерн, что asset/audio/input/plugin golden).
- flecs meta/json аддоны (амальгама, дефолт) — meta-driven (de)serialize per-component.
- CMake FetchContent + CI-структура — новый блок гейтов в `.github/workflows/ci.yml` (POSIX+all-OS).

## Технич. решения внутри PoC
- **flecs**: FetchContent, пиннутый тег (v4.x). C++ API + meta. JSON-аддон (`ecs_ptr_to_json`/
  `ecs_ptr_from_json`) для per-component meta-driven (de)serialize; внешнюю структуру (GUID-порядок,
  сортировка компонентов) контролируем сами ради детерминизма.
- **fix32 opaque**: регистрация 1× как opaque → сериализуется как raw int (детерм., целочисл. golden
  cross-arch, как #6 fix32-golden).
- **Scene-doc**: `Guid{uint64}`-компонент = стабильный ключ на диске; Scene хранит GUID↔entity.
  Компоненты среза: Position/Velocity(fix32), Name(string), Parent(guid) — opt-in иерархия.
- **Command-bus**: типы команд (AddEntity/RemoveEntity/SetField/Reparent), транзакция = группа
  (один undo). Стек линейный, redo-хвост обрубается новой командой.

## Границы (честно)
- Live UI (вьюпорт/гизмо/property-grid render), Play-процесс, IPC shmem/socket, build-loop, перф-бенч —
  **следующие срезы**, не в этом. Здесь только детерм. ядро (гейты 1+2), CI-тестируемое.
- TOML — спека допускает TOML/JSON; берём JSON (flecs json-аддон готов, детерминизм проще). TOML —
  follow-up при желании.

## Фазы (этот срез)
1. [x] docs: note (этот файл) + коммит spec #7 + ADR 0007 (Proposed) [phase-1 docs] — `e5c8836`
2. [x] flecs FetchContent (v4.1.6, flecs_static) + API-сверка (opaque/to_json/from_json/member)
3. [x] scene-doc: components + meta-регистрация + fix32 opaque + Scene(GUID↔entity)
4. [x] serialize: детерм. текст-конверт (GUID-порядок, meta-driven flecs-JSON) + deserialize
5. [x] gate 1: round-trip byte-identical + golden `0x2de54a36e54e0684` + run-to-run → ЗЕЛЁНО
6. [x] command-bus (header-only) + undo/redo + группировка + redo-truncation + destroy-snapshot
7. [x] gate 2: do/undo/redo + группировка + truncation + destroy-restore → ЗЕЛЁНО
8. [x] CI wiring (гейты 1+2 golden-grep на 3 OS + ASan/UBSan Linux)
9. [x] verify T4 (гейты 1+2 зелёные + ASan/UBSan чисто) → code-reviewer ×2 (gate1 FAIL→5; gate2
       FAIL→2low+1taste) → все фикс → commit `21808b5`
10. [x] чекпоинт: dev-log записан; решение о следующем срезе — за owner (Play+IPC / live UI / build-loop)

## Срез 2 — Процесс + IPC (owner-выбор 2026-07-21): гейты 3 (Play-spawn+зеркало), 4 (крэш-изоляция), 8 (IPC-замер)
Главный арх. риск спеки. DoD: (3) spawn game-процесса + read-only shmem-зеркало 10k сущностей,
редактор читает консистентный снапшот; layout-drift → reject. (4) крэш child → parent(редактор) жив,
детектит смерть, чистит. (8) bench shmem vs socket @10k: p50/p99, копии/кадр, CPU% → фиксация
транспорта в ADR 0007. POSIX live+CI (macOS owner); Windows — код+CI-build (follow-up, как #6 SEH).

Модули: `ide/ipc/` — mirror-layout (POD header+MirrorEntity[], schema_hash/layout_version), shmem
(POSIX shm_open/mmap RW-writer/RO-reader), seqlock (single-writer/multi-reader), socket_channel
(unix-socket control + bench-путь). `ide/game_child.cpp` (sim→publish зеркало, `--crash` режим),
`ide/play_spawn_test.cpp` (гейт 3+4), `ide/ipc_bench.cpp` (гейт 8).

Фазы среза 2:
1. [x] ipc/mirror + shmem + seqlock + same-process self-test (195k чтений, 0 torn) — ЗЕЛЁНО
2. [x] game_child (fork/exec) publish → parent read зеркало 10k (гейт 3) + layout-drift reject — ЗЕЛЁНО
3. [x] крэш-изоляция: child null-deref → parent жив + waitpid-детект (гейт 4) — ЗЕЛЁНО
4. [x] ipc_bench shmem vs socket (гейт 8): shmem ~25–50× дешевле → транспорт зафиксирован в ADR
5. [x] CI wiring (POSIX run + Win build-skip) + ASan/UBSan
6. [x] verify T4 (все гейты среза 2 зелёные + ASan) → code-reviewer ×2 (FAIL→1med+4low фикс; re-review
       PASS) → commit `3a500d3` → чекпоинт. **Гейты 5/8 закрыты (1,2,3,4,8); осталось 5,6,7.**

## Срез 3 — Build-loop (owner-выбор): гейт 5 — ✅ ЗЕЛЁНЫЙ, коммит `a676ec6` (dir=`ide/compile/`, не `build*`)
DoD: watch .cpp → build → hot-reload .so + панель ошибок (click-to-`file:line`). Переиспользует
dlopen/hot-reload #1. Build-часть CI-тестируема; live-панель — owner follow-up.
- **Гейт 5 — ✅ ЗЕЛЁНЫЙ:** `ide/build/` — diagnostics (парсер clang/gcc `file:line:col: sev: msg` →
  структурные записи для панели + click-to-open), build_orchestrator (fork/exec компилятора + захват
  stdout+stderr → BuildResult{success,diagnostics}; file_changed mtime-watcher). `build_loop_test`:
  v1 build→load→acc=3; правка→watcher-детект→rebuild→hot-reload v2→acc=33 (host-state пережил, поведение
  сменилось); битая правка→build fail + диагностика error с file:line. `diagnostics_test` — детерм. unit
  (cross-OS, 3 диагностики + краевые). Стабильно ×3, ASan/UBSan чисто. CI: 3 шага (POSIX) + ASan Linux.
- **Границы:** live build-панель (ImGui) + click-to-open во внешний редактор — owner follow-up (гейт 6
  live UI). Инкрементальность = пересборка одной .so (как #1/#6). Windows — follow-up (fork/exec POSIX).

## Срез 5 — Live UI (owner-выбор): гейт 6 — 🔶 data-путь+сборка ✅, live окно owner-pending
DoD: вьюпорт #2 + гизмо + property-grid из flecs meta в ImGui-shell #6. Owner выбрал «собрать shell +
headless data-путь, live за owner».
- **Гейт 6 data-путь — ✅:** `ide/editor/` — property_grid (**meta-driven генерик**: обход EcsStruct
  member-list любого компонента → значения прямым чтением по offset+kind; fix32 opaque→raw, std::string
  opaque→строка, примитивы u64/i32/f32; range из meta для слайдеров), gizmo (2D камера world↔screen
  детерм. round-trip + гизмо screen-space хит-тест). `editor_selftest` (headless): property-grid
  Position/Velocity(fix32)/Name(string)/Parent(u64) значения верны + пустая/missing сущность + камера
  round-trip identity + гизмо хит-тест. ASan/UBSan чисто. **Третья опора «одной рефлексии» закрыта**
  (save/load #1 + IPC-зеркало #3 + property-grid #6 — все из flecs meta).
- **editor_shell — ✅ собран:** `editor_shell_main.cpp` — ImGui docking (#6): Hierarchy (виртуализ.
  ImGuiListClipper) + Inspector (property-grid rows + DragInt2 Position через command-bus + undo/redo) +
  Viewport (ImGui draw-list: сетка + сущности-точки world→screen + selection-outline + гизмо-оси) +
  Console. Линкуется imgui+glfw+GL (PLUGIN_UI on). **Live-запуск окна — owner-HW pending.**
- **Границы:** live-окно на экране — owner (как plugin UI-shell / input_demo). Генерик-opaque
  произвольного C++-типа (не fix32/std::string) в property-grid — follow-up (serialize-callback;
  cursor.get_int для I32-opaque вернул 0 → перешёл на прямое чтение по offset). Редактирование в
  инспекторе — Position typed через bus; полный generic-edit из meta = follow-up.
- **Нюанс (cursor):** `ecs_meta_get_int` НЕ инвокает opaque-serialize для fix32 (→0), хотя get_string
  для std::string работает; чтение значений — прямым offset+kind (детерм., корректно для схемы).
- **БАГ+ФИКС (owner live):** первый запуск editor_shell → SIGSEGV в `flecs_table_ensure_edge`. Причина:
  `Scene`/`CommandBus` были глобалами → flecs::world конструировался в static-init (до main). Фикс:
  состояние → локаль `EditorState` в main(). Подтверждено lldb: после фикса процесс входит в render-loop
  без краша. Урок: **flecs-мир не создавать в static-init**. Ревью рефактора → чисто.
- **WebGPU-бэкенд (owner: OpenGL deprecated опасен):** editor_shell мигрирован с ImGui-OpenGL3 на
  **imgui_impl_wgpu (wgpu-native, как render #2)** — OpenGL на macOS deprecated с 10.14, могут выпилить.
  glfw `GLFW_NO_API` + glfw3webgpu surface + GpuContext (render/gpu.cpp) + `IMGUI_IMPL_WEBGPU_BACKEND_WGPU`.
  Render-loop: surface texture → render pass (clear) → `ImGui_ImplWGPU_RenderDrawData` → submit → present;
  resize → реконфиг surface. **0 deprecation-варнингов** (было 3: glViewport/glClear/glClearColor).
  Запуск под lldb: окно открылось, render-loop без краша.
- **WebGPU-консолидация (owner: мигрировать plugin_ui_shell #6 + проверить остальное):** grep по всему
  репо → единственный OpenGL был в `plugin/ui_shell_main.cpp` (render_demo уже на WebGPU). Мигрировал и
  его; общую WebGPU+ImGui оконную обвязку вынес в шаредный `render/wgpu_imgui.{hpp,cpp}` (configure_surface
  + present) — DRY, оба шелла тонкие (editor_shell_main 72 строки, plugin 162). imgui-либа: opengl3-бэкенд
  УБРАН, только wgpu-бэкенд + wgpu_imgui + webgpu PUBLIC. **OpenGL полностью вычищен из исходников**
  (otool: оба шелла линкуют libwgpu_native, ни один — OpenGL.framework). Оба запускаются без краша (lldb),
  plugin-manifest headless-гейт PASS. memset-варнинг в imgui_impl_glfw — сторонний код imgui (не наш).
- **Заглушены сторонние build-варнинги (owner):** (1) zstd `cmake_minimum_required <3.10` deprecated →
  `CMAKE_POLICY_VERSION_MINIMUM 3.10` (CMake≥3.31; поднимает деп-политику, нашу 3.24 не трогает;
  CMAKE_WARN_DEPRECATED не сработал — zstd переустанавливает политику); (2) imgui_impl_glfw `memset`
  non-trivial → `-Wno-nontrivial-memcall` на imgui-таргете (Clang). Проверено: чистый reconfigure 0
  deprecated, форс-пересборка imgui_impl_glfw 0 варнингов, headless-гейты зелёные. Оба — код зависимостей.

## Срез 4 — Перф 10k (owner-выбор): гейт 7 — ✅ ЗЕЛЁНЫЙ
DoD: выделение/property-grid/undo/виртуализация ≤ бюджет + zero per-frame heap на 10k+.
- **Гейт 7 — ✅:** `ide/perf_test.cpp` (reuse scene_core + command). Override operator new = счётчик
  аллокаций. Результаты (owner-HW ARM): выделение 159µs (≤3ms), property-grid 0.17µs/кадр (≤20µs),
  виртуализация-окно K=64 ~0µs/кадр (≤5µs), undo 0.31µs/оп (≤50µs). **Zero per-frame heap** в hot-UI
  (selection-скан/property-grid/окно = 0 аллокаций). **Масштаб-инвариантность 10k↔50k:** property-grid
  и undo O(1) в размере сцены, окно O(видимого) — не O(N). Стабильно ×3, ASan/UBSan чисто.
- **Технич.:** property-grid hot-путь = typed `try_get<T>` → snprintf в фикс. буфер (zero-alloc);
  виртуализация = плоский отсортир. индекс guid → срез [off,off+K] O(K); выделение = скан by_guid_
  в преаллок. буфер. CI: 2 шага (perf POSIX + ASan Linux).
- **Границы:** meta-cursor generic zero-alloc property-grid (вместо typed try_get) — follow-up для
  произвольных компонентов; undo аллоцирует std::function-замыкание (per-action, не per-frame — ок).
  Счётчик считает только operator new (не flecs-internal malloc — try_get без alloc, но следящий гейт
  уже операторов new). Scale-guard (10k↔50k, порог ×4+ε) ловит ~6x-регресс, не истинный линейный O(N)
  (baseline sub-µs + аддитив на шум CI) — ужесточение = риск флейка; строгий тесный порог = follow-up
  (нагрузка 10k↔100k). Ревью code-reviewer ×2 (FAIL→3med+3low: DCE/malloc-scope/scale-invariance/
  бюджеты/ASan-timing/snprintf-кламп → все фикс; re-review PASS, 1 taste оставлен).

## Прогресс среза 2
- **Гейт 3 (Play-spawn + зеркало) — ✅:** `ide/ipc/` (mirror POD-layout + schema_hash/layout_version,
  seqlock single-writer/multi-reader, POSIX shmem RW-writer/RO-reader), `game_child` (fork+exec,
  публикует sim-зеркало), `play_spawn_test` (parent создаёт shmem owner+RO, читает консистентный
  снапшот 10k: guids+count+единый gen). 5/5 прогонов стабильно.
- **Гейт 4 (крэш-изоляция) — ✅:** crash-режим child (null-deref) → SIGSEGV (обычн.) / SIGABRT (ASan),
  parent детектит через waitpid + жив + читает устаревшее зеркало без краха. Граница процессов = 100%.
- **Layout-drift — ✅:** badlayout-child (layout_version=999) → reader отвергает (magic/layout/schema_hash
  guard до seq_read) — не читает мусор.
- **Гейт 8 (замер) — ✅:** симметрично (produce+транспорт+read у обоих): shmem p50≈11.6µs/p99≈12–13µs
  (0 сериализации, read из mmap) vs socket p50≈200µs/p99≈255µs (320KB/кадр, memcpy+syscall+kernel) →
  **~17× (p50) / ~20× (p99) → data-plane=shmem**.
- **Санитайзеры:** ASan/UBSan чисто (self-test, parent+child); ASan-SEGV в stderr = намеренный крэш child
  (grep по stdout PASS). TSan на seqlock не применяю (data-plane намеренно не-атомарен; в гейтах
  writer/reader — разные процессы, внутрипроцессной гонки нет).
- **Границы:** bench = 2-поток in-process (механизм-стоимость честна: socketpair — реальный kernel-путь);
  Windows IPC/shmem-cleanup — follow-up (POSIX-only, как #6 SEH). double-buffer поверх seqlock —
  оптимизация follow-up (seqlock-retry достаточен и измерен).

## Прогресс
- **Гейт 1 (scene round-trip) — ✅ ЗЕЛЁНЫЙ:** `ide/` — components (Name/Parent/Position/Velocity,
  fix32 opaque→I32, std::string opaque), Scene (flecs::world + std::map<guid,entity>, детерм.
  итерация), serialize (текст-конверт `E <guid>`/`C <Name> <flecs-json>`, meta-driven to_json/
  from_json — доказывает ставку «одна рефлексия»). save→reload→save байт-идентичны; golden
  `0x2de54a36e54e0684` (FNV над ASCII+целочисл. fix32-raw → cross-OS); run-to-run идентичен.
  ASan/UBSan чисто (macOS; LSan→Linux CI). Сущности сорт. по GUID вопреки порядку создания.
- **Гейт 2 (undo/redo) — ✅ ЗЕЛЁНЫЙ:** `command.hpp` (header-only CommandBus): create/destroy/
  set_component<T> через bus, единый линейный undo-стек, транзакции (begin/end_group → drag=1 undo),
  новая команда обрубает redo-хвост, destroy-undo через per-entity snapshot (reuse рефлексии).
  Оракул — serialize() гейта 1. ASan/UBSan чисто.
- **Ревью:** code-reviewer (gate1) → FAIL: 2 medium (OOB `+2` в парсере, C-до-E краш) + 3 low
  (dup-guid leak, вакуумные command-цели, CRLF) — все исправлены (starts_with-парсер, have_cur guard,
  destruct старой сущности в create, header-only command, `\r`-trim). Re-review gate2 — pending.
- **Границы:** формат = строковый конверт вокруг flecs-JSON (не полный TOML/JSON-парсер) — как
  line-based .manifest в #6; реальный TOML/JSON doc-парсер = follow-up. Live UI/Play/IPC — след. срезы.
