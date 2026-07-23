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
- [~] **S2b** Mobile bring-up. **iOS-симулятор — DONE**, коммит `727be61`.
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
  - [ ] **Осталось (отдельные сессии):** Android-эму (NDK + ANativeWindow + APK), iOS на owner-
    устройствах iPhone/iPad (подпись), полировка тача/ориентации на устройстве.
- [ ] **S3** Гейт 1 воспр. билд (P0–P3), CI build-twice+cmp.
- [ ] **S4** Гейт 3 кросс-компиляция (native-matrix замер + mobile NDK/iOS build).
- [ ] **S5** Гейт 4+2 бандл per-OS + шов assetc→билд.
- [ ] **S6** Гейт 5 release-оркестрация (tag→matrix→артефакты+VDF).
- [ ] **S7** Игра: бой (снаряды/враги/коллизии/счёт).
- [ ] **S8** Игра: босс + мини-сюжет + экран очков.
- [ ] **S9** Игра: частицы + bloom #2 + аудио #3.
- [ ] **S10** Гейт 7 полная игра на mobile (пере-прогон S7–S9 на эму/симуляторе/owner-устройствах + подпись).
- [ ] **S11** Финализация (ADR Accepted / spec Validated / README #8 / dev-log).

## Открытые числа (заполнять по PoC)
- build-time native-matrix (`max` vs sum) — S4.
- byte-repro pass per-OS (`cmp`) — S3.
- version-stamp custom-target форма (без лишних ре-компиляций) — S5.

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
