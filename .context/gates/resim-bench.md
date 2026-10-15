# Бенч ресима на бюджете зонтика — `brawl_resim_bench`

```
./build/brawl_resim_bench [--against <файл crossplay>]   # CI × 3 ОС, после шага crossplay
bash scripts/mobile_bench.sh ios-sim|ios-device|android  # телефон, ручной гейт §34 владельца
```

Спека #25, В9в (2026-10-14). Тело — `framework::ai::bench::run_resim_bench(against)` в
`engine/framework/ai/resim_bench.cpp` (библиотека `resim_bench`), драка —
`framework::ai::bench::Fight` в `framework_ai_bench_fight.hpp` (Sim для `rollback::Session`).
Десктопный вход — `brawl_resim_bench.cpp`: только разбор `--against`.

Драка: 2 игрока (`scripted`), 8 врагов с мозгом `APPROACH`, 4 ящика (hp 30) и 12 снарядов
(жизнь 24 тика) — ровно `POOL_CAPACITY` 26. Бойцы не умирают (`TOUGH`): мозг снимается только
внутри `step_ai`, и замена павшего в том же тике упёрлась бы в `BRAINS_MAX`. Ящики и снаряды
добираются до бюджета в начале тика (`restock`). Ввод P2 приезжает на 8 тиков позже, а его
`move_x` меняется каждый тик, поэтому каждый из 600 кадров — откат на 8 тиков и переигрывание.

## Что судит

PASS — детерминизм и полнота бюджета, **не время**: p50/p99 и доля кадра 16.6 мс только
печатаются. Цифра раннера описывает раннер; решение о глубине отката и бюджете врагов даёт самый
слабый телефон (В9г, ручной гейт).

- голден драки `resim bench fight hash = 0x0f60b3fbe989d27e` — в шаге CI, в Debug-шаге и в
  `scripts/check_debug_golden.sh` (`STATE_TARGETS`, ci-lint `mirrors-var`);
- ресим через `Session` кончается там же, где прямой прогон;
- `rollbacks == 600`, `replayed == 4800`, `too_deep == 0`;
- минимум по ролям за тик равен бюджету (2/8/4/12);
- `hits_dropped == 0`; контроль — попадания, сломанные ящики и выпущенные снаряды больше нуля;
- `--against`: текст сценариев кроссплея (`имя 0x<16hex>\n`) побайтово равен файлу
  `framework_brawl_crossplay_test --crossplay` той же ОС. Оба пишут/читают в двоичном режиме — CRLF
  на Windows не возникает.

## Позитивный контроль (2026-10-14)

Мутанты `framework_ai_bench_fight.hpp`, каждый падает своей проверкой: ввод P2 постоянный →
«rolled back 8 ticks»; без `restock` → «full umbrella budget» и контроль; `restore` без `ai` или
без `tick` → «resimulated … ends where the straight one does». `--against` с изменённым хешем,
с CRLF и с отсутствующим файлом → FAIL; без пути → код 2.

## Замер

macOS M-серии, Release: снапшот 5904 B, кадр p50 ≈ 43 us (0.25 % кадра), p99 ≈ 49 us; за 600
кадров попаданий 2210, пик 9 из 64 за тик.

## Мобильный вход (В9г, 2026-10-14)

Тот же `run_resim_bench` — второй вход iOS- и Android-оболочки: опция CMake
`LIKE_NES_MOBILE_BENCH=ON` (`platform/mobile/resim_bench.cmake`) собирает вместо игры приложение
`com.likenes.resimbench` без UI (чёрный экран), каталоги `build-ios-bench`, `build-ios-bench-device`,
`build-android-bench`. Файл, против которого бенч судит хеши, — `LIKE_NES_BENCH_AGAINST`: CMake
копирует его в `<build>/resim_bench/crossplay.txt`, а оттуда — в `assets/crossplay.txt` бандла
(iOS) или APK (Android распаковывает его `unpack_apk_asset` во `internalDataPath/bench`). Опция без
файла — `FATAL_ERROR` при конфигурировании: бенч на телефоне без сверки хешей ответил бы только
на половину гейта.

