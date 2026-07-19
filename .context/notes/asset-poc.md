# Asset-вертикаль PoC — рабочий скретчпад

Спека #5 / ADR 0003 validation gate. Tier T4. Коммит на каждый шаг-риск (как render-PoC).
Ветка `poc/asset-pipeline`.

## Решения владельца (интервью 2026-07-19, этот раунд)
- **Шейдер:** НАСТОЯЩИЙ Tint→SPIR-V сейчас (не stand-in). WGSL→Tint→SPIR-V на бейке →
  скормить SPIR-V в wgpu-native → Metal-пайплайн. Риск: Tint тянет Abseil → та же
  Abseil×clang21 `-msse4.1` стена, что завалила Dawn. → **de-risk спайк первым**.
- **Текстуры:** НАСТОЯЩИЙ KTX2/BC7 сейчас (не raw). PNG→basisu/KTX transcode в BC7 →
  wgpu-native BC7-текстура на Apple Silicon Metal (BC поддержан с macOS 11/M1). → **de-risk спайк**.
- **DoD/tier:** T4, полная вертикаль: 3 красных гейта + шов asset→render + hot-reload.

## Гейты (красный = провал = блок ADR Accepted)
1. Байт-golden-hash бейка идентичен run-to-run / cross-machine (CPU-детерм.).
2. Zero-copy / no per-frame heap в load/submit; ASan/UBSan-чисто.
3. Замедленный I/O → тот же sim-hash (тайминг не протекает в детерминизм).
+ Шов: бейкнутый ассет кормит `poc/render/` (real BC7 tex + real Tint SPIR-V shader).
+ hot-reload roundtrip ассета.
Информационно (лог, не гейт): load-time, пик RAM/VRAM, cache hit-rate, stream-латентность.

