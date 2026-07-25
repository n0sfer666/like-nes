# ADR 0008: Билд/деплой + кросс-компиляция

- **Дата:** 2026-07-23
- **Статус:** **Accepted** (2026-07-25) — validation gate закрыт: walking-skeleton build/deploy-
  вертикаль PoC (`poc/`, сессии S1–S11, T4) — все **7 гейтов зелёные**, CI 3 ОС зелёный. Owner-интервью
  пройдено (kickoff `notes/spec8-kickoff.md`), три research-развилки (A тулчейн / B детерм. / C
  упаковка) закрыты desk-research'ем + доказаны PoC. **Итог гейтов:** (1) воспр. билд P0–P2 +
  SHA-пин депов — macOS 181 арт. / Linux(Docker) 183 арт. 0 diffs, CI build-twice+cmp; (2) шов
  assetc→билд — `game.bundle` UASTC→BC7, игра стартует с бейкнутыми; (3) кросс-компиляция — desktop
  native-matrix (wall=max, не sum), mobile true-cross (iOS `aarch64-apple-ios-sim` / Android
  `aarch64-linux-android`, wgpu-native из Rust); (4) бандл per-OS — `.app`(@rpath)/tarball(`$ORIGIN`)/
  папка+DLL, macOS live из чужого cwd; (5) release — tag→matrix→per-OS артефакты+VDF+gated steamcmd
  (реальный Actions-прогон `v0.0.1-rc3`); (6) игра-образец — sidescroller-shooter (бой/босс/сюжет/
  частицы/bloom/аудио), live macOS, combat-golden `0x32a094e89eacf2f2`; (7) mobile runnable — полная
  игра (S7–S9 паритет) на iOS-симуляторе + Android-эму (Pixel 8a, реальный тач), T4. **Follow-up**
  (осознанно отложено, gated): Steam-заливка (appid), macOS notarization / Windows code-sign, mobile
  прод-подпись устройств + сторы, P3 cross-machine контейнер, полноценное mobile-аудио.
