# like-nes — правила репозитория

Контекст проекта: [`.context/`](.context/) (README, stack, architecture, conventions, specs,
decisions, notes). Читать перед началом любой задачи. Стиль кода —
[`.context/conventions.md`](.context/conventions.md).

## Ветки

| Ветка | Роль | Правила |
|---|---|---|
| `main` | Дефолтная, релизная. Отражает подтверждённое состояние движка. | Прямые коммиты **запрещены**. Только merge из PR. |
| `dev` | Рабочая: код, PoC, проверка гипотез, промежуточные коммиты. | Коммитим сюда. Может быть шумной. |

Фича-ветки (`feat/*`, `fix/*`, `poc/*`) — по желанию, от `dev`, вливаются в `dev`.
Долгоживущих параллельных веток не заводить: одна линия `dev` → `main`.

## Раунд → PR

**Раунд** = завершённая веха: закрытая спека из roadmap, отдельная вертикаль PoC,
или согласованный блок работ с зафиксированным DoD.

После **каждого раунда** — обязательно PR `dev` → `main`:

1. Проверки пройдены, CI зелёный на всех 3 ОС (`.github/workflows/ci.yml`).
2. `.claude/.verify-state.json` и `.claude/.review-state.json` — свежие, `result: pass`.
3. `gh pr create --base main --head dev` — заголовок в стиле conventional commits,
   тело: что сделано / DoD и как проверено / ссылки на спеку и ADR.
4. Merge (по умолчанию `--squash` не использовать — история раундов линейная и осмысленная,
   merge-commit сохраняет границу раунда).
5. После merge — `git checkout dev && git merge main` (синхронизация), продолжаем работу.

Промежуточные коммиты внутри раунда в `main` не попадают отдельно — только через PR раунда.

## Ручная проверка владельцем (правило 18 глобального CLAUDE.md)

Владелец гоняет живое железо сам: **macOS**, **Linux** (Nobara, evdev, X11 и Wayland) и
**Windows** (MSVC, XInput) — плюс реальные пад, мышь, трекпад, наушники. VM тут не предлагать:
половина находок этих раундов (Wayland-поверхность, знак сырой оси Y, шум покоящегося XInput)
недостижима ни на раннере, ни в виртуалке.

Артефакты этой проверки — **код задачи, а не документация «потом»**:
[`docs/owner-verification.md`](docs/owner-verification.md) (сценарии гейтов, каждый закрытый — с
баннером `Closed` и уликами), [`docs/owner-setup.txt`](docs/owner-setup.txt) (чистая машина →
прогон, с ОЖИДАЕМЫМ выводом построчно), [`docs/first-run.md`](docs/first-run.md),
[`scripts/win-dev.bat`](scripts/win-dev.bat) (обёртка, владеющая vcvars),
`scripts/owner_check.sh` и `scripts/gate8_e2e.sh`. Меняется поведение или имя цели — процедура
обновляется **в том же коммите**: устаревший сценарий не падает, он молча проверяет вчерашний код.
Так уже было — `./build/game_sidescroller` в runbook'е без шага сборки этой цели.

## Гейты

Устройство каждого гейта, история его находок и позитивный контроль живут своим файлом в
[`.context/gates/`](.context/gates/). **Задача трогает гейт — открыть его файл до кода.**

Сюда идёт только то, что нужно в каждом запросе: этот файл грузится в КАЖДЫЙ запрос. Описания
гейтов, выросшие в нём до 211 KB (≈ 60k токенов), к 2026-09-15 оставляли на работу ~35k контекста —
авто-компакт каждые 10–30 запросов. **Описание нового гейта пишется в `.context/gates/<гейт>.md`, а
сюда — одна строка таблицы.**

Перед КАЖДЫМ коммитом обязательны `types` и `lint` из [`.context/checks.json`](.context/checks.json):
их читает pre-commit hook, и без зелёного прогона коммит запрещён. Сборка — ноль ошибок и ноль
предупреждений; предупреждение не глушится `-Wno-…`, скрипты зовутся через `bash`, а не по exec-биту.

