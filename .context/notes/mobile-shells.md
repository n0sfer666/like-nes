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
  `ndk;28.2.13676358`, `platforms;android-35`, `build-tools;35.0.0` — пин NDK в `android_ndk.sh`,
  платформа и build-tools в `package_apk.sh`.
  Лицензии SDK приняты `sdkmanager --licenses`. Проверено `ls`: всё на месте; рядом лежат
  `android-36.1` и build-tools `36.1.0`/`37.0.0` от Android Studio — `package_apk.sh` их не берёт.
  Префикс NDK на macOS — `darwin-x86_64` (так в самом NDK); с В1в его даёт тулчейн NDK
  (`ANDROID_HOST_TAG`), а не `uname` скрипта.

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
  `platform_process_android.cpp` и `platform_shmem_android.cpp` — явные отказы шва. С #26 В1а
  `minSdkVersion` = 29: источник — `<uses-sdk>` манифеста игры, `build_apk.sh` передаёт его
  `-DANDROID_PLATFORM`, манифест бенча обязан совпасть, `wgpu_native.cmake` сверяет
  `ANDROID_PLATFORM_LEVEL` с манифестом (тулчейн без флага молча взял бы 21). Отказы шва — как были.
- **SDK и NDK** — `platform/android/android_sdk.sh` (его же подключает `mobile_bench.sh`): пин NDK
  сверяется с `Pkg.Revision` выбранного каталога, `ANDROID_NDK_HOME` раннера GitHub не подменит его.
