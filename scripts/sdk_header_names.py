"""Базовые имена заголовков, которые игра видит в поставленном SDK (спека #24, В1), — для include-seam.

Источник — списки CMake, а не своя копия: она отстала бы от первого же нового публичного заголовка,
и правило по ней молчало бы о нём. `LIKE_NES_SDK_HEADERS` (`cmake/sdk_headers.cmake`) — список
установки заголовков движка. `sdk_third_party` (`cmake/install_sdk.cmake`) — белый список угловых
подключений в поставленных заголовках; он перечисляет ровно то, что ставят соседние
`install(FILES)` в `third_party/`, но сам установку не ведёт. Из него берутся только элементы без
каталога: `-I` игры смотрит в `third_party/`, и голым именем достижим лишь `glfw3webgpu.h`, а
`<webgpu/webgpu.h>` заголовок игры `webgpu.h` не затеняет.

Разбор строгий: каждая форма CMake, которую он не понимает (кавычки, `${…}`, генератор-выражение,
`list(APPEND …)`, повторный `set()`), — отказ с названием, а не тихая потеря имени.
"""

import os
import re
from pathlib import Path

# (файл, список, каталог установки для находки; None — только элементы без каталога)
LISTS = (("cmake/sdk_headers.cmake", "LIKE_NES_SDK_HEADERS", "engine/"),
         ("cmake/install_sdk.cmake", "sdk_third_party", None))
EDITS = r"(?:APPEND|PREPEND|INSERT|REMOVE_\w+|FILTER|TRANSFORM|POP_\w+|SORT|REVERSE)"
TOKEN = re.compile(r"[A-Za-z0-9_./-]+\.(?:h|hpp|inl)")


def names_in(text, var):
    """(элементы `set(<var> …)`, причина отказа или None). Команды CMake без учёта регистра."""
    code = "\n".join(line.split("#", 1)[0] for line in text.split("\n"))
    v = re.escape(var)
    if re.search(r"(?i)\blist\s*\(\s*" + EDITS + r"\s+" + v + r"(?=[\s)])", code):
        return [], f"{var} is changed by list(); the gate reads set() only"
    starts = list(re.finditer(r"(?im)^[ \t]*set[ \t]*\(\s*" + v + r"(?=[\s)])", code))
    if len(starts) != 1:
        return [], f"{var}: {len(starts)} set() commands, expected exactly one"
    end = code.find(")", starts[0].end())
    tokens = code[starts[0].end():end].split() if end >= 0 else []
    bad = [t for t in tokens if not TOKEN.fullmatch(t)]
    if end < 0 or bad:
        return [], f"{var}: unsupported element {bad[0] if bad else '(no closing paren)'}"
    return (tokens, None) if tokens else ([], f"{var} is empty")


def sdk_header_names(root):
    """({имя в нижнем регистре: путь заголовка}, [отказы]). Отказ по каждому списку отдельно:
    общий порог не заметил бы пропажу короткого списка целиком."""
    names, problems = {}, []
    for rel, var, home in LISTS:
        try:
            text = Path(root, rel).read_text(encoding="utf-8")
        except OSError as e:
            problems.append(f"{rel}: {e}")
            continue
        tokens, problem = names_in(text, var)
        if problem:
            problems.append(f"{rel}: {problem}")
        for tok in tokens:
            if home is not None:
                names[os.path.basename(tok).lower()] = home + tok
            elif "/" not in tok:
                names[tok.lower()] = "third_party/" + tok
    return names, problems
