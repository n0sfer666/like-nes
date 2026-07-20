# Спецификация #6: Система плагинов + конфигурируемый интерфейс

- **Дата:** 2026-07-21
- **Статус:** **Validated** — walking-skeleton plugin-вертикаль PoC закрыта (T4, все 6 гейтов зелёные,
  live UI-shell на owner-HW подтверждён, WASM-sandbox local pinned-T4 на wasmtime; см. «Результат PoC»).
  ADR [0006](../decisions/2026-07-21-plugin-system.md) → Accepted.
- **Наследует:** [спека #1](2026-07-18-language-and-core.md) (детерминизм: fixed timestep, sim-hash,
  fix32, hot-reload shared-lib, SEH/сигнал-изоляция host↔lib, детерм. граф систем),
  [#2](2026-07-18-render-pipeline.md) (render-graph = точка расширения render-pass; GLFW-окно = host
  UI-shell), [#3](2026-07-19-audio-subsystem.md) (audio-шины = ext-point), [#4](2026-07-20-input-system.md)
  (`InputSource` HAL = ext-point), [#5](2026-07-19-asset-pipeline.md) (asset-codec-по-классу = ext-point,
  hot-reload).

## Контекст

Столпы №6 (расширяемость: плагины + конфигурируемый интерфейс) и №3 (минималистичный IDE) брифа.
Движок — «завод по сборке игры»: его ценность в том, что студия/соло-разработчик расширяет и
переконфигурирует его под свой продукт, не форкая ядро. Все предыдущие подсистемы уже спроектированы
с точками расширения (render-pass #2, asset-codec #5, `InputSource` #4, audio-bus #3) — эта спека даёт
**единый механизм**, который эти точки унифицирует и открывает стороннему коду, плюс делает интерфейс
редактора **конфигурируемым данными** (панели из манифестов).

Драйвер архитектуры — те же инварианты, что везде: **детерминизм** (столп №1) и **изоляция сбоев**
(живая разработка не падает целиком). Плагин, трогающий симуляцию, обязан играть по правилам детерм.
графа; недоверенный плагин обязан быть заперт в песочнице. Уровень — серьёзный инструмент (native-перф
для first-party + безопасный WASM для marketplace), не «скрипт-хук на коленке».

## Требования

### Функциональные

- **Граница плагина (2 борта):**
  - **Trusted → native C-ABI shared-lib** (`.so`/`.dylib`/`.dll`): прямой доступ к ECS/HAL,
    zero-cost, hot-reload. Переиспользует host↔lib seam спеки #1 (dlopen/dlsym + host-owned state +
    SEH/сигнал-изоляция границы). Для first-party и доверенных расширений.
  - **Untrusted → WASM-sandbox** (wasmtime/WAMR за HAL): linear-memory изоляция, host-функции =
    единственный выход (capability-импорты). Для marketplace/недоверенного кода. Guest на fix32 →
    детерминизм сохраняется.
- **Единый реестр точек расширения (`ExtensionPointRegistry`):** типизированная регистрация по
  видам швов:
  - **ecs-system** — система в детерм. граф (позиция задаётся зависимостями, НЕ порядком загрузки).
  - **render-pass** — узел render-graph (#2).
  - **asset-codec** — кодек-по-классу ассета (#5).
  - **input-source** — HAL-источник ввода (#4).
  - **audio-bus** — шина/эффект микшера (#3).
  - **ui-panel** — панель конфигурируемого интерфейса.
  Регистрация — **чистая функция от манифеста**: множество и связи расширений не зависят от порядка
  dlopen/инстанциации.
- **Манифест плагина (декларативный):** `plugin.toml` — id, версия, требуемая версия API движка,
  зависимости от других плагинов, объявленные ext-points, запрошенные **capabilities** (для WASM —
  какие host-функции импортирует), config-схема, объявленные ui-панели. Загрузка = разрешение
  зависимостей (топосорт) + проверка версий + проверка capability-политики.
- **Capability-модель (для untrusted):** WASM-гость видит ТОЛЬКО импортированные host-функции,
  перечисленные и одобренные в манифесте. Нет импортированной функции = нет доступа (нет FS/сети/
  host-памяти вне контракта). Native-плагин доверенный (capability-модель — документирована, не
  форсится песочницей).
- **Конфигурируемый интерфейс:** декларативная config-схема (значения-данные, не код) + **минимальный
  живой докируемый UI-shell** — панели (`ui-panel`-расширения) рождаются из манифестов, пользователь
  включает/двигает/докует их. Доказывает «интерфейс конфигурируется данными». Полноценный IDE-редактор
  (свойства, undo/redo, темы, вьюпорт-сцена) — спека #7.
- **Жизненный цикл:** load (разрешение манифеста) → register (ext-points) → activate → [hot-reload
  swap] → deactivate → unload. Сбойный плагин на любой стадии → изоляция + пометка failed, остальные
  живут.

### Нефункциональные (инварианты — нарушение = баг)

1. **Детерминизм (инвариант #2 спеки #1):** golden sim-hash воспроизводим run-to-run/cross-machine;
   sim-плагин реально влияет на него (H_with ≠ H_without → гейт ловит регресс), но H_with НЕ зависит
   от порядка загрузки — порядок исполнения sim-систем определяется детерм. графом (зависимостями),
   НЕ порядком dlopen. Native-плагин и WASM-плагин одной логики дают идентичный sim-hash.
2. **Изоляция сбоев (инвариант #4 спеки #1):** паника/крэш плагина не роняет host/редактор. Native —
   SEH (Win) / сигнал-изоляция (*nix) на границе; WASM — trap в песочнице. Сбойный плагин выключается.
3. **Безопасность untrusted:** WASM-гость не может читать/писать host-память вне своей linear memory
   и не может вызвать host-функцию, не импортированную по манифесту.
4. **Перф (столп №1):** trusted-путь zero-cost (прямой вызов через C-ABI, hot-path без маршалинга);
   маршалинг только на WASM-границе (осознанный trade-off untrusted).
5. **Предсказуемость памяти:** регистрация/маршалинг вне hot-кадра; hot-путь плагина без скрытых
   heap-аллокаций (арены/пулы как в ядре).

## Тест-матрица (T4) и границы PoC

| Путь | Реализация | Валидация |
|---|---|---|
| Determinism gate (native sim-плагин) | все OS | **CI на всех 3 OS** (golden sim-hash с/без + reorder) |
| Determinism native≡WASM | все OS | **CI** (тот же hash из WASM-гостя) |
| Crash-изоляция native (null-deref → disable) | *nix сигнал / Win SEH | *nix live+CI; **Win — код+CI-компиляция** (SEH-путь) |
| Hot-reload swap плагина | все OS | CI + **live (owner macOS)** |
| WASM-sandbox escape (OOB / незаявл. cap → trap) | все OS (wasmtime/WAMR) | **CI** + live owner |
| UI-shell из манифестов (докинг панелей) | macOS | **live, owner HW** (GLFW-окно #2) |
| Реестр: 6 типов ext-point | все OS | ECS+asset-codec сквозной live; render/input/audio/ui — регистрация+unit |

> ⚠️ **Честно зафиксировано (границы PoC):** live-валидация — **macOS (owner HW)**: hot-reload swap,
> WASM escape-тест, UI-shell докинг. **Windows SEH-изоляция — код + CI-компиляция**, live — follow-up
> (нет HW/ОС). Сквозной шов реестра доказывается на **2 представительных ext-point** (ecs-system +
> asset-codec); render-pass/input-source/audio-bus/ui-panel — регистрация через тот же API + unit
> (полный live-интеграционный прогон каждого — отдельные follow-up). Выбор WASM-runtime (wasmtime vs
> WAMR) фиксируется в ADR по итогу фазы 5. Marketplace/подпись/песочница-политика, версионирование API
> (semver-эволюция), полноценный IDE (#7) — точки расширения / отдельные спеки.

## Результат PoC — ✅ все 6 гейтов зелёные (T4)

Реализация `poc/plugin/` (walking-skeleton). Статус гейтов:

- **Гейт 1 (детерминизм) — ✅ ЗЕЛЁНЫЙ.** `plugin_determinism_test`: H_with=`0x7ad0493f0f2ddf47`,
  H_without=`0x7263e404db89bcaf` → плагин реально влияет (ловит регресс); H_with **не зависит от
  порядка загрузки** (топосорт по зависимостям + tie-break по id, не по dlopen); run-to-run стабилен;
  golden совпал Debug≡Release и cross-arch (macOS ARM ≡ Linux x86_64 в CI). ASan+UBSan-чисто.
- **Гейты 2+3 (изоляция + hot-reload) — ✅ ЗЕЛЁНЫЕ.** `plugin_isolation_test`: native-крэш (null-deref)
  пойман на probe-активации (сигнал-изоляция, флаг `armed` скоупит перехват) → плагин disabled, host
  и остальные плагины живы; повторный probe после reload снова ловит (armed re-arm); host-owned state
  переживает hot-swap; дубли/утечки handle при reload отсутствуют. POSIX.
- **Гейт 4 (шов реестра) — ✅ ЗЕЛЁНЫЙ.** `plugin_seam_test`: asset-codec RLE0 decode end-to-end;
  все **6 типов ext-point** (ecs-system/render-pass/asset-codec/input-source/audio-bus/ui-panel)
  регистрируются через единый реестр. ASan-чисто.
- **Гейт 6 (конфиг-интерфейс) — ✅ ЗЕЛЁНЫЙ (headless + live).** `plugin_manifest_test`:
  декларативный манифест → панели + dock-слоты (right/left/bottom/center) + widgets; robustness на
  битом вводе (нет terminate). ASan+UBSan-чисто. **Live `plugin_ui_shell`** (Dear ImGui docking +
  GLFW + GL3): панели рождаются из 2 манифестов и докируются по хинту — **подтверждено live на
  owner-HW (macOS 2026-07-21):** окно отрисовалось, панели видны (как input_demo / miniaudio --play).
- **Гейт 5 (WASM-sandbox) — ✅ ЗЕЛЁНЫЙ (local pinned-T4).** `plugin_wasm_test` (wasmtime C-API v26,
  guest = WAT `gravity.wat` на i32 fix32, загрузка через `wasmtime_wat2wasm` — без wasm-ld):
  **native≡WASM** WASM golden = `0x7ad0493f0f2ddf47` = native (бит-в-бит fix32 через ABI-границу);
  **escape OOB** linear-memory → wasmtime trap, host жив; **escape** вызов незаявленного host-import →
  link rejected (capability-контроль). UBSan-чисто (ASan несовместим с guard-page SIGSEGV wasmtime).

**CI:** determinism/seam/isolation — POSIX (macOS+Linux) + Windows build-валидация; manifest — все 3 OS;
ASan/UBSan — Linux. WASM + live-UI — **local pinned-T4** (C-API/окно не на раннерах), как miniaudio
`--play` / perceptual-golden render #2. ADR **Accepted** / spec **Validated** — все 6 гейтов закрыты.
Коммиты: `4a8fc55` native · `749408d` UI · `4fe43a3` CI · `f839d4b`/`758b8f7` docs · +WASM (этот).
