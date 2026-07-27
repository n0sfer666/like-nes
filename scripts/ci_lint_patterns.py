"""Словарь среды: чем инструмент, флаг или путь отличается от соседнего по виду.

Отделён от `ci_lint_rules.py` по границе вопроса: здесь — ЧТО считается командой, флагом, пробой
и путём, там — КОГДА это становится находкой. Таблицы правятся при каждой новой красной сборке,
логика правил — почти никогда.
"""
import re

# Инструмент -> ОС, на которых он есть у раннера GitHub. Windows означает git-bash: это НЕ POSIX,
# в нём нет ни perl-скриптов вроде shasum, ни половины GNU-специфичных флагов. Сомнение всегда
# трактуется как «нет»: пропущенный вызов стоит красного прогона, лишняя находка — одной строки.
TOOLS = {
    "shasum": {"Linux", "macOS"},
    "md5sum": {"Linux"},
    "sha256sum": {"Linux"},
    "nproc": {"Linux"},
    "timeout": {"Linux"},
    "ldd": {"Linux"},
    "readelf": {"Linux"},
    "objdump": {"Linux"},
    "ldconfig": {"Linux"},
    "strace": {"Linux"},
    "setsid": {"Linux"},
    "apt-get": {"Linux"},
    "sudo": {"Linux"},
    "dpkg": {"Linux"},
    "xvfb-run": {"Linux"},
    "vulkaninfo": {"Linux"},
    "otool": {"macOS"},
    "install_name_tool": {"macOS"},
    "codesign": {"macOS"},
    "sw_vers": {"macOS"},
    "lipo": {"macOS"},
    "plutil": {"macOS"},
    "hdiutil": {"macOS"},
    "dtruss": {"macOS"},
}
# Не отсутствие команды, а расхождение диалектов: одноимённая утилита ведёт себя иначе.
DIALECTS = {
    "sed-i": (r"sed +-i +(?!\.)", "sed -i без суффикса", {"Linux", "Windows"}),
    "grep-P": (r"grep +-[a-zA-Z]*P\b", "grep -P", {"Linux", "Windows"}),
    "stat-c": (r"stat +-c\b", "stat -c", {"Linux", "Windows"}),
}
# git-bash подменяет аргумент, начинающийся со слэша, на путь Windows: `/DWIN32` доезжает до
# компилятора как `C:/Program Files/Git/DWIN32`. Флаг при этом в диагностике не назван — clang-cl
# отвечает «cannot specify /Fo when compiling multiple source files», и ищется причина не там.
# Отличать флаг от пути приходится по форме. Флаг — заглавная первая буква (`/W4`, `/EHsc`,
# `/D_WINDOWS`, `/OUT:x.exe`) либо имя из строчного списка (`/std:c++20`, `/bigobj`). Путь — всё
# остальное: корни POSIX со строчной (`/usr`, `/c/Program Files`) под первое условие не подходят,
# немногие заглавные корни перечислены исключением, а хвостовая проверка отсекает многосегментный
# путь — кроме флагов, чей аргумент путём и является (`/Iinclude/x`, `/Fobuild/a.obj`).
POSIX_ROOTS = "Users|Library|Applications|System|Volumes|Network|Developer"
PATH_FLAGS = r"I|F[oedp]|LIBPATH:|OUT:|IMPLIB:|PDB:"
NAMED_FLAGS = (r"nologo|bigobj|openmp|analyze|clr|showIncludes|permissive-?|utf-8|link"
               r"|(?:std|arch|subsystem|machine|diagnostics|wholearchive|external"
               r"|source-charset|execution-charset|experimental):[\w.+:-]+")
MSVC_FLAG = re.compile(
    rf"(?<![\w./:}}-])/(?!(?:{POSIX_ROOTS})\b)"
    rf"(?:(?:{PATH_FLAGS})\S+|(?:[A-Z]\w*(?::[\w.+:-]+)?|{NAMED_FLAGS})(?![/\w]))")
TOKEN = re.compile(r"[^\s\"']*")
SYSTEM_PATHS = re.compile(r"(?<![\w/])(?:/usr/|/etc/|/opt/|/Library/|/Applications/|/System/)\S*")
# Проба существования — только то, что действительно спрашивает файловую систему. Идиома
# `… || { echo; exit 1; }` пробой НЕ считается: это обработчик ошибки, он есть почти в каждом шаге
# и, будучи признан проверкой, выключал бы правило на всём файле.
PROBE = re.compile(r"\bls\b|\bfind\b|\[\[?\s+-[efdrx]\s|test +-[efdrx] ")
SEGMENT = re.compile(r"&&|\|\||;|\|")
SEARCH = re.compile(r"\bgrep\b|\brg\b")
ASSIGN = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)=\$\(")
COUNTER = re.compile(r"wc\s+-l|grep\s+-[a-zA-Z]*c\b")

