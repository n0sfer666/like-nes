#!/usr/bin/env python3
"""Статический разбор .github/workflows: классы отказов, которые видны только в CI.

Каждое правило здесь — это уже случившаяся красная сборка, а не гипотеза. Цена ошибки такого
класса измеряется прогонами CI по 20+ минут на трёх ОС, поэтому проверка обязана быть локальной
и мгновенной. Самопроверка правил — `--selftest`.
"""
import sys
from pathlib import Path

# Соседние модули — до их импорта: скрипт зовут по пути (`python3 scripts/ci_lint.py`) из корня,
# и каталога scripts/ в sys.path тогда нет.
sys.path.insert(0, str(Path(__file__).resolve().parent))

from ci_lint_lists import check as check_lists  # noqa: E402
from ci_lint_rules import RULES  # noqa: E402
from ci_workflow import parse  # noqa: E402


def lint(path, text):
    return [f for step in parse(path, text) for rule in RULES for f in rule(step)]


def main(argv):
    from ci_lint_selftest import selftest
    # Кодировка задаётся явно с обоих концов, потому что на Windows locale-дефолт — cp1251, а не
    # UTF-8, и оба конца ломались по-разному: чтение workflow падало `UnicodeDecodeError` на первом
    # же не-ASCII байте (гейт не отработал вовсе), а вывод в перенаправленный отчёт уезжал в
    # cp1251 и читался кракозябрами. Ни то, ни другое не воспроизводится ни на одном раннере CI:
    # там локаль UTF-8.
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")
    if "--selftest" in argv:
        return selftest(lint)
    # Самопроверка перед каждым прогоном: сломанное правило молчит ровно так же, как чистый
    # workflow, и линтер незаметно превращается в декорацию — тот же класс, что вакуумный гейт.
    if selftest(lint, verbose=False):
        print("ci-lint: FAIL — сломаны сами правила, находкам верить нельзя")
        return 1
    root = Path(__file__).resolve().parent.parent
    workflows = root / ".github/workflows"
    files = sorted(p for mask in ("*.yml", "*.yaml") for p in workflows.glob(mask))
    if not files:
        # Позитивный контроль на сам линтер: пустой список — это переехавший каталог, а не
        # проект без CI, и «0 находок» тут значит «ничего не проверено».
        print(f"ci-lint: FAIL — в {workflows} не найдено ни одного workflow")
        return 1
    rel = [p.relative_to(root).as_posix() for p in files]

    def read(path):
        target = root / path
        return target.read_text(encoding="utf-8") if target.is_file() else None

    findings = [f for p in rel for f in lint(p, read(p))]
    # Сверка руками написанных списков с их источником правды: она читает ДВА файла разом и
    # шагом workflow не ограничена, поэтому в общий проход правил не встраивается.
    findings += check_lists(rel, read)
    for finding in findings:
        print(finding)
    print(f"ci-lint: {'FAIL' if findings else 'PASS'} — "
          f"{len(files)} workflow, находок: {len(findings)}")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
