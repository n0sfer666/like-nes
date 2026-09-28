"""Фикстуры вакуумного гейта на поиске, отличном от `grep`/`rg` (аудит #21 B11).

Промах пути делает вечно зелёным и `find`, и `awk`, и `ls` — ровно как греп. Обратная форма,
проверка наличия (`[ -n "$E" ] || exit`), громкая: пустой результат роняет шаг, и в workflow таких
десятки, поэтому она стоит здесь чистой фикстурой, а не только сломанной.
"""
from ci_lint_fixtures import LINUX_JOB

CASES = (
    ("vacuous-gate", "гейт на `find`: опечатка в корне даёт пустой список и зелёный шаг",
     LINUX_JOB + """\
      - name: No merge leftovers
        shell: bash
        run: |
          HITS=$(find engine -name '*.orig')
          [ -z "$HITS" ] || { echo "$HITS"; exit 1; }
""",
     LINUX_JOB + """\
      - name: No merge leftovers
        shell: bash
        run: |
          TOTAL=$(find engine -name '*.cpp' | wc -l)
          [ "$TOTAL" -ge 100 ] || { echo "search is broken"; exit 1; }
          HITS=$(find engine -name '*.orig')
          [ -z "$HITS" ] || { echo "$HITS"; exit 1; }
"""),
    ("vacuous-gate", "гейт на `awk` в форме `if [ -n … ]`",
     LINUX_JOB + """\
      - name: No raw fopen
        shell: bash
        run: |
          HITS=$(awk '/std::fopen/' engine/core/*.cpp)
          if [ -n "$HITS" ]; then echo "$HITS"; exit 1; fi
""",
     LINUX_JOB + """\
      - name: No raw fopen
        shell: bash
        run: |
          TOTAL=$(awk '/platform::open_file/' engine/core/*.cpp | wc -l)
          [ "$TOTAL" -ge 1 ] || { echo "search is broken"; exit 1; }
          HITS=$(awk '/std::fopen/' engine/core/*.cpp)
          if [ -n "$HITS" ]; then echo "$HITS"; exit 1; fi
"""),
    ("vacuous-gate", "гейт на `ls` в форме `[ -n … ] && провал`",
     LINUX_JOB + """\
      - name: No core dumps
        shell: bash
        run: |
          LEFT=$(ls build/core.* 2>/dev/null)
          [ -n "$LEFT" ] && { echo "$LEFT"; exit 1; }
          true
""",
     LINUX_JOB + """\
      - name: No core dumps
        shell: bash
        run: |
          ALL=$(ls build | wc -l)
          [ "$ALL" -ge 1 ] || { echo "search is broken"; exit 1; }
          LEFT=$(ls build/core.* 2>/dev/null)
          [ -n "$LEFT" ] && { echo "$LEFT"; exit 1; }
          true
"""),
)

QUIET = (
    ("проверка наличия после `find`: пустой результат роняет шаг", LINUX_JOB + """\
      - name: Redact
        shell: bash
        run: |
          E=$(find build -maxdepth 2 -type f -name 'platform_redact_test' | head -1)
          [ -n "$E" ] || { echo "not built"; exit 1; }
          "$E"
"""),
    ("две проверки наличия цепочкой `&& … ||`", LINUX_JOB + """\
      - name: Play
        shell: bash
        run: |
          T=$(find build -maxdepth 2 -type f -name 'play_spawn_test' | head -1)
          G=$(find build -maxdepth 2 -type f -name 'game_child' | head -1)
          [ -n "$T" ] && [ -n "$G" ] || { echo "not built"; exit 1; }
          "$T" "$G"
"""),
)
