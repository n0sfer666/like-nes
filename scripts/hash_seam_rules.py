"""Правила шва хешей: что считать рукописной копией FNV и чем это подавляется.

Вынесено из `check_hash_seam.py` по той же границе, по какой рядом живут `ci_lint_rules.py` и
`line_budget_rules.py`: там — обход дерева и охранники, здесь — суждение о ТЕКСТЕ файла. Обе
половины проверяются одним набором (`check_hash_seam_selftest.py`).

Правил ДВА, и второе не следует из первого.

1. КОНСТАНТА этих семей голым литералом вне примитивов. Сравнение идёт по ЗНАЧЕНИЮ, а не по
   написанию: `0x100000001b3`, `1099511628211` и `0x100000001B3ULL` — одно и то же число.
2. ИМЕНОВАННАЯ константа в арифметике смешивания вне примитивов. Литеральное правило её не видит
   вовсе, а копипаста с соседнего файла пишется именно так — `h ^= x; h *= asset::FNV_PRIME;`
   проходило первую редакцию гейта молча, и ровно такая копия жила в каталоге самого примитива
   (`engine/asset/asset_determinism_test.cpp`, находка ревью). Признак — константа, к которой
   ПРИМЫКАЕТ `*` или `^`: затравка (`uint64_t h = asset::FNV_OFFSET;`) арифметикой не является и
   отбиваться не должна.

Обход у правила 2 не полон и это сказано вслух: `constexpr uint64_t P = asset::FNV_PRIME;` с
последующим `h *= P;` отмывает имя и остаётся невидимым. Закрывать это значило бы разбирать C++
всерьёз; мера ловит то, чем копия пишется на практике — теми же тремя строками, что и оригинал.

Комментарии снимаются перед поиском: число, названное в объяснении, мерой не является. Маркер
`// hash-seam: allow <причина минимум в три слова>` ищется, наоборот, ТОЛЬКО в комментариях —
в сыром тексте его подделывает строковый литерал.
"""
import re

# Два примитива — и только они. Их пути написаны ЯВНО, а не выведены из имени файла: «файл, в имени
# которого есть hash» — это правило, под которое подпадает первая же новая копия, названная
# `foo_hash.hpp`.
PRIMITIVES = ("engine/asset/hash.hpp", "engine/framework/physics/hash_mix.hpp")

# Значения, а не написания. Первое — каноническая база FNV-1a-64, второе — наша (семья `asset`),
# третье — прайм, общий у обеих семей.
BANNED = {0xCBF29CE484222325: "каноническая база FNV-1a-64",
          1469598103934665603: "база FNV-1a семьи asset",
          0x100000001B3: "прайм FNV-1a-64"}

# Числовой литерал C++ вместе с суффиксом: hex или десятичный, разделители разрядов допустимы.
NUM = re.compile(r"(?<![\w.])(0[xX][0-9a-fA-F']+|\d[\d']*)[uUlLzZ]*(?![\w.])")
NAMED = r"(?:\w+\s*::\s*)*FNV_(?:PRIME|OFFSET)"
# Именованная константа, к которой примыкает умножение или xor, — с любой стороны.
FORM = re.compile(r"[*^]=?\s*" + NAMED + r"|" + NAMED + r"\s*[*^]")
ALLOW = re.compile(r"hash-seam:\s*allow\b(.*)")


def _blank(chunk):
    """Пустышка той же ВЫСОТЫ: номера строк во второй половине не должны поехать."""
    return "\n" * chunk.count("\n")


def split_code_comments(text):
    """(код, комментарии) — обе половины сохраняют нумерацию строк исходника.

    Строковые и символьные литералы гасятся в ОБЕИХ: `"http://x"` иначе открывал бы комментарий до
    конца строки, а маркер, написанный внутри литерала, подавлял бы находку.
    """
    code, com, i, n = [], [], 0, len(text)
    while i < n:
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
        else:
            j = _literal_end(text, i)
            if j < 0:
                code.append(text[i])
                com.append(_blank(text[i]))
                i += 1
                continue
            code.append(_blank(text[i:j]))
            com.append(_blank(text[i:j]))
            i = j
            continue
        com.append(text[i:j])
        code.append(_blank(text[i:j]))
        i = j
    return "".join(code), "".join(com)


def _literal_end(text, i):
    """Конец строкового/символьного литерала, начатого в позиции i, или -1, если он там не начат."""
    if text.startswith('R"', i):
        m = re.compile(r'R"([^()\\ \t\n]{0,16})\(').match(text, i)
        if m:
            end = text.find(')%s"' % m.group(1), m.end())
            return len(text) if end < 0 else end + len(m.group(1)) + 2
    if text[i] not in "\"'":
        return -1
    if text[i] == "'" and _digit_sep(text, i):
        return -1
    quote, j = text[i], i + 1
    while j < len(text) and text[j] != quote:
        j += 2 if text[j] == "\\" else 1
    return min(j + 1, len(text))


def _digit_sep(text, i):
    """Апостроф разделителя разрядов (`1'099'511'628'211`), а не начало символьного литерала.

    Принят за литерал — и `1'099'511'628'211` гасится вместе с половиной строки, то есть ровно то
    написание прайма, которым его напишет следующая копия, проходит гейт молча. Отличается по тому,
    ЧЕМ начинается слово слева: цифрой — число, буквой — префикс символьного литерала (`u8'a'`).
    """
    j = i - 1
    while j >= 0 and (text[j].isalnum() or text[j] == "'"):
        j -= 1
    return j + 1 < i and text[j + 1].isdigit()


def allowed_lines(comments):
    """Номера строк, накрытых маркером: сама строка маркера и следующая за ней."""
    ok = set()
    for num, line in enumerate(comments.splitlines(), 1):
        m = ALLOW.search(line)
        if m and len(m.group(1).split()) >= 3:
            ok.update((num, num + 1))
    return ok


def _line_of(text, pos):
    return text.count("\n", 0, pos) + 1


def constants(code):
    """Запрещённые константы голым литералом: [(номер строки, объяснение)]."""
    found = []
    for m in NUM.finditer(code):
        try:
            value = int(m.group(1).replace("'", ""), 0)
        except ValueError:
            continue
        if value in BANNED:
            found.append((_line_of(code, m.start()),
                          "%s голым литералом" % BANNED[value]))
    return found


def named_forms(code):
    """Именованная константа в арифметике смешивания: [(номер строки, объяснение)]."""
    return [(_line_of(code, m.start()),
             "смешивание FNV руками (`%s`) — именованная константа арифметику не оправдывает"
             % " ".join(m.group(0).split()))
            for m in FORM.finditer(code)]


def file_hits(text):
    """Обе половины правила разом: [(номер строки, объяснение)] плюс множество накрытых маркером."""
    code, com = split_code_comments(text)
    return sorted(constants(code) + named_forms(code)), allowed_lines(com)
