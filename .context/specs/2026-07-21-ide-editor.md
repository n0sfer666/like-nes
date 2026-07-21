# Спецификация #7: IDE — UI/UX и архитектура редактора

- **Дата:** 2026-07-21
- **Статус:** **Proposed** — интервью пройдено (owner-driven, AskUserQuestion ×4 пачки). ADR 0007 —
  Proposed (создать); PoC-вертикаль — pending **T4**. Три выбора уходят в research→ADR / PoC-замер
  (UI-тулкит, рефлексия, IPC-транспорт) по правилу «доказывать данными».
- **Наследует:** [#1](2026-07-18-language-and-core.md) (host владеет state, детерминизм, snapshot/
  rollback, hot-reload shared-lib, SEH/сигнал-изоляция host↔lib, детерм. граф систем),
  [#2](2026-07-18-render-pipeline.md) (render-graph, GLFW-окно — вьюпорт),
  [#3](2026-07-19-audio-subsystem.md)/[#4](2026-07-20-input-system.md)/[#5](2026-07-19-asset-pipeline.md)
  (source→детерм.bake через assetc, content-hash, mmap; action-map=keymap),
  [#6](2026-07-21-plugin-system.md) (**UI-shell**: ImGui docking, манифест→докируемые панели, ext-points).

## Контекст

Столп №3 брифа: **минималистичный IDE, UI/UX первично, ресёрч лучших практик, не изобретать
велосипед**. Движок — «завод по сборке игры»: без встроенных редакторов музыки/картинок; IDE
**собирает и настраивает** контент, а не создаёт его. Спека #6 явно отложила сюда полноценный
редактор (свойства, undo/redo, темы, вьюпорт-сцена). #7 проектирует **каркас редактора** и его
ядровые панели поверх фундамента #1–6.

## Ключевые решения (owner)

| # | Развилка | Решение |
|---|---|---|
| 1 | Композиция IDE | **Монолитное ядро редактора + hook-points** для сторонних плагинов (#6). |
| 2 | Модель сцены | **Плоский ECS-мир (flecs) + иерархия = opt-in `Parent`-компонент** (не навязана). |
| 3 | Формат source | **Текст (TOML/JSON) + стабильные entity-GUID** → bake через assetc #5 в target-native. |
| 4 | Edit↔Play | **Отдельный процесс** (тот же host-движок), редактор = контроллер по IPC; игра в своём окне ОС. |
| 5 | Live-инспекция | **Read-only зеркало состояния по IPC** (нет write-назад → детерминизм/реплей целы). |
| 6 | Авторинг кода | **Внешний код-редактор** (VS Code/CLion); IDE оркеструет watch→build→hot-reload .so + ошибки. |
| 7 | Undo/redo | **Command/transaction-log**, единый линейный стек, группировка (drag=1 команда). |
| 8 | Гизмо/оверлей | **Отдельный immediate debug-draw слой** (сетка/гизмо/selection-outline поверх вьюпорта). |
| 9 | Граф-редакторы | Архитектура **резервирует шов** (generic panel/document/node-graph framework + ext-points); сами anim-FSM/VFX/audio-bus — **отдельные спеки**. |
| 10 | UI-тулкит | **research✓ → Вариант B: гибрид ImGui docking-shell (#6) + точечный натив** (файл-диалоги/IME/DPI). Retained/Qt отвергнут (ADR 0007). PoC валидирует. |
| 11 | Рефлексия property-grid | **research✓ → flecs meta (встроенный аддон) + codegen-хелпер** (шаг #5). Одна рефлексия = property-grid + IPC-зеркало + save/load. C++26 P2996 не для прода-2026. |
| 12 | IPC-транспорт | **research✓ → гибрид shmem(seqlock,read-only) зеркало + socket control**; `IMirrorSink`/`IControlChannel`-абстракция, финал — **PoC-замер** (гейт 8). |

## Требования

### Функциональные (surface #7)

- **Scene/level layout:** вьюпорт (edit-mode, рендер сцены через #2 внутри редактора) + выделение +
  гизмо перемещения/поворота/масштаба (immediate debug-draw).
- **Inspector + property-grid:** редактирование значений компонентов через авто-грид из рефлексии
  ECS-типов (механизм — research, реш. #11).
- **Asset-browser + import-настройки:** обозреватель ассетов, настройки импорта (кодек/компрессия #5),
  hot-reload watch. Создание контента — во внешних DCC.
- **Проект/сцена:** текст-source + стабильные GUID; save/load; bake через assetc #5. Структура на
  диске: `.likenes`-проект, папки `assets/ scenes/ src/`, VCS-friendly, кеш/бейк в `.gitignore`.
- **Play-in-editor:** spawn игры-процесса (тот же host) → read-only IPC-зеркало сущностей/компонентов →
  Stop=kill child. Крэш игры не трогает редактор.
- **Build-оркестрация:** watch `src/*.cpp` → инкрементальный build → hot-reload .so; панель
  build-статуса/ошибок/варнингов, click-to-open в внешнем редакторе на `file:line`.
- **Конфигурируемость интерфейса (в разумных пределах):** темы (свет/тьма/кастом-цвета) · keymap
  (перебинды + пресеты VS/Rider/Blender) · workspaces/перспективы (именованные раскладки) · видимость/
  содержимое панелей+тулбаров. Docking-layout — из #6.
- **Undo/redo:** все мутации сцены/компонентов через command-bus; единый стек; autosave + crash-recovery
  редактора поверх command-log.

### Нефункциональные (инварианты — нарушение = баг)

1. **Детерминизм save/load:** round-trip байт-детерминирован (save→reload→save идентичны); bake сцены —
   тот же детерм. golden-hash gate, что текстуры/аудио #5. Read-only зеркало Play **не пишет** в
   sim → sim-hash/реплей #1 целы.
2. **Изоляция:** крэш игры-процесса не роняет редактор (граница процессов, сильнее host↔lib-изоляции
   #1/#6). Сторонний плагин-панель падает → редактор жив (hook-points переиспользуют изоляцию #6).
3. **Перф (столп №1 — к самому IDE):** отзывчивость на большой сцене (10k+ сущностей): выделение/
   property-grid/undo/иерархия ≤ бюджет (виртуализация списков, без per-frame heap в hot-UI).
4. **Предсказуемость памяти:** UI/зеркало через арены/пулы; IPC-зеркало zero-copy где возможно.
5. **Не изобретать велосипед:** паттерны из зрелых IDE (docking, командный undo, property-grid,
   keymap-пресеты) — подтверждены ресёрчем, отклонения только где явно удобнее.

## Архитектура (эскиз)

```
Editor process (монолит-ядро)                    Game process (Play, отдельный)
 ┌───────────────────────────────┐  spawn/IPC   ┌──────────────────────────┐
 │ docking-shell (#6) · command- │ ───────────▶ │ host-движок #1 (тот же)  │
 │  bus/undo · project-model     │ ◀─ IChannel ─│ sim (fix32, детерм.граф) │
 │  scene-doc (text+GUID)        │  read-mirror │ render #2 → своё окно ОС │
 │ panels: viewport(#2)+gizmo,   │              └──────────────────────────┘
 │  inspector/property-grid,     │  build orchestration
 │  asset-browser(#5), console,  │ ──── watch .cpp → cmake → hot-reload .so ───▶
 │  build-status                 │
 │ hook-points → 3rd-party plugin│  ← reserved: node-graph/document framework
 └───────────────────────────────┘     (anim-FSM/VFX/audio-bus = отдельные спеки)
```

## Тест-матрица (T4) и границы PoC — walking-skeleton (широкий)

| Гейт | Что доказывает | Валидация |
|---|---|---|
| 1 Scene round-trip | save→reload→save байт-идентичны; bake детерм. golden | CI 3 OS |
| 2 Undo/redo | command do/undo/redo корректность + группировка | CI |
| 3 Play spawn+IPC | spawn игры-процесса + read-only зеркало 10k сущностей | CI (headless) + live |
| 4 Крэш-изоляция | крэш игры → редактор жив (граница процессов) | *nix live+CI; Win код+CI-build |
| 5 Build-loop | watch .cpp → build → hot-reload .so, ошибки в панель | live (owner) + CI(build-часть) |
| 6 Live UI | вьюпорт #2 + гизмо + inspector/property-grid | **live, owner-HW** (окно) |
| 7 Перф | 10k+ сущностей: выделение/property-grid/undo ≤ бюджет | CI-бенч + live |
| — IPC-замер | shmem-ring vs socket @ 10k-зеркало → выбор транспорта | PoC-бенч → ADR |

> ⚠️ **Границы (честно):** live-валидация — **macOS (owner-HW)** (как #2–#6); Windows process-spawn/
> IPC/SEH — код+CI-build, live — follow-up. UI-тулкит/рефлексия/IPC-транспорт фиксируются в ADR по
> итогу research/бенча. Граф-редакторы (anim-FSM/VFX/audio-bus) — резерв шва + отдельные спеки.
> Полный live 3rd-party-плагин-панелей, i18n IDE, полный screen-reader — follow-up.

## Риски и митигации

| Риск | Вер. | Митигация |
|---|---|---|
| UI-тулкит выбран неверно (a11y/IME/сложные виджеты/DPI) | сред | **research first** (Blender/Godot/Unity-UITK/Rider) → ADR; абстракция UI-слоя; #6 ImGui как baseline, натив точечно |
| Рефлексия C++ — churn/боль | сред | research (Unreal UHT/EnTT-meta/boost.describe/**C++26 P2996**); fallback = codegen как #5 (единый шаг) |
| IPC-зеркало 10k: latency/трафик/сложность | сред | `IChannel`-абстракция + **бенч shmem vs socket**; zero-copy shmem-ring как гипотеза |
| Отдельный процесс Play: старт/IPC-мост дороже same-process | низк | измерить; выигрыш = 100% крэш-изоляция + паритет с реальной игрой у игрока |
| Монолит → 3rd-party расширения «second-class» | сред | hook-points = **реальный API**; часть панелей догфудить через тот же API |
| Дрейф схемы состояния editor↔game (чтение зеркала) | сред | версионированная схема/общая рефлексия; проверка версии при коннекте |
| Перф ImGui inspector/hierarchy на 10k | сред | виртуализация списков; ленивое раскрытие; гейт 7 ловит регресс |
| C++ build-time в hot-loop убивает итерацию | сред | инкрементальный build только изменённой .so; фон-сборка; кеш |
| DPI/multi-monitor чёткость | низк | часть UI-тулкит-ресёрча; per-monitor scale в архитектуре |
| Потеря несохранённого (крэш редактора) | сред | autosave + crash-recovery поверх command-log |

## Открытые вопросы

1. ~~UI-тулкит~~ — **закрыто research'ем → Вариант B (гибрид ImGui-shell)**, ADR 0007; PoC валидирует.
2. ~~Рефлексия~~ — **закрыто research'ем → flecs meta + codegen-хелпер**, ADR 0007; PoC валидирует.
3. **IPC-транспорт** — подход выбран (shmem+socket), финальный shmem-vs-socket — **PoC-замер (гейт 8)**.
4. **Гизмо 2D-специфика** — screen-space vs world-space хэндлы для 2D-движка (уточнить в PoC).
5. **Мобильный редактор** — вне scope (разработка только desktop, stack.md); IDE = desktop-only.
6. **A11y-потолок** — клавиатурная навигация + контраст-темы в #7; полный screen-reader = риск тулкита (AccessKit-слой follow-up).
