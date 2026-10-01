"""Фикстуры шва подключений: каждое правило обязано сработать на поломке и промолчать на починке.

Контракт тот же, что у `check_hash_seam_selftest.py`: правила проверяются синтетическим словарём
«путь → текст», охранники — своим, а обход — НАСТОЯЩИМ репозиторием, потому что берёт файлы у git.
Ключ поиска — подстрока, которую обязана назвать находка, а не её полный текст.
"""
import subprocess
import tempfile
from pathlib import Path

from sdk_header_names_selftest import selftest as names_selftest

CMAKE = {"engine/audio", "engine/core", "engine/framework", "engine/framework/physics",
         "tools/ide"}
# Заголовки, на которые ссылаются фикстуры, стоят в КАЖДОМ дереве: путь, которого в дереве нет,
# гейт считает сторонней библиотекой, и без них порчи молчали бы по чужой причине.
BASE = {p: "" for p in ("engine/core/fixed.hpp", "engine/framework/graphics/graphics_sprite.hpp",
                        "engine/achievements/ach_bake.hpp", "tools/ide/ipc/seqlock.hpp",
                        "tools/ide/compile/diagnostics.hpp")}
LEAVES = "уходит из подсистемы"
SAME = "одно имя у"
SDK_NAME = "совпадает с поставленным заголовком SDK"
SDK = {"camera.hpp": "engine/framework/graphics/camera.hpp",
       "glfw3webgpu.h": "third_party/glfw3webgpu.h"}
GAME = "games/neon-rumble/src"

# (метка, ключ в находке, сломанное дерево, починенное дерево)
CASES = [
    ("подъём в соседнюю подсистему", LEAVES,
     {"engine/audio/mixer.cpp": '#include "../core/fixed.hpp"\n'},
     {"engine/audio/mixer.cpp": '#include "fixed.hpp"\n'}),
    ("подъём через корень дерева", LEAVES,
     {"tools/ide/scene.cpp": '#include "../../engine/core/fixed.hpp"\n'},
     {"tools/ide/scene.cpp": '#include "fixed.hpp"\n'}),
    ("путь от корня дерева", LEAVES,
     {"engine/audio/mixer.cpp": '#include "engine/core/fixed.hpp"\n'},
     {"engine/audio/mixer.cpp": '#include "fixed.hpp"\n'}),
    # Вложенная цель — своя подсистема: подъём из неё к родителю уже межцелевой.
    ("из вложенной цели к родительской", LEAVES,
     {"engine/framework/physics/world.cpp": '#include "../fixed_point.hpp"\n'},
     {"engine/framework/physics/world.cpp": '#include "fixed_point.hpp"\n'}),
    # Файл без CMakeLists.txt над собой судится своим каталогом: корневой CMakeLists.txt целью
    # подсистемы не является.
    ("файл без своей цели", LEAVES,
     {"platform/mobile/game.cpp": '#include "../../engine/core/fixed.hpp"\n'},
     {"platform/mobile/game.cpp": '#include "fixed.hpp"\n'}),
    ("пробелы вокруг решётки", LEAVES,
     {"engine/audio/mixer.cpp": '  #  include "../core/fixed.hpp"\n'},
     {"engine/audio/mixer.cpp": '  #  include "fixed.hpp"\n'}),
    ("подъём, вернувшийся в чужой каталог", LEAVES,
     {"engine/audio/mixer.cpp": '#include "sub/../../core/fixed.hpp"\n'},
     {"engine/audio/mixer.cpp": '#include "sub/../fixed.hpp"\n'}),
    # Каталог относительно `-I` родителя: `-I engine/framework` у образца, `-I engine` у фаззера.
    ("каталог через -I родителя", LEAVES,
     {"example_ugly_game/draw.cpp": '#include "graphics/graphics_sprite.hpp"\n'},
     {"example_ugly_game/draw.cpp": '#include "graphics_sprite.hpp"\n'}),
    ("каталог через -I корня движка", LEAVES,
     {"tests/fuzz/targets.cpp": '#include "achievements/ach_bake.hpp"\n'},
     {"tests/fuzz/targets.cpp": '#include "ach_bake.hpp"\n'}),
    ("обратный слеш в подъёме", LEAVES,
     {"engine/audio/mixer.cpp": '#include "..\\core\\fixed.hpp"\n'},
     {"engine/audio/mixer.cpp": '#include "fixed.hpp"\n'}),
    ("блочный комментарий перед директивой", LEAVES,
     {"engine/audio/mixer.cpp": '/* c */ #include "../core/fixed.hpp"\n'},
     {"engine/audio/mixer.cpp": '/* c */ #include "fixed.hpp"\n'}),
    ("одноимённые заголовки в двух подсистемах", SAME,
     {"engine/audio/engine.hpp": "", "engine/core/engine.hpp": ""},
     {"engine/audio/audio_engine.hpp": "", "engine/core/engine.hpp": ""}),
    ("имена, различные лишь регистром", SAME,
     {"engine/audio/Scene.hpp": "", "tools/ide/scene.hpp": ""},
     {"engine/audio/audio_scene.hpp": "", "tools/ide/scene.hpp": ""}),
    ("одноимённый .inl", SAME,
     {"engine/audio/mix.inl": "", "engine/core/mix.inl": ""},
     {"engine/audio/audio_mix.inl": "", "engine/core/mix.inl": ""}),
    # Спека #24, В1: голое имя в заголовке SDK взяло бы файл игры, стоит её -I раньше.
    ("заголовок игры с именем заголовка SDK", SDK_NAME,
     {f"{GAME}/camera.hpp": ""}, {f"{GAME}/rumble_camera.hpp": ""}),
    ("имя SDK у игры другим регистром", SDK_NAME,
     {f"{GAME}/Camera.hpp": ""}, {f"{GAME}/rumble_camera.hpp": ""}),
    ("имя стороннего заголовка SDK", SDK_NAME,
     {f"{GAME}/glfw3webgpu.h": ""}, {f"{GAME}/rumble_surface.h": ""}),
    ("одноимённые заголовки внутри одной игры", SAME,
     {f"{GAME}/hud.hpp": "", f"{GAME}/ui/hud.hpp": ""},
     {f"{GAME}/hud.hpp": "", f"{GAME}/ui/ui_hud.hpp": ""}),
    ("заголовок прямо в games/", SDK_NAME, {"games/camera.hpp": ""}, {"games/games_camera.hpp": ""}),
    ("имена прямо в games/, различные лишь регистром", SAME,
     {"games/hud.hpp": "", "games/HUD.hpp": ""}, {"games/hud.hpp": "", "games/games_hud.hpp": ""}),
]

