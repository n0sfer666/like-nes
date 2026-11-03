<!-- en-sha256: 9518654a56b83e051bc50104f2ba31b590f25c5b82f72b46e5bb82b70d974167 -->

# SDK и своя игра

[English](../../en/guide/sdk-and-your-game.md) · Русский

Игра не живёт внутри дерева движка. Это отдельный CMake-проект: он находит **поставленный SDK**
через `find_package`, линкует две цели и во время сборки пропекает своё содержимое в один
`game.bundle`. Эта страница ведёт от пустого префикса до запущенной игры и всюду опирается на
`games/neon-rumble/`. Это настоящая игра: CI собирает её против поставленного SDK на всех трёх
операционных системах, поэтому файлы ниже — её собственные, а не копии.

## Поставить SDK в префикс

SDK — это компонент установки `sdk`: публичные заголовки, статические библиотеки движка в Release
**и** Debug, рантайм wgpu, `library.bundle` с материалами, CMake-пакет и пекарь `assetc`. Обе
конфигурации ставятся в **один** префикс. Запускать из корня клона движка, после того как
выполнены требования [сборки](../getting-started/build.md):

```sh
cmake -S . -B build-sdk-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF -DIDE_POC=OFF
cmake --build build-sdk-release --target like_nes_sdk assetc
cmake --install build-sdk-release --config Release --component sdk --prefix ../like-nes-sdk
cmake -S . -B build-sdk-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF -DIDE_POC=OFF
cmake --build build-sdk-debug --target like_nes_sdk
cmake --install build-sdk-debug --config Debug --component sdk --prefix ../like-nes-sdk
```

На Windows эти команды запускаются из окна `scripts\win-dev.bat shell`, где доступен `cl.exe`.
Заголовки, которые игре разрешено включать, перечислены поимённо в `cmake/sdk_headers.cmake`.
Заголовок движка, которого в этом списке нет, — внутренний, и игра, включившая его, против префикса
не скомпилируется. Так задумано.

`bash scripts/check_sdk_game.sh --keep` выполняет те же шесть шагов, собирает Neon Rumble против
результата и оставляет всё в `build-sdk-work/`. Это кратчайший путь к префиксу, которому можно
доверять.

### Мобильный префикс

Сборке для телефона нужен свой префикс на платформу: `ios-sim`, `ios-device` или `android-<abi>`.
Он собирается только в Release, и `assetc` в нём нет: пекарь работает на машине сборки. Игра
передаёт десктопный префикс как `LIKE_NES_HOST_PREFIX`, и бандлы печёт его `assetc`. Neon Rumble
печёт своим инструментом `rumble_assetc` (см. ниже), а сборка для телефона не может запустить то,
что собирает под телефон: `bash scripts/rumble_tool.sh <desktop prefix> <dir>` собирает инструмент
для машины сборки против десктопного префикса и печатает его путь, а игра принимает этот путь как
`-DRUMBLE_ASSETC`. Без него телефонная сборка Neon Rumble останавливается на конфигурировании с этой
командой. Пути ниже
должны быть абсолютными: кросс-тулчейн не раскрывает относительный `like-nes_DIR`. Для
iOS-симулятора:

```sh
cmake -S . -B build-sdk-ios-sim -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-sdk-ios-sim --target like_nes_sdk
cmake --install build-sdk-ios-sim --component sdk --prefix <ios prefix>
bash scripts/rumble_tool.sh <desktop prefix> build-rumble-tool
cmake -S games/neon-rumble -B build-game-ios-sim -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 -Dlike-nes_DIR=<ios prefix>/lib/cmake/like-nes -DLIKE_NES_HOST_PREFIX=<desktop prefix> -DRUMBLE_ASSETC=<the path it printed>
cmake --build build-game-ios-sim
```

Для Android SDK и игра конфигурируются файлом тулчейна NDK 28.2.13676358 —
`-DCMAKE_TOOLCHAIN_FILE=<NDK>/build/cmake/android.toolchain.cmake -DANDROID_ABI=x86_64
-DANDROID_PLATFORM=android-29` вместо флагов iOS. Тогда сборка игры ещё и упаковывает
`neon_rumble.apk`. Префикс помнит, под что он собран: игру с другим минимальным iOS, другим уровнем
API или другой ревизией NDK `find_package` отклоняет и называет нужное значение.
`check_sdk_game.sh` собирает оба префикса после десктопной половины (iOS — на macOS, Android — на
macOS и Linux с пиннутым NDK). Он проверяет, что каждый пакет несёт бинарь своей платформы, оба
бандла и лицензии и что его `game.bundle` побайтно равен десктопному.

