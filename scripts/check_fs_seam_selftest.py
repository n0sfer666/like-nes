"""Прогон фикстур шва файлового ввода-вывода: правило обязано сработать на поломке и промолчать на починке.

Тот же контракт, что у `check_hash_seam_selftest.py`, и по той же причине: гейт, чьё правило
сломано, молчит неотличимо от гейта, которому нечего сказать. Правила проверяются синтетическим
словарём «путь → текст» (`fs_seam_fixtures.py`), охранники — своим, обход — НАСТОЯЩИМ
репозиторием, а различение файловой формы от алгоритма — ещё и настоящим деревом:
`std::remove(first, last, value)` живёт в нём, и утверждение, заточенное под игрушечную фикстуру,
выглядело бы здоровым ровно до первого прогона гейта.
"""
import re
import subprocess
import tempfile
from pathlib import Path

from fs_seam_fixtures import BAD_PATH, BASE, CASES, FULL, GUARDS, LOOKALIKE, QUIET

# Признак алгоритмической формы для ЯКОРЯ: итераторы в аргументах. Считать её тем же
# `top_level_args`, который якорь и подтверждает, значило бы спрашивать у проверяемого, здоров ли
# он: сломайся счёт скобок — и множество «алгоритмов» опустело бы вместе с находками, а якорь
# напечатал бы SKIP.
ALGO = re.compile(r"(?<![\w.>])(?:\w+\s*::\s*)*remove\s*\([^;]*?\.\s*begin\s*\(\s*\)")


def _fixture_repo(root):
    """Настоящий репозиторий: обход берёт файлы у git, а не у os.walk."""
    for rel, text in (("engine/a.cpp", "int a;\n"), ("engineering/b.cpp", "int b;\n"),
                      ("docs/examples/c.cpp", "int c;\n"), ("engine/notes.md", "текст\n"),
                      ("engine/skip.cpp", "int s;\n")):
        (root / rel).parent.mkdir(parents=True, exist_ok=True)
        (root / rel).write_text(text, encoding="utf-8")
    (root / ".gitignore").write_text("engine/skip.cpp\n", encoding="utf-8")
    subprocess.run(["git", "init", "-q"], cwd=root, check=True)
    subprocess.run(["git", "add", "-A"], cwd=root, check=True)
    # Ненаписанный в индекс файл берётся наравне с индексом: иначе свежий обход шва падал бы только
    # на коммит-гейте, когда под него уже написан код.
    (root / "engine/new.cpp").write_text("int n;\n", encoding="utf-8")


def _run(title, want, verbose):
    if verbose or not want:
        print(f"  [{'PASS' if want else 'FAIL'}] {title}")
    return 0 if want else 1


def _anchor(audit, scan, verbose):
    """Якорь на НАСТОЯЩЕМ дереве: живая алгоритмическая форма обязана не быть находкой.

    Фикстура игрушечная по построению, и разбор скобок, случайно заточенный под неё, выглядел бы
    здоровым до первого прогона гейта. Форм в дереве не осталось — сказать вслух и пропустить:
    молчаливый пропуск читался бы как доказанное различение.
    """
    root = Path(__file__).resolve().parent.parent
    files = scan(root)
    algo = {p: len(ALGO.findall(t)) for p, t in files.items() if ALGO.search(t)}
    if not algo:
        print("  [SKIP] якорь: алгоритмической формы `std::remove` в дереве не осталось")
        return 0
    bad = [f for f in audit(files) if any(f.startswith(p + ":") for p in algo)]
    ok = _run(f"якорь: алгоритм в {len(algo)} файл(ах) дерева не находка", not bad, verbose)
    for finding in bad:
        print(f"         лишняя находка: {finding}")
    return ok


def selftest(audit, guards, scan, verbose=True):
    failures = 0
    for title, key, bad, ok in CASES:
        path = LOOKALIKE if "похожим именем" in title else BAD_PATH
        fired = [f for f in audit({**BASE, path: bad}) if key in f]
        silent = [f for f in audit({**BASE, path: ok}) if key in f]
        failures += _run(title, bool(fired) and not silent, verbose)
        if not fired:
            print("         правило промолчало на сломанной фикстуре")
        for finding in silent:
            print(f"         правило сработало на починенной фикстуре: {finding}")
    for title, text in QUIET:
        found = audit({**BASE, BAD_PATH: text})
        failures += _run(f"no-false-positive: {title}", not found, verbose)
        for finding in found:
            print(f"         лишняя находка: {finding}")
    # Опорный кейс охранников идёт ПЕРВЫМ: не пройди его исправное дерево — все три порчи ниже
    # отбивались бы чужим отказом, ничего не сказав о своём.
    failures += _run("охранники молчат на исправном дереве", not guards(FULL), verbose)
    for title, key, tree in GUARDS:
        tree = {k: v for k, v in tree.items() if v is not None}
        failures += _run(f"охранник: {title}", bool([m for m in guards(tree) if key in m]), verbose)
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        _fixture_repo(root)
        got = set(scan(root))
        want = {"engine/a.cpp", "docs/examples/c.cpp", "engine/new.cpp"}
        failures += _run("обход: корни, расширения, индекс и ненаписанное", got == want, verbose)
        if got != want:
            print(f"         лишнее: {sorted(got - want)}; недостача: {sorted(want - got)}")
    failures += _anchor(audit, scan, verbose)
    total = len(CASES) + len(QUIET) + len(GUARDS) + 3
    if verbose or failures:
        print(f"fs-seam selftest: {'FAIL' if failures else 'PASS'} — "
              f"{total} кейсов, провалов: {failures}")
    return 1 if failures else 0
