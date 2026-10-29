# Доводка движка (сессия с 2026-10-28)

Запрос владельца: закончить работу над движком. Проверять только то, что проверяется на macOS;
остальное — долг. Любое решение, даже тривиальное, принимает владелец через интервью.

## Решения владельца (интервью 2026-10-28)

1. Скоуп: #29 В2 (разбор жанра из SDK), #26 В2г–В2е и В3–В7, #27 (сеть на двоих).
2. Порядок: #29 В2 → #26 В2г–В2е → #26 В3–В7 → #27.
3. Долг — файл на спеку в `~/wiki/dev-log/like-nes/checks/<спека>.md`.
4. Push в `dev` после проверенного коммита, CI смотреть (`ci_watch.sh`) и чинить падения.
5. PR `dev` → `main` создаю и мержу сам при зелёном CI на 3 ОС, затем `dev` ← `main`. У #29 В2 —
   один PR на раунд В2а–В2е; у #26 — PR на каждую вертикаль, как в спеке.
6. Музыка: треки уходят в пилот #29 (пункт в «Дальше» спеки #29), шаг 0 снят с #26; В6 судится
   синтетическим PCM, трек под замер В7 — синтетический.
7. Повестка недели 20.10–26.10 пишется первой.
8. #18, #19, #20, #22 закрываются с долгом (ручные гейты → `checks/<спека>.md`). ADR 0018, 0019,
   0020 остаются Proposed; ADR 0022 по #22 написан при закрытии, Proposed.
9. #29 В2а, инструмент бейка: игровой вид записи манифеста — таблицей параметром
   (`std::span<const Kind>` в точку входа `assetc_core`); режимы `--game`/`--verify-game` — таблица
   секций параметром core, у базового `assetc` их нет, их зовёт `ugly_assetc`; инструменты живут в
   `tools/` своей игры (`games/neon-rumble/tools/` — подкаталог игры и проект хоста,
   `example_ugly_game/tools/`); `like_nes_bake(TOOL)` опционален, мобильная сборка rumble без
   `-DRUMBLE_ASSETC` — `FATAL_ERROR` с командой сборки инструмента хоста.
10. #29 В2а, детали (интервью 5–7): пекарь вида отдаёт ядру байты таблицы, `push_table` делает
    ядро, `AssetInput` в публичный заголовок не идёт; `like-nes::assetc_core` — отдельная цель
    desktop-префикса рядом с `like-nes::engine`; заголовок — `include/like-nes/assetc/`
    (`assetc_kind.hpp`); корень дерева подключает `games/neon-rumble/tools`, там же
    `assetc_fighter_test`; пекари секций `--game` — либа дерева `assetc_tables` (без movement),
    movement — в `example_ugly_game/tools/`; базовый `assetc` на `--game`/`--verify-game` — отказ с
    подсказкой про `ugly_assetc`; `kit_platformer` переносится в В2д; запись игрового вида —
    `<вид>|<имя>|<путь>.<ext>`, в отказе виды игры идут после встроенных; мобильные гейты собирают
    инструмент `scripts/rumble_tool.sh <desktop-prefix> <dir>`; путь `TOOL` — абсолютный и
    существующий; `sim-float` — `SIM_PATHS` (каталоги и файлы) + `games/neon-rumble/sim`, мутант в
    игровом каталоге, `REQUIRED_DIR` до В2г прежний.
11. #29 В2а (интервью 8): `rumble_assetc` против префикса линкует только `like-nes::assetc_core` и
    `like-nes::framework_brawl_fighter`, include-каталоги — из `like-nes::engine` (без webgpu);
    `assetc_kind.hpp` — `asset::manifest`, `Kind{name, extension ".fighter", bake}`;
    `example_ugly_game/tools` подключается из `example_ugly_game/CMakeLists.txt` до выхода по
    `IDE_POC`; usage `run_manifest` печатает имя из `argv[0]`.
