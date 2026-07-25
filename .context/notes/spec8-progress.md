# Spec #8 — прогресс walking-skeleton (build/deploy)

Живой скретчпад. Читать в начале каждой сессии (и после `/compact`). Одна сессия = один гейт = один
коммит (verify-state + review-state перед коммитом, code-reviewer как в #1–#7).

## Канон-решения (S1, зафиксировано owner 2026-07-23)
- **Порядок:** тонкая игра → полный пайплайн на ней → утолщение игры. Payload всегда настоящая игра.
- **Гранулярность:** ~11 мелких сессий (таблица в spec #8).
- **Развилка A (тулчейн):** desktop native per-OS matrix; mobile true-cross (Android NDK / iOS
  `CMAKE_SYSTEM_NAME`+симулятор на macOS-хосте). ⚠️ wgpu НЕТ prebuilt под mobile → **wgpu-native
  собираем из Rust-исходников** (`aarch64-linux-android` + `aarch64-apple-ios`+sim). **mobile RUNNABLE**
  (render+input), iOS первый класс. Уточнение owner 2026-07-23: mobile должен запускаться (не build-only).
- **Развилка B (детерм.):** P0–P3 (флаги + SHA-пин депов + герметичный контейнер), per-triple same-host
  байт-идентичность + cross-machine stretch; cross-OS = non-goal.
- **Развилка C (упаковка):** macOS `.app`(@rpath) / Linux tarball(`$ORIGIN`) / Windows папка+DLL(`/MT`);
  НЕ AppImage/Flatpak. Steam = дизайн+VDF, без заливки.

## Статус сессий
- [x] **S1** Дизайн — spec #8 + ADR 0008 (Proposed) + этот скретчпад. Коммит: _(следующий)_.
- [x] **S2a** Desktop-скелет игры + bring-up. Коммит `c2e8b77`. Модуль `poc/game/` (таргет
  `game_sidescroller`): flecs+fix32 fixed-timestep 60Hz, управляемый корабль (ввод #4), **свой**
  WebGPU instanced sprite-batch (НЕ трогает render_core #2 — тот одно-спрайтовый), процедурный
  atlas (силуэт корабля + звезда), parallax-starfield wrap-scroll. Два режима: оконный live
  (`game/live.cpp`) + headless `--demo` (`game/demo.cpp` — скриптует RawEvent через реальный HAL →
  offscreen PNG). **T4 macOS live: pass** (build-all clean / demo рендерит управление во все стороны /
  окно открывается+present+чистый выход+GameController.framework). code-review: 6 находок
  (1 high/2 med/2 low fixed, 1 taste skip). Паритет 3 ОС → CI native-matrix (авто в `all` под IDE_POC).
- [~] **S2b** Mobile bring-up. **iOS-симулятор + Android-эмулятор — DONE** (`727be61` iOS, `122cf14` Android).
  - **Research-гейт (доказан):** wgpu-native **v0.19.4.1** (= версия desktop-prebuilt из
    `wgpu-native-git-tag.txt`) собран ИЗ Rust-исходников под `aarch64-apple-ios-sim` (otool:
    platform 7 = iOS Simulator, minos 14.0, arm64). Заголовки `webgpu.h`/`wgpu.h` **байт-идентичны**
    desktop → C++ игры компилируется 1:1 (главный риск снят). Rust 1.94 собрал крейт 2024 чисто (~1 мин).
  - **iOS app-shell (`poc/ios/`, standalone CMake):** CAMetalLayer→wgpu surface, CADisplayLink 60Hz,
    тач→виртуальный стик→PadAxis через реальный HAL #4, аспект-корректный вьюпорт. Игра (game/* +
    input_core + render/gpu + flecs) запущена на **iPhone 16 симуляторе**: рендерит корабль+starfield,
    `--demo` двигает корабль. **T4: запуск+управление pass** (живой тач owner проверит сам).
  - **Воспроизводимость:** `poc/mobile/wgpu_native.cmake` — FetchContent пин wgpu-native + cargo
    `--locked` custom-command → IMPORTED static lib + header-шим. Тест==коммит.
  - code-review: 7 находок (4 low + 3 taste, все fixed — retain-cycle CADisplayLink через weak-proxy,
    -dealloc, одиночный тач, пауза в фоне).
  - **Android-эму DONE (`122cf14`):** NativeActivity чистый C++/NDK (android_native_app_glue),
    ANativeWindow→wgpu surface (Vulkan), тач(AMotionEvent)→виртуальный стик→PadAxis. wgpu-native из
    Rust под `aarch64-linux-android` (readelf: AArch64). Ручная APK (`build_apk.sh`: aapt2+zipalign+
    apksigner, без Gradle/dex). Запущено на Pixel_8a эму: рендер+РЕАЛЬНЫЙ тач (adb motionevent) двигает
    корабль. Ревью: 5 находок (1 high double-teardown→double-free) — фикс lifecycle локально в
    native_main (идемпотентный teardown, guard re-init, one-time sim-setup), общий batch.cpp НЕ тронут.
  - [ ] **Осталось (отдельная сессия):** iOS на owner-устройствах iPhone/iPad (нужна подпись owner),
    полировка тача/ориентации/виртуального-стика-оверлея на реальных устройствах.
- [~] **S3** Гейт 1 воспр. билд. **P0–P2 + SHA-пин депов — DONE** (`1e4c0e7`). Контейнер (P3 cross-machine) — отдельно.
  - **Флаги** (`poc/cmake/determinism.cmake`, до FetchContent → покрывает депы): `-ffile-prefix-map`
    (source+binary→`.`), `-fno-ident`, детерм. `ar Dqc`/`ranlib -D` (Linux) / `ZERO_AR_DATE` env (macOS),
    `--build-id=none` (Linux), MSVC `/Brepro`+`/INCREMENTAL:NO`; `SOURCE_DATE_EPOCH` (env) → `__DATE__`.
  - **rpath-фикс (`reproducible_rpath`):** GPU-таргеты иначе получают АБСОЛЮТНЫЙ build-rpath в _deps
    (путь течёт → ломает cmp); webgpu-хелпер хардкодит `INSTALL_RPATH "./"` (cwd-относит.). Перекрыли на
    `@executable_path`/`$ORIGIN` (7 GPU-таргетов) → детерм. И запуск из любого cwd (+ бонус для packaging S5).
  - **SHA-пин:** все 8 FetchContent-депов (тег→полный SHA, тег в комменте). stb уже был SHA.
  - **Проверка (T4):** macOS build-twice-на-разных-путях → 181 арт., 0 diffs; Linux (Docker ubuntu) →
    183 арт., 0 diffs; SHA-fetch ок; full `all` собран чисто; exe запускается из любого cwd. CI-гейт
    (ubuntu) build-twice+cmp. Ревью: 4 находки (1 med CI-время→ubuntu-only, 1 low cmp-guard, 2 taste).
  - [ ] **Осталось (отдельно):** герметичный Docker-тулчейн-контейнер + cross-machine байт-идентичность
    (P3 stretch); Windows/MSVC-детерминизм в CI (сейчас build-only, репро = follow-up).
- [x] **S4** Гейт 3 кросс-компиляция — **DONE** (`scripts/xcompile_verify.sh`, локальный pinned-T4).
  - **Desktop native-matrix замер:** single-node (macOS) clean `all` = 16s. CI matrix (3 ОС concurrent,
    `fail-fast:false`) wall = max(t_linux,t_win,t_mac), НЕ sum → ~3x (истор. CI: mac 42 / ubuntu 66 /
    win 62s → wall 66 vs sum 170). CI-изменений по mobile НЕ вносили (owner: локальный T4).
  - **Mobile true-cross (arch-verify pass):** iOS `aarch64-apple-ios-sim` → `lipo` arm64 + `vtool`
    platform IOSSIMULATOR (Mach-O EXECUTE ARM64); wgpu-native из Rust arm64. Android
    `aarch64-linux-android` (NDK arm64-v8a) → `file` ELF ARM aarch64; APK содержит
    `lib/arm64-v8a/{libgame,libc++_shared}.so`. iOS-device (не-симулятор) — не делали (owner: S10).
  - **Грабля:** `unzip -l big.apk | grep -q` под `set -o pipefail` → grep короткозамыкает, unzip
    SIGPIPE → ложный fail. Фикс: захват в переменную + pure-shell `case`.
- [x] **S5** Гейт 4+2 бандл per-OS + шов assetc→билд — **DONE** (macOS live).
  - **Шов (гейт 2):** `game/assets/atlas.png` (source) → `assetc --game` UASTC-бейк →
    `game/assets/game.bundle` (committed golden, hash `0x5e391d11f4910868`). Рантайм
    (`game/bundle_atlas.cpp`) открывает бандл через `AssetManager`, транскодит UASTC→BC7
    (`asset_gpu`) → `SpriteBatch` грузит BC7-текстуру (не процедурный код). `build_atlas()` остаётся
    fallback + для mobile-шеллов. WIN32 (`bundle_atlas_stub.cpp`): basis POSIX-only → процедурный
    fallback (паритет поведения). Fallback при отсутствии бандла проверен (рендерит).
  - **Бандл per-OS (гейт 4):** ручные CMake `install()`-правила (`COMPONENT game`): macOS `.app`
    (Contents/MacOS+Frameworks(@rpath `@executable_path/../Frameworks`)+Resources+Info.plist) /
    Linux tarball / Windows папка. version-stamp: `configure_file` git-hash → `version.txt`+Info.plist.
    `scripts/package.sh` (`cmake --install --component game`).
  - **T4 macOS live pass:** `.app` запущен из ЧУЖОГО cwd (/tmp) → dylib через @rpath + `game.bundle`
    через `../Resources` (самодостаточен); окно открылось+present+чистый выход; `--demo` рендерит
    корабль+starfield из **бейкнутой** BC7-текстуры (скриншот). Инварианты целы: determinism golden
    `0xa213fe659921c9fb`, asset synthetic `0xf2255dc74fbdb6bc` — не поехали; full `all` build чист.
    Linux tarball/Windows папка — install-правила есть, owner тестит на своих ОС (как S2).
- [x] **S6** Гейт 5 release-оркестрация — **DONE** (реальный Actions-прогон, T3 end-to-end).
  - **release.yml** на тег `v*` (+ workflow_dispatch): matrix **Linux+Windows** (macOS НЕ в CI —
    owner пакует локально; репо public → минуты безлимит, но macOS ×10 экономим) → configure+build
    game_sidescroller (`-DGAME_VERSION=тег`) → `cmake --install --component game` → архив
    (tar.gz/zip) → upload-artifact (retention 7d). publish: changelog (git log с прошлого тега) +
    `gen_vdf.sh` (app_build+depot_{linux,windows,macos}, placeholder appid, SetLive=beta) + gated
    steamcmd (skip без секретов) + `gh release` (idempotent).
  - **Проверка (T3):** throwaway-тег `v0.0.1-rc3` → run зелёный (linux+windows+publish success);
    GitHub prerelease с `like-nes-v0.0.1-rc3-{linux.tar.gz 4.8MB, windows.zip 3.2MB}` + steam-vdf;
    changelog «Changes since v0.0.1-rc2»; steamcmd skip-notice; VDF placeholder-путь. Локально:
    actionlint чист, VDF парсится (python vdf), gen_vdf отклоняет инъекцию. Экшены SHA-пиннуты.
    code-review: 6 находок (1 med/4 low/1 taste, все fixed). Теги rc/rc2 подчищены, rc3 = proof.
  - **Вскрыто matrix'ем (2 фикса, коммиты `494e2f8`+`fe634f0`):** game_sidescroller НИКОГДА не
    собирался на Windows в CI (ci.yml repro-шаг `if Linux`). (1) `find_package(Threads)` был в
    `if(NOT WIN32)` → на Windows `Threads::Threads` не найден (configure-фейл); вынесен на верх.
    (2) `-Wno-deprecated-declarations` (capture.cpp) → MSVC `D8021`; обёрнут в
    `$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:...>`.
  - ⚠️ **CI-hardening (вскрыто, отдельно от S6 — коммит `dc7e9b3`):** ветку раньше НИКОГДА не пушили
    → ci.yml на ней не гонялся; claim'ы «CI зелёный» из S1–S5 НЕ подтверждены. По просьбе owner
    починил ci.yml: (1) невалидный YAML — имена шагов 232/301 с `: ` → закавычены; (2) MSVC-guard
    GCC-флагов (stb_vorbis/stb_impl → `if(NOT MSVC)`). **Реальный прогон 3 ОС: Windows ✅** (впервые!),
    но **ubuntu/macos ❌ — предсуществующие баги специй #6/#7:** `ide/perf_test.cpp:28`
    `__has_feature(...)` в одном `#if` ломает GCC (нужен вложенный `#ifdef __has_feature`);
    macOS plugin-seam-гейт `dlopen` на `CMakeFiles/*.dir` (ci.yml `find -name 'plugin_*.*'` без
    `-type f` цепляет директорию).
  - **CI-hardening DONE — ВСЕ 3 ОС ЗЕЛЁНЫЕ ✅** (было 0/3; ci.yml не парсился). Коммиты
    `dc7e9b3`,`48abb93`,`10d3995`. Пофикшено **7 предсуществующих багов** (ветку не пушили → не
    ловились): (1) YAML-имена шагов с `: `; (2) `find_package(Threads)` в `if(NOT WIN32)` → Windows
    configure; (3) GCC-only `-Wno-*` флаги (capture/stb_vorbis/stb_impl) → MSVC D8021 →
    `if(NOT MSVC)`/gen-expr; (4) `perf_test.cpp` `__has_feature` в одном `#if` → GCC → вложенный
    `#ifdef`; (5) plugin-find без `-type f` → `dlopen` на `.dir`; (6) `find libzstd_static.a` пусто →
    реальный `libzstd.a`; (7) IDE Play-spawn ASan: crash-child намеренно SEGV'ит, ASan ловил →
    SIGABRT вместо SIGSEGV → `ASAN_OPTIONS=handle_segv=0:abort_on_error=0`.
    **Метод:** ubuntu-санитайзеры воспроизведены локально в Docker (aarch64) — 16/16 pass после
    фиксов, потом подтверждено реальным CI x86_64 (3/3 success). `scripts/xcompile_verify.sh` +
    Docker-репро — durable. См. [[branch-never-ci-validated]].
- [x] **S7** Игра: бой — **DONE** (коммит `4326234`, T4). Боевой слой в детерм. fix32 sim:
  стрельба игрока (кулдаун), волны врагов (LCG, целочисл. y), AABB-коллизии (raw fix32/int64,
  канон. порядок seq) пуля×враг=очки / враг×корабль=−жизнь, GameState-синглтон (владелец у
  вызывающего). Спрайты через #5-шов: atlas 256x256 (+enemy/bullet/битмап-цифры 0-9), rebaked
  `game.bundle` BC7 (hash `0xb4cd9519a96304f4`). HUD: счёт/жизни цифрами-спрайтами. **Проверка T4:**
  `game_sim_test` (headless, все ОС) → combat-golden `0xaef7d0d8bb69ca00` + run-to-run YES, CI-гейт
  на 3 ОС; demo рендерит бой+HUD (GIF); live macOS clean exit; iOS пересобран (mobile-паритет).
  Инварианты целы (determinism/asset goldens). Ревью: 4 находки (1 low sim_hash-hp + 3 taste),
  low+1 fixed, 2 обоснованно skip. **grabli:** пустые flecs-теги (Ship/Bullet) нельзя как `T&` в
  `each` → фильтр `has<>`; старый ship-push ловил ВСЕ Transform+Velocity (пули/враги тоже Velocity!).
- [x] **S8** Игра: босс + мини-сюжет + экран очков — **DONE** (коммит `11d36eb`, T4). Фаза-машина
  (Intro→Play→Boss→Victory/GameOver→рестарт, переходы по action_pressed edge, авто Play→Boss по
  kills>=10/таймауту) в детерм. sim. Босс (`boss.cpp`): HP=40 + въезд/осцилляция + **ответный огонь**
  hostile-снарядами; пуля×босс=−HP, hostile×корабль=−жизнь; HP-бар. Текст: шрифт расширен до A–Z
  (`draw_glyph`), atlas 256→512×256 (+boss/hostile/solid), rebaked `game.bundle` BC7; `push_text`+
  `push_screen` (intro/victory/gameover, центр. текст). **Проверка T4:** combat-golden
  `0x32a094e89eacf2f2` (phase=2 достигнута в скрипте, run-to-run + CI cross-arch); demo рендерит
  intro-экран+бой+босса с HP-баром+hostile-огонь (GIF); live macOS clean exit; iOS пересобран.
  Ревью: 4 находки (1 med mobile-intro-freeze → авто PH_Play на мобиле; 2 low + 1 taste) — все fixed.
  **grabli:** phase-гейт движения корабля заморозил бы mobile (нет A_Fire-ввода) → авто-старт Play;
  создание entity внутри flecs `each` (boss hostile) → выносить спавн после итерации.
- [x] **S9** Игра: частицы + bloom + аудио — **DONE** (коммит `0523619`, T3). Всё презентационное →
  детерминизм sim не тронут (combat-golden `0x32a094e89eacf2f2` НЕ изменился). Частицы (`fx.cpp`):
  взрывы/искры/muzzle/выхлоп-трейлы + наклон корабля (batch Instance+=rot). Ключ: **fx-события —
  побочный выход** `combat_step(FxSink* fx=nullptr)` → при nullptr (headless) sim идентичен.
  Bloom (`bloom.cpp`, техника #2): HDR-таргет→bright-pass→blur(half-res)→ACES-tonemap+композит;
  яркие tint>1 (пули/hostile/частицы) светятся. Аудио (`audio.cpp`, #3, локально): audio.bundle
  (sfx+music)→Mixer→AudioEngine событийно + музыка-луп + miniaudio-device; pimpl+guard
  AUDIO_HAVE_MINIAUDIO → CI headless no-op (game не линкует audio). **Проверка T3:** live macOS
  (bloom+частицы+наклон+"audio: on"+clean exit, owner слышит звук); demo-GIF взрывов/bloom;
  golden цел; full/audio-off/iOS собираются; determinism+audio_seam goldens целы. Ревью: 4 low
  (bloom.cpp>200→bloom_shaders.hpp; mobile-tint-clamp документ.; bloom.init()-check; мёртвые поля)
  — все fixed. **grabli:** визуальные эффекты держать вне sim (побочный fx-канал, не GameState) →
  golden не двигается; лёгкий `fx_events.hpp` (без webgpu) для combat → headless game_sim_test цел.
- [x] **S10** Гейт 7 полная игра на mobile — **DONE** (T4, iOS-sim + Android-эму live). Полный
  desktop-паритет S7–S9 на обоих таргетах. Развилки (owner AskUserQuestion 2026-07-25): полный
  gameplay-паритет + экранная кнопка-огонь + bloom ON + аудио graceful + T4 обе платформы.
  - **Общий модуль `poc/mobile/mobile_game.{hpp,cpp}` (DRY):** весь кадр (sim+частицы+HUD+сюжетные
    экраны+bloom(HDR)+audio+кнопка-огонь+мульти-тач стик/огонь) в ОДНОМ месте; шеллы `ios/view.mm` +
    `android/native_main.cpp` стали тонкими (только платформенный тач/lifecycle → pointer/cancel/frame).
    `stick.hpp` перемещён `ios/`→`mobile/` (теперь общий). CMake обоих: +`fx.cpp bloom.cpp audio.cpp`
    +`mobile_game.cpp`, include `${POC}/mobile`.
  - **Огонь:** экранная кнопка (правый-низ, круг atlas.star, светится в bloom) → тач `PadButtonDown
    PadA` через тот же HAL → `A_Fire`. Настоящая фаза-машина (убран авто-PH_Play): intro→play по
    `action_pressed`. Мульти-тач: левая зона=стик, кнопка=огонь одновременно (iOS multipleTouchEnabled,
    Android pointerId). `--demo` (iOS-симулятор, simctl без тача) скриптует стик+A_Fire → босс гибнет.
  - **Bloom:** batch target RGBA16Float→`bloom.resolve`→swapchain (Metal/Vulkan). Fallback LDR при
    `bloom.init()==false` (use_bloom_ синхронно рулит форматом batch И таргетом кадра).
  - **Аудио:** `audio.cpp` линкуется БЕЗ `AUDIO_HAVE_MINIAUDIO` → no-op stub (`init()==false`), шов в
    кадре идентичен desktop-CI-headless. Полноценный miniaudio-девайс + шиппинг audio.bundle на mobile —
    **follow-up** (как iOS device-подпись; блокер — APK AAssetManager / iOS bundle-resource).
  - **Проверка T4:** iOS-sim (iPhone 16, `--demo`) — HUD/HP-бар/частицы/bloom/кнопка/GAME OVER-экран
    (скриншоты). Android-эму (Pixel_8a, **РЕАЛЬНЫЙ adb-тач**) — intro→тап-огонь→play→boss, поток пуль,
    HP-бар, частицы, bloom (скриншоты). combat-golden `0x32a094e89eacf2f2` НЕ сдвинут (детерминизм цел).
    Desktop `game_sidescroller`+`game_sim_test` собираются (poc/game/* не менялся). Android debug-APK
    (apksigner). Ревью code-reviewer: 5 находок (1 med Android ACTION_CANCEL не сбрасывал все указатели
    → застрявший огонь/движение; 2 low: Down-в-кнопку→стик, init не идемпотентен bloom/batch/audio;
    2 taste). Все ≥low + 1 taste fixed, пересобрано+re-verify pass.
  - [ ] **Осталось (отдельно):** iOS на owner-устройствах iPhone/iPad (подпись/provisioning owner сам);
    полноценное mobile-аудио (miniaudio-девайс + audio.bundle в APK/`.app`); полировка тача/оверлея.
- [x] **S11** Финализация — **DONE**. ADR 0008 **Accepted** (2026-07-25, validation gate закрыт, все
  7 гейтов + CI 3 ОС); spec #8 **Validated** (сессии S1–S11 done); README строка #8 → **Закрыта**;
  dev-log записан. Спека #8 (build/deploy + кросс-компиляция) полностью закрыта. Follow-up (gated,
  вне #8): Steam-заливка (appid), macOS notarization / Windows code-sign, mobile прод-подпись устройств
  + сторы, P3 cross-machine контейнер, полноценное mobile-аудио, live iOS-устройства (owner-подпись).

## Открытые числа (заполнять по PoC)
- build-time native-matrix (`max` vs sum) — **S4 done:** single-node macOS 16s; matrix wall=max, не sum.
- byte-repro pass per-OS (`cmp`) — **S3 done:** macOS 181 арт. 0 diffs, Linux(Docker) 183 арт. 0 diffs.
- version-stamp custom-target форма (без лишних ре-компиляций) — **S5 done:** `configure_file` git-hash
  → `version.txt` + Info.plist (@ONLY, при install; ре-компиляций exe не вызывает).

## Заметки/грабли (пополнять)
- FetchContent-депы живут в `build/_deps/*-src` → `-ffile-prefix-map` маппить и BINARY dir, не только SOURCE.
- Сейчас SHA-пин только у `stb`; `glfw/webgpu/imgui/flecs/zstd/basisu/miniaudio/glfw3webgpu` — теги/ветки → SHA для P3.
- Prebuilt wgpu/wasmtime/miniaudio dylib — не через compile-гейт (копируются), а через checksum-пин.
- Research-инцидент (2026-07-23): general-purpose суб-агенты каскадно наплодили вложенных → auth-403.
  Впредь research через `Explore` (нет инструмента Agent) — не может спавнить под-агентов.
- **S2a-грабли:** `render_core`/`Renderer` (#2) — ОДНО-спрайтовый deferred+bloom showcase
  (`SceneSnapshot` = 1 opaque + 1 glow + 3 света), НЕ батч. Для игры (много спрайтов) → свой
  instanced-батч (`game/batch.cpp`), переиспользуя `GpuContext` (`render/gpu.cpp`) + WebGPU-идиомы.
  Bloom/эффекты Renderer подключим в S9 (утолщение).
- **WGSL:** `half` — зарезервированное слово (naga main-v0.2.0 его ПРИНЯЛ, но future-proof →
  переименовано в `half_extent`); для S2b (wgpu из Rust, возможно новее naga) особенно важно.
- **Retina-viewport:** world→NDC маппинг должен делить на ЛОГИЧЕСКИЙ дизайн-размер (VIEW_W/H),
  НЕ на framebuffer-пиксели (на macOS retina fb = 2× → сцена сжимается в центр). Surface конфигурим
  в fb-пикселях, а координатный маппинг — резолюшн-независимо.
- **fix32-инвариант:** позиции-как-sim-state считать в целочисленном домене (LCG % range → from_int),
  НЕ через float-арифметику + from_float (даже для «визуальной» раскладки — ломается на fast-math/FMA).
- **GIF-пайплайн (нет ffmpeg/magick):** `--demo` пишет PNG-кадры → `uv run --with Pillow python3`
  собирает GIF (эфемерно, Python uv-managed — pip запрещён). Готовый скрипт-паттерн в job-tmp.
- **S2b-грабли (iOS):**
  - Версию wgpu-native брать из `_deps/webgpu-backend-wgpu-src/wgpu-native-git-tag.txt` (v0.19.4.1) —
    та же версия → заголовки байт-идентичны → C++ без изменений. Собирать иную версию = риск API-дрейфа.
  - Заголовки wgpu-native лежат `ffi/webgpu-headers/webgpu.h` + `ffi/wgpu.h` (оба include `"webgpu.h"`
    из своей папки), а код включает `<webgpu/…>` → нужен header-шим `webgpu/{webgpu.h,wgpu.h}`.
  - GLFW — desktop-only; на iOS окно/surface = UIView(CAMetalLayer)+`WGPUSurfaceDescriptorFromMetalLayer`.
    Тач-ввод → PadAxis через action-map (ре-юз gamepad-биндингов), НЕ новый путь.
  - **Ориентация iOS 26:** landscape-mask на программном UIWindow НЕ поворачивает устройство, а
    ВРАЩАЕТ контент → сцена на 90°. Решение: portrait-lock + **аспект-корректный вьюпорт в движке**
    (мир 960×540 вписывается в любую surface, короткая ось заполняется) — робастно к ориентации/DPI.
  - Симулятор: `simctl` НЕ инъектит тач → доказательство управления через `--demo` (scripted PadAxis),
    как на десктопе. Скриншоты `simctl io booted screenshot` → GIF через Pillow.
  - CADisplayLink retain-cycle: target через weak-proxy (`forwardingTargetForSelector`), иначе VC+все
    C++/WebGPU-ивары не освобождаются; +пауза по background-нотификациям (краш Metal при рендере в фоне).
- **S2b-грабли (Android):**
  - NativeActivity (чистый C++/NDK, `android_native_app_glue`, `ANativeActivity_onCreate` через
    `-u`-линк) → surface из ANativeWindow (SType 0x9), wgpu использует **Vulkan**-бэкенд on-device.
    Тач(`AMotionEvent`, ACTION_MASK, getX/Y(0))→виртуальный стик→PadAxis (тот же HAL). НЕ нужен Java/JNI.
  - **APK без Gradle:** NativeActivity не требует classes.dex → ручная упаковка `aapt2 link` (только
    манифест, без res) + `zip` native-либ + `zipalign 4` + `apksigner` (debug-keystore). Минует
    риск AGP×JDK26. `extractNativeLibs=true` → сжатые .so ок. Нужен `libc++_shared.so` из NDK рядом.
  - **cargo под android из CMake:** линкер = `$NDK/.../aarch64-linux-android<API>-clang` через env
    (`CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER`/`CC_`/`AR_`). API брать из `ANDROID_PLATFORM_LEVEL`
    (НЕ `CMAKE_SYSTEM_VERSION` — тот дал `1` → `...android1-clang` not found).
  - **Lifecycle:** `APP_CMD_TERM_WINDOW`+`destroyRequested` → teardown ЗОВЁТСЯ ДВАЖДЫ → `SpriteBatch::
    shutdown()` не зануляет хэндлы → double-free. Решение: идемпотентный teardown (guard по `ready`).
    Повторный `INIT_WINDOW` → guard в init + one-time sim-setup (иначе `spawn()` дублирует сущности).
  - **Плюс vs iOS:** `adb shell input motionevent DOWN/MOVE/UP` инъектит РЕАЛЬНЫЙ тач → можно доказать
    тач-путь по-настоящему (на iOS-симуляторе simctl не умеет — там scripted). adb-раундтрип ≈ задержка
    между кадрами (foreground sleep заблокирован).
- **S10-грабли (полный mobile-паритет):**
  - **Android `ACTION_CANCEL`** (шторка/звонок/системный жест): pointer-index в action НЕ кодируется →
    `idx=0`, освобождение по одному id оставляет чужой fire_id_/stick_id_ висящим (застрявший огонь/
    движение). Фикс: CANCEL → безусловный сброс ВСЕХ активных касаний (`MobileGame::cancel()`), не
    dispatch-Up по idx. iOS `touchesCancelled` даёт весь set → там ок.
  - **Кнопка-огонь = круг, а не только «первый палец»:** Down внутри круга при занятом fire_id_ НЕ
    должен становиться стиком (иначе случайный дрейф-стик в углу). Проверять попадание в круг ДО
    ветки стика.
  - **`--demo` держит A_Fire:** `PadButtonDown PadA` раз на t=60 (не отпуская) → down-edge проходит
    intro→play (`action_pressed`), удержание → непрерывный огонь (`action_held`) → босс гибнет без
    реальной кнопки (симулятор без тача).
  - **Идемпотентность init/shutdown mobile-модуля:** Android TERM→INIT_WINDOW зовёт teardown/init
    повторно; `world_ = flecs::world()` (re-spawn на свежем мире, иначе дубли сущностей) + флаг
    `inited_` (init самоочищается, shutdown no-op без парного init → нет утечки bloom/batch/audio и
    double-free; batch_/bloom_.shutdown() не зануляют хэндлы).
  - **Mobile-аудио честно = stub:** линковать `audio.cpp` без `AUDIO_HAVE_MINIAUDIO` → `init()==false`,
    шов кадра идентичен CI-headless. Реальный miniaudio-девайс блокирован шиппингом `audio.bundle`
    (APK → AAssetManager-извлечение; iOS → bundle-resource + NSBundle-резолв) — follow-up, не в S10.
