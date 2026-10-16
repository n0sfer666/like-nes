# Спецификация #26: UI и шрифты, тач, landscape и safe area, сейвы, потоковая музыка, Android-эмулятор

- **Дата:** 2026-10-15
- **Статус:** **В работе** — интервью владельца закрыто 2026-10-15 (решения ниже), независимое
  ревью черновика пройдено (6 high, 14 medium, 9 low — все внесены; CROSS уточнён владельцем).
  В1 сделан 2026-10-16 (В1а–В1д), телефон Android — гейт §36 у владельца. Шаг 0 ждёт выбора
  музыки владельцем (`~/wiki/dev-log/like-nes/checks/26-music.md`). Следующий шаг — В2.
- **Зонтик:** [#23](2026-10-01-beatemup-game.md), раздел «#26». Здесь — подробности и DoD раунда.
- **Наследует:** [#3](2026-07-19-audio-subsystem.md) и [#5](2026-07-19-asset-pipeline.md) (микшер,
  кольцо `SampleRing`, `stb_vorbis`), [#4](2026-07-20-input-system.md) и
  [#14](2026-07-26-framework-input.md) (`InputEngine`, `ActionMap`, `RebindStore`, `PadRegistry`,
  лобби, форма оси, rumble), [#12](2026-07-26-windows-core-parity.md) (шов `platform`, fs-seam),
  [#17](2026-07-26-graphics-framework.md) (`viewport_fit`, `text_quads`),
  [#24](2026-10-01-content-pipeline.md) (манифест, `LNFN` из bitmask JSON, SDK, инвентарь лицензий
  и бюджет ассетов), [#25](2026-10-05-brawl-framework.md) (`BrawlInput`, `LNFT`, цепочки,
  `track_run`, хотсит, ресим 8 тиков).
- **Уровень проверки:** **T4** на сейвах (roundtrip, fuzz битого файла, миграции, карантин,
  атомарность под обрывом), музыке (стык лупа, смена трека под откатом, RT-safe под TSan/ASan) и UI
  (голден квадов и фокуса); **T3** на эмуляторе Android и симуляторе iOS (landscape в обе стороны,
  safe area, два пальца, кадр не чёрный, фон → возврат); **T2** на бейке шрифтов, строк, музыки и
  раскладки. Живое железо — две точки владельца: раскладка тача после В2 и замер кадров в В7.

---

## Контекст

Разведка 2026-10-15 по `platform/{mobile,ios,android}`, `engine/{audio,input,achievements,platform}`,
`engine/framework/{graphics,input,brawl,rollback}`, `games/neon-rumble`, `example_ugly_game`, `ci.yml`:

| Что есть | Чего не хватает |
|---|---|
| `platform/mobile/mobile_game.{hpp,cpp}` — кадр шутера `game::MobileGame` (`init/pointer/cancel/frame/shutdown`), тянет `game_world.hpp`, `achievements.hpp`, `input_setup.hpp` шутера | Оболочка как библиотека SDK, интерфейс игры, CMake-функция для `.app` и APK |
| `native_main.cpp:91`: `APP_CMD_TERM_WINDOW` → `game.shutdown()` + `release_gpu` — уход в фон убивает игру | Раздельная жизнь игры и поверхности |
| `platform/mobile/mobile_stick.hpp` — `stick_axis()` на `double` и `__builtin_sqrt`; iOS-гейт судит маршрутизацию через `stick_touch()`/`fire_touch()` (`mobile_game.hpp:39-40`) | Плавающий стик, общая с #14 форма оси, тач как устройство `InputEngine`, шов пробы |
| Портрет: `view.mm`, `Info.plist.in:30`, `AndroidManifest.xml:9`; манифест без `uses-permission` и `uses-sdk`, `hasCode="false"`; `build_apk.sh` — `API=24`, ресурсы не компилирует | Landscape в обе стороны, safe area, `shortEdges`, immersive, minSdk 29, разрешения |
| ABI зашит: `build_apk.sh` (`ABI="arm64-v8a"`, `HOST_TAG="darwin-x86_64"`, путь `libc++_shared.so` aarch64, SDK `~/Library/Android/sdk`), `wgpu_native.cmake:34-57` (Rust-цель `aarch64-linux-android`, линкер, `CC`/`AR`) | Сборка `x86_64` на Linux-хосте CI для эмулятора |
| Гейт iOS-симулятора `ios_sim_gate.sh` + `ios_touch_verdict.py` (история касаний по нонсу прогона) | Такой же гейт Android-эмулятора |
| `SourceKind{None, Key, MouseButton, MouseAxis, PadButton, PadAxis}` (`input_types.hpp:17`); `PlayerAssign{pad_slot, use_kbd_mouse}` собирается позиционно (`lobby.cpp:76`, `:83`) | Источник тача со своим слотом устройства |
| `source_gamepad_macos.mm` — GameController + rumble, `PadInfo.name` из `vendorName` (категория одинакова у всех extended-падов), собирается только в `input_native` вместе с GLFW, не под `LIKE_NES_MOBILE` (`engine/input/CMakeLists.txt:28`) | Пад-бэкенд на iOS и Android |
| `GamepadSource::set_rumble(slot, low, high, ms)` | Шов хаптики с телефоном как целью |
| Разбег по двойному рывку уже распознаёт симуляция: `track_run` (`body_run.cpp:20-33`, окно `run_tap`); цепочки `jab jab cross` (banderas), `jab jab cross kick` (rainbird), рантайм — `body_chain.cpp` | DODGE на таче (блок + рывок) |
| `LNFN` из bitmask JSON (`font_import.hpp`), monogram: 390 глифов, 66 кириллических; `text_quads(font, places, pen, out)` | Таблица строк en/ru, гейт покрытия глифов, UI-слой |
| `ach::write_atomic`, `temp_path_for`, `read_file` (`engine/achievements/store.hpp:11-13`) — долг #14 «write_atomic продублирован»; `sync_file` на Apple = `fsync` без `F_FULLFSYNC` (`platform_fs_posix.cpp:98`); `user_data_dir` без ветки Android (`platform_fs_posix.cpp:161-167`); путь сейва на Android даёт оболочка `env_put("LIKENES_SAVE_DIR", internalDataPath)` (`native_main.cpp:101-105`) для `resolve_save_path` шутера | API сейвов со слотами, заголовком, миграциями и карантином; каталог сейвов из оболочки |
| Микшер на фиксированных 48 кГц (`SAMPLE_RATE`, `audio_types.hpp:12`), ресемплера нет; голос берёт ОДИН моно-сэмпл на выходной фрейм и панорамирует (`mixer.cpp:31-33`); `register_source` — только вне RT до `play` (`mixer.hpp:36`); `decode_vorbis` — моно int16 целиком | Потоковый стерео-голос, кольца A/B, точки лупа, кроссфейд, `want()` |
| `rollback::Session::LEAD = 16` (`session.hpp:32`), глубина — параметр `reset` (в #25 — 8) | Константа устойчивости музыки в тиках |
| Neon Rumble: меню #25 (`rumble_menu.cpp`), титры, хотсит, вход P2 | Главное меню, настройки, рекорды, сейвы, музыка |

---

## Ключевые решения (владелец, 2026-10-15)

| Тема | Решение |
|---|---|
| Объём | Одна спека, шаг 0 и вертикали В1–В7. **PR `dev` → `main` на каждую вертикаль** (не один на раунд, как #24/#25) |
| Шрифты | BMFont из #26 убран. Остаётся `LNFN` из bitmask JSON. Добавляется гейт покрытия глифов строк в бейке |
| Оболочка | Уходит в SDK: библиотека оболочки, интерфейс игры, `like_nes_mobile_app()` для `.app` и APK, мобильные префиксы установки; `sdk-game` проверяет мобильную сборку |
| Шутер | `example_ugly_game` — второй потребитель интерфейса и тач-модуля. Сим-голден `0x32a094e89eacf2f2` стережёт переезд, iOS-гейт двух пальцев остаётся |
| Android-гейт | AVD landscape с эмуляцией выреза: запуск, safe area из лога, два пальца через `sendevent` в `/dev/input`, скриншот без чёрного кадра. Вердикт — как `ios_touch_verdict.py`, без Java. Preflight + CI-джоб ubuntu x86_64 с KVM |
| Ориентация | Обе стороны landscape (`sensorLandscape` / `UIInterfaceOrientationMaskLandscape`); safe area и раскладка пересчитываются при повороте, активные касания отпускаются |
| Тач | Виртуальное устройство `InputEngine`: `ActionMap`, перебинды, лобби хотсита, запись и реплей — как с падом |
| Кнопки | Пять: удар (PUNCH), пинок (KICK), прыжок (JUMP), захват (GRAB), блок (BLOCK). **CROSS — существующая цепочка бойца** (`jab jab cross`: третий удар, по попаданию; уточнено владельцем после ревью — `LNFT` и симуляция не меняются). DODGE — блок + рывок стика. Разбег — двойной рывок стика (уже `track_run`) |
| Стик | Плавающий: центр тянется за пальцем на поводке за радиусом. Мёртвая зона и кривая — форма оси #14 |
| Тач + пад | Тач занял место игрока → кнопки видны при любых падах. Места нет и пад подключён → кнопки скрыты; касание экрана показывает их снова |
| UI | Immediate-mode кодом со стабильными id: list, button, slider, field. Фокус в контексте, без аллокаций в кадре, вывод — квады через `text_quads`. Голден по квадам и фокусу |
| Ввод текста | IP, код комнаты, имя из 3 букв — своя экранная сетка (цифры, `.`, `:`, A–Z), одна реализация для пада, тача и клавиатуры; клавиатура печатает напрямую. Без системной клавиатуры и IME |
| Единицы | UI и HUD — пиксели зоны × k; на 4:3 HUD в верхней полосе `viewport_fit`. Тач-кнопки — исключение: размер в точках экрана, положение в safe area |
| Строки | Нет ключа в ru → откат на en и счётчик в сводке бейка; `--strict` делает это отказом (релизный бейк и `sdk-game` — strict). Глиф без шрифта — отказ всегда |
| Сейвы | `save::store/load(slot)`, файл на слот, заголовок с magic, версией, длиной и CRC, атомарная запись. Слоты `progress`, `records`, `settings`. Достижения и профили управления сохраняют форматы, но пишут через тот же `write_atomic` и в тот же каталог — долг #14 закрывается. Битый слот не тянет остальные. Путь — за fs-seam |
| Версии | Старая версия — цепочка миграций vN→vN+1 (функции игры). Новая или битая — в карантин (`.bak` / `.bad`), старт с пустого и сообщение игроку. Безвозвратно ничего не теряется |
| Облако | Шов `save::Backend` в #26, единственная реализация — локальная; правило конфликта — больший счётчик записей, а не часы. iCloud и Play Saved Games — follow-up с аккаунтами (как Steam-адаптер #10) |
| Фон | Пауза + запись слотов и достижений. Живой процесс возвращается на экран паузы. Снапшот драки в 1.0 не пишется; убитый процесс начинает с меню |
| Музыка | Потоковый Vorbis в рабочем потоке, RT-колбэк читает только кольцо. Точки лупа — в манифесте, в сэмплах, по умолчанию весь файл; бейк сверяет их с длиной трека. Голден стыка `loop_end → loop_start`. Кроссфейд |
| Выбор трека | Функция состояния: каждый кадр `music.want(трек)`; тот же трек — ничего; смена ждёт устойчивости дольше глубины отката. Откат #27 музыку не дёргает |
| Треки | Шаг 0: я ищу CC0/CC-BY синтвейв/киберпанк (вступление + луп; меню и уровень 1), владелец слушает и выбирает. Инвентарь лицензий и бюджет 30 МиБ; точки лупа ставлю я, стык на слух судит владелец |
| Хаптика | `haptic::pulse(target, strength, ms)`, цель — телефон или пад игрока. iOS — Core Haptics; Android — `VibrationEffect.createOneShot` через JNI (`VIBRATE`); пад — rumble #4. Игра зовёт из `HitEvent` как декор, только для локального игрока; выключатель в настройках |
| Пады | iOS — `GCController.extendedGamepad` → generic-профиль. Android — оси и кнопки из `AInputEvent` без JNI, VID/PID через JNI `InputDevice` для профиля #14. `pad_info` заполняется на обеих ОС. **Имя — `vendorName`, как в macOS-бэкенде** (в интервью звучал `productCategory`; ревью: категория одинакова у всех extended-падов и профиль по ней не выбрать) |
| Ранняя точка железа | Раскладку тача владелец пробует на телефоне сразу после В2 (сценарий в `owner-verification.md`) |
| Экраны игры | Главное (соло / хотсит / титры), настройки (громкость музыки и эффектов, хаптика, тряска и вспышки, язык, прозрачность тача → слот `settings`), рекорды с вводом имени сеткой (слот `records`). Полное меню — #28 |
| Перф | Игра печатает p50/p99 кадра и число пропусков за прогон с музыкой, UI и тачем. Порог — пропусков < 1% на самом медленном телефоне владельца, сценарий как §34. Эмулятор и симулятор цифру не судят |
| Подпись | Вне #26, follow-up #8. На телефоне владельца — дев-подпись (Xcode personal team / debug keystore), как в §34 |
| Вне #26 | Портрет и планшетные раскладки (та же landscape-раскладка с масштабом в точках); редактор раскладки тача в игре (раскладка — данные + прозрачность) |

**Мои решения** (обоснование в тексте вертикалей; владелец может развернуть до старта вертикали):
- ABI эмулятора — по хосту: `arm64-v8a` на Apple silicon, `x86_64` на CI (arm64-образ на
  x86_64-хосте эмулятор не запускает). APK для телефона остаётся arm64.
- Запись манифеста музыки: `music | <имя> | vorbis | <путь>.ogg [| <loop_start> | <loop_end>]` — поле
  кодека, как у `font`/`level`/`clips`: контейнер может смениться, а разбор не должен.
- Строки — один файл `strings | <имя> | tsv | <путь>.tsv | <шрифт>` со столбцами `key`, `en`, `ru`:
  пропуск перевода виден в одной строке, а не в разнице двух файлов; шрифт назван, потому что гейт
  покрытия судит строки по нему.
- Тач — свой вид сырых событий и свой слот устройства (`RawKind::TouchButton`/`TouchAxis`,
  `DeviceKind::Touch`), а не пад-слот: пад-слоты нумерует ОС, и тач на месте пада занял бы слот и
  сдвинул бы их. В `PlayerAssign` — именованное поле `use_touch`.
- Тач-кнопка паузы — значок в углу safe area, вне пяти кнопок боя (иначе на таче паузы нет).
- В запись и снапшот тач попадает только действиями (`InputFrame`): рывки и DODGE распознаются в
  детерминированном слое по тикам, не в источнике.

---

## Требования

### Шаг 0. Музыка (до В6, параллельно В1)

- Я ищу 4–6 кандидатов CC0 / CC-BY (OpenGameArt, freemusicarchive, itch.io): синтвейв/киберпанк,
  трек со вступлением и лупом, на меню и уровень 1. Для каждого — ссылка, лицензия, длина, частота
  дискретизации, размер `.ogg` при 96–128 кбит/с, есть ли чистая точка лупа.
- Частота — **48 кГц или перекодирование** в 48 кГц при подготовке ассета (микшер без ресемплера,
  трек 44.1 кГц заиграл бы на 8.8% выше).
- Сводка — `~/wiki/dev-log/like-nes/checks/26-music.md`; владелец слушает и выбирает два.
- Выбранные — в `games/neon-rumble/assets/<автор>/`, в `LICENSES.toml` и `NOTICES.txt`; гейты
  `asset-licenses` и `asset-budget` зелёные. В манифест — с В6.

### В1. Оболочка в SDK и интерфейс игры

- `like_nes::mobile` — библиотека оболочки в SDK: `platform/ios/{app.mm,view.mm}`,
  `platform/android/native_main.cpp` и общий кадр перестают знать про шутер. Игра даёт реализацию
  `mobile::Game` (API ниже) и фабрику `mobile::make_game()`.
- **Жизнь игры ≠ жизнь поверхности.** `init` создаёт игру один раз; `attach_surface`/
  `detach_surface` — поверхность и swapchain. `APP_CMD_TERM_WINDOW` (каждый уход в фон на Android)
  снимает только поверхность; `shutdown` — только на `APP_CMD_DESTROY` / `applicationWillTerminate`.
  Шутер (`std::optional<Achievements>`, повторный `init`) и бенч ресима `bench_main.cpp` (#25 В9г)
  переезжают на ту же схему.
- `like_nes_mobile_app(<цель> SOURCES … BUNDLES … NAME … ID …)` — CMake-функция SDK: на iOS собирает
  `.app` (Info.plist из шаблона SDK), на Android — `.so` + APK через `build_apk.sh` из префикса
  установки. Мобильные префиксы (`ios-sim`, `ios-device`, `android-<abi>`) ставятся тем же
  `cmake --install`, что десктоп.
- ABI-зависимые места параметризуются ABI: `build_apk.sh` (`ABI`, `HOST_TAG` по `uname`, путь
  `libc++_shared.so` из triple ABI, SDK/NDK из `ANDROID_HOME`/`ANDROID_NDK_HOME` с дефолтом по ОС),
  `wgpu_native.cmake` (Rust-цель, линкер, `CC`/`AR` из triple). Сборка с `-DANDROID_PLATFORM=android-29`,
  `uses-sdk minSdkVersion=29` в манифесте. Бенч-APK `--bench` и гейт `wgpu-pin` — на той же
  параметризации.
- **Отклонения реализации (В1в, 2026-10-16).** APK собирает `platform/android/package_apk.sh` —
  цель `<цель>_apk` функции `like_nes_mobile_app`, одна у дерева и у префикса; `build_apk.sh`
  остался обёрткой дерева (конфигурирование + копия в `<build>/apk/`). `HOST_TAG` и путь
  `libc++_shared.so` даёт тулчейн NDK (`ANDROID_HOST_TAG`), а не `uname`. `<цель>_apk` — в ALL.
  Разбор — `.context/notes/mobile-shells.md`, раздел «Оболочка в SDK».
- Шутер переезжает на интерфейс как есть: `game::MobileGame : mobile::Game`, его логика касаний
  остаётся до В2.
- Гейт `sdk-game` получает мобильную половину: Neon Rumble собирается против поставленного SDK для
  iOS-симулятора (macOS-хост) и `android-x86_64` (Linux-хост CI) — сборка без запуска. Префикс
  SDK заголовков шутера не содержит: оболочка, тянущая заголовок шутера, не собирается.
- Neon Rumble запускается на iOS-симуляторе: уровень 1, демо-ввод из скрипта хотсита. Фон →
  возврат: процесс жив, поверхность пересоздана, игра продолжает (экран паузы — В7).
- **Отклонения реализации (В1г, 2026-10-16).** Драка на телефоне шагает тиком 1/60 с по `dt`
  (аккумулятор, не больше 4 тиков за кадр), а не тиком на кадр: экран 90/120 Гц ускорил бы её в
  1.5–2 раза, а фиксированного шага в движке до В7 нет. Улика непрерывности — строки
  `[game] surface attached/detached … world=<хеш драки>`, и сверх неё хеш на тике detach сверяется
  с десктопным `--headless` той же сборки: телефон играет ту же драку. Процедура —
  `bash scripts/ios_sim_rumble.sh` (шаг R2а `docs/owner-setup.txt`). Запуск на Android — ручная
  проверка владельца в конце В1; сценарий в `docs/owner-verification.md` — В1д.
- **Отклонения реализации (В1д, 2026-10-16).** Мобильная половина `sdk-game` —
  `scripts/sdk_game_mobile.sh`: кроме сборки пакет судится по составу (бинарь своей платформы,
  `libc++_shared.so`, оба бандла, все лицензии префикса), а `game.bundle` пакета побайтно равен
  бандлу десктопной игры. Android x86_64 собирается при пиннутом NDK (иначе SKIP с причиной);
  в CI `SDK_GAME_ANDROID=required` делает его обязательным на Linux, `skip` выключает на macOS, на Windows мобильной половины нет. Отказ на заголовке шутера — фикстура с
  `#include "shooter_mobile.hpp"` на обеих платформах. Запуск на телефоне Android —
  `scripts/android_rumble.sh` (arm64-v8a, Home и возврат интентом лаунчера, та же сверка с
  `--headless`, что у `ios_sim_rumble.sh`; общий суд — `scripts/mobile_resume_lib.sh` и `scripts/mobile_fight_lib.sh`), гейт §36
  `docs/owner-verification.md`. Попутно: фикстура CRT `sdk-game` после В1г ничего не вырезала
  (вызов `like_nes_add_game` ушёл в `else()` с отступом) — шаблон с отступом и FAIL на пустой правке.
- Голден `0x32a094e89eacf2f2` неизменен, `ios_sim_gate.sh` зелёный на шутере.

### В2. Тач, landscape, safe area, Android-гейт

**Устройство.**
- `TouchSource` получает касания от оболочки (`pointer(id, phase, x, y)`, x/y — точки экрана: iOS
  points, Android px / density) и раскладку данными (`touch | <имя> | layout | <путь>.txt`: кнопка,
  якорь safe area, смещение и радиус в точках; секция `LNTL`). Эмитит в `InputEngine` оси стика и
  пять кнопок — `RawKind::TouchButton`/`TouchAxis` в `DeviceState` слота тача.
- Точки правки ввода: `RawKind`/`DeviceKind`/`DeviceState` (+ `reset_all_held`), `action_map.cpp`
  (`source_pressed`/`source_axis`), `action_shared.cpp` (`reaches`), `source_names.cpp` (текст и
  разбор `touch:a…e`, `touch:stick_x/y`), `layout_keys.cpp` (вход в лобби касанием). `lobby.cpp:76`
  и `:83` собирают `PlayerAssign` по именам полей — позиционная сборка обнулила бы `use_touch` при
  потере пада. Два места с `use_touch` — отказ (экран один).
- Формат профиля управления расширяется именами тач-источников; файлы прежних версий читаются без
  изменений, голдены ввода #14 неизменны.
- Плавающий стик: первое касание левой половины ставит центр; палец за радиусом тянет центр на
  поводке длиной радиус. Ось — общая функция формы #14 (мёртвая зона, внешняя граница, кривая) в
  fix32; `mobile_stick.hpp` удаляется, `double` из пути ввода уходит.
- Касание принадлежит тому, кто принял его первым (стик или кнопка N) до `Up`/`cancel`.
- Шов пробы: `TouchSource::owner(control) → touch id | none`. Строки `[touch]` iOS-гейта, вердикт
  и `uitest/touch_gate.m` переписываются на него **в том же коммите**, что удаляет
  `stick_touch()`/`fire_touch()`.

**Приёмы.**
- Разбег — существующий `track_run`: двойной рывок стика даёт смену знака `move_x` в окне
  `run_tap`, нового кода нет.
- DODGE — функция детерминированного слоя рядом с `command_of`: удержан BLOCK и ось вышла из мёртвой
  зоны за порог за ≤ K тиков по последовательности `InputFrame`. Пад и клавиатура сохраняют
  отдельную кнопку DODGE; на таче она не нужна.
- CROSS — цепочка бойца, без изменений.

**Видимость.** Правило «Тач + пад». На экранах UI тач-кнопки боя скрыты, остаётся значок паузы в
игре. Прозрачность — из слота `settings` (В7; до него — дефолт). Пад отключили посреди боя: место
уходит в `Lost` (лобби #14); на мобилке первое касание кнопок боя отдаёт это место тачу, если тач
не занят другим местом.

**Экран.**
- Landscape в обе стороны; `Game::resize(fb, SafeArea)` при повороте, смене выреза и insets;
  `cancel()` отпускает касания в том же тике.
- iOS: `UIInterfaceOrientationMaskLandscape`, `safeAreaInsets` + `viewWillTransitionToSize`
  (срабатывает и на 180°), скрытый home indicator с отложенным жестом.
- Android: `sensorLandscape`; `layoutInDisplayCutoutMode=shortEdges` через тему — значит, в APK
  компилируются ресурсы (`aapt2 compile/link` в `build_apk.sh`, `hasCode="false"` остаётся); insets —
  JNI `WindowInsets.getDisplayCutout()` + `getInsets(systemBars)`; immersive sticky
  (`WindowInsetsController` на API 30+, `setSystemUiVisibility` на 29). Поворот на 180° конфигурацию
  не меняет — оболочка опрашивает `Display.getRotation()` через JNI раз в 30 кадров и зовёт `resize`
  при смене.
- Разрешения: `INTERNET`, `VIBRATE`, `NSLocalNetworkUsageDescription`, `NSBonjourServices`.

**Гейт Android-эмулятора** `scripts/android_emu_gate.sh` (гоняет Neon Rumble):
- AVD API 29+, вырез — оверлеем `cmd overlay enable com.android.internal.display.cutout.emulation.corner`,
  GPU `-gpu swiftshader_indirect` (wgpu на Vulkan/SwiftShader; GLES — запасной путь не допускается,
  см. память «fallback делает гейт вакуумным»: бэкенд печатается в лог и судится).
- Флаг пробы — extra интента (`am start … --es probe <нонс>`), оболочка читает его через JNI
  `getIntent()`; у NativeActivity нет argv.
- Сценарий: запуск → строки `[safe]` (≠ 0 со стороны выреза) → два пальца `sendevent` (multitouch
  protocol B) → поворот `settings put system accelerometer_rotation 0`, `user_rotation 1` и `3` →
  повтор `[safe]` (вырез сменил сторону) и двух пальцев → `screencap`, кадр не чёрный.
- Координаты `sendevent` — в натуральной ориентации панели (портрет); скрипт переводит зоны раскладки
  в них по `user_rotation`, вердикт судит зоны.
- Вердикт — общий модуль `scripts/touch_verdict.py` (`ios_touch_verdict.py` — тонкая обёртка), строки
  с нонсом прогона.
- Preflight + CI-джоб `android-emu` (ubuntu, KVM, x86_64-образ): кэш образа и snapshot AVD, кэш
  cargo/wgpu-native, `rustup target add x86_64-linux-android`, NDK из `sdkmanager`.
- iOS-гейт получает landscape: симулятор с вырезом, повороты в обе стороны, `[safe]` и два пальца
  после каждого.

**Точка железа:** сценарий `owner-verification.md` «раскладка тача на телефоне» — владелец играет
уровень 1 на своём телефоне; неудобная раскладка меняет набор кнопок до В3 (риск зонтика).

### В3. Пады на мобилках и хаптика

- iOS: `source_gamepad_macos.mm` выносится из `input_native` в отдельную цель без GLFW и собирается
  под `LIKE_NES_MOBILE` (уже GameController: `extendedGamepad`, нотификации подключения,
  `vendorName`, vid/pid = 0 → generic по контракту `PadInfo`). macOS-сборка и профили `PadRegistry`
  неизменны.
- Android: оси и кнопки из `AInputEvent` (`AMOTION_EVENT_AXIS_*`, `AKEYCODE_BUTTON_*`) в
  `native_main.cpp` без JNI; VID/PID через JNI `InputDevice.getVendorId/getProductId` один раз при
  подключении → профиль `PadRegistry` #14.
- `haptic::pulse(Target, strength, ms)`, `Target{Kind::Phone}` или `Target{Kind::Pad, slot}`.
  iOS — Core Haptics (`CHHapticEngine`, transient), Android — `Vibrator.vibrate(VibrationEffect.createOneShot)`
  через JNI, пад — `set_rumble`. Нет движка (симулятор, старый телефон) — no-op и одна строка в лог
  при старте.
- Neon Rumble зовёт `pulse` из `HitEvent` (декор, вне снапшота), только для локального игрока;
  выключатель — в слоте `settings` (до В7 — включено).

### В4. Сейвы

- `engine/framework/save/`, цель `framework_save` в SDK.
- Заголовок, little-endian, сериализация по полям (без `memcpy` структуры — набивка): magic `LNSV`,
  версия контейнера (u16), версия слота (u16, задаёт игра), длина тела (u32), счётчик записей (u64),
  CRC32 по заголовку без поля CRC **и** телу. Флип бита версии даёт `Corrupt`, а не `Future`.
- `save::write_atomic(path, bytes)` — temp + `sync_file` + `replace_file` + `sync_dir_of`; одна
  реализация вместо `ach::write_atomic`. `sync_file` на Apple переходит на `fcntl(F_FULLFSYNC)`:
  иначе переименование может дойти до носителя раньше данных и заменить целый сейв битым. Задевает
  всех пользователей шва (`ach::write_atomic`, `rebind_store.cpp`, пересборку бандла, crash-тест
  достижений) — они перепроверяются в той же вертикали.
- Достижения и профили управления (`controls.txt`, `controls-p2.txt` … — `profile_file.hpp:7`)
  переходят на неё и на каталог `save::dir()`, форматы не меняются. Первый старт после обновления
  переносит найденные файлы со старого места (`resolve_save_path`, `LIKENES_SAVE_DIR`) в
  `save::dir()`, не перезаписывая существующие.
- Каталог: оболочка задаёт его через шов до `init` (`platform::set_save_dir`: iOS — `Application
  Support`, не видна в Files; Android — `internalDataPath`); десктоп — `user_data_dir(app)`.
  `save::dir()` пустой или недоступный — отказ записи с сообщением, а не запись в текущий каталог.
- `load(slot, version, chain, body, &quarantine) → Ok | Empty | Migrated | MigrateFailed | Future |
  Corrupt`. Контракт цепочки: `chain[i]` переводит v(i+1) → v(i+2), `chain.size() == version − 1`,
  иначе отказ (ошибка программы).
- Карантин без перезаписи: `<slot>.v<N>.<k>.bak` (будущая версия), `<slot>.<k>.bad` (битый),
  `<slot>.v<N>.<k>.pre` (копия до миграции), `k` — первый свободный. Путь карантина возвращается
  игре для сообщения. `MigrateFailed` — оригинал уже в `.pre`, слот стартует пустым.
- Счётчик записей не сбрасывается карантином: новый слот продолжает с max(счётчики читаемых
  заголовков слота и его карантина) + 1.
- `save::Backend` — шов (`read/write/list`), единственная реализация `LocalBackend`. Конфликт двух
  копий — больший счётчик записей; часы не участвуют. Правило судится тестом на fake-backend с двумя
  копиями.
- Фон: iOS — `applicationDidEnterBackground` пишет синхронно под `beginBackgroundTask`. Android —
  `APP_CMD_PAUSE` ставит паузу; запись — на `APP_CMD_SAVE_STATE` (`android_native_app_glue` ждёт
  его обработки, в отличие от `PAUSE`), повтор на `APP_CMD_STOP`.
- fuzz-цель `save` в `fuzz_readers` (битый заголовок, длина больше файла, CRC, версии 0/65535).

### В5. Строки, UI, сетка ввода

- `strings | <имя> | tsv | <путь>.tsv | <шрифт>` → секция `LNST` (ключ FNV через общий примитив —
  гейт hash-seam; коллизия ключей — отказ). Сводка бейка: число ключей, откатов на en; `--strict` —
  откат = отказ.
- Гейт покрытия: каждый кодпоинт каждой строки есть в названном шрифте, иначе отказ с ключом и
  кодпоинтом. Алфавиты сетки ввода — тоже ключи `strings.tsv` (`grid.ip`, `grid.name`, `grid.room`)
  и проходят тот же гейт. Видимый игроку текст — только из строк; литерал в коде экранов — нарушение
  (гейт по коду игры, `--selftest`).
- `framework::ui::Context` — immediate-mode: `begin(UiInput)`, `list/button/slider/field(id, …)`,
  `end()` → квады. Id — `FNV(родитель, ключ, индекс)`: элементы списка с одинаковым ключом не
  совпадают. Фокус, нажатие и перемещение — в контексте; буферы фиксированной ёмкости, ноль
  аллокаций в кадре (тест считает).
- `UiInput` — свой канал, не `PlayerAssign`: навигация с любого пада и клавиатуры (D-pad/стик + A/B,
  стрелки, Enter, Esc) через пресет `ui` на всех устройствах сразу, плюс указатели касаний из оболочки
  в пикселях зоны (касание виджета фокусирует и активирует).
- Сетка ввода `ui::glyph_grid(id, alphabet, buffer, max_len)`: алфавит задаёт поле (IP: цифры, `.`,
  `:`; имя: A–Z; код комнаты: A–Z и цифры). Клавиатура печатает символы алфавита напрямую.
- Раскладка в пикселях зоны × k; на 4:3 HUD — в верхней полосе `viewport_fit`.
- fuzz-цель `strings`.

### В6. Музыка

- Запись `music`: бейк проверяет частоту = 48000 (иначе отказ), `0 ≤ loop_start < loop_end ≤ длина`
  в сэмплах; кладёт `.ogg` как есть, метаданные лупа и кроссфейда — секцией `LNMU`. fuzz-цель `music`.
- Стерео-голос в `Mixer`: L/R без пана, отдельный путь от моно-голоса; Fix32-голден микса стерео-
  голоса. Существующие `audio_golden`, `audio_rt_test` и звук шутера неизменны.
- `audio::MusicStream`: два кольца A/B, зарегистрированные при старте (`register_source` только вне
  RT). Рабочий поток декодирует `stb_vorbis` стерео блоками, на `loop_end` прыгает на `loop_start`
  без разрыва. Смена трека — команда консьюмеру с поколением: кольцо со старым поколением RT-колбэк
  не играет и сбрасывает, продюсер начинает новое поколение. Недобор кольца — тишина и счётчик
  `underruns`. Бюджет — два декодера одновременно максимум.
- Кроссфейд: линейный, длина из манифеста или 1 с. Третье желание во время кроссфейда: уходящий трек
  обрывается коротким спадом (10 мс), входящий становится уходящим со своей текущей громкостью,
  новый берёт освободившееся кольцо.
- `want(трек)` зовётся каждый тик симуляции. Смена — когда одно и то же желание держится
  `SETTLE_TICKS` тиков подряд, `SETTLE_TICKS = depth + Session::LEAD + 1` (при глубине 8 — 25 тиков,
  ~0.42 с): откат любой допустимой глубины желание не перевешивает. Константа называется в
  `rollback`, а не дублируется числом.
- `decode_vorbis` (моно целиком) остаётся для эффектов.

### В7. Экраны Neon Rumble, замер кадров, документация, итог

- Главное меню (соло / хотсит / титры), настройки (громкость музыки, эффектов, хаптика, тряска и
  вспышки, язык, прозрачность тача → `settings`), рекорды с вводом имени сеткой (`records`), экран
  паузы при возврате из фона и по значку паузы.
- Сообщение о карантине сейва — один раз, с путём файла.
- Строки игры en/ru в `strings.tsv`, релизный бейк strict.
- `--frame-stats` печатает p50/p99 кадра, пропуски и `underruns` за прогон с музыкой, UI и тачем;
  `scripts/frame_stats_verdict.py` судит, что все поля на месте; сценарий на телефоне владельца
  (как §34).
- Документация en/ru (`docs/guide`), runbook, сценарии `owner-verification.md`, итог DoD.

---

## Нефункциональные

- 60 FPS на самом медленном телефоне владельца: пропусков < 1% (В7). iOS 17+, Android API 29+.
- Аудио-колбэк: без локов, heap и I/O — RT-тест считает `operator new` и `pthread_mutex_lock`
  (перехват), TSan/ASan-прогон музыки в CI.
- UI: ноль аллокаций в кадре после первого.
- Сейв: запись слотов в `suspend` укладывается в окно ОС на уход в фон; время записи печатает
  `--frame-stats` (судит замер В7, а не число в спеке).
- Бюджет ассетов игры 30 МиБ с музыкой.
- Ничего из #26 не входит в снапшот и хеш драки, кроме действий `InputFrame`.

## Архитектура

```
platform/mobile/   библиотека оболочки like_nes::mobile, mobile::Game, like_nes_mobile_app(), wgpu по ABI
platform/ios/      app.mm, view.mm (landscape, safe area, фон), Core Haptics
platform/android/  native_main.cpp (insets, rotation, AInputEvent, JNI VID/PID, вибрация, intent), build_apk.sh (ABI, res)
engine/input/      RawKind/DeviceKind Touch, пад-бэкенд GameController без GLFW
engine/framework/
  touch/           TouchSource, раскладка LNTL, плавающий стик, проба владельца касания
  brawl/           dodge по последовательности InputFrame (рядом с track_run)
  ui/              Context, UiInput, виджеты, glyph_grid
  save/            заголовок LNSV, слоты, миграции, карантин, write_atomic, Backend
  haptic/          pulse → платформенный шов
engine/audio/      стерео-голос, MusicStream (кольца A/B, поколения)
engine/platform/   set_save_dir, F_FULLFSYNC в sync_file, safe_area/rotation, haptic
tools/assetc/      strings (LNST + покрытие глифов), music (LNMU), touch (LNTL)
games/neon-rumble/ mobile::Game, экраны, слоты
```

ОС-`#ifdef` — только в `engine/platform` и оболочках (инвариант #12).

## API (эскиз; уточняется вертикалью)

```cpp
namespace mobile {
struct SafeArea { float left, top, right, bottom, scale; };
enum class Touch { Down, Move, Up };
class Game {
public:
    virtual ~Game() = default;
    virtual bool init(GpuContext&) = 0;
    virtual bool attach_surface(WGPUSurface, uint32_t fb_w, uint32_t fb_h, const SafeArea&) = 0;
    virtual void detach_surface() = 0;
    virtual void resize(uint32_t fb_w, uint32_t fb_h, const SafeArea&) = 0;
    virtual void pointer(intptr_t id, Touch, float x_pt, float y_pt) = 0;
    virtual void cancel() = 0;
    virtual void frame(double dt) = 0;
    virtual void suspend() = 0;
    virtual void resume() = 0;
    virtual void shutdown() = 0;
};
std::unique_ptr<Game> make_game();
}

namespace save {
enum class Status { Ok, Empty, Migrated, MigrateFailed, Future, Corrupt };
using Migrate = bool (*)(std::vector<uint8_t>& body);
Status load(const char* slot, uint16_t version, std::span<const Migrate> chain,
            std::vector<uint8_t>& body, std::string* quarantine);
bool store(const char* slot, uint16_t version, std::span<const uint8_t> body);
bool write_atomic(const std::string& path, std::span<const uint8_t> bytes);
const std::string& dir();
}

namespace haptic {
enum class Kind { Phone, Pad };
struct Target { Kind kind; int pad_slot = -1; };
void pulse(Target, uint8_t strength, uint16_t ms);
}

namespace audio { class MusicStream { public: void want(uint32_t track); void tick(); }; }
```

## UI/UX

- Тач: стик слева (плавающий), справа пять кнопок дугой у правого края safe area, значок паузы в
  верхнем углу; размер в точках, одинаковый на телефонах разной плотности. Прозрачность — настройка.
- Подключили пад, когда тач не держит игрока, — кнопки гаснут; касание экрана возвращает их.
- Поворот на 180° — раскладка следует за вырезом, удержанные кнопки отпускаются, игра не встаёт на
  паузу.
- Битый или будущий сейв — одно сообщение на старте с путём файла карантина, игра стартует;
  повторно не показывается.
- Возврат из фона — экран паузы; убитый процесс — главное меню.
- Сетка ввода: фокус на символе, пад двигает фокус, A — ввести, B — стереть, Start — готово; тач —
  касание символа; клавиатура — печать.
- Язык меняется сразу, без перезапуска.

## Edge Cases

- Третий палец; палец, начавшийся на кнопке и ушедший на стик, — касание у того, кто принял первым.
- Системный жест / шторка / звонок → `cancel()`: все действия отпущены в том же тике.
- Поворот посреди касания — отпустить, а не перенести координаты.
- `TERM_WINDOW` без `DESTROY` (фон) и повторный `INIT_WINDOW` — та же игра, новая поверхность.
- Сейв: обрыв между temp и replace — старый файл; длина больше файла — `Corrupt`; две копии с равным
  счётчиком — локальная; второй битый слот подряд — новый `k`, прежний `.bad` цел.
- Каталог сейвов недоступен (полный диск) — игра идёт, сообщение один раз, запись повторяется на
  следующем `store`.
- Музыка: трек короче кольца, `loop_end` = длина, `loop_start` = 0; третье желание во время
  кроссфейда — правило В6.
- Откат на максимальную глубину — смены трека нет; устойчивость `SETTLE_TICKS` — есть (голден).
- Строка с глифом вне шрифта — отказ бейка; пустой перевод ru — откат на en (не пустая строка).
- Пад отключили посреди боя на мобилке — правило передачи места тачу (В2).

## Риски

| Риск | Вероятность | Влияние | Митигация |
|---|---|---|---|
| Эмулятор Android на CI нестабилен (KVM, холодный старт, flaky `sendevent`) | высокая | среднее | Кэш образа и snapshot AVD, таймаут с одной повторной попыткой, вердикт по истории касаний с нонсом |
| wgpu на эмуляторе CI без GPU (SwiftShader) не поднимается или идёт молча другим бэкендом | средняя | высокое | Бэкенд печатается и судится вердиктом; без Vulkan гейт красный, не зелёный |
| Холодная сборка wgpu-native под `x86_64-linux-android` на CI (Rust, NDK) | высокая | среднее | Кэш cargo и артефакта по пину `wgpu-pin`; время джоба замеряется в В2, оценка «8–12 мин» — после кэша |
| Раскладка тача неудобна на телефоне | средняя | высокое | Точка железа сразу после В2; раскладка — данные |
| Переезд оболочки ломает шутер или бенч ресима | средняя | среднее | Голден `0x32a094e8…`, iOS-гейт двух пальцев и бенч на симуляторе в В1 |
| `F_FULLFSYNC` замедляет запись достижений и профилей | низкая | низкое | Запись — вне кадра; время печатает `--frame-stats` |
| Потоковый декод не успевает на слабом телефоне | низкая | высокое | Кольцо ≥ 250 мс, `underruns` в `--frame-stats`, замер В7 |
| Миграция сейва теряет данные | средняя | высокое | Копия `.pre` до цепочки, карантин без перезаписи; T4 на цепочке |
| Тач вносит недетерминизм в реплей | низкая | высокое | В запись идут действия; DODGE по тикам; ресим с тач-записью = прямой прогон |
| Раунд разрастается (семь вертикалей, PR на каждую) | высокая | среднее | Каждая вертикаль — свой PR и зелёный CI; сокращение — решением владельца с правкой DoD |

## Тестовая стратегия

| Вертикаль | Гейт | Сломанная фикстура |
|---|---|---|
| Шаг 0 | `asset-licenses`, `asset-budget` | трек без записи в инвентаре |
| В1 | `sdk-game` с мобильной сборкой (префикс без шутера); голден шутера; `ios_sim_gate.sh`; фон → возврат на симуляторе | оболочка с заголовком шутера не собирается против префикса; `TERM_WINDOW` зовёт `shutdown` — состояние после возврата сброшено |
| В2 | `android_emu_gate.sh` + iOS-гейт: обе стороны landscape, `[safe]` со стороны выреза, два пальца по зонам после поворота, бэкенд wgpu, кадр не чёрный; голден формы оси; DODGE и разбег в записи = реплей; потеря пада сохраняет `use_touch`; голдены ввода #14 | второй палец теряется; safe area нулём (`shortEdges` снят); координаты без поворота; чёрный кадр; DODGE по часам; позиционный `PlayerAssign`; два `use_touch` |
| В3 | профиль по `pad_info` на фикстуре; macOS-бэкенд без GLFW собирается; `pulse` no-op без движка | имя из `productCategory` → профили слиты |
| В4 | roundtrip; fuzz `save`; цепочка v1→v3; `MigrateFailed`; карантин дважды подряд; обрыв между temp и replace (crash-тест, как у достижений); перенос старых файлов; fake-backend с двумя копиями; счётчик после карантина | CRC без заголовка (флип версии → `Future`); `.bad` перезаписан; будущая версия перезаписана; битый слот обнуляет соседний; пустой `dir()` пишет в cwd |
| В5 | голден квадов и фокуса; ноль аллокаций; покрытие глифов и алфавитов сетки; strict; коллизия FNV; fuzz `strings` | кодпоинт вне шрифта проходит; откат без счётчика; id без индекса (два элемента списка — один фокус); литерал в экране |
| В6 | стык лупа: сэмплы после `loop_end` = декод с `loop_start` той же ОС; голден стерео-голоса и стыка на синтетическом целочисленном PCM через путь стрима; кроссфейд и третье желание; откат на max глубину — смены нет; RT-тест + TSan/ASan; fuzz `music` | 44.1 кГц проходит бейк; `loop_end` > длины проходит; смена на `SETTLE_TICKS − 1`; старое поколение кольца доигрывает; мьютекс в колбэке |
| В7 | `frame_stats_verdict.py`; сценарии владельца | вывод без поля `underruns` |

Байты декода Vorbis между ОС не пинятся: `stb_vorbis` считает во float (FMA на arm64); кросс-ОС
голден — только на целочисленном PCM. Новые тесты — в `LIKE_NES_BUILD_TARGETS` и списки `ci.yml`;
вердикт — `kebab-case: PASS`; **fuzz-readers:** +`save`, +`strings`, +`music`, +`touch` в конец
списка, счётчики по правилу #24. Новые гейты (`android-emu`, покрытие глифов, литералы экранов) —
строкой в таблицу `CLAUDE.md` и файлом в `.context/gates/`, у каждого `--selftest`.

## Порядок и DoD

Шаг 0 ∥ В1 → В2 (точка железа) → В3 → В4 → В5 → В6 → В7. Каждая вертикаль — свой коммит(ы) в `dev`,
зелёный CI и **PR `dev` → `main`**.

**DoD раунда:**
1. Оболочка в SDK; Neon Rumble и шутер — потребители `mobile::Game`; фон не убивает игру; голден
   шутера неизменен.
2. Android-эмулятор и iOS-симулятор: landscape в обе стороны, safe area, два пальца, запуск игры —
   гейты зелёные, Android — в CI.
3. Тач как устройство `InputEngine`: запись с тачем = реплей; раскладка проверена владельцем на
   телефоне.
4. Пады и хаптика на обеих мобильных ОС (сценарий владельца).
5. Сейвы: roundtrip, fuzz битого файла, миграции, карантин без потерь; долг #14 `write_atomic` закрыт.
6. Строки en/ru, гейт покрытия глифов, strict в релизе; UI-голден.
7. Музыка: стык лупа, RT-safe под TSan/ASan; треки выбраны владельцем, в инвентаре и бюджете.
8. Экраны Neon Rumble; замер кадров на телефоне владельца записан, пропусков < 1%.
9. Документация en/ru, гейты docs*; сценарии `owner-verification.md` и runbook.

---

## Открытые вопросы

1. Облачные сейвы (iCloud, Play Saved Games) — follow-up с аккаунтами поверх `save::Backend`.
2. Подпись и сторы — follow-up #8.
3. Набор кнопок может измениться после точки железа В2 — правкой спеки отдельным коммитом.
4. Длина кроссфейда, порог DODGE (K тиков) и громкости по умолчанию — подбираются в В2/В6/В7.
