#!/usr/bin/env python3
"""Инвариант: строковые литералы дерева — чистый ASCII.

    python3 scripts/ascii_output_check.py            # гейт
    python3 scripts/ascii_output_check.py --selftest # правила проверяются сломанными фикстурами

Причина ровно одна и уже случилась четырежды: консоль Windows не UTF-8, и русский текст,
напечатанный из процесса, приезжает владельцу как «bye тАФ overlay in memory». Дефект каждый раз
находился ГЛАЗАМИ в отчёте о ручном гейте — то есть после круга «собрать → отдать владельцу →
получить нечитаемый лог», самого дорогого круга в проекте. Литерал ASCII или нет — вопрос,
на который отвечает статическая проверка за полсекунды.

Проверяются ЛИТЕРАЛЫ, а не файлы: комментарии по-прежнему русские (стиль репозитория), и
разделять их с кодом обязан разборщик, а не грep по строке. Отсюда отдельный скрипт: то же
основание, по которому `ci_lint.py` разбирает workflow, а не грепает его.

Осознанное исключение — `// ascii: allow <причина минимум в три слова>` на строке литерала или
на предыдущей. Юникод бывает и содержанием: фикстура, которая проверяет разбор пути с русскими
буквами, обязана его содержать. Односимвольная отписка не подавляет — то же правило, что у
`# ci-lint: allow` в ci_lint.py.
"""

import argparse
import pathlib
import re
import sys
import tempfile

EXTS = {".c", ".cc", ".cxx", ".cpp", ".h", ".hpp", ".inl", ".m", ".mm"}
ROOTS = ("engine", "tools", "example_ugly_game", "platform")
ALLOW = re.compile(r"ascii:\s*allow\b(.*)")
RAW_OPEN = re.compile(r'R"([^("\\ ]*)\(')
# Префиксы символьного литерала. Пустой — обычный `'x'`; остальные это `L'x'`, `u'x'`, `U'x'`,
# `u8'x'`. Всё прочее перед апострофом означает разделитель разрядов (`1'000'000`).
CHAR_PREFIXES = {"", "L", "u", "U", "u8"}

# Позитивный контроль. Греп-гейт, который ничего не нашёл, и греп-гейт, который сломан, выглядят
# одинаково — правило `vacuous-gate` в ci_lint.py про этот же класс. Числа с большим запасом вниз:
# они ловят «разборщик перестал видеть литералы», а не «дерево немного усохло».
MIN_FILES = 60
MIN_LITERALS = 300


def char_literal_at(text, i):
    """Апостроф на позиции i начинает символьный литерал, а не разделяет разряды числа?

    Разница не косметическая: `L'"'` в win32_cmdline.cpp — символьный литерал С КАВЫЧКОЙ внутри,
    и разборщик, принявший его за разделитель, уезжает в мнимую строку до конца файла и выдаёт
    находку на пустом месте."""
    j = i
    while j and (text[j - 1].isalnum() or text[j - 1] == "_"):
        j -= 1
    return text[j:i] in CHAR_PREFIXES


def literals(text):
    """Строковые литералы файла: [(номер строки, содержимое)]. Комментарии пропускаются."""
    out = []
    i, n, line = 0, len(text), 1
    while i < n:
        ch = text[i]
        if ch == "\n":
            line += 1
            i += 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            line += text.count("\n", i, j)
            i = j
        elif ch == "'" and char_literal_at(text, i):
            i += 1
            while i < n and text[i] != "'":
                i += 2 if text[i] == "\\" else 1
            i += 1
        elif (m := RAW_OPEN.match(text, i)) is not None:
            close = ")" + m.group(1) + '"'
            j = text.find(close, m.end())
            j = n if j < 0 else j + len(close)
            out.append((line, text[m.end():max(m.end(), j - len(close))]))
            line += text.count("\n", i, j)
            i = j
        elif ch == '"':
            j, buf = i + 1, []
            while j < n and text[j] != '"':
                if text[j] == "\\":
                    buf.append(text[j:j + 2])
                    j += 2
                else:
                    buf.append(text[j])
                    j += 1
            out.append((line, "".join(buf)))
            line += text.count("\n", i, j)
            i = j + 1
        else:
            i += 1
    return out


def allowed(lines, no):
    """Подавление действует на строке литерала и на предыдущей — многострочный printf иначе
    заставлял бы вешать маркер на каждый кусок."""
    for cand in (no, no - 1):
        if 1 <= cand <= len(lines):
            m = ALLOW.search(lines[cand - 1])
            if m is not None and len(m.group(1).split()) >= 3:
                return True
    return False


