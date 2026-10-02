#!/usr/bin/env python3
"""Инвариант: заголовок чужой цели подключается голым именем, и имя это в дереве одно.

    python3 scripts/check_include_seam.py            # гейт
    python3 scripts/check_include_seam.py --selftest # правила проверяются сломанными фикстурами

Находка 13 аудита #21: девяносто пять подключений вида `#include "../../engine/…"` поднимались
выше своей цели в чужую, и ещё двадцать одно шло каталогом (`achievements/bake.hpp`) через `-I`
родительского каталога. Такой путь обходит граф целей: файл собирается, даже если ребро
`target_link_libraries` убрали, и узнают об этом на линковке или на переезде каталога, а не на
препроцессоре. Сами пути были не прихотью — у подсистем стояли ОДНОИМЁННЫЕ заголовки
(`registry.hpp`, `sim.hpp`, `engine.hpp`), и голое имя разрешалось бы порядком `-I`, то есть
молча брало бы чужой файл. Поэтому правил два и порознь они бессмысленны:

1. `#include` путём, уходящим из своей подсистемы, запрещён: подъёмом `..` или каталогом, который
   находится не у файла и не у корня подсистемы, а где-то ещё в дереве (`-I` родителя). Подсистема —
   ближайший каталог с `CMakeLists.txt`; у файла без такого предка — его каталог. Путь, которого в
   дереве нет вовсе, — сторонняя библиотека (`backends/imgui_impl_wgpu.h`).
2. Базовые имена заголовков уникальны по дереву, без учёта регистра: на macOS и Windows
   `Scene.hpp` находится по `#include "scene.hpp"`.

Игры под `games/` собираются против поставленного SDK отдельными проектами (спека #24, В1), поэтому
у них правило 2 своё: имя уникально внутри своей игры, а не по дереву, — и
3. имя заголовка игры не совпадает с поставленным заголовком SDK: SDK подключает их голым именем
   через каталоги (`"fixed.hpp"` из physics) и при `-I` игры раньше взял бы файл игры.
"""

import os
import re
import subprocess
import sys
from pathlib import Path

import py_utf8
from cpp_text import split_code_comments
from sdk_header_names import sdk_header_names

EXTS = {".c", ".cc", ".cxx", ".cpp", ".h", ".hpp", ".inl", ".m", ".mm"}
# ПЯТАЯ копия списка корней; внешнего эталона у него нет, поэтому копии сверяются между
# собой (`scripts/check_tree_roots.py`, форма `mirrors-group`).
ROOTS = ("engine", "tools", "example_ugly_game", "platform", "docs/examples", "tests", "games")
HEADERS = {".h", ".hpp", ".inl"}
INCLUDE = re.compile(r'[ \t]*#[ \t]*include[ \t]*"([^"]+)"')
BLOCK = re.compile(r"/\*.*?\*/")

# Позитивный контроль: обход обязан видеть дерево, корни подсистем и сами подключения. Без корней
# каждый файл судился бы своим каталогом, и первое же законное `../` внутри подсистемы стало бы
# находкой; без подключений регулярка могла сломаться и молчать неотличимо от чистого дерева.
MIN_FILES = 60
MIN_CMAKE = 10
MIN_INCLUDES = 300
MIN_SDK = 100  # имён из списков SDK: меньше — правило 3 судит не тот список


def owner(path, cmake_dirs):
    """Корень подсистемы файла: ближайший предок с CMakeLists.txt, корень репозитория не в счёт."""
    here = os.path.dirname(path)
    d = here
    while d:
        if d in cmake_dirs:
            return d
        d = os.path.dirname(d)
    return here


def includes(text):
    """(строка, путь) каждого `#include "…"` вне комментариев; `\\` в пути — как `/` (MSVC).

    Путь берётся из исходника: кодовая половина `split_code_comments` гасит литерал, а вместе с ним
    и путь. По ней лишь отличается директива от её упоминания в блочном комментарии.
    """
    code = split_code_comments(text)[0].split("\n")
    out = []
    for i, raw in enumerate(text.split("\n")):
        m = INCLUDE.match(BLOCK.sub(" ", raw)) if code[i].lstrip().startswith("#") else None
        if m:
            out.append((i + 1, m.group(1).replace("\\", "/")))
    return out


def target_of(path, inc, home, files):
    """Куда из подсистемы ведёт подключение с каталогом, или None, если оно из неё не уходит."""
    if ".." in inc.split("/"):
        dest = os.path.normpath(os.path.join(os.path.dirname(path), inc)).replace(os.sep, "/")
        return None if dest.startswith(home + "/") else dest
    if "/" not in inc:
        return None
    own = {f"{os.path.dirname(path)}/{inc}", f"{home}/{inc}"}
    if own & files.keys():
        return None
    elsewhere = sorted(p for p in files if f"/{p}".endswith(f"/{inc}"))
    return ", ".join(elsewhere) or None


