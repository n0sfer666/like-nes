#!/usr/bin/env python3
"""Инвариант: в C++-коде нет C-style кастов — только `static_cast`/`reinterpret_cast`.

    python3 scripts/check_c_casts.py <корень>...            # гейт (корни даёт tree_invariants.sh)
    python3 scripts/check_c_casts.py --selftest              # правила проверяются фикстурами

Находка 8 аудита #21: C-style касты против запрета из `conventions.md` (аудит насчитал грепом 97,
этот разбор на том дереве — 174), и ни одного гейта. Первая линия — `-Wold-style-cast` в `cmake/warnings.cmake`: компилятор судит точно, но
только файлы, которые он собирает. Windows-файлы не собирает ни clang, ни gcc, мобильные оболочки
CI не собирает вовсе. Эта линия видит всё дерево, и платит за это тем, что она эвристика: типа от
переменной по тексту не отличить. Поэтому `(имя) - x` и `(имя) * x` — выражение с тем же правом,
что и каст, и кастом считаются, только когда за скобкой стоит операнд, которым выражение
продолжиться не может (`(WORD)x`, `(uint8_t)(v * 255)`, `(Foo*)p`, `(unsigned long long)n`,
ObjC-сообщение `(NSString*)[d key]`).
Вызов через скобки `(fn)(x)` от каста не отличим вовсе и в дереве не встречается. Слепое пятно
по построению: `>` вплотную к скобке читается концом шаблона, поэтому `a>(int)b` без пробела молчит.

`(void)x` — отбрасывание значения, а не приведение типа: разрешено (решение владельца по B8).
Файлы на C (`.c`, `.m`) не судятся: другого приведения в C нет. Список `EXTS` при этом полный —
он копия признака обхода, которую сверяет `check_tree_roots.py`.
"""

import os
import re
import subprocess
import sys
from pathlib import Path

import py_utf8
from cpp_text import split_code_comments

EXTS = {".c", ".cc", ".cxx", ".cpp", ".h", ".hpp", ".inl", ".m", ".mm"}
C_ONLY = {".c", ".m"}
BUILTIN = r"(?:unsigned|signed|short|long|int|char|float|double|bool|void|wchar_t|char8_t|char16_t|char32_t)"
QUAL = r"(?:(?:const|volatile|struct|enum|class|typename)\s+)*"
NAME = r"(?:::)?[A-Za-z_]\w*(?:\s*::\s*[A-Za-z_]\w*)*(?:\s*<[^()<>;]*(?:<[^()<>;]*>[^()<>;]*)*>)?"
TYPE = rf"{QUAL}(?:{BUILTIN}(?:\s+{BUILTIN})*|{NAME})(?:\s*const)?(?P<ptr>(?:\s*[*&]+(?:\s*const)?)*)"
CAST = re.compile(rf"\(\s*(?P<type>{TYPE})\s*\)\s*")
PLAIN = re.compile(r"(?:::)?[A-Za-z_]\w*(?:\s*::\s*[A-Za-z_]\w*)*")
OPERAND = re.compile(r"[A-Za-z_0-9(\-~!+&*.\[]|::")
WORD = re.compile(r"[A-Za-z_0-9]")
# Перед `(` — вызов, объявление, условие. `>` — конец шаблона только вплотную к скобке
# (`f<T>(x)`): `a > (int)b` и `a >> (int)b` — выражение.
NOT_CAST_AFTER = re.compile(r"(?:[\w\])]\s*|>|operator\s*\S{1,3}\s*)$")
# Objective-C: `- (BOOL)name:(NSSet*)arg` — объявление метода и его аргументы, не приведение.
# Объявление начинается `-`/`+` с нулевой колонки и тянется до `{` или `;`: только внутри него
# `name:(T)x` — параметр. Вне его то же `name:(T)x` — аргумент сообщения или ветвь тернарного.
OBJC_METHOD = re.compile(r"[-+]\s*\(")
OBJC_DECL = re.compile(r"^\s*[-+]\s*$|^[^\[]*\w:\s*$")
# `new` сюда не входит: `new (buf) T` — размещение, а не каст.
EXPR_WORDS = {"return", "case", "co_return", "co_yield", "co_await", "throw", "else", "do", "and",
              "or", "not", "delete"}
KEYWORDS_TYPES = set(re.findall(r"\w+", BUILTIN))
# Заведомые типы среди простых имён: за ними и унарный операнд — каст (`(DWORD)-1`, `(size_t)~0u`).
# Словарь Win32 явный: `(MAX) - 1` с константой в верхнем регистре кастом не станет.
KNOWN_TYPE = re.compile(r"\w+_t|DWORD|WORD|BYTE|UINT|ULONG|LONG|SHORT|USHORT|BOOL|HANDLE|SIZE_T"
                        r"|UINT_PTR|ULONG_PTR|DWORD_PTR|LONG_PTR|INT_PTR|HRESULT")
