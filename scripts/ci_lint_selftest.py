"""Прогон фикстур: правило обязано сработать на сломанной половине и промолчать на починенной.

Правило, которое не падает на поломке, не проверяет ничего — и молчит ровно так же, как правило
исправное. Поэтому самопроверка гоняется перед КАЖДЫМ разбором файлов, а не по запросу.
"""
from ci_lint_fixtures import CASES as RUNNER_CASES, QUIET
from ci_lint_fixtures_gates import CASES as GATE_CASES

CASES = RUNNER_CASES + GATE_CASES


def selftest(lint, verbose=True):
    failures = 0
    for rule, title, broken, fixed in CASES:
        fired = [f for f in lint("fixture.yml", broken) if f.rule == rule]
        silent = [f for f in lint("fixture.yml", fixed) if f.rule == rule]
        status = "PASS" if fired and not silent else "FAIL"
        failures += status == "FAIL"
        if verbose or status == "FAIL":
            print(f"  [{status}] {rule}: {title}")
        if not fired:
            print("         правило промолчало на сломанной фикстуре")
        for finding in silent:
            print(f"         правило сработало на починенной фикстуре: {finding}")
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
              f"{len(CASES) + len(QUIET)} кейсов, провалов: {failures}")
    return 1 if failures else 0