# Чистые входы: находок быть не должно ни одной.
QUIET = [
    ("подъём внутри своей подсистемы",
     {"engine/framework/physics/sub/a.cpp": '#include "../world.hpp"\n'}),
    ("файл без своей цели, путь в свой каталог",
     {"platform/mobile/game.cpp": '#include "sub/../stick.hpp"\n'}),
    ("подкаталог своей подсистемы", {"tools/ide/editor.cpp": '#include "ipc/seqlock.hpp"\n'}),
    ("подкаталог от корня подсистемы из вложенного каталога",
     {"tools/ide/editor/panel.cpp": '#include "compile/diagnostics.hpp"\n'}),
    ("чужая библиотека с каталогом", {"tools/ide/shell.cpp": '#include "backends/imgui_impl_wgpu.h"\n'}),
    ("системный заголовок", {"engine/audio/mixer.cpp": "#include <../core/fixed.hpp>\n"}),
    ("подключение в строчном комментарии",
     {"engine/audio/mixer.cpp": '// #include "../core/fixed.hpp"\n'}),
    ("подключение в блочном комментарии",
     {"engine/audio/mixer.cpp": '/*\n#include "../core/fixed.hpp"\n*/\n'}),
    ("одно имя у исходников, не у заголовков",
     {"engine/audio/test.cpp": "", "engine/core/test.cpp": ""}),
    # Игры — отдельные проекты: в одну строку -I их заголовки не попадают ни друг с другом, ни
    # с непоставленными заголовками дерева.
    ("одно имя у заголовков двух игр",
     {"games/a/src/hud.hpp": "", "games/b/src/hud.hpp": ""}),
    ("игра и непоставленный заголовок движка",
     {"games/a/src/renderer_internal.hpp": "", "engine/render/renderer_internal.hpp": ""}),
    ("имя SDK у заголовка дерева", {"engine/audio/audio_camera.hpp": "", "tools/ide/camera.hpp": ""}),
]

