"""Фикстуры разбора списков SDK для include-seam: понятая форма CMake даёт имена, непонятая — отказ.

Тихая потеря имени — ровно тот дефект, ради которого разбор строгий: правило 3 по неполному списку
молчало бы о заголовке, выпавшем из него. Поэтому каждая порча обязана дать отказ, а не меньше имён.
"""
import tempfile
from pathlib import Path

from sdk_header_names import names_in, sdk_header_names

V = "LIKE_NES_SDK_HEADERS"

# (метка, текст CMake, ожидаемые элементы; None — отказ)
PARSE = [
    ("комментарий с `)`, соседний set(), SET с пробелом",
     f"set(OTHER a/x.hpp)\nSET ({V}\n  core/fixed.hpp # note (see)\n  GLFW/glfw3.h\n)\n",
     ["core/fixed.hpp", "GLFW/glfw3.h"]),
    ("элемент в кавычках", f'set({V} "core/fixed.hpp")\n', None),
    ("генератор-выражение", f"set({V} $<$<BOOL:1>:core/fixed.hpp>)\n", None),
    ("подстановка переменной", f"set({V} ${{DIR}}/fixed.hpp)\n", None),
    ("дописан list(APPEND)", f"set({V} core/fixed.hpp)\nlist(APPEND {V} core/more.hpp)\n", None),
    ("второй set()", f"set({V} core/fixed.hpp)\nset({V} core/more.hpp)\n", None),
    ("блочный комментарий в теле", f"set({V}\n#[[ старый\nсписок ]]\n core/fixed.hpp)\n", None),
    ("списка нет", "set(OTHER core/fixed.hpp)\n", None),
    ("список пуст", f"set({V})\n", None),
]

SDK_LIST = "set(LIKE_NES_SDK_HEADERS\n  framework/graphics/camera.hpp core/fixed.hpp\n)\n"
THIRD = "set(sdk_third_party webgpu/webgpu.h GLFW/glfw3.h glfw3webgpu.h)\n"


def _tree(root, third):
    (root / "cmake").mkdir()
    (root / "cmake/sdk_headers.cmake").write_text(SDK_LIST, encoding="utf-8")
    if third:
        (root / "cmake/install_sdk.cmake").write_text(THIRD, encoding="utf-8")
    return sdk_header_names(root)


def selftest(run, verbose):
    """(провалов, кейсов). run(метка, исход, verbose) — печать из набора include-seam."""
    failures = 0
    for title, text, want in PARSE:
        tokens, problem = names_in(text, V)
        ok = (problem is None and tokens == want) if want else (problem is not None and not tokens)
        failures += run(f"разбор списка SDK: {title}", ok, verbose)
        if not ok:
            print(f"         получено: {tokens}, отказ: {problem}")
    want = {"camera.hpp": "engine/framework/graphics/camera.hpp",
            "fixed.hpp": "engine/core/fixed.hpp", "glfw3webgpu.h": "third_party/glfw3webgpu.h"}
    with tempfile.TemporaryDirectory() as tmp:
        got = _tree(Path(tmp), True)
    # Сторонний заголовок с каталогом голым именем не достижим: `-I` игры смотрит в third_party/.
    failures += run("списки SDK: пути, только голые сторонние", got == (want, []), verbose)
    if got != (want, []):
        print(f"         получено: {got}")
    with tempfile.TemporaryDirectory() as tmp:
        names, problems = _tree(Path(tmp), False)
    lost = any("install_sdk.cmake" in p for p in problems)
    failures += run("списки SDK: пропал короткий список — отказ, а не меньше имён", lost, verbose)
    return failures, len(PARSE) + 2
