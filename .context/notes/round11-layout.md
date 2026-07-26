# Раунд #11 — рабочий скретчпад (переезд репозитория)

Спека: [`specs/2026-07-26-repo-layout.md`](../specs/2026-07-26-repo-layout.md). Уровень T3.

## Допущения, не зафиксированные в спеке явно

| # | Развилка | Решение | Почему |
|---|---|---|---|
| 1 | `poc/assets/src/` (hero_albedo.png, hero_normal.png, sprite.wgsl) — в целевом дереве спеки такого узла нет | → `tools/assetc/assets/src/` | Единственный потребитель — `assetc` (демо-бейк, пути через argv). В `engine/asset/` (рантайм) им не место по решению #2 спеки |
| 2 | `poc/audio_seam.wav` (384 КБ) — закоммичен, но это вывод `audio_seam` в cwd | Удалить из индекса вместе с корневым мусором | Тот же класс, что `syn.bundle`: артефакт локального прогона гейта. Гейт 4 требует пустой `git status` после прогона |
| 3 | Таргет `poc` (`src/main.cpp`, webgpu-smoke спеки #1) | → `core_smoke`, строки `[poc]` → `[core-smoke]` | Требование «имя `poc` не встречается ни в одном отслеживаемом файле» |
| 4 | `engine/framework/` — пустой по спеке | `.gitkeep`, без `add_subdirectory` | git не хранит пустые каталоги; собирать нечего до #14 |
| 5 | `poc/*` как **префикс ветки** в CLAUDE.md / CONTRIBUTING.md | Оставляем | Это не путь, а имя ветки «proof of concept»; переезд его не касается |
| 6 | Каталог сборки `poc/build` | → `build` | Уже покрыт `.gitignore` (`build*/`) |

## Порядок коммитов

1. `chore: drop built artifacts from the index, extend .gitignore` — только `git rm --cached` + `.gitignore`.
2. `refactor(layout): move the tree to engine/ tools/ platform/ example_ugly_game/` — **только** `git mv`, ноль правок содержимого. Репозиторий на этом коммите не собирается — это ожидаемо и есть цена доказуемости.
3. `build(cmake): split CMakeLists per module, extract tools/assetc` — корневой + per-module, правка относительных `#include`, поехавших от смены глубины.
4. `ci: point workflows, scripts and packaging at the new layout`.
5. `docs(context): close round #11` — ADR + roadmap.

## Проверка целостности переезда

`blobs-before.txt` (в скретчпаде сессии) — `git ls-files -s -- poc`, 265 записей: blob-SHA каждого файла до переезда. После `git mv` те же blob-SHA обязаны совпасть пофайлово — это сильнее, чем sha256 одного `game.bundle`.

Опорные sha256 (гейт 6):
- `game.bundle` = `f5dcf1e350b96f8277f7687ee8eeec2e9eb4a34d99dc028d66a8dfecb3cbc6dd`
- `audio.bundle` = `1b54d2656f92a5cf8ca788953a8fd4efd721da80dd44c569b2ef060d9fd90bd9`
