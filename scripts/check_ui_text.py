#!/usr/bin/env python3
"""В5 #26: литералы экранов запрещены; #26 судится selftest, реальные screens подключит #29."""
import argparse
import sys
from pathlib import Path

import py_utf8
from ui_text_rules import KEY, file_hits

SOURCE_EXTENSIONS = {".cpp", ".cc", ".cxx", ".hpp", ".h", ".inl", ".mm"}
MAX_TSV_BYTES = 16 << 20


def read_keys(paths):
    keys = set()
    for path in paths:
        if path.suffix != ".tsv":
            raise ValueError(f"{path}: expected a TSV up to16MiB")
        with path.open("rb") as source:
            data = source.read(MAX_TSV_BYTES + 1)
        if len(data) > MAX_TSV_BYTES or b"\0" in data:
            raise ValueError(f"{path}: strings exceed16MiB or contain NUL")
        rows = data.decode("utf-8").split("\n")
        if rows[-1] == "":
            rows.pop()
        rows = [row.removesuffix("\r") for row in rows]
        if not rows or rows[0] != "key\ten\tru":
            raise ValueError(f"{path}: expected exact key/en/ru header")
        for line, row in enumerate(rows[1:], 2):
            if not row:
                continue
            columns = row.split("\t")
            if len(columns) != 3 or not KEY.fullmatch(columns[0]) or not columns[1]:
                raise ValueError(f"{path}:{line}: malformed strings row")
            if columns[0] in keys:
                raise ValueError(f"{path}:{line}: duplicate string key {columns[0]}")
            if len(keys) == 4096:
                raise ValueError(f"{path}:{line}: too many string keys")
            keys.add(columns[0])
    if not keys:
        raise ValueError("no string keys; nothing was checked")
    return keys


def audit(strings, screens):
    if not strings or not screens:
        raise ValueError("provide both --strings and --screens; an empty scan is refused")
    keys = read_keys(strings)
    if not file_hits('draw_text("UI gate control");', keys):
        raise ValueError("literal detector failed its positive control")
    bad = []
    for path in screens:
        if path.suffix not in SOURCE_EXTENSIONS or not path.is_file():
            raise ValueError(f"{path}: expected a C++ screen source")
        text = path.read_text(encoding="utf-8")
        if text.startswith("\ufeff"):
            raise ValueError(f"{path}: UTF8 BOM is forbidden in project sources")
        for line, why in file_hits(text, keys):
            bad.append(f"{path}:{line}: {why}")
    return bad


def main(argv=None):
    py_utf8.enable()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--strings", nargs="+", type=Path)
    parser.add_argument("--screens", nargs="+", type=Path)
    args = parser.parse_args(argv)
    if args.selftest:
        if args.strings or args.screens:
            parser.error("selftest cannot replace an explicit screen scan")
        from check_ui_text_selftest import selftest
        return selftest()
    try:
        bad = audit(args.strings, args.screens)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"ui-text: FAIL {error}")
        return 1
    for problem in bad:
        print(problem)
    print("ui-text: FAIL" if bad else f"ui-text: PASS {len(args.screens)} screen(s), known strings keys only")
    return int(bool(bad))


if __name__ == "__main__":
    sys.exit(main())
