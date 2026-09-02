# Спецификация #4: Система ввода (клавиатура/мышь/тачпад/геймпад, action-map, детерминизм)

- **Дата:** 2026-07-20
- **Статус:** **Validated** — walking-skeleton input-вертикаль PoC закрыта (T4, все гейты зелёные,
  live на owner-HW подтверждён, см. «Результат PoC»). ADR
  [0005](../decisions/2026-07-20-input-system.md) → Accepted.
- **Наследует:** [спека #1](2026-07-18-language-and-core.md) (детерминизм: fixed timestep, sim-hash,
  fix32, инвариант «выход не кормит сим» — здесь зеркально: **ввод — единственный недетерм. вход**,
  и он обязан быть детерминированно записан/воспроизведён), [спека #2](2026-07-18-render-pipeline.md)
  (GLFW-окно = desktop event-pump platform-части HAL).

## Контекст

Ввод — **вход** в симуляцию и зеркало output-доменов (рендер #2, аудио #3). Столп №4 брифа:
«поддержка из коробки клавиатуры, мыши, тачпада, геймпада (все! Sony/Microsoft/Logitech/etc)».
Драйвер архитектуры — тот же, что везде: **детерминизм** (столп №1, инвариант #2 спеки #1). Реплей,
rollback и netplay ([спека #22](2026-09-02-deterministic-net.md)) держатся на том, что ввод сэмплится в фиксированной точке тика и
записывается как воспроизводимый поток, а НЕ читается асинхронно по wall-clock. Уровень — серьёзный
indie-инструмент (перебинды, контексты, несколько игроков), не «две кнопки».

## Требования

### Функциональные
- **Модель потоков (2 домена):** (1) **input-поток / опрос устройств** — ОС доставляет async-события
  (клавиши, движение мыши, оси/кнопки геймпада, hot-plug) с собственным таймингом; они складываются в
  **lock-free SPSC-очередь** сырых событий с монотонной меткой. (2) **sim-поток** — в **sync-точке
  тика** дренит очередь и коалесцирует события в **`InputFrame` @tick N** (состояния Action + значения
  осей). Метка события — **номер тика**, в который его увидел sim (tick N), НЕ wall-clock → частота/
  джиттер опроса не влияют на то, в какой тик попадёт ввод.
- **Слой действий (Full-scope):**
  - **Action** (цифровое): `pressed/held/released` за тик (edge + level). **Axis1D/Axis2D**
    (аналоговое, fix32): стики/триггеры/WASD-как-ось/движение мыши; **dead zone** (radial для 2D,
    linear для 1D), нормализация, инверсия, чувствительность.
  - **Binding** device-agnostic: Action ← {Key | MouseButton | PadButton | ...}; Axis ← {пары клавиш |
    PadAxis | MouseDelta}. Несколько биндингов на одно действие (kbd И pad одновременно).
  - **Контексты ввода**: стек активных карт (`Gameplay/UI/Menu`); верхний контекст может «съедать»
    ввод (consume) или пропускать ниже. Переключение контекста детерминировано (в тике).
  - **Rebind-hooks**: рантайм-перебиндивание (карта — данные, не код), «listen for next input» API;
    UI перебиндов — IDE (спека #7), здесь только механизм.
  - **Per-player device assignment**: устройство → слот игрока (P1/P2…); hot-plug назначает/освобождает;
    несколько геймпадов → несколько игроков.
  - **Input buffer (N тиков)**: кольцевой буфер последних N `InputFrame`; API «была ли Action нажата
    в последние N тиков» (прыжок-буфер/leniency). Combo/gesture-recognizer — точка расширения над буфером.
- **Устройства (HAL, platform-native — все 3 бэкенда):**
  - **Windows:** **XInput** (геймпад: Xbox-класс, вибрация) + **RawInput** (kbd/mouse, high-precision).
  - **macOS:** **GameController.framework** (GCController: Xbox/DualSense/Switch + haptics/rumble,
    hot-plug) + kbd/mouse через native-хэндл окна (Cocoa event-pump).
  - **Linux:** **evdev** (`/dev/input/event*`) + **libudev** (hot-plug/enumeration) для kbd/mouse/pad.
  - **Null/synthetic device** (проигрывание записанного `InputFrame`-потока) для CI и golden-теста.
- **Rumble/haptics:** per-устройство API `set_rumble(low, high, duration)`; бэкенд-специфично (XInput
  vibration / GameController haptics). Best-effort, не в детерм.-пути.
- **Vendor-охват:** маппинг кнопок/осей по vendor/product (Xbox/PlayStation/Switch/generic-HID);
  таблица маппингов — данные (аналог GameControllerDB), пополняемая.

### Нефункциональные (столп №1)
- **RAM:** очередь событий, `InputFrame`-буфер, пул устройств, карты биндингов — **преаллоцированные**;
  без per-frame heap на горячем пути дрейна/коалесценции.
- **CPU:** опрос/декод HID вне sim-потока; в тике — только дрейн очереди + разрешение биндингов
  (O(active bindings)). Мышь high-precision без потери сэмплов между тиками (аккумуляция дельт).
- **Детерминизм (инвариант #2 спеки #1):** `InputFrame`, поданный в сим за тик N, — **чистая функция
  от записанного потока**, независим от тайминга/частоты опроса ОС. Два прогона одного записанного
  ввода → идентичный sim-hash. Оси — **fix32** (dead-zone/нормализация целочисленно-детерминированы).
  Ввод **никогда** не читается из состояния сим (нет sim→input write-back).
- **Устойчивость:** hot-unplug активного устройства → его Action'ы **нейтрализуются** (release, не
  «залипшая» кнопка), слот игрока освобождается; re-plug — переназначение. Нет устройства/битый HID →
  neutral-placeholder, не crash. Потеря фокуса окна → все held-Action освобождаются (нет «застрявшего» W).

## Архитектура

```
OS-устройства (kbd/mouse/трекпад/gamepad, hot-plug)
        │  platform-native backend (HAL InputSource):
        │  Win: XInput+RawInput · mac: GameController+Cocoa · linux: evdev+libudev
        ▼
  raw events (монотонная метка) ──► SPSC (lock-free, input-поток → sim)
        │
  ┌─ sim-поток (спека #1) ────────────────────────────────────────────────┐
  │  @sync_point тика N:                                                    │
  │   drain SPSC → coalesce → InputFrame{ actions, axes(fix32) } @tick N     │
  │   ActionMap (контексты Gameplay/UI/Menu, dead zone, per-player)          │
  │   push → input-buffer (ring N тиков)                                     │
  │   record → InputRecord-поток (golden) ; debug event-log (не хешится)     │
  │  update(tick): системы читают InputFrame (снапшот), НЕ устройства        │
  └───────────────────────────────┬──────────────────────────────────────── ┘
                                   ▼
                          ECS state (fix32) → sim-hash (golden T4)
  replay: InputRecord-поток → те же InputFrame → идентичный sim-hash
```

- **HAL (спека #1 open Q #2):** ввод — platform-часть HAL (отдельно от RHI/asset-IO/audio-device),
  3 native-бэкенда за интерфейсом `InputSource`; null/synthetic — для CI.
- **Фазирование (KISS, как #2/#3/#5):** проектируем под всё (2 домена, action-map, контексты, буфер,
  per-player, rebable, 3 native-бэкенда, rumble); реализуем desktop-first вертикаль с **live-валидацией
  на macOS + Xbox** (owner HW); Win/Linux native — код + CI-компиляция, live = follow-up; combo/gesture,
  IDE-UX перебиндов — точки расширения.

## API / Интерфейсы

- **InputEngine:** `poll_devices()` (input-поток), `begin_tick(tick)` → дренаж+коалесценция →
  `const InputFrame&`; `record()` / `replay(stream)`.
- **InputFrame:** POD-снапшот тика: `action_state[]` (bitset pressed/held/released), `axis[]` (fix32).
  Детерм. по значению; хешируется в sim-hash.
- **ActionMap:** `bind(Action, Source)`, `bind_axis(Axis, Source, dz)`, контексты
  `push_context/pop_context`, `consume`; rebind: `listen_next(Action)`.
- **InputSource (HAL):** `poll(sink)` / `set_rumble(dev, lo, hi, ms)` / `caps()` — native-бэкенды +
  null/synthetic. Hot-plug через колбэк-события в sink.
- **InputBuffer:** `pressed_within(Action, n_ticks)`; ring последних N `InputFrame`.
- **Player assignment:** `assign(device, slot)` / `on_unplug(device)`.

## UI/UX

Здесь только механизм (карты, контексты, API опроса Action/Axis, буфер, rumble). Визуальный редактор
перебиндов, схемы контроллеров, prompt-иконки (Xbox/PS-глифы), настройка dead-zone кривыми — IDE,
спека #7. Разработчик игры: типизированный `input.pressed(Action_Jump)`, `input.axis(Axis_Move)`,
карта-как-данные (hot-reload через ассет-пайплайн #5 — точка интеграции).

## Edge Cases

- **Hot-unplug** активного pad в held-состоянии → все его Action → released (не залипание); слот P-n
  освобождается; лог; не crash. Re-plug → переназначение (по возможности тот же слот).
- **Потеря фокуса окна** (alt-tab) → все kbd/mouse held → released; при возврате фокуса — свежий опрос.
- **Мышь high-precision:** дельты аккумулируются между тиками (несколько OS-событий → один axis-дельта
  в тике), без потери субпиксельного движения; raw vs OS-accel — raw в детерм.-пути.
- **Трекпад:** на десктопе приходит как mouse (движение/скролл/кнопки); мульти-тач-жесты трекпада —
  точка расширения (не в вертикали).
- **Один физический ввод в неск. контекстах:** верхний контекст `consume` → ниже не доходит; иначе
  каскад. Детерминировано порядком стека.
- **Незнакомый gamepad (нет в таблице маппинга):** generic-HID fallback (оси/кнопки по индексам) +
  лог «unmapped»; не crash.
- **Одновременный kbd+pad на одно Action:** OR-семантика (нажато на любом); ось — приоритет последнего
  активного источника (детерм. правило).
- **Дребезг/повтор клавиш ОС (key-repeat):** фильтруется — Action.pressed = только edge (переход
  up→down), level held отдельно.

## Риски и митигации

| Риск | Вероятность | Митигация |
|------|-------------|-----------|
| **Тайминг/частота опроса ОС течёт в sim** (ломает детерм./реплей) | Высокая | ввод коалесцируется @tick sync-point; метка = tick N, не wall-clock; гейт «record→replay→идентичный sim-hash» + «разная частота опроса → тот же hash» |
| **Async device-поток ↔ sim гонка** | Высокая | lock-free SPSC (input=продюсер, sim=консюмер); нет sim→input write-back; гейт TSan-чисто |
| **Оси недетерм.** (float dead-zone/нормализация cross-arch) | Средняя | fix32-оси, целочисл. dead-zone/нормализация → cross-machine байт-идентичность в записи/хеше |
| **3 native-бэкенда без live-железа под Win/Linux** | Средняя/ожид. | live-валидация на macOS+Xbox (owner HW); Win/Linux native = код + CI-компиляция + deviceless-гейт; live — follow-up (прецедент perceptual-golden #2) |
| **Vendor-маппинг неполон** (не все pad'ы) | Средняя | таблица-данные (аналог GameControllerDB) + generic-HID fallback; Sony-маппинг = unit-тест на записанных HID-дескрипторах (нет Sony-HW у owner) |
| **Hot-unplug → залипшая кнопка** | Средняя | unplug/focus-loss → форс-release всех held; edge-guard |
| **heap-alloc в горячем дрейне** | Низкая | преаллок. очередь/буфер/пулы; гейт no-alloc + ASan |

## Тестовая стратегия

- **Уровень: T4.** DoD = ADR + walking-skeleton input-вертикаль + воспроизводимые регресс-гейты +
  live-проверка на реальном железе (owner).
- **PoC (полная вертикаль):** OS-устройства → native `InputSource` → SPSC → coalesce @tick →
  `InputFrame` → ActionMap (контексты/dead-zone/per-player) → input-buffer → сим (fix32) → sim-hash;
  record→replay. Live-демо (macOS): kbd/mouse/трекпад + Xbox pad + hot-plug + rumble.
- **Гейты (красный = провал):**
  1. **Record→replay→sim-hash:** записанный `InputFrame`-поток → реплей даёт **идентичный sim-hash**
     run-to-run + cross-machine (fix32-оси, целочисл. dead-zone). **Сильнее:** разная частота
     синтетич. опроса (напр. 1000 Hz vs 125 Hz raw-событий) → **тот же** `InputFrame`-поток и sim-hash
     (частота опроса устройства не влияет — реальная гарантия реплей/netplay-детерминизма).
  2. **Нет утечки тайминга/гонки:** async device-поток параллельно с sim; замедленный/джиттер опрос →
     тот же sim-hash; связь строго one-way SPSC (нет sim→input). **TSan-чисто.**
  3. **RT/safety:** нет heap-alloc в дрейне+коалесценции тика (глоб. new-счётчик); hot-unplug/потеря
     фокуса → форс-release (нет залипания), не crash; unmapped-pad → fallback. **ASan/UBSan-чисто.**
- **Live (owner HW, macOS, pinned-T4):** клавиатура + мышь + трекпад + **Xbox pad** (кнопки/оси/
  hot-plug/**вибрация**) через GameController.framework. **Ограничения железа owner:** только 1 Xbox
  pad (не последний), **rumble = только вибрация Xbox**; **DualSense/DualShock отсутствуют** → Sony-путь
  = дизайн + unit-тест маппинга на записанных HID-дескрипторах, НЕ live.
- **Информационно (лог, не гейт):** латентность ввод→тик, throughput опроса, число активных устройств,
  размер очереди.
- Deviceless-гейты (record/replay-det / no-race / RT-safety) — в CI на всех 3 ОС (синтетич.
  `InputFrame`-поток); реальные устройства + rumble = локальный pinned-T4 (как perceptual-golden #2).

## Результат PoC (validated 2026-07-20)

Walking-skeleton input-вертикаль реализована в `poc/input/` — все T4-гейты зелёные (deviceless — в
CI на всех 3 ОС; live — на owner-HW macOS+Xbox). Переиспользованы `fix32` (`poc/src/fixed.hpp`),
FNV-1a (`poc/asset/hash.hpp`), GLFW-окно (спека #2), CMake FetchContent, CI-структура.

- **Гейт #1 (record→replay→sim-hash):** записанный `InputFrame`-поток → реплей даёт байт-идентичный
  sim-hash `0x4c6429af58bde01e` — перепиннут в раунде #15 на `0xcc26a1897a326f6f` вместе с голденом
  ядра — сменой округления `fix32::operator*`; причина и замер в спеке #11, «Гейты приёмки».
  **Сильнее:** тот же хеш при плотности синтетич. опроса d1≡d3≡d8 →
  результат тика **независим от частоты/дробления событий ОС** (идемпотентность уровня кнопок + сумма
  дельт мыши). Оси fix32 (целочисл. dead-zone/нормализация) → cross-machine. ASan/UBSan-чисто.
  Ревью нашло, что гейт был СЛЕП к величине дельты мыши (clamp dead-zone маскировал) — исправлено
  (`scale` оси + знак-корректное дробление), и доказано, что гейт теперь ловит регресс суммирования.
- **Гейт #2 (нет утечки тайминга/гонки):** async device-поток параллельно с sim (джиттер обоих) →
  sim-hash идентичен single-thread. Связь строго one-way SPSC (input=продюсер, sim=консюмер). **TSan-чисто.**
- **Гейт #3 (RT/safety):** глоб. operator-new-счётчик — **0 heap-аллокаций** в дренаже+коалесценции
  (5000 тиков); hot-unplug активного pad и потеря фокуса → force-release всех held (нет залипания);
  unmapped-код/OOB slot-axis-key → fallback без crash. SPSC lock-free. ASan/UBSan-чисто.
- **Гейт #4 (action-map «Full»):** контексты+consume (верхний блокирует нижние), rebind (listen-next
  `capture_source`), per-player device assignment (pad-слот→игрок), input-buffer leniency
  (`pressed_within`) — прямо провалидированы `input_actionmap_test`. ASan/UBSan-чисто.
- **Live (owner HW, macOS):** GLFW-окно (kbd/mouse/трекпад) + GameController.framework (Xbox pad).
  Подтверждено: WASD/Space/ЛКМ, **трекпад** (raw-relative 2D-aim через `GLFW_CURSOR_DISABLED` +
  `RAW_MOUSE_MOTION`, субпиксельная аккумуляция дельт, 60 Гц fixed timestep), Xbox pad (кнопки/стики/
  **hot-plug**/**вибрация** через CoreHaptics). Все устройства управляют `InputFrame`.
- **Native-бэкенды:** macOS (GameController + CoreHaptics, ARC) — полн., live; Windows (XInput +
  RawInput-путь) и Linux (evdev + FF_RUMBLE) — компилируются в CI на своих ОС (таргет `input_demo`),
  live = follow-up (нет HW/ОС). Kbd/mouse — через GLFW-пумп (raw-HID kbd/mouse = точка расширения).

**Тулчейн:** GLFW (окно+kbd/mouse, present из спеки #2), GameController.framework/CoreHaptics (macOS
gamepad+rumble, локально; в CI `-DINPUT_NATIVE` компилирует нужный бэкенд), XInput/evdev (Win/Linux).

**Упрощения / точки расширения:** combo/gesture-recognizer над буфером; мульти-тач жесты трекпада /
мобильный touchscreen; raw-HID kbd/mouse per-OS; Sony/Switch live-маппинг (нет HW → дизайн+таблица);
Win/Linux native live; Steam Input/глифы; карта-как-ассет (hot-reload #5) + IDE-редактор перебиндов
(спека #7); cross-arch байт-идентичность float pad-осей (firm-гейт = fix32-снапшот, целочисл.).

## Открытые вопросы

1. **Live-валидация Win/Linux native** (XInput/RawInput, evdev/libudev) — нужен доступ к железу/ОС;
   сейчас код + CI-компиляция + deviceless-гейт, live = follow-up.
2. **Sony/Switch маппинг live** — нет HW у owner; сейчас маппинг-таблица + unit-тест на записанных HID.
3. **Мульти-тач жесты трекпада / мобильный touchscreen** — точка расширения (десктоп-трекпад = mouse
   сейчас); мобильный touch — при выходе на Android/iOS.
4. **Combo/gesture-recognizer** (fighting-game, charge-moves) над input-буфером — отдельная задача.
5. **Steam Input / платформенные оверлеи** (ремаппинг Steam, глифы) — интеграция при спеке достижений
   (#10) / деплое (#8).
6. **Карта биндингов как ассет** (hot-reload через #5) + IDE-редактор перебиндов/dead-zone кривых —
   спека #7.
7. **Интеграция с netplay/rollback ([#22](2026-09-02-deterministic-net.md)):** `InputFrame`-поток = основа отправки инпутов по сети +
   спекулятивного реплея; политика буферизации/задержки — уточнить в спеке нетворкинга.
