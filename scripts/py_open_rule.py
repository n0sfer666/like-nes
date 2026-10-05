"""Третье правило гейта кодировки: файл, открытый в ТЕКСТОВОМ режиме, называет кодировку явно.

`open` без `encoding` берёт её у локали, и на русской Windows это cp1251: прогон владельца
2026-10-03 умер `UnicodeDecodeError` на первом же UTF-8 файле с кириллицей, и пять таких мест
нашлись руками. Это тот же шов, что второе правило судит у `subprocess`, только со стороны файла.

Судятся все входы, у которых локаль — значение по умолчанию: встроенный `open`, `io.open`,
`os.fdopen`, `read_text`/`write_text`, `.open()` у пути pathlib, `tempfile` с текстовым режимом и
`io.TextIOWrapper`. Произвольный `.open()` не судится: у `gzip`, `tarfile` и `zipfile` он бинарный
по умолчанию, и правило отбивало бы их за чужую семантику, поэтому `.open()` судится, только когда
получатель — путь pathlib по построению. Режим, не записанный литералом, считается текстовым:
правило, которое верит тому, чего не видит, обходится переменной вместо строки. `encoding=None` —
та же локаль, названная вслух, и засчитывается как отсутствие.
"""
import ast
from collections import namedtuple

# mode — индекс позиционного режима (None — режима нет, вход всегда текстовый); binary — режим по
# умолчанию бинарный; encoding — индекс позиционной кодировки.
Spec = namedtuple("Spec", "mode binary encoding")
SPECS = {
    "open": Spec(1, False, 3),
    "path_open": Spec(0, False, 2),
    "read_text": Spec(None, False, 0),
    "write_text": Spec(None, False, 1),
    "tempfile": Spec(0, True, 2),
    "TextIOWrapper": Spec(None, False, 1),
}
MODULE_CALLS = {("io", "open"): "open", ("os", "fdopen"): "open",
                ("io", "TextIOWrapper"): "TextIOWrapper",
                ("tempfile", "NamedTemporaryFile"): "tempfile",
                ("tempfile", "TemporaryFile"): "tempfile"}
BARE_CALLS = {"open": "open", "TextIOWrapper": "TextIOWrapper",
              "NamedTemporaryFile": "tempfile", "TemporaryFile": "tempfile"}


def _path_class(node):
    return ((isinstance(node, ast.Name) and node.id == "Path")
            or (isinstance(node, ast.Attribute) and node.attr == "Path"
                and isinstance(node.value, ast.Name) and node.value.id == "pathlib"))


def _pathish(node, names):
    """Выражение, которое по построению — путь pathlib: `Path(…)`, его `.parent`, методы, `/`."""
    if isinstance(node, ast.Call):
        return _path_class(node.func) or (isinstance(node.func, ast.Attribute)
                                          and _pathish(node.func.value, names))
    if isinstance(node, ast.Attribute):
        return _pathish(node.value, names)
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Div):
        return _pathish(node.left, names)
    return isinstance(node, ast.Name) and node.id in names


def _path_names(tree):
    names = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Assign) and _pathish(node.value, names):
            names.update(t.id for t in node.targets if isinstance(t, ast.Name))
    return names


def _kind(call, names):
    """(вид из SPECS, сдвиг позиций) или None. Сдвиг 1 — несвязанный `Path.read_text(p)`."""
    func = call.func
    if isinstance(func, ast.Name):
        return (BARE_CALLS[func.id], 0) if func.id in BARE_CALLS else None
    if not isinstance(func, ast.Attribute):
        return None
    if isinstance(func.value, ast.Name) and (func.value.id, func.attr) in MODULE_CALLS:
        return MODULE_CALLS[(func.value.id, func.attr)], 0
    if func.attr in ("read_text", "write_text"):
        return func.attr, int(_path_class(func.value))
    if func.attr == "open" and _pathish(func.value, names):
        return "path_open", 0
    return None


def _binary(call, spec, shift):
    if spec.mode is None:
        return False
    index = spec.mode + shift
    mode = call.args[index] if len(call.args) > index else None
    for kw in call.keywords:
        if kw.arg == "mode":
            mode = kw.value
    if mode is None:
        return spec.binary
    return isinstance(mode, ast.Constant) and isinstance(mode.value, str) and "b" in mode.value


def _named(call, spec, shift):
    for kw in call.keywords:
        if kw.arg == "encoding":
            return not (isinstance(kw.value, ast.Constant) and kw.value.value is None)
    return len(call.args) > spec.encoding + shift


def opens_locale(tree):
    """Текстовые открытия файла в разобранном модуле: сколько их и в каких строках кодировка взята
    у локали."""
    names, seen, bad = _path_names(tree), 0, []
    for node in ast.walk(tree):
        kind = isinstance(node, ast.Call) and _kind(node, names)
        if not kind:
            continue
        spec, shift = SPECS[kind[0]], kind[1]
        if _binary(node, spec, shift):
            continue
        seen += 1
        if not _named(node, spec, shift):
            bad.append(node.lineno)
    return seen, bad
