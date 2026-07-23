# Kickoff спеки #8: Билд/деплой + кросс-компиляция (owner-интервью 2026-07-23)

Статус: интервью пройдено, спека/ADR ещё НЕ написаны. Следующий шаг — spec #8 + ADR 0008 (Proposed),
затем PoC walking-skeleton (T4). Наследует #1–#7 (все Validated/Закрыты).

## Переформулировка
Система **сборки/деплоя**: воспроизводимые кросс-платформенные билды из исходников + ассетов
(bake target-native через assetc #5) → распространяемые артефакты (Steam-бандлы), + кросс-компиляция.
**Валидируется постройкой реальной (примитивной) игры** — догфуд движка, упражняющий #1–#7.

## Решения owner (AskUserQuestion 2026-07-23)
1. **Скоуп** — «**Оба полно (desktop + mobile)**». Desktop = приоритет №1 (Steam Linux/macOS/Windows,
   stack.md); mobile (Android/iOS) — тоже в scope (NDK/iOS SDK). NB: разработка только desktop (stack) →
   mobile-валидация = build-уровень + эмуляторы (live-железо мобил — граница, как owner-HW в #2–#7).
2. **Кросс-компиляция** — «**Research-развилка → доказать данными (как #7)**». НЕ решать reasoning'ом:
   desk-research **native per-OS CI-раннеры** (расширить текущую ci.yml matrix) **vs true cross-compile
   с одного хоста** (zig cc / clang-cross / mingw; для mobile — NDK/iOS SDK) → ADR 0008 + PoC-замер.
   Развилки-кандидаты в research (как UI/рефлексия/IPC в #7): (a) тулчейн кросс-компиляции; (b) детерм.-
   билд подход (пиннутый toolchain, `-ffile-prefix-map`, SOURCE_DATE_EPOCH, reproducible-builds практики);
   (c) упаковка/бандлинг per-OS (bundle-структура, code-signing/notarization — хотя бы дизайн).
3. **DoD (гейты walking-skeleton, owner выбрал ВСЕ 4 + игру):**
   - **Воспроизводимый/детерм. билд:** одинаковые входы → байт-идентичный артефакт (пиннутый toolchain,
     без timestamp/path-leak). Столп «оптимизация/предсказуемость».
   - **Шов assetc→билд:** ассеты бейкятся под целевую arch (#5) и бандлятся в артефакт; игра стартует
     с бейкнутыми ассетами (сквозной шов с #5).
   - **Распространяемый бандл/артефакт:** на каждую desktop-ОС — запускаемый бандл (.app/AppImage/.exe)
     с dylib/ассетами; version-stamp; готов к Steam-заливке.
   - **Release-оркестрация + CI:** release-matrix (tag → сборка всех ОС → артефакты загружены);
     version/changelog stamping.
   - **★ ОБРАЗЕЦ ИГРЫ (owner, ключевое):** собрать **реальную примитивную игру** как payload/догфуд —
     **sidescroller-shooter: очки, 1 босс, мини-сюжет, пиксельная графика.** Именно её билдим+деплоим
     кросс-платформенно. Она упражняет весь движок: render #2 (спрайты/пиксель-арт), input #4, audio #3,
     assets #5 (пиксель-текстуры/звук), ECS/sim #1 (flecs, fix32-детерм.), опц. editor #7 (авторинг сцены)
     + plugins #6. **Держать МИНИМАЛЬНОЙ** — «уже игра», но крошечная (1 уровень + 1 босс + экран очков +
     пара строк сюжета). Игра = vehicle для валидации build/deploy, не самоцель.

## Границы (предв., уточнить в спеке)
- Mobile live — эмулятор/build-уровень (нет мобил-железа у owner); desktop live — owner-HW (macOS).
- Code-signing/notarization (Apple)/Steam-SDK-интеграция — хотя бы дизайн; полная — возможно follow-up.
- Игра-образец: пиксель-арт минимальный (можно программные/плейсхолдер-спрайты или простые PNG через #5).

## Рабочий процесс (как #1–#7)
Spec #8 + ADR 0008 (Proposed) → PoC walking-skeleton (T4, коммит-per-шаг-риск, code-reviewer на каждый
шаг, ASan/UBSan, CI-гейты) → owner подтверждает live (запуск игры на desktop) → ADR Accepted / spec
Validated / README #8 Закрыта → dev-log. Модель: спека/архитектура = **Opus**; тяжёлая реализация
(игра/тулчейн) = **Fable/Sonnet** (экономия лимита). Ветка `poc/plugin-system` (продолжаем) или новая.

## Реиспользование из #1–#7
CI-матрица 3 ОС (ci.yml) · assetc bake target-native + content-hash (#5) · fix32-детерм. (#1) ·
render WebGPU (#2, wgpu-native prebuilt per-OS dylib — **важно для кросс-компиляции**: нужны бинарники
wgpu под каждую цель) · input HAL (#4) · audio (#3) · flecs ECS + scene-doc/editor (#7) · CMake
FetchContent + пиннутые версии зависимостей.