def scan(files):
    """→ (находки, число файлов, число литералов). Находка: (путь, строка, литерал)."""
    found, seen_files, seen_lits = [], 0, 0
    for path in files:
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as exc:
            # Молча пропустить нельзя: «пропущено» и «чисто» неразличимы на глаз.
            found.append((path, 0, f"<не прочитан: {exc}>"))
            continue
        seen_files += 1
        lines = text.splitlines()
        for no, lit in literals(text):
            seen_lits += 1
            if any(ord(c) > 127 for c in lit) and not allowed(lines, no):
                found.append((path, no, lit))
    return found, seen_files, seen_lits


def tree_files(root):
    out = []
    for name in ROOTS:
        for p in sorted((root / name).rglob("*")):
            if p.is_file() and p.suffix in EXTS:
                out.append(p)
    return out


FIXTURES = [
    ("плохо: русский в литерале", 'void f() { puts("привет"); }', 1),
    ("хорошо: русский в // комментарии", '// привет\nvoid f() { puts("hi"); }', 0),
    ("хорошо: русский в /* */ комментарии", '/* привет\n   ещё */ void f() { puts("hi"); }', 0),
    ("хорошо: подавление с причиной", 'puts("привет"); // ascii: allow фикстура проверяет юникод', 0),
    ("плохо: подавление без причины", 'puts("привет"); // ascii: allow ага', 1),
    ("хорошо: подавление на предыдущей строке",
     '// ascii: allow фикстура проверяет юникодный путь\nputs("привет");', 0),
    ("плохо: экранированная кавычка не сбивает разбор", 'puts("a\\"b"); puts("привет");', 1),
    ("хорошо: апостроф как разделитель разрядов", "uint64_t x = 1'000'000; // ок\n", 0),
    ("плохо: литерал после разделителя разрядов", "uint64_t x = 1'000'000;\nputs(\"привет\");", 1),
    ("хорошо: символьный литерал с экранированием", "char c = '\\''; puts(\"hi\");", 0),
    ("хорошо: широкий символьный литерал с кавычкой", "out += L'\"';\nputs(\"hi\");", 0),
    ("плохо: юникод после широкого символьного литерала", "out += L'\"';\nputs(\"привет\");", 1),
    ("плохо: raw-строка с юникодом", 'auto s = R"(привет)";', 1),
    ("хорошо: юникод только в имени файла-комментарии", '#include "codes.hpp" // привет', 0),
]


def selftest():
    """Сломанное правило молчит ровно так же, как чистое дерево. Отсюда фикстуры: каждая
    называет ОЖИДАЕМОЕ число находок, и правило, переставшее срабатывать, валит прогон."""
    bad = 0
    with tempfile.TemporaryDirectory() as tmp:
        for i, (name, body, want) in enumerate(FIXTURES):
            p = pathlib.Path(tmp) / f"case{i}.cpp"
            p.write_text(body, encoding="utf-8")
            got = len(scan([p])[0])
            ok = got == want
            bad += 0 if ok else 1
            print(f"  [{'PASS' if ok else 'FAIL'}] {name}" + ("" if ok else f" — ждали {want}, нашли {got}"))
    print(f"ascii-check selftest: {'PASS' if not bad else 'FAIL'} — {len(FIXTURES)} кейсов, провалов: {bad}")
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()
    if args.selftest:
        return selftest()

    root = pathlib.Path(__file__).resolve().parent.parent
    files = tree_files(root)
    found, seen_files, seen_lits = scan(files)
    if seen_files < MIN_FILES or seen_lits < MIN_LITERALS:
        print(f"ascii-check: гейт вакуумен — прочитано {seen_files} файлов, {seen_lits} литералов "
              f"(ждали ≥{MIN_FILES} и ≥{MIN_LITERALS}). Сломан обход дерева или разборщик.")
        return 1
    for path, no, lit in found:
        print(f"{path.relative_to(root)}:{no}: не-ASCII в литерале: {lit[:70]}")
    if found:
        print(f"\nascii-check: FAIL — {len(found)} находок. Рантайм-вывод обязан быть ASCII: консоль "
              f"Windows не UTF-8. Юникод по делу — `// ascii: allow <причина в три слова>`.")
        return 1
    print(f"ascii-check: PASS — {seen_files} файлов, {seen_lits} литералов, не-ASCII нет")
    return 0


if __name__ == "__main__":
    sys.exit(main())