def scope(path):
    """Область уникальности имени: у игры — её каталог (у файла прямо в `games/` — `games`)."""
    parts = path.split("/")
    return "/".join(parts[:min(2, len(parts) - 1)]) if parts[0] == "games" else ""


def audit(files, cmake_dirs, sdk):
    """files: {путь: текст}, cmake_dirs: каталоги с CMakeLists.txt, sdk: {имя: заголовок}."""
    bad = []
    for path in sorted(files):
        home = owner(path, cmake_dirs)
        for line, inc in includes(files[path]):
            dest = target_of(path, inc, home, files)
            if dest is None:
                continue
            bad.append(f"{path}:{line}: `#include \"{inc}\"` уходит из подсистемы `{home}` в "
                       f"`{dest}` — путь обходит граф целей. Подключи голым именем и свяжи цели "
                       f"ребром target_link_libraries.")
    names = {}
    for path in files:
        if os.path.splitext(path)[1] in HEADERS:
            name = os.path.basename(path).lower()
            names.setdefault((scope(path), name), []).append(path)
            if scope(path) and name in sdk:
                game = os.path.basename(scope(path)).replace("-", "_")
                bad.append(f"{path}: имя заголовка игры совпадает с поставленным заголовком SDK "
                           f"`{sdk[name]}` — голое `#include \"{name}\"` возьмёт тот, чей -I "
                           f"раньше. Дай имени префикс игры (`{game}_{name}`).")
    for key in sorted(names):
        if len(names[key]) > 1:
            bad.append(f"{key[1]}: одно имя у {len(names[key])} заголовков "
                       f"({', '.join(sorted(names[key]))}) — голое `#include` возьмёт первый по "
                       f"порядку -I. Дай имени префикс подсистемы (`audio_engine.hpp`).")
    return bad


def guards(files, cmake_dirs, sdk, sdk_problems):
    """Отказы охранников: обход, промахнувшийся мимо дерева, обязан отличаться от чистого прогона."""
    if len(files) < MIN_FILES:
        return [f"обход дал {len(files)} файл(ов) при пороге {MIN_FILES}: он описывает не то "
                f"дерево, по которому его запустили"]
    if len(cmake_dirs) < MIN_CMAKE:
        return [f"обход нашёл {len(cmake_dirs)} CMakeLists.txt при пороге {MIN_CMAKE}: корни "
                f"подсистем неизвестны, и каждый файл судился бы своим каталогом"]
    seen = sum(len(includes(text)) for text in files.values())
    if seen < MIN_INCLUDES:
        return [f"разбор нашёл {seen} подключений при пороге {MIN_INCLUDES}: регулярка директивы "
                f"сломана, и молчит она неотличимо от чистого дерева"]
    if sdk_problems or len(sdk) < MIN_SDK:
        return [f"списки SDK не разобраны ({'; '.join(sdk_problems) or f'{len(sdk)} имён при '
                f'пороге {MIN_SDK}'}): заголовки игр сверять не с чем"]
    return []


def scan(root):
    """(исходники, каталоги с CMakeLists.txt) глазами git — та же идиома, что у check_hash_seam.py.

    Ненаписанное в индекс берётся наравне с индексом: иначе свежий одноимённый заголовок проходил
    бы локальный прогон молча и падал бы только на коммит-гейте.
    """
    out = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        capture_output=True, text=True, encoding="utf-8", check=True).stdout
    prefixes = tuple(r + "/" for r in ROOTS)
    files, cmake_dirs = {}, set()
    for name in (n for n in out.split("\0") if n):
        path = Path(name)
        if not (root / path).is_file() or not name.startswith(prefixes):
            continue
        if path.name == "CMakeLists.txt":
            cmake_dirs.add(os.path.dirname(name))
        elif path.suffix in EXTS:
            files[name] = (root / path).read_text(encoding="utf-8", errors="replace")
    return files, cmake_dirs


def gate(root):
    files, cmake_dirs = scan(root)
    sdk, sdk_problems = sdk_header_names(root)
    stop = guards(files, cmake_dirs, sdk, sdk_problems)
    if stop:
        for line in stop:
            print(f"include-seam: FAIL — {line}")
        return 1
    bad = audit(files, cmake_dirs, sdk)
    for line in bad:
        print(line)
    print(f"include-seam: {'FAIL' if bad else 'PASS'} — {len(files)} файл(ов), "
          f"{len(cmake_dirs)} подсистем(ы), находок: {len(bad)}")
    return 1 if bad else 0


def main(argv):
    py_utf8.enable()
    from check_include_seam_selftest import selftest
    if "--selftest" in argv:
        return selftest(audit, guards, scan)
    # Та же дисциплина, что у ci_lint.py: сломанное правило молчит ровно так же, как чистое дерево.
    if selftest(audit, guards, scan, verbose=False):
        print("include-seam: FAIL — сломаны сами правила, находкам верить нельзя")
        return 1
    return gate(Path(__file__).resolve().parent.parent)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
