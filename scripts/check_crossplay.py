#!/usr/bin/env python3
import re
import sys
from pathlib import Path

import py_utf8

OSES = ("Linux", "Windows", "macOS")
COUNT_HEADER = "engine/framework/brawl/framework_brawl_crossplay.hpp"
COUNT = re.compile(r"\bCROSSPLAY_SCENARIOS\s*=\s*(\d+)\b")
LINE = re.compile(r"[a-z0-9][a-z0-9-]* 0x[0-9a-f]{16}")


def expected_count(header_text):
    hit = COUNT.search(header_text)
    return int(hit.group(1)) if hit else None


def file_problems(os_name, blob, expected):
    if blob is None:
        return [f"{os_name}: нет файла crossplay-{os_name}.txt"]
    if not blob:
        return [f"{os_name}: файл пуст"]
    if b"\r" in blob:
        return [f"{os_name}: в файле CR — запись не в бинарном режиме"]
    if not blob.endswith(b"\n"):
        return [f"{os_name}: последняя строка без LF"]
    lines = blob.decode("utf-8", errors="replace").split("\n")[:-1]
    bad = [f"{os_name}: строка {n} не «имя 0x<16 hex>»: {line!r}"
           for n, line in enumerate(lines, 1) if not LINE.fullmatch(line)]
    names = [line.split(" ")[0] for line in lines]
    if len(set(names)) != len(names):
        bad.append(f"{os_name}: имя сценария повторяется")
    if len(lines) != expected:
        bad.append(f"{os_name}: сценариев {len(lines)}, ожидалось {expected}")
    return bad


def hashes(blob):
    return dict(line.split(" ") for line in blob.decode("utf-8").split("\n")[:-1])


def mismatches(blobs):
    base = blobs[OSES[0]]
    if all(blobs[o] == base for o in OSES):
        return []
    tables = {o: hashes(blobs[o]) for o in OSES}
    names = sorted({n for t in tables.values() for n in t})
    out = []
    for name in names:
        row = {o: tables[o].get(name, "—") for o in OSES}
        if len(set(row.values())) > 1:
            out.append(f"{name}: " + ", ".join(f"{o}={h}" for o, h in row.items()))
    return out or ["файлы различаются порядком сценариев"]


def verdict(blobs, expected):
    if expected is None:
        return [f"в {COUNT_HEADER} не найдено CROSSPLAY_SCENARIOS"]
    problems = [p for o in OSES for p in file_problems(o, blobs.get(o), expected)]
    return problems or mismatches(blobs)


GOOD = b"walk 0x00000000000000aa\nhop 0x00000000000000bb\n"


def same(blob):
    return {o: blob for o in OSES}


def with_one(os_name, blob):
    blobs = same(GOOD)
    blobs[os_name] = blob
    return blobs


CASES = (
    ("три одинаковых файла сходятся", same(GOOD), 2, 0),
    ("пустой файл", with_one("Windows", b""), 2, 1),
    ("нет файла одной ОС", {o: GOOD for o in OSES if o != "macOS"}, 2, 1),
    ("файл без сценария", same(b"walk 0x00000000000000aa\n"), 2, 3),
    ("хеш одной ОС изменён", with_one("macOS", GOOD.replace(b"bb\n", b"bc\n")), 2, 1),
    ("порядок сценариев другой", with_one("Linux", b"hop 0x00000000000000bb\nwalk 0x00000000000000aa\n"), 2, 1),
    ("CRLF вместо LF", with_one("Windows", GOOD.replace(b"\n", b"\r\n")), 2, 1),
    ("строка не по формату", with_one("Linux", b"walk 0xaa\nhop 0x00000000000000bb\n"), 2, 1),
    ("имя сценария повторяется", same(b"walk 0x00000000000000aa\nwalk 0x00000000000000bb\n"), 2, 3),
    ("без LF в конце", with_one("Linux", GOOD[:-1]), 2, 1),
    ("нет константы числа", same(GOOD), None, 1),
)


def selftest(verbose=True):
    fails = 0
    for name, blobs, expected, want in CASES:
        got = len(verdict(blobs, expected))
        ok = got == want
        fails += 0 if ok else 1
        if verbose or not ok:
            print(f"  {'ok' if ok else 'FAIL'}: {name} (находок {got}, ожидалось {want})")
    count_ok = expected_count("constexpr uint32_t CROSSPLAY_SCENARIOS = 6;") == 6
    fails += 0 if count_ok else 1
    if verbose or not count_ok:
        print(f"  {'ok' if count_ok else 'FAIL'}: число сценариев читается из заголовка")
    if verbose:
        print(f"crossplay selftest: {'PASS' if fails == 0 else 'FAIL'} — {len(CASES) + 1} контролей")
    return fails


def gate(root, folder):
    expected = expected_count((root / COUNT_HEADER).read_text(encoding="utf-8"))
    blobs = {}
    for o in OSES:
        path = folder / f"crossplay-{o}.txt"
        blobs[o] = path.read_bytes() if path.is_file() else None
    problems = verdict(blobs, expected)
    for line in problems:
        print(f"crossplay: {line}")
    print(f"crossplay: {'FAIL' if problems else 'PASS'} — {len(OSES)} ОС, сценариев {expected}")
    return 1 if problems else 0


def main(argv):
    py_utf8.enable()
    if "--selftest" in argv:
        return 1 if selftest() else 0
    if len(argv) != 1:
        print("usage: check_crossplay.py <каталог с crossplay-<os>.txt> | --selftest")
        return 2
    if selftest(verbose=False):
        print("crossplay: FAIL — сломаны сами правила, находкам верить нельзя")
        return 1
    return gate(Path(__file__).resolve().parent.parent, Path(argv[0]))


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
