# ADR 0007: IDE — UI/UX и архитектура редактора

- **Дата:** 2026-07-21
- **Статус:** **Proposed** — интервью пройдено, 3 research-развилки закрыты desk-research'ем (см.
  «Обоснование»); validation gate (walking-skeleton IDE-вертикаль, T4) — **pending**.
- **Контекст:** [спека #7](../specs/2026-07-21-ide-editor.md); наследует [ADR 0001](2026-07-18-language-and-core.md)
  (host владеет state, детерминизм, snapshot/rollback, hot-reload shared-lib, SEH/сигнал-изоляция),
  [0002](2026-07-18-render-pipeline.md) (render-graph, GLFW-окно), [0003](2026-07-19-asset-pipeline.md)
  (source→детерм.bake, content-hash, mmap, кодоген `AssetId<T>`), [0004](2026-07-19-audio-subsystem.md),
  [0005](2026-07-20-input-system.md) (action-map=keymap), [0006](2026-07-21-plugin-system.md)
  (UI-shell: ImGui docking, манифест→панели, ext-points, host↔lib изоляция).

## Проблема

Столп №3: минималистичный IDE, UI/UX первично, «не изобретать велосипед», без встроенных редакторов
контента («завод по сборке»). #6 дал докируемый UI-shell, но полноценный редактор (вьюпорт, инспектор,
undo/redo, темы, Play) отложен сюда. Нельзя нарушить инварианты ядра: детерминизм (edit/save/load и
live-инспекция не должны возмущать sim-hash/реплей) и изоляция сбоев (крэш игры не роняет редактор).
Три подвопроса нельзя закрывать reasoning'ом — их надо доказать данными: **UI-тулкит** (immediate vs
retained для pro-IDE), **рефлексия** (C++ не имеет её для property-grid), **IPC-транспорт** (live-зеркало
10k сущностей @60Гц через границу процессов).

## Решения