DEFINE = re.compile(r"\s*#\s*define\s+\w+(?:\([^)]*\))?")

# Позитивный контроль: обход обязан видеть дерево, а правило — хоть одну скобку с типом внутри,
# иначе сломанная регулярка молчит неотличимо от чистого дерева.
MIN_FILES = 60
MIN_PARENS = 300


def _is_cast(line, m, decl, chained):
    """Скобка с типом — каст, если перед ней не вызов/объявление, а после неё — операнд.

    decl — строка внутри объявления метода ObjC. chained — скобка стоит вплотную за кастом
    (`(int)(intptr_t)t`): перед ней `)`, но не вызова.
    """
    pre = line[:m.start()]
    if decl and OBJC_DECL.search(pre):
        return False
    last = re.search(r"(\w+)\s*$", pre)
    if last and last.group(1) not in EXPR_WORDS:
        return False
    if not last and not chained and NOT_CAST_AFTER.search(pre):
        return False
    t = re.sub(r"\s+", " ", m.group("type")).strip()
    if t == "void":
        return False
    rest = line[m.end():]
    plain = (PLAIN.fullmatch(t) and not KNOWN_TYPE.fullmatch(t.split("::")[-1].strip())
             and not (set(re.findall(r"\w+", t)) & KEYWORDS_TYPES))
    if not rest:
        return not plain
    if not OPERAND.match(rest):
        return False
    if plain and not (WORD.match(rest) or rest.startswith("(")):
        return False
    return True


def casts(text, objc=False):
    """(строка, тип) каждого C-style каста вне комментариев и литералов; objc — файл `.mm`."""
    out = []
    decl = False
    for i, line in enumerate(split_code_comments(text)[0].split("\n")):
        decl = decl or bool(objc and OBJC_METHOD.match(line))
        head = DEFINE.match(line)
        if head:
            line = " " * head.end() + line[head.end():]
        prev = -1
        for m in CAST.finditer(line):
            if _is_cast(line, m, decl, m.start() == prev):
                prev = m.end()
                out.append((i + 1, re.sub(r"\s+", " ", m.group("type")).strip()))
        decl = decl and not re.search(r"[{;]", line)
    return out


def audit(files):
    """files: {путь: текст}. Находки словами."""
    bad = []
    for path in sorted(files):
        for line, t in casts(files[path], path.endswith(".mm")):
            bad.append(f"{path}:{line}: C-style каст `({t})` — static_cast, а на границе C-API "
                       f"reinterpret_cast с обоснованием в комментарии.")
    return bad


def parens(files):
    return sum(len(CAST.findall(split_code_comments(t)[0])) for t in files.values())


def guards(files):
    if len(files) < MIN_FILES:
        return [f"обход дал {len(files)} файл(ов) при пороге {MIN_FILES}: он описывает не то "
                f"дерево, по которому его запустили"]
    seen = parens(files)
    if seen < MIN_PARENS:
        return [f"разбор нашёл {seen} скобок с типом при пороге {MIN_PARENS}: регулярка типа "
                f"сломана, и молчит она неотличимо от чистого дерева"]
    return []


def scan(root, roots):
    """Исходники C++ под корнями глазами git: индекс и ненаписанное, как у check_include_seam.py."""
    out = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z", "--cached", "--others", "--exclude-standard",
         "--", *roots], capture_output=True, text=True, encoding="utf-8", check=True).stdout
    files = {}
    for name in (n for n in out.split("\0") if n):
        path = root / name
        if path.suffix in EXTS - C_ONLY and path.is_file():
            files[name] = path.read_text(encoding="utf-8", errors="replace")
    return files


def gate(root, roots):
    missing = [r for r in roots if not (root / r).is_dir()]
    if not roots or missing:
        print(f"c-casts: FAIL — корни не найдены: {' '.join(missing) or '(не переданы)'}")
        return 1
    files = scan(root, roots)
    stop = guards(files)
    if stop:
        for line in stop:
            print(f"c-casts: FAIL — {line}")
        return 1
    bad = audit(files)
    for line in bad:
        print(line)
    print(f"c-casts: {'FAIL' if bad else 'PASS'} — {len(files)} файл(ов), скобок с типом: "
          f"{parens(files)}, находок: {len(bad)}")
    return 1 if bad else 0


def main(argv):
    py_utf8.enable()
    from check_c_casts_selftest import selftest
    if "--selftest" in argv:
        return selftest(audit, guards, scan)
    if selftest(audit, guards, scan, verbose=False):
        print("c-casts: FAIL — сломаны сами правила, находкам верить нельзя")
        return 1
    return gate(Path(__file__).resolve().parent.parent, argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
