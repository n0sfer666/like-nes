# like-nes — 2D game engine

Профессиональный 2D-игровой движок с приоритетом на оптимизацию (RAM/диск/CPU/GPU),
заложенную в архитектуру. Не «самый простой для старта», а инструмент для серьёзных
indie-продуктов уровня Hollow Knight. Только движок — «завод по сборке игры», без
встроенных редакторов музыки/картинок.

Полный оригинальный бриф: [`notes/original-brief.md`](notes/original-brief.md).

## Столпы

1. Оптимизация (приоритет №1) — в архитектуре, без «танцев с бубном».
2. Кроссплатформенность: desktop + mobile на старте, консоли позже.
3. Минималистичный IDE (UI/UX первично).
4. Устройства ввода из коробки (kbd/mouse/touch/gamepad).
5. Серьёзная 2D-графика и спецэффекты.
6. Расширяемость (плагины + конфигурируемый интерфейс).
7. Один язык логики, обусловленный оптимизацией.
8. Лицензирование: опенсорс + royalty + friendship.
9. Система достижений (интеграция Steam и др.).
10. Учёт «боли» 2D-разработчиков.

## Roadmap спек (черновой, ≥10 маленьких)

| # | Спека | Статус |
|---|---|---|
| 1 | Выбор языка + архитектура ядра | **Закрыта** (C++, PoC validated, CI 3 ОС) — [`specs/2026-07-18-language-and-core.md`](specs/2026-07-18-language-and-core.md) |
| 2 | Рендер-пайплайн и спецэффекты (2D, Hollow-Knight-уровень) | **Закрыта** (ADR Accepted, render-вертикаль PoC validated T4) — [`specs/2026-07-18-render-pipeline.md`](specs/2026-07-18-render-pipeline.md) |
| 3 | Аудио-подсистема | **Закрыта** (ADR 0004 Accepted, audio-вертикаль PoC validated T4: byte-golden+block-независимый микс / нет sim-readback+TSan / RT-safe callback+ASan / шов asset→audio real vorbis / hybrid fix32-детерм.) — [`specs/2026-07-19-audio-subsystem.md`](specs/2026-07-19-audio-subsystem.md) |
| 4 | Система ввода (все устройства) | **Закрыта** (ADR 0005 Accepted, input-вертикаль PoC validated T4: record→replay sim-hash+poll-rate-независимость / нет-гонки+TSan / RT-safe+force-release / action-map full / live owner-HW: kbd+mouse+трекпад+Xbox pad+hotplug+rumble) — [`specs/2026-07-20-input-system.md`](specs/2026-07-20-input-system.md) |
| 5 | Ассет-пайплайн (формат, компрессия, стриминг) | **Закрыта** (ADR 0003 Accepted, asset-вертикаль PoC validated T4: байт-golden bake / zero-copy+ASan / детерм.-гейт / шов asset→render / hot-reload) — [`specs/2026-07-19-asset-pipeline.md`](specs/2026-07-19-asset-pipeline.md) |
| 6 | Система плагинов + конфигурируемый интерфейс | **Закрыта** (ADR 0006 Accepted, plugin-вертикаль PoC validated T4: детерм.-реестр/топосорт (native≡WASM golden `0x7ad0493f`) / крэш-изоляция+hot-reload+re-probe / 6 ext-point + asset-codec seam / WASM-sandbox wasmtime (OOB+cap escape→trap) / манифест→докируемые ImGui-панели live-owner) — [`specs/2026-07-21-plugin-system.md`](specs/2026-07-21-plugin-system.md) |
| 7 | IDE: UI/UX и архитектура редактора | **Proposed** (интервью + 3 research-развилки закрыты, ADR 0007 Proposed; монолит-ядро+hook-points / отдельный-процесс Play+IPC read-only-mirror / внешний код-редактор / command-undo / текст-source→bake #5; **UI=гибрид ImGui-shell**, **рефлексия=flecs meta+codegen**, **IPC=shmem+socket**; PoC pending T4) — [`specs/2026-07-21-ide-editor.md`](specs/2026-07-21-ide-editor.md) · [`decisions/2026-07-21-ide-editor.md`](decisions/2026-07-21-ide-editor.md) |
| 8 | Билд/деплой и кросс-компиляция | planned |
| 9 | Лицензирование + royalty + friendship | planned |
| 10 | Система достижений (Steam+) | planned |
| 11 | Нетворкинг/rollback (опционально, детерминизм уже заложен) | planned |
| 12 | «Боль 2D-разработчиков» — дополнительные фичи | planned |

## Модельная стратегия работы (Claude Code)

- Спеки/интервью/архитектура: **Opus 4.8**.
- Лёгкие/рутинные секции: **Sonnet 5** (экономит недельный лимит Max 5x).
- Тяжёлые автономные прогоны реализации: **Fable 5** — но жжёт лимиты ~2× быстрее Opus,
  для активной разработки рассматривать Max 20x ($200) или API pay-as-you-go.
- План $100 (Max 5x) достаточен для фазы спек.