## CMakeLists.txt игры

Это весь файл сборки Neon Rumble:

Общий список исходников — в [`sources.cmake`](../../../games/neon-rumble/sources.cmake).
Мобильное приложение и его CPU-тест с моком хоста собирают один игровой код по этому списку.

<!-- snippet: games/neon-rumble/CMakeLists.txt -->
```cmake
cmake_minimum_required(VERSION 3.21)
project(neon_rumble CXX)

find_package(like-nes 0.1 REQUIRED CONFIG)
add_subdirectory(sim)

include(${CMAKE_CURRENT_LIST_DIR}/sources.cmake)
if(CMAKE_SYSTEM_NAME MATCHES "^(iOS|Android)$")
  like_nes_mobile_app(neon_rumble SOURCES ${RUMBLE_MOBILE_SOURCES} ${RUMBLE_SOURCES}
    NAME "Neon Rumble" ID com.likenes.neonrumble)
  target_link_libraries(neon_rumble PRIVATE rumble_sim like-nes::engine)
else()
  add_executable(neon_rumble src/main.cpp src/rumble_window.cpp src/rumble_keys.cpp src/rumble_pad_report.cpp
    src/rumble_window_script.cpp src/rumble_shot.cpp
    ${RUMBLE_SOURCES})
  target_link_libraries(neon_rumble PRIVATE rumble_sim like-nes::engine like-nes::window)
  like_nes_add_game(neon_rumble)
endif()
# Бойцов печёт инструмент игры (спека #29, В2а). Мобильная сборка его не запускает — бинарь хоста
# приходит готовым путём.
if(CMAKE_SYSTEM_NAME MATCHES "^(iOS|Android)$")
  if(NOT RUMBLE_ASSETC)
    message(FATAL_ERROR "neon_rumble: a mobile build bakes with the host's rumble_assetc; build it with "
                        "'bash scripts/rumble_tool.sh <desktop-prefix> <dir>' and pass -DRUMBLE_ASSETC=<path it prints>")
  endif()
  like_nes_bake(neon_rumble MANIFEST game.manifest OUT game.bundle TOOL "${RUMBLE_ASSETC}")
else()
  add_subdirectory(tools)
  like_nes_bake(neon_rumble MANIFEST game.manifest OUT game.bundle TOOL rumble_assetc)
endif()
```
<!-- /snippet -->

- `find_package(like-nes 0.1 REQUIRED CONFIG)` принимает любой 0.1.x. Пока мажорная версия 0, API
  может ломаться между минорными, поэтому 0.2 — другой пакет.
- `like-nes::engine` приносит фреймворк, рендерер и платформенный слой. `like-nes::window` —
  настольное окно и нативный ввод: GLFW-пумп клавиатуры и мыши и бэкенд пада ОС
  (`input/source.hpp`, `make_gamepad_source`): GameController на macOS, XInput на Windows, evdev на
  Linux.
  В `like-nes::engine` также входит `framework_touch`: раскладки и `TouchSource` переводят
  координаты экрана в ту же карту действий, что у остальных устройств ввода.
- `add_subdirectory(sim)` добавляет `rumble_sim` — код драки без окна. Его файл сборки линкует
  `like-nes::engine`, если такая цель есть, как здесь, а иначе — цели движка из дерева исходников:
  одна библиотека собирается и в дереве движка под его строгими предупреждениями, и против SDK для
  игры. Статический C-рантайм она ставит себе сама, как `like_nes_add_game` — игре.
- `rumble_sim` также подключает общий кит платформера из `games/kits/platformer/` как
  `kit_platformer`. Кит владеет контроллером и профилем движения и собирается против SDK;
  SDK предоставляет collision, slide, support, push и состояние персонажа. Исходники кита
  поставляются вместе с исходниками игры, вне установленного префикса.
