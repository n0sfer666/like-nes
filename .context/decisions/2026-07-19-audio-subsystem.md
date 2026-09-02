# ADR 0004: Архитектура аудио-подсистемы (микшер, шины, 2D-спатиализация, стриминг, детерминизм)

- **Дата:** 2026-07-19
- **Статус:** **Accepted** (2026-07-19) — validation gate закрыт: walking-skeleton audio-вертикаль
  PoC (T4) зелёная (все 4 условия финализации выполнены, см. ниже + «Результат PoC» в спеке #3).
- **Контекст:** [спека #3](../specs/2026-07-19-audio-subsystem.md); наследует [ADR 0001](2026-07-18-language-and-core.md)
  (детерминизм, fix32), [ADR 0003](2026-07-19-asset-pipeline.md) (аудио = кодек-класс, residency, ready-set).

## Проблема

Аудио — output-домен, но с жёстким real-time-контрактом (glitch-free callback) И обязан не ломать
бит-в-бит детерминизм симуляции спеки #1. Наивное «микшировать в тике сим» связало бы sim-hash с
таймингом звукового устройства (джиттер, размер буфера) и внесло бы блокирующий I/O/декод в горячий
путь. Нужна архитектура, которая: держит инвариант «выход не кормит сим», даёт RT-safe callback,
переиспользует ассет-пайплайн #5 (стрим/резидент) и оставляет опцию детерминированного аудио для
реплея/netplay ([спека #22](../specs/2026-09-02-deterministic-net.md)).

## Решения

| Аспект | Выбор |
|---|---|
| Модель потоков | **3 домена**: sim (эмит команд @sync_point) / audio-callback (RT-safe микс) / decode-worker (декод вне callback и sim) |
| Планирование событий | команды таймштампятся в **sample-time** (tick N → sample N·spt), НЕ wall-clock → тайминг callback не влияет на sim и на реплей |
| Детерминизм (гибрид) | event-stream детерминирован всегда; **Mixer-seam**: `FloatMixer` (прод, best-effort) + `Fix32Mixer` (Q16.16 → байт-golden, реплей/netplay/тест) |
| Связь sim↔audio | **lock-free SPSC** (sim→mixer команды, worker→mixer PCM); нет audio→sim readback (инвариант #3) |
| Бэкенд/устройство | **miniaudio** за HAL `AudioDevice` (WASAPI/CoreAudio/ALSA/Pulse + mobile); **null/offline device** для CI/golden; console — позже |
| Голоса | фикс. **пул** (без per-callback heap); **voice stealing** по приоритету (детерм.: min-priority→oldest) + fade |
| Шины (Базовый+) | фикс. `Music/SFX/Ambience/UI` + `Master`, per-bus gain; **ducking** = sidechain-lite (music пригашается duck-огибающей) |
| 2D-спатиализация | per-voice поз. (fix32) → **constant-power пан** + distance-аттенюация; считается в момент команды (детерм.) |
| Стрим vs резидент | музыка → **Stream** (chunked-декод впереди playhead, огранич. ring); короткие SFX → **резидент** (полный декод) — закрывает спеку #5 open Q #5 |
| Кодек/ассет | `AssetType::Audio`, vorbis/opus-контейнер (спека #5); декод `AudioDecoder` (stb_vorbis) на worker |
| Устойчивость | underrun → тишина (не блок); битый/нет ассета → **тишина-placeholder** (editor громко / shipped тихо) |
| Ресемпл | детерм. путь на фикс. rate; resample на выходном каскаде (вне golden) — точка расширения |

## Ключевой tradeoff — детерминизм при real-time аудио

Звуковое устройство недетерминировано по таймингу (джиттер callback, размер буфера, sample-rate
дрейф), а сим спеки #1 обязана быть бит-в-бит воспроизводима. **Решение:** сим общается с аудио
**только через команды в sample-time**, эмитируемые в sync-точке тика; какой sample получит событие —
чистая функция от номера тика (`N·samples_per_tick`), НЕ от того, когда физически позовётся callback.
Микс и декод физически живут вне sim-потока (callback + worker). Поэтому: (а) тайминг устройства не
может возмутить sim-hash (инвариант #2), (б) нет audio→CPU→sim readback (инвариант #3), (в) при
одинаковых командах + одинаковом PCM `Fix32Mixer` даёт бит-в-бит одинаковый выход → аудио
воспроизводимо для реплея/netplay и покрывается golden-хешем. Прод по умолчанию — `FloatMixer`
(быстрее, SIMD), не хешируется; детерм.-режим включается для тестов/реплея.

> ⚠️ **Честно зафиксировано:** cross-arch байт-идентичность real-vorbis-декода (float в декодере) не
> гарантирована — как texture-часть спеки #5. **Firm-гейт** = `Fix32Mixer` над пиннутым/raw PCM
> (целочисл., cross-machine). Реальный vorbis-декод→микс — run-to-run локально. Богатый DSP
> (реверб/фильтры/слоистая музыка), детерм. resampler, console/mobile-бэкенд — точки расширения.

## Условия финализации (validation gate) — ✅ ЗАКРЫТЫ 2026-07-19

Proposed → Accepted, когда walking-skeleton audio-вертикаль (T4) закрывает главные риски (реализация
`poc/audio/`, детали — «Результат PoC» в спеке #3):
1. **Байт-golden хеш микса:** `Fix32Mixer` над детерм. PCM байт-идентичен run-to-run + cross-machine
   (целочисл.). ✅ `0x2cf5b5597afa3241`; сверх того — **block-size-независим** (block=800≡256 →
   sample-accurate, тайминг устройства не влияет). ASan/UBSan-чисто.
2. **Нет sim-readback:** замедленный/джиттер audio-callback → тот же sim-hash; TSan-чисто. ✅
   sim-hash `0x9639afe71ee82e50` fast≡slow; one-way SPSC; TSan-чисто.
3. **RT-safe callback:** нет локов/heap/I/O в callback (декод вне потока → ring); ASan/UBSan-чисто. ✅
   0 heap-аллокаций в mix() (incl voice-stealing), lock-free SPSC.
4. **Шов asset→audio:** бейкнутый аудио-ассет через ассет-пайплайн #5 → decode → mixer → выход
   (аудио-аналог шва asset→render; закрывает спеку #5 open Q #5). ✅ real stb_vorbis-декод,
   стрим+резидент, offline WAV rms=4433 + miniaudio `--play`.

## Последствия

- Закрывает open Q #5 спеки #5 (стрим музыки vs резидентные сэмплы) эмпирикой.
- Даёт аудио-домену тот же детерм.-контракт, что рендер: output-only, event-driven, вне sim; готовит
  почву для netplay/rollback ([спека #22](../specs/2026-09-02-deterministic-net.md)) через опц. fix32-детерм.-mix.
- Богатый DSP, console/mobile `AudioDevice`-бэкенд, детерм. resampler, вертикальная слоистая музыка,
  node-редактор миксов (IDE, спека #7) — отдельные задачи/спеки позже (точки расширения готовы).
- HAL-граница аудио-устройства — ещё один platform-бэкенд наряду с RHI (спека #2) и asset-IO (спека #5).
</content>
