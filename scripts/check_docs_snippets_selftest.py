#!/usr/bin/env python3
"""Позитивный контроль гейта врезок (гейт 3 спеки #19).

Утверждение, у которого нет фикстуры, где оно падает, неотличимо от отсутствующего, поэтому каждая
находка разбора ломается своим деревом: источника нет, источник вне `docs/examples/`, маркера нет,
маркер не закрыт, маркер объявлен дважды, тело разошлось, форма врезки испорчена, врезок нет вовсе.

Гейт зовётся ВНЕШНИМ процессом, а не импортом: предмет здесь — вердикт целиком, вместе с вычиткой
списка документов со стдина и кодом возврата, и половина находок живёт именно там.
"""
import atexit
import os
import shutil
import subprocess
import sys
import tempfile

import py_utf8

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

py_utf8.enable()
BAD = 0


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
    global BAD
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
        BAD = 1


def case(want, name, doc=DOC_OK, src=SRC_OK, docs=("doc.md",), extra=None):
    files = {"doc.md": doc}
    if src is not None:
        files["docs/examples/sample.cpp"] = src
    if extra:
        files.update(extra)
    expect(want, name, build(files), list(docs))


case("pass", "исправная врезка совпадает с телом маркера")

# Опорный кейс на МЕРУ: тело маркера с отступом (внутри функции, внутри `if`) попадает в документ
# без общего сдвига. Без снятия отступа исправная врезка расходилась бы с исправным источником.
case("pass", "общий отступ тела снят",
     src="""void f() {
    // docs:begin(hello)
    int hello() { return 1; }
    // docs:end(hello)
}
""")

# Врезка без `#имени` берёт файл ЦЕЛИКОМ — так в текст попадает ожидаемый вывод примера, и показать
# вывод, которого программа не печатает, документация не может.
case("pass", "врезка без имени берёт файл целиком",
     doc="""<!-- snippet: docs/examples/sample.out -->
```text
one
two
```
<!-- /snippet -->
""", src=None, extra={"docs/examples/sample.out": "one\ntwo\n"})

# Опорный кейс на закрытый список языков: `sh` показывает КОМАНДУ, компилируемого источника у неё не
# бывает, и требовать его значило бы запретить документации показывать вызов. Без этого `pass`
# утверждение о голых фенсах нельзя отличить от «запрещены все фенсы».
case("pass", "голый блок ```sh врезкой быть не обязан",
     doc=DOC_OK + "\n```sh\nbash scripts/build_check.sh\n```\n")

# Решение 5 спеки: «примерного кода» в документации нет. До этого утверждения гейт сверял ВРЕЗКИ и
# молчал про блок кода, написанный руками рядом с ними, — то есть инвариант держался дисциплиной.
case("fail", "голый блок ```cpp вне врезки",
     doc=DOC_OK + "\n```cpp\nint written_by_hand() { return 0; }\n```\n")

# Обратное направление той же связи: врезка без маркера — ошибка, а маркер, на который не ссылается
# никто, оставался тишиной. После переименования врезки он молча описывает вчерашний текст — ровно
# тот класс, что стережёт `assert_examples_paired` у соседнего гейта.
case("fail", "маркер источника не показан ни одной врезкой",
     src=SRC_OK + """// docs:begin(orphan)
int orphan() { return 2; }
// docs:end(orphan)
""")

# Префикс каталога проверяется ПОСЛЕ нормализации: строка начинается с `docs/examples/` и всё равно
# открывает файл вне его — тот же класс, который гейт документации закрыл у ссылок, уходящих выше
# корня дерева.
case("fail", "источник уходит из docs/examples/ через ..",
     doc=DOC_OK.replace("docs/examples/sample.cpp",
                        "docs/examples/../../engine/core/fixed.hpp"),
     src=None, extra={"engine/core/fixed.hpp": SRC_OK})

# Язык фенса против расширения источника: `.cpp` под ```text рендерится без подсветки, и заметить
# это может только читатель — то есть никто.
case("fail", "врезка из .cpp помечена ```text",
     doc=DOC_OK.replace("```cpp", "```text"))

# Маркеры источника разбираются и для врезки БЕЗ имени: незакрытый `docs:begin` забирает тело до
# конца файла, а показанный целиком файл проверку маркеров раньше не проходил вовсе.
case("fail", "незакрытый маркер в файле, показанном целиком",
     doc="""<!-- snippet: docs/examples/sample.cpp -->
```cpp
// docs:begin(hello)
int hello() { return 1; }
```
<!-- /snippet -->
""", src="// docs:begin(hello)\nint hello() { return 1; }\n")