- `add_subdirectory(tools)` добавляет `rumble_assetc`: ядро пекаря из SDK `like-nes::assetc_core` и
  собственную запись игры `fighter` (`tools/fighter_kind.cpp`), которой `assetc` из SDK не знает.
  Вид записи — `asset::manifest::Kind` из `assetc_kind.hpp`: имя, расширение файла и функция,
  превращающая текст файла в байты таблицы; манифест читает, путь проверяет и секцию пишет ядро.
  `main` инструмента отдаёт свои виды в `asset::manifest::run_manifest` из
  `assetc_manifest_mode.hpp`.
  Инструмент линкует только `like-nes::assetc_core` и игровую `rumble_fighter`, а пути
  заголовков берёт у `like-nes::engine`, поэтому рендер не тянет.
- На iOS и Android `like_nes_mobile_app(<цель> SOURCES <файл>... [BUNDLES <файл>...] NAME <имя> ID <bundle id>)`
  собирает игру на мобильной оболочке SDK вместо исполняемого файла: `.app` на iOS, `.so` и цель
  `<цель>_apk` на Android. Игра определяет `mobile::make_game()` (`mobile/mobile_app.hpp`); у Neon
  Rumble это `src/rumble_mobile.cpp`. `like-nes::window` есть только на десктопе, поэтому `main.cpp`,
  `rumble_window.cpp`, `rumble_keys.cpp` и `rumble_pad_report.cpp` остаются в десктопной ветке.
- `like_nes_add_game(<цель>)` заставляет игру использовать тот же C-рантайм, что и движок. На MSVC
  движок линкует CRT статически, и Debug-игра без этого вызова не слинкуется с `LNK2038`. Вызов
  также копирует рантайм wgpu рядом с исполняемым файлом. На MSVC Debug против префикса только с
  Release он останавливает конфигурирование через `like_nes_require_debug_libs(<цель>)`; её зовёт и
  инструмент игры, собранный против префикса.
- `like_nes_bake(<цель> MANIFEST <файл> OUT <бандл> [TOOL <пекарь>])` запускает пекаря на манифесте
  и копирует рядом с исполняемым файлом бандл игры и `library.bundle` из SDK. Без `TOOL` пекарь —
  `assetc` из SDK. `TOOL` — цель сборки игры, которая собирается первой, или абсолютный путь к
  существующему исполняемому файлу; всё остальное останавливает конфигурирование. Рядом с путём
  нужен `<путь>.like-nes-build` со сборкой этого SDK: его пишет `rumble_assetc`, собранный против
  префикса, а инструмент другой сборки движка останавливает конфигурирование. Neon Rumble
  передаёт `rumble_assetc` на десктопе и `${RUMBLE_ASSETC}` на телефоне. Пекарь пишет
  depfile со всеми прочитанными файлами (манифест, каждый PNG, каждый `.tmj` и `.tsj`, на которые он
  ссылается), поэтому правка тайлсета перепекает бандл на следующей сборке. Release, RelWithDebInfo
  и MinSizeRel добавляют `--strict`: отсутствие перевода ru останавливает бейк; Debug считает откаты.

Сконфигурировать и собрать игру против префикса:

```sh
cmake -S games/neon-rumble -B build-game -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=../like-nes-sdk
cmake --build build-game
./build-game/neon_rumble --headless --frames 240
```

## Манифест

`game.manifest` перечисляет, что попадает в `game.bundle`, по записи на строку. Поля каждой строки
разделены `|`:

