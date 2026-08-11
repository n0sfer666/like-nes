"""Фикстуры бюджета длины: каждое правило обязано сработать на поломке и промолчать на починке.

Тот же контракт, что у `ci_lint_selftest.py`, и по той же причине: гейт, чьё правило сломано,
молчит неотличимо от гейта, которому нечего сказать. Фикстуры синтетические — правила чистые
функции от (файлы, allowlist), так что ни каталогов, ни git'а тут не нужно.

Ключ поиска — подстрока, которую обязана назвать находка. Сравнивать с полным текстом значило бы
пинить формулировку, и первая же правка сообщения красила бы самопроверку, ничего не проверив.
"""
from line_budget_allow import HARD, SOFT

GOOD = (SOFT + 3, "single", "причина ровно из нескольких слов")

# (метка, ключ в находке, сломанные файлы, сломанный allowlist, починенные файлы, починенный allow)
CASES = [
    ("над жёстким лимитом", "ЖЁСТКОГО",
     {"a.cpp": HARD + 1}, {"a.cpp": (HARD + 1, "single", "причина из нескольких слов тут")},
     {"a.cpp": HARD}, {"a.cpp": (HARD, "single", "причина из нескольких слов тут")}),
    ("над мягким без записи", "нет записи",
     {"a.cpp": SOFT + 1}, {},
     {"a.cpp": SOFT}, {}),
    ("вырос за выписанный бюджет", "выписанном бюджете",
     {"a.cpp": SOFT + 9}, {"a.cpp": GOOD},
     {"a.cpp": SOFT + 3}, {"a.cpp": GOOD}),
    ("запись на несуществующий файл", "файла в дереве нет",
     {"b.cpp": 10}, {"a.cpp": GOOD},
     {"b.cpp": 10, "a.cpp": SOFT + 3}, {"a.cpp": GOOD}),
    ("протухшая запись", "пора убрать",
     {"a.cpp": SOFT - 1}, {"a.cpp": GOOD},
     {"a.cpp": SOFT + 3}, {"a.cpp": GOOD}),
    ("бюджет выше жёсткого", "выше жёсткого лимита",
     {"a.cpp": SOFT + 3}, {"a.cpp": (HARD + 1, "single", "причина из нескольких слов тут")},
     {"a.cpp": SOFT + 3}, {"a.cpp": GOOD}),
    ("вид не из списка", "не из закрытого списка",
     {"a.cpp": SOFT + 3}, {"a.cpp": (SOFT + 3, "потому что так", "причина из нескольких слов")},
     {"a.cpp": SOFT + 3}, {"a.cpp": GOOD}),
    ("причина-отписка", "отписка",
     {"a.cpp": SOFT + 3}, {"a.cpp": (SOFT + 3, "single", "надо")},
     {"a.cpp": SOFT + 3}, {"a.cpp": GOOD}),
]

# Чистые входы: находок быть не должно ни одной. Гейт, который ругается на честное дерево, снимают
# целиком — и вместе с ним уходит всё, что он ловил.
QUIET = [
    ("файл ровно на мягком лимите", {"a.cpp": SOFT}, {}),
    ("выписанный файл ровно в своём бюджете", {"a.cpp": SOFT + 3}, {"a.cpp": GOOD}),
    ("пустое дерево без разрешений", {}, {}),
]


# Отдельная пара фикстур для VENDORED: правило смотрит не на длины, а на листинг дерева, и общий
# кортеж CASES ему не подходит. (метка, сломанный листинг, починенный листинг, вендор)
VENDORED_CASES = [
    ("запись VENDORED на несуществующий путь",
     {"engine/other.cpp"}, {"engine/other.cpp", "vendor/blob.c"}, {"vendor/blob.c"}),
]


def selftest(audit, audit_vendored, verbose=True):
    failures = 0
    for title, bad, ok, vendored in VENDORED_CASES:
        fired = audit_vendored(bad, vendored)
        silent = audit_vendored(ok, vendored)
        status = "PASS" if fired and not silent else "FAIL"
        failures += status == "FAIL"
        if verbose or status == "FAIL":
            print(f"  [{status}] {title}")
        if not fired:
            print("         правило промолчало на сломанной фикстуре")
        for finding in silent:
            print(f"         правило сработало на починенной фикстуре: {finding}")
    for title, key, bad_files, bad_allow, ok_files, ok_allow in CASES:
        fired = [f for f in audit(bad_files, bad_allow) if key in f]
        silent = [f for f in audit(ok_files, ok_allow) if key in f]
        status = "PASS" if fired and not silent else "FAIL"
        failures += status == "FAIL"
        if verbose or status == "FAIL":
            print(f"  [{status}] {title}")
        if not fired:
            print("         правило промолчало на сломанной фикстуре")
        for finding in silent:
            print(f"         правило сработало на починенной фикстуре: {finding}")
    for title, files, allow in QUIET:
        found = audit(files, allow)
        status = "PASS" if not found else "FAIL"
        failures += status == "FAIL"
        if verbose or status == "FAIL":
            print(f"  [{status}] no-false-positive: {title}")
        for finding in found:
            print(f"         лишняя находка: {finding}")
    if verbose or failures:
        print(f"line-budget selftest: {'FAIL' if failures else 'PASS'} — "
              f"{len(CASES) + len(QUIET) + len(VENDORED_CASES)} кейсов, провалов: {failures}")
    return 1 if failures else 0
