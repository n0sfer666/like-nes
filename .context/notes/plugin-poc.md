# Скретчпад: плагины + конфиг-интерфейс PoC (спека #6)

Дата старта: 2026-07-21. Tier: **T4**. Раунд: spec+ADR+PoC (owner-driven, интервью пройдено).
Ветка `poc/plugin-system`. Коммит на каждый шаг-риск (как render/asset/audio/input PoC).

## Развилки (owner, AskUserQuestion 2026-07-21)
- **Граница (ABI)** — «Native C-ABI + WASM-точка расширения», НО гейт-выбор (см. ниже) поднял
  WASM до live-требования → эффективно **оба борта в этом раунде**: trusted → native C-ABI
  shared-lib (переиспользуем hot-reload + SEH/сигнал-изоляцию); untrusted → **WASM-sandbox live**.
- **Surface** — «Полный реестр всех seam'ов»: единый `ExtensionPoint` на ECS-систему · render-pass
  (#2) · asset-codec (#5) · input-source (#4) · audio-bus (#3) · ui-panel.
- **Конфиг-интерфейс** — «Механизм + минимальный UI-shell»: декларативный манифест плагина +
  config-схема + **минимальный живой докируемый UI-shell** (панели рождаются из манифестов).
  Полноценный IDE-редактор = спека #7.
- **DoD/гейты** — «+ WASM-sandbox live-гейт»: полный набор (детерминизм + изоляция + hot-reload +
  шов) ПЛЮС живой escape-тест WASM-песочницы.

## Инвариант (ядро дизайна)
Плагин — **расширение host'а через объявленные точки**, НЕ параллельный владелец state.
Зеркало инвариантов спеки #1: (#1) весь мутабельный game-state в host, плагин stateless по
контракту sim-швов; (#2) sim-плагин обязан fixed-point + стабильный порядок в детерм. графе →
не возмущает golden sim-hash; (#4) паника/крэш плагина изолируется на границе (SEH/сигнал) →
плагин выключается, host жив. Untrusted-плагин физически не может тронуть host-память вне
контракта (WASM linear memory + capability-импорты). Регистрация расширений = чистая функция от
манифеста, не от порядка загрузки .so.

## Гейты (красный=провал)
1. **Детерминизм:** golden sim-hash воспроизводим run-to-run/cross-machine, при этом (а) sim-плагин
   РЕАЛЬНО влияет на hash (H_with ≠ H_without → гейт ловит регресс, зеркало input-дельты), (б) H_with
   НЕ зависит от порядка загрузки плагинов (топосорт по зависимостям, НЕ по dlopen), (в) native-плагин
   ≡ WASM-плагин той же логики. Sim-плагин на fix32.
2. **Изоляция:** крэш/паника в native-плагине (null-deref) ловится на границе → плагин помечен
   failed и выключен, host продолжает, остальные плагины живы. ASan/UBSan-чисто.
3. **Hot-reload:** hot-swap плагина (.so) без рестарта host; state в host переживает swap
   (переиспользуем hotreload-seam).
4. **Шов (реестр реален):** плагин через `ExtensionPoint` реально драйвит ≥1 существующую
   подсистему сквозным примером (min: ECS-система в детерм. графе + asset-codec-регистрация).
5. **WASM-sandbox:** untrusted WASM-плагин выполняется в песочнице; попытка выйти за границы
   (OOB linear-memory read/write, вызов неимпортированного capability) → trap, host жив, память
   host'а не тронута. Native≡WASM sim-hash (гейт 1в) доказывает эквивалентность контракта.

## Тест-матрица (честно — T4)
| Путь | Реализация | Валидация |
|---|---|---|
| Determinism gate (native sim-плагин) | все OS | **CI на всех 3 OS** (golden sim-hash) |
| Determinism native≡WASM | все OS | **CI** (тот же hash из WASM-плагина) |
| Crash-изоляция native | *nix (сигнал) + Win (SEH) | *nix live+CI; Win — код+CI-компиляция |
| Hot-reload swap | все OS | CI + live (owner macOS) |
| WASM-sandbox escape (OOB/cap) | все OS (wasmtime/wamr) | **CI** (trap-гейт) + live owner |
| UI-shell из манифестов (докинг) | macOS | **live, owner HW** (GLFW-окно #2) |
| Реестр: 6 типов ext-point | все OS | ECS+asset-codec live; render/input/audio/ui — регистрация+unit |

## Переиспользование
- `poc/hotreload/host.cpp` — C-ABI dlopen/dlsym + sigsetjmp/siglongjmp изоляция + host-owned state.
- `poc/hotreload/game_api.h`, `game_crash.cpp` — шаблон C-ABI контракта + crash-плагин.
- `poc/src/fixed.hpp` fix32 — детерм. sim-плагин / golden.
- `poc/asset/hash.hpp` FNV-1a — golden sim-hash.
- `poc/render/` GLFW-окно — host для UI-shell (докируемые панели).
- Детерм. граф систем (спека #1) — точка вставки ECS-плагина.
- CMake FetchContent + CI-структура (#2/#3/#5/#4): гейты на 3 OS + ASan/UBSan.

## Контракты / ограничения ядра (зафиксировано после ревью 2026-07-21)
- **`Registry::schedule()` отдаёт сырые fn-ptr из .so.** Расписание НЕВАЛИДНО после `unload`/`reload`
  (в не-ASan сборке dlclose размапливает код). Контракт: **пересобирать расписание после любого
  unload/reload** (`run_sim` планирует заново каждый вызов; isolation-тест пересобирает после reload).
- **`probe_and_disable_crashers` — single-shot, tick=0.** Ловит крэшеры, падающие детерминированно на
  начальном состоянии. Крэшер, падающий только на конкретных данных/тике позже, probe пройдёт →
  умрёт в `run_sim` (systems там вызываются напрямую, без per-tick изоляции ради детерминизма/перфа).
  Это осознанная граница PoC; расширение — многотиковый/фаззинг-probe (follow-up).
- **Сигнал-изоляция — single-thread.** `g_plugin_jmp`/хендлеры глобальны на процесс; `safe_call_system`
  корректен только в одном потоке. Изоляция **заскоуплена флагом `g_armed`**: вне взведённого guarded-
  вызова хендлер восстанавливает default-disposition и re-raise (реальный крэш в host = нормальный
  крэш, НЕ прыжок в мёртвый фрейм). Windows — SEH, follow-up.
- **ABI-версия энфорсится:** плагин экспортирует `plugin_abi_version` (макрос `PLUGIN_EXPORT_ABI`);
  host сверяет с `PLUGIN_API_VERSION` ДО вызова `plugin_main`, несовпадение → реджект (нет вызова
  ABI-несовместимого .so). Дубли ecs-system id реджектятся (тотальный порядок tie-break сохранён).
- **ASan × dlclose (macOS):** `dl_close` компилируется вне ASan-сборки (`__has_feature`) — dlclose
  выгружает модуль, на который ссылаются shadow/интерцепторы ASan → SIGSEGV в рантайме ASan.
  Реальный dlclose/hot-reload — обычная сборка (isolation-гейт); ASan валидирует не-unload пути.

## WASM-runtime (решение внутри PoC)
- **Выбран: wasmtime** (owner, AskUserQuestion 2026-07-21) — Cranelift-JIT, зрелый C-API/WASI/доки.
  Fix32-путь (только i32-операции) детерминирован и под JIT (нет float-reassoc). За HAL, C-API.
- Guest-тулчейн: `clang --target=wasm32` (fix32-логика в guest → .wasm), host-импорты = capability.
- Гейт: OOB linear-memory trap + вызов незаявленного import → trap + native≡WASM sim-hash.

## Фазы
1. [ ] docs: spec #6 + ADR 0006 (Proposed) + этот note
2. [ ] core: `PluginHost` + `ExtensionPointRegistry` (typed reg: ecs-system/render-pass/asset-codec/
       input-source/audio-bus/ui-panel) + манифест-парсер + capability-таблица
3. [ ] native C-ABI seam: plugin_api.h (C-ABI), загрузчик (переиспользуем hotreload), ≥1 ECS-плагин
       в детерм. графе + asset-codec-плагин
4. [ ] determinism gate: golden sim-hash с/без плагина + переупорядочивание (deviceless, CI)
5. [ ] WASM-sandbox: выбор runtime (wasmtime/wamr) за HAL, WASM-плагин той же sim-логики,
       escape-тест (OOB/cap-trap), native≡WASM hash-гейт
6. [ ] crash-изоляция гейт (native null-deref → disable, host жив) + hot-reload swap
7. [ ] UI-shell: минимальный докируемый shell (GLFW #2), панели из манифестов, live owner
8. [ ] CI wiring (гейты 3 OS + ASan/UBSan; WASM-гейт)
9. [ ] verify T4 → code-reviewer → фикс ≥low
10. [ ] ADR Accepted / spec Validated / README #6 / stack.md / dev-log / коммиты по фазам

## Прогресс
- **Фаза 1 (docs):** spec #6 + ADR 0006 (Proposed) + note — готово.
- **Фазы 2–4,6 (native-вертикаль):** `poc/plugin/` — registry (типизир. 6 ext-point + топосорт-
  планировщик, tie-break по id → порядок-независимость), host (кросс-платформ. загрузчик
  dlopen/LoadLibrary + сигнал-изоляция POSIX + probe-активация), builtin sim + runner, 5 плагинов
  (gravity/wind/rle_codec/multi/crash). Гейты **зелёные**:
  - Гейт 1 (determinism): H_with=`0x7ad0493f0f2ddf47`, H_without=`0x7263e404db89bcaf`; order-indep + run-to-run + эффект. ASan+UBSan-чисто.
  - Гейт 4 (seam): RLE0 codec decode OK; все 6 типов ext-point (ecs/codec/render/input/audio/ui) регистрируются. ASan-чисто.
  - Гейты 2+3 (isolation+hot-reload): zzz_crash пойман на probe → disabled, host жив, ecs=4; host-state переживает swap. Обычная сборка (реальный dlclose).
  - **Нюанс:** ASan×dlclose на macOS → SIGSEGV в рантайме ASan (ограничение платформы, не баг: UBSan чист).
    `dl_close` компилируется вне ASan-сборки (`__has_feature(address_sanitizer)`); реальный dlclose/hot-reload — в обычной сборке (isolation-гейт).
- **Осталось:** фаза 5 (WASM-sandbox: runtime + escape + native≡WASM), фаза 7 (UI-shell), манифест
  plugin.toml (version/deps/capabilities), CI wiring, verify T4 + review + финализация.