# 1. Источника нет в дереве: врезка ссылается на файл, которого гейт 2 не собирает, потому что его
# нет вовсе.
case("fail", "источника нет в дереве", src=None)

# 2. Источник вне docs/examples/: про такой файл никто не утверждает, что он собирается и
# запускается, — то есть текст снова показывал бы код, за который никто не отвечает.
case("fail", "источник вне docs/examples/",
     doc=DOC_OK.replace("docs/examples/sample.cpp", "engine/core/fixed.hpp"),
     src=None, extra={"engine/core/fixed.hpp": SRC_OK})

# 3. Маркера с таким именем в источнике нет — гейт спеки требует ошибки именно на этом.
case("fail", "в источнике нет маркера с таким именем",
     src=SRC_OK.replace("hello", "other"))

# 4. Маркер открыт и не закрыт: тело кончалось бы концом файла, и врезка молча показывала бы всё до
# последней строки.
case("fail", "маркер открыт и не закрыт", src="// docs:begin(hello)\nint hello() { return 1; }\n")

# 5. Имя объявлено дважды: какое из двух тел показывать — вопрос без ответа, и молчаливый выбор
# первого означал бы, что правка второго не видна в документе никогда.
case("fail", "маркер объявлен дважды", src=SRC_OK + SRC_OK)

# Вложенность запрещена по той же причине: строки внутреннего маркера пришлось бы либо оставить во
# внешнем теле, либо выбросить, показав код с дырой.
case("fail", "маркер внутри незакрытого маркера",
     src="""// docs:begin(hello)
// docs:begin(inner)
int hello() { return 1; }
// docs:end(inner)
// docs:end(hello)
""")

case("fail", "docs:end без docs:begin", src="// docs:end(hello)\n" + SRC_OK)

# 6. Тело разошлось с источником — то, ради чего механизм и заведён: правка примера, не доехавшая до
# текста, есть документация, показывающая вчерашний код.
case("fail", "тело врезки разошлось с источником",
     doc=DOC_OK.replace("return 1", "return 2"))

# 7. Открывающий комментарий без закрывающего: конец тела определялся бы фенсом, а синхронизатор и
# гейт решали бы это по-разному — расходились бы они молча.
case("fail", "врезка не закрыта <!-- /snippet -->",
     doc=DOC_OK.replace("<!-- /snippet -->", ""))

# 8. Между комментариями нет fenced-блока: текст врезки уехал бы в документ как обычный абзац.
case("fail", "за врезкой нет открывающего ```",
     doc="""<!-- snippet: docs/examples/sample.cpp#hello -->
int hello() { return 1; }
<!-- /snippet -->
""")

case("fail", "блок врезки не закрыт ```",
     doc="""<!-- snippet: docs/examples/sample.cpp#hello -->
```cpp
int hello() { return 1; }
""")

# 9. Ноль врезок во всём наборе: механизм, который никто не употребляет, неотличим от сломанного
# разбора, а гейт при этом зелен (тот же класс, что vacuous-gate в ci_lint.py).
case("fail", "ни одной врезки во всём наборе", doc="# doc\n\nбез врезок\n")

# Пустой вход: обход, промахнувшийся мимо дерева, обязан отличаться от чистого прогона.
case("fail", "на входе ни одного документа", docs=())

# Документ назван, но его нет: список приходит извне, и рассинхрон с деревом обязан быть находкой.
case("fail", "названного документа нет в дереве", docs=("doc.md", "missing.md"))

# Фикстура игрушечная по построению, и утверждение, случайно заточенное под неё, выглядело бы
# здоровым ровно до первого прогона гейта. Список документов настоящего дерева берётся у той же
# docs_all_files, которой пользуется сам гейт.
real = subprocess.run(["bash", "-c", ". scripts/docs_content_lib.sh; docs_all_files ."],
                      cwd=ROOT, capture_output=True, text=True, encoding="utf-8")
real_docs = [d for d in real.stdout.splitlines() if d.strip()]
# Список приходит от ЧУЖОГО процесса, и его отказ отдаёт пустой список, на котором гейт отказывает
# по СВОЕЙ вакуумной ветке: кейс падал бы по чужой причине, неотличимо от сломанного разбора врезок.
if real.returncode != 0 or not real_docs:
    sys.stderr.write("docs-snippets-selftest: БРАК docs_all_files не отдала списка: код %d, "
                     "документов %d\n%s" % (real.returncode, len(real_docs), real.stderr))
    BAD = 1
else:
    expect("pass", "настоящее дерево проходит гейт", ROOT, real_docs)

if BAD:
    sys.stderr.write("docs-snippets-selftest: FAIL\n")
    sys.exit(1)
print("docs-snippets-selftest: PASS")
