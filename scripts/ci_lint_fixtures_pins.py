"""Фикстуры класса «происхождение экшена»: чем закреплён чужой код, который исполняет раннер.

Сломанная половина повторяет строки `ci.yml` до аудита #21 A·3·8 дословно: те же экшены в трёх
других workflow уже стояли по SHA, а эта копия отстала от них.
"""
from ci_lint_fixtures import HEAD

CALLER = """\
name: fixture
on: [push]
jobs:
  shared:
"""

CASES = (
    ("unpinned-action", "сторонний экшен по тегу (ci.yml до A·3·8)",
     HEAD + """\
      - uses: actions/checkout@v4.2.2
      - name: Ninja
        uses: seanmiddleditch/gha-setup-ninja@v5
""",
     HEAD + """\
      - uses: actions/checkout@11bd71901bbe5b1630ceea73d27597364c9af683 # v4.2.2
      - name: Ninja
        uses: seanmiddleditch/gha-setup-ninja@96bed6edff20d1dd61ecff9b75cc519d516e6401 # v5
"""),
    ("unpinned-action", "SHA, укороченный до семи знаков, — тоже подвижная ссылка",
     HEAD + """\
      - name: MSVC
        uses: ilammy/msvc-dev-cmd@0b201ec
""",
     HEAD + """\
      - name: MSVC
        uses: ilammy/msvc-dev-cmd@0b201ec74fa43914dc39ae48a89fd1d8cb592756 # v1
"""),
    ("unpinned-action", "reusable workflow на уровне job: шагов нет, правило шагов его не видит",
     CALLER + """\
    uses: octo/shared/.github/workflows/build.yml@main
    secrets: inherit
""",
     CALLER + """\
    uses: octo/shared/.github/workflows/build.yml@0b201ec74fa43914dc39ae48a89fd1d8cb592756 # v1
    secrets: inherit
"""),
    ("unpinned-action", "образ docker:// по тегу, а не по дайджесту",
     HEAD + """\
      - name: Lint
        uses: docker://alpine:3.20
""",
     HEAD + """\
      - name: Lint
        uses: docker://alpine@sha256:33ceb71981b602c1a7443a53469e4dba065f7503eab3078a2d7a57a2ab987517
"""),
)

QUIET = (
    ("локальный экшен `./` — часть дерева, пиннить нечего", HEAD + """\
      - name: Run
        uses: ./.github/actions/run
"""),
)
