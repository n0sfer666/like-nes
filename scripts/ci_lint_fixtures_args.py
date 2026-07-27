"""Фикстуры класса «форма аргумента»: что шелл шага делает с командной строкой по дороге.

Отделены от «среды раннера» (`ci_lint_fixtures.py`) не по длине, а по вопросу: там команды нет на
этой ОС, здесь команда есть, доезжает исковерканной и жалуется не на то. Сломанная половина снята
с реального красного прогона и повторяет его ДОСЛОВНО.
"""
from ci_lint_fixtures import HEAD

CASES = (
    ("arg-mangling", "MSVC-флаг слэшем в git-bash: `/DWIN32` доезжает путём (реальный отказ гейта #7)",
     HEAD + """\
      - name: ASan on Windows
        if: runner.os == 'Windows'
        shell: bash
        run: |
          cmake -S . -B build-asan -G Ninja \\
            -DCMAKE_C_FLAGS="/DWIN32 /D_WINDOWS -fsanitize=address" \\
            -DCMAKE_CXX_FLAGS="/DWIN32 /D_WINDOWS /EHsc -fsanitize=address"
""",
     HEAD + """\
      - name: ASan on Windows
        if: runner.os == 'Windows'
        shell: bash
        run: |
          cmake -S . -B build-asan -G Ninja \\
            -DCMAKE_C_FLAGS="-DWIN32 -D_WINDOWS -fsanitize=address" \\
            -DCMAKE_CXX_FLAGS="-DWIN32 -D_WINDOWS -EHsc -fsanitize=address"
"""),
    ("arg-mangling", "флаг с подчёркиванием и двоеточием — единственный на строке, прикрыть некому",
     HEAD + """\
      - name: MSVC build
        if: runner.os == 'Windows'
        shell: bash
        run: |
          clang-cl /std:c++20 /D_CRT_SECURE_NO_WARNINGS /bigobj main.cpp
""",
     HEAD + """\
      - name: MSVC build
        if: runner.os == 'Windows'
        shell: bash
        run: |
          clang-cl -std:c++20 -D_CRT_SECURE_NO_WARNINGS -bigobj main.cpp
"""),
    ("arg-mangling", "шелл из `defaults.run.shell` job: у шага своего `shell:` нет",
     """\
name: fixture
on: [push]
jobs:
  build:
    runs-on: windows-latest
    defaults:
      run:
        shell: bash
    steps:
      - name: MSVC build
        run: |
          clang-cl /W4 /EHsc main.cpp
""",
     """\
name: fixture
on: [push]
jobs:
  build:
    runs-on: windows-latest
    defaults:
      run:
        shell: pwsh
    steps:
      - name: MSVC build
        run: |
          clang-cl /W4 /EHsc main.cpp
"""),
)

QUIET = (
    ("POSIX-путь в git-bash — не MSVC-флаг: подмену ловим по форме токена", HEAD + """\
      - name: ASan on Windows
        if: runner.os == 'Windows'
        shell: bash
        run: |
          export PATH="/c/Program Files/LLVM/bin:$PATH"
          command -v clang-cl >/dev/null || exit 1
          ls "$(clang -print-resource-dir)/lib" 2>/dev/null | head -5
"""),
    ("слэш-форма вне bash: в pwsh MSYS-рантайма нет, флаг доезжает как написан", HEAD + """\
      - name: MSVC build
        if: runner.os == 'Windows'
        shell: pwsh
        run: |
          cl /W4 /WX /EHsc /nologo main.cpp
"""),
    ("выражение GHA перед слэшем: `}}/Vulkan` — путь, а не флаг", HEAD + """\
      - name: Vulkan SDK
        if: runner.os == 'Windows'
        shell: bash
        run: |
          mkdir -p "${{ runner.temp }}/Vulkan"
          cp -r "${{ github.workspace }}/Release" "${{ runner.temp }}/Vulkan"
"""),
    ("флаги в `with:` чужого композита: шелл объявляет он, а не наш job", HEAD + """\
      - name: MSVC build via composite action
        if: runner.os == 'Windows'
        uses: ./.github/actions/run
        with:
          cmd: cmake -S . -B build -DCMAKE_CXX_FLAGS="/DWIN32 /EHsc"
"""),
)
