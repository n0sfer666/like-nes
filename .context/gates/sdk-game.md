# Игра против поставленного SDK (спека #24, В1)

```
bash scripts/check_sdk_game.sh          # префикс и сборки игры во временном каталоге
bash scripts/check_sdk_game.sh --keep   # то же, префикс и игра остаются в build-sdk-work/
```

Предмет — **поставленный префикс**, а не дерево. Внутри дерева игре видны все заголовки и все цели,
и забытый в [`cmake/sdk_headers.cmake`](../../cmake/sdk_headers.cmake) заголовок или библиотека
проявились бы только у игрока. Гейт:

1. конфигурирует и собирает `build-sdk-release` (`like_nes_sdk` + `assetc`) и `build-sdk-debug`
   (`like_nes_sdk`) с `-DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF -DIDE_POC=OFF`;
2. ставит компонент `sdk` из обоих в ОДИН префикс (`lib/like-nes/` и `lib/like-nes/debug/`);
3. собирает `games/neon-rumble` через `find_package(like-nes 0.1 REQUIRED CONFIG)` в Release и
   Debug (`cmake --fresh`) и запускает `--headless --frames 60`. Утверждение — rc 0 **и** строки
   `neon-rumble: headless run ok, 60 frames` и `neon-rumble: library.bundle <N> bytes`: бандл рядом
   с exe кладёт `like_nes_bake` (заглушка В1), рантайм wgpu — `like_nes_add_game`.

Каталоги СВОИ, как у голдена Debug: одолженный `build-ci` поменял бы конфигурацию чужому этапу.
Оба конфигурируются из одного состояния дерева — последняя установка перетирает Config в префиксе
(так однажды и было: правленный `.in` поставился из Release, а Debug поставил старый поверх).

## Сломанные фикстуры (в каждом прогоне, [`scripts/sdk_game_lib.sh`](../../scripts/sdk_game_lib.sh))

| фикстура | обязана упасть | по причине |
|---|---|---|
| копия игры с `#include "renderer_internal.hpp"` | сборка | имя заголовка + `not found` / `No such file` / `Cannot open include file` |
| КОПИЯ префикса без `libframework_physics.a` (`framework_physics.lib`) | `find_package` | `missing library <файл>` из Config; лог сверяется со схлопнутыми пробелами — CMake переносит причину по ширине, и место переноса зависит от длины пути |
| только MSVC: копия без `like_nes_add_game`, Debug | компоновка | `LNK2038` (статический CRT движка против динамического по умолчанию) |

Отказ без своей причины в логе — FAIL фикстуры: упало что-то другое, и она ничего не доказала.
Фикстура CRT вне MSVC печатает SKIP — у clang/gcc нет второго CRT по построению. MSVC судится по
`CMAKE_CXX_COMPILER_ID` сборки игры, а не по окружению: под vcvars с clang-cl LNK2038 не бывает.

## Отказы конфигурирования дерева (`cmake/install_sdk.cmake`, `cmake/sdk_links.cmake`)

Идут в КАЖДОМ конфигурировании, не только в гейте:

- `#include "x"` поставленного заголовка — только поставленный; `<x>` — поставленный сторонний или
  стандартный C++ (без точки и слэша);
- элемент ссылок — цель SDK, `Threads::Threads`, `-framework`, голое имя или системный путь;
  файл из дерева или его сборки — отказ;
- `INTERFACE_COMPILE_OPTIONS` / `LINK_OPTIONS` / `LINK_DIRECTORIES` цели SDK — отказ; каталог
  include — только поставленный (плюс stb: его не подключает ни один поставленный заголовок);
- макрос с `$<`, `]==]` или путём дерева — отказ; значения пишутся в Config bracket-аргументом.

Мутации 2026-10-01 (macOS): `<stb_image.h>` в `arena.hpp`, PUBLIC include `tools/` у
`framework_physics`, ссылка на `build-sdk-release/libfoo.a` — каждая валит конфигурирование своим
текстом.

## Где идёт

- preflight — этап группы `preflight_build_rules.sh`;
- CI — джоб `sdk-game` в `ci.yml` на трёх ОС.

Окно игры на живом железе (Wayland, драйвер) CI не открыть: `--keep` оставляет префикс для
оконного прогона владельцем — `docs/owner-setup.txt`, раздел S.

Имя заголовка игры против поставленного судит не этот гейт, а include-seam (правило 3,
`.context/gates/include-seam.md`): ему не нужна сборка, и он идёт на каждом коммите.

## Что ещё не судит (следующие вертикали #24)

- настоящий бейк `game.bundle` из манифеста — В4; сейчас `like_nes_bake` проверяет только, что
  манифест существует, и копирует `library.bundle`.