FULL = {f"engine/core/f{i}.cpp": '#include "a.hpp"\n' * 5 for i in range(70)}
CMAKE_FULL = CMAKE | {f"engine/sub{i}" for i in range(10)}
SDK_FULL = {f"h{i}.hpp": f"engine/core/h{i}.hpp" for i in range(120)}

# (метка, ключ в отказе, дерево, каталоги целей, имена SDK, отказы разбора списков)
GUARDS = [
    ("обход мимо дерева", "файл(ов) при пороге", {"engine/core/a.cpp": ""}, CMAKE_FULL, SDK_FULL,
     []),
    ("корни подсистем не найдены", "CMakeLists.txt при пороге", FULL, {"engine/core"}, SDK_FULL,
     []),
    ("регулярка директивы сломана", "подключений при пороге",
     {k: "#import <a.h>\n" for k in FULL}, CMAKE_FULL, SDK_FULL, []),
    ("имён SDK меньше порога", "имён при пороге", FULL, CMAKE_FULL, SDK, []),
    ("список SDK не разобран при полном счёте", "install_sdk.cmake", FULL, CMAKE_FULL, SDK_FULL,
     ["cmake/install_sdk.cmake: sdk_third_party is empty"]),
]


def _fixture_repo(root):
    """Настоящий репозиторий: обход берёт файлы у git, а не у os.walk."""
    for rel in ("engine/core/CMakeLists.txt", "engine/core/a.cpp", "engineering/b.cpp",
                "docs/examples/c.hpp", "engine/notes.md", "engine/skip.cpp", "CMakeLists.txt"):
        (root / rel).parent.mkdir(parents=True, exist_ok=True)
        (root / rel).write_text("int x;\n", encoding="utf-8")
    (root / ".gitignore").write_text("engine/skip.cpp\n", encoding="utf-8")
    subprocess.run(["git", "init", "-q"], cwd=root, check=True)
    subprocess.run(["git", "add", "-A"], cwd=root, check=True)
    (root / "engine/new.hpp").write_text("int n;\n", encoding="utf-8")


def _run(title, want, verbose):
    if verbose or not want:
        print(f"  [{'PASS' if want else 'FAIL'}] {title}")
    return 0 if want else 1


def selftest(audit, guards, scan, verbose=True):
    failures = 0
    for title, key, bad, ok in CASES:
        fired = [f for f in audit({**BASE, **bad}, CMAKE, SDK) if key in f]
        silent = [f for f in audit({**BASE, **ok}, CMAKE, SDK) if key in f]
        failures += _run(title, bool(fired) and not silent, verbose)
        if not fired:
            print("         правило промолчало на сломанной фикстуре")
        for finding in silent:
            print(f"         правило сработало на починенной фикстуре: {finding}")
    for title, tree in QUIET:
        found = audit({**BASE, **tree}, CMAKE, SDK)
        failures += _run(f"no-false-positive: {title}", not found, verbose)
        for finding in found:
            print(f"         лишняя находка: {finding}")
    # Опорный кейс охранников идёт ПЕРВЫМ: не пройди его исправное дерево — порчи ниже отбивались
    # бы чужим отказом, ничего не сказав о своём.
    failures += _run("охранники молчат на исправном дереве",
                     not guards(FULL, CMAKE_FULL, SDK_FULL, []), verbose)
    for title, key, tree, dirs, sdk, problems in GUARDS:
        refusals = guards(tree, dirs, sdk, problems)
        failures += _run(f"охранник: {title}", any(key in m for m in refusals), verbose)
    names_failures, names_total = names_selftest(_run, verbose)
    failures += names_failures
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        _fixture_repo(root)
        files, dirs = scan(root)
        want = ({"engine/core/a.cpp", "docs/examples/c.hpp", "engine/new.hpp"}, {"engine/core"})
        got = (set(files), dirs)
        failures += _run("обход: корни, расширения, цели, индекс и ненаписанное", got == want,
                         verbose)
        if got != want:
            print(f"         получено: {got}")
    total = len(CASES) + len(QUIET) + len(GUARDS) + 2 + names_total
    if verbose or failures:
        print(f"include-seam selftest: {'FAIL' if failures else 'PASS'} — "
              f"{total} кейсов, провалов: {failures}")
    return 1 if failures else 0
