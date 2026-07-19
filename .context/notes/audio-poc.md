# Скретчпад: аудио-вертикаль PoC (спека #3)

Дата старта: 2026-07-19. Tier: **T4**. Раунд: spec+ADR+PoC в один заход (owner).

## Развилки (owner, AskUserQuestion 2026-07-19)
- **Детерминизм — гибрид:** event-stream детерминирован всегда; опц. fixed-point детерм.-mix
  режим (реплей/тесты); прод float best-effort (не хешируется).
- **Бэкенд — miniaudio за HAL** (WASAPI/CoreAudio/ALSA/Pulse + mobile; декод внутри). Null/offline
  device для CI (рендер в буфер → golden).
- **DSP-скоуп — Базовый+:** шины (music/SFX/ambience/UI) + громкость + 2D-пан/аттенюация +
  стрим-музыка vs резидент-SFX + ducking.

## Инвариант (ядро дизайна)
Аудио — output-домен (как рендер): сим детерминированно эмитит команды в sync_point тика в
lock-free SPSC-очередь; команды таймштампятся в **sample-time** (tick N → sample N*spt), НЕ в
wall-clock. Микс живёт в audio-callback (RT-safe), декод — на worker (вне callback, вне sim).
Джиттер callback НЕ меняет какой sample получит событие → тайминг не течёт в sim-hash И реплей
воспроизводим. Нет audio→sim readback (инвариант #3 спеки #1).

## Гейты (красный=провал)
1. Байт-golden хеш микса — fix32 детерм.-режим над детерм. PCM-входом; run-to-run + cross-machine
   (целочисл.). Реальный vorbis-декод→микс = локальный run-to-run (cross-arch float-декод follow-up).
2. Нет sim-readback — замедл./джиттер callback → тот же sim-hash. TSan-чисто (SPSC sim→mixer).
3. RT-safe callback — нет локов/heap/I/O в callback (декод вне потока→ring). ASan/UBSan-чисто.
- Шов asset→audio — бейкнутый ogg через ассет-пайплайн #5 → декод → mixer → выход.

## Переиспользование
- `poc/src/fixed.hpp` fix32 — детерм. mix-аккумуляция.
- `poc/asset/` AssetManager (Stream=музыка/резидент=SFX, ready-set gate, io_delay_us), format.hpp
  (+ AssetType::Audio, Codec для vorbis-контейнера), assetc (+ audio bake), hash.hpp (FNV-1a golden).
- CMake FetchContent (miniaudio + stb_vorbis, single-header как stb — SOURCE_SUBDIR do-not-build).
- CI: deviceless-гейты на всех POSIX; реальный девайс+vorbis = локальный pinned-T4.

## Фазы
1. [ ] docs: spec #3 + ADR 0004 (Proposed) + note
2. [ ] CMake: miniaudio + stb_vorbis; format AssetType::Audio + audio codec; assetc bake audio
3. [ ] audio core: SPSC ring/queue, voice pool, mixer seam (Float/Fix32), buses+pan+atten+duck
4. [ ] sim→audio командный поток (sample-time), decode-worker → ring
5. [ ] miniaudio device HAL + null/offline device; RT-safe callback
6. [ ] гейты: mix-golden / sim-hash det / RT-safety; шов asset→audio
7. [ ] CI wiring
8. [ ] verify T4 (build+гейты+sanitizers) → code-reviewer → фикс ≥low
9. [ ] ADR Accepted / spec Validated / README #3 / stack.md / dev-log / коммиты по фазам

## Прогресс
- Фазы 1–7 сделаны. Все гейты зелёные (локально, arm64-macOS):
  - Гейт #1 mix-golden: `0x0278d3c1ec13f3fb` (run-to-run + ASan/UBSan идентичен).
  - Гейт #2 determinism: sim_hash fast==slow `0x9639afe71ee82e50` (TSan-чисто).
  - Гейт #3 rt-safety: no-alloc callback, peak voices=32 (steal), lock-free (ASan/UBSan-чисто).
  - Шов: sfx 14400 / music 96000 фреймов @48k, rms=4439, offline WAV; ASan/UBSan-чисто.
  - Audio bundle bake детерм.: `0x9b3ce443c3096e31` (байт-идентичен run-to-run).
- Регресс: asset synthetic `0xf2255dc7…` без изменений, asset_test PASS, render_golden PASS.
- CI: аудио-гейты добавлены (POSIX deviceless + Linux ASan/UBSan/TSan), `-DAUDIO_MINIAUDIO=OFF`.
- Фаза 8 (code-reviewer): FAIL → 1 critical + 2 medium + 4 low + 1 taste. **Все 8 исправлены.**
  - critical: start_offset не сбрасывался между блоками → голос терял сэмплы; фикс + гашение после блока.
  - medium: Play без bus-range check → OOB busL[]; duck-рамп по-блочно → block-зависимость.
    **Усилил гейт #1** (block=800≡256) — поймал остаточную block-зависимость duck → duck теперь
    считается ПО СЕМПЛУ (по факту «звучит ли ducking-голос», f>=start_offset). Микс sample-accurate.
  - low: fetch_advance frames==0 OOB; SampleRing init(0) div-0; engine.play post-результат; format-коммент.
  - taste: device мёртвые fn_/user_.
  - Golden-хеш изменился (start_offset+duck+расписание) → `0x2cf5b5597afa3241`, перепиннут в ci.yml.
  - Re-verify: все гейты PASS, ASan/UBSan/TSan чисто, block-независимость PASS, регресса нет.
- Фаза 9: spec Validated + «Результат PoC», ADR 0004 Accepted, README #3, stack.md — готово.
  Осталось: dev-log + (коммиты — по запросу пользователя, правило 11).
- **Новый golden-хеш микса: `0x2cf5b5597afa3241`** (block-независимый). Audio bundle: `0x9b3ce443c3096e31`.
  sim-hash det-гейта: `0x9639afe71ee82e50`.
</content>
</invoke>