| Аспект | Выбор |
|---|---|
| Композиция | **монолитное C++ ядро редактора + hook-points** для сторонних плагинов (#6); hook = реальный API, часть панелей догфудить через него |
| Модель сцены | **плоский ECS-мир (flecs) + иерархия = opt-in `Parent`-компонент**; префаб = flecs-шаблон |
| Формат source | **текст (TOML/JSON) + стабильные entity-GUID** → детерм.-bake через assetc #5 в target-native (git-friendly) |
| Edit↔Play | **отдельный процесс** (тот же host-движок), редактор = контроллер; игра рендерит в **своё окно ОС**; Stop=kill child |
| Live-инспекция | **read-only зеркало по IPC** (`PROT_READ`/`FILE_MAP_READ` — read-only на уровне ОС → не пишет в sim, реплей цел) |
| Авторинг кода | **внешний код-редактор** (VS Code/CLion); IDE оркеструет watch→build→hot-reload .so + панель ошибок, click-to-`file:line` |
| Undo/redo | **command/transaction-log**, единый линейный стек, группировка; autosave + crash-recovery поверх лога |
| Гизмо/оверлей | **отдельный immediate debug-draw слой** (сетка/гизмо/selection-outline) поверх вьюпорта |
| Граф-редакторы | архитектура **резервирует шов** (generic panel/document/node-graph framework + ext-points); anim-FSM/VFX/audio-bus = отдельные спеки |
| **UI-тулкит** | **Вариант B — гибрид: Dear ImGui docking-shell (#6) как ядро + точечный натив** (файл-диалоги, IME/текст, high-DPI) [research] |
| **Рефлексия** | **flecs meta (встроенный reflection-аддон) как единый источник + лёгкий codegen-хелпер** поверх шага #5 [research] |
| **IPC-транспорт** | **гибрид: shared-memory (double-buffer + seqlock) для зеркала + local socket/pipe для control**; за абстракцией `IMirrorSink`/`IControlChannel`, финальный выбор — PoC-замер [research] |
| Конфигурируемость | темы (свет/тьма/кастом) · keymap (перебинды + пресеты VS/Rider/Blender, связь с action-map #4) · workspaces/перспективы · видимость/содержимое панелей; docking-layout из #6 |

## Обоснование research-развилок (доказательно)

**UI-тулкит → B (гибрид ImGui-shell), НЕ retained/Qt.** Immediate-mode **выживает в проде именно для
dev-tools** на масштабе (Tracy — миллионы точек @60fps, RemedyBG — на Dear ImGui). Но **все полноценные
IDE — retained/гибрид**: Unity мигрировала ИЗ IMGUI (per-frame перерисовка «processor intensive») в UI
Toolkit; Unreal Slate — гибрid + «Active Timer/Sleep» чтобы не жечь CPU; Godot/JetBrains — retained.
Qt отвергнут по причинам Blender (вес, внешняя зависимость, лицензия) + противоречит минимализму и
выбросил бы рабочий #6-shell. Известные дыры ImGui (a11y — фундаментальная; IME/CJK/RTL; high-DPI до
1.92; docking не в master) решаемы точечным нативом/апгрейдом ветки, а не переписыванием UI. Источники:
Unity IMGUI→UITK migration docs, Epic Slate Active-Timer, ImGui #8022 (a11y)/#7034 (IME)/#6059 (DPI)/
Wiki-Docking.

**Рефлексия → flecs meta + codegen-хелпер.** Находка: **у flecs есть встроенный meta-аддон** (`EcsStruct`/
`EcsMember`/`EcsEnum`/`EcsBitmask`, вложенность 32 ур., array/vector/map, opaque-типы, meta cursor), а
компоненты инспектора — это и есть flecs-компоненты. **Одна рефлексия покрывает три требования #7
сразу:** (a) авто-property-grid; (b) сериализация значений для read-only IPC-зеркала; (c) детерм.
save/load round-trip — что попутно **снимает риск дрейфа схемы editor↔game** (схема общая по построению).
Прецедент — flecs Explorer (авто-инспектор на meta). fix32 = opaque-тип (регистрация 1×). Boilerplate
`.member()` убирается codegen-хелпером поверх уже существующего шага #5. **C++26 P2996 — не для прода в
2026** (только форк Bloomberg, нет в релизных clang/gcc/MSVC); оставить как будущий шов за codegen-слоем.
Источники: flecs Meta docs/GitHub, flecs-hub/explorer, P2996R13/bloomberg clang-p2996.

**IPC → гибрид shmem+socket.** Индустрия для editor↔game берёт **локальный TCP-сокет** (Godot 6006/6007,
Unity PlayerConnection, Tracy loopback+lock-free-очередь) — но **никто не гонит полное zero-copy зеркало
10k×60Гц**: их данные семплированные/низкочастотные. Наш кейс жёстче → мерить, не копировать вслепую.
Shmem даёт **0 копий и константную latency независимо от размера** (iceoryx2 ~240нс на 64B и 64KB), сокет
— latency растёт с payload (1500→23000нс) + memcpy/сериализация каждый кадр (жжёт CPU, трение со столпом
№1). Поэтому data-plane = shmem (double-buffer + **seqlock** single-writer/multi-reader, read-only mmap),
control-plane = сокет (редкий, авто-cleanup при крэше). Обязательны: **schema_hash/layout_version в
заголовке** (hot-reload .so меняет layout → редактор отвергает несовместимый снапшот, не читает мусор),
полный снапшот (не дельты — при 1.28MB/кадр дёшево). gRPC/TCP для зеркала отклонён. Источники: iceoryx2
#435, Tracy/Unity/Godot docs, Boost.Interprocess #108 (SIGBUS/cleanup-грабли), seqlock-refs.

## Ключевой tradeoff — изоляция процессов ценой IPC

Play-in-editor **отдельным процессом** (не same-process snapshot) — сознательный размен: платим стартом
процесса + IPC-мостом, получаем **100% крэш-изоляцию** (падение игры физически не трогает редактор,
сильнее host↔lib-изоляции #1) + **паритет с реальной игрой у игрока** (тот же бинарь/окно/движок). Риск
IPC-стоимости снят выбором транспорта (shmem zero-copy) и тем, что зеркало **read-only на уровне ОС** —
детерминизм/реплей не могут быть нарушены инспекцией по построению, а не по договорённости. Монолит-ядро
(а не IDE-из-плагинов) — размен скорости старта против чистоты догфуда: смягчается тем, что hook-points
проектируются как настоящий API (#6-реестр) и часть first-party панелей идёт через него.

## Условия финализации (validation gate) — частично, T4

Proposed → Accepted при закрытии walking-skeleton IDE-вертикали (как render/asset/audio/input/plugin PoC),
гейты (спека #7, тест-матрица). Статус PoC (срезы 1–2, POSIX+CI):
1. ✅ **Scene round-trip** — save→reload→save байт-идентичны; golden `0x2de54a36e54e0684` (CI 3 OS). Срез 1.
2. ✅ **Undo/redo** — command do/undo/redo + группировка + redo-truncation (CI 3 OS). Срез 1.
3. ✅ **Play spawn + IPC** — fork+exec game-процесса + read-only shmem-зеркало 10k, консистентный
   seqlock-снапшот (POSIX CI + live macOS). Срез 2.
4. ✅ **Крэш-изоляция** — null-deref child → parent-редактор жив, waitpid-детект (POSIX live+CI; Win —
   follow-up). Срез 2.
5. ✅ **Build-loop** — watch .cpp (mtime) → build (реальный компилятор, захват вывода) → hot-reload .so
   (host-state переживает, dlopen #1); диагностик-парсер (file:line, click-to-open) + build-fail в панель.
   POSIX CI (build-часть); live-панель — owner follow-up. Срез 3.
6. ⏳ **Live UI** — вьюпорт #2 + гизмо + inspector/property-grid на flecs meta (live owner-HW).
7. ✅ **Перф** — 10k+ сущностей (owner-HW): выделение 159µs (≤3ms), property-grid 0.17µs/кадр (≤20µs,
   zero-alloc), виртуализация-окно ~0µs/кадр (≤5µs), undo 0.31µs/оп (≤50µs). **Zero per-frame heap**
   (override operator new = 0 аллокаций в hot-UI пути) + **масштаб-инвариантность 10k↔50k** (property-grid/
   undo O(1), окно O(видимого), не O(N)). CI-бенч POSIX. Срез 4.
8. ✅ **IPC-замер** — **shmem vs socket @10k, симметрично (produce+транспорт+read у обоих), owner-HW ARM:
   shmem p50≈11.6µs / p99≈12–13µs, 0 сериализации (read из mmap); socket p50≈200µs / p99≈252–259µs,
   320KB/кадр (memcpy+syscall+kernel) → shmem ~17× дешевле p50, ~20× p99.** Layout-drift
   (schema_hash/layout_version) → reader отвергает снапшот (валид.). **Решение зафиксировано: data-plane =
   shmem-зеркало (seqlock, read-only mmap), control-plane = socket.** Срез 2. Cross-platform crash-cleanup
   shmem (SIGBUS/orphan) на Windows — follow-up.

> **Итог гейтов:** 7/8 закрыты (1,2,3,4,5,7,8), транспорт зафиксирован. Осталось **6 (live UI, owner-HW:
> вьюпорт+гизмо+property-grid)** — финальный срез; ADR остаётся **Proposed** до его закрытия.

## Последствия

- **+** Детерминизм и изоляция сохранены на уровне ОС; flecs meta унифицирует инспектор/IPC/save-load
  (одна рефлексия, нет дрейфа схемы); переиспользуется #6-shell и #5-bake/кодоген.
- **−** Зависимость от docking-ветки ImGui (пинить тег, абстракция над docking-API); shmem-cleanup при
  крэше — риск (SIGBUS/orphan), закрывается PoC-гейтом 8; a11y — архитектурная дыра (клавнавигация +
  контраст-темы в #7, полный screen-reader = follow-up/AccessKit); C++ build-time в hot-loop (инкрем.
  сборка только изменённой .so).
- **Open:** гизмо 2D-специфика (screen vs world хэндлы) — уточнить в PoC; мобильный редактор вне scope
  (desktop-only); финальный IPC-транспорт — по гейту 8.