- **Контекст:** [спека #8](../specs/2026-07-23-build-deploy.md); наследует
  [ADR 0001](2026-07-18-language-and-core.md) (C++, детерминизм, fixed timestep, fix32-сим,
  пиннутый toolchain-бандл, hot-reload shared-lib), [0002](2026-07-18-render-pipeline.md)
  (**wgpu-native prebuilt per-OS dylib** — критично для кросс-компиляции), [0003](2026-07-19-asset-pipeline.md)
  (source→детерм.bake через assetc, content-hash, target-native, **байт-golden с пиннутым zstd**),
  [0004](2026-07-19-audio-subsystem.md) (miniaudio dylib), [0005](2026-07-20-input-system.md)
  (platform-native gamepad HAL), [0006](2026-07-21-plugin-system.md) (wasmtime C-API prebuilt),
  [0007](2026-07-21-ide-editor.md) (editor авторит сцену игры-образца).

## Проблема

Столп «оптимизация/предсказуемость» на уровне **артефакта**: из исходников + ассетов получить
**воспроизводимый, распространяемый Steam-бандл под каждую desktop-ОС** + кросс-компиляцию (mobile).
Три подвопроса нельзя закрывать reasoning'ом — доказать данными (как UI/рефлексия/IPC в #7):
**(A) тулчейн кросс-компиляции**, **(B) детерминированный билд**, **(C) упаковка/бандлинг per-OS**.
Инвариант: не сломать байт-golden ассетов #5 и sim-детерминизм #1. Валидируется постройкой
**реальной примитивной игры** (payload/догфуд, упражняет #1–#7).

## Решения

| Аспект | Выбор |
|---|---|
| Скоуп | **desktop полно** (Steam Linux/macOS/Windows, бандлы owner тестит на своих Linux/Win-машинах) + **mobile runnable** (Android + **iOS первый класс**, рендер+ввод) — верификация на симуляторе/эмуляторе/owner-устройствах (macOS-хост) |
| **Тулчейн кросс-компиляции** | **research✓ → desktop = native per-OS CI-matrix** (НЕ true-cross); **mobile = true-cross** раздельно (Android NDK-toolchain; iOS через `CMAKE_SYSTEM_NAME=iOS`+симулятор на macOS-хосте). **wgpu-native собирается ИЗ Rust-исходников под mobile-таргеты** (prebuilt их не даёт) [research A] |
| **Детерминированный билд** | **research✓ → per-target-triple same-host байт-идентичность** (P0–P2 флаги) **+ герметичный контейнер + SHA-пин FetchContent-депов** (P3, cross-machine stretch как гейт); **cross-OS байт-идентичность = явный non-goal** (ELF≠Mach-O≠PE, покрыто runtime sim-hash #1) [research B] |
| **Упаковка per-OS** | **research✓ → macOS `.app`** (`@rpath`, dylib в `Contents/Frameworks`) / **Linux relocatable-tarball** (`$ORIGIN` rpath) / **Windows папка + DLL рядом** (статич. CRT `/MT`); **НЕ AppImage/Flatpak** (Steam сам даёт runtime — sniper) [research C] |
| Code-signing/notarization | ad-hoc подпись локально (бесплатно); **Developer ID + `notarytool` = релиз-only gated CI-джоба** (нужен Apple Dev $99/год; skip без секретов) |
| Version-stamp | `configure_file(version.h.in)` + git-hash/branch → `CFBundleShortVersionString` / `version.txt` / SteamPipe `AppBuild.Desc` |
| Шов assetc→билд | ассеты бейкятся **target-native** (#5) → бандлятся в артефакт → игра стартует с бейкнутыми (сквозной шов #5) |
| Release-оркестрация | **tag → matrix-сборка 3 ОС → per-OS артефакты** + сгенерированные **VDF** (`app_build.vdf`/`depot_*.vdf`) + **gated steamcmd-ступень** (по депоту на ОС, beta-ветка; `default` нельзя авто-SetLive); реальная заливка — когда появится appid |
| Игра-образец | **sidescroller-shooter** (очки / 1 босс / мини-сюжет / пиксель-арт + частицы + эффекты) — payload/догфуд движка, **минимальный** (1 уровень + босс + экран очков) |

## Обоснование research-развилок (доказательно)

**A. Тулчейн → desktop native-matrix, mobile true-cross (раздельно).** Проверено на реальных
данных: prebuilt-релиз **wgpu-native** (`eliemichel/WebGPU-binaries` v0.1.0, распакован) содержит
ровно `linux/macos/windows × x86_64` + `macos-arm64` — **те же триплеты, что дают GH-раннеры**, «дыры»
для моста нет. CI-matrix идёт параллельно → wall-clock = `max(3)`, а не сумма; single-host
sequential-cross заведомо медленнее. **osxcross юридически рискован** для Steam-релиза (Xcode SLA §2.5
— только Apple-железо; osxcross сам требует самому доставать SDK). Техн. след macOS мал (1 файл `.mm`,
своих `.metal` нет — трансляция WGSL→MSL внутри prebuilt wgpu), но правовой гейт закрывает вопрос.
mingw vs MSVC: C-ABI wgpu (`webgpu.h`) снимает ABI-риск, но `windows`-раннер уже нативно собирает MSVC
без риска — менять не на что. **Android = только NDK-cross** (офиц. `android.toolchain.cmake`, нет
«нативного Android-раннера»); **iOS** — на `macos`-раннере через `CMAKE_SYSTEM_NAME=iOS` (Apple-
санкционированный тулчейн), НЕ cctools-port с Linux (та же EULA). **⚠️ У wgpu-native НЕТ prebuilt под
Android/iOS** — но owner выбрал **runnable mobile** → wgpu-native собирается **из Rust-исходников**
под `aarch64-linux-android` + `aarch64-apple-ios`(+симулятор) через cargo (штатно для этих таргетов,
больше setup — **в scope #8**). iOS = первый класс: симулятор + iPhone + iPad; Android — Pixel 8a
эмулятор. Прод-подпись устройств/сторы — follow-up. Источники: unzip
WebGPU-binaries v0.1.0, Apple Xcode SLA / Dev Forums, developer.android.com/ndk/guides/cmake, Godot
iOS-cross docs.

**B. Детерм. билд → P0–P3, per-triple same-host гейт, cross-OS non-goal.** Источники неdeterm. и
точные фиксы: `__DATE__`/`__TIME__` → `SOURCE_DATE_EPOCH` (gcc≥7/clang≥16) + `-Werror=date-time`;
build-path в `__FILE__`/debug-info → `-ffile-prefix-map` на **SOURCE и BINARY dir** (депы FetchContent
живут в `build/_deps/*-src` — маппить обязательно) + `-fdebug-compilation-dir=.` + `-no-canonical-
prefixes`; toolchain-ident → `-fno-ident`; **метаданные `.a`** (uid/gid/mtime) → детерм. `ar Dqc` +
`ranlib -D` (Ubuntu уже деф.) + `ZERO_AR_DATE=1` (macOS); build-id → `-Wl,--build-id=sha1`|`none`;
MSVC → `/Brepro` (cl+lib+link) + `/INCREMENTAL:NO` + `/PDBALTPATH` + **пин тулсета**. **ASLR/PIE —
runtime, НЕ билд** (байты на диске не трогает) → вне скоупа. clang+lld и gcc — байт-идентичны same-host
малой кровью; MSVC — «с оговорками». Cross-OS невозможен by construction. P3 (owner): SHA-пин всех
FetchContent-тегов (сейчас SHA только у `stb`; `glfw 3.4`/`webgpu main-v0.2.0`/`imgui`/`flecs`/`zstd`/
`basisu`/`miniaudio`/`glfw3webgpu` — теги/ветки, перемещаемы) + пиннутый тулчейн-контейнер → cross-
machine. Гейт: build-twice на **разных путях** + `cmp` (диагностика — diffoscope). Источники:
reproducible-builds.org (build-path/SOURCE_DATE_EPOCH/archives), LLVM blog (Deterministic builds with
clang and lld), nikhilism.com (Windows det-builds), Conan blog, Debian Wiki (Timestamps in static libs).

**C. Упаковка → .app / relocatable-tarball / папка+DLL; Steam-депот = дерево файлов.** SteamPipe
чанкает/патчит файлы — **депот не инсталлятор**, задача упаковки на всех ОС: «релокейтабельная папка,
запускается на месте», рядом — native-либы. **macOS `.app`**: dylib в `Contents/Frameworks`, линк по
`@rpath` (`LC_RPATH=@executable_path/../Frameworks`, install-name `@rpath/libwgpu_native.dylib` через
`install_name_tool`); Steam **требует** 64-bit + нотаризацию; hardened-runtime + энтайтлменты
`disable-library-validation`/`allow-dyld-environment-variables`; локально — ad-hoc (`codesign -s -`),
релиз — Developer ID ($99). **Linux**: НЕ AppImage/Flatpak (двойной bundling, конфликт с pressure-
vessel) — плоский tarball, `$ORIGIN` rpath; таргетить **Steam Linux Runtime 3.0 «sniper»**. **Windows**:
папка, DLL рядом с `.exe`, **`/MT`** (без VC-redist) — или Common Redistributables позже; инсталлятор
не нужен. Release: `app_build.vdf` (1 build → N депотов) + `depot_<os>.vdf`, **депот на ОС**, push в
beta-ветку (`default` нельзя авто-`SetLive`). Источники: partner.steamgames.com (Uploading/Platforms/
Common Redist), steamrt/sniper README, Apple Bundle docs, install_name_tool refs, CMake
BUILD_RPATH_USE_ORIGIN.

## Ключевой tradeoff — native-matrix ценой N раннеров, не единого хоста

Desktop-сборка идёт **нативно на каждой ОС** (не true-cross с одного хоста) — сознательный размен:
платим N CI-раннерами (уже есть) и отсутствием единого «собери-всё-локально», получаем **верность
нативных API/ABI** (Metal/Obj-C++, MSVC-ABI, evdev), **соответствие prebuilt-триплетам wgpu без дыр**
и **снятие правового риска osxcross**. Детерминизм при этом — per-triple (не cross-OS): предсказуемость
достигается на каждой цели отдельно + P3-контейнер для cross-machine, а cross-OS-эквивалентность
осознанно отдана runtime sim-hash-голдену #1. Mobile — runnable (рендер+ввод) через
wgpu-native из Rust-исходников; верификация на симуляторе/эмуляторе/owner-устройствах (прод-подпись
и публикация в сторы — follow-up).

## Условия финализации (validation gate) — ✅ ЗАКРЫТ (2026-07-25, T4)

Proposed → Accepted при закрытии walking-skeleton build/deploy-вертикали (как render/asset/audio/
input/plugin/IDE PoC). **Все 7 гейтов закрыты** (сессии S1–S11, CI 3 ОС зелёный). Гейты (детали —
спека #8, тест-матрица):

1. **Воспр. билд** — build-twice на разных путях → байт-идентичный артефакт (per-triple, P0–P2); P3 —
   cross-machine в пиннутом контейнере + SHA-пин депов. CI-`cmp` 3 ОС. *(развилка B)*
2. **Шов assetc→билд** — ассеты бейкятся target-native (#5) → в бандл → игра стартует с бейкнутыми.
3. **Кросс-компиляция** — desktop native-matrix (замер `max` vs sum); mobile Android-NDK + iOS-cross
   продюсят корректный таргет-артефакт (`file`/`otool`-проверка arch). *(развилка A)*
4. **Бандл per-OS** — `.app` / tarball(`$ORIGIN`) / папка+DLL с dylib+ассетами+version-stamp,
   запускается (macOS live owner-HW + CI smoke 3 ОС). *(развилка C)*
5. **Release-оркестрация** — tag → matrix → per-OS артефакты + VDF-скрипты + version/changelog; gated
   steamcmd-ступень (skip без appid).
6. **★ Игра-образец** — sidescroller-shooter собрана и запущена кросс-платформенно (упражняет #1–#7),
   live owner-HW (macOS).
7. **mobile runnable** — Android (Pixel 8a эмулятор) + iOS (симулятор + iPhone + iPad): игра
   запускается, ввод/управление работает (render+input). Проверяю запуск+управление; owner —
   глубже на своих устройствах.

## Follow-up (осознанно отложено)
- [ ] Реальная Steam-заливка (нужен appid/секреты) — VDF+steamcmd-ступень готова, gated.
- [ ] macOS notarization live (нужен Apple Dev $99) — CI-джоба спроектирована, skip без секретов.
- [ ] Windows code-signing (SmartScreen) — release-hardening, отдельно.
- [ ] Mobile прод-подпись устройств + публикация в App Store / Play — вне #8 (dev-run: симулятор/
  эмулятор/owner-устройства). Сборка wgpu-native из Rust под mobile — **в scope** (enabler S2).
- [ ] Steam Common Redistributables (dynamic CRT `/MD`) — если депы потребуют.
- [ ] MSVC байт-гейт residuals (`.lib`/PDB-таймстампы, `ducible`-патч) — если гейт «флапнет».

## Последствия
- **+** Предсказуемость на уровне артефакта (per-triple воспр. + P3 cross-machine); шов #5→бандл→запуск
  сквозной; native-matrix верен нативным API и prebuilt-триплетам; osxcross-риск снят; игра-догфуд
  упражняет весь движок; VDF-дизайн готов к Steam.
- **−** N CI-раннеров вместо единого хоста; cross-OS байт-эквивалентность отдана runtime-голдену;
  MSVC-детерминизм «с оговорками» (пин тулсета); **mobile-рендер требует сборки wgpu-native из Rust-
  исходников** под Android/iOS (больше setup, но owner выбрал runnable); notarization/Steam-заливка/
  прод-подпись устройств требуют платных аккаунтов → gated follow-up; P3-контейнер добавляет возни.
- **Open:** окончательные числа замеров (build-time native-matrix, byte-repro pass, `max` vs sum) — по
  гейтам PoC; форма version-stamp custom-target (без лишних ре-компиляций) — уточнить в S5.