12. #29 В2а (интервью 9, line-budget): `name_ok`/`path_ok` — публичные в `assetc_manifest.hpp`
    (живут в `assetc_manifest_paths.cpp`); бейк игрового вида — свой файл
    `manifest_kind_bake.{hpp,cpp}` (`TextRecord`, `bake_kind`); таблица видов игры остаётся
    параметром, но вид с именем встроенного и второй вид с тем же именем — отказ (`kinds_ok`).
13. #29 В2а (интервью 10, ревью): все находки — по ревьюеру; путь `TOOL` сверяется штампом
    `<bin>.like-nes-build` (`_ln_build` префикса, против которого собран инструмент) с
    `_ln_build` хост-префикса мобильной сборки; `report()` — определение в `assetc_core`;
    MSVC Debug против Release-only префикса — общая функция Config, её зовут `like_nes_add_game`
    и `rumble_assetc`.
14. #29 В2а (интервью 11, line-budget): тесты таблицы видов — свой бинарь `assetc_kinds_test`
    (`assetc_manifest_test` возвращается к грамматике, путям и depfile).
15. #29 В2а (интервью 12, повторное ревью): шесть механических low/taste — по ревьюеру; отказы
    `like_nes_bake(TOOL)` закрепляет мини-проект в desktop-половине `check_sdk_game.sh`, по фикстуре
    на каждый отказ; `rumble_tool.sh` собирается лениво, перед первой мобильной платформой;
    `run_manifest` — свой поставленный заголовок `assetc_manifest_mode.hpp`, `assetc_kind.hpp` — только `Kind`.
16. #29 В2а (интервью 13): `rumble_tool.sh` приводит префикс к абсолютному (относительный не
    находился: `like-nes_DIR` CMake считает от каталога сборки); провал ленивой сборки пекаря
    запоминается — вторая мобильная платформа падает сразу, без второй сборки и второй строки FAIL.
17. #29 В2а (интервью 14, ревью дельты): три low — по ревьюеру (`tr -s '[:space:]'` в фикстурах
    TOOL, `RUMBLE_TOOL`/`RUMBLE_TOOL_FAILED` обнуляет гейт, `CDPATH='' cd --` в `rumble_tool.sh`);
    `rumble_tool.sh` вне macOS/Linux — явный отказ; лог — `<dir>/rumble_tool.log`, а не `<dir>.log`
    в корне репозитория.
18. #29 В2б (интервью 15–16): пространства имён по каталогу (`framework::depth`/`hits`/`kinematic`,
    дополнения — в пространстве своего каталога); `brawl/` подключает ядра `using`-объявлениями на
    месте, старые заголовки `brawl/` удаляются без переходников; `MAX_WALLS`/`MAX_STRUCK`/`SEATS` —
    шаблон хранения с дефолтом, функции ядра нешаблонные на `std::span`, код в `.cpp`; цель на
    каталог (`framework_depth`/`hits`/`kinematic` в SDK), дополнения — в существующие цели;
    автоскролл берёт свою дорожку `graphics::ScrollLane`, игра строит её из `scene::Autoscroll`
    каждый тик; состояние ядра — вложенным членом (`StageCamera.autoscroll`, `RopeState.swing`,
    `Body.queued`) с прежним порядком полей; позитивный контроль — сломанная копия в самом тесте;
    коммит на группу ядер, `framework_brawl_floor_test` → `framework_depth_floor_test` с прежним
    вердиктом.
