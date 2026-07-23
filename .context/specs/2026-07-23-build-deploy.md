# Спецификация #8: Билд/деплой + кросс-компиляция

- **Дата:** 2026-07-23
- **Статус:** **Proposed / в работе** — owner-интервью пройдено (kickoff `notes/spec8-kickoff.md`);
  [ADR 0008](../decisions/2026-07-23-build-deploy.md) **Proposed**; три research-развилки (A тулчейн /
  B детерм.билд / C упаковка) доказаны desk-research'ем, глубину owner подтвердил. PoC-вертикаль
  (walking-skeleton, сессии S2–S11) — **открыта, T4**. Ветка `poc/plugin-system`.
- **Наследует:** [#1](2026-07-18-language-and-core.md) (C++, детерминизм, fix32-сим, пиннутый
  toolchain, hot-reload .so), [#2](2026-07-18-render-pipeline.md) (**wgpu-native prebuilt per-OS
  dylib**), [#3](2026-07-19-audio-subsystem.md) (miniaudio), [#4](2026-07-20-input-system.md)
  (input-HAL), [#5](2026-07-19-asset-pipeline.md) (**assetc target-native bake + байт-golden zstd**),
  [#6](2026-07-21-plugin-system.md) (wasmtime prebuilt), [#7](2026-07-21-ide-editor.md) (editor
  авторит сцену игры-образца).

## Контекст

Столп брифа: **кроссплатформенность (desktop + mobile) + воспроизводимые билды** («завод по сборке
игры»). Спеки #1–#7 дали язык/ядро, рендер, аудио, ввод, ассеты, плагины, IDE — но **не** механизм
получения распространяемого артефакта. #8 проектирует **build/deploy-пайплайн**: из исходников +
ассетов → воспроизводимый Steam-бандл под каждую desktop-ОС + кросс-компиляцию (mobile-scaffold).
**Валидируется постройкой реальной примитивной игры** (payload/догфуд, упражняет #1–#7).

## Ключевые решения (owner) — детали и обоснование в ADR 0008

| # | Развилка | Решение |
|---|---|---|
| 1 | Скоуп | desktop полно (Steam Linux/macOS/Windows; бандлы owner тестит на своих Linux/Win) + **mobile runnable** (Android + **iOS первый класс**, render+input; симулятор/эмулятор/owner-устройства). |
| 2 | **Тулчейн кросс-компиляции** | **research✓ → desktop native per-OS CI-matrix**; **mobile true-cross** (Android NDK / iOS `CMAKE_SYSTEM_NAME`+симулятор на macOS-хосте). НЕ osxcross. **wgpu-native из Rust-исходников под mobile** (prebuilt нет). |
| 3 | **Детерм. билд** | **research✓ → per-triple same-host байт-идентичность (P0–P2) + герметичный контейнер + SHA-пин депов (P3)**; cross-OS = non-goal. |
| 4 | **Упаковка per-OS** | **research✓ → macOS `.app`(@rpath) / Linux tarball(`$ORIGIN`) / Windows папка+DLL(`/MT`)**; НЕ AppImage/Flatpak. |
| 5 | Code-sign/notarize | ad-hoc локально; Developer ID + `notarytool` = релиз-only gated джоба. |
| 6 | Version-stamp | `configure_file` + git-hash → Info.plist / version.txt / SteamPipe Desc. |
| 7 | Release | tag → matrix → per-OS артефакты + VDF-скрипты + gated steamcmd (beta-ветка); заливка = когда appid. |
| 8 | Игра-образец | sidescroller-shooter (очки/1 босс/мини-сюжет/пиксель-арт+частицы+эффекты), минимальная. |

## Требования

### Функциональные (surface #8)

- **Воспроизводимый билд:** одинаковые входы → байт-идентичный артефакт (per-target-triple). Флаги
  P0–P2 в CMake; P3 — SHA-пин FetchContent-депов + пиннутый тулчейн-контейнер (cross-machine).
- **Шов assetc→билд:** assetc (#5) бейкает ассеты target-native → бандлятся в артефакт; игра стартует
  с бейкнутыми ассетами (не с source).
- **Кросс-компиляция:** desktop — native-matrix (расширение текущей `ci.yml`); mobile — Android
  NDK (arm64-v8a) → APK, iOS `CMAKE_SYSTEM_NAME=iOS`(+симулятор) → `.app`/`.ipa`. **wgpu-native
  собирается из Rust-исходников под оба mobile-таргета** (prebuilt нет) → игра запускаема (render+input).
- **Бандл per-OS:** `.app` (Contents/MacOS+Frameworks+Resources, Info.plist, @rpath) / relocatable-
  tarball (`$ORIGIN` rpath, .so рядом) / Windows-папка (.exe + DLL рядом, `/MT`); version-stamp внутри.
- **Release-оркестрация:** GitHub Actions на tag `vX.Y.Z` → matrix-сборка 3 ОС → per-OS бандл-
  артефакты (upload) + version/changelog stamping; генерация VDF (`app_build`/`depot_<os>`) + gated
  steamcmd-ступень (skip без секретов/appid).
- **Игра-образец (payload):** 1 уровень sidescroller-shooter, управляемый корабль, враги, снаряды,
  коллизии, счёт, 1 босс, мини-сюжет (интро/аутро), экран очков, частицы (выхлоп/взрывы/попадания),
  эффекты (bloom #2), звук+музыка (#3). Спрайты — программные пиксель-арт-плейсхолдеры (узнаваемые
  силуэты, не примитивы), бейкнутые через #5.

### Нефункциональные (инварианты — нарушение = баг)

1. **Детерминизм не нарушен:** build-флаги не трогают runtime-логику; sim игры-образца детерминирован
   (fix32 #1), sim-hash-реплей воспроизводится; байт-golden ассетов #5 цел (bake не сломан бандлингом).
2. **Артефакт самодостаточен:** бандл запускается на чистой целевой ОС без установки зависимостей
   (dylib/DLL/.so рядом, rpath корректен; Windows — статич. CRT).
3. **Кросс-платформенный паритет:** тот же исходник/ассеты → рабочая игра на всех 3 desktop-ОС
   (поведение, не байты).
4. **Изоляция сборки:** падение сборки одной цели в matrix не валит остальные (`fail-fast: false`).

## Тест-матрица (walking-skeleton, T4) — гейты валидации

| # | Гейт | Что доказывает | Проверка |
|---|---|---|---|
| 1 | Воспр. билд | build-twice на разных путях → `cmp` идентичен (P0–P2); P3 — cross-machine контейнер + SHA-пин | CI 3 ОС + `cmp`/diffoscope |
| 2 | Шов assetc→билд | ассеты target-native в бандле; игра стартует с бейкнутыми | запуск бандла читает `assets.pak` |
| 3 | Кросс-компиляция | desktop `max` vs sum замер; mobile arm64-ELF/Mach-O корректны | `file`/`otool`/`vtool` арх-проверка |
| 4 | Бандл per-OS | `.app`/tarball/папка запускается, самодостаточен | macOS live owner-HW + CI smoke 3 ОС |
| 5 | Release | tag → per-OS артефакты + VDF + version-stamp | throwaway-tag продюсит артефакты |
| 6 | ★ Игра-образец | собрана+запущена кросс-платформенно, упражняет #1–#7 | live owner-HW (macOS) |
| 7 | mobile runnable | Android Pixel 8a эмулятор + iOS симулятор/iPhone/iPad: запуск + управление работают (render+input) | live-run; owner — глубже на устройствах |

## План сессий (walking-skeleton: тонкая игра → пайплайн → утолщение)

Одна сессия = один гейт = один коммит (verify/review-state перед коммитом). Прогресс —
`notes/spec8-progress.md`.

- **S1** ✅ Дизайн: spec #8 + ADR 0008 (Proposed) — этот документ.
- **S2** Скелет игры (payload) + **кросс-платформенный bring-up**: flecs+fix32 fixed-timestep,
  корабль, ввод #4, спрайт-рендер #2, плейсхолдер-спрайты #5 + **сборка wgpu-native из Rust под
  mobile**. **T4: запуск + управление на macOS(live) / iOS(симулятор+iPhone+iPad) / Android(Pixel 8a
  эмулятор); Linux+Windows бандлы — owner тестит на своих машинах.** Внутренне может делиться:
  S2a desktop(macOS live), S2b mobile bring-up. Я проверяю запуск+управление, дальше owner сам.
- **S3** Гейт 1 — воспр. билд (P0–P3): флаги + SHA-пин + контейнер, CI build-twice+cmp.
- **S4** Гейт 3 — кросс-компиляция: native-matrix замер + mobile NDK/iOS build.
- **S5** Гейт 4+2 — бандл per-OS + шов assetc→билд (dylib+ассеты+version-stamp).
- **S6** Гейт 5 — release-оркестрация (tag→matrix→артефакты+VDF).
- **S7** Игра: бой (снаряды, враги, коллизии, счёт HUD). → T3.
- **S8** Игра: босс + мини-сюжет + экран очков. → T3.
- **S9** Игра: частицы + bloom #2 + аудио #3. → T3.
- **S10** Гейт 7 — полная игра на mobile: пере-прогон утолщённой игры (S7–S9) на Android-эмуляторе +
  iOS-симуляторе/owner-устройствах; packaging/подпись для owner-устройств. (Базовый bring-up — в S2.)
- **S11** Финализация: ADR Accepted / spec Validated / README #8 Закрыта / dev-log.

## Границы (осознанные, → follow-up в ADR 0008)

- Реальная Steam-заливка (нужен appid) — VDF+steamcmd gated, skip без секретов.
- macOS notarization live (Apple Dev $99), Windows code-signing — release-hardening, gated/отдельно.
- Mobile прод-подпись устройств + публикация в App Store / Play — вне #8 (dev-run: симулятор/
  эмулятор/owner-устройства). Сборка wgpu-native из Rust под mobile — **в scope** (enabler S2).
- Live iOS-устройства — через owner-подпись (у owner iPhone/iPad+macOS); Android — Pixel 8a эмулятор.
