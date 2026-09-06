"""Путь к bash, выбранный ОТВЕТОМ кандидата, а не наличием имени в PATH.

На windows-раннере GitHub первым `bash` в PATH стоит `C:\\Windows\\System32\\bash.exe` — мост в WSL,
а не шелл: без установленного дистрибутива он печатает своё сообщение в UTF-16 и возвращает 1.
`subprocess.run(["bash", ...])` брал именно его, и 2026-09-06 самопроверка врезок прочитала
одиннадцать строк «Windows Subsystem for Linux has no installed distributions» как одиннадцать
документов дерева: отказ выглядел оборвавшимся обходом `docs/`, которого не было вовсе.

Выбор по наличию имени в PATH принял мост за шелл — то же основание, по которому
`check_release_container.sh` выбирает движок ОТВЕТОМ `info`, а не наличием бинаря в PATH.
"""
import os
import subprocess
import tempfile

MARK = "posix-bash-ok"


def names():
    """Имена, под которыми запуск по строке "bash" нашёл бы кандидата на этой ОС.

    Под Windows расширения берутся из PATHEXT, а не прибиваются списком: порядок в нём и есть
    порядок разрешения имени, и перебор, написавший свой, выбирал бы не то, что выбрал бы запуск.
    """
    if os.name != "nt":
        return ("bash",)
    exts = os.environ.get("PATHEXT", ".COM;.EXE;.BAT;.CMD").split(os.pathsep)
    return tuple(["bash" + e.strip().lower() for e in exts if e.strip()] + ["bash"])


def candidates(path=None):
    """Все bash из PATH в порядке поиска. Кандидат не один: первый может шеллом не быть."""
    raw = os.environ.get("PATH", "") if path is None else path
    seen, out = set(), []
    for directory in raw.split(os.pathsep):
        if not directory:
            continue
        for name in names():
            full = os.path.join(directory, name)
            if full not in seen and os.path.isfile(full) and os.access(full, os.X_OK):
                seen.add(full)
                out.append(full)
    return out


def spellings(mark, nt=None):
    """Как один и тот же файл хоста называет шелл, которому мы его показываем.

    Ветвление берётся аргументом, а не только у `os.name`: обе формы обязаны утверждаться на той
    ОС, где набор гоняется, а windows-форму на macOS иначе не проверить ничем.

    Под Windows форм две, и обе — про диск ХОСТА: `C:/…` понимает git-bash, `/c/…` — он же в
    своей корневой раскладке. WSL не понимает НИ ОДНОЙ (у него тот же диск лежит в `/mnt/c`), и
    ровно это отличает мост с установленным дистрибутивом от шелла: `printf` он отработает и там.
    """
    plain = mark.replace("\\", "/")
    if nt is None:
        nt = os.name == "nt"
    if not nt or len(plain) < 2 or plain[1] != ":":
        return [plain]
    return [plain, "/%s%s" % (plain[0].lower(), plain[2:])]


def probe(path):
    """Причина, по которой кандидат шеллом ЭТОГО хоста не является; ответил — None.

    Проба спрашивает про файловую систему хоста, а не только про умение исполнить `-c`: мост WSL
    с УСТАНОВЛЕННЫМ дистрибутивом `printf` отработает и вернёт мусор на windows-путь, который ему
    потом отдадут в `cwd`. Файл-маркер лежит в каталоге хоста, и linux-сторона моста его не видит.

    Причина возвращается СЛОВАМИ: модуль заведён ровно потому, что неверный диагноз стоил четырёх
    прогонов CI, и «ни один bash не ответил» без причины восстанавливается так же ничем.
    """
    fd, mark = tempfile.mkstemp(prefix="posix-bash-")
    os.close(fd)
    seen = " || ".join("[ -f '%s' ]" % form for form in spellings(mark))
    try:
        got = subprocess.run([path, "-c", "{ %s; } && printf '%%s' %s" % (seen, MARK)],
                             capture_output=True, text=True, encoding="utf-8",
                             errors="replace", timeout=60)
    except (OSError, subprocess.SubprocessError) as exc:
        return "%s: %s" % (type(exc).__name__, exc)
    finally:
        os.remove(mark)
    if got.returncode != 0:
        return "код %d, вывод %r" % (got.returncode, got.stdout.strip()[:80])
    if got.stdout.strip() != MARK:
        return "код 0, но вывод %r вместо %r" % (got.stdout.strip()[:80], MARK)
    return None


def find():
    """Первый ответивший bash. Не ответил ни один — ОТКАЗ со списком проверенных и причинами.

    Молчаливый откат на строку "bash" вернул бы ровно тот мост, ради которого модуль и заведён.
    """
    why = []
    for path in candidates():
        reason = probe(path)
        if reason is None:
            return path
        why.append("%s — %s" % (path, reason))
    raise RuntimeError("posix_bash: ни один bash из PATH не ответил пробой; проверено: %s"
                       % ("; ".join(why) or "ни одного кандидата"))