- iOS: `platform/mobile/bench_game.cpp` поверх оболочки `like_nes_mobile` — строка
  `[bench] iOS <версия>, <машина>`, бенч в своём потоке с `QOS_CLASS_USER_INTERACTIVE`, stdout построчный → консоль Xcode
  (`simctl launch --stdout` в симуляторе). Цель `like_nes_resim_bench`, бандл собирает та же
  `like_nes_mobile_app`, что и игру (`cmake/like_nes_mobile_app.cmake`, В1в спеки #26). С В1б спеки #26 бенч стоит на общей оболочке и тянет wgpu-native, как
  игра: GPU и окно поднимаются и для него (`[gpu]` — первая строка вывода), а кадров он не просит
  (`wants_frames() == false`). Решение, а не долг: бенч судит оболочку, на которой поедет игра.
- Android: `platform/mobile/bench_game.cpp` поверх оболочки `like_nes_mobile` (`libbench.so`, манифест — общий шаблон
  `AndroidManifest.xml.in` с id `com.likenes.resimbench`)
  — строка `[bench] Android <версия> (API n), <производитель> <модель>, soc <soc>`, бенч в своём
  потоке, stdout → logcat тегом `like-nes`. APK — `build_apk.sh --bench <файл>`.
- `scripts/mobile_bench.sh` без `--against` берёт `crossplay-macOS` последнего зелёного прогона
  `ci.yml` для HEAD (`gh run download`) в `build-mobile-bench/<sha>/`; прогона нет — FAIL с советом
  `ci_watch.sh`. Файл macOS, а не любой: кроссплей-джоб уже утверждает равенство трёх ОС, и одного
  достаточно. `ios-sim` и `android` ждут вердикт до 180 с и судят `brawl-resim-bench: PASS`;
  `ios-device` открывает проект Xcode (подпись — команда владельца, `--team` или вкладка Signing).

Проверено агентом 2026-10-14: сборка под симулятор (Ninja) и под iPhone (`xcodebuild … -sdk
iphoneos CODE_SIGNING_ALLOWED=NO`) без предупреждений, в бандле `assets/crossplay.txt` побайтово
равен файлу CI 38485741863, plist `com.likenes.resimbench` / `SceneDelegate`; APK
`build_apk.sh --bench` без предупреждений компилятора, `libbench.so` экспортирует
`ANativeActivity_onCreate`, libc++ статический, `assets/crossplay.txt` в APK; игровой APK
собирается как прежде; опция без файла — `FATAL_ERROR` своей строкой.

Прогнано агентом 2026-10-14 в симуляторе iPhone 16 / iOS 18.6 (22G86), HEAD `b887502`, файл CI
38557291268: `bash scripts/mobile_bench.sh ios-sim` → 14 сценариев `match`, p50 42.1 мкс, p99
46.8 мкс, `[mobile-bench] PASS`. Мутант: `--against` с `run-slide …66` → `…67` — `DIFFER`,
`brawl-resim-bench: FAIL`, `[mobile-bench] FAIL`. Рантайм 18.6 взят из бандла MobileAsset на диске
(`xcodebuild -importPlatform <…/AssetData/*.dmg>`; `simctl runtime add` на нём падает — бандл
только для чтения). Позже докачан iOS 27.0 (24A434): тот же прогон против CI 38570513796 (`0edac74`) —
`match`, p50 43.0 мкс, p99 80.5 мкс, PASS.

Риск устаревания: `xcompile_verify.sh` и CI собирают оболочки только с игрой; мобильный вход
бенча не собирает никто, кроме `mobile_bench.sh`. Тронута сигнатура `run_resim_bench`,
`apk_assets` или `platform/ios/CMakeLists.txt` — прогнать `mobile_bench.sh ios-sim`.

Темп: бенч гоняет 600 кадров подряд без пауз, а игра на 60 Гц простаивает между кадрами, поэтому
DVFS телефона может дать бенчу частоту выше игровой. Если p99 близок к решению — вариант с паузой
до 16.6 мс между кадрами (не сделан, решение владельца).
