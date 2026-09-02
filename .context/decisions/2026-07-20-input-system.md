# ADR 0005: Архитектура системы ввода (устройства, action-map, детерминизм)

- **Дата:** 2026-07-20
- **Статус:** **Accepted** (2026-07-20) — validation gate закрыт: walking-skeleton input-вертикаль
  PoC (T4) зелёная (все 4 условия финализации выполнены, см. ниже + «Результат PoC» в спеке #4;
  условие #4 подтверждено live на owner-HW: macOS + Xbox pad + трекпад).
- **Контекст:** [спека #4](../specs/2026-07-20-input-system.md); наследует [ADR 0001](2026-07-18-language-and-core.md)
  (детерминизм, fixed timestep, sim-hash, fix32), [ADR 0002](2026-07-18-render-pipeline.md)
  (GLFW-окно = desktop event-pump platform-HAL).

## Проблема

Ввод — единственный **недетерминированный вход** в бит-в-бит воспроизводимую симуляцию спеки #1.
ОС доставляет события устройств (kbd/mouse/трекпад/gamepad) асинхронно, со своим таймингом и частотой
опроса, с hot-plug. Наивное «читать устройство прямо в системах сим» связало бы sim-hash с таймингом/
частотой опроса ОС (джиттер, USB polling rate, потеря фокуса) и убило бы реплей/rollback/netplay
([спека #22](../specs/2026-09-02-deterministic-net.md)). Плюс столп №4 требует «все устройства из коробки» (Sony/MS/Logitech). Нужна архитектура,
которая: коалесцирует async-ввод в детерминированный per-tick снапшот, даёт богатый слой действий
(перебинды/контексты/несколько игроков), покрывает устройства через platform-native бэкенды и
оставляет ввод записываемым/воспроизводимым для netplay.

## Решения

| Аспект | Выбор |
|---|---|
| Модель потоков | **2 домена**: input-поток (async-опрос ОС) / sim-поток (коалесценция @sync_point тика). Между ними — **lock-free SPSC** сырых событий |
| Планирование событий | событие метится **номером тика**, в который его увидел sim (tick N), НЕ wall-clock → частота/джиттер опроса не влияют на sim и реплей |
| Детерминизм (гибрид) | сим потребляет **per-tick `InputFrame` snapshot** (хешируется в sim-hash, основа реплея); параллельно **debug event-log** (сырые события, НЕ хешится) |
| Оси | **fix32**; dead-zone (radial 2D / linear 1D) и нормализация — целочисленно-детерминированы → cross-machine байт-идентичность |
| Слой действий (Full) | **ActionMap**: Action (edge+level) / Axis1D-2D; device-agnostic биндинги (несколько на действие); **контексты** (стек Gameplay/UI/Menu + consume); **rebind** (карта=данные, listen-next); **per-player** device assignment; **input-buffer** (ring N тиков, leniency) |
| Бэкенд/устройства (HAL) | **platform-native, все 3**: Win **XInput+RawInput** · macOS **GameController.framework**+Cocoa · Linux **evdev+libudev**; за интерфейсом `InputSource`; **null/synthetic device** (проигрыш записи) для CI/golden |
| Связь input↔sim | **lock-free SPSC** (input→sim); нет sim→input write-back (зеркало инварианта #3 спеки #1) |
| Rumble/haptics | per-устройство `set_rumble`, бэкенд-специфично (XInput vibration / GC haptics); best-effort, вне детерм.-пути |
| Vendor-охват | таблица маппингов-данных (аналог GameControllerDB) + **generic-HID fallback** для незнакомых pad'ов |
| Устойчивость | hot-unplug/потеря фокуса → **форс-release** held-Action (нет залипания); нет/битое устройство → neutral-placeholder, не crash |

## Ключевой tradeoff — детерминизм при асинхронном вводе

ОС-ввод недетерминирован по таймингу (USB polling rate, джиттер, hot-plug, фокус), а сим спеки #1
обязана быть бит-в-бит воспроизводима. **Решение (зеркало аудио ADR 0004):** сим общается с вводом
**только через per-tick `InputFrame`**, собранный в sync-точке тика дренажом SPSC-очереди сырых
событий. Какой `InputFrame` увидит тик N — **чистая функция от записанного потока событий**, НЕ от
того, когда/как часто ОС физически опросила устройство. Физический опрос/декод HID живёт вне
sim-потока (input-поток). Поэтому: (а) частота/джиттер опроса не могут возмутить sim-hash (инвариант
#2), (б) нет input→sim ↔ sim→input двусторонней связи (one-way SPSC), (в) при одинаковом записанном
потоке `InputFrame` бит-в-бит одинаковы (fix32-оси) → ввод воспроизводим для реплея/netplay и покрыт
golden-хешем. Прод и запись — один путь (в отличие от аудио, где два микшера): детерминизм ввода
дешёвый (снапшот целочисленных состояний), отдельный «float-режим» не нужен.

> ⚠️ **Честно зафиксировано (границы PoC):** live-валидация — только **macOS + 1 Xbox pad** (owner HW;
> rumble = вибрация Xbox). **Windows (XInput/RawInput) и Linux (evdev/libudev) бэкенды — код +
> CI-компиляция + deviceless-гейт**, live — follow-up (нет HW/ОС в этом заходе). **Sony/DualSense/
> DualShock отсутствуют у owner** → Sony-маппинг = дизайн + unit-тест на записанных HID-дескрипторах,
> НЕ live. Firm-гейт детерминизма (record→replay→sim-hash) — deviceless, cross-OS в CI. Combo/gesture,
> мульти-тач трекпада, Steam Input, IDE-редактор перебиндов — точки расширения.

## Условия финализации (validation gate) — ✅ ЗАКРЫТЫ 2026-07-20

Proposed → Accepted, когда walking-skeleton input-вертикаль (T4) закрывает главные риски (реализация
`poc/input/`, детали — «Результат PoC» в спеке #4):
1. **Record→replay→sim-hash:** записанный `InputFrame`-поток → реплей даёт идентичный sim-hash
   run-to-run + cross-machine; **разная частота синтетич. опроса → тот же поток/hash** (тайминг опроса
   не влияет). ✅ `0x4c6429af58bde01e`; poll-rate d1≡d3≡d8 (с ВЕЛИЧИНОЙ дельты мыши в хеше — гейт
   доказанно ловит регресс суммирования); record→replay идентичен. ASan/UBSan-чисто.
2. **Нет утечки тайминга/гонки:** async device-поток параллельно с sim; замедленный/джиттер опрос →
   тот же sim-hash; one-way SPSC. ✅ threaded(jitter) ≡ single-thread. TSan-чисто.
3. **RT/safety:** нет heap-alloc в дрейне+коалесценции тика; hot-unplug/потеря фокуса → форс-release
   (нет залипания), не crash; unmapped-pad → fallback. ✅ 0 heap-alloc (5000 тиков); force-release +
   fallback подтверждены. ASan/UBSan-чисто.
4. **Live (owner HW, macOS):** kbd + mouse + трекпад + Xbox pad (кнопки/оси/hot-plug/вибрация) через
   GameController.framework — реальные устройства управляют `InputFrame` (аналог perceptual-golden #2 /
   miniaudio `--play` #3). ✅ подтверждено owner-ом: WASD/Space/ЛКМ, трекпад (raw-relative aim 2D),
   Xbox pad (кнопки/стики/hot-plug/вибрация) — всё управляет `InputFrame`.

## Последствия

- Даёт вводу тот же детерм.-контракт, что output-доменам (рендер #2 / аудио #3): единственный вход в
  сим — записываемый/воспроизводимый per-tick снапшот; **готовит netplay/rollback ([спека #22](../specs/2026-09-02-deterministic-net.md))** —
  `InputFrame`-поток = основа сетевой отправки инпутов и спекулятивного реплея.
- HAL-граница ввода — ещё один platform-бэкенд наряду с RHI (#2), asset-IO (#5), audio-device (#3);
  3 native-бэкенда за `InputSource`.
- Live Win/Linux native, Sony/Switch live-маппинг, combo/gesture, мульти-тач трекпада, Steam Input,
  IDE-редактор перебиндов/dead-zone, карта-как-ассет (hot-reload #5) — отдельные задачи/спеки позже
  (точки расширения готовы).
- Слой action-map (перебинды/контексты/несколько игроков/буфер) закрывает «боль 2D-разработчика»
  (столп №10 брифа) на уровне ввода без внешних зависимостей.
