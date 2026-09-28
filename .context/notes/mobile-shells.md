# Мобильные оболочки: паритет с десктопом и гейт iOS-симулятора (2026-09-27)

Поводок: фикс id касания iOS в B8 (`142e3e1`) нечем проверить, кроме `clang++ -fsyntax-only`.
Оболочки iOS и Android не собираются: исходники игры перечислены в них руками, список застыл до
`4bd2002` (`controls.cpp` удалён), `mobile_game.cpp` отстал от API (`make_map`, `TrailQuery`,
`push_scene`, `Fx::render`, `begin_clear`) — 6 ошибок компиляции, до линковки дело не доходило.

## Решения владельца (интервью 2026-09-27)

| вопрос | решение |
|---|---|
| рантайм симулятора | ставлю сам — стоит iOS 27.0 (24A434) |
| чем слать касания | XCUITest: события через UIKit и `view.mm`, двухпальцевый жест = два касания разом |
| раунды | два: (1) оболочки собираются, `xcompile_verify.sh` зелёный; (2) гейт симулятора |
| где гейт | локально в `preflight.sh`; без рантайма — явный пропуск с причиной; в CI не добавлять |
| исходники | общий список `example_ugly_game/game_sources.cmake` для `game_sidescroller` и обеих оболочек |
| объём игры | ПАРИТЕТ с десктопом: бандл в ресурсах `.app`/APK, материалы, пресеты ввода, достижения |
| Android | NDK ставлю, чиню обе оболочки |

## Окружение (поставлено 2026-09-27)

- Xcode 27.0 (27A266a), SDK `iphonesimulator27.0`, рантайм iOS 27.0; устройство `iPhone 16` есть в
  devicetypes, само устройство под гейт ещё не создано.
- rustup: `aarch64-apple-ios-sim`, `aarch64-linux-android` (wgpu-native собирается из Rust,
  `platform/mobile/wgpu_native.cmake`, тег v0.19.4.1).
- Android: `brew --cask android-commandlinetools`, SDK в `~/Library/Android/sdk`:
  `ndk;28.2.13676358`, `platforms;android-35`, `build-tools;35.0.0` — версии из `build_apk.sh`.
  Лицензии SDK приняты `sdkmanager --licenses`. Проверено `ls`: всё на месте; рядом лежат
  `android-36.1` и build-tools `36.1.0`/`37.0.0` от Android Studio — `build_apk.sh` их не берёт.
  Префикс NDK на macOS — `darwin-x86_64` (так в самом NDK, HOST_TAG скрипта совпадает).

## Раунд 1 — план

1. `game_sources.cmake`: платформенно-независимые исходники `game_sidescroller`
   (`example_ugly_game/CMakeLists.txt:116`); десктоп берёт его + свои (glfw, `main.cpp`, `live.cpp`).
2. Оболочки линкуют то, что десктоп берёт целями (`engine_core`, `framework_input`,
   `framework_graphics`, `asset_gpu`, `ach_bundle`, `material_hot`, …): решить, собирать ли их из
   корня (`add_subdirectory` с мобильным набором опций) или тем же общим списком — ВЫНЕСТИ на
   интервью, если общий список исходников их не покрывает.
3. `mobile_game.cpp` — по образцу `live.cpp` (`make_trail_query`, `push_scene(batch, world, atlas,
   sfx)`, `begin_clear(enc, view, color)`), плюс бандл из ресурсов `.app`/APK.
4. `xcompile_verify.sh` зелёный целиком; runbook (`docs/owner-verification.md`, `owner-setup.txt`)
   — в том же коммите.

## Решения по пункту 2 (интервью 2026-09-27)

| вопрос | решение |
|---|---|
| как собирать цели движка | корень — единственный вход: `cmake -S .` с iOS/Android-тулчейном; мобильный режим пропускает glfw/webgpu-dist/imgui/wasm/tools/тесты, `webgpu` = алиас `wgpu_native`, `engine/*` — `EXCLUDE_FROM_ALL`, оболочка — `add_subdirectory(platform/ios\|android)` |
| материалы | `material_gpu` без `material_hot`: `game::MaterialFx` разрезается — наблюдение шейдера уходит в свой класс (`material_hot`), `MaterialFx` остаётся на `material_gpu` (решение владельца поверх рекомендации «линковать hot») |
| звук | реальный miniaudio (iOS CoreAudio+AVFoundation, Android AAudio/OpenSL), graceful no-op без устройства |
| проверка | T2 `xcompile_verify.sh` + smoke в симуляторе (`simctl install/launch`, stdout + скриншот) |

Факты под решением: `CMAKE_SOURCE_DIR` в `engine/` и `example_ugly_game/` — 19 мест (корень под
оболочкой их ломает); `platform_core` на APPLE тянет FSEvents (на iOS нет), вне Apple — `-lrt` и
`shm_open` (в bionic нет); wgpu-native на десктопе и мобиле один — v0.19.4.1. Шов платформы
ветвится в `engine/platform/CMakeLists.txt`, недоступное — явный отказ/poll, без `#ifdef` в `.cpp`.
Состояние достижений — песочница приложения; нативный бэкенд-плагин на мобиле не грузится.

## Раунд 1 — итог (2026-09-28)