- **ABI** (#26 В1а): `build_apk.sh --abi arm64-v8a|x86_64` (умолчание arm64-v8a), каталог
  `build-android[-bench][-x86_64]`; Rust-цель wgpu-native — из `ANDROID_ABI`, уровень API — из
  `ANDROID_PLATFORM_LEVEL` без умолчания. Хост — по `uname -s` (darwin-x86_64 / linux-x86_64).
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

## Раунд 3 — итог (2026-09-29)

Решения владельца: минимальный iOS 17.0, `noAudioSessionActivate`, улики — пять последних
прогонов, любое предупреждение рантайма в UI-тесте — красный гейт. Устройство и позитивные
контроли — `.context/gates/ios-sim.md`.

- **minos.** `platform/mobile/ios_min_os.cmake` (корень включает его до `project()`) задаёт
  `CMAKE_OSX_DEPLOYMENT_TARGET`; `Info.plist.in` и cargo (`IPHONEOS_DEPLOYMENT_TARGET`) читают её.
  Согласие бинаря, plist и кеша проверяет `platform/mobile/ios_min_os.sh` — в `xcompile_verify.sh`
  (`build-ios`) и в гейте (`build-ios-xcode/Debug`). Кеш cargo переменную не видит: уже собранный
  wgpu-native несёт умолчание Rust (14.0 симулятор, 10.0 устройство) до чистой сборки — безвредно,
  объекты старше цели. Ниже
  iOS 27 не прогонялось ничего.
- **AVAudioSession.** Предупреждение давал `setActive:true` в `ma_context_init` на главном потоке
  (и `setActive:false` в `ma_context_uninit` — нашло ревью: `close()` из `dealloc` и рестарта
  `poll`); обе выключены, сессию активирует запуск AudioUnit. Симулятор доказывает только
  отсутствие предупреждения и `audio: on`; слышимость, возврат из фона и прерывание — iPhone
  владельца (§R шаг 4).
- **Улики.** Гейт в начале прогона удаляет `ios-sim-gate.*` старше четырёх последних.

- **Второй вход — бенч ресима (спека #25, В9г).** `-DLIKE_NES_MOBILE_BENCH=ON
  -DLIKE_NES_BENCH_AGAINST=<файл>` собирает вместо игры `com.likenes.resimbench` (та же оболочка,
  другая `mobile::Game` — `platform/mobile/bench_game.cpp`; манифест Android — тот же шаблон
  `AndroidManifest.xml.in`, с В1в спеки #26); обе
  `CMakeLists.txt` делают `return()` до цели игры. Каталоги `build-ios-bench`,
  `build-ios-bench-device`, `build-android-bench` — свои, кэш игры не трогают. Устройство и риски —
  [`gates/resim-bench.md`](../gates/resim-bench.md).

## Оболочка и игра (спека #26, В1б)

- Оболочка — статическая библиотека `like_nes_mobile` в каждой ОС: iOS — `app.mm`, `view.mm`,
  `host.mm`; Android — `native_main.cpp`, `android_host.cpp`, `apk_assets.cpp`,
  `stdout_logcat.cpp`; обе несут `engine/render/gpu.cpp` (из `GAME_CORE_SRC` цели игры он
  вычеркнут `list(REMOVE_ITEM)`). Игру она получает через `mobile::make_game()`
  (`platform/mobile/mobile_app.hpp`), услуги отдаёт через `mobile_host.hpp` (`asset_path`,
  `has_arg`, `device_info`).
- Жизнь игры и поверхности разведены: `init`/`shutdown` — раз на процесс, `attach_surface`/
  `detach_surface` — на каждом уходе в фон (iOS: `UISceneDidEnterBackground` снимает поверхность,
  Android: `APP_CMD_TERM_WINDOW`). Шутер строит bloom/материалы/батч на attach и рвёт на detach;
  мир, звук и достижения живут дольше. Улика — stderr `[game] surface detached tick=T world=H` и
  `[game] surface attached #N tick=T world=H` (`world` — `sim_hash`): после возврата из фона пара
  совпадает.
- `wants_frames()` решает, будит ли оболочка петлю: false — Android ждёт событий без таймаута,
  iOS ставит `CADisplayLink` на паузу, до следующего attach/resize. Бенч кадров не просит вовсе;
  шутер — пока у него есть цели поверхности и устройство не потеряно.
- Потеря устройства уходом в фон не лечится: GPU оболочка не пересоздаёт, шутер перестаёт просить
  кадры, картинку возвращает только перезапуск. Решение В1б, а не забытое: пересоздание GPU с
  сохранением мира — отдельная работа, а теряется устройство на телефоне редко.
- `suspend`/`resume` — о жизни на переднем плане, не о поверхности: Android шлёт `APP_CMD_RESUME`
  раньше нового окна, iOS зовёт `resume` после attach. Игра на этот порядок не опирается.
- Шутер — `example_ugly_game/shooter_mobile.{hpp,cpp}` (`game::MobileGame`) и
  `shooter_touch.{hpp,cpp}` (обёртка над `framework::touch::TouchSource`: раскладка — ассет `touch`
  бандла, касания уходят в движок ввода устройством Touch, демо — падом).
  Бенч ресима — `platform/mobile/bench_game.cpp`: счёт в своём потоке с первого `init`, GPU и
  окно поднимаются и для него (на Android — нужен разблокированный экран).
- Android распаковывает ВСЕ файлы корня `assets/` APK в `internalDataPath/assets`; какие из них
  обязательны, знает только игра (без `game.bundle` откажет её `init`).
- `frame(dt)` шутер не читает: тик 1/60 на кадр, на 120 Гц игра вдвое быстрее — долг до В2.

## Оболочка в SDK (спека #26, В1в)

- Одна функция упаковки у дерева и у игры против SDK — `like_nes_mobile_app(<цель> SOURCES … [BUNDLES …]
  NAME … ID …)` (`cmake/like_nes_mobile_app.cmake`): iOS — `.app` с ассетами в `assets/` и
  лицензиями в `Resources/licenses`; Android — `.so` с `-u ANativeActivity_onCreate -u android_main`
  и цель `<цель>_apk` (зовёт `platform/android/package_apk.sh`). Манифест — шаблон
  `AndroidManifest.xml.in` (id, имя, библиотека, minSdk). Испечённые `like_nes_bake` бандлы
  дописываются свойством `LIKE_NES_ASSETS`, поэтому упаковка — отдельной целью после линковки.
- `libc++_shared.so` едет в APK всегда, хотя `ANDROID_STL` по умолчанию статическая и в NEEDED
  `libgame.so` её нет: поставку обещают NOTICE и `xcompile_verify.sh`.
- Мобильное дерево — `platform/mobile/tree.cmake` (корень делает `return()` после него):
  движок EXCLUDE_FROM_ALL, оболочка, установка префикса. Префикс несёт слово платформы
  (`cmake/sdk_platform.cmake`: `desktop | ios-sim | ios-device | android-<abi>`), Config сверяет
  его со сборкой игры и отказывает на чужой. Платформенные половины установки и Config —
  `install_sdk_{desktop,mobile}.cmake`, `sdk_config_{desktop,mobile}.cmake`; ассет-компилятор на
  мобильной сборке — хостовый, `-DLIKE_NES_HOST_PREFIX=<десктопный префикс>`.
- `build_apk.sh` только конфигурирует дерево и собирает `<цель>_apk`, копию кладёт в
  `<build>/apk/<имя>.apk` — путь, который читают `xcompile_verify.sh` и runbook. Выбор SDK —
  `android_sdk.sh` (его же зовут `package_apk.sh` и `mobile_bench.sh`), пин NDK — `android_ndk.sh`
  (только сборке дерева: игра против префикса приходит со своим тулчейном).
- `<цель>_apk` в ALL, как `.app` на iOS: голый `cmake --build` даёт то, что ставится на телефон.
- Config мобильного префикса сверяет до первой цели: платформу, уровень API (Android), ревизию NDK
  (`_ln_ndk_rev` из `source.properties` NDK сборки префикса — статика собрана его libc++),
  `CMAKE_OSX_DEPLOYMENT_TARGET` с `_ln_ios_min` (iOS: пустой дал бы пустой MinimumOSVersion), и
  хостовый префикс — десктопный, той же сборки `_ln_build` (`<версия>+<git-хеш>`), с `bin/assetc`.
- `like_nes_mobile_app` отказывает на ID не вида `com.example.game` и на `& < > "` в NAME (уходят в
  XML без экранирования); ассеты абсолютизируются от каталога вызова, два файла с одним именем в
  `assets/` — отказ.
- **Долг В1д закрыт (2026-10-16).** Мобильная половина `check_sdk_game.sh` —
  `scripts/sdk_game_mobile.sh` (iOS-симулятор на macOS, Android x86_64 при пиннутом NDK; в CI
  `SDK_GAME_ANDROID` делает его обязательным на Linux и выключает на macOS), процедура мобильного префикса — `docs/{en,ru}/guide/sdk-and-your-game.md`.
  Относительный `-Dlike-nes_DIR` под кросс-тулчейном не раскрывается (проверено: `like-nes_DIR`
  остаётся NOTFOUND) — путь в доке и скриптах абсолютный. Телефон Android — `scripts/android_rumble.sh`,
  гейт §36.

## Neon Rumble на мобильной оболочке (спека #26, В1г)

- `rumble::MobileRumble : mobile::Game` (`games/neon-rumble/src/rumble_mobile.cpp`): `init` — бандлы
  через `mobile::asset_path`, титры, меню, стек сцен, сразу `enter` в уровень 1, хотсит; квады и
  текстуры сцены — от attach до detach (`bind_scene` из `rumble_gpu_frame`, общий с окном
  десктопа). `resize` с живыми целями только переконфигурирует поверхность.
- Демо-ввод — `rumble_demo.{hpp,cpp}` (`demo_p1`, `demo_p2`), общий с `--headless`; порядок шага
  тот же, что у `run_headless`, поэтому хеш драки на тике N совпадает с десктопным
  `--headless --frames N` — так `scripts/ios_sim_rumble.sh` и судит телефон.
- Шаг по `dt`: тик 1/60 с, аккумулятор, не больше 4 тиков за кадр (долгий кадр возврата из фона
  отбрасывается). У шутера долг тот же — он шагает кадром.
- Десктопные части игры — только в ветке не-мобильной сборки CMake: `main.cpp`,
  `rumble_window.cpp`, `rumble_keys.cpp`, `rumble_pad_report.cpp` (`like-nes::window` в мобильном
  префиксе нет). Признак — `CMAKE_SYSTEM_NAME` iOS/Android.
- `scripts/ios_sim_rumble.sh` требует `build-sdk-work/` от `check_sdk_game.sh --keep`: десктопный
  префикс даёт `assetc` (сборки сверяет Config), десктопная игра — хеш для сверки. iOS-префикс
  ставится в пустой временный каталог; общий разбор строк attach/detach — `ios_resumed` в
  `scripts/ios_sim_lib.sh` (его же зовёт `ios_sim_run.sh`).
