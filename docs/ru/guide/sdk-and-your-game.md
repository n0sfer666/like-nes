<!-- en-sha256: 0d2c757fbe28775bc418ead6de7d900e099dc4a5c0f60c7a260e8104427b69c8 -->

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
Заголовки, которые игре разрешено включать, перечислены поимённо в `cmake/install_sdk.cmake`.
Заголовок движка, которого в этом списке нет, — внутренний, и игра, включившая его, против префикса
не скомпилируется. Так задумано.

`bash scripts/check_sdk_game.sh --keep` выполняет те же шесть шагов, собирает Neon Rumble против
результата и оставляет всё в `build-sdk-work/`. Это кратчайший путь к префиксу, которому можно
доверять.

## CMakeLists.txt игры

Это весь файл сборки Neon Rumble:

<!-- snippet: games/neon-rumble/CMakeLists.txt -->
```cmake
cmake_minimum_required(VERSION 3.21)
project(neon_rumble CXX)

find_package(like-nes 0.1 REQUIRED CONFIG)

add_executable(neon_rumble src/main.cpp src/rumble.cpp src/rumble_level.cpp src/rumble_layers.cpp
  src/rumble_window.cpp src/rumble_fighter.cpp src/rumble_fighter_quads.cpp
  src/rumble_credits.cpp)
target_link_libraries(neon_rumble PRIVATE like-nes::engine like-nes::window)
like_nes_add_game(neon_rumble)
like_nes_bake(neon_rumble MANIFEST game.manifest OUT game.bundle)
```
<!-- /snippet -->

- `find_package(like-nes 0.1 REQUIRED CONFIG)` принимает любой 0.1.x. Пока мажорная версия 0, API
  может ломаться между минорными, поэтому 0.2 — другой пакет.
- `like-nes::engine` приносит фреймворк, рендерер и платформенный слой. `like-nes::window` —
  настольное окно.
- `like_nes_add_game(<цель>)` заставляет игру использовать тот же C-рантайм, что и движок. На MSVC
  движок линкует CRT статически, и Debug-игра без этого вызова не слинкуется с `LNK2038`. Вызов
  также копирует рантайм wgpu рядом с исполняемым файлом.
- `like_nes_bake(<цель> MANIFEST <файл> OUT <бандл>)` запускает `assetc` из SDK на манифесте и
  копирует рядом с исполняемым файлом бандл игры и `library.bundle` из SDK. `assetc` пишет
  depfile со всеми прочитанными файлами (манифест, каждый PNG, каждый `.tmj` и `.tsj`, на которые он
  ссылается), поэтому правка тайлсета перепекает бандл на следующей сборке.

Сконфигурировать и собрать игру против префикса:

```sh
cmake -S games/neon-rumble -B build-game -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=../like-nes-sdk
cmake --build build-game
./build-game/neon_rumble --headless --frames 60
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
# the credits section (B8b).
texture | street_tiles | pixel | assets/warped-city/tileset.png
texture | sign_tiles | pixel | assets/warped-city/signs.png
texture | sky | pixel | assets/warped-city/sky.png
texture | far_city | pixel | assets/warped-city/buildings-bg.png
texture | near_city | pixel | assets/warped-city/near-buildings-bg.png
level | level1 | tiled | levels/level1.tmj | viewport
texture | queen_sheet | pixel | assets/chewbatrij/queen-rows.png
clips | queen | aseprite | assets/chewbatrij/queen-rows.json
font | monogram | bitmask | assets/monogram/monogram-bitmap.json
credits | credits | assets/credits.txt
```
<!-- /snippet -->

| Запись | Поля | Что пропекает |
|---|---|---|
| `texture` | имя, `pixel` или `hd`, путь к PNG | одну текстуру. `pixel` — RGBA8 с nearest-выборкой и без мипмапов. `hd` требует `basisu`, а SDK его не поставляет |
| `level` | имя, `tiled`, путь к `.tmj`, необязательно `viewport` | карту Tiled: см. [Уровни в Tiled](tiled-levels.md) |
| `clips` | имя, `aseprite` или `sheet`, путь | клипы анимации: см. [Анимации в Aseprite](aseprite-animations.md) |
| `font` | имя, `bitmask`, путь к `.json` | растровый шрифт и его атлас: см. [Шрифты и титры](fonts-and-credits.md) |
| `credits` | имя, путь к `.txt` | таблицу титров: см. [Лицензии ассетов](asset-licenses.md) |

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

Бейк детерминирован: один и тот же вход даёт одни и те же байты на любой ОС. `assetc` печатает
`[assetc] bundle_hash = 0x…`, когда пропекает, и Neon Rumble коммитит ожидаемое значение в
`games/neon-rumble/bundle.hash`. `check_sdk_game.sh` сравнивает их в Release и Debug. Другой хеш на
одной машине — находка, а не повод переписать файл. Файл меняется только в коммите, который меняет
содержимое или пекаря.

## Запуск игры

`--headless --frames 60` отображает бандл, открывает каждую таблицу и печатает сводку прочитанного.
Этот прогон и проверяет CI. Без `--headless` игра открывает окно 960×540, размер которого можно
менять. **F3** переключает отладочный оверлей бойца, **F1** — экран титров. `--frames <n>` закрывает
окно через `n` кадров с кодом выхода 0. `--headless` требует `--frames` с `n` больше 0, поэтому
безоконный прогон всегда заканчивается.