19. #29 В2б (интервью 17–18): алиасы шаблонов в игровых заголовках (`DepthFloor` — новый
    `brawl/brawl_floor.hpp`, `StruckList` — `brawl_body.hpp`, `SEATS = 2` и `Seats` — `seat_step.hpp`);
    пол — `FloorView{band, span walls}` в `.cpp` плюс inline-перегрузки на `DepthFloor<N>`;
    `ActionBuffer` — `clear_action`/`buffer_action`/`take_action(buf, use)`, `NO_ACTION = 0xffff`,
    `NO_STRIKE = input::NO_ACTION`; `hits::hitstop_tick(uint16_t&)`, `place_box(DepthBody, int8_t
    facing, Rect16)`, `physics::Pendulum{arc, speed}`, `rope_*` по спеке, `swing_pendulum(…, damping)`,
    `ROPE_PUMP` — игре; brawl берёт `kinematic::Movers` (`StageMovers` — алиас, `step_stage_movers` —
    игровая обёртка); автоскролл — `settle_autoscroll` + `ride_autoscroll`, `SETTLE_SHARE` — игре
    параметром; тесты дополнений — `framework_ids_test`, `framework_flick_test`,
    `framework_action_buffer_test`, `framework_physics_pendulum_test`,
    `framework_graphics_autoscroll_test`; CI — в те же списки, что соседи.
20. #29 В2б (интервью 19): `Seat`/`Seats` — в `core/seats.hpp`, `seats_hash` — в
    `physics/seat_hash` (core не видит `hash_mix.hpp`: слой core зависит только от `engine_core`).
21. #29 В2б (интервью 20): закрепление ГСЧ (`framework_ai_test`) и поля хеша мест
    (`framework_brawl_seat_test`) переезжают в `framework_ids_test`; там же `EntId ==`/`!=`,
    `seats_hash` на `Seats<3>` = хеш span и видит третье место; позитивный контроль — сломанные
    копии `seats_hash` (забыт `present`) и `next_u32`; вердикт `framework-ids`; цель — в `physics/`;
    параметр `Seats<N>` — `uint32_t`.
22. #29 В2б (интервью 21): ai подключает `core::EntId` using-объявлением в `namespace ai` (запись
    `brawl::EntId` в ai → `EntId`); `using core::WorldRng/next_u32/below` — в `ai_state.hpp`.
23. #29 В2б (интервью 22): `sim-float` — ядра, вынесенные из жанра в `core/` и `physics/`, входят в
    `SIM_PATHS` файлами поимённо; новые каталоги `depth`/`hits`/`kinematic` — целиком.
24. #29 В2б (интервью 23, ревью группы core): `sim-float` — строки selftest на файловую запись и
    отказ гейта на записи `SIM_PATHS` без файлов (удалённый каталог уходит из списка тем же
    коммитом); сломанная копия `Seats<3>` на двух местах; спека правится сразу; из taste — только
    порядок `sdk_headers`, лёгкий `brawl_seats.hpp` не делаем (решение 19 в силе).
25. Интервью 24 (группа input В2б): `dodge_flick` → `input::flick` с проверкой по `::input::MAX_ACTIONS`;
    чистые случаи → `framework_flick_test` (`framework-flick`) таблицей со сломанной копией без покоя, в
    brawl-тесте остаются Live/replay/roll; `strike_queue` → `input::ActionBuffer` (поле `action`),
    `NO_STRIKE`/`NO_CHAIN` — в `brawl_body.hpp`; тест буфера — три правила `take_action` и копия без
    сброса на нуле; `begin()` по-прежнему зовёт `clear_action`.
26. Интервью 25 (ревью input): ручные гейты owner-verification называют `input/flick.*` и
    `input/action_buffer.*` в строках, которые раньше ловил глоб `brawl/*`; в доке `framework::input::flick`;
    `FlickRule::block` → `hold`; нулевой срок `buffer_action` кладёт пустой буфер, `next_step` берёт
    действие параметром, `use` не кладёт в свой буфер; `framework-flick: PASS - 18 cases` пиннут в CI;
    двойной `clear_action` в `begin()` остаётся.
27. Интервью 26 (группа physics В2б): `physics::Pendulum{arc, speed}` внутри `RopeState{swing, rider,
    cooldown}`, порядок смешивания хеша прежний; функции `pendulum_turns/cap/end/tangent`,
    `swing_pendulum(Pendulum&, length, g, push, damping)`; `ROPE_*` остаются константами драки;
    `framework_pendulum_test` — энергия без ввода не растёт, размах зажимается, `end`/`tangent` в отвесе
    и на пределе, сломанная копия без затухания; `rope_energy` фикстуры драки остаётся.
