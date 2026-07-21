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
1. [ ] docs: note (этот файл) + коммит spec #7 + ADR 0007 (Proposed) [phase-1 docs]
2. [ ] flecs FetchContent + smoke (meta+json собираются и работают на owner-HW)
3. [ ] scene-doc: components + meta-регистрация + fix32 opaque + Scene(GUID↔entity)
4. [ ] serialize: детерм. JSON (GUID-порядок, meta-driven per-component) + deserialize
5. [ ] gate 1: round-trip byte-identical + golden bake-hash (тест) → зелёно
6. [ ] command-bus + undo/redo + группировка
7. [ ] gate 2: do/undo/redo + группировка (тест) → зелёно
8. [ ] CI wiring (гейты 1+2, 3 OS / POSIX + golden-hash grep) + ASan/UBSan
9. [ ] verify T4 (гейты 1+2 зелёные локально + CI) → code-reviewer → фикс ≥low
10. [ ] чекпоинт: dev-log + решение о следующем срезе

## Прогресс
- (старт) — фаза 1 в работе.
