"""Прогон фикстур: правило обязано сработать на сломанной половине и промолчать на починенной.

Правило, которое не падает на поломке, не проверяет ничего — и молчит ровно так же, как правило
исправное. Поэтому самопроверка гоняется перед КАЖДЫМ разбором файлов, а не по запросу.
"""
from ci_lint_fixtures import CASES as RUNNER_CASES, QUIET as RUNNER_QUIET
from ci_lint_fixtures_gates import CASES as GATE_CASES
from ci_lint_fixtures_args import CASES as ARG_CASES, QUIET as ARG_QUIET
from ci_lint_fixtures_lists import CASES as LIST_CASES, SWEEP_CASES
from ci_lint_lists import analyze, check

CASES = RUNNER_CASES + GATE_CASES + ARG_CASES
QUIET = RUNNER_QUIET + ARG_QUIET


def _report(rule, title, fired, silent, verbose):
    status = "PASS" if fired and not silent else "FAIL"
    if verbose or status == "FAIL":
        print(f"  [{status}] {rule}: {title}")
    if not fired:
        print("         правило промолчало на сломанной фикстуре")
    for finding in silent:
        print(f"         правило сработало на починенной фикстуре: {finding}")
    return status == "FAIL"


def _lists(verbose):
    """Сверка списков берёт ДВА текста разом — workflow и его эталон, — поэтому у неё своя пара
    циклов. Чтение эталона подменяется словарём: диска эти кейсы не касаются."""
    failures = 0
    for rule, title, broken, fixed in LIST_CASES:
        bt, br = broken
        ft, fr = fixed
        failures += _report(rule, title, analyze("fixture.yml", bt, br.get),
                            analyze("fixture.yml", ft, fr.get), verbose)
    for rule, title, broken, fixed in SWEEP_CASES:
        failures += _report(rule, title,
                            check([k for k in broken if k.endswith(".yml")], broken.get),
                            check([k for k in fixed if k.endswith(".yml")], fixed.get), verbose)
    return failures


def selftest(lint, verbose=True):
    failures = _lists(verbose)
    for rule, title, broken, fixed in CASES:
        failures += _report(rule, title,
                            [f for f in lint("fixture.yml", broken) if f.rule == rule],
                            [f for f in lint("fixture.yml", fixed) if f.rule == rule], verbose)
    for title, text in QUIET:
        found = lint("fixture.yml", text)
        status = "PASS" if not found else "FAIL"
        failures += status == "FAIL"
        if verbose or status == "FAIL":
            print(f"  [{status}] no-false-positive: {title}")
        for finding in found:
            print(f"         лишняя находка: {finding}")
    if verbose or failures:
        print(f"ci-lint selftest: {'FAIL' if failures else 'PASS'} — "
              f"{len(CASES) + len(QUIET) + len(LIST_CASES) + len(SWEEP_CASES)} кейсов, "
              f"провалов: {failures}")
    return 1 if failures else 0
