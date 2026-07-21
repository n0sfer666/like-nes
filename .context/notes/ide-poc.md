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
9. [~] verify T4 (гейты 1+2 локально зелёные + ASan/UBSan чисто) → code-reviewer (gate1 FAIL→5 фикс) →
       re-review gate2 → commit
10. [ ] чекпоинт: dev-log + решение о следующем срезе

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
