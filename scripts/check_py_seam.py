#!/usr/bin/env python3
"""Гейт: python, запускаемый со шва процесса, получает тело ФАЙЛОМ или `-c`, но не из stdin.

    python3 scripts/check_py_seam.py             # гейт
    python3 scripts/check_py_seam.py --selftest  # правило проверяется сломанными фикстурами

`python3` в PATH Windows — это ЛОНЧЕР (PyManager, Store-заглушка), и рантайм он выбирает, осматривая
аргументы на шебанг. У формы `python3 - <путь>` первым аргументом идёт путь к `.sh`-фикстуре, её
`#!/usr/bin/env bash` лончер и находит: вместо heredoc запускается bash, причём кодом НОЛЬ. Подмена
молча не происходит, вызывающий читает успех. Прогон preflight 2026-10-03 нашёл этим девять порч в
шести наборах, и спасла их только сверка `cksum` до и после.

Мера против этого — `scripts/py_run.sh` — была заведена БЕЗ гейта, то есть ровно списком без
эталона: ничто не мешало следующему call-site'у написать прежнюю форму заново, и один такой в дереве
остался — `ci_pick_run` в release_ci_lib.sh, защищённый лишь тем, что его argv[1] оказался `.json`
без шебанга. Это случайность данных, а не мера. Теперь форму судит гейт — то же основание, по
которому заведены `py_utf8.py` с `check_py_utf8.py` и `posix_bash.py` со своей самопроверкой.

Формы `-c` и «тело файлом» лончер не перехватывает: у первого аргумента расширения `.py` или шебанга
нет, осмотр на нём и заканчивается. Поэтому запрещена РОВНО одна форма, а не вызов python вообще.

Область — `.sh`, `.yml`, `.yaml`: тело из stdin требует heredoc или конвейера, а это конструкции
POSIX-шелла и workflow. У `cmd.exe` heredoc'а нет, и `.bat` сюда не попадает по построению.

Комментарные СТРОКИ снимаются перед разбором: своё основание этот шов описывает той самой формой,
которую запрещает, и грep по файлу целиком отбивал бы `py_run.sh` за объяснение. Комментарий — `#` в
начале слова, то же определение, что в `ci_lint_rules.py`.
"""
import os
import re
import subprocess
import sys
import tempfile

import py_utf8

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MEASURE = "scripts/py_run.sh"
GLOBS = ("*.sh", "*.yml", "*.yaml")
# Хвост пути тоже вызов: `/usr/bin/python3 -` ломается точно так же, поэтому `/` в запрет перед
# именем не входит. Входят `\w`, `.` и `-`: ими начинаются `mypython3`, `x.python3` и `--python3`.
NAME = r"(?<![\w.\-])(?:python|python3)(?:\.exe)?"
CALL = re.compile(NAME + r"(?=\s)")
# `-` аргументом целиком: `--version` и `-c` отсекает требование пробела или конца строки за ним.
STDIN = re.compile(NAME + r"\s+-(?=\s|$)")
COMMENT = re.compile(r"^\s*#")


def tracked(root):
    """Файлы швов у git ВМЕСТЕ с ненаписанными в индекс: свежесозданный скрипт иначе проходил бы
    локальный прогон молча и падал бы только на коммит-гейте, когда под него уже написан код."""
    out = subprocess.run(["git", "ls-files", "--cached", "--others", "--exclude-standard", "--"]
                         + list(GLOBS), cwd=root, capture_output=True, text=True, encoding="utf-8")
    return sorted(p for p in out.stdout.splitlines() if p.strip())


def scan(text):
    """Сколько вызовов python в файле и в каких строках тело едет из stdin."""
    seen, bad = 0, []
    for number, line in enumerate(text.splitlines(), 1):
        if COMMENT.match(line):
            continue
        seen += len(CALL.findall(line))
        if STDIN.search(line):
            bad.append(number)
    return seen, bad


def gate(root, quiet=False):
    seen, bad = 0, []
    files = tracked(root)
    for rel in files:
        path = os.path.join(root, rel)
        if not os.path.isfile(path):
            continue
        found, lines = scan(open(path, encoding="utf-8").read())
        seen += found
        bad += ["%s:%d" % (rel, line) for line in lines]
    # Пустое равно пустому: обход, промахнувшийся мимо дерева, обязан отличаться от чистого
    # прогона — тот же класс, что правило vacuous-gate в ci_lint.py.
    if not seen:
        sys.stderr.write("py-seam: FAIL — обход не нашёл ни одного вызова python в %s, "
                         "проверять нечего\n" % ", ".join(GLOBS))
        return 1
    # Гейт, не нашедший файла с мерой, описывает не то дерево, по которому его запустили.
    if os.path.isfile(os.path.join(root, MEASURE)) and MEASURE not in files:
        sys.stderr.write("py-seam: FAIL — обход не видит %s, где живёт мера\n" % MEASURE)
        return 1
    for place in bad:
        sys.stderr.write("%s: тело python едет из stdin (`python3 - <путь>`) — на Windows лончер "
                         "выберет рантайм по шебангу АРГУМЕНТА и запустит не python, вернув ноль. "
                         "Тело файлом (`py_run` из %s) либо `-c`\n" % (place, MEASURE))
    if bad:
        sys.stderr.write("py-seam: FAIL — вызовов осмотрено: %d, находок: %d\n" % (seen, len(bad)))
        return 1
    if not quiet:
        print("py-seam: ok (%d вызов(ов) python на швах процесса, тело ни у одного не из stdin)"
              % seen)
    return 0


def selftest():
    from check_py_seam_selftest import selftest as run
    return run(gate, tempfile.mkdtemp)


if __name__ == "__main__":
    py_utf8.enable()
    if len(sys.argv) > 1 and sys.argv[1] == "--selftest":
        sys.exit(selftest())
    sys.exit(gate(ROOT))
