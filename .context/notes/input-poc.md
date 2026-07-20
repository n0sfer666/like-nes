# Скретчпад: input-вертикаль PoC (спека #4)

Дата старта: 2026-07-20. Tier: **T4**. Раунд: spec+ADR+PoC (owner-driven, интервью пройдено).

## Развилки (owner, AskUserQuestion 2026-07-20)
- **Backend — platform-native** (НЕ GLFW/SDL): HAL `InputSource` с per-OS бэкендами
  XInput+RawInput (Win) / IOKit+GameController (mac) / evdev+libudev (Linux). Owner выбрал
  **все 3 бэкенда сразу**.
- **Scope — Full:** action-map (перебиндивые Action + оси, dead zones, контексты gameplay/UI/menu)
  + input-buffer (N тиков) + per-player device assignment + rebind-hooks; combo/gesture-recognizer
  — точка расширения.
- **Детерминизм — гибрид:** сим потребляет per-tick `InputFrame` snapshot (хешируемый гейт),
  параллельно debug event-log (не хешится).
- **HW-гейт — + rumble/бренды**, НО по факту железа (owner 2026-07-20): только **1 Xbox pad
  (не последний)**, вибрация есть; **DualShock/DualSense — нет вовсе**.

## Инвариант (ядро дизайна)
Ввод — **вход** в сим (зеркало аудио-выхода). Async OS-события устройств коалесцируются в
**sync-точке тика** в `InputFrame` (состояния Action + оси @tick N), таймштамп — tick, НЕ wall-clock.
Джиттер/частота опроса устройства НЕ меняет, в какой тик попадёт ввод → тайминг не течёт в sim-hash
И реплей воспроизводим. Нет sim→input write-back. (Инвариант #2 спеки #1.)

## Гейты (красный=провал)
1. **Record→replay→sim-hash:** записанный поток `InputFrame` → реплей даёт идентичный sim-hash;
   block/timing-независимость (разная частота опроса → тот же результат). Целочисл./fix32 оси.
2. **Нет утечки wall-clock в sim:** async device-events → coalesce @tick sync-point; TSan-чисто
   (SPSC device-thread → sim). Медленный/джиттер опрос → тот же sim-hash.
3. **RT/safety:** нет heap-alloc в hot-выборке кадра; битое/отключённое устройство (hot-unplug) →
   нейтраль-placeholder (не «залипшая» кнопка), не crash. ASan/UBSan-чисто.

## Тест-матрица (честно — T4)
| Путь | Реализация | Валидация |
|---|---|---|
| Determinism gate (deviceless) | все OS | **CI на всех 3 OS** (синтетич. InputFrame) |
| macOS native (kbd/mouse/трекпад + Xbox pad + hotplug) | macOS | **live, owner HW** |
| Xbox rumble (вибрация) | macOS GameController | **live, owner HW** |
| Windows XInput+RawInput | Win | код + CI-компиляция; **live — follow-up (нет HW)** |
| Linux evdev+libudev | Linux | код + CI-компиляция; **live — follow-up (нет HW)** |
| Sony DualSense/DualShock mapping | все | mapping unit-тест на записанных HID-дескрипторах; **НЕ live (нет HW)** |

## Переиспользование
- `poc/src/fixed.hpp` fix32 — детерм. оси/dead-zone.
- `poc/asset/hash.hpp` FNV-1a — golden sim-hash / input-record.
- GLFW-окно (render #2) — desktop event-pump для kbd/mouse на пути, где native завязан на окно.
- CMake FetchContent, CI-структура (#2/#3/#5): deviceless-гейты на всех POSIX + Win.

## Фазы
1. [ ] docs: spec #4 + ADR 0005 (Proposed) + этот note
2. [ ] core: InputFrame/Action/ActionMap, contexts, dead zones, fix32-оси; SPSC device→sim; coalesce @tick
3. [ ] record/replay + sim-hash golden гейт (deviceless, синтетич. поток)
4. [ ] HAL InputSource seam + 3 native-бэкенда (mac полн. + win/linux скелет), null/offline для CI
5. [ ] input-buffer (N тиков), per-player device assignment, rebind-hooks
6. [ ] live-демо: kbd/mouse/трекпад + Xbox pad + hotplug + rumble (owner HW, macOS)
7. [ ] CI wiring (deviceless-гейты 3 OS + ASan/UBSan/TSan)
8. [ ] verify T4 → code-reviewer → фикс ≥low
9. [ ] ADR Accepted / spec Validated / README #4 / stack.md / dev-log / коммиты по фазам

## Прогресс
- **Фаза 1 (docs):** spec #4 + ADR 0005 (Proposed) + note — готово.
- **Фаза 2 (core):** `poc/input/` — spsc.hpp, input_types.hpp, device_state.hpp (коалесценция,
  идемпотентная по уровню + сумма дельт), action_map.hpp/.cpp (биндинги OR, контексты+consume,
  dead-zone fix32, per-player), input_buffer.hpp, engine.hpp (дренаж@tick + record/replay +
  begin_tick_marked для async), sim.hpp/codes.hpp — готово.
- **Фаза 3 (гейты, deviceless):** ВСЕ ТРИ ЗЕЛЁНЫЕ (локально arm64-macOS + через проектный CMake):
  - Гейт #1 determinism: sim-hash `0x4a767da8c64e2943`; run-to-run + **poll-rate independent
    (d1≡d3≡d8)** + record→replay идентичны. ASan/UBSan чисто.
  - Гейт #2 race/TSan: threaded(jitter) ≡ single-thread `0x4a767da8c64e2943`; one-way SPSC. **TSan-чисто.**
  - Гейт #3 rt-safety: **0 heap-alloc** в дренаже+coalesce (5000 тиков); hot-unplug/focus-loss →
    force-release (нет залипания); unmapped/OOB → fallback без crash. ASan/UBSan чисто.
  - CMake: `input_core` + 3 гейт-таргета (input_determinism/race/rt_test).
- **Фаза 4 (HAL + native):** source.hpp (GamepadSource) + source_glfw.cpp (kbd/mouse/focus пумп) +
  source_gamepad_macos.mm (GameController.framework + CoreHaptics rumble, ARC) + source_gamepad_win.cpp
  (XInput скелет) + source_gamepad_linux.cpp (evdev скелет). CMake INPUT_NATIVE (input_demo, по
  бэкенду на ОС → компиляционная валидация native в CI на всех 3 ОС). Готово.
- **Фаза 5 (Full-слой):** rebind (listen-next capture_source), контексты+consume, per-player, буфер —
  прямо провалидированы `input_actionmap_test` (4-й deviceless-гейт). Готово.
- **Фаза 7 (CI):** ввод-шаги в ci.yml — determinism golden (cross-machine, все 3 ОС) + actionmap +
  rt + race + Linux ASan/UBSan/TSan. Готово.
- **Фаза 8 (review):** code-reviewer FAIL → 2 medium + 4 low + 1 taste. **Все 7 исправлены:**
  - medium: (1) флагман-гейт детерминизма был СЛЕП к сумме дельт мыши (clamp dead-zone маскировал
    величину + баг дробления для отриц.). Фикс: `scale` в оси (мышь 1/16 → суб-единично, не
    клампится) + знак-корректное дробление. **Проверено:** старая формула теперь даёт poll-rate:NO→FAIL
    (защитная сеть восстановлена). (2) macOS haptics MRC→UAF+утечки → включил `-fobjc-arc` + __bridge.
  - low: dz-кламп [0,1); post()-drop-счётчик + лог в демо; win axes-reset на connect; linux erase
    закрытых Dev. taste: нейтральный дефолт RawEvent.kind (TickMark).
  - Re-verify: 4 гейта PASS под ASan/UBSan/TSan. verify/review-state записаны (T4, pass, 7/7).
- **Новый golden sim-hash: `0x4c6429af58bde01e`** (мышь вносит величину). Пиннут в ci.yml.
- **Фаза 6 (live) — ЗАКРЫТА (owner подтвердил 2026-07-20):** kbd/mouse/трекпад + Xbox pad + hotplug +
  вибрация работают. Трекпад-aim потребовал 3 live-фикса сверх гейтов: субпиксельная аккумуляция дельт
  (тачпад <1px → int-усечка в 0), `GLFW_CURSOR_DISABLED`+`RAW_MOUSE_MOTION` (raw-relative, иначе события
  окну под курсором), 60 Гц fixed-timestep кап (иначе относит. дельта размазана по млн тиков). +AimY (2D-aim).
- **Фаза 9 (финализация) — ГОТОВО:** ADR 0005→Accepted, spec #4→Validated + «Результат PoC», README #4
  Закрыта, stack.md (ввод-строка), dev-log записан. Коммит — единый комплексный (verify+review покрыли
  всю вертикаль разом; head=commit-parent).
- **СПЕКА #4 ЗАКРЫТА.** (DualSense/DualShock — нет HW: Sony-путь спроектирован, live=нет; Win/Linux
  native = CI-компиляция, live=follow-up.)
