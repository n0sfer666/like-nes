# Спецификация #5: Ассет-пайплайн (формат, компрессия, mmap, стриминг, шейдер-кеш)

- **Дата:** 2026-07-19
- **Статус:** **Validated** — walking-skeleton asset-вертикаль PoC закрыта (T4, все гейты зелёные,
  см. «Результат PoC»). ADR [0003](../decisions/2026-07-19-asset-pipeline.md) → Accepted.
- **Наследует:** [спека #1](2026-07-18-language-and-core.md) (кастомный бинформат + mmap + стриминг — *заложено, не валидировано PoC*), [спека #2](2026-07-18-render-pipeline.md) (шейдер-кеш WGSL/SPIR-V, выбор Tint/naga).

## Контекст

Спека #1 заложила «кастомный бинформат + mmap + стриминг», но не проверила PoC. Спека #2
оставила два open-Q, зависящих отсюда: кеш скомпилированных шейдеров в ассет-формате и выбор
транслятора WGSL→IR. Эта спека закрывает оба хвоста и де-рискует ассет-подсистему эмпирикой.
Драйвер — столп №1 (RAM/диск/CPU/GPU в архитектуре). Разблокирует: спека #1 open Q #2/#3/#5,
спека #2 open Q #1/#5.

## Требования

### Функциональные
- **Бинформат:** target-native raw-структуры с offset-указателями, `mmap`→`reinterpret_cast`
  без парсинга (zero-parse); per-platform bake. `magic`+`fmt_version` в заголовке.
- **Паковка:** именованные **бандлы/группы** — единица стриминга, резидентности, патчинга
  (Steam-депоты) и hot-reload. Между mono-pak и файл-на-ассет.
- **I/O (гибрид):** `mmap` для резидентного (индекс, метаданные, мелкие/hot ассеты — zero-copy)
  + **явный async-I/O** (io_uring/DirectStorage-стиль) для крупных стримовых payload'ов. I/O
  никогда не блокирует sim/render-поток.
- **Компрессия — кодек по классу ассета:** текстуры → KTX2/basis supercompression → transcode
  в BC7/ASTC (сжато в VRAM); bulk (меши/сцены) → zstd-блок → распаковка в арену; audio →
  opus/vorbis-контейнер (уже сжат, не zstd); hot/мелкое → raw (mmap zero-copy). SDF-атлас
  текста (спека #2) — отдельный asset-класс.
- **Идентичность:** стабильный **GUID** (ссылки из кода/сцен, переживает rename/move) +
  **content-hash** (ключ incremental/remote-кеша + дедуп).
- **Детерминизм бейка:** байт-в-байт репродьюсибельный (тот же source+settings → тот же hash).
  Даёт golden-hash T4 и remote build-cache. Пиннутые версии кодеков, без таймштампов/путей.
- **Шейдер-кеш (закрывает спеку #2):** транслятор **Tint** (шипается с Dawn, C++, без Rust)
  за интерфейсом `ShaderCompiler` (naga/native-Vulkan вставляются позже). Кеш **два уровня**:
  переносимый backend-IR (SPIR-V/MSL/DXIL) в ассет-формате (шипается) + драйвер-скомпилированный
  pipeline-blob через Dawn (локальный, ключ `{gpu, driver, ir-hash}`). Варианты (material ×
  lighting-technique × backend × defines) — **on-demand + variant-manifest** (только фактические
  комбо). Единый формат кеша, два режима записи (редактор пишет на лету = тёплый кеш → релиз
  шипает готовый). Смена GPU/драйвера → miss → тихая перекомпиляция.
- **Резидентность:** гибрид — явный `request/release` бандла + refcount, `pin` для always-resident;
  + опциональные auto-prefetch hint'ы. Жёсткий RAM/VRAM-бюджет → LRU-вытеснение незапиненного.
- **Схема ECS (спека #1 open Q #3):** источник истины = authoring-данные (versioned) → сцены/префабы
  **rebake** при смене схемы (бейкнутый blob производный, одноразовый); **сейвы игрока** (нет
  источника) → версионированная миграция. Два механизма по природе данных.
- **Авторинг:** headless **CLI-бейкер** (`assetc`) — ядро (запускается в CI, воспроизводимо);
  IDE (спека #7) оборачивает watch-режимом (save → инкрем. бейк → hot-reload). Один код бейка.
- **Ссылки из кода:** типизированные handle через **codegen-хедер** (`AssetId<Texture> Hero_Idle`);
  опечатка = ошибка компиляции, автодополнение. Рантайм резолвит GUID→bundle→данные.
- **Bake-кеш:** инкрементальный локальный (content-hash CAS) + **remote/shared CAS сразу**
  (владелец выбрал осознанно; инфра-scope помечен в рисках).

### Нефункциональные (столп №1)
- **RAM:** load/submit-путь без скрытых heap-аллокаций в кадре; транзитные буферы через арены/пулы
  (инвариант #5 спеки #1). mmap-резидент — zero-copy.
- **Диск:** кодек по классу; байт-детерминизм → дедуп через content-hash.
- **CPU:** zero-parse загрузка; декомпрессия на IO/worker-пуле, вне sim-потока.
- **Детерминизм (инвариант #2/#3 спеки #1):** **готовность ассета = детерминированный gate** —
  sim читает ready-set только в sync-точке тика; тайминг async-I/O не протекает в sim-hash.
  Нет GPU→CPU→sim readback (стриминг/декомпрессия не кормят симуляцию).
- **Безопасность:** свои pak'и — trusted (checksum от bit-rot); внешние/мод-pak'и (плагины
  спеки #6) — validated-режим (bounds-check всех offset'ов при загрузке, reject не crash).

## Архитектура

```
authoring (source: png/aseprite/ogg/ttf/wgsl, versioned) = ИСТИНА
        │  assetc (CLI, headless, детерм. bake)
        ▼
content-hash CAS (local + remote) ──► bundle (.bundle: target-native, magic+fmt_ver)
        │                                   ├ index/meta        → mmap (zero-copy)
        │                                   ├ small/hot         → mmap (zero-copy)
        │                                   ├ texture           → KTX2/basis (→BC7/ASTC VRAM)
        │                                   ├ bulk (mesh/scene) → zstd block (→ arena)
        │                                   ├ audio             → opus/vorbis
        │                                   └ shader            → backend-IR (Tint)
        ▼
Runtime AssetManager
  request(bundle)→refcount / pin / budget-evict(LRU) / auto-prefetch
        │  IO/worker pool: async read + decompress  (НЕ sim-поток)
        ▼  ready-set (детерм. gate @tick-start)
  ┌── sim (spec #1): читает ready-set детерминированно ──┐
  └── render (spec #2): mmap tex + shader-IR → Dawn ─────┘
                                    │ ShaderCompiler(Tint) → IR
                                    │ Dawn pipeline-blob (local, key gpu+driver+ir-hash)
        HAL platform-io: open / map / read_async / close + caps{mmap?, directstorage?}
          desktop native (сейчас) → console/mobile backend (позже, точка расширения)
```

- **HAL/IO (спека #1 open Q #2):** асс­ет-I/O — за **platform-частью HAL** (отдельно от RHI),
  desktop нативно, консольный I/O-API вставляется позже.
- **Фазирование (KISS, как спека #2):** проектируем под всё (bundle-границы, HAL-IO, tier-кодеки,
  2 уровня шейдер-кеша), реализуем **desktop-first вертикалью**; console/mobile backend'ы и
  native-Vulkan шейдер-путь — в готовые точки расширения позже.

## API / Интерфейсы

- **AssetId<T>:** типизированный handle (codegen из GUID); `assets.get<T>(id) → Handle<T>`.
- **Bundle:** `request(name)→ticket` / `release(ticket)` (refcount), `pin(name)`, статус residency.
- **ShaderCompiler:** `compile(wgsl, target, defines) → backend-IR`; реализация Tint, интерфейс
  под naga/native-Vulkan.
- **assetc (CLI):** `bake <src-dir> <out> [--platform] [--incremental] [--cache <remote>]`.
- **HAL platform-io:** `open/map/read_async/close` + capability-флаги.

## UI/UX

Здесь только механизм (CLI-бейк, watch-хук для hot-reload). Node-редактор материалов, менеджер
ассетов в IDE, авторинг-UX — спека #7. Разработчик игры: `save → авто-перебейк → hot-reload`,
типизированные ссылки на ассеты с автодополнением, «завод по сборке» (столп №11).

## Edge Cases

- Чужая `fmt_version` → чёткая ошибка «rebake ассеты» (fail-hard, ассеты производны/дёшевы).
- Порченный/враждебный mod-pak → validated-режим: bounds-check offset'ов, reject не crash.
- Отсутствующий/битый ассет в рантайме → **placeholder** (magenta-tex/тишина), никогда не crash;
  редактор — громкий (баннер+лог), shipped — тихий лог (игра игрока живёт).
- Смена GPU/драйвера у игрока → pipeline-blob miss → тихая перекомпиляция из IR.
- Схема компонента изменилась → сцены rebake из authoring; сейв игрока → migrate(schema_ver).
- Async-стрим не успел к тику → детерминированная реакция (wait/placeholder по правилу),
  тайминг диска не влияет на sim-hash.
- Бюджет RAM/VRAM превышен → LRU-вытеснение незапиненного; pin защищает критичное (UI/шрифты).
- mmap недоступен на платформе (caps=false) → fallback на async-read через HAL.

## Риски и митигации

| Риск | Вероятность | Митигация |
|------|-------------|-----------|
| **Стриминг ломает детерминизм** (async-тайминг → sim-hash) | Высокая | Готовность = детерм. gate; стрим/декомпрессия вне sim-потока; тест «медленный I/O → тот же sim-hash» (гейт) |
| **mmap page-fault столл в кадре** (спека #1 наивно закладывала «всё mmap») | Высокая | Гибрид: mmap только резидент/мелкое, крупное — явный async-I/O |
| **Взрыв вариантов шейдера** (material×technique×backend×defines) | Высокая | On-demand + variant-manifest (только фактические комбо), не полный крест |
| **Remote-cache инфра-scope** (владелец выбрал «сразу», трётся о фазирование) | Средняя | Content-hash CAS спроектирован remote-ready; транспорт/хранилище/auth — уточнить (open Q #1); можно стартовать с local, включить remote инкрементально |
| **Target-native offset-структуры = OOB attack surface** (моды/bit-rot) | Средняя | trusted/validated два режима; validated bounds-check'ит offset'ы |
| **Байт-детерминизм бейка недостижим** (недетерм. кодеки/потоки) | Средняя | Пиннутые версии кодеков, детерм. порядок, без таймштампов; negative-тест ловит регресс |
| **Шов asset→render не валидирован** → «insert later» = rewrite | Средняя | Асс­ет-PoC кормит реальный бейкнутый ассет в render-PoC (end-to-end гейт) |
| **Per-platform bake ×N таргетов** (поддержка) | Средняя | Desktop-first (все little-endian), endianness-риск низкий; console/mobile позже |
| **fmt_version fail-hard бьёт по shipped pak без source** | Низкая | Ассеты производны (rebake дёшево); backward-compat-ридеры — только если появится need |

## Тестовая стратегия

- **Уровень: T4.** DoD = ADR + walking-skeleton asset-вертикаль + воспроизводимые регресс-гейты.
- **PoC (полная вертикаль):** source (png+wgsl) → `assetc` детерм. bake → bundle → mmap-резидент
  zero-copy + async-стрим-путь → текстура/шейдер-IR в GPU → **hot-reload roundtrip ассета**.
  Асс­ет-PoC **кормит render-PoC** (`poc/render/` потребляет реальный бейкнутый ассет вместо
  hardcoded) — доказывает шов asset→render end-to-end и валидирует шейдер-кеш спеки #2.
- **Гейты (красный = провал):**
  1. **Байт-golden-hash бейка:** `bake(src,cfg)` идентичен между прогонами/машинами (как sim
     golden-hash спеки #1 — тут байтовый корректен, бейк CPU-детерминирован).
  2. **Zero-copy / no per-frame heap:** load/submit-путь без скрытых heap-аллокаций в кадре;
     ASan/UBSan-чисто.
  3. **Нет sim-readback:** замедленный I/O → тот же sim-hash (тайминг не протекает).
- **Информационно (лог + baseline, не гейт):** load-time, пик RAM/VRAM, cache hit-rate,
  stream-латентность, throughput декомпрессии.
- Offscreen/headless путь общий с render-PoC (CI без реального GPU, спека #1/#2 хвост).

## Результат PoC (validated 2026-07-19)

Walking-skeleton asset-вертикаль реализована в `poc/asset/` — все T4-гейты зелёные (локально,
pinned arm64-macOS; tools-free гейты — в CI на всех POSIX-ОС).

- **Гейт #1 (байт-golden bake):** `assetc` детерм. bake байт-идентичен run-to-run. Реальный
  бейк (Tint SPIR-V + basisu KTX2/UASTC + zstd) → `0xa4de7267328f9444`; synthetic (writer+zstd,
  **zstd пиннут v1.5.6** → cross-machine байт-golden в CI) → `0xf2255dc74fbdb6bc`. Оба кодека
  детерминированы: Tint `--ep` per-stage (spirv-val-валиден), basisu `-no_multithreading
  -ktx2_no_zstandard`. Cross-arch байтность texture-части (float в basisu-энкодере) — follow-up.
- **Гейт #2 (zero-copy / no per-frame heap):** shader SPIR-V — zero-copy указатель в mmap-регион
  (content-hash сходится), bulk zstd → декомпрессия в CPU-арену, submit-петля 0 арен-роста за
  120 кадров. **ASan/UBSan чисто.** Validated-режим: порченный offset → reject не crash.
- **Гейт #3 (детерминизм при slow I/O):** sim (fix32) параллельно со стримингом; sim-hash
  идентичен при fast/slow(4ms) I/O. **TSan чисто** (worker↔sync_point). Тайминг не протекает.
- **Шов asset→render:** бейкнутый Tint SPIR-V → `WGPUShaderModule` (SPIRV-descriptor, zero-copy)
  → naga → Metal pipeline; бейкнутый KTX2 → BC7 транскод → GPU-текстура; textured-quad оффскрин
  визуально = source-спрайт. Валидирует SPIR-V ingestion (спека #1 open Q #5) + шейдер-кеш (спека #2).
- **Hot-reload roundtrip:** `AssetManager::reload()` свапит бандл, guid стабилен, refcount выживает,
  содержимое обновляется.

**Тулчейн (пиннут):** Tint (Dawn-subdir, собран standalone — Abseil×clang21 более не стена),
basis_universal `v1_60` (транскодер, `BASISD_SUPPORT_KTX2_ZSTD=0`), zstd `v1.5.6` (FetchContent).

**Упрощения / точки расширения (реализованы под расширение):** L2 driver-pipeline-blob кеш —
нужен Dawn cache-API (wgpu-native не даёт), L1 IR-кеш = шипнутый SPIR-V; variant-manifest —
поле `variant_key` есть, multi-variant бейк — расширение; полный бейк(tint/basisu) в CI —
локальный pinned-T4 (тулчейн не на раннерах, как perceptual-golden render-PoC); Windows/console
I/O-backend, remote-CAS (open Q #1) — позже.

## Открытые вопросы

1. **Remote-cache инфра:** транспорт/хранилище (S3-like / локальный сетевой CAS) + auth — уточнить
   при реализации (выбран «remote сразу», но инфра не спроектирована в деталях).
2. Точные поля заголовка бандла и упаковка каналов текстур (сводится с G-buffer open Q #1 спеки #2).
3. Гранулярность бандла vs патч-эффективность Steam-депотов (delta-патчи — сейчас design-for).
4. Формат authoring-данных сцен/префабов (human-readable/versioned) и его миграция — стык с ECS
   (может уйти в отдельную спеку сериализации/сцен).
5. Аудио-стриминг музыки vs резидентные сэмплы — детально в аудио-спеке #3 (здесь только asset-класс).
6. Text-shaping (HarfBuzz) и SDF-атлас генерация — стык со спекой #2 open Q #5 / IDE-спекой #7.
7. Console/mobile HAL-IO backend и native-Vulkan шейдер-путь (валидируются только 2-м backend'ом).