<!-- snippet: games/neon-rumble/game.manifest -->
```text
# What like_nes_bake bakes into game.bundle (spec #24, B4-B5): one record per line,
# texture|<name>|<pixel or hd>|<path from this directory>, level|<name>|tiled|<path>.tmj,
# clips|<name>|aseprite|<path>.json (spec #24, B6) over a sheet that is a texture record by path.
# A trailing |viewport on a level makes the bake prove its layers cover the viewport limit (B7).
# font|<name>|bitmask|<path>.json bakes a glyph atlas and the fonts section, credits|<name>|<path>.txt
# the credits section (B8b). fighter|<name>|<path>.fighter bakes a fighter table (spec #25, B2b).
# Global viewport settings: positive integer sides, standard values add no bundle section.
zone | 384 | 216
zone_limit | 576 | 312
touch | touch | layout | controls/touch.txt
texture | street_tiles | pixel | assets/warped-city/tileset.png
texture | sign_tiles | pixel | assets/warped-city/signs.png
texture | sky | pixel | assets/warped-city/sky.png
texture | far_city | pixel | assets/warped-city/buildings-bg.png
texture | near_city | pixel | assets/warped-city/near-buildings-bg.png
level | level1 | tiled | levels/level1.tmj | viewport
texture | rainbird_sheet | pixel | assets/puffolotti/rainbird.png
clips | rainbird | aseprite | assets/puffolotti/rainbird.json
texture | banderas_sheet | pixel | assets/puffolotti/banderas.png
clips | banderas | aseprite | assets/puffolotti/banderas.json
texture | adler_sheet | pixel | assets/puffolotti/adler.png
clips | adler | aseprite | assets/puffolotti/adler.json
font | monogram | bitmask | assets/monogram/monogram-bitmap.json
credits | credits | assets/credits.txt
fighter | banderas_fighter | fighters/banderas.fighter
fighter | rainbird_fighter | fighters/rainbird.fighter
fighter | adler_fighter | fighters/adler.fighter
```
<!-- /snippet -->

| Запись | Поля | Что пропекает |
|---|---|---|
| `texture` | имя, `pixel` или `hd`, путь к PNG | одну текстуру. `pixel` — RGBA8 с nearest-выборкой и без мипмапов. `hd` требует `basisu`, а SDK его не поставляет |
| `level` | имя, `tiled`, путь к `.tmj`, необязательно `viewport` | карту Tiled: см. [Уровни в Tiled](tiled-levels.md) |
| `clips` | имя, `aseprite` или `sheet`, путь | клипы анимации: см. [Анимации в Aseprite](aseprite-animations.md) |
| `font` | имя, `bitmask`, путь к `.json` | растровый шрифт и его атлас: см. [Шрифты и титры](fonts-and-credits.md) |
| `strings` | имя, `tsv`, путь к `.tsv`, имя шрифта | таблицу переводов LNST; шрифт можно объявить ниже |
| `credits` | имя, путь к `.txt` | таблицу титров: см. [Лицензии ассетов](asset-licenses.md) |
| `fighter` | имя, путь к `.fighter` | таблицу бойца: скорости, прыжок, глубину, hp и удары по клипам бойца. Запись инструмента игры `rumble_assetc`: `assetc` из SDK её отклоняет |

Запись строк — `strings|labels|tsv|labels.tsv|monogram`. Literal UTF-8 TSV начинается с точных
колонок `key`, `en`, `ru`, разделённых табами; каждая строка данных содержит три поля. Пробелы
сохраняются, кавычки и escape-последовательности не интерпретируются. Пустой ru использует en и
добавляет один откат в `[assetc] strings labels: keys=N fallbacks=M`. `--strict` отказывает при
любом откате; явно заданный ru, совпадающий с en, считается полным переводом. Каждый кодпоинт
обоих значений должен быть в названном шрифте, включая алфавиты под ключами `grid.ip`, `grid.name`,
`grid.room`. Отказ называет ключ и `U+XXXX`. Ключ — `[A-Za-z0-9_.-]`, до 128 байт; дубликаты и
коллизии FNV ID отказываются. В таблице до 4096 ключей; исходный TSV и готовая таблица — до 16 MiB каждый.

В рантайме откройте именованную raw-секцию через `framework::ui::Strings` из `strings.hpp`, затем
вызывайте `find(key, Language::En)` или `Language::Ru`; отсутствующий ключ возвращает пустой view.
`font_name()` указывает связанный шрифт. Выровненный исходный буфер должен жить и оставаться
неизменным: reader и возвращаемые view ссылаются на него. Open и lookup не выделяют память в куче.

Бейк отказывает строке, а не угадывает, что имелось в виду:

- Имя — `[A-Za-z0-9_.-]`, уникальное в манифесте. `tilemap`, `visual`, `objects`, `clips` и
  `fonts` зарезервированы: эти имена бейк даёт своим таблицам.
