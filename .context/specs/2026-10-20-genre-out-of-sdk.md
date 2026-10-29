# Спецификация #29 В2: разбор жанра из SDK — инвентарь и подраунды

- **Дата:** 2026-10-20
- **Статус:** **Черновик** — инвентарь сделан 2026-10-20, три развилки решены владельцем
  2026-10-20 (ниже, «Решения владельца»), независимое ревью 2026-10-20 в два захода (20 и 7 находок, все ≥ low
  исправлены здесь). Код не начат.
- **Родитель:** [#29 В2](2026-10-19-pilot-game-tuning.md) — таблица «Где | Что известно» и DoD
  раунда; [ADR 0021](../decisions/2026-10-19-pilot-game.md) — SDK даёт только абстракции, константы
  абстракций становятся параметрами с прежними дефолтами (решения 8–10); частично отменяет
  [ADR 0016](../decisions/2026-08-30-character-tilemap.md).
- **Уровень проверки:** **T4**. Поведение rumble и платформера неподвижно, и это проверяется
  хешами, а не чтением кода. Каждый подраунд — переезд или выделение без изменения логики.

---

## Решения владельца (2026-10-20)

1. **Ядро выделяется сейчас.** Модули с жанровым входом (`Body`, `BrawlInput`, `BrawlWorld`)
   режутся по месту: ядро переходит в SDK на узком типе SDK, `rumble::Body` включает его
   композицией. Альтернативу «отдать модуль игре целиком, ядро позже» владелец отклонил: она
   оставляет SDK без глубины belt, которую пилот берёт первой.
2. **`character/` режется.** Ядро — `collision`, `slide`, `support`, `push`, `character_state` —
   остаётся в SDK. Контроллер — `controller`, `assist`, `ladder`, `profile*`, `trajectory` —
   уходит в кит `games/kits/platformer/`. Кит — исходники вне установки SDK, и его включает
   игра-потребитель.
3. **У каждой игры свой инструмент бейка.** `tools/assetc` становится библиотекой
   `assetc_core` с тонким `main`. Игра собирает `<игра>_assetc` со своей таблицей пекарей, а
   `like_nes_bake(... TOOL <цель или путь>)` зовёт его вместо SDK-шного `assetc`.

## Сборка жанра в дереве, против SDK и на телефоне

`games/neon-rumble/` собирается **только** против установленного SDK (`find_package(like-nes)`,
гейт `sdk-game`), в том числе под iOS и Android (`games/neon-rumble/CMakeLists.txt:17`). Тесты
жанра, crossplay, бенч ресима, fuzz `LNFT` и ASan собираются **в дереве**. Поэтому переехавший
жанр — это библиотека с двумя режимами сборки, а не россыпь исходников игры.

- **`games/neon-rumble/sim/`** — цель `rumble_sim`. Её `CMakeLists.txt` линкует
  `like-nes::engine`, если такая цель есть (сборка игры), и цели дерева, если нет. Корневой
  `CMakeLists.txt` подключает `games/neon-rumble/sim` и `games/neon-rumble/tests` после
  `engine/framework`.
- **`games/kits/platformer/`** — цель `kit_platformer`, тот же приём. Её включают
  `example_ugly_game` (в дереве) и `rumble_sim`: `platform_body` берёт `default_profile()`
  (`rumble_platform.cpp:43`).
- **Игра `neon_rumble`** линкует `rumble_sim` (а через него `kit_platformer`) через
  `add_subdirectory` своего каталога.

**Инструмент бейка.**

- Desktop SDK ставит `assetc_core` (библиотека, заголовки, цель в `like-nesConfig`) и базовый
  `bin/assetc` без пекарей бойца и профиля. Базовый `assetc` остаётся: от него зависят
  `sdk_config_desktop.cmake:7-11,41` (`LIKE_NES_ASSETC`), `install_sdk_desktop.cmake:16`,
  `install_engine.cmake:30`, гейты `release*`, `docs-start`, `sdk-game` и
  `docs/*/guide/sdk-and-your-game.md`.
- В desktop-сборке rumble `rumble_assetc` собирается тем же деревом игры.
- В мобильной сборке инструмент хоста кросс-компиляцией не получить. Сейчас бейк идёт
  десктопным `assetc` из `LIKE_NES_HOST_PREFIX` (`like_nes_bake.cmake:26`,
  `sdk_config_mobile.cmake:51-53`). Так же пойдёт и игровой: `games/neon-rumble/tools/` — проект
  хоста против desktop-префикса, собирающий `rumble_assetc`, а мобильная сборка получает путь
  `-DRUMBLE_ASSETC=<путь>`. Скрипты мобильных гейтов (`ios_sim_gate.sh`, `android_rumble.sh`,
  мобильная половина `sdk-game`) сначала собирают инструмент хоста.

**Load-bearing предположения В2а.** Двухрежимная цель собирается против установленного SDK на
трёх ОС. Инструмент хоста печёт мобильный бандл rumble с прежним `bundle.hash` на iOS-симуляторе
и Android. Если любое из двух не выполняется, спека возвращается к владельцу до переезда кода.

## Инвентарь

Классы:

- **SDK** — остаётся как есть;
- **SDK·парам** — остаётся, константа становится параметром с прежним дефолтом;
- **ядро→SDK** — ядро выделяется в SDK-модуль, остаток уходит игре;
- **игра** — уходит в `games/neon-rumble/sim/`;
- **кит** — уходит в `games/kits/platformer/`.

Правила:

- Хеш-спутник (`*_hash`, `*_snapshot`) идёт за своим модулем.
- Тест и фикстура идут за модулем, который проверяют.
- Тест, который входит через `brawl_step`, фикстуру драки или `controller`, остаётся с игрой
  или китом, даже если проверяет ядро. **У каждого ядра SDK — свой тест** только на заголовках
  ядра, с позитивным контролем на сломанной реализации. По включениям на 2026-10-20 чистый тест
  есть только у `depth_floor` (`framework_brawl_floor_test`). У `path_follow`, `movers`,
  `pendulum`, `seats`, `flick` тесты идут через фикстуры драки, у ядра `character/` — через
  `controller.hpp`/`profile.hpp`. Им нужны новые тесты.

### Целевые модули SDK

| Модуль | Состав | Откуда |
|---|---|---|
| `framework/depth/` | `depth_body`, `depth_profile`, `depth_floor` (`MAX_WALLS` = 16 → параметр), `depth_move`, `belt_ride` (`ride_belt`) | `brawl/depth_*`, `depth_step.cpp:14-39`, `conveyor_step` |
| `framework/hits/` | `hit_box` (`HitRect`, `place_box(DepthBody, facing, box)`, `rects_cross`, `depths_cross`), `struck_list` (`MAX_STRUCK` → параметр, дефолт 16), `hitstop` (счётчик стоп-кадра: «тикнуть и сказать, заморожен ли этот тик»; потолка в ядре нет — его держит парсер бойца, `MAX_HIT_TICKS` в `fighter_parse.cpp:53`, `fighter_read.cpp:26`, игра) | `brawl/hit_geometry`, `brawl/struck_list`, `brawl_step.cpp:57-61` |
| `framework/kinematic/` | `path_follow`, `movers` (`Movers` строится и шагает по `std::span<const scene::Path>` и `dt`), `movers_snapshot`, `movers_hash` | `brawl/path_follow`, `brawl/stage_movers*` |
| `framework/input/` (дополнение) | `flick` (`FlickRule` уже параметр), `action_buffer` (`{action, ticks}`, три операции: сброс, положить со сроком, взять с отсчётом срока и сбросом на нуле) | `brawl/dodge_flick`, `strike_queue` + `body_chain.cpp:19,26-44` |
| `framework/physics/` (дополнение) | `pendulum`: `rope_turns`, `rope_cap`, `rope_end`, `rope_tangent`, шаг на дуге и скорости (`ROPE_PUMP`, `ROPE_DAMPING` → параметры) | `brawl/rope_swing` без `rider`, `cooldown`, `ROPE_REACH`, `ROPE_COOLDOWN` |
| `framework/core/` (дополнение) | `ent_id`, `world_rng`, `seats` (`SEATS` → параметр шаблона, дефолт 2); `seat_hash` — в `framework/physics/` (core не видит `hash_mix.hpp`; `.context/notes/engine-finish.md`, п. 20) | `brawl/ent_id`, `ai/world_rng`, `brawl/seat*` |
| `framework/graphics/` (политика камеры) | `camera_autoscroll`: состояние езды (`scroll`, `riding`, `ride`, `settle`) и шаг по оси; `SETTLE_SHARE` → параметр. Кламп к прямоугольнику — уже существующий `graphics::clamp_axis` (`camera.hpp`), нового модуля нет | ядро `autoscroll_step` |
| `framework/scene/` | объекты без жанра: `LevelRect`, `Area`, `Mark`, `Path`, `Autoscroll`, `ScrollAxis` и их читатели; `ClassRule<Out>` и `read_objects<Out>(map, span<const ClassRule<Out>>, Out&)` — выход объявляет игра | `scene_objects.hpp`, `scene_read.cpp:124-143` |

Каталоги `engine/framework/brawl/` и `engine/framework/ai/` исчезают. `framework_brawl`,
`framework_brawl_fighter` и `framework_ai` уходят из `LIKE_NES_SDK_*` (`cmake/sdk_headers.cmake:99-110`).

**Подскок belt — абстракция, не «прыжок».** `drive` (`depth_step.cpp:14-26`) берёт из `BrawlInput`
только `present`, `move_x`, `move_z` и бит `JUMP`. Ядро `depth_move` принимает узкий
`DepthIntent {mx, mz, hop}` без `button::JUMP`, а игра выводит `hop` из своих кнопок. Вертикаль
belt (`y`, `vy`, `gravity`, `jump_vy` в `DepthProfile`) — это ось глубины 2.5D-тела, нужная любой
belt-игре, включая не-драку. Жанровое «когда и какой прыжок» остаётся в игре.

**Стоп-кадр — абстракция.** ADR 0021 (решение 9) и правило 7 `engine/framework/README.md` называют
его абстракцией. Модулем `hits/hitstop` он и становится. Поле `hitstop` остаётся в `rumble::Body`,
и порядок полей в `body_hash.cpp:15,47` не меняется. Ядро — только «тикнуть и сказать, заморожен
ли» (`brawl_step.cpp:57-61`: декремент, `++age`, ранний выход — `age` и выход остаются в игре).
Порядок `queue_strike` (`:55`) → `immune` (`:56`) → стоп-кадр остаётся в `step` игры: он входит
в `world_hash`.

### `brawl/` — 97 заголовков, 1 cpp без заголовка, 42 теста

| Класс | Файлы |
|---|---|
| ядро→SDK `depth/` | `depth_body`, `depth_profile`, `depth_floor` (включает `brawl_body.hpp` лишним — API на `DepthBody`), `depth_step` (ядро — `drive`/`fall`/`grounded` на `DepthIntent`, остаток `step_body`/`coast_body` на `Body` — игра), `conveyor_step` (`ride_belt` → SDK, `conveyor_under(Body, level)` — игра) |
| ядро→SDK `hits/` | `hit_geometry` (`place_box(Body)` → `place_box(DepthBody, facing)`), `struck_list`; стоп-кадр из `brawl_step.cpp:57-61` |
| ядро→SDK `kinematic/` | `path_follow`, `stage_movers`, `stage_movers_snapshot`, `stage_movers_hash`. Конструктор (`stage_movers.cpp:46-50`) берёт `LevelObjects`, шаг (`:52-62`) — `w.level` и `w.platform->dt`. Ядро берёт `span<const Path>` и `dt`; проверки «пути без movers» и «мир movers ≠ сцена платформы» (`:54-60`, `platform_world` — игра) остаются в игровой обёртке |
| ядро→SDK `input/` | `dodge_flick`, `strike_queue` (буфер — `body_chain.cpp:19` сброс в `begin`, `:26-29` `queue_strike`, `:31-44` `take_strike`, сцеплен с `chain_root`/`begin`; ядро — сброс, положить, взять с отсчётом; цепочки и `NO_CHAIN` — игре) |
| ядро→SDK `physics/` | `rope_swing` (`RopeState` с `rider`/`cooldown` и `ROPE_REACH`, `ROPE_COOLDOWN` — игре) |
| ядро→SDK `core/` | `ent_id`, `seats`; `seat_hash` → `physics/` (`engine-finish.md`, п. 20) |
| ядро→SDK камера | `autoscroll_step` (езда → SDK; кламп к арене в `ride_scroll:47-49` и `StageCamera` — игра) |
| игра: тело и пул | `brawl_body`, `body_pool`, `body_hash`, `body_role`, `body_kind`, `body_chain`, `body_clip`, `body_guard`, `body_react`, `body_run`, `clipless_kind`, `reaction`, `grip`, `run_state`, `living` |
| игра: бойцы `LNFT` | `fighter`, `fighter_format`, `fighter_read`, `fighter_bake`, `fighter_chain`, `fighter_fail`, `fighter_move_parse`, `fighter_name`, `fighter_parse.cpp`, `archetype`, `archetype_chain` |
| игра: удары | `hit_apply`, `hit_collect`, `hit_events`, `hit_grab`, `hit_strike`, `projectile` |
| игра: шаг и ввод | `brawl_input`, `brawl_step`, `seat_step`, `death_facts`, `death_step` |
| игра: сцена драки | `stage_step`, `stage_camera`, `stage_camera_hash`, `stage_waves`, `stage_waves_hash`, `wave_facts`, `wave_step`, `next_arena`, `level_clamp` (это `clamp_axis` по прямоугольнику арены — ядро уже в SDK), `level_progress`, `progress_hash`, `checkpoint_step`, `stage_leash`, `stage_sections`, `stage_sections_hash`, `section_exit`, `section_mode`, `section_step`, `platform_body`, `platform_world`, `stage_ropes`, `stage_ropes_hash`, `rope_ride` |
| игра: фикстуры и прогон | все `framework_brawl_*` заголовки (20: `check`, `crossplay`, `scenario_run`, `scenarios`, `section_spawn`, `fighter_corruptions` и 14 `*_fixture`) и 41 из 42 `framework_brawl_*_test.cpp`; `framework_brawl_floor_test` — за `depth/` |

`level_progress` (одно поле `checkpoint`) уходит игре вместе с `Checkpoint`: структура из одного
индекса абстракцией не является.

### `ai/`

| Класс | Файлы |
|---|---|
| ядро→SDK `core/` | `world_rng` |
| игра | `ai_brain`, `ai_state`, `ai_step`, `ai_target`, `attack_tokens` (ADR 0021 называет «ИИ с жетонами» жанром прямо), `resim_bench`, `brawl_resim_bench.cpp`, все `framework_ai_*` |

### `character/`

| Класс | Файлы |
|---|---|
| SDK | `collision`, `support`, `push`, `character_state`, `slide` (`MAX_SLOPE` переезжает из `profile.hpp:36` в `slide.hpp`; `slope_limit` уже параметр с этим потолком — `slide.hpp:63`; включение кита рвётся) |
| кит | `controller`, `assist`, `ladder`, `profile`, `profile_bake`, `profile_format`, `profile_parse.cpp`, `profile_read`, `trajectory` |
| кит: тесты | все 18 тестов `framework_character*_test.cpp` и их заголовки (`scene`, `platform_scene`, `ladder_scene`, `perf_level`, `perf_probe`): каждый входит через `controller.hpp` или `profile.hpp` — `corner`, `snap`, `oneway`, `slope`, `tunnel`, `wedge` напрямую, `push` и `platform` через `platform_scene`, `collision` через `profile.hpp` |
| SDK: новые тесты | ядро `collision`/`slide`/`support`/`push` — тест без кита, с позитивным контролем |

`character_state` держит `MoveState::Ladder` и поля прыжка в `MoveInput`, то есть платформенную
семантику. Владелец оставил его в SDK. Это известный остаток, и словарь гейта `sdk-genre` его не
задевает (см. ниже).

### `scene/`

`scene::Level` (`scene_level.hpp:27-43`) держит и бандл с тайлмапом, и `LevelObjects` с бойцами.
Поэтому он режется надвое:

- **SDK-часть `scene::Level`:** `bundle`, `name`, `collision`, `table`, `map`, `object_table`,
  `object_map`, `clips`, `texture_count`, `guids`, `pixels`, `bounds`;
- **игровой `rumble::Stage`** (включает `scene::Level`): `objects`, `fighters`, `fighter_count`,
  а с ними `LevelFighter` и `LEVEL_FIGHTERS` (`:20-25`) и включение `fighter_read.hpp`.

Загрузка — шагами. SDK `load_level` грузит таблицы, тайлмап и текстуры. Игра читает объекты
своей таблицей: `read_objects` (`level_load.cpp:52`, вызов на `:67`) уходит в её загрузчик.
Дальше игра грузит бойцов, а клипы — через отдельный SDK-шаг `load_clips(Level&, LoadError&)`.
Условие `objects.waves.empty()` (`level_load.cpp:68`) — игровое: SDK клипы безусловно не
грузит, решает вызывающий. Порядок шагов и первый отказ неизменны — их пинят тесты отказа.

| Класс | Файлы |
|---|---|
| SDK | `scene_stack`, `scene_object`, `level_textures` (пишет в SDK-часть `Level`), `level_error` (без `LoadError::BadFighter`, `level_error.hpp:18` — он уходит в игровой код ошибки; тексты отказов, запиненные тестами, неизменны) |
| ядро→SDK | `scene_objects.hpp` (`Section`, `SectionMode`, `Arena`, `WaveSpawn`, `Checkpoint`, `Conveyor`, `Rope`, `LevelObjects` → игра), `scene_read` (таблица `RULES` → игра, чтение по таблице → SDK), `level_load` (SDK грузит части `Level` и даёт `load_clips`; `read_objects`, `load_fighters` и условие `objects.waves` из `level_load.cpp:67-68` — игровая загрузка поверх), `level_parts.hpp` (`level_table`, `load_textures` — SDK; `load_fighters` — игра), `scene_level.hpp` (SDK-часть `Level`) |
| игра | `level_fighters.cpp`, `scene_links` (целиком: `arena_by_id`, `checkpoint_by_order`, `section_at`, связь волна→арена — `scene_links.cpp:8-88`) |
| тесты | `framework_scene_stack_*` — SDK; `framework_scene_test`, `framework_scene_refusal_test` («68 cases»), `framework_scene_level_test`, `framework_scene_level_refusal_test` («30 cases») и их фикстуры проверяют жанровые объекты → игра, пины числа случаев в `ci.yml` переезжают с ними; SDK получает тест читателя по таблице с игрушечным классом |

Связь `framework_scene` → `framework_brawl_fighter` PUBLIC (`scene/CMakeLists.txt:4`) рвётся.

### Остальные потребители

| Где | Решение |
|---|---|
| `tools/assetc` | `bakers_fighter`, диспатч fighter (`assetc_manifest.cpp:121-136,149`, `manifest_bake.cpp:132-135,195-196`) → `rumble_assetc`; профиль (`bakers_table.cpp:63-64`, `assetc_main.cpp:126`, `verify_game.cpp:27`) → только `ugly_assetc`. У rumble профиля в манифесте нет — `default_profile()` |
| `platform/mobile/bench_game.cpp` | `run_resim_bench` уходит в `rumble_mobile.cpp`; мобильная оболочка SDK бенча не знает. Процедура §34 обновляется в том же коммите |
| `tests/fuzz` | цель `LNFT` (`fuzz_targets_content.cpp`) → `games/neon-rumble/tests`, в составе `fuzz_readers`; `fuzz_targets_framework.cpp:24` (профиль) → кит; `fuzz_targets_scene.cpp` — по разрезу сцены: SDK-читатель остаётся, жанровые объекты → игра |
| `framework_section_align_test.cpp:33` | за кодом, который он включает (`character` → кит или ядро) |
| Зона | `view_zone_half(zone)` и `viewport_fit(screen, zone)` уже принимают зону (`viewport_fit.hpp:44,47`); `VIEW_LIMIT{576,312}` (`viewport_fit.hpp:25`) зашит в `viewport_fit.cpp:43-44` и `layer_cover.cpp:100,103` → параметр `limit` с дефолтом. Ключи манифеста `zone`/`zone_limit` с дефолтами 384×216 и 576×312 читает игра и кладёт в свой мир (поле зоны `BrawlWorld`), откуда её берут `autoscroll_step.cpp:20,47`, `wave_step.cpp:31`, `stage_step.cpp:61`. Прочие вызывающие: `layer_cover.cpp:116`, `rumble_window.cpp:134` (`VIEW_ZONE`), `rumble_join_report:19`, `rumble_wave_read:78`, `rumble_wave:49`, `rumble_layers:38`, `framework_brawl_stage_test.cpp:86` |
| `example_ugly_game` | линкует `kit_platformer` + ядро `framework_character`; `movement.txt` печётся `ugly_assetc`; закоммиченный `assets/game.bundle` и `--verify-game` (`ci.yml:791-796`) неизменны |
| `physics_query.hpp`, `tile_rules.hpp`, `tilemap/CMakeLists.txt` | упоминают `character` в тексте или связях — сверить в В2д |

## Гейты, прибитые к путям

Каждый переезжает **в одном коммите** с кодом, который он судит. Список ниже — известные места;
полноту даёт `git grep --untracked` старого пути или имени цели по `scripts/`, `.github/`,
`docs/`, `cmake/`, `.context/gates/`, а не этот список.

- **`check_sim_float.py:8-9`.** Сейчас `SIM_DIRS` покрывает весь `brawl/` и `ai/`, то есть и
  будущие ядра. Скрипт умеет только каталоги, поэтому дорабатывается до `SIM_PATHS` — каталогов
  и файлов, с selftest и `MIN_FILES`.
  - Каталоги: `framework/{depth,hits,kinematic}`, `games/neon-rumble/sim`,
    `games/kits/platformer`.
  - Файлы ядер в общих каталогах: `input/action_buffer`, `input/flick`, `physics/pendulum`,
    `core/{ent_id,world_rng,seats}`, `physics/seat_hash`, `graphics/camera_autoscroll`. В `graphics/` и
    `input/` целиком законный `float`.
  - `REQUIRED_DIR` → `games/neon-rumble/sim`. Мутант-фикстура с `float` — в игровом каталоге.
- **`check_crossplay.py:9`** — `COUNT_HEADER` → `games/neon-rumble/sim/rumble_crossplay.hpp`.
- **Голдены и прогоны.** `check_debug_golden.sh:47,58-67`, `check_goldens.sh:126`,
  `owner_perf.sh:49-50`, `sdk_game_coop.sh`.
- **`line_budget_allow_code.py`** — записи затронутых файлов (`character` на 57-130, `brawl` на
  169, `controller.cpp` на 106, `framework_scene_refusal_test.cpp` на 176).
- **`.github/workflows/ci.yml`** — ASan-список, джоб `crossplay`, пины «N cases» сцены, шаг
  `--verify-game` (791-796).
- **Состав и документация.** `cmake/sdk_headers.cmake:48-110`, документация SDK en/ru
  (`bodies-and-hits`, `camera-and-sections`, `hotseat`, `scenes-and-levels`,
  `sdk-and-your-game`, `tiled-levels`) и `docs/owner-setup.txt` — по новому составу.

**Имена целей.**

| Было | Стало |
|---|---|
| `framework_brawl_*` | `rumble_*` |
| `framework_ai_*` | `rumble_ai_*` |
| `framework_character_*` | `kit_platformer_*` |
| тесты ядер SDK | `framework_{depth,hits,kinematic,character}_*` |

Вердикты (`kebab-case: PASS`) и строки хешей **не меняются**. Тест `rumble_*` будет печатать
`framework-brawl-…: PASS` — это осознанно: вердикт пинят `ci.yml` и ASan-список
(`<бинарь>:<вердикт>`), и смена вердикта — отдельный перепин, а не часть переезда.

## Новый гейт: запрещённое в SDK

`python3 scripts/check_sdk_genre.py` (коммит) и `--selftest`. Вход — список
`LIKE_NES_SDK_HEADERS` из `cmake/sdk_headers.cmake` и тексты этих заголовков в дереве. Две
проверки:

1. **Пути.** В списке нет `framework/brawl/`, `framework/ai/` и файлов кита
   (`framework/character/{controller,assist,ladder,profile*,trajectory}.hpp`).
2. **Слова.** Проверяются имена файлов и идентификаторы кода; комментарии и строковые литералы
   вырезаются до проверки (комментарий «Combo/gesture-recognizer» в `input_buffer.hpp:8` — не
   нарушение). Идентификатор режется на токены по границам snake_case и CamelCase, сравнение без
   учёта регистра, совпадение — целым токеном после снятия окончания `-s`/`-es` (`waves`,
   `fighters`, `checkpoints`, `strikes` ловятся; `waveform` словом `wave` — нет).
   - Словарь: `brawl`, `fighter`, `archetype`, `wave`, `checkpoint`, `grip`, `strike`, `combo`,
     `knock` и пара токенов `attack`+`token`.
   - Нет в словаре: `arena` (`render/arena.hpp`, `TargetArena`), `ladder` (`TILE_LADDER` в
     `tilemap/grid.hpp:64`, `MoveState::Ladder`) и `controller` (законные значения в SDK).
     Файлы кита ловит проверка путей.

Словарь живёт в скрипте, каждое слово — с поводом в `.context/gates/sdk-genre.md`. До включения
гейта — прогон по итоговому списку: попадание в файл, остающийся в SDK, — это переименование или
allowlist с поводом в файле гейта, решаемые в В2е.

**Selftest:** четыре сломанные фикстуры — поле `waves` во множественном числе, `framework/brawl/x.hpp` в списке, идентификатор
`FighterTable` в чистом по имени файле, `framework/character/controller.hpp` в списке — и два
чистых контроля: `TargetArena` и `waveform` не ловятся. Строка в таблице `CLAUDE.md`. `sdk-game`
этот гейт не дублирует: он судит, что игра собирается, а не что SDK чист.

## Подраунды

Порядок жёсткий. Сначала всё, что меняет API или логику, — **на месте**, в старом дереве, где
хеши сверяются на каждом шаге. Потом переезд — **чистым `git mv`** с правкой только путей,
целей и гейтов. Если смешать выделение и переезд, сломанный хеш нельзя будет приписать ни одному
из них.

| | Что | Хеши после |
|---|---|---|
| **В2а** | каркас и инструмент:<br>• `rumble_sim` с двумя режимами сборки (`kit_platformer` — в В2д, решение 2026-10-28); первый файл `rumble_sim` — игровой исходник из `games/neon-rumble/src/` без окна — `rumble_floor.cpp` (включает только SDK-шные `depth_floor.hpp` и `fixmath.hpp`), не из SDK;<br>• `assetc_core` + `main` и поставка в desktop SDK;<br>• `like_nes_bake(TOOL)`;<br>• `rumble_assetc` (с пекарем бойца) и `ugly_assetc` (с профилем), проект хоста `games/neon-rumble/tools/` и `-DRUMBLE_ASSETC` в мобильных гейтах;<br>• базовый `assetc` без бойца и профиля;<br>• `sim-float` на `SIM_PATHS` с мутантом | неподвижны; `sdk-game` зелёный на 3 ОС, мобильный бандл тот же |
| **В2б** | ядра на месте:<br>• `framework/{depth,hits,kinematic}`, дополнения `input/physics/core` и политика камеры; `brawl/` включает их;<br>• `slide` теряет `profile.hpp`;<br>• тесты ядер с позитивным контролем | неподвижны |
| **В2в** | разрез сцены на месте:<br>• `scene::Level` / `rumble::Stage`, `ClassRule<Out>`, `scene_links` и `level_fighters` за игрой, `BadFighter`;<br>• `run_resim_bench` → `rumble_mobile.cpp` | неподвижны; пины «N cases» те же |
| **В2г** | переезд `git mv`:<br>• `brawl/`, `ai/`, жанр `scene/` → `games/neon-rumble/sim/`;<br>• тесты, фикстуры, crossplay, бенч ресима, fuzz `LNFT` → `games/neon-rumble/tests`;<br>• гейты путей | неподвижны; каталогов нет |
| **В2д** | `character/` режется: кит → `games/kits/platformer/` (цель `kit_platformer` с двумя режимами сборки — из В2а); `example_ugly_game` и rumble по киту | неподвижны; бандл платформера тот же |
| **В2е** | зона параметром манифеста + голден покрытия слоёв на 640×360 со сломанным слоем-контролем; гейт `sdk-genre`; `sdk_headers.cmake`, документация en/ru | неподвижны; гейт зелёный, selftest ловит все фикстуры |

## DoD раунда

- **После каждого подраунда неизменны:**
  - `bundle.hash` rumble (desktop и мобильный);
  - строки `sdk_game_run.sh`/`sdk_game_coop.sh`;
  - все crossplay-хеши;
  - `ai fight hash = 0x5138fba1d8279fa4`;
  - голдены `check_debug_golden.sh:58-67`: траектории `0x7801049c168bb301`, профиля
    `0xa554e52327f8d33d`, бойца `0x34030b3c5a326d2d`, прогресса `0xf1c00aba70879d31`, бенча
    ресима `0x0f60b3fbe989d27e`, лифта `0x7ec76882f27ca481`, троса `0x03892e88165766d3`;
  - голдены `game_platformer_{sim,rollback,replay}` (`check_goldens.sh:126`);
  - закоммиченный `example_ugly_game/assets/game.bundle` под `--verify-game`.
- **Каталоги и гейты:**
  - `engine/framework/brawl/` и `engine/framework/ai/` нет;
  - в установленном SDK нет жанровых заголовков — гейт `sdk-genre` со сломанными фикстурами;
  - у каждого ядра SDK свой тест без жанра и кита, с позитивным контролем;
  - `sim-float` видит `games/neon-rumble/sim`, `games/kits/platformer` и файлы ядер: мутант
    пойман.
- Голден покрытия слоёв на зоне 640×360 со сломанным слоем-контролем.
- CI зелёный на 3 ОС, `sdk-game` на 3 ОС, iOS-симулятор и Android-бандл rumble.

## Открытые вопросы

- Двухрежимная цель и инструмент хоста — проверяются первым шагом В2а (см. «Сборка жанра»).
- Попадания словаря `sdk-genre` в файлы, остающиеся в SDK, — по прогону до включения гейта (В2е).
