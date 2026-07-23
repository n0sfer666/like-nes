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
- [ ] **S2b** Mobile bring-up: **wgpu-native из Rust-исходников** под aarch64-linux-android +
  aarch64-apple-ios(+sim), запуск игры на Android Pixel 8a эму / iOS sim+iPhone+iPad (owner-подпись
  устройств). **T4: запуск+управление.** Отдельная сессия (owner решил резать S2a/S2b — тяжёлый
  mobile-тулчейн + завязка на owner-устройства).
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
