# Игра против поставленного SDK (спека #24, В1, В4, В5а, В5б)

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
   `neon-rumble: headless run ok, 60 frames` и `neon-rumble: library.bundle <N> bytes`: бандлы рядом
   с exe кладёт `like_nes_bake`, рантайм wgpu — `like_nes_add_game`. С В5б ещё две строки сводки:
   `level level1 40x12 tile 16, 2 visual layer(s), 1 texture(s) 384x256` — игра открыла
   `game.bundle` маппингом, прочитала таблицу `visual` (`raw_table`) и тайлсет сырым RGBA8
   (`raw_rgba8`), — и `frame 960x540 zoom 2: <N> sprite(s), <M> run(s), 0 unknown, 0 rejected,
   0 dropped` — кадр окна прошёл `draw_layer` и `layer_quads` из поставленных заголовков и целиком лёг
   в квады. Числа спрайтов и прогонов не пинятся (их судит голден в дереве), только ненулевые; нули
   отказов пинятся: чужой guid текстуры, источник за краем текстуры или нехватка буферов — находка;
4. сверяет `bundle_hash` `game.bundle` рядом с exe (смещение 32, `od`) с закоммиченным
   `games/neon-rumble/bundle.hash` в обеих конфигурациях. Читается копия рядом с exe, а не выход
   `assetc`: грузит игра именно её. Другой хеш на одной ОС — находка. Законная смена — только
   коммитом, который намеренно меняет PNG, манифест или байты пекаря (`engine/asset/`,
   `tools/assetc/`, импорт Tiled `engine/framework/tilemap/`, `levels/*.tmj`/`*.tsj`): значение из
   строки `[assetc] bundle_hash` лога сборки, `bundle.hash` — в том же коммите, после зелёного гейта
   на трёх ОС. С В5а в бандле уровень `level1` (секции `tilemap`, `visual`, `objects`); пересохранение
   `level1.tmj` в настоящем Tiled хеш менять не должно — сценарий в `docs/owner-verification.md`.

`like_nes_bake` (В4) печёт `assetc --manifest … --depfile …` поставленным `assetc` в
`<build>/like_nes_bake/<цель>/`, а в каталог exe кладёт цель `<игра>_bundle` (всегда исполняемая,
`copy_if_different`). Подкаталог — не вкус: у Ninja он совпадал бы с каталогом exe, копия стала бы
копией файла в себя, и шаг, без которого у Visual Studio и Xcode бандла рядом с exe нет, гейт на
Ninja не судил бы (мутант «копии нет» выжил ровно так, 2026-10-02). PNG в DEPENDS нет — о нём сборка
знает только из depfile; `CMP0116 NEW` ставится в `like_nes_bake.cmake`.

Каталоги СВОИ, как у голдена Debug: одолженный `build-ci` поменял бы конфигурацию чужому этапу.
Оба конфигурируются из одного состояния дерева — последняя установка перетирает Config в префиксе
(так однажды и было: правленный `.in` поставился из Release, а Debug поставил старый поверх).

## Сломанные фикстуры (в каждом прогоне, [`scripts/sdk_game_lib.sh`](../../scripts/sdk_game_lib.sh))

| фикстура | обязана упасть | по причине |
|---|---|---|
| копия игры с `#include "renderer_internal.hpp"` | сборка | имя заголовка + `not found` / `No such file` / `Cannot open include file` |
| КОПИЯ префикса без `libframework_physics.a` (`framework_physics.lib`) | `find_package` | `missing library <файл>` из Config; лог сверяется со схлопнутыми пробелами — CMake переносит причину по ширине, и место переноса зависит от длины пути |
| только MSVC: копия без `like_nes_add_game`, Debug | компоновка | `LNK2038` (статический CRT движка против динамического по умолчанию) |
| `bundle.hash` с чужим значением против Release-сборки | сверка хеша | `bundle.hash says 0x0123456789abcdef` |
| копия игры, манифест `texture\|street_tiles\|<путь>` без кодека | сборка игры | `game.manifest:1: texture record needs a codec` (строка `assetc` в логе сборки) |
| копия игры: пересборка без правок; `ladder` → `solid` в `.tsj`; манифест без уровня; PNG подменён другим — между правками `sleep 2` | — | без правок нет строки `Baking game.bundle`; после каждой правки она есть, хеш рядом с exe после `.tsj` ≠ `bundle.hash`, после подмены PNG ≠ хешу до неё. Раунд PNG идёт без уровня: тайлсет сверяет размер картинки, и чужой PNG отбился бы отказом, а не перебейком. Отметка из будущего вместо `sleep` отравляла бы следующие раунды — файл «новее» навсегда |

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

Мутации `like_nes_bake` 2026-10-02 (macOS, префикс `--keep`): без `DEPFILE`, без копии бандла в
каталог exe, копия через `POST_BUILD` вместо отдельной цели — каждая валит свою фикстуру или сверку.
Последнюю ловит только подмена PNG ДРУГИМ файлом: после голого `touch` байты те же, и недоехавший
бандл неотличим от доехавшего.

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

- пиксели кадра игры: GPU-путь слоёв (`QuadRenderer`, флипы, анимация) судит в дереве
  `framework_layer_golden` против CPU-эталона на трёх ОС (шаг CI «Layers — tile layers on a real
  GPU»), а окно игры — владелец (`docs/owner-verification.md` §18, шаг 5);
- записи `level`, `clips`, `credits` — `assetc` их отбивает как `unsupported record kind` до В5–В8.