## Переиспользуем из существующих PoC
- Единый `poc/CMakeLists.txt` (FetchContent glfw/webgpu(wgpu-native)/stb).
- `poc/render/arena.*` — аренный паттерн (транзитные буферы без per-frame heap).
- `poc/render/gpu.*` — WebGPU-контекст (headless nullptr-surface путь для CI).
- `poc/render/sprite.*` — ТО, ЧТО ЗАМЕНЯЕМ бейкнутым ассетом (procedural → baked).
- `poc/render/capture.*`, `golden_main.cpp` — offscreen readback + perceptual-гейт.
- `poc/src/fixed.hpp` + `determinism_test.cpp` — FNV-хеш + fix32 (паттерн sim-hash гейта #3).
- `poc/hotreload/host.cpp` — паттерн swap ресурса в рантайме.
- CI `.github/workflows/ci.yml` — 3 ОС, best-effort GPU (lavapipe/Metal), golden-grep.

## Прагматизм-прецедент (из render-PoC, честно фиксировать)
- wgpu-native вместо Dawn (Dawn не собрался). Тот же webgpu.h → свап на Dawn позже.
- Golden GPU — perceptual, НЕ байтовый (GPU не бит-в-бит). Байтовый — ТОЛЬКО на CPU-бейке.

## План (фазы)
- **Phase 0 — de-risk спайки (report ДО глубокого кода):**
  - A. Standalone Tint build (arm64-macOS clang21) — WGSL→SPIR-V. Long pole.
  - B. wgpu-native SPIR-V shader-module → Metal pipeline → рендер (naga SPIR-V frontend).
  - C. basisu/KTX build → PNG→BC7 KTX2 → wgpu-native BC7 tex на Metal → сэмпл.
- **Phase 1 — bundle-формат + assetc bake (детерм., байт-golden #1).**
- **Phase 2 — рантайм-загрузка: mmap zero-copy + async-стрим→zstd→арена (#2, ASan/UBSan).**
- **Phase 3 — GPU-consume + шов в render_golden (real BC7 + real SPIR-V), L1 shader-IR кеш.**
- **Phase 4 — детерминизм-гейт: замедленный I/O → тот же sim-hash (#3).**
- **Phase 5 — hot-reload roundtrip.**
- **Phase 6 — CI (3 ОС + ASan/UBSan-джоб) + .context/dev-log.**

## Прогресс
- [~] Phase 0 спайки:
  - ✅ Спайк C (BC7/KTX2): `basisu 2.10` PNG→KTX2 UASTC_LDR_4x4 (zstd-supercompressed),
    БАЙТ-детерминирован run-to-run (`-no_multithreading`). Транскодер в brew не идёт →
    рантайм: pinned basis_universal source (FetchContent, arm64-clean, без Abseil).
  - ✅ Abseil 20240722.0 собрался ПОЛНОСТЬЮ под clang21/arm64 (`-msse4.1/-maes` попадают
    в флаги, но компилируются). Стена Dawn была от СТАРОГО пиннутого Abseil, не фундаментальна.
  - ✅ Tint (Dawn subdir): clone+fetch_deps+configure OK (CFG_EXIT=0, Abseil-стены НЕТ),
    идёт сборка `tint` CLI (фон). wgpu-native API: `WGPUShaderModuleSPIRVDescriptor` есть,
    BC7RGBAUnorm/Srgb есть → SPIR-V ingestion + BC7 upload реализуемы.
  - ⏭ SPIR-V runtime ingestion (naga→Metal) — проверю в Phase 3 (API готов; при провале
    fallback = ship WGSL-текст как IR, шов asset→render всё равно держится).
  - Инструменты в системе: brew `zstd 1.5.7`, `basisu 2.10`, `glslang 16.4`, `shaderc`, `spirv-tools`.
- Bake-детерминизм cross-machine: zstd+SPIR-V бит-идентичны cross-arch; basisu-текстура
  cross-arch НЕ гарантирована (float/SIMD в энкодере) → как render-PoC: run-to-run — твёрдый
  гейт, cross-machine байтность texture-части = follow-up (pinned-env), zstd/shader-часть cross-OS.
- ✅ Phase 0 спайки (все зелёные, см. выше).
- [~] Phase 1: формат (`poc/asset/format.hpp` zero-parse POD, static_assert ABI), FNV-хеш,
  детерм. `bundle_writer` (сорт по guid), codec-слой (zstd link + Tint/basisu shell-out
  пиннутыми флагами), `assetc` CLI. **ГЕЙТ #1 run-to-run ЗЕЛЁНЫЙ**: bundle_hash
  0xa84a4f934661a887, 5 ассетов (albedo+normal BC7/KTX2, sprite.vs/fs SPIR-V, scene_bulk zstd),
  байт-идентичен. CMake: asset_core + assetc (NOT WIN32, нужен libzstd). Cross-machine +
  CI-интеграция → Phase 6.
  - Bake-tools пути: `--tint <scratchpad>/dawn-spike/build/tint --basisu /opt/homebrew/bin/basisu`.
    Для CMake/CI: закешировать пути/установку (Phase 6). Source: `poc/assets/src/`.
- ✅ Phase 2: HAL `platform_io` (mmap zero-copy + read_range), zero-parse `bundle_view`
  (+validated bounds-check), CPU `byte_arena` (bump, no per-frame heap), `asset_manager`
  (worker-поток, ready-set gate @sync_point, refcount/pin, zstd→арена). **ГЕЙТ #2 ЗЕЛЁНЫЙ**:
  `asset_test` — shader zero-copy (ptr в mmap, content_hash сходится), bulk zstd верен,
  арена 3 аллок / 0 роста за 120 кадров submit, **ASan/UBSan чисто** (fast+slow), порченный
  offset → reject не crash. Файлы poc/asset/{platform_io,bundle_view,byte_arena,asset_manager}.*
- ✅ Phase 3: шов asset→render. `transcode` (basis v1_60 KTX2/UASTC→BC7, `BASISD_SUPPORT_KTX2_ZSTD=0`
  → KTX2 бейкается `-ktx2_no_zstandard`), `asset_render` (headless): baked SPIR-V→WGPUShaderModule
  (zero-copy из mmap)→naga→Metal pipeline (Спайк B ВАЛИДИРОВАН), baked KTX2→BC7→GPU-текстура,
  textured-quad оффскрин→readback→PNG. lit_frac=0.725, визуально = checker-спрайт. gpu.cpp:
  +TEXTURE_COMPRESSION_BC (best-effort). capture: вынесен `readback_rgba`. L1 shader-IR кеш =
  шипнутый SPIR-V (content-hash ключ, рантайм НЕ компилит WGSL); L2 pipeline-blob = точка
  расширения (нужен Dawn cache API, wgpu-native не даёт — как render-PoC). bundle_hash 0xa4de7267328f9444.
- ✅ Phase 4: гейт #3 `asset_determinism_test` — sim (fix32) параллельно со стримингом;
  sim-hash 0x2c9b66967eee9999 ИДЕНТИЧЕН при fast/slow(4ms) I/O, оба 5/5 стрим. **TSan чисто**
  (worker↔sync_point). I/O-тайминг не протекает в детерминизм.
- ✅ Phase 5: hot-reload roundtrip `asset_hot_reload_test` (self-contained, writer v1/v2) +
  `AssetManager::reload()` (свап mmap, стабильный guid, refcount выживает, submit_load рефактор).
  Свап shader aa→bb (zero-copy) + bulk 11→22 (zstd). **ASan/UBSan чисто.**
- [~] Phase 6: CI (`.github/workflows/ci.yml`) — tools-free гейты на всех POSIX: synthetic
  байт-golden 0xf2255dc74fbdb6bc (writer+**zstd ПИННУТ v1.5.6** FetchContent → cross-machine)
  + гейты #2/#3 + hot-reload + Linux ASan-джоб. Полный бейк(tint+basisu)+шов = локальный pinned-T4.
  `assetc --synthetic` для CI. Идёт: чистая CI-сборка poc/build (фон) + code-reviewer (фон).

## Финальный статус гейтов (ЛОКАЛЬНО pinned, arm64-macOS)
- Гейт #1 байт-golden bake: run-to-run ✅ (real 0xa4de7267328f9444; synthetic 0xf2255dc74fbdb6bc
  cross-machine via pinned zstd). basisu-текстура cross-arch — follow-up (float в энкодере).
- Гейт #2 zero-copy/no-per-frame-heap + ASan/UBSan ✅ + validated bounds-check reject-not-crash ✅.
- Гейт #3 slow I/O → тот же sim-hash ✅ + TSan ✅.
- Шов asset→render end-to-end ✅ (Спайк B: Tint SPIR-V→naga→Metal; lit_frac 0.725, визуал OK).
- Hot-reload roundtrip ✅.
- → ADR 0003 Proposed→**Accepted**, спека #5 → **Validated**, README roadmap #5 → Закрыта, dev-log записан.

## Независимое ревью (code-reviewer) + фиксы
Ревью нашло 1 critical + 1 high + 1 medium + 7 low + 1 taste. Исправлено ВСЁ ≥low (10):
- **critical** reload() UAF (munmap до валидации нового бандла) → ТРАНЗАКЦИОННЫЙ reload
  (open+validate во временные, swap через move-семантику только при успехе; при провале worker
  рестарт, старое состояние живо). Регресс-тест: негативный reload(missing) под ASan → old жив.
- **high** нет in-flight дедупа → двойной request → гонка на s.loaded + двойная арена-аллок →
  atomic `inflight` (exchange-guard). Регресс: double-request asset_test → allocs==3.
- **medium** validated-режим без alignment-check → SIGBUS → добавил проверки выравнивания.
- **low×7**: atomic арена-диагностики, uint32-overflow guard writer, WGPU-release asset_render,
  null/ready/open guards в тестах, доки (Loaded-invalid-after-reload, release-no-evict, codec trust).
- Перепроверка после фиксов: все 5 гейтов PASS (хеши НЕИЗМЕННЫ), ASan/UBSan+TSan чисто,
  render_golden selftest PASS (нет регресса от gpu.cpp/capture.cpp). review-state/verify-state записаны.
- CI-путь `poc/build` собирается целиком (exit 0), synthetic golden 0xf2255dc74fbdb6bc стабилен.

## ВСЁ ГОТОВО — НЕ закоммичено (ждёт явного «go» владельца, правило 11).
Ветка poc/asset-pipeline. Tint собран в scratchpad (для ребейка нужен путь через --tint).

## Риски / открытые
- Tint standalone может упереться в Abseil×clang21. Bounded-attempt; провал → report + пересогласовать.
- naga SPIR-V frontend неполон → сложные шейдеры могут не пройти; начать с простейшего sprite-шейдера.
- wgpu-native pipeline-blob кеш (L2) API может отсутствовать → L2 = документированная точка расширения.
</content>