- Путь относителен манифесту и пишется через `/`. `..`, `\`, `:`, ведущий `/` и пустой или `.`
  сегмент отбиваются. Каждый сегмент обязан совпадать с листингом каталога **по регистру**, на
  любой ОС. `Tiles/`, написанный вместо `tiles/`, работал бы на macOS и падал бы у игрока на Linux.
- Текстуре нужен кодек. Пропущенный кодек — отказ, а не молчаливое `hd`.
- Сторона текстуры — не больше 2048 px: безопасный предел wgpu на мобильных GPU.
- `#` начинает комментарий в любом месте строки, а UTF-8 BOM отбивается.

Каждый отказ называет `<манифест>:<строка>` и останавливает сборку игры на этой строке.

## Хеш бандла

Бейк детерминирован: один и тот же вход даёт одни и те же байты на любой ОС. Пекарь печатает
`[assetc] bundle_hash = 0x…`, когда пропекает, и Neon Rumble коммитит ожидаемое значение в
`games/neon-rumble/bundle.hash`. `check_sdk_game.sh` сравнивает их в Release и Debug. Другой хеш на
одной машине — находка, а не повод переписать файл. Файл меняется только в коммите, который меняет
содержимое или пекаря.

## Запуск игры

`--headless --frames 240` отображает бандл, открывает каждую таблицу и печатает сводку прочитанного.
Этот прогон и проверяет CI. Без `--headless` игра открывает окно 960×540, размер которого можно
менять, на меню: **↑** и **↓** выбирают `level`, `wave` или `credits`, **Enter** входит, **Esc**
возвращает из уровня в меню. `--level <имя>` задаёт уровень меню, `--wave` выделяет пункт wave, а
прогон headless входит в выбранный пункт без меню
(см. [Сцены и уровни](scenes-and-levels.md)). Banderas — игрок 1: WASD водят его, Space или K — прыжок, J — jab, U — cross, L —
kick, J в прыжке — jump_kick. Rainbird — игрок 2, он выходит на улицу по любой кнопке свободного
пада или по клавише своей раскладки: стрелка, нампад 0–6 или минус нампада. Backspace снимает игрока 1 с клавиатуры,
минус нампада — игрока 2, Back на паде — его игрока. **F3** переключает отладочный оверлей бойцов, **F1** открывает экран титров и
листает его страницы, нажатие после последней закрывает его. `--frames <n>` закрывает
окно через `n` кадров с кодом выхода 0. `--headless` требует `--frames` с `n` больше 0, поэтому
безоконный прогон всегда заканчивается.

Мобильная сборка по умолчанию управляется тачем на тренировочной арене карты уровня 1.
`--wave` включает существующее прохождение, `--demo` явно выбирает скриптовый ввод для сверки.
Новый интерактивный запуск, пять кнопок боя и физическая проверка
раскладки — в [Тач на телефоне](mobile-touch.md).

## Сейвы

`framework_save` поставляет `save.hpp`. До глобальных `load`/`store` вызовите `save::init`
с каноническим ID приложения (синтаксис как у имён слотов) на игровом потоке. Явный каталог оболочки приоритетнее, включая пустой
отказ. Иначе непустой абсолютный `LIKENES_SAVE_DIR` переопределяет системный каталог десктопа.
Недоступный или относительный override отказывает записи: перехода к каталогу exe или cwd нет.
Следующая запись повторяет подготовку того же каталога. `save::dir` отдаёт активный корень;
`save::file_path` подготавливает его и проверяет имя совместимого файла. Форматы ACH и controls
сохранены.

Имена слотов — строчные ASCII-буквы, цифры, `_` и `-`, до63 байт, без DOS-имён. Сериализуйте
операции над слотом и его резервными копиями. Тело LNSV ограничено16 MiB. Для `load` нужна
полная цепочка callbacks от версии1 до запрошенной: по одному на каждый переход, без исключений.
Перед миграцией оригинал сохраняется в `.pre`, будущая версия — в `.bak`, битая — в `.bad`;
свободный суффикс выбирается без перезаписи существующего backup. Глобальный overload возвращает
абсолютный путь карантина. Неверные аргументы и ошибки IO отличаются от повреждения данных.
Новая запись продолжает наибольший читаемый счётчик слота и его backup; переполнение отказывает
записи. `save_backend.hpp` также даёт `select_record` для выбора копии и `LocalBackend` для
overloads с явным backend. Равные счётчики разных валидных образов дают `Conflict`, обе копии
остаются целыми.
