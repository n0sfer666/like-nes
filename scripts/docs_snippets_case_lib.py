"""Механика кейса самопроверки врезок: фикстурное дерево, запуск гейта, вердикт набора.

Отделено от перечисления кейсов не счётчиком строк, а предметом: там — список того, что обязано
быть отбито, здесь — то, чем каждый кейс запускается. Перечисление упёрлось в ЖЁСТКИЙ лимит 250,
а он причиной не выкупается ни для кого.

Гейт зовётся ВНЕШНИМ процессом, а не импортом: предмет набора — вердикт целиком, вместе с вычиткой
списка документов со стдина и кодом возврата, и половина находок живёт именно там.
"""
import atexit
import os
import shutil
import subprocess
import sys
import tempfile

import posix_bash

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GATE = os.path.join(ROOT, "scripts", "check_docs_snippets.py")

SRC_OK = """// docs:begin(hello)
int hello() { return 1; }
// docs:end(hello)
"""

DOC_OK = """# doc

<!-- snippet: docs/examples/sample.cpp#hello -->
```cpp
int hello() { return 1; }
```
<!-- /snippet -->
"""
STATE = {"bad": 0}


def build(files):
    """Фикстурное дерево из словаря «путь -> текст». Каталоги создаются по пути файла: список
    каталогов, написанный отдельно, разъехался бы с самими файлами."""
    root = tempfile.mkdtemp()
    # Каталоги убираются за собой: прогон, оставляющий два десятка деревьев во временном каталоге,
    # приучает не смотреть на них вовсе, а гейт 4 спеки #11 требует чистого следа от локального
    # прогона (git status их не видит, поэтому напомнить о них некому).
    atexit.register(shutil.rmtree, root, True)
    for rel, text in files.items():
        path = os.path.join(root, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(text)
    return root


def gate(root, docs):
    """Код возврата гейта на дереве `root` со списком документов на стдине — и его вывод."""
    p = subprocess.run([sys.executable, GATE, root], input="\n".join(docs),
                       capture_output=True, text=True, encoding="utf-8")
    return p.returncode, (p.stdout or "") + (p.stderr or "")


def expect(want, name, root, docs):
    rc, out = gate(root, docs)
    ok = (want == "pass" and rc == 0) or (want == "fail" and rc != 0)
    if ok:
        print("docs-snippets-selftest: OK   %s (%s)" % (name, want))
    else:
        # Вывод гейта печатается ЦЕЛИКОМ: контроль, назвавший только код возврата, сообщает ФАКТ
        # отказа и молчит о причине. На своей ОС это лечится повторным запуском руками, а на чужом
        # раннере причина не восстанавливается вовсе — и стоит полного круга CI.
        sys.stderr.write("docs-snippets-selftest: БРАК %s: ожидали %s, код %d\n" % (name, want, rc))
        for line in out.splitlines():
            sys.stderr.write("docs-snippets-selftest:      | %s\n" % line)
        STATE["bad"] = 1


def case(want, name, doc=DOC_OK, src=SRC_OK, docs=("doc.md",), extra=None):
    files = {"doc.md": doc}
    if src is not None:
        files["docs/examples/sample.cpp"] = src
    if extra:
        files.update(extra)
    expect(want, name, build(files), list(docs))


def real_docs():
    """Список документов настоящего дерева — у той же `docs_all_files`, которой пользуется гейт.

    Bash берётся ОТВЕТОМ (`posix_bash`), а не именем в PATH: на windows-раннере первым `bash`
    стоит мост в WSL, и 2026-09-06 набор прочитал его сообщение как одиннадцать документов дерева.

    Отказ ВЫБОРА возвращается тем же кортежем, что и отказ обхода: исключение из библиотеки убило
    бы набор трейсбеком до вердикта, то есть «шелла не нашли» выглядело бы как упавший кейс.
    """
    try:
        shell = posix_bash.find()
    except RuntimeError as exc:
        return subprocess.CompletedProcess([], 127, "", "%s\n" % exc), []
    got = subprocess.run([shell, "-c", ". scripts/docs_content_lib.sh; docs_all_files ."],
                         cwd=ROOT, capture_output=True, text=True, encoding="utf-8")
    return got, [d for d in got.stdout.splitlines() if d.strip()]


def verdict():
    """Код возврата набора: имя упавшего названо выше, здесь — только исход."""
    if STATE["bad"]:
        sys.stderr.write("docs-snippets-selftest: FAIL\n")
        return 1
    print("docs-snippets-selftest: PASS")
    return 0

