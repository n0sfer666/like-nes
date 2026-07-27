"""Фикстуры класса «среда раннера»: чем шаг отличается на Linux, Windows и macOS.

Сломанная половина снята с реального красного прогона и повторяет его ДОСЛОВНО — фикстура,
написанная под реализацию, проверяет реализацию, а не дефект. Правило обязано сработать на первой
и промолчать на второй. Фикстуры класса «доказательность гейтов» — в `ci_lint_fixtures_gates.py`.
"""

HEAD = """\
name: fixture
on: [push]
jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
    runs-on: ${{ matrix.os }}
    steps:
"""

LINUX_JOB = """\
name: fixture
on: [push]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
"""

CASES = (
    ("portability", "shasum на всех ОС (git-bash его не несёт — реальный отказ раунда #12)",
     HEAD + """\
      - name: Reproducible build
        shell: bash
        run: |
          shasum -a256 a.bundle > h1
          shasum -a256 b.bundle > h2
""",
     HEAD + """\
      - name: Reproducible build
        shell: bash
        run: |
          cmp -s a.bundle b.bundle || { echo "bundles differ"; exit 1; }
"""),
    ("portability", "однострочный `run:` без блока — команда лежит на самой строке run",
     HEAD + """\
      - name: Render smoke
        run: xvfb-run -a ./build/core_smoke
""",
     HEAD + """\
      - name: Render smoke
        if: runner.os == 'Linux'
        run: xvfb-run -a ./build/core_smoke
"""),
    ("portability", "`run:` на строке с дефисом — форма без `name:`, шаг легко выпадает из разбора",
     HEAD + """\
      - run: shasum -a256 build/game.bundle
""",
     HEAD + """\
      - run: cmp -s build/game.bundle build/ref.bundle
"""),
    ("portability", "команда в `with:` чужого экшена — вне тела `run:`, но выполняется так же",
     HEAD + """\
      - name: Hash via composite action
        uses: ./.github/actions/run
        with:
          cmd: shasum -a256 build/game.bundle
""",
     HEAD + """\
      - name: Hash via composite action
        uses: ./.github/actions/run
        with:
          cmd: cmp -s build/game.bundle build/ref.bundle
"""),
    ("portability", "команда ПОСЛЕ однострочного `if …; fi` — снаружи защиты, а не внутри",
     HEAD + """\
      - name: Deps and hash
        shell: bash
        run: |
          if [ "$RUNNER_OS" = "Linux" ]; then sudo apt-get install -y xorg-dev; fi
          shasum -a256 build/game.bundle
""",
     HEAD + """\
      - name: Deps and hash
        shell: bash
        run: |
          if [ "$RUNNER_OS" = "Linux" ]; then sudo apt-get install -y xorg-dev; fi
          cmp -s build/game.bundle build/game2.bundle
"""),
    ("portability", "подавление без причины не подавляет",
     HEAD + """\
      - name: Hash
        shell: bash
        run: |
          # ci-lint: allow shasum
          shasum -a256 file
""",
     HEAD + """\
      - name: Hash
        shell: bash
        run: |
          # ci-lint: allow shasum — шаг добавлен под Linux-only job спеки #14
          shasum -a256 file
"""),
    ("portability", "причина в одно слово — отписка, читается через полгода не лучше пустой",
     HEAD + """\
      - name: Hash
        shell: bash
        run: |
          # ci-lint: allow shasum ok
          shasum -a256 file
""",
     HEAD + """\
      - name: Hash
        shell: bash
        run: |
          # ci-lint: allow shasum — эталон считается только на Linux-джобе, см. спеку #14
          shasum -a256 file
"""),
    ("portability", "GNU-диалект sed -i, молча ломающийся на macOS",
     HEAD + """\
      - name: Patch version
        shell: bash
        run: |
          sed -i s/0.0.0/1.0.0/ version.txt
""",
     HEAD + """\
      - name: Patch version
        shell: bash
        run: |
          python3 -c "import pathlib; pathlib.Path('version.txt').write_text('1.0.0')"
"""),
    ("gate-downgrade", "continue-on-error на обязательном шаге (инвариант 3 спеки #12)",
     HEAD + """\
      - name: Determinism golden hash
        continue-on-error: true
        shell: bash
        run: ./build/determinism_test
""",
     HEAD + """\
      - name: Render smoke — best-effort
        continue-on-error: true
        shell: bash
        run: ./build/core_smoke
"""),
)

# Ложное срабатывание тоже дефект: линтер, ругающийся на корректный код, будет отключён первым же
# человеком, которому он помешал. Здесь — конструкции, которые обязаны проходить молча.
QUIET = (
    ("xvfb-run под рантайм-проверкой RUNNER_OS", HEAD + """\
      - name: Demo
        shell: bash
        run: |
          RUN=""
          if [ "$RUNNER_OS" = "Linux" ]; then
            RUN="xvfb-run -a"
          fi
          $RUN ./build/game --demo out
"""),
    ("apt-get в шаге, ограниченном if: runner.os", HEAD + """\
      - name: Linux deps
        if: runner.os == 'Linux'
        run: |
          sudo apt-get install -y xorg-dev
"""),
    ("`#` внутри кавычек — не начало комментария", HEAD + """\
      - name: Grep for ifdef
        shell: bash
        run: |
          grep -c '#ifdef' engine/platform/platform_io.hpp
"""),
    ("ключ `timeout-minutes:` — не вызов `timeout`", HEAD + """\
      - name: Smoke
        timeout-minutes: 5
        shell: bash
        run: ./build/core_smoke
"""),
    ("шаг из одного `uses:` — разбирать в нём нечего", HEAD + """\
      - uses: actions/checkout@v4
"""),
)
