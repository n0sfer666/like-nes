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
8. Лицензирование: полностью свободный движок (MIT OR Apache-2.0, без royalty).
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
| 7 | IDE: UI/UX и архитектура редактора | **Закрыта** (ADR 0007 Accepted, IDE-вертикаль PoC validated T4, все 8 гейтов: scene round-trip (golden `0x2de54a36`) / undo-redo+группировка / Play-spawn+read-only shmem-зеркало 10k / крэш-изоляция / build-loop+диагностики / перф 10k zero-alloc / **IPC-замер shmem ~17×** / live UI owner-HW: вьюпорт+гизмо+property-grid из **flecs meta** (одна рефлексия=save/load+зеркало+инспектор), оба shell'а на **WebGPU**) — [`specs/2026-07-21-ide-editor.md`](specs/2026-07-21-ide-editor.md) · [`decisions/2026-07-21-ide-editor.md`](decisions/2026-07-21-ide-editor.md) |
| 8 | Билд/деплой и кросс-компиляция | **Закрыта** (ADR 0008 Accepted, build/deploy-вертикаль PoC validated T4, все 7 гейтов + CI 3 ОС: воспр. билд P0–P2+SHA-пин (macOS 181 / Linux 183 арт. 0 diffs) / шов assetc→бандл BC7 / кросс-компиляция native-matrix (wall=max) + mobile true-cross (iOS-sim+Android NDK, wgpu-native из Rust) / бандл `.app`+tarball+папка самодостаточны / release tag→matrix→VDF+gated steamcmd (rc3-прогон) / **игра-образец** sidescroller-shooter (бой/босс/сюжет/частицы/bloom/аудио, golden `0x32a094e89eacf2f2`) live macOS / **mobile runnable** — полная игра на iOS-симуляторе+Android-эму (реальный тач). Follow-up gated: Steam-заливка/notarization/mobile-подпись) — [`specs/2026-07-23-build-deploy.md`](specs/2026-07-23-build-deploy.md) · [`decisions/2026-07-23-build-deploy.md`](decisions/2026-07-23-build-deploy.md) |
| 9 | Лицензирование (free & open source) | **Закрыта** (ADR 0009 Accepted, спека Validated T3, все 4 гейта: **отмена royalty/friendship** → дуальная `MIT OR Apache-2.0` / инвентарь 14 C/C++-компонентов + 138 Rust-крейтов wgpu-native по факту (все permissive, армы дуальных элегированы явно: zstd→BSD-3-Clause) + **дословные нотисы** в `THIRD-PARTY-NOTICES.txt`, сгенерированном `THIRD-PARTY-NOTICES-RUST.txt` и `THIRD-PARTY-NOTICES-NDK.txt` (инвентарь ≠ notice-retention; границы покрытия записаны явно) / DCO в CI на git-трейлерах (автор **или** коммиттер, cutoff `DCO_SINCE`) / 7 файлов лицензий во всех трёх бандлах (desktop/iOS/APK — проверено сборкой каждого) стоковой сборкой + `FATAL_ERROR` при пропаже / атрибуция из `license.hpp` live macOS; sim-golden `0x32a094e89eacf2f2` цел. Follow-up: юр. ревью перед публичным релизом, per-file SPDX) — [`specs/2026-07-25-licensing.md`](specs/2026-07-25-licensing.md) · [`decisions/2026-07-25-licensing.md`](decisions/2026-07-25-licensing.md) |
| 10 | Система достижений (Steam+) | **Закрыта** (ADR 0010 Accepted, спека Validated T4, все 7 гейтов: детерм. ядро-**наблюдатель** (golden `0xe728fef199e87fc9`) / идемпотентный анлок / крэш-during-write 12/12 валидных снимков / авторинг данными `assetc --game` → zero-parse таблица в бандле #5 + рантайм-`define()`, дубликат id = hard error, 890 бит-флипов отбито / плагинный шов `EXT_ACHIEVEMENT_BACKEND` (**ABI 1 → 2**) + оффлайн-очередь / Steam-адаптер плагином на контракт-стабе `ISteamUserStats` (SDK не вендорится) + реконсиляция union / **регресс #8: `0x32a094e89eacf2f2` bare = observer = observer+плагин**, live macOS с тостом. Follow-up: живой прогон с настоящим SDK (процедура в спеке), Windows-путь бандла, мобильные адаптеры) — [`specs/2026-07-25-achievements.md`](specs/2026-07-25-achievements.md) · [`decisions/2026-07-25-achievements.md`](decisions/2026-07-25-achievements.md) |
| 11 | Нетворкинг/rollback (опционально, детерминизм уже заложен) | planned |
| 12 | «Боль 2D-разработчиков» — дополнительные фичи | planned |

## Модельная стратегия работы (Claude Code)

- Спеки/интервью/архитектура: **Opus 4.8**.
- Лёгкие/рутинные секции: **Sonnet 5** (экономит недельный лимит Max 5x).
- Тяжёлые автономные прогоны реализации: **Fable 5** — но жжёт лимиты ~2× быстрее Opus,
  для активной разработки рассматривать Max 20x ($200) или API pay-as-you-go.
- План $100 (Max 5x) достаточен для фазы спек.
