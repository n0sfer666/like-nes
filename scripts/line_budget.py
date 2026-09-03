#!/usr/bin/env python3
"""Бюджет длины файлов (правило 5): мягкий лимит с письменной причиной, жёсткий — без исключений.

Лимит, который не проверяет никто, лимитом не является: к моменту заведения этого гейта за мягкие
200 строк перевалили тринадцать файлов дерева, а `scripts/preflight.sh` тихо дорос до 297 при
жёстком пороге 250. Здесь он становится механическим — и, как остальные гейты этого репозитория,
несёт позитивный контроль: обход, потерявший файлы, обязан падать, а не печатать «находок нет».

Правила проверяются сломанными фикстурами перед каждым прогоном (`--selftest`).
"""
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from line_budget_allow import ALLOW  # noqa: E402
from line_budget_rules import HARD, KINDS, SOFT, VENDORED  # noqa: E402

# Область действия названа списком ВКЛЮЧЕНИЯ, и это осознанное сужение: правило 5 — про авторский
# код, а `.md`, `.txt` и `.yml` под него не попадают по самому его тексту («текст, читаемый целиком
# сверху вниз»). Так что `ci.yml` на 1200 строк и runbook'и молчат здесь не по недосмотру — молчат
# они и в CLAUDE.md. Список расширять — отдельное решение: с ним первым же прогоном покраснеет
# workflow и половина docs/, и это будет НЕ находка про длину, а смена области.
EXTS = {".c", ".cc", ".cpp", ".h", ".hpp", ".mm", ".bat", ".cmake", ".py", ".sh"}
NAMES = {"CMakeLists.txt"}
# Позитивный контроль обхода: файл, который обязан быть найден всегда. Число найденных файлов
# порогом тут не годится — оно плывёт с деревом, а «переехал корень» и «дерево похудело» обязаны
# различаться. Ссылка на себя точна: гейт, не увидевший собственный исходник, не увидел ничего.
SELF = "scripts/line_budget.py"
MIN_WORDS = 3


def audit(files, allow):
    """files: {путь: строк}. Возвращает список находок строками."""
    out = []
    for path, lines in sorted(files.items()):
        entry = allow.get(path)
        if lines > HARD:
            out.append(f"{path}: {lines} строк — выше ЖЁСТКОГО лимита {HARD}. "
                       f"Жёсткий лимит не выкупается причиной: файл разбивается или задача "
                       f"останавливается и спрашивает.")
            continue
        if lines <= SOFT or entry is None:
            if lines > SOFT:
                kinds = "; ".join(f"{k} — {v}" for k, v in KINDS.items())
                out.append(f"{path}: {lines} строк — выше мягкого лимита {SOFT} и нет записи в "
                           f"ALLOW. Либо разбить, либо записать причину из закрытого списка: "
                           f"{kinds}.")
            continue
        budget, kind, reason = entry
        if lines > budget:
            out.append(f"{path}: {lines} строк при выписанном бюджете {budget}. Рост за пределы "
                       f"выписанного — это новое решение, а не продолжение старого: либо ужать, "
                       f"либо поднять число в ALLOW осознанно.")
    for path, (budget, kind, reason) in sorted(allow.items()):
        lines = files.get(path)
        if lines is None:
            out.append(f"{path}: запись в ALLOW есть, а файла в дереве нет — переименован или "
                       f"удалён, разрешение висит ни на чём.")
            continue
        if lines <= SOFT:
            out.append(f"{path}: {lines} строк, это уже в пределах мягкого лимита {SOFT} — "
                       f"запись в ALLOW пора убрать, разрешение больше не нужно.")
        if budget > HARD:
            out.append(f"{path}: бюджет {budget} выписан выше жёсткого лимита {HARD} — таких "
                       f"разрешений не бывает.")
        if kind not in KINDS:
            out.append(f"{path}: вид обоснования '{kind}' не из закрытого списка "
                       f"({', '.join(sorted(KINDS))}).")
        if len(reason.split()) < MIN_WORDS:
            out.append(f"{path}: причина из {len(reason.split())} слов(а) — отписка не считается "
                       f"обоснованием, нужно минимум {MIN_WORDS}.")
    return out


def audit_vendored(listing, vendored):
    """Пути вендоренного против дерева: обход выкидывает их ДО всех правил, включая жёсткий порог.

    Это единственный способ пройти гейт бесследно, поэтому запись здесь обязана указывать на живой
    файл: переименованный или удалённый вендор оставляет строку, которая молча гасит будущий файл
    с тем же путём. Причина словами тут не спрашивается — чужой код и есть причина, — но висеть
    ни на чём ей нельзя, ровно как записи в ALLOW.
    """
    return [f"{path}: путь в VENDORED есть, а файла в дереве нет — вендор переименован или удалён, "
            f"а строка продолжает молча гасить этот путь."
            for path in sorted(vendored) if path not in listing]


def scan(root):
    """Наши исходники глазами git: сборочные каталоги, deps/ и прочий мусор отсекает .gitignore.

    Ненаписанное в индекс берётся наравне с индексом (`--others --exclude-standard`): иначе
    свежесозданный файл на четыреста строк проходил бы локальный прогон молча и падал бы только
    на коммит-гейте, когда его уже добавили, — то есть после того, как под него написан код.
    """
    out = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        capture_output=True, text=True, check=True).stdout
    listing = {name for name in out.split("\0") if name}
    files = {}
    for name in listing:
        if name in VENDORED:
            continue
        path = Path(name)
        if path.suffix not in EXTS and path.name not in NAMES:
            continue
        blob = root / path
        if not blob.is_file():
            continue
        files[name] = len(blob.read_bytes().splitlines())
    return files, listing


def main(argv):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")
    from line_budget_selftest import selftest
    if "--selftest" in argv:
        return selftest(audit, audit_vendored)
    # Та же дисциплина, что у ci_lint.py: сломанное правило молчит ровно так же, как чистое дерево.
    if selftest(audit, audit_vendored, verbose=False):
        print("line-budget: FAIL — сломаны сами правила, находкам верить нельзя")
        return 1
    root = Path(__file__).resolve().parent.parent
    files, listing = scan(root)
    if SELF not in files:
        print(f"line-budget: FAIL — обход не нашёл даже {SELF}: искать было негде, "
              f"и 'находок нет' здесь значит 'ничего не проверено'")
        return 1
    findings = audit(files, ALLOW) + audit_vendored(listing, VENDORED)
    for finding in findings:
        print(finding)
    print(f"line-budget: {'FAIL' if findings else 'PASS'} — {len(files)} файл(ов), "
          f"выписано разрешений: {len(ALLOW)}, находок: {len(findings)}")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
