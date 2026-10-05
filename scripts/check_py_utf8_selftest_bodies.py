"""Позитивный контроль третьего правила (`py_open_rule.py`) и встроенных тел (`py_embedded.py`).

Порча в каждой форме извлечения ловит только то, что тело ВООБЩЕ нашлось. Что оно нашлось
ПРАВИЛЬНО — разэкранированным, без отступа блока YAML, без табов `<<-`, — ловят опорные `pass` той
же формы с исправной кодировкой: сломанное извлечение отдаёт тело, которое не разбирается, и `pass`
падает отказом. Ревью 2026-10-04 доказало это мутантами: без таких `pass` все три поломки
извлечения проходили набор целиком.

Неизвестная форма запуска — отказ, и у каждой формы, которую извлечение знает, своя порча: иначе
форма, выпавшая из словаря, была бы неотличима от исправной.
"""
SH = "#!/usr/bin/env bash\n%s\n"
YML = "jobs:\n  x:\n    steps:\n      - run: |\n          %s\n"
OPEN = 'open("x")'
OK = 'open("x", encoding="utf-8")'


def cases(case, good, call):
    lib = lambda body: {"scripts/a.py": good, "scripts/lib.py": body}
    sh = lambda body: {"scripts/a.py": good, "scripts/b.sh": SH % body}
    yml = lambda body: {"scripts/a.py": good, ".github/w.yml": YML % body}

    # Без этих `pass` третье правило неотличимо от «encoding обязателен у любого open».
    case("pass", "байтовое открытие кодировки не требует", lib('open("x", "rb")\n'))
    case("pass", "кодировка передана позицией",
         lib('open("x", "r", -1, "utf-8")\nimport pathlib\npathlib.Path("x").read_text("utf-8")\n'))
    case("pass", "tempfile по умолчанию и Path.open('rb') бинарные",
         lib('import pathlib, tempfile\ntempfile.NamedTemporaryFile()\npathlib.Path("x").open("rb")\n'))
    case("fail", "текстовый open без encoding в .py", lib(OPEN + "\n"), "opens")
    case("fail", "read_text без encoding", lib('import pathlib\npathlib.Path("x").read_text()\n'),
         "opens")
    # Режим, не записанный литералом, — текстовый: иначе правило обходится переменной.
    case("fail", "режим передан переменной", lib('mode = "rb"\nopen("x", mode)\n'), "opens")
    case("fail", "encoding=None — та же локаль", lib('open("x", encoding=None)\n'), "opens")
    case("fail", ".open() у Path(...)", lib('from pathlib import Path\nPath("x").open()\n'), "opens")
    case("fail", ".open('w') у имени из Path",
         lib('from pathlib import Path\np = Path("x") / "y"\np.open("w")\n'), "opens")
    case("fail", "NamedTemporaryFile в текстовом режиме",
         lib('import tempfile\ntempfile.NamedTemporaryFile("w")\n'), "opens")
    case("fail", "TextIOWrapper без encoding", lib('import io\nio.TextIOWrapper(b)\n'), "opens")
    case("fail", "os.fdopen в текстовом режиме", lib('import os\nos.fdopen(3, "w")\n'), "opens")
    # Несвязанный вызов: первый позиционный — путь, а не кодировка.
    case("fail", "Path.read_text(p) несвязанным вызовом",
         lib('import pathlib\npathlib.Path.read_text(p)\n'), "opens")
    case("fail", "модуль .py не разбирается", lib("def (:\n"), "unparsed")

    case("pass", "открытие в комментарии .sh не судится", sh("# python3 -c '%s'" % OPEN))
    case("pass", "хвостовой комментарий не судится", sh("echo hi # python3 -c 'open(('"))
    case("pass", "текст чужого heredoc не судится", sh("cat <<EOF\npython3 -c '%s'\nEOF" % OPEN))
    # Heredoc у python СО скриптом кормит stdin данными: разбор их как кода отказывал бы на чужом
    # формате.
    case("pass", "heredoc скрипта — данные, а не тело", sh("python3 tool.py <<'EOF'\nне код (\nEOF"))
    case("pass", "-c двойных кавычек разэкранирован", sh(r'python3 -c "open(\"x\", encoding=\"utf-8\")"'))
    case("pass", "b'\\r\\n' в -c двойных кавычек остаётся литералом",
         sh(r'''python3 -c "d = open('x', 'rb').read().replace(b'\r\n', b'\n'); open('y', encoding='utf-8')"'''))
    case("pass", "<<- снимает табы, а не вложенный отступ",
         sh("py_run x <<-'PY'\n\tif 1:\n\t    %s\n\tPY" % OK))
    case("pass", "heredoc .yml без отступа блока",
         yml("py_run x <<'PY'\n          if 1:\n              %s\n          PY" % OK))
    # В `.sh` шелл сравнивает строку-терминатор целиком: `  PY` тело не обрывает.
    case("pass", "строка с отступом не терминатор в .sh",
         sh("py_run x <<'PY'\ns = '''\n  PY\n'''\n%s\nPY" % OK))
    case("pass", "py_run с переносом перед <<", sh("py_run x \\\n  <<'PY'\n%s\nPY" % OK))
    case("pass", '"$PY" - аргумент <<', sh('"$PY" - "$a" <<\'PY\'\n%s\nPY' % OK))

    case("fail", "open в heredoc py_run", sh("py_run x <<'PY'\n%s\nPY" % OPEN), "opens")
    case("fail", "open в heredoc <<-", sh("py_run x <<-'PY'\n\tif 1:\n\t    %s\n\tPY" % OPEN), "opens")
    case("fail", "open в -c двойных кавычек", sh(r'python3 -c "open(\"x\")"'), "opens")
    case("fail", "open в -c внутри .yml", yml("python3 -c '%s'" % OPEN), "opens")
    case("fail", "heredoc с отступом блока .yml",
         yml("py_run x <<'PY'\n            %s\n          PY" % OPEN), "opens")
    case("fail", "subprocess во встроенном теле",
         sh("py_run x <<'PY'\nimport subprocess\n" + call % "" + "PY"), "reads")
    case("fail", '"$PY" - аргумент <<', sh('"$PY" - "$a" <<\'PY\'\nimport subprocess\n' + call % ""
                                          + "PY"), "reads")
    case("fail", '"$PY" -c', sh("\"$PY\" -c '%s'" % OPEN), "opens")
    case("fail", "py -3 -c", sh("py -3 -c '%s'" % OPEN), "opens")
    case("fail", "python3 -X utf8 -c", sh("python3 -X utf8 -c '%s'" % OPEN), "opens")
    case("fail", "-c слитно с кавычкой", sh("python3 -c'%s'" % OPEN), "opens")
    case("fail", "python3 -u - <<", sh("python3 -u - <<'PY'\n%s\nPY" % OPEN), "opens")
    case("fail", "<<\\PY", sh("py_run x <<\\PY\n%s\nPY" % OPEN), "opens")
    case("fail", "<<'END-PY'", sh("py_run x <<'END-PY'\n%s\nEND-PY" % OPEN), "opens")

    # Тело, которого гейт не видит, не может считаться чистым — ни подстановка шелла, ни обрыв, ни
    # форма вне словаря извлечения.
    case("fail", "встроенное тело не разбирается", sh('python3 -c "print($x)"'), "unparsed")
    case("fail", "кавычка -c не закрыта", sh("python3 -c 'open("), "unparsed")
    case("fail", "-c $'…'", sh("python3 -c $'%s'" % OPEN), "unparsed")
    case("fail", "here-string <<<", sh("python3 <<< '%s'" % OPEN), "unparsed")
    case("fail", "два heredoc в одной команде",
         sh("py_run x <<'A' 3<<'B'\n%s\nA\nB" % OK), "unparsed")
    case("fail", "heredoc без терминатора", sh("py_run x <<'PY'\n%s" % OK), "unparsed")
    case("fail", "python3 - без heredoc", sh("echo '%s' | python3 -" % OK), "unparsed")
    case("fail", "шаг shell: python",
         {"scripts/a.py": good, ".github/w.yml":
          "jobs:\n  x:\n    steps:\n      - shell: python\n        run: print(1)\n"}, "unparsed")

    # Номер строки в отчёте — то, по чему чинят: склейка `\`+перенос и сдвиг тела heredoc его не
    # сбивают.
    case("fail", "номер строки за склейкой -c",
         sh('python3 -c "import sys; \\\nx = 1\nopen(\\"x\\")"'), "opens", "scripts/b.sh:4")
    case("fail", "номер строки в теле heredoc",
         sh("py_run x <<'PY'\nimport os\n%s\nPY" % OPEN), "opens", "scripts/b.sh:4")

    case("fail", "в дереве нет ни одного текстового open",
         {"scripts/a.py": good, "scripts/runner.sh": SH % "python3 -c 'print(1)'"}, "vacuous")
    case("fail", "в дереве нет ни одного встроенного тела",
         {"scripts/a.py": good, "scripts/runner.sh": "", "scripts/lib.py": OK + "\n"}, "vacuous")