28. Интервью 27 (ревью pendulum): условия `damping > 0` и `length` 1..5000 px — в `pendulum.hpp`,
    без проверки на горячем пути; у размаха и геометрии сломанные копии (без обнуления, без клампа,
    стоп ровно на пределе, `sin`/`cos` переставлены, касательная `+sin`), точное сравнение вместо
    допуска, случай «шаг ровно на предел»; owner-verification §13 называет `physics/pendulum.*`;
    теневая энергия и `EnergyGate` — общий `framework_physics_pendulum_energy.hpp`, `rope_energy`
    драки — обёртка над ним (отменяет хвост п. 27); цель — `framework_physics_pendulum_test`,
    вердикт `framework-physics-pendulum`; `rope_swing.hpp` включает `fixmath.hpp` сам.
29. Интервью 28 (группа depth В2б): `DepthIntent{mx, mz, hop}`, нулевое намерение (пад отсутствует)
    останавливает тело и не подбрасывает; лицо по знаку `mx` и бит прыжка — игре (`intent_of`,
    `step_body` в `brawl/depth_step`); ядро — `grounded`/`drive`/`fall`; `move` и `belt` — одна цель
    `framework_depth` с тестом `framework_depth_move_test` (`framework-depth-move`).
30. Интервью 29 (depth): функции пола на `FloorView` по значению, inline-перегрузки на
    `DepthFloor<N>` через член `view()`; ёмкость стен проверяется на `DepthFloor<3>` и дефолте;
    тест пола — свой `check` и сломанные копии (`Tunnel`, `OpenEdge`);
    `ride_belt(DepthBody&, belt, FloorView)` везёт только стоящее тело.
31. Интервью 30–31 (ревью depth): у `blocked` своя группа в тесте пола (края стены замкнуты, края
    полосы проходимы, шаг за полосу — отказ) со сломанными копиями `OpenEdge` и `NoBand`; у старых
    случаев пола (край полосы, шов, угол, кламп) — свои контроли; сломанные копии и `Expect` —
    заголовок `framework_depth_floor_fixture.hpp`, бинарь один; вердикт → `framework-depth-floor`
    (отменяет «прежний вердикт» пп. 18 и 30); дубль `using depth::DepthProfile` в `fighter.hpp` и
    `depth_step.hpp` остаётся (цепочки include между ними нет); контроль `drive_keeps_speed`; в brawl-тесте
    отсутствующий игрок не прыгает на зажатом JUMP и не поворачивается на устаревшем стике;
    `framework_brawl` не объявляет `framework_depth` повторно (приходит через fighter); комментарии-
    пересказы убраны.
32. Интервью 33–34 (повторное ревью depth): ревью дельты; параметры сломанных `sweep`/`band`,
    живущие только в отброшенной ветке `if constexpr`, — `[[maybe_unused]]` (MSVC C4100 судит по
    инстанцированию); `broken_blocked<OpenEdge>` проверяет полосу замкнуто, контроль держится только
    на линиях стены; тест хода берёт `px`/`check`/`walled` из фикстуры пола; пересказ над `Break`
    удалён, над группой `blocked` — причина.

## Состояние

- [x] повестка `26-10-28-engine-finish-scope.md`
- [x] #18/#19/#20/#22 → «Закрыта с долгом», `checks/18.md`, `19.md`, `20.md`, `22.md`
- [x] #29 В2а — `a0a79f5`, CI зелёный на 3 ОС; долг — `checks/29.md`
- [ ] #29 В2б–В2е: В2б — core `bb694d0`, input `9159b3e`, physics/pendulum `6559712`, depth
  готов; дальше hits, kinematic, камера, slide
- [ ] #26 В2г–В2е
- [ ] #26 В3–В7
- [ ] #27 спека + интервью + реализация