Сделано по решениям выше; `xcompile_verify.sh` зелёный, smoke в симуляторе iPhone 16 / iOS 27.0:
`materials: on (3 pipeline(s))`, `audio: on`, `achievements: 5 defined`, титул на экране. Находки,
которых план не предвидел:

- **wgpu-native v0.19.4.1 не собирается под clang 21 (Xcode 27) ни под какую цель**, не только iOS:
  bindgen 0.69 из его `Cargo.lock` теряет поля 72 структур `webgpu.h` (в AST больше нет
  `ElaboratedType`) → 248 ошибок rustc. `platform/mobile/wgpu_bindgen.patch` поднимает bindgen до
  0.72.1, `wgpu_native.cmake` накладывает его идемпотентно. Десктоп берёт prebuilt и не задет.
  Upstream-баг `build.rs` (`-isysroot {sdk}` одной строкой с `\n`) остаётся — безвреден: clang
  предупреждает о несуществующем sysroot, а заголовки берёт из SDK симулятора (так в
  `rerun-if-changed` вывода build-скрипта).
- **SDK iOS 27 требует жизненный цикл UIScene**: `app.mm` — `SceneDelegate`, манифест сцены в
  `Info.plist.in`; `UIScreen.mainScreen` → `traitCollection.displayScale`.
- **bionic API 24**: нет `posix_spawn*` (API 28), `addclosefrom_np` (API 34), `shm_open` вовсе —
  `platform_process_android.cpp` и `platform_shmem_android.cpp` — явные отказы шва.
- **stdout NativeActivity уходит в /dev/null** — `platform/android/stdout_logcat.cpp` заворачивает
  его в logcat (тег `like-nes`), иначе стартовые строки на устройстве не видны.
- **Статический wgpu_native дважды в строке линковки** → `ld: ignoring duplicate libraries` на
  Apple; в мобильном режиме корень включает `CMP0156 NEW` (дедупликация по возможностям линкера).
- **Категория сессии iOS**: умолчание miniaudio — PlayAndRecord (сессия с микрофоном);
  `engine/audio/device.cpp` создаёт контекст сам и ставит SoloAmbient (вне iOS поле не читается).
- **Фон = смерть без dealloc/onDestroy**: `MobileGame::suspend()` сохраняет достижения при
  `UISceneDidEnterBackground` и `APP_CMD_PAUSE`. Повторный init сбрасывает `controls_` (пресет
  дописывает привязки, а не заменяет), отказ init освобождает GPU, APK без `audio.bundle` или
  `library.bundle` деградирует, а не падает.
- Процедура владельца — `docs/owner-setup.txt` §R (симулятор прогнан; iPhone и Android-устройство —
  у владельца).

Попутное, не чинилось: `Info.plist.in` обещает `MinimumOSVersion 14.0`, а бинарь собран с
`minos 27.0` (`CMAKE_OSX_DEPLOYMENT_TARGET` не задан) — на iOS < 27 не запустится.

## Раунд 2 — итог (2026-09-28)

Гейт `scripts/ios_sim_gate.sh` (устройство и находки — `.context/gates/ios-sim.md`): корень под
генератором Xcode в `build-ios-xcode`, UI-тест `platform/ios/uitest/touch_gate.m` ставит стик и
огонь разом через приватный синтез XCUIAutomation, вердикт `ios_touch_verdict.py` судит строки
`[touch]` из системного лога (флаг `--touch-probe` у `view.mm`), два скриншота из `.xcresult`.
Этап preflight, в CI нет; живые мутанты (`multipleTouchEnabled = NO`, id-константа) — руками.

- Набросок ждал склейки касаний на `int` — **опровергнуто**: усечённые id различны (улика в
  сводке вердикта). B8 остаётся правильным по типу, но регресса «на int» гейт не ловит — ловит
  мутант с константой.
- Синтезатор вставляет лишний тап (2 мс) — судится история, а не последовательность.
- Попутное из раунда 1 (`MinimumOSVersion 14.0` против `minos 27.0`) по-прежнему не чинилось.

Дальше: iPhone и Android-устройство — у владельца (`docs/owner-setup.txt` §R, шаги 4–5).

## Запуск игры в симуляторе руками (2026-09-28)

Владелец не смог поднять игру по §R шаг 2. Причины: bash-синтаксис (`$(…)`, `\`) в Nushell;
`xcompile_verify.sh` требует Android SDK ради одной iOS-игры; повторный `simctl create` заводил
двойника `like-nes-smoke`; `boot` загруженного падает; **в Xcode 27 нет Simulator.app** — окно
устройства теперь Device Hub (`com.apple.dt.Devices`), `open -a Simulator` не находит приложение.
Итог — `bash scripts/ios_sim_run.sh [--background-save]`: сборка `build-ios`, устройство
`like-nes-play-<рантайм>`, Device Hub, игра с консолью. Режим фона — PASS/FAIL вместо ручного `ls`:
автосейв на титуле не пишет (прогресс не меняется, замер 20 с), уход в фон пишет через 10–21 с;
мутант без `game_.suspend()` в `pause` — FAIL.

Раунд 3 (решение владельца): попутные баги мобилы — `MinimumOSVersion 14.0` против `minos 27.0`,
AVAudioSession на главном потоке, копящиеся `ios-sim-gate.*` в `$TMPDIR`.
