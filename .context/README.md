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
| 11 | Реорганизация репозитория (движок ≠ игра-образец) | **Закрыта** (ADR 0011 Accepted, спека Validated T3, все 7 гейтов: дерево `engine/` + `tools/` + `example_ugly_game/` + `platform/`, имя `poc` исчезло / **8 golden-хешей целы бит-в-бит** (`0xa213fe65…` ядро, `0xf2255dc7…` asset, `0x2cf5b559…` audio, `0x4c6429af…` input, `0x7ad0493f…` plugin, `0x2de54a36…` IDE, `0x32a094e8…` sim, `0xe728fef1…` ach) + `compile_commands.json` **336↔336 TU, 0 расхождений** / CI зелёный на 3 ОС, инвентарь шагов **52=52**, наборы skipped идентичны / корень чист, `git status` пуст после гейтов+паковки+релизного dry-run / граница движок→игра машинно проверена по CMake-графу / `.app` с 7 лицензиями стартует из чужого cwd, sha256 `game.bundle` цел / `release.yml` dry-run. CMake 644 → 142 + 13 per-module. Гейт 3 вскрыл порядко-зависимый флак резолва артефактов, невидимый локально. Долг в ADR: ребро `engine→game` в `plugin_host_test.cpp`, `PUBLIC`-экспорт каталогов — оба в #20) — [`specs/2026-07-26-repo-layout.md`](specs/2026-07-26-repo-layout.md) · [`decisions/2026-07-26-repo-layout.md`](decisions/2026-07-26-repo-layout.md) |
| 12 | Windows-паритет ядра движка | **Код раунда готов, ждёт CI** (ADR 0012 **Proposed**, спека Validated T4, 9 гейтов: шесть швов в `engine/platform/` (`platform_io` / `platform_fs` / `platform_path` / `platform_process` / `platform_module` / `platform_guard`), переключатель — CMake, **ни одного OS-`#ifdef` вне шва** (греп-гейт в CI до сборки) / ассет+аудио+плагины+hot-reload+игра собираются на 3 ОС, все 9 golden-хешей обязаны совпасть / `fork` заменён на `platform::Child` (перезапуск себя через `exe_path`) — kill-during-write больше не POSIX-only / ASan clang-cl отдельным build-каталогом (UBSan под MSVC-таргет рантайма не имеет — явная разница покрытия) / юникод+пробел в пути: бейк, mmap и respawn из «юникод тест-каталог» / гейты 2 и 6 — машинный суррогат (`ma_backend_null`, headless `--demo` с проверкой, что кадры **меняются**), сенсорика — owner-HW follow-up. **Windows-половина локально не компилировалась ни разу** (MSVC у владельца нет): статус ADR поднимается до Accepted только по зелёному раннеру) — [`specs/2026-07-26-windows-core-parity.md`](specs/2026-07-26-windows-core-parity.md) · [`decisions/2026-07-26-windows-core-parity.md`](decisions/2026-07-26-windows-core-parity.md) |
| 13 | Паритет среды разработки на трёх десктопных ОС | **Закрыта** (ADR 0013 Accepted; гейты 1–5 и 7 зелёные на 3 ОС: shmem-зеркало, Play-spawn, build-loop с живым `cl.exe`, диагностики MSVC, перф, кросс-ОС сцена. Единственный `if(NOT WIN32)` в дереве — `ide_ipc_bench`, постоянная граница. Гейты 6 (X11+Wayland) и 8 (сквозной сценарий) закрыты разработчиком на живых сессиях 2026-08-05/06, коммит `0e4294c`) — [`specs/2026-07-26-desktop-dev-parity.md`](specs/2026-07-26-desktop-dev-parity.md) · [`decisions/2026-07-27-desktop-dev-parity.md`](decisions/2026-07-27-desktop-dev-parity.md) |
| 14 | Каркас gameplay-фреймворка + ввод из коробки | **Закрыта** (ADR 0014 Accepted, спека T4, гейты 1–7 машинно: `Schedule` с детерминированным порядком / fix32-форма стика (радиальная и по-осевая мёртвая зона, кривые, триггеры) / пресеты и профили падов — испечённые zero-parse данные в `game.bundle`, golden разрешения ввода `0x416969081dcf230d` на 3 ОС / профиль пада по VID/PID из нового шва HAL `pad_info`, неизвестный → generic, hotplug / перебинды накладкой по ИМЕНИ действия с конфликтом и force, транзакционная запись профиля / игра-образец переведена на пресеты, sim-golden `0x32a094e8…` цел / направление зависимостей подсистемы→слой запрещено грепом в CI / 0 аллокаций в тике. **Гейт 8 (owner-HW) закрыт 2026-08-07** на macOS, Linux (evdev, `pid=0b12`) и Windows (XInput, `pid=02ff`): профиль по паспорту, знаки всех четырёх направлений стика по контракту, перебинд с конфликтом и force, накладка через рестарт, hotplug, игра пройдена с пада — протокол в `docs/owner-verification.md` §3. Живой прогон снёс две редакции свидетеля осей, прошедшие синтетику. Долг в ADR: `write_atomic` продублирован с достижениями; раскладка игры не биндит `mouseaxis:` — трекпад не двигает корабль) — [`specs/2026-07-26-framework-input.md`](specs/2026-07-26-framework-input.md) · [`decisions/2026-07-28-framework-input.md`](decisions/2026-07-28-framework-input.md) |
| 15 | Детерминированная 2D-физика — ядро | **Draft** — [`specs/2026-07-26-physics-core.md`](specs/2026-07-26-physics-core.md) |
| 16 | Движение персонажа и тайлмап-коллизии | **Draft** — [`specs/2026-07-26-character-tilemap.md`](specs/2026-07-26-character-tilemap.md) |
| 17 | Графика из коробки: спрайты, анимации, камера, частицы | **Draft** — [`specs/2026-07-26-graphics-framework.md`](specs/2026-07-26-graphics-framework.md) |
| 18 | Материалы и шейдеры | **Draft** — [`specs/2026-07-26-materials-shaders.md`](specs/2026-07-26-materials-shaders.md) |
| 19 | README и документация en/ru | **Draft** — [`specs/2026-07-26-docs-en-ru.md`](specs/2026-07-26-docs-en-ru.md) |
| 20 | Релизные инсталляторы и скрипт сборки релизов | **Draft** — [`specs/2026-07-26-release-installers.md`](specs/2026-07-26-release-installers.md) |
| 21 | Глобальный аудит кодовой базы (2 независимых ревью) | **Draft** — [`specs/2026-07-26-global-audit.md`](specs/2026-07-26-global-audit.md) |
| 22 | Нетворкинг/rollback (опционально, детерминизм уже заложен) | planned |

> Бывшая строка «Боль 2D-разработчиков — дополнительные фичи» раскрыта спеками **#14–#18**:
> пресеты ввода и шаблоны контроллеров, физика, платформерный контроллер с окнами прощения,
> спрайты/анимации/камера/частицы, библиотека шейдерных эффектов.

## Порядок раундов #11–#21 (и почему такой)

```
#11 реорганизация  ─┬─→ #12 Windows-ядро ──→ #13 паритет среды разработки
                    │                              │
                    └──────────────────────────────┴─→ #14 каркас фреймворка + ввод
                                                          │
                                    #15 физика-ядро ──→ #16 контроллер + тайлмапы
                                                          │
                                    #17 графика ──────→ #18 материалы и шейдеры
                                                          │
                                              #19 документация en/ru
                                                          │
                                              #20 инсталляторы
                                                          │
                                              #21 глобальный аудит
```

- **#11 первым** — все последующие раунды пишут код в новую раскладку; переезд позже стоил бы
  переписывания их путей.
- **#12–#13 до фреймворка** — иначе слой #14–#18 пишется в POSIX-предположениях, и паритет
  превращается в дорогой retrofit. Сейчас на Windows не собирается ассет-пайплайн, аудио,
  hot-reload, изоляция плагинов и весь инструментальный слой IDE (15 блоков `if(NOT WIN32)`,
  24 POSIX-only шага CI).
- **#15 → #16 и #17 → #18** — жёсткие зависимости внутри пар; сами пары друг от друга не зависят
  и могут идти в любом порядке.
- **#19 после фреймворк-серии** — документировать то, что ещё не закрыто гейтами, значит писать
  документацию дважды.
- **#21 последним** — аудит до завершения серии проверял бы код, который заведомо будет переписан.

## Модельная стратегия работы (Claude Code)

- Спеки/интервью/архитектура: **Opus 4.8**.
- Лёгкие/рутинные секции: **Sonnet 5** (экономит недельный лимит Max 5x).
- Тяжёлые автономные прогоны реализации: **Fable 5** — но жжёт лимиты ~2× быстрее Opus,
  для активной разработки рассматривать Max 20x ($200) или API pay-as-you-go.
- План $100 (Max 5x) достаточен для фазы спек.
