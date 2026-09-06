"""Позитивный контроль гейта переключения вывода (`check_py_utf8.py`).

Утверждение, у которого нет фикстуры, где оно падает, неотличимо от отсутствующего, поэтому каждая
находка ломается своим деревом: кандидат без вызова, вызов в комментарии, импорт без вызова, вызов
без импорта, дерево без кандидатов вовсе и дерево, где собственный исходник гейта скрыт от обхода.

Дерево фикстуры — НАСТОЯЩИЙ git-репозиторий: обход берёт файлы у git, и подмена его на os.walk
означала бы, что набор проверяет не тот механизм, которым гейт пользуется на дереве.
"""
import os
import subprocess
import sys

SHEBANG = "#!/usr/bin/env python3\n"
CYR = 'print("вывод по-русски")\n'
GOOD = SHEBANG + "import py_utf8\n\n\ndef main():\n    py_utf8.enable()\n    " + CYR


def build(mkdtemp, files):
    """Git-репозиторий из словаря «путь -> текст». Файлы добавляются ПОИМЁННО."""
    root = mkdtemp()
    for rel, text in files.items():
        path = os.path.join(root, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)
    subprocess.run(["git", "init", "-q"], cwd=root, check=True)
    for rel in files:
        # Игнорируемый файл git добавить откажется, а фикстуре «исходник скрыт от обхода» он и
        # нужен неотслеженным: пропуск здесь ОСОЗНАННЫЙ, а не проглоченная ошибка.
        if subprocess.run(["git", "check-ignore", "-q", "--", rel], cwd=root).returncode == 0:
            continue
        subprocess.run(["git", "add", "--", rel], cwd=root, check=True)
    return root


def selftest(gate, mkdtemp):
    bad = 0

    def case(want, name, files):
        nonlocal bad
        rc = gate(build(mkdtemp, files), quiet=True)
        ok = (want == "pass" and rc == 0) or (want == "fail" and rc != 0)
        if ok:
            print("py-utf8-selftest: OK   %s (%s)" % (name, want))
        else:
            sys.stderr.write("py-utf8-selftest: БРАК %s: ожидали %s, код %d\n" % (name, want, rc))
            bad = 1

    case("pass", "кандидат зовёт py_utf8.enable()", {"scripts/a.py": GOOD})
    # Импортируемому модулю поток настраивает тот, кто его запустил, — требовать вызова у него
    # значило бы делать настройку побочным эффектом импорта.
    case("pass", "модуль без шебанга освобождён",
         {"scripts/a.py": GOOD, "scripts/lib.py": CYR})
    # Литералы, а не файл целиком: комментарии по-русски — стиль репозитория, и до вывода они не
    # доезжают. Без этого `pass` правило неотличимо от «зови всегда».
    case("pass", "шебанг с ASCII-литералами вызова не требует",
         {"scripts/a.py": GOOD, "scripts/b.py": SHEBANG + "# комментарий\nprint('ascii')\n"})
    case("fail", "кандидат не зовёт вовсе", {"scripts/a.py": SHEBANG + CYR})
    # Грep по подстроке был бы зелен на этом дереве: разбор обязан быть синтаксическим.
    case("fail", "вызов написан в комментарии",
         {"scripts/a.py": SHEBANG + "# py_utf8.enable()\n" + CYR})
    case("fail", "импорт есть, вызова нет", {"scripts/a.py": SHEBANG + "import py_utf8\n" + CYR})
    case("fail", "вызов есть, импорта нет",
         {"scripts/a.py": SHEBANG + "py_utf8.enable()\n" + CYR})
    # Пустое равно пустому: обход, промахнувшийся мимо дерева, обязан отличаться от чистого прогона.
    case("fail", "в дереве нет ни одного кандидата", {"scripts/lib.py": CYR})
    # Собственный исходник, скрытый от обхода: гейт описывает не то дерево, по которому его
    # запустили, и молчание тут читалось бы как «нарушений нет».
    case("fail", "исходник гейта скрыт от обхода",
         {"scripts/a.py": GOOD, "scripts/check_py_utf8.py": GOOD,
          ".gitignore": "scripts/check_py_utf8.py\n"})

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    rc = gate(root, quiet=True)
    if rc == 0:
        print("py-utf8-selftest: OK   настоящее дерево проходит гейт (pass)")
    else:
        sys.stderr.write("py-utf8-selftest: БРАК настоящее дерево: код %d\n" % rc)
        bad = 1

    if bad:
        sys.stderr.write("py-utf8-selftest: FAIL\n")
        return 1
    print("py-utf8-selftest: PASS")
    return 0