| гейт | команда | когда | файл |
|---|---|---|---|
| сборка без предупреждений | `bash scripts/build_check.sh` | коммит | [build](.context/gates/build.md) |
| линтер workflow | `python3 scripts/ci_lint.py` | коммит | [ci-lint](.context/gates/ci-lint.md) |
| бюджет длины файлов | `python3 scripts/line_budget.py` | коммит | [line-budget](.context/gates/line-budget.md) |
| кодировка на швах процесса | `python3 scripts/check_py_utf8.py` | коммит | [py-utf8](.context/gates/py-utf8.md) |
| выбор bash на швах процесса | `python3 scripts/posix_bash_selftest.py` | preflight | [posix-bash](.context/gates/posix-bash.md) |
| документация en/ru | `bash scripts/check_docs.sh` | коммит | [docs](.context/gates/docs.md) |
| код в документации | `bash scripts/check_docs_snippets.sh` · `bash scripts/check_docs_examples.sh <каталог>` | коммит · preflight | [docs-examples](.context/gates/docs-examples.md) |
| установка на чистой машине | `bash scripts/check_docs_start.sh [--live]` | коммит · `--live` руками | [docs-start](.context/gates/docs-start.md) |
| список ручных гейтов | `bash scripts/check_owner_gates.sh` | коммит | [owner-gates](.context/gates/owner-gates.md) |
| константы FNV в примитивах | `python3 scripts/check_hash_seam.py` | коммит | [hash-seam](.context/gates/hash-seam.md) |
| файловый ввод-вывод за швом | `python3 scripts/check_fs_seam.py` | коммит | [fs-seam](.context/gates/fs-seam.md) |
| инварианты дерева, копии признака обхода | `bash scripts/tree_invariants.sh` · `python3 scripts/check_tree_roots.py` | коммит | [preflight](.context/gates/preflight.md) |
| релизный пакет | `bash scripts/check_release.sh` | preflight | [release](.context/gates/release.md) |
| пакет Linux с машины macOS | `bash scripts/check_release_container.sh [--live]` | preflight · `--live` руками | [release-container](.context/gates/release-container.md) |
| пакет Windows задачей CI | `bash scripts/check_release_ci.sh [--live]` | preflight · `--live` руками | [release-ci](.context/gates/release-ci.md) |
| образ macOS | `bash scripts/check_release_dmg.sh` | preflight | [release-dmg](.context/gates/release-dmg.md) |
| образ Linux | `bash scripts/check_release_appimage.sh` | preflight | [release-appimage](.context/gates/release-appimage.md) |
| установщик Windows | `bash scripts/check_release_msi.sh` | preflight | [release-msi](.context/gates/release-msi.md) |
| пакет Windows без VC++ Redistributable | `bash scripts/check_release_crt.sh` | preflight | [release-crt](.context/gates/release-crt.md) |
| всё до CI | `bash scripts/preflight.sh` | перед push | [preflight](.context/gates/preflight.md) |
| аудит чужим компилятором | `python3 scripts/tu_sweep.py <каталог>` | руками | [tu-sweep](.context/gates/tu-sweep.md) |
| прогоны CI коммита | `bash scripts/ci_watch.sh [<sha>]` | после push | [ci-watch](.context/gates/ci-watch.md) |

У каждого гейта своя самопроверка на сломанных фикстурах (`--selftest` или `*_selftest.*`) — её
команды и число контролей в файле гейта.

## Коммиты

Conventional commits, английский: `feat(scope): …`, `fix(ci): …`, `docs: …`.
Без AI-подписей и `Co-Authored-By`. Не коммитить код, не прошедший линтер/типы/сборку.

## Push (переопределяет правило 11 глобального CLAUDE.md)

**`git push` в `dev` и фича-ветки — без спроса**, сразу после проверенного коммита. Решение
владельца от 2026-08-10: в этом репозитории единственный способ узнать про MSVC `/W4`, поведение
git-bash на Windows-раннере и lavapipe — прогон CI, а ожидание апрува на push просто отодвигает
находку на несколько часов, ничего не защищая. Проверенный коммит без прогона — это незаконченная
проверка, а не готовая работа.

Границы остаются: **push в `main` запрещён** (только merge из PR раунда), `--force`/`--force-with-lease`
по чужой истории — по-прежнему с явного разрешения, и правило 12 сильнее этого разрешения —
непроверенное не пушится, потому что не коммитится.
