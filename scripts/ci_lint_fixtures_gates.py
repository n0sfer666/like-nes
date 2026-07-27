"""Фикстуры класса «доказательность гейтов»: проверка, которая проходит, ничего не проверив.

Общий признак — зелёный результат при нуле выполненной работы: греп по несуществующему пути,
переменная окружения, указывающая в пустоту, шаг, который линтер не разобрал. Класс дорогой:
такой гейт не падает никогда, поэтому и не замечается.
"""
from ci_lint_fixtures import HEAD, LINUX_JOB

CASES = (
    ("env-assumption", "путь ICD в env: — форма, в которой отказ и случился (гейт 6, Ubuntu)",
     LINUX_JOB + """\
      - name: Render
        shell: bash
        env:
          VK_ICD_FILENAMES: /usr/share/vulkan/icd.d/lvp_icd.x86_64.json
        run: ./build/render_test
""",
     LINUX_JOB + """\
      - name: Render
        shell: bash
        run: |
          ICD=$(ls /usr/share/vulkan/icd.d/lvp_icd*.json 2>/dev/null | head -1)
          [ -n "$ICD" ] || { echo "no ICD"; exit 1; }
          VK_ICD_FILENAMES="$ICD" ./build/render_test
"""),
    ("env-assumption", "проверка ДРУГОГО пути не оправдывает зашитый: правило посрочное",
     LINUX_JOB + """\
      - name: Render
        shell: bash
        run: |
          [ -f /usr/bin/vulkaninfo ] || { echo "no vulkaninfo"; exit 1; }
          VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json ./build/render_test
""",
     LINUX_JOB + """\
      - name: Render
        shell: bash
        run: |
          [ -f /usr/bin/vulkaninfo ] || { echo "no vulkaninfo"; exit 1; }
          ICD=$(ls /usr/share/vulkan/icd.d/lvp_icd*.json | head -1)
          VK_ICD_FILENAMES="$ICD" ./build/render_test
"""),
    ("env-assumption", "проба на ТОЙ ЖЕ строке, но другого пути: `ls build` не проверяет ICD",
     LINUX_JOB + """\
      - name: Render
        shell: bash
        run: |
          ls build && VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json ./build/render
""",
     LINUX_JOB + """\
      - name: Render
        shell: bash
        run: |
          ls build && ls /usr/share/vulkan/icd.d/lvp_icd.x86_64.json
          VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json ./build/render
"""),
    ("vacuous-gate", "греп-гейт без доказательства, что поиск вообще что-то видит",
     LINUX_JOB + """\
      - name: No OS ifdef outside the seam
        shell: bash
        run: |
          HITS=$(grep -rn '_WIN32' engine | grep -v '^engine/platform/' || true)
          [ -z "$HITS" ] || { echo "leak"; exit 1; }
""",
     LINUX_JOB + """\
      - name: No OS ifdef outside the seam
        shell: bash
        run: |
          SEAM=$(grep -rn '_WIN32' engine/platform | wc -l)
          [ "$SEAM" -ge 5 ] || { echo "search is broken"; exit 1; }
          HITS=$(grep -rn '_WIN32' engine | grep -v '^engine/platform/' || true)
          [ -z "$HITS" ] || { echo "leak"; exit 1; }
"""),
    ("vacuous-gate", "тот же дефект в форме `if [ -n … ]` — правило не про одну запись условия",
     LINUX_JOB + """\
      - name: No banned includes
        shell: bash
        run: |
          HITS=$(grep -rl 'std::fopen' engine || true)
          if [ -n "$HITS" ]; then echo "$HITS"; exit 1; fi
""",
     LINUX_JOB + """\
      - name: No banned includes
        shell: bash
        run: |
          TOTAL=$(grep -rl 'platform::open_file' engine | wc -l)
          [ "$TOTAL" -ge 5 ] || { echo "search is broken"; exit 1; }
          HITS=$(grep -rl 'std::fopen' engine || true)
          if [ -n "$HITS" ]; then echo "$HITS"; exit 1; fi
"""),
    ("vacuous-gate", "порог считает НЕ тот поиск: постороннее число доказывает только себя",
     LINUX_JOB + """\
      - name: No banned includes
        shell: bash
        run: |
          OBJ=$(ls build/*.o | wc -l)
          [ "$OBJ" -ge 1 ] || { echo "nothing built"; exit 1; }
          HITS=$(grep -rn 'std::fopen' engine || true)
          [ -z "$HITS" ] || { echo "leak"; exit 1; }
""",
     LINUX_JOB + """\
      - name: No banned includes
        shell: bash
        run: |
          TOTAL=$(grep -rn 'platform::open_file' engine | wc -l)
          [ "$TOTAL" -ge 5 ] || { echo "search is broken"; exit 1; }
          HITS=$(grep -rn 'std::fopen' engine || true)
          [ -z "$HITS" ] || { echo "leak"; exit 1; }
"""),
    ("vacuous-gate", "нерекурсивный греп по одному файлу вакуумен ровно так же",
     LINUX_JOB + """\
      - name: No POSIX-only marker left
        shell: bash
        run: |
          HITS=$(grep -n 'POSIX-only' .github/workflows/ci.yml || true)
          [ -z "$HITS" ] || { echo "marker left"; exit 1; }
""",
     LINUX_JOB + """\
      - name: No POSIX-only marker left
        shell: bash
        run: |
          TOTAL=$(grep -c 'shell: bash' .github/workflows/ci.yml)
          [ "$TOTAL" -ge 10 ] || { echo "search is broken"; exit 1; }
          HITS=$(grep -n 'POSIX-only' .github/workflows/ci.yml || true)
          [ -z "$HITS" ] || { echo "marker left"; exit 1; }
"""),
    ("unparsed", "шаг без `uses:` и без тела: разбор промахнулся, а выглядит как чистый шаг",
     HEAD + """\
      - name: Smoke
        shell: bash
""",
     HEAD + """\
      - name: Smoke
        shell: bash
        run: ./build/core_smoke
"""),
)
