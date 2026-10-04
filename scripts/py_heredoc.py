"""Heredoc шелла глазами гейта: где кончается логическая строка, какое слово закрывает тело и какие
строки файла вообще не код шелла, а чьё-то тело.

Вынесено из `py_embedded.py`, потому что нужно ему дважды и по разным причинам: достать тело
python и ВЫЧЕСТЬ из обхода тела всех прочих heredoc — `cat <<EOF` с примером `python3 -c '…'` в
тексте справки иначе судился бы как код.
"""
import re

# Слово в кавычках, с обратной косой или голое — все три формы шелл принимает, и гейт, знающий одну,
# молча пропускал бы тело с `<<\PY` или `<<'END-PY'`.
WORD = r"<<(-?)[ \t]*(?:'([^'\n]+)'|\"([^\"\n]+)\"|\\?([^\s'\"<>|;&()]+))"
HEREDOC = re.compile(WORD)
# Тот же оператор где угодно в строке, но не `<<<`: here-string телом не кончается и строк не съедает.
ANY = re.compile(r"(?<!<)" + WORD)
COMMENT = re.compile(r"^\s*#")


def word(match):
    return next(group for group in match.groups()[1:] if group is not None)


def logical(text, pos):
    """[начало, конец) логической строки вокруг `pos`: перенос за обратной косой её не кончает."""
    start = text.rfind("\n", 0, pos)
    while start > 0 and text[start - 1] == "\\":
        start = text.rfind("\n", 0, start - 1)
    end = text.find("\n", pos)
    while end > 0 and text[end - 1] == "\\":
        end = text.find("\n", end + 1)
    return start + 1, len(text) if end < 0 else end


def _closes(raw, term, dash, yaml):
    # В `run: |` терминатор стоит с отступом блока, который YAML снимет до шелла. В `.sh` шелл
    # сравнивает строку целиком, и `  PY` внутри тела его НЕ обрывает; `<<-` снимает только табы.
    if yaml:
        return raw.strip() == term
    return (raw.lstrip("\t") if dash else raw) == term


def span(lines, first, term, dash, yaml):
    """Индекс строки-терминатора начиная с `first` или None, если тело тянется до конца файла."""
    for index in range(first, len(lines)):
        if _closes(lines[index], term, dash, yaml):
            return index
    return None


def hidden(lines, yaml):
    """Индексы строк, лежащих внутри тела какого-либо heredoc: это текст, а не команды шелла.
    Оператор без терминатора ничего не прячет — он мог быть просто строкой (`1 << n`)."""
    out, index = set(), 0
    while index < len(lines):
        match = None if COMMENT.match(lines[index]) else ANY.search(lines[index])
        tail = index
        while match and tail < len(lines) - 1 and lines[tail].endswith("\\"):
            tail += 1
        close = span(lines, tail + 1, word(match), match.group(1) == "-", yaml) if match else None
        if close is None:
            index += 1
            continue
        out.update(range(tail + 1, close + 1))
        index = close + 1
    return out
