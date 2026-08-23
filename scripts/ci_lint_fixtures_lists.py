"""Фикстуры сверки списков: правило обязано падать на разъехавшейся паре и молчать на сошедшейся.

Кейс здесь — это ПАРА текстов (workflow + эталон), а не один текст, поэтому форма отличается от
остальных фикстур линтера. Диска ни один кейс не касается: чтение эталона подменяется словарём.
"""
CMAKE = """add_library(framework_physics STATIC
  body.cpp broadphase.cpp query.cpp query_index.cpp
  world.cpp)
target_link_libraries(framework_physics PUBLIC framework_core)
"""

SHELL = 'STATE_TARGETS="alpha_test beta_test gamma_test"\n'


def _flow(assignment, marker="# ci-lint: mirrors-target cmake/CMakeLists.txt framework_physics"):
    return f"""jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - name: Physics
        shell: bash
        run: |
          {marker}
          {assignment}
"""


FULL = ('LIB="engine/core/fixmath.cpp cmake/body.cpp cmake/broadphase.cpp cmake/query.cpp '
        'cmake/query_index.cpp cmake/world.cpp"')
MISSING = FULL.replace("cmake/query_index.cpp ", "")
STALE = FULL.replace("cmake/world.cpp", "cmake/world.cpp cmake/gone.cpp")
VAR_MARKER = "# ci-lint: mirrors-var scripts/g.sh STATE_TARGETS"
VAR_FULL = 'LIKE_NES_BUILD_TARGETS: "alpha_test beta_test gamma_test"'
VAR_SHORT = 'LIKE_NES_BUILD_TARGETS: "alpha_test beta_test"'
VAR_LONG = 'LIKE_NES_BUILD_TARGETS: "alpha_test beta_test gamma_test delta_test"'

REFS = {"cmake/CMakeLists.txt": CMAKE, "scripts/g.sh": SHELL}

# (правило, название, сломанная пара, починенная пара). Пара — (текст workflow, словарь эталонов).
CASES = [
    ("list-drift", "исходник цели не попал в руками написанный список",
     (_flow(MISSING), REFS), (_flow(FULL), REFS)),
    ("list-drift", "в списке остался файл, которого в цели уже нет",
     (_flow(STALE), REFS), (_flow(FULL), REFS)),
    ("list-drift", "список целей Debug-шага короче своего эталона в шелле",
     (_flow(VAR_SHORT, VAR_MARKER), REFS), (_flow(VAR_FULL, VAR_MARKER), REFS)),
    ("list-drift", "список целей Debug-шага длиннее своего эталона в шелле",
     (_flow(VAR_LONG, VAR_MARKER), REFS), (_flow(VAR_FULL, VAR_MARKER), REFS)),
    ("list-drift", "эталон переехал: путь из маркера не читается",
     (_flow(FULL), {}), (_flow(FULL), REFS)),
    ("list-drift", "цель переименована: в CMakeLists её больше нет",
     (_flow(FULL), {"cmake/CMakeLists.txt": CMAKE.replace("framework_physics STATIC",
                                                          "framework_phys STATIC")}),
     (_flow(FULL), REFS)),
    ("list-drift", "маркер стоит, а списка под ним нет",
     (_flow("echo no list here"), REFS), (_flow(FULL), REFS)),
]

# Позитивный контроль на саму сверку: набор без единого маркера обязан быть находкой, а не
# молчанием, — иначе переехавший формат комментария читается как чистый прогон.
NO_MARKER = _flow("LIB=\"cmake/body.cpp\"", "# просто комментарий, не маркер")

# Группа сверяется со СВОИМИ копиями, а не с внешним эталоном, поэтому кейс — это набор файлов,
# и живёт он в сводных фикстурах (SWEEP_CASES), а не в парных.
GROUP_MARKER = "# ci-lint: mirrors-group seam-fs"
SEAM = 'PLAT_FS="platform_fs.cpp platform_fs_posix.cpp platform_path_posix.cpp"'
SEAM_SHORT = 'TILES_FS="platform_fs.cpp platform_fs_posix.cpp"'
SEAM_TWIN = SEAM.replace("PLAT_FS", "TILES_FS")


def _group(*assignments):
    return {f"g{i}.yml": _flow(a, GROUP_MARKER) for i, a in enumerate(assignments)}


SWEEP_CASES = [
    ("list-drift", "в дереве не нашлось ни одного маркера сверки",
     {"a.yml": NO_MARKER}, {"a.yml": _flow(FULL), **REFS}),
    ("list-drift", "одна копия списка швов разъехалась с остальными",
     _group(SEAM, SEAM_SHORT), _group(SEAM, SEAM_TWIN)),
    ("list-drift", "у группы осталась единственная копия: сверять не с чем",
     _group(SEAM), _group(SEAM, SEAM_TWIN)),
]
