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
- [ ] **S2** Скелет игры + кросс-платф. bring-up (flecs+fix32, корабль, ввод #4, рендер #2, спрайты #5,
  **wgpu из Rust под mobile**). **T4: запуск+управление на macOS live / iOS sim+iPhone+iPad / Android
  Pixel 8a эму; Linux+Win бандлы owner тестит сам.** Может делиться S2a desktop / S2b mobile.
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
