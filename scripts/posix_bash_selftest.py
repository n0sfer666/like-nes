#!/usr/bin/env python3
"""Позитивный контроль выбора bash (`posix_bash.py`).

Утверждение, у которого нет фикстуры, где оно падает, неотличимо от отсутствующего, а падает это
на ОДНОЙ ОС из трёх: мост WSL стоит в PATH только на Windows, и без порчи о выборе не было бы
известно ничего — ровно так дефект и дожил до раннера, где `bash` оказался не шеллом.

Кандидат подставляется ЗАГЛУШКОЙ в PATH: предмет здесь — реакция выбора на чужой исход, а не
устройство настоящего шелла. Заглушка ведёт себя как наблюдённый мост — печатает своё и
возвращает единицу.
"""
import atexit
import os
import shutil
import stat
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import posix_bash  # noqa: E402
import py_utf8  # noqa: E402

py_utf8.enable()
BAD = 0
ROOM = tempfile.mkdtemp()
# Каталог убирается и на аварийном выходе: набор роняет ЛЮБОЙ отказ ниже, а финальная строка
# уборки до него тогда не доезжает — след прогона оставался бы во временном каталоге машины.
atexit.register(shutil.rmtree, ROOM, True)


def stub(name, out, code):
    """Каталог с единственным кандидатом `bash`, который печатает `out` и выходит кодом `code`.

    Ветвление по ОС здесь неизбежно и предметно: на Windows исполняемым кандидатом может быть
    только файл из PATHEXT-набора, настоящий PE фикстуре взять негде.
    """
    room = os.path.join(ROOM, name)
    os.makedirs(room, exist_ok=True)
    if os.name == "nt":
        path = os.path.join(room, "bash.cmd")
        text = "@echo off\r\necho %s\r\nexit /b %d\r\n" % (out, code)
    else:
        path = os.path.join(room, "bash")
        text = "#!/bin/sh\nprintf '%%s' '%s'\nexit %d\n" % (out, code)
    with open(path, "w", encoding="utf-8", newline="") as fh:
        fh.write(text)
    os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    return room


def only(room):
    """Путь единственного кандидата каталога — им же и назовётся ожидаемый выбор."""
    found = posix_bash.candidates(room)
    return found[0] if found else None


def expect(want, name, path, chosen=None, says=None):
    """`want` — «выбор состоялся» или «выбор отказал», при PATH равном `path`.

    `says` требует подстроку в тексте отказа: отказ, не назвавший ни причины, ни проверенных
    кандидатов, восстанавливается на чужом раннере ровно ничем — ради этого модуль и заведён.
    """
    global BAD
    keep = os.environ["PATH"]
    os.environ["PATH"] = path
    try:
        got, err = posix_bash.find(), None
    except RuntimeError as exc:
        got, err = None, str(exc)
    finally:
        os.environ["PATH"] = keep
    if want == "pass":
        ok = got is not None and (chosen is None or got == chosen)
    else:
        ok = got is None and (says is None or says in (err or ""))
    if ok:
        print("posix-bash-selftest: OK   %s (%s)" % (name, want))
    else:
        sys.stderr.write("posix-bash-selftest: БРАК %s: ожидали %s, выбран %r, отказ %r\n"
                         % (name, want, got, err))
        BAD = 1


# Формы пути маркера утверждаются ОБЕ и на любой ОС: `C:/…` понимает git-bash, `/c/…` — он же в
# своей раскладке, а WSL не понимает ни одной (тот же диск лежит у него в `/mnt/c`). Форма, которую
# на машине набора не проверить запуском, иначе доехала бы до раннера непроверенной.
FORMS = posix_bash.spellings("C:\\Users\\runner\\AppData\\Local\\Temp\\m", nt=True)
if FORMS != ["C:/Users/runner/AppData/Local/Temp/m", "/c/Users/runner/AppData/Local/Temp/m"]:
    sys.stderr.write("posix-bash-selftest: БРАК формы windows-пути: %r\n" % FORMS)
    BAD = 1
elif posix_bash.spellings("/tmp/m", nt=False) != ["/tmp/m"]:
    sys.stderr.write("posix-bash-selftest: БРАК путь posix размножен формами\n")
    BAD = 1
else:
    print("posix-bash-selftest: OK   маркер назван обеими формами диска хоста (pass)")

BRIDGE = stub("bridge", "Windows Subsystem for Linux has no installed distributions.", 1)

# Заглушка, которую перебор не ВИДИТ, читалась бы как «выбор её отбил»: ветка «только мост»
# отказала бы по причине соседнего кейса — «в PATH ни одного кандидата», — ничего не доказав.
if not posix_bash.candidates(BRIDGE):
    sys.stderr.write("posix-bash-selftest: БРАК заглушка не видна перебору: %s\n" % BRIDGE)
    BAD = 1

# Заглушка, которую перебор видит, но НЕ ЗАПУСКАЕТ, тоже читалась бы как «выбор её отбил»: все
# порчи ниже ждут отказа, и отказ по невозможности запуска неотличим от отказа по ответу.
HONEST = stub("honest", posix_bash.MARK, 0)
expect("pass", "честная заглушка запускается и выбирается", HONEST, only(HONEST))

# Якорь на настоящем дереве: на машине, где набор гоняется, выбор обязан состояться настоящим
# шеллом, а не заглушкой. Без него все порчи проходили бы и на выборе, который не умеет ничего.
try:
    REAL = posix_bash.find()
    print("posix-bash-selftest: OK   на этой машине выбор возвращает отвечающий bash (pass)")
except RuntimeError as exc:
    REAL = None
    sys.stderr.write("posix-bash-selftest: БРАК выбор отказал на настоящем PATH: %s\n" % exc)
    BAD = 1

# Главное утверждение: мост стоит ПЕРВЫМ, как на windows-раннере, и перебор обязан пойти дальше.
if REAL is not None:
    expect("pass", "мост в PATH первым — выбран следующий кандидат",
           BRIDGE + os.pathsep + os.path.dirname(REAL), REAL)

# Молчаливый откат на строку "bash" вернул бы ровно мост, ради которого модуль и заведён. Отказ
# обязан НАЗВАТЬ мост: без имени проверенного кандидата причина красного прогона не восстановима.
expect("fail", "в PATH только мост — отказ, а не мост", BRIDGE, says=only(BRIDGE))

expect("fail", "в PATH ни одного кандидата — отказ со списком", os.path.join(ROOM, "empty"),
       says="ни одного кандидата")

# Судится ВЫВОД вместе с кодом: каждая половина порознь принимает за шелл чужой процесс.
expect("fail", "кандидат вышел нулём, но напечатал чужое",
       stub("chatty", "hello from somewhere else", 0), says="вместо")
expect("fail", "кандидат напечатал слово пробы, но вышел единицей",
       stub("marked-fail", posix_bash.MARK, 1), says="код 1")

if BAD:
    sys.stderr.write("posix-bash-selftest: FAIL\n")
    sys.exit(1)
print("posix-bash-selftest: PASS")
