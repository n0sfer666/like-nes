"""Позитивный контроль гейта формы запуска python (`check_py_seam.py`).

Утверждение без фикстуры, где оно падает, неотличимо от отсутствующего. Поэтому каждая находка
ломается своим деревом: запрещённая форма в `.sh`, та же в `.yml`, написанная словом `python`,
взятая хвостом пути и собранная конвейером вместо heredoc.

Разрешённые формы — свои фикстуры, иначе правило неотличимо от «python на швах запрещён вовсе»:
`-c`, `-m`, тело файлом, `--version` (где `-` начинает ДРУГОЙ аргумент) и та самая форма в
КОММЕНТАРИИ — на ней стоит `py_run.sh`, объясняющий свою причину запрещённой формой.

Дерево фикстуры — настоящий git-репозиторий: обход берёт файлы у git, и подмена его на os.walk
означала бы, что набор проверяет не тот механизм, которым гейт пользуется на дереве. Сборщик взят у
соседнего набора целиком: репозиторий и поимённый `git add` — тот же механизм, а вторая его копия
разъехалась бы с первой молча.
"""
import os
import sys

from check_py_utf8_selftest import build

# Исправный вызов лежит в КАЖДОЙ фикстуре: дерево без вызовов python гейт отвергает как вакуумное, и
# без этого опорные `fail` падали бы по чужой причине, а не по проверяемой.
RUNNER = "#!/usr/bin/env bash\npython3 -c 'print(1)'\n"
SH = "#!/usr/bin/env bash\n%s\n"
YML = "jobs:\n  x:\n    steps:\n      - run: |\n          %s\n"
BODY = "cat > /tmp/b.py <<'EOF'\nprint(1)\nEOF\n"


def fixture(call, ext="sh"):
    """Дерево из одного скрипта с проверяемым вызовом плюс исправный вызов рядом."""
    name = "scripts/a." + ext
    text = (SH if ext == "sh" else YML) % call
    return {name: text, "scripts/runner.sh": RUNNER}


def selftest(gate, mkdtemp):
    bad = 0

    def case(want, name, files):
        nonlocal bad
        rc = gate(build(mkdtemp, files, runner=False), quiet=True)
        ok = (want == "pass" and rc == 0) or (want == "fail" and rc != 0)
        if ok:
            print("py-seam-selftest: OK   %s (%s)" % (name, want))
        else:
            sys.stderr.write("py-seam-selftest: БРАК %s: ожидали %s, код %d\n" % (name, want, rc))
            bad = 1

    case("pass", "тело приехало через -c", fixture("python3 -c 'print(1)'"))
    # Форма, на которую переведены call-site'ы: тело лежит файлом, лончеру нечего осматривать.
    case("pass", "тело приехало файлом", fixture(BODY + "python3 /tmp/b.py arg"))
    case("pass", "модуль приехал через -m", fixture("python3 -m json.tool /tmp/x.json"))
    # `-` начинает ДРУГОЙ аргумент: без этого `pass` правило ловило бы любой дефис за именем.
    case("pass", "ключ начинается с дефиса", fixture("python3 --version"))
    # То, на чём стоит сам шов: свою причину `py_run.sh` описывает запрещённой формой, и грep по
    # файлу целиком отбивал бы его за объяснение.
    case("pass", "запрещённая форма названа в комментарии",
         fixture("# раньше тут было python3 - <<'EOF'\npython3 -c 'print(1)'"))

    case("fail", "тело едет из stdin heredoc'ом", fixture("python3 - /tmp/x.json <<'EOF'\nEOF"))
    # Workflow — тот же шов процесса: на раннере Windows ровно тот же лончер в PATH.
    case("fail", "та же форма в workflow", fixture("python3 - /tmp/x.json <<'EOF'", ext="yml"))
    # Имя без тройки — тот же лончер: прибитая строка «python3» делала бы правило обходимым.
    case("fail", "форма написана словом python", fixture("python - /tmp/x.json <<'EOF'\nEOF"))
    # Хвост пути ломается так же, поэтому `/` перед именем запрет не снимает.
    case("fail", "вызов взят хвостом пути", fixture("/usr/bin/python3 - /tmp/x.json <<'EOF'\nEOF"))
    # Конвейер вместо heredoc: `-` стоит последним словом строки, и требование пробела за ним
    # пропустило бы эту форму, если бы конец строки не считался концом аргумента.
    case("fail", "тело едет конвейером", fixture("cat /tmp/b.py | python3 -"))

    # Пустое равно пустому: обход, промахнувшийся мимо дерева, обязан отличаться от чистого прогона.
    case("fail", "в дереве нет ни одного вызова python", {"scripts/a.sh": SH % "echo ok"})
    # Файл с мерой, скрытый от обхода: гейт описывает не то дерево, по которому его запустили, и
    # молчание тут читалось бы как «нарушений нет».
    case("fail", "файл меры скрыт от обхода",
         {"scripts/runner.sh": RUNNER, "scripts/py_run.sh": RUNNER,
          ".gitignore": "scripts/py_run.sh\n"})

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    rc = gate(root, quiet=True)
    if rc == 0:
        print("py-seam-selftest: OK   настоящее дерево проходит гейт (pass)")
    else:
        sys.stderr.write("py-seam-selftest: БРАК настоящее дерево: код %d\n" % rc)
        bad = 1

    if bad:
        sys.stderr.write("py-seam-selftest: FAIL\n")
        return 1
    print("py-seam-selftest: PASS")
    return 0
