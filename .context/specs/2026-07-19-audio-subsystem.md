# Спецификация #3: Аудио-подсистема (микшер, шины, 2D-спатиализация, стриминг, детерминизм)

- **Дата:** 2026-07-19
- **Статус:** **Validated** — walking-skeleton audio-вертикаль PoC закрыта (T4, все гейты зелёные,
  см. «Результат PoC»). ADR [0004](../decisions/2026-07-19-audio-subsystem.md) → Accepted.
- **Наследует:** [спека #1](2026-07-18-language-and-core.md) (детерминизм: fixed timestep, sim-hash,
  инвариант «выход не кормит сим»; fix32), [спека #5](2026-07-19-asset-pipeline.md) (аудио =
  кодек-класс opus/vorbis, стрим-резидентность, ready-set gate — open Q #5 закрывается здесь).

## Контекст

Аудио — второй output-домен движка после рендера (спека #2). Бриф: «тут музыка» как контент-класс
(«завод по сборке игры», без встроенного редактора звука). Драйвер архитектуры — тот же, что везде
в проекте: **детерминизм** (столп №1 + инвариант «выход не кормит сим»). Спека #5 отложила сюда
«стриминг музыки vs резидентные сэмплы» (open Q #5). Цель уровня — Hollow Knight (шины, 2D-позиционал,
ducking), не FMOD/Wwise-полнота.

## Требования

### Функциональные
- **Модель потоков (3 домена):** (1) **sim-поток** — детерминированно эмитит `AudioCommand`
  (play/stop/set-param/listener) в sync-точке тика в **lock-free SPSC-очередь**; команды таймштампятся
  в **sample-time** (tick N → sample `N*samples_per_tick`), НЕ в wall-clock. (2) **audio-callback**
  (RT-поток miniaudio) — тянет команды, микширует активные голоса из PCM-ring'ов в буфер устройства;
  **RT-safe** (без локов/heap/I/O/syscall). (3) **decode-worker** — тянет сжатый аудио из AssetManager
  (Stream), декодит в per-voice ring **впереди playhead** (как asset-worker, вне sim/callback).
- **Голоса:** фиксированный пул (без per-callback heap). Voice = {asset, playhead(sample), gain, pan,
  bus, loop, priority, state}. Переполнение → **voice stealing** по приоритету (детерм.: min-priority,
  затем oldest) с быстрым fade.
- **Шины (Базовый+):** фикс. набор `Music / SFX / Ambience / UI` (submix) + `Master`; per-bus gain.
  **Ducking** — music-шина пригашается duck-огибающей, пока активен голос с флагом ducking (диалог/
  громкий SFX). Sidechain-lite, без полного компрессора.
- **2D-спатиализация:** per-voice позиция (fix32 x,y) относительно listener → **constant-power пан** +
  **distance-аттенюация** (linear/inverse rolloff). Считается в момент команды (позицию даёт сим) →
  детерминировано.
- **Микшер (seam):** два бэкенда за интерфейсом `Mixer` — `FloatMixer` (прод, best-effort, SIMD-friendly)
  и `Fix32Mixer` (детерм. Q16.16-аккумуляция → байт-golden). Одинаковый маршрут голосов/шин, разный тип.
- **Стриминг vs резидент (закрывает спеку #5 open Q #5):** музыка → **Stream** (chunked-декод впереди
  playhead в ограниченный ring, не полный декод) ; короткие SFX → **резидент** (полный декод в пул/арену
  при загрузке). Выбор по residency-флагу ассета (спека #5).
- **Ассет-интеграция:** `AssetType::Audio`, кодек = vorbis/opus-контейнер (спека #5, «уже сжат, не
  zstd»). Бейк хранит сжатый payload + метаданные (sample_rate, channels, frames, loop-points). Декод —
  `AudioDecoder` (stb_vorbis) на worker.
- **Устройство (HAL):** бэкенд = **miniaudio** (WASAPI/CoreAudio/ALSA/Pulse + Android/iOS) за интерфейсом
  `AudioDevice`; console/mobile-бэкенды — точки расширения. **Null/offline device** (рендер блоками фикс.
  размера в буфер) для CI и golden-теста.

### Нефункциональные (столп №1)
- **RAM:** без per-callback heap; голоса/ring'и/командная очередь — преаллоцированные пулы; декод в
  арены. Музыка — ограниченный ring, не полный декод.
- **CPU:** декод вне audio-потока; в callback только микс (RT-bounded). Float-путь SIMD-friendly.
- **Детерминизм (инвариант #2/#3 спеки #1):** командный поток детерминирован + **sample-accurate
  scheduling** (событие тика N → фикс. sample, независимо от тайминга callback). Аудио **никогда** не
  кормит сим (нет audio→CPU→sim readback). Замедленный/джиттер callback → тот же sim-hash. Опц.
  fix32-детерм.-mix для реплея/netplay + golden.
- **Безопасность/устойчивость:** RT-safe callback (lock-free SPSC sim→mixer и worker→mixer); underrun →
  тишина для голоса (placeholder), не блок; битый/отсутствующий аудио-ассет → **тишина-placeholder**
  (editor громко / shipped тихо — паттерн спеки #5), никогда не crash.

## Архитектура

```
authoring (source: ogg/wav, versioned) = ИСТИНА
        │  assetc (спека #5): audio codec-класс (vorbis/opus контейнер) + метаданные
        ▼
bundle (.bundle): AssetType::Audio  ── Stream (музыка) / резидент (SFX)
        ▼
AssetManager (спека #5): request/ready-set gate @tick — тайминг I/O не течёт в sim
        │
  ┌─ sim-поток (спека #1) ─────────────────────────────────────────┐
  │  update(tick): эмит AudioCommand в sample-time @sync_point       │
  └───────────────┬──────────────────────────────────────────────── ┘
                  │ SPSC (lock-free, sim→mixer)
                  ▼
  decode-worker ──► per-voice PCM ring  ──► audio-callback (RT-safe)
   (stb_vorbis,        (SPSC worker→mixer)     │ drain commands @sample-boundary
    вне callback)                              │ Mixer: voices → pan/atten → buses → duck → master
                                               │   FloatMixer (прод) | Fix32Mixer (детерм./golden)
                                               ▼
                          AudioDevice (HAL): miniaudio (desktop/mobile) | null/offline (CI)
                            caps{sample_rate, channels}; console-бэкенд — позже
```

- **HAL (спека #1 open Q #2):** аудио-устройство за platform-частью HAL (отдельно от RHI/asset-IO),
  desktop = miniaudio, console/mobile — точки расширения.
- **Фазирование (KISS, как спеки #2/#5):** проектируем под всё (3 домена, шины, 2 микшера, HAL,
  стрим/резидент), реализуем desktop-first вертикалью; богатый DSP (реверб/фильтры/слоистая музыка),
  console/mobile-бэкенды, детерм. resampler — точки расширения позже.

## API / Интерфейсы

- **AudioEngine:** `play(AssetId<Audio>, PlayParams) → VoiceHandle`, `stop(VoiceHandle)`,
  `set_bus_gain(Bus, fix32)`, `set_listener(pos)`, `update(tick)` (дренаж sim-side → команды @sync_point).
- **PlayParams:** `{ Bus bus; fix32 gain; bool loop; uint8 priority; vec2 pos; bool ducking; }`.
- **Mixer (seam):** `mix(block, out)` — реализации `FloatMixer` / `Fix32Mixer`.
- **AudioDevice (HAL):** `start(callback)` / `stop()` / `caps()` — miniaudio + null/offline.
- **AudioCommand:** POD в SPSC (`sample_time`, тип, параметры голоса) — детерм. по значению.
- **assetc:** `AssetType::Audio` bake (vorbis-контейнер + метаданные + residency-флаг).

## UI/UX

Здесь только механизм (микшер, шины, API `play`). Node-редактор миксов, кривые аттенюации в IDE,
авторинг-UX звука — спека #7. Разработчик игры: типизированный `play(Sfx_Jump)`, шины с громкостью,
`save → перебейк → hot-reload` аудио-ассета (наследуется от спеки #5).

## Edge Cases

- Пул голосов исчерпан → voice stealing (min-priority, затем oldest; быстрый fade — без клика).
- Ring underrun (декод не успел) → тишина для голоса, resume при заполнении; лог; callback не блок.
- Sample-rate устройства ≠ движка → resample на выходном каскаде (вне детерм. mix); PoC — ассеты 48k.
- Устройство пропало/сменилось (наушники) → re-init miniaudio; sample-time голосов сохранён.
- Аудио-ассет битый/отсутствует → тишина-placeholder; editor громко, shipped тихо (спека #5).
- Loop-points → sample-accurate wrap в декоде/ring.
- Музыкальный переход → кроссфейд = два music-голоса с gain-огибающими (Базовый+); вертикальная
  слоистая музыка/stingers — open Q (богатый scope позже).

## Риски и митигации

| Риск | Вероятность | Митигация |
|------|-------------|-----------|
| **Джиттер callback течёт в sim** (детерм.) | Высокая | sample-time scheduling; команды @фикс. sample-границе; event→sample независим от тайминга callback; гейт «медленный callback → тот же sim-hash» |
| **RT-safety нарушена** (лок/alloc/priority-inversion → glitch) | Высокая | lock-free SPSC, преаллок. пулы, нет syscall в callback; гейт no-alloc + ASan |
| **Float-микс недетерминирован** (cross-platform FP) ломает реплей | Высокая | гибрид: fix32 детерм.-mix для реплея/golden; float только прод (не хешируется) |
| **Vorbis-декод cross-arch float-разброс** | Средняя | golden над пиннутым/raw PCM (как basisu follow-up спеки #5); реальный декод = run-to-run локально |
| **Resample недетерминирован** | Средняя | детерм. путь на фикс. rate; resampler — документированная точка расширения, вне golden |
| **Voice steal — слышимый артефакт** | Низкая | приоритет + быстрый fade на steal |
| **miniaudio-девайс недоступен в CI** | Низкая/ожид. | null/offline device рендерит в буфер; реальный девайс — локально (прецедент perceptual-golden спеки #2) |

## Тестовая стратегия

- **Уровень: T4.** DoD = ADR + walking-skeleton audio-вертикаль + воспроизводимые регресс-гейты.
- **PoC (полная вертикаль):** source (ogg) → `assetc` audio bake → bundle → AssetManager ready-set →
  decode-worker → ring → mixer (шины+пан+аттенюация+ducking) → miniaudio device. Гибрид: прод
  FloatMixer + опц. Fix32Mixer (детерм.). Аудио-PoC **кормится реальным бейкнутым ассетом** через
  ассет-пайплайн #5 (шов asset→audio, как asset→render).
- **Гейты (красный = провал):**
  1. **Байт-golden хеш микса:** Fix32Mixer над детерм. PCM-входом байт-идентичен run-to-run + cross-
     machine (целочисл. математика). Реальный vorbis-декод→микс — run-to-run локально.
  2. **Нет sim-readback:** замедленный/джиттер audio-callback → тот же sim-hash (event-stream детерм.);
     TSan-чисто (SPSC sim→mixer).
  3. **RT-safe callback:** нет локов/heap/I/O в callback (декод вне потока → ring); ASan/UBSan-чисто.
- **Информационно (лог, не гейт):** CPU микса, латентность, ring-underruns, число голосов, throughput
  декода.
- Deviceless-гейты (mix-golden / sim-hash det / RT-safety) — в CI на всех POSIX; реальный девайс +
  vorbis-декод = локальный pinned-T4 (как perceptual-golden рендера).

## Результат PoC (validated 2026-07-19)

Walking-skeleton audio-вертикаль реализована в `poc/audio/` — все T4-гейты зелёные (локально,
pinned arm64-macOS; deviceless-гейты — в CI на всех POSIX-ОС). Переиспользованы `fix32`
(`poc/src/fixed.hpp`), ассет-пайплайн #5 (AssetManager Mmap zero-copy, ready-set gate),
FNV-1a golden, CMake FetchContent, CI-структура.

- **Гейт #1 (байт-golden хеш микса):** `Fix32Mixer` над детерм. целочисленным PCM → mix-буфер
  `0x2cf5b5597afa3241`, байт-идентичен run-to-run. **Сильнее:** тот же хеш при block=800 И block=256
  → микс **sample-accurate и block-size-независим** (тайминг/размер буфера устройства не влияет на
  выход — настоящая гарантия реплей/netplay-детерминизма). Только целочисл. математика (fix32-sat +
  integer sqrt) → cross-machine (x86≡ARM). ASan/UBSan-чисто.
- **Гейт #2 (нет sim-readback):** sim (fix32, 3000 тиков) параллельно с audio-callback; sim-hash
  `0x9639afe71ee82e50` идентичен при fast/slow(1.5ms-jitter) callback. Связь строго one-way SPSC
  (sim=продюсер, mixer=консюмер), sim не читает микшер. **TSan-чисто.** Тайминг не протекает.
- **Гейт #3 (RT-safe callback):** глобальный operator-new-счётчик подтверждает **0 heap-аллокаций**
  в `mix()`-пути (включая voice-stealing при 40 голосах > пула 32); SPSC lock-free (atomic-индексы).
  **ASan/UBSan-чисто.**
- **Шов asset→audio:** бейкнутый Ogg Vorbis (committed source) → mmap zero-copy через ассет-пайплайн
  #5 → **реальный stb_vorbis-декод** (sfx 14400 / music 96000 фреймов @48k) → mixer (SFX резидент +
  музыка через стрим-ring) → offline WAV (rms=4433, слышимый) + `--play` на реальном устройстве
  miniaudio. Закрывает спеку #5 open Q #5 (стрим музыки vs резидентные сэмплы).
- **Bake детерминизм:** audio-бандл (Mmap/Vorbis) `0x9b3ce443c3096e31` байт-идентичен run-to-run
  (копирует committed .ogg — энкодер не нужен в CI).

**Тулчейн:** stb_vorbis (present в fetched stb, декод), miniaudio `0.11.21` (устройство, локально;
в CI `-DAUDIO_MINIAUDIO=OFF`), oggenc (vorbis-tools, только авторинг committed source .ogg).

**Упрощения / точки расширения (реализованы под расширение):** богатый DSP (реверб/фильтры,
sidechain-компрессор, слоистая музыка) — за Базовый+; детерм. resampler (ассеты фикс. 48k в детерм.
пути) — точка расширения; bake-time аудио-мета (sample_rate/channels/frames в слотах AssetEntry) —
сейчас рантайм выводит из ogg (нужен decode в assetc); console/mobile `AudioDevice`-бэкенд + низко-
латентный путь — позже; cross-arch байт-идентичность real-vorbis-декода (float) — follow-up, firm-
гейт = fix32-mix над пиннутым PCM.

## Открытые вопросы

1. **Богатый DSP:** реверб/фильтры (occlusion), sidechain-компрессор, вертикальная слоистая музыка
   (transitions/stingers), snapshot-миксы — отдельная спека, если понадобится (за Базовый+).
2. **Детерм. resampler:** ассеты не-48k в детерм. пути (сейчас фикс. rate); линейный/полифазный
   детерм. ресемпл — точка расширения.
3. **Cross-arch байт-идентичность vorbis-декода** (float в декодере) — как texture-часть спеки #5;
   firm-гейт = fix32-mix над пиннутым PCM.
4. **Console/mobile AudioDevice-бэкенд** (валидируется только 2-м бэкендом) + низколатентный путь
   (WASAPI exclusive / CoreAudio) — позже.
5. **HRTF/3D** — вне scope 2D-движка; если появится изометрия с «высотой» — переоценить.
6. **Интеграция с netplay/rollback (спека #11):** fix32-детерм.-mix даёт аудио-паритет; политика
   «перемотка звука при rollback» (заглушить/не трогать) — уточнить в спеке нетворкинга.
</content>
