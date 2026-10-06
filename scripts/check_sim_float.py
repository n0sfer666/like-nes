#!/usr/bin/env python3
import re
import subprocess
import sys
from pathlib import Path

import py_utf8

SIM_DIRS = ("engine/framework/brawl", "engine/framework/ai", "engine/framework/scene")
REQUIRED_DIR = "engine/framework/brawl"
EXTS = {".c", ".cc", ".cxx", ".cpp", ".h", ".hpp", ".inl"}
MIN_FILES = 10
RULES = (
    (re.compile(r"\b(?:float|double|from_float|to_double|float_t|double_t|_Float\d+|float\d+_t)\b"),
     "плавающий тип или конверсия"),
    (re.compile(r"(?<![\w.'])(?:\d[\d']*\.[\d']*|\.\d[\d']*)(?:[eE][+-]?\d+)?[fFlL]?(?![\w.])"
                r"|(?<![\w.'])\d[\d']*[eE][+-]?\d+[fFlL]?(?![\w.])"
                r"|\b0[xX][\dA-Fa-f'.]*[pP][+-]?\d+[fFlL]?"),
     "плавающий литерал"),
    (re.compile(r"#\s*include\s*<c?math(?:\.h)?>"), "плавающая математика"),
)
WITNESS = re.compile(r"\bfix32\b")
LEXEME = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|(?<![0-9A-Fa-f])\'(?:\\.|[^\'\\\n])*\'', re.S)


def code_only(text):
    return LEXEME.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), text)


def audit(files):
    bad = []
    for path in sorted(files):
        for n, line in enumerate(code_only(files[path]).splitlines(), 1):
            for rule, kind in RULES:
                for hit in rule.finditer(line):
                    bad.append(f"{path}:{n}: `{hit.group(0)}` — {kind}: в симуляции только fix32 и целые")
    return bad


def guards(files):
    if not any(p.startswith(REQUIRED_DIR + "/") for p in files):
        return [f"обход не нашёл {REQUIRED_DIR}: искать было негде"]
    if len(files) < MIN_FILES:
        return [f"обход дал {len(files)} файл(ов) при пороге {MIN_FILES}"]
    if not any(WITNESS.search(code_only(t)) for t in files.values()):
        return ["ни в одном файле не найден `fix32`: разбор кода сломан"]
    return []


def scan(root):
    out = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        capture_output=True, text=True, encoding="utf-8", check=True).stdout
    prefixes = tuple(d + "/" for d in SIM_DIRS)
    files = {}
    for name in (n for n in out.split("\0") if n):
        blob = root / name
        if Path(name).suffix in EXTS and name.startswith(prefixes) and blob.is_file():
            files[name] = blob.read_text(encoding="utf-8", errors="replace")
    return files


FILLER = {f"{REQUIRED_DIR}/f{i}.hpp": "fix32 x;\n" for i in range(MIN_FILES)}

CASES = (
    ("float в строчном комментарии не валит", {"a.hpp": "fix32 v; // float here\n"}, 0),
    ("double в блочном комментарии не валит", {"a.hpp": "/* double\n float */ fix32 v;\n"}, 0),
    ("float в строке не валит", {"a.cpp": 'const char* s = "float";\n'}, 0),
    ("hex и целые не литерал", {"a.cpp": "int a = 0x1e3 + 1'000 + 7u; fix32 v;\n"}, 0),
    ("точка члена не литерал", {"a.cpp": "auto w = p.x + arr[0].y + b.e1;\n"}, 0),
    ("<cstdint> не математика", {"a.cpp": "#include <cstdint>\n"}, 0),
    ("from_float валит", {"a.cpp": "fix32 v = fix32::from_float(0);\n"}, 1),
    ("to_double валит", {"a.cpp": "auto d = v.to_double();\n"}, 1),
    ("float_t и _Float32 валят", {"a.cpp": "float_t a; _Float32 b;\n"}, 2),
    ("литерал с точкой валит", {"a.cpp": "auto k = 0.5;\n"}, 1),
    ("литерал 1.f и .5 валят", {"a.cpp": "auto k = 1.f + .5;\n"}, 2),
    ("литерал 1e3 валит", {"a.cpp": "auto k = 1e3;\n"}, 1),
    ("hex-float валит", {"a.cpp": "auto k = 0x1.8p3;\n"}, 1),
    ("<cmath> валит", {"a.cpp": "#include <cmath>\n"}, 1),
    ("<math.h> валит", {"a.cpp": "# include <math.h>\n"}, 1),
    ("float в коде валит", {"a.hpp": "float speed = 0;\n"}, 1),
    ("double в коде валит", {"a.cpp": "fix32 v; double d;\n"}, 1),
    ("код после комментария на той же строке виден", {"a.cpp": "/* x */ float f;\n"}, 1),
    ("long double — одна находка", {"a.cpp": "long double d;\n"}, 1),
    ("разделитель разрядов не прячет код", {"a.cpp": "int a = 1'000; float f = 2'0;\n"}, 1),
    ("// внутри строки не открывает комментарий", {"a.cpp": 'auto u = "http://x"; float f;\n'}, 1),
)


def selftest(verbose=True):
    fails = 0
    for name, extra, want in CASES:
        files = dict(FILLER)
        files.update({f"{REQUIRED_DIR}/{k}": v for k, v in extra.items()})
        got = len(audit(files))
        ok = got == want and not guards(files)
        fails += 0 if ok else 1
        if verbose or not ok:
            print(f"  {'ok' if ok else 'FAIL'}: {name} (находок {got}, ожидалось {want})")
    refusals = (("пустой обход", {}),
                ("нет каталога brawl", {f"engine/framework/ai/f{i}.hpp": "fix32 x;\n" for i in range(MIN_FILES)}),
                ("fix32 только в комментариях", {k: "// fix32\n" for k in FILLER}))
    for name, files in refusals:
        ok = bool(guards(files))
        fails += 0 if ok else 1
        if verbose or not ok:
            print(f"  {'ok' if ok else 'FAIL'}: охранник — {name}")
    if verbose:
        print(f"sim-float selftest: {'PASS' if fails == 0 else 'FAIL'} — {len(CASES) + len(refusals)} контролей")
    return fails


def gate(root):
    files = scan(root)
    stop = guards(files)
    for line in stop:
        print(f"sim-float: FAIL — {line}")
    if stop:
        return 1
    bad = audit(files)
    for line in bad:
        print(line)
    print(f"sim-float: {'FAIL' if bad else 'PASS'} — {len(files)} файл(ов), находок: {len(bad)}")
    return 1 if bad else 0


def main(argv):
    py_utf8.enable()
    if "--selftest" in argv:
        return 1 if selftest() else 0
    if selftest(verbose=False):
        print("sim-float: FAIL — сломаны сами правила, находкам верить нельзя")
        return 1
    return gate(Path(__file__).resolve().parent.parent)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
