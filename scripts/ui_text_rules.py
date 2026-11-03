"""Fail-closed правила литералов в C++ экранах; анализ потока произвольного C++ не обещается."""
import re
from dataclasses import dataclass

from cpp_text import _literal_end, line_of

KEY = re.compile(r"[A-Za-z0-9_.-]{1,128}\Z", re.ASCII)
PREFIX = re.compile(r'''(?:u8|u|U|L)?(?=R?"|')''')
KEY_ARGS = {"button": {1: "key"}, "slider": {1: "key"}, "field": {1: "key"},
            "glyph_grid": {1: "key"}, "find": {0: "key"}, "ui_id": {1: "id"},
            "UiListItem": {0: "id", 1: "key"}, "UiGridLabels": {0: "key", 1: "key"}}
DIAGNOSTICS = {"printf", "fprintf", "assert", "static_assert"}


@dataclass(frozen=True)
class Token:
    text: str
    at: int
    literal: bool = False


def tokens(text):
    """Комментарии и include не являются текстом UI; позиции остальных токенов сохраняются."""
    out, i = [], 0
    while i < len(text):
        if text[i].isspace():
            i += 1
            continue
        if text.startswith("//", i):
            end = text.find("\n", i)
            i = len(text) if end < 0 else end
            continue
        if text.startswith("/*", i):
            end = text.find("*/", i + 2)
            i = len(text) if end < 0 else end + 2
            continue
        if text.startswith("%:", i):
            out.append(Token("#", i)) # Стандартный C++ digraph препроцессора.
            i += 2
            continue
        prefix = PREFIX.match(text, i)
        end = _literal_end(text, prefix.end() if prefix else i)
        if end >= 0:
            out.append(Token(text[i:end], i, True))
            i = end
            continue
        if text[i].isalpha() or text[i] == "_" or ord(text[i]) > 127:
            # Unicode identifier нельзя разрезать на префикс и разрешённый ASCII suffix.
            # Неизвестные non-ASCII символы включаем консервативно, а не открываем whitelist.
            end = i + 1
            while end < len(text) and not text[end].isspace() and (text[end].isalnum() or text[end] == "_" or ord(text[end]) > 127):
                end += 1
        elif text[i].isdigit():
            end = i + 1
            while end < len(text) and (text[end].isalnum() or text[end] in "'._"):
                end += 1
        else:
            end = i + 1
        out.append(Token(text[i:end], i))
        i = end
    return out


def arguments(items, opening):
    """Разделить только аргументы верхнего уровня, сохраняя вложенные вызовы/initializers."""
    stack, start, result = [], opening + 1, []
    for i in range(opening, len(items)):
        word = items[i].text
        if items[i].literal:
            continue
        if word in ("(", "[", "{"):
            stack.append(word)
        elif word in (")", "]", "}"):
            if not stack or stack.pop() != {")": "(", "]": "[", "}": "{"}[word]:
                return []
            if not stack:
                result.append(list(range(start, i)))
                return result
        elif word == "," and len(stack) == 1:
            result.append(list(range(start, i)))
            start = i + 1
    return []


def plain_key(token):
    return token[1:-1] if token.startswith('"') and token.endswith('"') and KEY.fullmatch(token[1:-1]) else None


def diagnostic_call(items, at):
    if at and items[at - 1].text in (".", ">"):
        return False
    if at >= 2 and items[at - 1].text == ":" and items[at - 2].text == ":":
        return at >= 3 and items[at - 3].text == "std" and (at < 4 or items[at - 4].text not in (":", ".", ">"))
    return True


def file_hits(text, keys):
    # Phase2 и raw-string restoration имеют разную семантику. Screen units требуют явных
    # токенов: отказываем splicing целиком, не пытаемся приближённо выполнять препроцессор.
    splices = [(line_of(text, m.start()), "backslash-newline splicing is forbidden in screen sources")
               for m in re.finditer(r"\\\r?\n", text)]
    if splices:
        return splices
    items = tokens(text)
    allowed = set()
    # Локальный macro не может выдать произвольный draw_text за известный UI/diagnostic API.
    shadowed = {items[i + 2].text for i in range(len(items) - 2)
                if items[i].text == "#" and items[i + 1].text == "define"}
    for i, token in enumerate(items):
        # Только настоящий include, не #define с произвольным текстом.
        if token.text == "#" and i + 2 < len(items) and items[i + 1].text == "include":
            if not text[text.rfind("\n", 0, token.at) + 1:token.at].strip() and items[i + 2].literal:
                allowed.add(i + 2)
        if token.text not in KEY_ARGS and token.text not in DIAGNOSTICS:
            continue
        if token.text in shadowed:
            continue
        if token.text in DIAGNOSTICS and not diagnostic_call(items, i):
            continue
        if i + 1 == len(items) or items[i + 1].text not in ("(", "{"):
            continue
        args = arguments(items, i + 1)
        for arg, indexes in enumerate(args):
            # Конкатенация/выражение не получает разрешение, вложенный вызов проверяется отдельно.
            if token.text in DIAGNOSTICS:
                if indexes and all(items[j].literal for j in indexes):
                    allowed.update(indexes)
                continue
            mode = KEY_ARGS[token.text].get(arg)
            if mode and len(indexes) == 1 and items[indexes[0]].literal:
                value = plain_key(items[indexes[0]].text)
                if value and (mode == "id" or value in keys):
                    allowed.add(indexes[0])
    harmless = {'""', "'\\0'", "'\\n'", "'\\r'", "'\\t'"}
    return [(line_of(text, t.at), "visible literal or unrecognized string key; use a Strings key at the UI call")
            for i, t in enumerate(items) if t.literal and i not in allowed and t.text not in harmless]
